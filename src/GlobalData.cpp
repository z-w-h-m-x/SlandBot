#include "GlobalData.h"

//Global variable
Data::ServerData serverConfig;
std::string programPath;
std::string perfPath;
SLBLog logger;

IServer* server;
IClient* client;
httplib::Client clnt("127.0.0.1");

bool disableLogSaving = false;
bool debugMode = false;

SLBLOG_DEBUG debugLogger(&logger,&debugMode);

std::string LetCCtoString(const char * c)
{
    std::string tmp;
    tmp = c;
    return tmp;
}