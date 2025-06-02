#pragma once

#include "FunctionExtern.h"
#include <thread>

extern std::thread terminalMainThread;

namespace slb
{
    typedef void (*CommandFunc)(const char * arg);
}

bool TerminalInit();
void TerminalStop();

Function_Extern void RegisterCommand(slb::CommandFunc func,const char * name, const char * description,int fid);



