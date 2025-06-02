#include "Plugin_Base.h"

iRF RegisterFunction;
iRSF RegisterSpecialFunction;
iSM BotSendMessage;
int pIndex;
iRC RegisterCommand;
iCW CallWorker;

Plugin_Extern void iInit(int i,iRF rf, iRSF rsf,iSM sm, iCW cw)
{
    pIndex = i;
    RegisterFunction = rf;
    RegisterSpecialFunction = rsf;
    BotSendMessage = sm;
    CallWorker = cw;
}
Plugin_Extern void CWorker_CallBack(WorkerType type,void* worker)
{
    switch (type)
    {
    case CommandRegister:
        RegisterCommand = (iRC)worker;
        break;
    
    default:
        break;
    }
}

void SubmitFunction(const char * name)
{
    RegisterFunction(pIndex,name);
}
void SubmitSpecialFunction(SpecialType type,const char * name,int parm)
{
    RegisterSpecialFunction(pIndex,name,type,parm);
}

void SubmitMessageTrigger(IMessageType mType,const char * name)
{
    RegisterSpecialFunction(pIndex,name,Message_Tirgger,mType);
}
void SubmitCommand(CommandFunc func,const char * name,const char * description)
{
    RegisterCommand(func,name,description,pIndex);
}