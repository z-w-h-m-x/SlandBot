#include "Network/ReportingServer/QQBotGateway.h"

#include "GlobalData.h"
#include "Network/QQBotCommon.h"
#include "Processor/MessageQueueProcessor.h"
#include "util.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

using nlohmann::json;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::steady_clock;

namespace
{
    // 官方群消息 content 可能带前导空格（@ 之后留下的），去掉更贴近 OneBot 的行为
    std::string TrimLeadingSpace(const std::string &text)
    {
        std::size_t begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
            return std::string();
        return text.substr(begin);
    }
}

bool Server_QQBotGateway::Init()
{
    qqbot::LoadConfigFromEnv();

    if (!qqbot::IsConfigured())
    {
        logger.PrintError("QQBot: 缺少 appId/clientSecret，请用 --appid/--secret 或 QQ_BOT_APPID/QQ_BOT_SECRET 提供");
        return false;
    }

    logger << "QQBot: 网关初始化完成 (host=" + qqbot::ApiHost() + ")";
    return true;
}

bool Server_QQBotGateway::Run(const char *host, int port)
{
    // host/port 对官方网关没有意义，保留签名只是为了对齐 IServer
    (void)host;
    (void)port;

    stopFlag = false;

    try
    {
        std::thread temp([this]() { GatewayLoop(); });
        gatewayThread.swap(temp);
        isRun = true;
    }
    catch (const std::exception &e)
    {
        logger.PrintError(std::string("QQBot: 启动网关线程失败: ") + e.what());
        return false;
    }

    return true;
}

bool Server_QQBotGateway::Stop()
{
    stopFlag = true;

    if (gatewayThread.joinable())
        gatewayThread.join();

    AfterStop();
    isRun = false;
    return true;
}

void Server_QQBotGateway::AfterStop()
{
    gatewayActive = false;
}

bool Server_QQBotGateway::Active()
{
    return gatewayActive;
}

void Server_QQBotGateway::GatewayLoop()
{
    int backoffMs = 1000;

    while (!stopFlag)
    {
        std::string error;
        bool ok = ConnectAndRun(&error);

        if (stopFlag)
            break;

        if (ok)
        {
            logger << "QQBot: 网关连接正常结束，准备重连";
            backoffMs = 1000;
        }
        else
        {
            logger.PrintError("QQBot: 网关连接失败: " + error);
            logger << "QQBot: " + std::to_string(backoffMs / 1000) + " 秒后重连";
        }

        // 分片 sleep，让 Stop() 能及时退出
        for (int elapsed = 0; elapsed < backoffMs && !stopFlag; elapsed += 100)
            slbUtils::Sleep(100);

        if (!ok)
            backoffMs = std::min(backoffMs * 2, 30000);
    }

    gatewayActive = false;
}

