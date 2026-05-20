#pragma once

#include "MessageDefinition.h"

#include <string>

struct iMessageContent : MessageContent
{
    std::string contentS;
};

MessageContent ToMessageContent(iMessageContent*);
iMessageContent ToIMessageContent(MessageContent);