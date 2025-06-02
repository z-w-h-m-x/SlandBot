#pragma once

#include "GlobalData.h"
#include "SDL2/SDL.h"
#include <map>

#include "Plugin/Plugin/Plugin_FunctionDefinition.h"

#include "FunctionExtern.h"

#ifdef _WIN32
#define REGEX_PATTERN_DLIBFILES "\\S+.[dD][lL][lL]$"
#else
#define REGEX_PATTERN_DLIBFILES "\\S+.[sS][oO]$"
#endif

//尝试加载目录下所有插件
bool LoadAllPlugin();
//尝试加载单个插件dll
bool LoadPlugin(const char *);

void* tryGetFunction(void*,const char*);

//由插件init过程调用
Function_Extern void RegisterFunction(int index,const char * name);
Function_Extern void RegisterSpecialFunction(int index,const char * name,SpecialType type,int parm);
Function_Extern void CallWorker(int index,WorkerType type,const char * argsInOne);