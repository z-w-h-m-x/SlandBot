#pragma once

#include <vector>
#include <string>

#include "Data/ServerData.h"
#include "Network/ServerBase.h"
#include "Network/ClientBase.h"

#include "SLBLog.h"

//Global variable
extern Data::ServerData serverConfig;
extern std::string programPath;
extern std::string perfPath;
extern SLBLog logger;

extern IServer* server;
extern IClient* client;
extern httplib::Client clnt;

extern bool disableLogSaving;
extern bool debugMode;
extern SLBLOG_DEBUG debugLogger;

std::string LetCCtoString(const char *);