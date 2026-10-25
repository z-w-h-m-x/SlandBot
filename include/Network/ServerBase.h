#pragma once

#include <thread>

class IServer
{
    protected:
        bool active = false;

    public:
        bool isRun = false;
        virtual bool Run(const char * host,int port);
        virtual bool Stop();
        virtual bool Init();
        virtual bool Active();
        virtual void AfterStop();
};