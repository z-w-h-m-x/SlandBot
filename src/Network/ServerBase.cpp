#include "Network/ServerBase.h"

#include "GlobalData.h"
#include "Processor/ProgramStop.h"

bool IServer::Run(const char * host,int port)
{
    return false;
}

bool IServer::Stop()
{
    return true;
}

void IServer::AfterStop() {}

bool IServer::Init() {return true;}
bool IServer::Active() {return true;}