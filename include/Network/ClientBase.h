#pragma once

#include "httplib.h"
#include "json.hpp"
#include "Message/MessageDefinition.h"

using nlohmann::json;

class IClient
{
    protected:
        bool active = false;
    public:
        virtual bool Init() = 0;
        virtual bool iSendMessage(MessageContent) = 0;
        virtual json GetGroupMemberList(long long) = 0;
};