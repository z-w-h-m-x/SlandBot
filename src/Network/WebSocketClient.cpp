#include "Network/WebSocketClient.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

/*
    QQ 官方机器人网关只提供 wss://，所以这里必须带上 TLS。
    如果本机 OpenSSL 找不到 CA 证书（常见于 Windows 的 MinGW 包），
    可以设置环境变量 SLB_INSECURE_TLS=1 跳过证书校验。
*/
static bool InsecureTls()
{
    const char *v = std::getenv("SLB_INSECURE_TLS");
    return v != nullptr && std::strcmp(v, "0") != 0;
}

#ifdef _WIN32
typedef SOCKET RawSocket;
static const RawSocket InvalidSocket = INVALID_SOCKET;
#else
typedef int RawSocket;
static const RawSocket InvalidSocket = -1;
#endif

static void EnsureNetInit()
{
#ifdef _WIN32
    static bool done = false;
    if (!done)
    {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        done = true;
    }
#endif
}

static void CloseRaw(RawSocket s)
{
    if (s == InvalidSocket)
        return;
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
}

static std::string Base64Encode(const unsigned char *data, std::size_t length)
{
    if (data == nullptr || length == 0)
        return std::string();

    std::string out;
    out.resize(4 * ((length + 2) / 3) + 1);
    int written = EVP_EncodeBlock(reinterpret_cast<unsigned char *>(&out[0]), data,
                                  static_cast<int>(length));
    if (written < 0)
        return std::string();
    out.resize(static_cast<std::size_t>(written));
    return out;
}

// Sec-WebSocket-Accept = base64(sha1(key + GUID))
static std::string MakeAcceptKey(const std::string &clientKey)
{
    static const char *kGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string source = clientKey + kGuid;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLength = 0;
    EVP_Digest(source.data(), source.size(), digest, &digestLength, EVP_sha1(), nullptr);
    return Base64Encode(digest, digestLength);
}

static bool ParseUrl(const std::string &url, std::string &scheme, std::string &host,
                     int &port, std::string &path)
{
    std::size_t schemeEnd = url.find("://");
    if (schemeEnd == std::string::npos)
        return false;

    scheme = url.substr(0, schemeEnd);
    std::string rest = url.substr(schemeEnd + 3);
    if (scheme != "wss" && scheme != "ws")
        return false;

    std::size_t slash = rest.find('/');
    std::string hostPort = (slash == std::string::npos) ? rest : rest.substr(0, slash);
    path = (slash == std::string::npos) ? "/" : rest.substr(slash);
    if (path.empty())
        path = "/";

    port = (scheme == "wss") ? 443 : 80;

    // IPv6 字面量（[::1]:443）暂不做特殊处理，QQ 网关是域名
    std::size_t colon = hostPort.rfind(':');
    if (colon != std::string::npos && hostPort.find(']') == std::string::npos)
    {
        port = std::atoi(hostPort.substr(colon + 1).c_str());
        host = hostPort.substr(0, colon);
    }
    else
    {
        host = hostPort;
    }

    return !host.empty() && port > 0 && port <= 65535;
}

WebSocketClient::WebSocketClient()
{
    EnsureNetInit();
}

WebSocketClient::~WebSocketClient()
{
    Close();
}

void WebSocketClient::ReleaseSocket()
{
    if (sslHandle != nullptr)
    {
        SSL_free(static_cast<SSL *>(sslHandle));
        sslHandle = nullptr;
    }
    if (sslContext != nullptr)
    {
        SSL_CTX_free(static_cast<SSL_CTX *>(sslContext));
        sslContext = nullptr;
    }
    if (socketHandle != -1)
    {
        CloseRaw(static_cast<RawSocket>(socketHandle));
        socketHandle = -1;
    }
}

void WebSocketClient::Close()
{
    std::lock_guard<std::mutex> lock(sendMutex);
    if (open && sslHandle != nullptr)
    {
        // 尽力发一个 close 帧，失败也无所谓
        unsigned char mask[4] = {0, 0, 0, 0};
        RAND_bytes(mask, sizeof(mask));
        unsigned char buffer[6] = {0x88, 0x80, mask[0], mask[1], mask[2], mask[3]};
        SSL_write(static_cast<SSL *>(sslHandle), buffer, sizeof(buffer));
    }
    open = false;
    fragment.clear();
    ReleaseSocket();
}

