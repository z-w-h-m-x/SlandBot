/*
    SlandBot
    Copyright (C) 2024 SlandBotDevelopers

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

*/

#include <iostream>

#include "SlackBBot.h"
#include "Processor/ProgramStop.h"

#include "httplib.h"

// #ifdef _WIN32
// #include "windows.h"
// #elif __linux__
// #include "unistd.h"
// #endif

#include "Data/Data.h"
#include "Processor/CommandProcessor.h"
#include "Processor/InitializingFramework.h"
#include "Plugin/PluginManager.h"
#include "Network/ReportingServer/OneBot11.h"
#include "Network/Client/NapCat.h"
#include "Network/ReportingServer/QQBotGateway.h"
#include "Network/Client/QQBot.h"
#include "Network/QQBotCommon.h"
#include "Processor/MessageQueueProcessor.h"
#include "Processor/TerminalProcessor.h"

using namespace std;

#undef main

bool terminalRun;

int main(int argc,char* argv[])
{
    // 初始化SDL
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    //获取配置目录
    perfPath = SDL_GetPrefPath("SlandBotDev","SlandBot");
    programPath = SDL_GetBasePath();

    //命令行
    RegisterCommandRule();
    SetFollowCommand(qoe::AnalysisArgOptToMap(argc,argv));

    SLB_Init();//初始化

    cout<<ProjectName<<" "<<ProjectVersion<<endl
        <<"Built in "<<CMAKE_SYSTEM_NAME<<endl;
    
    //终端控制台启动
    terminalRun = TerminalInit();

    if ( !LoadAllPlugin() )
    {
        logger.PrintError("The plugin loading state is abnormal");
    }

    Server_OneBot11 OneBot11;
    Server_QQBotGateway qqBotGateway;
    Napcat napcatC;
    QQBotClient qqBotC;

    //选择接口适配器
    if (serverConfig.adapter == Data::AdapterType::QQBot)
    {
        qqbot::config.appId = serverConfig.qqBotAppId;
        qqbot::config.clientSecret = serverConfig.qqBotClientSecret;
        qqbot::config.sandbox = serverConfig.qqBotSandbox;

        server = &qqBotGateway;
        client = &qqBotC;

        logger << "Adapter: QQBot (QQ official open platform)";
    }
    else
    {
        server = &OneBot11;
        client = &napcatC;

        logger << "Adapter: OneBot11 + NapCat";
    }

    if (!server->Init())
    {
        logger.PrintError("Fail to init server adapter");
        Stop_Error();
    }
    server->Run(serverConfig.host.c_str(), serverConfig.port);

    if (!client->Init())
    {
        logger.PrintError("Fail to init client adapter");
        Stop_Error();
    }

    SLB_MessageProcessorInit();

    if (!terminalRun)
    {
        std::cout << "Press Enter to stop the server..." << std::endl;
        std::cin.get();  // 等待用户按下回车键
    }
    else
    {
        if(terminalMainThread.joinable())
            terminalMainThread.join();
    }

    Stop();
}

void Stop()
{
    logger.Screen("wait server close");
    server->Stop();
    SLB_MessageProcessorStop();
    TerminalStop();
    ProgramQuit();
}

void ProgramQuit()
{
    exit(0);
}