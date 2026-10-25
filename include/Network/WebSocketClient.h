#pragma once

#include <cstdint>
#include <mutex>
#include <string>

/*
    最小可用的 WebSocket 客户端（RFC 6455）
    Minimal WebSocket client (RFC 6455)

    只实现 QQ 官方机器人网关需要的能力：
    - wss:// 连接（OpenSSL）
    - 文本帧收发、ping/pong、close
    - 分片消息拼装

    这个类不处理业务逻辑，业务在 QQBotGateway 里。
    This class only speaks the protocol; the bot logic lives in QQBotGateway.
*/
class WebSocketClient
{
public:
    enum class RecvResult
    {
        Message, // 收到一条完整消息
        Timeout, // 超时，暂时没有消息
        Closed,  // 对端关闭或连接已断开
        Error    // 协议/网络错误
    };

    WebSocketClient();
    ~WebSocketClient();

    WebSocketClient(const WebSocketClient &) = delete;
    WebSocketClient &operator=(const WebSocketClient &) = delete;

    // url 形如 wss://api.sgroup.qq.com/websocket
    bool Connect(const std::string &url, std::string *error = nullptr);
    bool SendText(const std::string &text);
    bool SendPing(const std::string &payload = std::string());
    // 阻塞读取，最多等 timeoutMs 毫秒
    RecvResult Receive(std::string &out, int timeoutMs);
    void Close();
    bool IsOpen() const { return open; }

private:
    // 用 intptr_t 存平台 socket，避免在头文件里引入 winsock 头
    std::intptr_t socketHandle = -1;
    void *sslContext = nullptr;
    void *sslHandle = nullptr;
    bool open = false;
    std::string fragment; // 分片消息缓存
    std::mutex sendMutex;

    bool ConnectTcp(const std::string &host, int port, std::string *error);
    bool StartTls(const std::string &host, std::string *error);
    bool Handshake(const std::string &host, int port, const std::string &path,
                   std::string *error);
    bool SendFrame(unsigned char opcode, const std::string &payload);
    bool ReadExact(void *buffer, std::size_t length);
    bool WaitReadable(int timeoutMs);
    RecvResult ReadFrame(std::string &out);
    void ReleaseSocket();
};