bool WebSocketClient::ConnectTcp(const std::string &host, int port, std::string *error)
{
    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result = nullptr;
    std::string portText = std::to_string(port);
    int rc = getaddrinfo(host.c_str(), portText.c_str(), &hints, &result);
    if (rc != 0 || result == nullptr)
    {
        if (error)
            *error = "DNS resolve failed: " + host;
        return false;
    }

    RawSocket connected = InvalidSocket;
    for (struct addrinfo *ai = result; ai != nullptr; ai = ai->ai_next)
    {
        RawSocket s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == InvalidSocket)
            continue;

        // 非阻塞 connect + select，避免在断网时卡很久
#ifdef _WIN32
        u_long nonBlocking = 1;
        ioctlsocket(s, FIONBIO, &nonBlocking);
#else
        int flags = fcntl(s, F_GETFL, 0);
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

        int ret = connect(s, ai->ai_addr, static_cast<int>(ai->ai_addrlen));
        bool ok = (ret == 0);

        if (!ok)
        {
            fd_set writeSet;
            fd_set errorSet;
            FD_ZERO(&writeSet);
            FD_ZERO(&errorSet);
            FD_SET(s, &writeSet);
            FD_SET(s, &errorSet);
            struct timeval tv;
            tv.tv_sec = 8;
            tv.tv_usec = 0;
            int sel = select(static_cast<int>(s) + 1, nullptr, &writeSet, &errorSet, &tv);
            if (sel > 0 && FD_ISSET(s, &writeSet) && !FD_ISSET(s, &errorSet))
            {
                int soError = 0;
                socklen_t len = sizeof(soError);
                getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&soError), &len);
                ok = (soError == 0);
            }
        }

        // 恢复阻塞模式，并设置读写超时，防止 TLS 读取永久挂住
#ifdef _WIN32
        nonBlocking = 0;
        ioctlsocket(s, FIONBIO, &nonBlocking);
        DWORD timeout = 15000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));
#else
        fcntl(s, F_SETFL, flags);
        struct timeval timeout;
        timeout.tv_sec = 15;
        timeout.tv_usec = 0;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

        if (ok)
        {
            connected = s;
            break;
        }

        CloseRaw(s);
    }

    freeaddrinfo(result);

    if (connected == InvalidSocket)
    {
        if (error)
            *error = "TCP connect failed: " + host + ":" + portText;
        return false;
    }

    socketHandle = static_cast<std::intptr_t>(connected);
    return true;
}

bool WebSocketClient::StartTls(const std::string &host, std::string *error)
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == nullptr)
    {
        if (error)
            *error = "SSL_CTX_new failed";
        return false;
    }

    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_default_verify_paths(ctx);

    bool insecure = InsecureTls();
    SSL_CTX_set_verify(ctx, insecure ? SSL_VERIFY_NONE : SSL_VERIFY_PEER, nullptr);

    SSL *ssl = SSL_new(ctx);
    if (ssl == nullptr)
    {
        SSL_CTX_free(ctx);
        if (error)
            *error = "SSL_new failed";
        return false;
    }

    SSL_set_fd(ssl, static_cast<int>(socketHandle));
    SSL_set_tlsext_host_name(ssl, host.c_str());
    if (!insecure)
        SSL_set1_host(ssl, host.c_str());

    if (SSL_connect(ssl) != 1)
    {
        unsigned long code = ERR_get_error();
        char text[256] = {0};
        ERR_error_string_n(code, text, sizeof(text));
        if (error)
            *error = std::string("TLS handshake failed: ") + text;
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return false;
    }

    if (!insecure && SSL_get_verify_result(ssl) != X509_V_OK)
    {
        if (error)
            *error = "TLS certificate verify failed (set SLB_INSECURE_TLS=1 to skip)";
        SSL_free(ssl);
        SSL_CTX_free(ctx);
        return false;
    }

    sslContext = ctx;
    sslHandle = ssl;
    return true;
}

