#pragma once

#include "../ServerBase.h"
#include "Network/WebSocketClient.h"

#include "json.hpp"

#include <atomic>
#include <string>
#include <thread>

/*
    QQ 官方机器人网关（QQ 开放平台）
    Official QQ Bot gateway

    官方事件是「客户端主动连 wss 网关」推下来的，没有 OneBot 那种 HTTP 上报，
    所以这里实现 IServer 只是为了跟框架的统一生命周期对齐，host/port 参数不使用。
*/
class Server_QQBotGateway : public IServer
{
private:
    std::thread gatewayThread;
    std::atomic<bool> stopFlag{false};
    std::atomic<bool> gatewayActive{false};

    std::string sessionId;
    int lastSeq = 0;
    bool warnedChannelEvent = false;

    void GatewayLoop();
    bool ConnectAndRun(std::string *error);
    // 返回 false 表示需要整体重连
    bool ProcessPayload(const std::string &text);
    void HandleDispatch(const std::string &eventType, const nlohmann::json &data);
    void HandleGroupMessage(const nlohmann::json &data);
    void HandlePrivateMessage(const nlohmann::json &data);

public:
    bool Run(const char * host, int port) override;
    bool Stop() override;
    bool Init() override;
    bool Active() override;
    void AfterStop() override;
};
