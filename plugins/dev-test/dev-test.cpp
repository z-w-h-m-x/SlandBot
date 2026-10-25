#include "dev-test.h"

Plugin_Extern const char * PluginName()
{
    return "Plugin_dev-test";
}

Plugin_Extern void Init()
{
    SubmitFunction("Filter_AfterReceiveMessage");
}

Plugin_Extern bool Filter_AfterReceiveMessage(MessageContent message)
{
    MessageContent tmp = message;
    if (message.sendID.isGroup && message.sendID.messageType == at)
    {
        tmp.content = "SlackBot A1 TEST.";
        BotSendMessage(tmp);
    }
    if (!message.sendID.isGroup)
    {
        tmp.content = "SlackBot A1 TEST.";
        BotSendMessage(tmp);
    }
    return true;
}