bool Server_QQBotGateway::ConnectAndRun(std::string *error)
{
    std::string token = qqbot::GetAccessToken();
    if (token.empty())
    {
        if (error)
            *error = "无法获取 access_token";
        return false;
    }

    // 1. 取网关地址
    httplib::Client api = qqbot::CreateApiClient();

    httplib::Headers headers;
    headers.emplace("Authorization", "QQBot " + token);

    auto res = api.Get("/gateway", headers);
    if (!res)
    {
        if (error)
            *error = "请求 /gateway 失败（网络错误）";
        return false;
    }
    if (res->status == 401)
    {
        qqbot::InvalidateAccessToken();
        if (error)
            *error = "获取网关地址时鉴权失败，token 已作废";
        return false;
    }
    if (res->status != 200)
    {
        if (error)
            *error = "获取网关地址失败, HTTP " + std::to_string(res->status) + " " + res->body.substr(0, 200);
        return false;
    }

    std::string gatewayUrl;
    try
    {
        json body = json::parse(res->body);
        gatewayUrl = body.value("url", "");
    }
    catch (const std::exception &e)
    {
        if (error)
            *error = std::string("网关地址响应解析失败: ") + e.what();
        return false;
    }

    if (gatewayUrl.empty())
    {
        if (error)
            *error = "网关地址为空";
        return false;
    }

    // 2. 建立 WebSocket
    WebSocketClient ws;
    if (!ws.Connect(gatewayUrl, error))
        return false;

    logger << "QQBot: WebSocket 已连接 " + gatewayUrl;

    // 3. 等 Hello（op 10），拿心跳间隔
    int heartbeatInterval = 41250;
    bool gotHello = false;
    auto helloDeadline = steady_clock::now() + seconds(15);
    std::string text;

    while (!stopFlag && !gotHello && steady_clock::now() < helloDeadline)
    {
        WebSocketClient::RecvResult result = ws.Receive(text, 1000);
        if (result == WebSocketClient::RecvResult::Message)
        {
            try
            {
                json payload = json::parse(text);
                if (payload.value("op", -1) == 10 && payload.contains("d"))
                {
                    heartbeatInterval = payload["d"].value("heartbeat_interval", 41250);
                    gotHello = true;
                }
            }
            catch (const std::exception &)
            {
                // 忽略非法数据
            }
        }
        else if (result == WebSocketClient::RecvResult::Closed ||
                 result == WebSocketClient::RecvResult::Error)
        {
            ws.Close();
            if (error)
                *error = "等待 Hello 时连接断开";
            return false;
        }
    }

    if (!gotHello)
    {
        ws.Close();
        if (error)
            *error = "等待 Hello 超时";
        return false;
    }

    // 4. Identify
    json identify;
    identify["op"] = 2;
    identify["d"]["token"] = "QQBot " + token;
    identify["d"]["intents"] = qqbot::config.intents;
    identify["d"]["properties"]["$os"] = "linux";
    identify["d"]["properties"]["$browser"] = "slandbot";
    identify["d"]["properties"]["$device"] = "slandbot";

    if (!ws.SendText(identify.dump()))
    {
        ws.Close();
        if (error)
            *error = "发送 Identify 失败";
        return false;
    }

    // 5. 事件循环（心跳在同一线程里按间隔发，简单可靠）
    auto nextHeartbeat = steady_clock::now() + milliseconds(heartbeatInterval);
    gatewayActive = true;

    while (!stopFlag)
    {
        WebSocketClient::RecvResult result = ws.Receive(text, 500);

        if (result == WebSocketClient::RecvResult::Message)
        {
            if (!ProcessPayload(text))
            {
                ws.Close();
                gatewayActive = false;
                if (error)
                    *error = "网关要求重连";
                return true; // 不算失败，外层会按正常重连处理
            }
        }
        else if (result == WebSocketClient::RecvResult::Closed ||
                 result == WebSocketClient::RecvResult::Error)
        {
            ws.Close();
            gatewayActive = false;
            if (error)
                *error = "连接已断开";
            return false;
        }

        if (steady_clock::now() >= nextHeartbeat)
        {
            json heartbeat;
            heartbeat["op"] = 1;
            heartbeat["d"] = (lastSeq > 0) ? json(lastSeq) : json(nullptr);

            if (!ws.SendText(heartbeat.dump()))
            {
                ws.Close();
                gatewayActive = false;
                if (error)
                    *error = "心跳发送失败";
                return false;
            }

            nextHeartbeat = steady_clock::now() + milliseconds(heartbeatInterval);
        }
    }

    ws.Close();
    gatewayActive = false;
    return true;
}

bool Server_QQBotGateway::ProcessPayload(const std::string &text)
{
    json payload;
    try
    {
        payload = json::parse(text);
    }
    catch (const std::exception &e)
    {
        logger.PrintError(std::string("QQBot: 收到无法解析的网关数据: ") + e.what());
        return true;
    }

    if (payload.contains("s") && payload["s"].is_number())
        lastSeq = payload["s"].get<int>();

    int op = payload.value("op", -1);
    switch (op)
    {
    case 0: // 事件分发
        if (payload.contains("t") && payload["t"].is_string())
        {
            std::string eventType = payload["t"].get<std::string>();
            if (payload.contains("d"))
                HandleDispatch(eventType, payload["d"]);
        }
        return true;

    case 7: // 服务端要求重连
        logger << "QQBot: 网关要求重连 (op 7)";
        return false;

    case 9: // session 失效
        logger << "QQBot: session 失效 (op 9)，将重新 Identify";
        sessionId.clear();
        lastSeq = 0;
        return false;

    case 11: // 心跳 ACK
        return true;

    default:
        return true;
    }
}

