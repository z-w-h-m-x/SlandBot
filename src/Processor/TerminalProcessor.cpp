#include "Processor/TerminalProcessor.h"
#include "Plugin/PluginManager.h"

#include <iostream>
#include <clocale>

#include <map>
#include <string>
#include <sstream>
#include <mutex>

#include "GlobalData.h"

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

std::thread terminalMainThread;

std::map<std::string,slb::CommandFunc> commands;
std::map<std::string,std::string> commandDescriptions;
std::map<std::string,int> commandFID;

std::mutex commandRegister;

bool stop = false;

void SLB_TP_Stop(const char * arg);
void SLB_TP_ListCommand(const char * arg);
void SLB_TP_Help(const char * arg);
void SLB_TP_PluginList(const char * arg);
void SLB_TP_LoadPlugin(const char * arg);

void TerminalThread()
{
    bool controller = false;
    using std::cout; using std::cin;
    std::string input;
    int breLogerMode;
    cout<<"Terminal interaction is enabled.\nEnter \"s\" to enter the controller.\nOr press Enter to stop the server ...\n";
    for(;!stop;)
    {
        if (controller) cout << "> ";
        std::getline(cin,input);
        std::istringstream ssinput(input.c_str());
        std::string firstInput;
        ssinput>>firstInput;
        if (input == "" && !controller) stop = true;
        if (input == "s" && !controller) 
        {
            cout<<"这段以后再翻译，中文先顶着\n控制器模式中日志不会打印到屏幕，但是还会输出到文件。\nEnter \"break\" to exit controller\nOK?Let's GOOOOOOO!\n";
            controller=true;
            breLogerMode = logger.mode;
            logger.mode = 1;
            goto TP0001;
        }
        if (firstInput == "break" && controller) 
        {
            controller = false;
            cout << "Enter \"s\" to enter the controller.\nOr press Enter to stop the server ...\n";
            logger.mode = 0;
        }
        if (commands.count(firstInput)>0 && controller)
        {
            input.erase(input.begin(), input.begin() + firstInput.size());
            commands[firstInput](input.c_str());
        }else if(controller)
        {
            cout << "The command \""<<firstInput<<"\" could not be found!\n";
        }
        TP0001:;
    }
}

void setUTF8Encoding() {
#ifdef _WIN32
    // 设置控制台代码页为UTF-8
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    // 解决Windows下换行符缓冲问题
    // setvbuf(stdout, nullptr, _IOFBF, 4096); 
#else
    if (!std::setlocale(LC_ALL, "en_US.UTF-8")) {
        std::cerr << "Warning: Failed to set en_US.UTF-8 locale\n";
    }
#endif
}

// void enableANSIControls() {
// #ifdef _WIN32
//     HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
//     DWORD dwMode = 0;
//     GetConsoleMode(hOut, &dwMode);
//     dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
//     SetConsoleMode(hOut, dwMode);
// #endif
// }

Function_Extern void RegisterCommand(slb::CommandFunc func,const char * name, const char * description,int fid)
{
    std::unique_lock<std::mutex> lock(commandRegister);
    commands[name] = func;
    commandDescriptions[name] = description;
    commandFID[name] = fid;
}

bool TerminalInit()
{
    setUTF8Encoding();

    //run
    std::thread tmp(TerminalThread);
    terminalMainThread.swap(tmp);

    RegisterCommand(SLB_TP_Stop,"stop","stop program",-1);
    RegisterCommand(SLB_TP_Help,"help","help",-1);
    RegisterCommand(SLB_TP_ListCommand,"command","list all command",-1);
    RegisterCommand(SLB_TP_LoadPlugin,"loadPlugin","try to load a plugin",-1);

    return true;
}

void TerminalStop()
{
    stop = true;
    if (terminalMainThread.joinable())
        terminalMainThread.join();
}

//--command--

void SLB_TP_Stop(const char * arg)
{
    stop = true;
}

void SLB_TP_ListCommand(const char * arg)
{
    std::cout << "break\texit controller\n";
    for (auto [key,val] : commandDescriptions)
    {
        std::cout << key << "\t" << val << std::endl;
    }
}

void SLB_TP_Help(const char * arg)
{
    std::cout << "Enter \"command\" to list all commands\n"; 
}

void SLB_TP_PluginList(const char * arg)
{
    
}
void SLB_TP_LoadPlugin(const char * arg)
{
    logger.mode = 0;
    LoadPlugin(arg);
    logger.mode = 1;
}