bool WebSocketClient::Connect(const std::string &url, std::string *error)
{
    Close();

    std::string scheme, host, path;
    int port = 0;
    if (!ParseUrl(url, scheme, host, port, path))
    {
        if (error)
            *error = "Invalid WebSocket url: " + url;
        return false;
    }
    if (scheme != "wss")
    {
        if (error)
            *error = "Only wss:// is supported";
        return false;
    }

    if (!ConnectTcp(host, port, error))
    {
        ReleaseSocket();
        return false;
    }

    if (!StartTls(host, error))
    {
        ReleaseSocket();
        return false;
    }

    if (!Handshake(host, port, path, error))
    {
        ReleaseSocket();
        return false;
    }

    open = true;
    return true;
}

bool WebSocketClient::Handshake(const std::string &host, int port,
                                const std::string &path, std::string *error)
{
    SSL *ssl = static_cast<SSL *>(sslHandle);

    unsigned char nonce[16];
    RAND_bytes(nonce, sizeof(nonce));
    std::string clientKey = Base64Encode(nonce, sizeof(nonce));

    std::string request;
    request += "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + ((port == 443) ? "" : (":" + std::to_string(port))) + "\r\n";
    request += "Upgrade: websocket\r\n";
    request += "Connection: Upgrade\r\n";
    request += "Sec-WebSocket-Key: " + clientKey + "\r\n";
    request += "Sec-WebSocket-Version: 13\r\n";
    request += "\r\n";

    if (SSL_write(ssl, request.data(), static_cast<int>(request.size())) <= 0)
    {
        if (error)
            *error = "Failed to send WebSocket handshake";
        return false;
    }

    // 逐字节读取响应头，直到遇到空行
    std::string response;
    char c = 0;
    while (response.size() < 16384)
    {
        int n = SSL_read(ssl, &c, 1);
        if (n <= 0)
            break;
        response += c;
        if (response.size() >= 4 &&
            response.compare(response.size() - 4, 4, "\r\n\r\n") == 0)
            break;
    }

    if (response.find(" 101 ") == std::string::npos)
    {
        if (error)
        {
            std::size_t lineEnd = response.find("\r\n");
            *error = "WebSocket upgrade rejected: " +
                     response.substr(0, lineEnd == std::string::npos ? 100 : lineEnd);
        }
        return false;
    }

    // 校验 Sec-WebSocket-Accept（缺失时也给过，兼容非常规代理）
    std::string expected = MakeAcceptKey(clientKey);
    if (response.find("Sec-WebSocket-Accept") != std::string::npos &&
        response.find(expected) == std::string::npos)
    {
        if (error)
            *error = "Sec-WebSocket-Accept mismatch";
        return false;
    }

    return true;
}

bool WebSocketClient::SendFrame(unsigned char opcode, const std::string &payload)
{
    std::vector<unsigned char> frame;
    frame.reserve(payload.size() + 14);

    frame.push_back(static_cast<unsigned char>(0x80 | opcode)); // FIN + opcode

    std::size_t length = payload.size();
    if (length < 126)
    {
        frame.push_back(static_cast<unsigned char>(0x80 | length)); // 客户端必须掩码
    }
    else if (length <= 0xFFFF)
    {
        frame.push_back(0x80 | 126);
        frame.push_back(static_cast<unsigned char>((length >> 8) & 0xFF));
        frame.push_back(static_cast<unsigned char>(length & 0xFF));
    }
    else
    {
        frame.push_back(0x80 | 127);
        for (int i = 7; i >= 0; --i)
            frame.push_back(static_cast<unsigned char>((length >> (8 * i)) & 0xFF));
    }

    unsigned char mask[4];
    RAND_bytes(mask, sizeof(mask));
    frame.insert(frame.end(), mask, mask + 4);

    std::size_t offset = frame.size();
    frame.resize(offset + length);
    for (std::size_t i = 0; i < length; ++i)
        frame[offset + i] = static_cast<unsigned char>(payload[i]) ^ mask[i % 4];

    SSL *ssl = static_cast<SSL *>(sslHandle);
    std::size_t sent = 0;
    while (sent < frame.size())
    {
        int n = SSL_write(ssl, frame.data() + sent, static_cast<int>(frame.size() - sent));
        if (n <= 0)
            return false;
        sent += static_cast<std::size_t>(n);
    }
    return true;
}

