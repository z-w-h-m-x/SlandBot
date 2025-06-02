#pragma once

#include "Plugin_FunctionDefinition.h"
#include "Message/MessageDefinition.h"

#ifdef _WIN32
#define Plugin_Extern extern "C" __declspec(dllexport)
#else
#define Plugin_Extern extern "C"
#endif
//dll导出定义

typedef void (*iRF)(int,const char *);
typedef void (*iRSF)(int,const char *, SpecialType, int);
typedef void (*iSM)(MessageContent);
typedef void (*iCW)(int, WorkerType, const char * );

typedef void (*CommandFunc)(const char * arg);
typedef void (*iRC)(CommandFunc func,const char * name, const char * description,int fid);

extern iRF RegisterFunction;
extern iRSF RegisterSpecialFunction;
extern iSM BotSendMessage;
extern int pIndex;
extern iRC RegisterCommand;
extern iCW CallWorker;

Plugin_Extern const char * PluginName();
Plugin_Extern void Init();

Plugin_Extern void iInit(int ,iRF, iRSF, iSM, iCW);
Plugin_Extern void CWorker_CallBack(WorkerType,void*);

void SubmitFunction(const char * name);
void SubmitSpecialFunction(SpecialType,const char * name,int parm);

void SubmitMessageTrigger(IMessageType,const char * name);

void SubmitCommand(CommandFunc func,const char * name,const char * description);