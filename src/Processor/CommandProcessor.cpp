#include "Processor/CommandProcessor.h"

#include <iostream>

#include "GlobalData.h"

#include "Config/CommandRuleConfig.h"

void RegisterCommandRule()
{
    using namespace qoe;
    //临时配置
    addCommandRule(ARG_Target_Port_ReportServer_T1,ARG_Port_ReportServer,ARG_RP_PRS);
    addCommandRule(ARG_Target_Port_ReportServer_T2,ARG_Port_ReportServer,ARG_RP_PRS);
    addCommandRule(ARG_Target_Port_NapServer,ARG_Port_NapServer,ARG_RP_NapP);

    addCommandRule(ARG_Target_Host_ReportServer_T1,ARG_Host_ReportServer,ARG_RP_HRS);
    addCommandRule(ARG_Target_Host_ReportServer_T2,ARG_Host_ReportServer,ARG_RP_HRS);
    addCommandRule(ARG_Target_Host_NapServer,ARG_Host_NapServer,ARG_RP_NapH);
    //适配器选择
    addCommandRule(ARG_Target_Adapter_T1,ARG_Adapter,ARG_RP_Adapter);
    addCommandRule(ARG_Target_Adapter_T2,ARG_Adapter,ARG_RP_Adapter);
    addCommandRule(ARG_Target_QQBot_AppID,ARG_QQBot_AppID,ARG_RP_QQBot_AppID);
    addCommandRule(ARG_Target_QQBot_Secret,ARG_QQBot_Secret,ARG_RP_QQBot_Secret);
    addCommandRule(ARG_Target_QQBot_Sandbox,ARG_QQBot_Sandbox,ARG_RP_QQBot_Sandbox);
    //测试
    addCommandRule("--test","t",false);
    //框架命令
    addCommandRule(ARG_Target_Save_Config,ARG_Save_Config,ARG_RP_Save);//保存配置
    addCommandRule(ARG_Target_DisableLogSaving,ARG_DisableLogSaving,ARG_RP_DisableLogSaving);//禁用文件日志
}

bool SetFollowCommand(qoe::commandArgData commandData)
{
    using namespace std;
    if (commandData["t"] == "1")
        cout<<"Hello,this test flag was target\n";
    //临时配置
    if (commandData[ARG_Port_ReportServer] != qoeCArgNull)
        serverConfig.port = stoi(commandData[ARG_Port_ReportServer]);
    if (commandData[ARG_Port_NapServer] != qoeCArgNull)
        serverConfig.napCatPort = stoi(commandData[ARG_Port_NapServer]);
    if (commandData[ARG_Host_ReportServer] != qoeCArgNull)
        serverConfig.host = commandData[ARG_Host_ReportServer];
    if (commandData[ARG_Host_NapServer] != qoeCArgNull)
        serverConfig.napCatHost = commandData[ARG_Host_NapServer];
    //适配器选择
    if (commandData[ARG_Adapter] != qoeCArgNull)
    {
        string adapter = commandData[ARG_Adapter];
        if (adapter == "qqbot" || adapter == "QQBot" || adapter == "qq")
            serverConfig.adapter = Data::AdapterType::QQBot;
        else if (adapter == "onebot11" || adapter == "onebot" || adapter == "napcat")
            serverConfig.adapter = Data::AdapterType::OneBot11;
        else
            cerr << "Unknown adapter: " << adapter
                 << " (expected onebot11 or qqbot), fallback to onebot11" << endl;
    }
    if (commandData[ARG_QQBot_AppID] != qoeCArgNull)
        serverConfig.qqBotAppId = commandData[ARG_QQBot_AppID];
    if (commandData[ARG_QQBot_Secret] != qoeCArgNull)
        serverConfig.qqBotClientSecret = commandData[ARG_QQBot_Secret];
    if (commandData[ARG_QQBot_Sandbox] == "1")
        serverConfig.qqBotSandbox = true;
    //命令
    if (commandData[ARG_DisableLogSaving] == "1")
        disableLogSaving = true;

    return true;
}