bool WebSocketClient::SendText(const std::string &text)
{
    std::lock_guard<std::mutex> lock(sendMutex);
    if (!open)
        return false;
    return SendFrame(0x1, text);
}

bool WebSocketClient::SendPing(const std::string &payload)
{
    std::lock_guard<std::mutex> lock(sendMutex);
    if (!open)
        return false;
    return SendFrame(0x9, payload);
}

bool WebSocketClient::ReadExact(void *buffer, std::size_t length)
{
    SSL *ssl = static_cast<SSL *>(sslHandle);
    std::size_t readBytes = 0;
    unsigned char *data = static_cast<unsigned char *>(buffer);

    while (readBytes < length)
    {
        int n = SSL_read(ssl, data + readBytes, static_cast<int>(length - readBytes));
        if (n <= 0)
            return false;
        readBytes += static_cast<std::size_t>(n);
    }
    return true;
}

bool WebSocketClient::WaitReadable(int timeoutMs)
{
    SSL *ssl = static_cast<SSL *>(sslHandle);
    if (ssl != nullptr && SSL_pending(ssl) > 0)
        return true;

    RawSocket s = static_cast<RawSocket>(socketHandle);
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(s, &readSet);

    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int ret = select(static_cast<int>(s) + 1, &readSet, nullptr, nullptr, &tv);
    return ret > 0;
}

WebSocketClient::RecvResult WebSocketClient::ReadFrame(std::string &out)
{
    unsigned char header[2];
    if (!ReadExact(header, 2))
        return RecvResult::Closed;

    bool fin = (header[0] & 0x80) != 0;
    unsigned char opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    std::uint64_t length = header[1] & 0x7F;

    if (length == 126)
    {
        unsigned char ext[2];
        if (!ReadExact(ext, 2))
            return RecvResult::Closed;
        length = (static_cast<std::uint64_t>(ext[0]) << 8) | ext[1];
    }
    else if (length == 127)
    {
        unsigned char ext[8];
        if (!ReadExact(ext, 8))
            return RecvResult::Closed;
        length = 0;
        for (int i = 0; i < 8; ++i)
            length = (length << 8) | ext[i];
    }

    // 单帧上限 16MB，防止异常数据撑爆内存
    if (length > 16 * 1024 * 1024)
        return RecvResult::Error;

    unsigned char mask[4] = {0, 0, 0, 0};
    if (masked && !ReadExact(mask, 4))
        return RecvResult::Closed;

    std::string payload;
    payload.resize(static_cast<std::size_t>(length));
    if (length > 0 && !ReadExact(&payload[0], static_cast<std::size_t>(length)))
        return RecvResult::Closed;

    if (masked)
    {
        for (std::size_t i = 0; i < payload.size(); ++i)
            payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
    }

    switch (opcode)
    {
    case 0x0: // continuation
        fragment += payload;
        if (fin)
        {
            out.swap(fragment);
            fragment.clear();
            return RecvResult::Message;
        }
        return RecvResult::Timeout;

    case 0x1: // text
    case 0x2: // binary（网关不会发，按二进制兜底）
        if (fin)
        {
            out.swap(payload);
            return RecvResult::Message;
        }
        fragment = payload;
        return RecvResult::Timeout;

    case 0x8: // close
        open = false;
        return RecvResult::Closed;

    case 0x9: // ping -> pong
        SendFrame(0xA, payload);
        return RecvResult::Timeout;

    case 0xA: // pong
        return RecvResult::Timeout;

    default:
        return RecvResult::Error;
    }
}

WebSocketClient::RecvResult WebSocketClient::Receive(std::string &out, int timeoutMs)
{
    if (!open)
        return RecvResult::Closed;

    const std::int64_t start =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const std::int64_t deadline = start + timeoutMs;

    while (true)
    {
        std::int64_t now =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        std::int64_t remaining = deadline - now;
        if (remaining <= 0)
            return RecvResult::Timeout;
        if (remaining > timeoutMs)
            remaining = timeoutMs;

        if (!WaitReadable(static_cast<int>(remaining)))
            return RecvResult::Timeout;

        RecvResult result = ReadFrame(out);
        if (result != RecvResult::Timeout)
            return result;
        // Timeout 表示只处理了控制帧，继续等业务消息
    }
}
