#include "Network/ReportingServer/OneBot11.h"

#include "GlobalData.h"
#include <string>
#include <sstream>
#include <map>

#include "json.hpp"

#include "Message/MessageDefinition.h"
#include "Message/iMessageDefinition.h"
#include "Processor/MessageQueueProcessor.h"

#include "Processor/ProgramStop.h"

#include "util.h"

using std::string;
using std::map;
using std::chrono::system_clock;
using nlohmann::json;

map<string,int> postTypeMap = 
{
    {"meta_event",1},{"message",2},{"notice",3}
};
map<string,int> metaEventTypeMap = 
{
    {"heartbeat",1}
};
map<string,int> messageTypeMap = //sub type(99xx),message block(50xx)
{
    {"private",1},{"group",2},
    {"at",5001},{"shake",5002},{"text",5003},{"face",5004},{"image",5005},{"record",5006},
                {"video",5007},{"rps",5008},{"dice",5009},{"poke",5010},{"anonymous",5011},
                {"share",5012},{"contact",5013},{"location",5014},{"music",5015},
                {"reply",5016},{"forward",5017},{"node",5018},{"xml",5019},{"json",5020},
    {"friend",9901},{"group",9902},{"other",9903},{"normal",9951},
                {"anonymous",9952},{"notice",9953}//sub type(private:xx<50,group:xx>=50)
};
map<string,int> noticeTypeMap = //sub type(99xx)
{
    {"notify",1},
    {"poke",9901}
};

void MessageArrayProcessor(json,iMessageContent*,long long);

bool Server_OneBot11::Run(const char * host,int port)
{
    try
    {
        {
            std::thread temp( [this,host,port](){svr.listen(host,port);} );
            svr_thread.swap(temp);
        }
        isRun = true;
    }
    catch(const std::exception& e)
    {
        logger << "catch error in running server";
        logger.Print( e.what() );
        Stop_Error();
    }

    return true;

}

bool Server_OneBot11::Stop()
{

    AfterStop();

    svr.stop();

    if (svr_thread.joinable())
    {
        svr_thread.join();
    }

    return true;
}

