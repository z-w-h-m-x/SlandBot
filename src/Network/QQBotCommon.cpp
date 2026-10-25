#include "Network/QQBotCommon.h"

#include "GlobalData.h"

#include "httplib.h"
#include "json.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

using nlohmann::json;

namespace qqbot
{
    AdapterConfig config;

    // ---------- 配置 ----------

    void LoadConfigFromEnv()
    {
        if (config.appId.empty())
        {
            const char *appId = std::getenv("QQ_BOT_APPID");
            if (appId != nullptr)
                config.appId = appId;
        }
        if (config.clientSecret.empty())
        {
            const char *secret = std::getenv("QQ_BOT_SECRET");
            if (secret != nullptr)
                config.clientSecret = secret;
        }

        // 被动回复限制：平台规则调整时用环境变量覆盖，不必改代码重编
        auto readPositiveInt = [](const char *name, int fallback)
        {
            const char *v = std::getenv(name);
            if (v == nullptr || *v == '\0')
                return fallback;
            int parsed = std::atoi(v);
            return parsed > 0 ? parsed : fallback;
        };

        config.groupReplyWindowSeconds = readPositiveInt(
            "QQ_BOT_GROUP_REPLY_WINDOW", config.groupReplyWindowSeconds);
        config.privateReplyWindowSeconds = readPositiveInt(
            "QQ_BOT_PRIVATE_REPLY_WINDOW", config.privateReplyWindowSeconds);
        config.groupMaxReplyPerMsg = readPositiveInt(
            "QQ_BOT_GROUP_MAX_REPLY", config.groupMaxReplyPerMsg);
        config.privateMaxReplyPerMsg = readPositiveInt(
            "QQ_BOT_PRIVATE_MAX_REPLY", config.privateMaxReplyPerMsg);
    }

    bool IsConfigured()
    {
        // 调试时可以只给 access_token，不填 appId/clientSecret
        if (std::getenv("QQ_BOT_ACCESS_TOKEN") != nullptr)
            return true;
        return !config.appId.empty() && !config.clientSecret.empty();
    }

    std::string ApiHost()
    {
        // 调试/代理场景可以覆盖，例如指向本地假网关做联调
        const char *overrideHost = std::getenv("QQ_BOT_API_HOST");
        if (overrideHost != nullptr && *overrideHost != '\0')
            return overrideHost;
        return config.sandbox ? "sandbox.api.sgroup.qq.com" : "api.sgroup.qq.com";
    }

    std::string GatewayUrl()
    {
        return "https://" + ApiHost() + "/gateway";
    }

    static void ApplyTlsOption(httplib::Client &cli)
    {
        // httplib 的证书开关只在启用 OpenSSL 时才有，用宏兜住，
        // 保证换构建方式（没开 SSL）也能编译。
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        const char *v = std::getenv("SLB_INSECURE_TLS");
        if (v != nullptr && std::strcmp(v, "0") != 0)
            cli.enable_server_certificate_verification(false);
#else
        (void)cli;
#endif
    }

    httplib::Client CreateApiClient()
    {
        httplib::Client cli("https://" + ApiHost());
        cli.set_connection_timeout(10);
        cli.set_read_timeout(15);
        ApplyTlsOption(cli);
        return cli;
    }

    httplib::Client CreateTokenClient()
    {
        httplib::Client cli("https://bots.qq.com");
        cli.set_connection_timeout(10);
        cli.set_read_timeout(10);
        ApplyTlsOption(cli);
        return cli;
    }

    // ---------- openid 映射 ----------

    // 官方 openid 是 base64 风格的字符串，框架 SendID 只有 long long。
    // 这里用 FNV-1a 折成 63 位正整数，冲突时线性探测。
    static std::mutex idMutex;
    static std::map<long long, std::string> idToOpenId;
    static std::map<std::string, long long> openIdToId;

    static long long HashOpenId(const std::string &openId)
    {
        std::uint64_t hash = 1469598103934665603ULL;
        for (unsigned char c : openId)
        {
            hash ^= c;
            hash *= 1099511628211ULL;
        }
        // 保留 63 位，保证转成 long long 是正数
        return static_cast<long long>(hash & 0x7FFFFFFFFFFFFFFFULL);
    }

    long long InternId(const std::string &openId)
    {
        if (openId.empty())
            return 0;

        std::lock_guard<std::mutex> lock(idMutex);

        auto hit = openIdToId.find(openId);
        if (hit != openIdToId.end())
            return hit->second;

        long long id = HashOpenId(openId);
        if (id == 0)
            id = 1;

        // 理论上的哈希冲突：线性探测直到可用
        while (idToOpenId.count(id) != 0 && idToOpenId[id] != openId)
        {
            id++;
            if (id <= 0)
                id = 1;
        }

        idToOpenId[id] = openId;
        openIdToId[openId] = id;
        return id;
    }

    std::string OpenIdOf(long long internId)
    {
        std::lock_guard<std::mutex> lock(idMutex);
        auto hit = idToOpenId.find(internId);
        return (hit == idToOpenId.end()) ? std::string() : hit->second;
    }

    bool HasOpenId(long long internId)
    {
        std::lock_guard<std::mutex> lock(idMutex);
        return idToOpenId.count(internId) != 0;
    }

    // ---------- access_token ----------

    static std::mutex tokenMutex;
    static std::string tokenValue;
    static std::chrono::steady_clock::time_point tokenRefreshAt;

