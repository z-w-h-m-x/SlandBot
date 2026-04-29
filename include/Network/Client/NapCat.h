#pragma once

#include "../ClientBase.h"

class Napcat : public IClient
{
    public:
        bool Init() override;
        bool iSendMessage(MessageContent) override;
        json GetGroupMemberList(long long) override;
};
