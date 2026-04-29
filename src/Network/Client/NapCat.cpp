#include "Network/Client/NapCat.h"

#include "GlobalData.h"

#include "json.hpp"

using nlohmann::json;

std::string clean_utf8(const char* data) {
    std::string result;
    while (*data != '\0') {
        uint8_t byte = static_cast<uint8_t>(*data);
        if ((byte <= 0x7F) || 
            ((byte & 0xE0) == 0xC0 && (data[1] & 0xC0) == 0x80) ||
            ((byte & 0xF0) == 0xE0 && (data[1] & 0xC0) == 0x80 && (data[2] & 0xC0) == 0x80) ||
            ((byte & 0xF8) == 0xF0 && (data[1] & 0xC0) == 0x80 && (data[2] & 0xC0) == 0x80 && (data[3] & 0xC0) == 0x80)) {
            result += *data;
        } else {
            result += '?'; // 替换非法字符
        }
        data++;
    }
    return result;
}

bool Napcat::Init()
{
    clnt = httplib::Client("127.0.0.1",3000);
    return true;
}

bool Napcat::iSendMessage(MessageContent message)
{
    json j;
    int i = 0;
    std::string tmp;

    j["auto_escape"] = true;
    if (message.sendID.isGroup)
        j["group_id"] = message.sendID.groupID;
    else
        j["user_id"] = message.sendID.userID;

    
    j["message"] = json::array();

    switch (message.sendID.messageType)
    {
    case at:
        j["message"][0]["type"] = "at";
        j["message"][0]["data"]["qq"] = message.sendID.userID;
        i++;
        break;
    
    default:
        break;
    }

    j["message"][i]["type"] = "text";
    tmp = message.content;
    j["message"][i]["data"]["text"] = tmp;

    if (message.sendID.isGroup)
        clnt.Post("/send_group_msg",j.dump(),"application/json");
    else
        clnt.Post("/send_private_msg",j.dump(),"application/json");

    return true;
}

json Napcat::GetGroupMemberList(long long groupID)
{
    json j,body,ref;
    j["group_id"] = groupID;
    j["no_cache"] = false; 
    auto res = clnt.Post("/get_group_member_list",j.dump(),"application/json");
    body.parse(res->body);

    ref["retcode"] = body["retcode"];
    if (body["retcode"] == 0 && ref["data"].is_array())
    {
        int i = 0;
        for(auto item : body["data"])
        {
            ref["data"][i]["user_id"] = item["user_id"];
            ref["data"][i]["nickname"] = item["nickname"];
            ref["data"][i]["card"] = item["card"];

            i++;
        }
    }
    else if (!ref["data"].is_array()) ref["retcode"] = -1;

    return ref;
}