#pragma once

#include "../ServerBase.h"

#include <ctime>
#include <chrono>
#include <mutex>

#include "httplib.h"

class Server_OneBot11 : public IServer 
{
    private:
        std::thread heartBeat_thread;
        bool stop = false;
        std::chrono::milliseconds lastHeartBeatTime;
        void HeartBeatProcessor();
        std::mutex mutex_Beat;
        u_int count = 0;
    protected:
        httplib::Server svr;
        std::thread svr_thread;

    public:
        bool Run(const char * host,int port) override;
        bool Stop() override;
        bool Init() override;
        bool Active() override;
        void AfterStop() override;
        
        void HeartBeat();
};