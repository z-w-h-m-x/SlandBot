#include "Network/Client/QQBot.h"

#include "GlobalData.h"
#include "Network/QQBotCommon.h"

#include <cstdlib>
#include <cstring>
#include <string>

using nlohmann::json;

bool QQBotClient::Init()
{
    qqbot::LoadConfigFromEnv();

    if (!qqbot::IsConfigured())
    {
        logger.PrintError("QQBot: 缺少 appId/clientSecret，请用 --appid/--secret 或 QQ_BOT_APPID/QQ_BOT_SECRET 提供");
        return false;
    }

    // 启动时先取一次 token，凭据错能立刻暴露，而不是等第一条消息
    if (qqbot::GetAccessToken().empty())
    {
        logger.PrintError("QQBot: 初始化失败，无法获取 access_token");
        return false;
    }

    active = true;
    return true;
}

bool QQBotClient::PostMessage(const std::string &path, const std::string &content,
                              const std::string &msgId, int msgSeq,
                              bool retryOnAuthError)
{
    std::string token = qqbot::GetAccessToken();
    if (token.empty())
        return false;

    json body;
    body["content"] = content;
    body["msg_type"] = 0; // 0 = 纯文本
    if (!msgId.empty() && msgSeq > 0)
    {
        body["msg_id"] = msgId;
        body["msg_seq"] = msgSeq;
    }

    httplib::Client cli = qqbot::CreateApiClient();
    httplib::Headers headers;
    headers.emplace("Authorization", "QQBot " + token);

    auto res = cli.Post(path, headers, body.dump(), "application/json");
    if (!res)
    {
        logger.PrintError("QQBot: 发送消息失败（网络错误）");
        return false;
    }

    // token 失效：刷新后重试一次
    if (res->status == 401 && retryOnAuthError)
    {
        logger.PrintError("QQBot: access_token 已失效，尝试刷新后重发");
        qqbot::InvalidateAccessToken();
        return PostMessage(path, content, msgId, msgSeq, false);
    }

    if (res->status != 200)
    {
        logger.PrintError("QQBot: 发送消息失败, HTTP " + std::to_string(res->status));
        logger.PrintError(res->body.substr(0, 300));
        return false;
    }

    return true;
}

bool QQBotClient::iSendMessage(MessageContent message)
{
    if (message.content == nullptr)
        return true;

    std::string content = message.content;
    if (content.empty())
        return true; // 空消息不占用接口额度

    bool isGroup = message.sendID.isGroup;
    long long targetId = isGroup ? message.sendID.groupID : message.sendID.userID;
    std::string targetOpenId = qqbot::OpenIdOf(targetId);
    if (targetOpenId.empty())
    {
        logger.PrintError("QQBot: 找不到目标 openid，消息未发送（target=" +
                          std::to_string(targetId) + "）");
        return false;
    }

    // 群聊 @ 用官方语法 <@!openid>
    if (message.sendID.messageType == at && isGroup)
    {
        std::string atOpenId = qqbot::OpenIdOf(message.sendID.userID);
        if (!atOpenId.empty())
            content = "<@!" + atOpenId + "> " + content;
    }

    // 有被动回复窗口就带上 msg_id，避免消耗主动推送额度
    std::string msgId;
    int msgSeq = 0;
    qqbot::TakeMsgId(targetId, isGroup, msgId, msgSeq);

    std::string path = isGroup
                           ? ("/v2/groups/" + targetOpenId + "/messages")
                           : ("/v2/users/" + targetOpenId + "/messages");

    return PostMessage(path, content, msgId, msgSeq, true);
}

json QQBotClient::GetGroupMemberList(long long groupID)
{
    json result;
    result["retcode"] = -1;
    result["message"] = qqbot::NoGroupMemberListMessage();
    result["data"] = json::array();

    logger.Print("QQBot: 官方 API 不提供群成员列表，GetGroupMemberList 返回空");
    return result;
}
