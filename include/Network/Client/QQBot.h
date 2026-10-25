#pragma once

#include "../ClientBase.h"

#include <string>

/*
    QQ 官方机器人客户端（QQ 开放平台 API）
    Official QQ Bot client

    和 Napcat 的区别：官方不发 /send_group_msg 这种 OneBot 请求，
    而是用 openid 走 v2 REST API，并且支持「被动回复」（带 msg_id）。
*/
class QQBotClient : public IClient
{
public:
    bool Init() override;
    bool iSendMessage(MessageContent) override;
    json GetGroupMemberList(long long) override;

private:
    bool PostMessage(const std::string &path, const std::string &content,
                     const std::string &msgId, int msgSeq, bool retryOnAuthError);
};