void Server_QQBotGateway::HandleDispatch(const std::string &eventType, const json &data)
{
    if (eventType == "READY")
    {
        if (data.contains("session_id") && data["session_id"].is_string())
            sessionId = data["session_id"].get<std::string>();

        std::string botName;
        if (data.contains("user") && data["user"].contains("username"))
            botName = data["user"]["username"].get<std::string>();

        logger << "QQBot: READY，机器人已上线" + (botName.empty() ? "" : " (" + botName + ")");
        return;
    }

    if (eventType == "RESUMED")
    {
        logger << "QQBot: 会话已恢复";
        return;
    }

    if (eventType == "GROUP_AT_MESSAGE_CREATE")
    {
        HandleGroupMessage(data);
        return;
    }

    if (eventType == "C2C_MESSAGE_CREATE")
    {
        HandlePrivateMessage(data);
        return;
    }

    // 频道相关的消息官方走另一套 API（channel_id），当前适配器不处理
    if (eventType == "AT_MESSAGE_CREATE" || eventType == "DIRECT_MESSAGE_CREATE")
    {
        if (!warnedChannelEvent)
        {
            warnedChannelEvent = true;
            logger << "QQBot: 收到频道消息事件（" + eventType +
                          "），当前适配器只支持群聊/单聊，已忽略";
        }
        return;
    }

    debugLogger < ("QQBot: 忽略事件 " + eventType);
}

void Server_QQBotGateway::HandleGroupMessage(const json &data)
{
    if (!data.contains("group_openid") || !data["group_openid"].is_string())
        return;

    std::string groupOpenId = data["group_openid"].get<std::string>();
    std::string memberOpenId;
    if (data.contains("author") && data["author"].is_object())
    {
        if (data["author"].contains("member_openid") && data["author"]["member_openid"].is_string())
            memberOpenId = data["author"]["member_openid"].get<std::string>();
        else if (data["author"].contains("id") && data["author"]["id"].is_string())
            memberOpenId = data["author"]["id"].get<std::string>();
    }

    std::string content;
    if (data.contains("content") && data["content"].is_string())
        content = TrimLeadingSpace(data["content"].get<std::string>());

    std::string msgId;
    if (data.contains("id") && data["id"].is_string())
        msgId = data["id"].get<std::string>();

    iMessageContent message;
    message.sendID.messageType = at; // 官方群事件只有 @机器人 才推送
    message.sendID.subType = group_normal;
    message.sendID.isGroup = true;
    message.sendID.groupID = qqbot::InternId(groupOpenId);
    message.sendID.userID = qqbot::InternId(memberOpenId);
    message.sendID.serverID = 0;
    message.contentS = content;
    message.content = nullptr;
    message.isEmpty = false;

    // 记下 msg_id，回复时走被动消息
    qqbot::RememberMsgId(message.sendID.groupID, true, msgId);

    logger << ("收到群聊(QQBot): <- " + std::to_string(message.sendID.userID) + ":" + content);

    ReceiveMessage(message);
}

void Server_QQBotGateway::HandlePrivateMessage(const json &data)
{
    std::string userOpenId;
    if (data.contains("author") && data["author"].is_object())
    {
        if (data["author"].contains("user_openid") && data["author"]["user_openid"].is_string())
            userOpenId = data["author"]["user_openid"].get<std::string>();
        else if (data["author"].contains("id") && data["author"]["id"].is_string())
            userOpenId = data["author"]["id"].get<std::string>();
    }

    if (userOpenId.empty())
        return;

    std::string content;
    if (data.contains("content") && data["content"].is_string())
        content = TrimLeadingSpace(data["content"].get<std::string>());

    std::string msgId;
    if (data.contains("id") && data["id"].is_string())
        msgId = data["id"].get<std::string>();

    iMessageContent message;
    message.sendID.messageType = normal;
    message.sendID.subType = private_friend;
    message.sendID.isGroup = false;
    message.sendID.groupID = 0;
    message.sendID.userID = qqbot::InternId(userOpenId);
    message.sendID.serverID = 0;
    message.contentS = content;
    message.content = nullptr;
    message.isEmpty = false;

    qqbot::RememberMsgId(message.sendID.userID, false, msgId);

    logger << ("收到私聊(QQBot): <- " + std::to_string(message.sendID.userID) + ":" + content);

    ReceiveMessage(message);
}
