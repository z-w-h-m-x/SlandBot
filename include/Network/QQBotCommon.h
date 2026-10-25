#pragma once

#include <string>

#include "httplib.h"

/*
    QQ 官方机器人（QQ 开放平台）适配器的公共部分
    Shared helpers for the official QQ Bot adapter

    官方 API 和 OneBot11 有两处根本差异：
    1. 官方用字符串 openid 标识用户/群，框架的 SendID 用 long long
    2. 官方是「主动连网关收事件 + HTTP API 发消息」，不是被动上报

    这里负责 openid 映射和 access_token 缓存。
*/
namespace qqbot
{
    struct AdapterConfig
    {
        std::string appId;
        std::string clientSecret;
        // GROUP_AND_C2C_EVENT = 1 << 25，群聊/单聊消息只需要这个
        unsigned int intents = 1u << 25;
        bool sandbox = false;
    };

    extern AdapterConfig config;

    // 按环境变量补全配置（QQ_BOT_APPID / QQ_BOT_SECRET）
    void LoadConfigFromEnv();

    bool IsConfigured();

    std::string ApiHost();
    std::string GatewayUrl();

    // 统一的 HTTP 客户端工厂：超时、TLS（SLB_INSECURE_TLS）都在这里处理，
    // 避免各调用点重复写 OpenSSL 条件编译。
    httplib::Client CreateApiClient();
    httplib::Client CreateTokenClient();

    // openid <-> 框架内部数字 ID 的双向映射。
    // 进程重启后映射会重建，插件不要把数字 ID 当持久化标识用。
    long long InternId(const std::string &openId);
    std::string OpenIdOf(long long internId);
    bool HasOpenId(long long internId);

    // access_token 自动缓存、提前刷新；失败返回空串
    std::string GetAccessToken(bool forceRefresh = false);
    void InvalidateAccessToken();

    // 被动回复窗口：记下最近一条收到的消息 msg_id。
    // 官方对主动推送有额度限制，回复时带上 msg_id 才算被动消息。
    void RememberMsgId(long long target, bool isGroup, const std::string &msgId);
    // 取出可用的 msg_id 并推进 msg_seq；窗口过期或次数用尽返回 false
    bool TakeMsgId(long long target, bool isGroup, std::string &msgId, int &msgSeq);

    // 官方 API 不提供群成员列表，这里统一返回 retcode=-1
    const char *NoGroupMemberListMessage();
}