bool Server_OneBot11::Init()
{
    {
        std::thread temp( [this](){HeartBeatProcessor();} );
        heartBeat_thread.swap(temp);
    }

    svr.Post("/", [this](const httplib::Request& req,httplib::Response& res){
        iMessageContent message;
        json body = json::parse(req.body.c_str());
        std::stringstream logText;
        res.status = 200;
        if (body["post_type"].is_string())//is json from onebot
        {
            string post_type = body["post_type"];
            long long self_id = body["self_id"];
            switch (postTypeMap[post_type])
            {
            case 1://meta_event
                //保留
                break;
            
            case 2://message
                if (body["message_type"].is_string())
                {   string message_type = body["message_type"];
                    std::string logtmp;
                    switch (messageTypeMap[message_type])
                    {
                    case 1://private
                        message.sendID.isGroup = false;
                        if (messageTypeMap[body["sub_type"]] == 9901)
                            message.sendID.subType = private_friend;
                        else if (messageTypeMap[body["sub_type"]] == 9902)
                            message.sendID.subType = private_group;
                        else if (messageTypeMap[body["sub_type"]] == 9903)
                            message.sendID.subType = private_other;

                        message.sendID.userID = body["user_id"];
                        // message_type = body["message"][0]["type"];
                        // if (message_type == "text")
                        // {
                        //     message_type = body["message"][0]["data"]["text"];
                        //     message.content = message_type.c_str();
                        // }

                        MessageArrayProcessor(body["message"],&message,self_id);

                        logText.str("");
                        logtmp = (message.sendID.messageType != at) ? "" : "(AT)";

                        logText << "收到私聊: " << " <- " << message.sendID.userID << ":" 
                            << logtmp << message.contentS; 

                        logger << logText.str();

                        message.sendID.messageType = normal;

                        ReceiveMessage(message);

                        break;
                    
                    case 2://group
                        if (body["message"][0]["type"].is_string())
                        {
                            message.sendID.isGroup = true;
                            // string s = body["message"][0]["type"];
                            // string qq = body["message"][0]["data"]["qq"];
                            // if ( s=="at" && atoi(qq.c_str()) == self_id )
                            // {
                            //     message.sendID.messageType = at;
                            // }
                            // if ( s == "shake" )
                            // {
                            //     message.sendID.messageType = poke;
                            // }
                            // if ( s == "text")
                            // {
                            //     qq = body["message"][0]["data"]["text"];
                            //     message.content = qq.c_str();
                            // }
                            int id = body["group_id"];
                            message.sendID.groupID = id;
                            message.sendID.userID = body["user_id"];

                            MessageArrayProcessor(body["message"],&message,self_id);

                            if (messageTypeMap[body["sub_type"]] == 9951)
                                message.sendID.subType = group_normal;
                            else if (messageTypeMap[body["sub_type"]] == 9952)
                                message.sendID.subType = group_anonymous;
                            else if (messageTypeMap[body["sub_type"]] == 9953)
                                message.sendID.subType = group_notice;

                            logText.str("");
                            logtmp = (message.sendID.messageType != at) ? "" : "(AT)";

                            logText << "收到群聊: " << " <- " << message.sendID.userID << ":" 
                                << logtmp << message.contentS; 

                            logger << logText.str();

                            ReceiveMessage(message);
                        }
                        break;

                    default:
                        break;
                    }
                }
                break;

            case 3://notice
                // if (body["notice_type"].is_string())
                // {   string notice_type = body["notice_type"];
                //     MessageContent message;
                //     if (notice_type == "notify")
                //     {
                        
                //         string sub_type = body["sub_type"];
                //         if (sub_type=="poke")
                //         {

                //             count++;//合规计数
                //             reply<< "Hit x" << count;
                //             if (body["group_id"].is_number())
                //             {
                //                 int id = body["group_id"];
                //                 json j;
                //                 j["group_id"] = id;
                //                 j["message"] = reply.str();
                //                 j["auto_escape"] = true;
                //                 Sleep(1000);
                //                 auto post_res = clientE.Post("/send_group_msg_rate_limited",j.dump(),"application/json");
                //                 if (post_res && post_res->status == 200) {
                //                     std::cout << "POST request succeeded: " << post_res->body << std::endl;
                //                 } else {
                //                     std::cout << "POST request failed" << std::endl;
                //                 }
                //             }
                //         }
                //     }
                // }
                break;

            default:
                break;
            }

        }
    });


    return true;
}

bool Server_OneBot11::Active()
{
    return true;
}

void Server_OneBot11::AfterStop()
{
    stop = true;

    if (heartBeat_thread.joinable()) heartBeat_thread.join();

    
}

void Server_OneBot11::HeartBeat()
{
    mutex_Beat.lock();
    lastHeartBeatTime = std::chrono::duration_cast<std::chrono::milliseconds>(system_clock::now().time_since_epoch());
    mutex_Beat.unlock();
}

void Server_OneBot11::HeartBeatProcessor()
{
    std::chrono::duration duration = std::chrono::duration_cast<std::chrono::milliseconds>(system_clock::now().time_since_epoch());
    while (!stop)
    {
        mutex_Beat.lock();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(duration - lastHeartBeatTime);
        mutex_Beat.unlock();

        if (duration.count() <= 3000)//3s
        {
            if(!active)
                active = true;
        }
        else
        {
            if(active)
                active = false;
        }

        slbUtils::Sleep(100);
    }
}

void MessageArrayProcessor(json body,iMessageContent* message,long long self_id)
{
    string content = "";
    if (body.is_array())
    {
    for (const auto item : body)
    {
        string msgType = item["type"];
        string tmp1="";
        switch (messageTypeMap[msgType])
        {
        case 5001://AT
            tmp1 = item["data"]["qq"];
            if ( atoll(tmp1.c_str()) == self_id)
                message->sendID.messageType = at;
            break;
        case 5003:
            if (item["data"]["text"].is_string())
            {
                tmp1 = item["data"]["text"];
                content += tmp1;
            }
            break;

        default:
            break;
        }
    }
        
    }

    message->contentS = content;
}