    void InvalidateAccessToken()
    {
        std::lock_guard<std::mutex> lock(tokenMutex);
        tokenValue.clear();
        tokenRefreshAt = std::chrono::steady_clock::time_point();
    }

    std::string GetAccessToken(bool forceRefresh)
    {
        // 调试用：直接指定 token，跳过接口获取
        const char *fixedToken = std::getenv("QQ_BOT_ACCESS_TOKEN");
        if (fixedToken != nullptr && *fixedToken != '\0')
            return fixedToken;

        std::lock_guard<std::mutex> lock(tokenMutex);

        auto now = std::chrono::steady_clock::now();
        if (!forceRefresh && !tokenValue.empty() && now < tokenRefreshAt)
            return tokenValue;

        if (!IsConfigured())
        {
            logger.PrintError("QQBot: appId/clientSecret 未配置");
            return std::string();
        }

        httplib::Client cli = CreateTokenClient();

        json body;
        body["appId"] = config.appId;
        body["clientSecret"] = config.clientSecret;

        auto res = cli.Post("/app/getAppAccessToken", body.dump(), "application/json");
        if (!res)
        {
            logger.PrintError("QQBot: 请求 access_token 失败（网络错误）");
            return std::string();
        }
        if (res->status != 200)
        {
            logger.PrintError("QQBot: 获取 access_token 失败, HTTP " + std::to_string(res->status));
            logger.PrintError(res->body.substr(0, 300));
            return std::string();
        }

        json response;
        try
        {
            response = json::parse(res->body);
        }
        catch (const std::exception &e)
        {
            logger.PrintError(std::string("QQBot: access_token 响应解析失败: ") + e.what());
            return std::string();
        }

        if (!response.contains("access_token") || !response["access_token"].is_string())
        {
            logger.PrintError("QQBot: access_token 响应缺少字段");
            return std::string();
        }

        tokenValue = response["access_token"].get<std::string>();

        long long expiresIn = 7200;
        if (response.contains("expires_in"))
        {
            if (response["expires_in"].is_number())
                expiresIn = response["expires_in"].get<long long>();
            else if (response["expires_in"].is_string())
                expiresIn = std::atoll(response["expires_in"].get<std::string>().c_str());
        }
        if (expiresIn < 120)
            expiresIn = 120;

        // 提前 2 分钟刷新，避免踩点过期
        tokenRefreshAt = now + std::chrono::seconds(expiresIn - 120);
        return tokenValue;
    }

    // ---------- 被动回复窗口 ----------

    namespace
    {
        struct PendingReply
        {
            std::string msgId;
            std::chrono::steady_clock::time_point receivedAt;
            int usedCount = 0;
        };

        // key = (isGroup ? groupInternId : userInternId)
        std::mutex replyMutex;
        std::map<long long, PendingReply> pendingReplies;

        // 被动回复限制来自 AdapterConfig（默认值按官方文档，可用环境变量覆盖）：
        //   单聊 60 分钟 / 同一 msg_id 最多 4 次
        //   群聊 5 分钟 / 同一 msg_id 最多 5 次
        int ReplyWindowSeconds(bool isGroup)
        {
            return isGroup ? config.groupReplyWindowSeconds : config.privateReplyWindowSeconds;
        }

        int MaxReplyPerMsg(bool isGroup)
        {
            return isGroup ? config.groupMaxReplyPerMsg : config.privateMaxReplyPerMsg;
        }

        // 群和私聊的 ID 空间可能撞车，私聊存负值用于区分
        long long ReplyKey(long long target, bool isGroup)
        {
            return isGroup ? target : -target;
        }

        void PruneExpiredReplies()
        {
            auto now = std::chrono::steady_clock::now();
            for (auto it = pendingReplies.begin(); it != pendingReplies.end();)
            {
                // key 为正代表群聊，为负代表单聊
                bool isGroup = it->first > 0;
                auto age = std::chrono::duration_cast<std::chrono::seconds>(
                               now - it->second.receivedAt)
                               .count();
                if (age > ReplyWindowSeconds(isGroup))
                    it = pendingReplies.erase(it);
                else
                    ++it;
            }
        }
    }

    void RememberMsgId(long long target, bool isGroup, const std::string &msgId)
    {
        if (target == 0 || msgId.empty())
            return;

        std::lock_guard<std::mutex> lock(replyMutex);
        long long key = ReplyKey(target, isGroup);

        PendingReply reply;
        reply.msgId = msgId;
        reply.receivedAt = std::chrono::steady_clock::now();
        reply.usedCount = 0;
        pendingReplies[key] = reply;

        PruneExpiredReplies();
    }

    bool TakeMsgId(long long target, bool isGroup, std::string &msgId, int &msgSeq)
    {
        if (target == 0)
            return false;

        std::lock_guard<std::mutex> lock(replyMutex);
        PruneExpiredReplies();

        long long key = ReplyKey(target, isGroup);
        auto hit = pendingReplies.find(key);
        if (hit == pendingReplies.end())
            return false;

        if (hit->second.usedCount >= MaxReplyPerMsg(isGroup))
            return false;

        hit->second.usedCount++;
        msgId = hit->second.msgId;
        msgSeq = hit->second.usedCount;
        return true;
    }

    const char *NoGroupMemberListMessage()
    {
        return "QQ official API does not expose group member list";
    }
}
