#include "Plugin/PluginManager.h"

#include "SlackBBot.h"
#include "Processor/ProgramStop.h"
#include "Processor/MessageQueueProcessor.h"
#include "Processor/TerminalProcessor.h"

#include "FakeWorkers.h"

#include <map>
#include <filesystem>
#include <regex>

#include <mutex>

using std::filesystem::directory_iterator;

std::vector<std::string> targetPath;//Dynamic library in folder of plugins.
std::map<std::string,std::string> pluginPath;//<name,path>
std::map<int,void*> plugins;//plugin index, handle
std::map<int,void(*)()> pluginsStop;//plugin index, handle

std::mutex mutex_ProcessPlugin;

typedef const char * (*iPluginName)();
typedef void (*iRF)(int,const char *);
typedef void (*iRSF)(int,const char *, SpecialType, int);
typedef void (*iSM)(MessageContent);
typedef void (*iCW)(int, WorkerType, const char * );

typedef void (*iInit)(int,iRF,iRSF,iSM,iCW);
typedef void (*pInit)(); typedef pInit pStop;

typedef void (*callWorker_CallBack)(WorkerType,void*);

extern bool terminalRun;

//command
// typedef void (*Terminal_RC_Handle)(slb::CommandFunc,const char *, const char *,int);

//try to find floder in folder and wrtier path to "targetPath"
void SearchPluginInFolder()
{
    //Dynamic library file suffix
    std::regex DLibFileS(REGEX_PATTERN_DLIBFILES);
    for (auto const& dir_entry : directory_iterator(programPath + "plugin/"))
    {
        if (std::regex_match(dir_entry.path().u8string(),DLibFileS))
        {
            targetPath.push_back(dir_entry.path().u8string());
        }
    }
}

//尝试加载目录下所有插件
bool LoadAllPlugin()
{
    logger<<"Search Plugin...";

    SearchPluginInFolder();

    for (std::string i : targetPath)
    {
        LoadPlugin(i.c_str());
    }

    return true;
}

//尝试加载单个插件
bool LoadPlugin(const char * path)
{
    std::unique_lock<std::mutex> lock(mutex_ProcessPlugin);

    void* handle = SDL_LoadObject(path);
    iPluginName getPluginName;
    iInit iiInit;
    pInit Init;
    int index = plugins.size();

    pStop pstop;
    
    if (handle == nullptr) {
        logger.PrintError("Fail to load object: " + std::string(path));
        SDL_UnloadObject(handle);
    }
    else {
        try
        {
            getPluginName = (iPluginName)tryGetFunction(handle,"PluginName");
            iiInit = (iInit)tryGetFunction(handle,"iInit");
            Init = (pInit)tryGetFunction(handle,"Init");
        }
        catch(const char * e)
        {
            SDL_UnloadObject(handle);
            logger.PrintError(e);
            return false;
        }

        
        try
        {
            pstop = (pStop)tryGetFunction(handle,"PluginStop");
            pluginsStop[index] = pstop;
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return false;
        }
        catch(const char * e)
        {
            logger < path < " dont have a STOP function\n";
        }

        plugins[index] = handle;
        pluginPath[getPluginName()] = path;
        iiInit(index,RegisterFunction,RegisterSpecialFunction,SLB_SendMessage,CallWorker);

        std::ostringstream oss;
        oss <<"Load Plugin: " << getPluginName() << " from " << path;
        logger<<oss.str();

        Init();
    }
    return true;
}

void StopPlugin(int index)
{

}

void StopAllPlugin()
{

}

void* tryGetFunction(void* handle,const char* functionName)
{
    void* tmp = SDL_LoadFunction(handle,functionName);
    if (tmp == nullptr)
    {
        throw "Can't to load Function";
    }
    return tmp;
}

std::map<std::string,int> mapName =
{   //1xxx fillter - 1oxx 1=Receive 2=Send 
    {"Filter_AfterReceiveMessage",1101},
    {"Filter_BeforeSendMessage",1201}
};

Function_Extern void RegisterFunction(int i,const char * name)
{
    if (plugins.find(i) == plugins.end())
        return ;

    try
    {
        if (mapName.count(name)>0)
        {   //存在
            switch (mapName[std::string(name)])
            {
            case 1101:
                logger<<name;
                SLB_Filter_AfterReceiveMessage.push_back((Filter)tryGetFunction(plugins[i],name));
                logger<<std::string(name) + " OK";
                break;
            case 1201:
                logger<<name;
                SLB_Filter_BeforeSendMessage.push_back((Filter)tryGetFunction(plugins[i],name));
                logger<<std::string(name) + " OK";
                break;
            default:
                break;
            }
        }
        else
        {   //不存在

        }
    }
    catch(const char * e)
    {
        logger.PrintError(e);
    }
}

Function_Extern void RegisterSpecialFunction(int index,const char * name,SpecialType type,int parm)
{
    if (plugins.find(index) == plugins.end())
        return ;
    
    try {

    switch (type)
    {
    case Message_Normal:
        logger<<name;
        SLB_MessageNormal.push_back((Msg_Normal)tryGetFunction(plugins[index],name));
        break;
    
    case Message_Tirgger:
        IMessageType mType = IMessageType(parm);

        if (mType == normal) break;

        logger<<name;

        SLB_MessageTirgger[mType].push_back((Msg_Tirgger)tryGetFunction(plugins[index],name));
        break;

    }

    logger<<std::string(name) + " OK";

    } catch(const char * e) {logger.PrintError(e);};
}

Function_Extern void CallWorker(int index,WorkerType type,const char * argsInOne)
{
    json j;
    switch (type)
    {
    case CommandRegister:
    callWorker_CallBack CW;
    try
    {
        CW = (callWorker_CallBack)tryGetFunction(plugins[index],"CWorker_CallBack");
        if (terminalRun) CW(CommandRegister,(void*)RegisterCommand);
        else CW(CommandRegister,(void*)FakeRegisterCommand);
    }
    catch(const char * e)
    {
        logger.PrintError(e);
    }
        break;

    case getAccountInfo:
        j.parse(argsInOne);
        if (j["func"] == "GetGroupList")
            return (void)client->GetGroupMemberList(j["group"]).dump().c_str(); 
        break;
    
    default:
        break;
    }    
}