#include "Message/iMessageDefinition.h"

MessageContent ToMessageContent(iMessageContent *imc)
{
    MessageContent tmp;
    tmp = *imc;
    tmp.content = (*imc).contentS.c_str();

    return tmp;
}

iMessageContent ToIMessageContent(MessageContent mc)
{
    iMessageContent tmp;
    tmp.isEmpty = mc.isEmpty;
    tmp.sendID = mc.sendID;
    tmp.contentS = mc.content;
    return tmp;
}