#pragma once

#include <string>

namespace Data
{
    enum class AdapterType
    {
        OneBot11, // OneBot11 上报 + NapCat 发送
        QQBot     // QQ 官方机器人（开放平台）
    };

    struct ServerData
    {
        AdapterType adapter = AdapterType::OneBot11;

        int port = 1234;
        int napCatPort = 3000;
        std::string napCatHost = "127.0.0.1";
        std::string host = "0.0.0.0";

        // QQ 官方机器人（QQ 开放平台）
        std::string qqBotAppId;
        std::string qqBotClientSecret;
        bool qqBotSandbox = false;
    };

}
