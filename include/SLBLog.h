#pragma once

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>

namespace slb
{

std::string GetCurrentTimeString();

}

class SLBLog
{
private:
    bool isOpenFile=false;
    std::ofstream logFile;
    std::string lastTime;
public:

    int mode = 0;
    /*
        * 0 : ALL
        * 1 : OnlyFile
        * 2 : OnlyPrint
    */

    void CloseFileStream();

    void Close();

    bool File_Init(const char * filePath);

    template <typename T>
    void PrintMessage(T text)
    {
        lastTime = slb::GetCurrentTimeString();
        if (mode == 0 || mode == 2)
            std::cout << "["<<lastTime<<"]" << '\t' << text << std::endl;
        if (isOpenFile && (int)mode < 2)
        {
            logFile << "["<<lastTime<<"]" << "\t" << text << std::endl;
        }
    }

    template <typename T>
    void PrintMessageNL(T text)
    {
        lastTime = slb::GetCurrentTimeString();
        if (mode == 0 || mode == 2)
            std::cout << "["<<lastTime<<"]" << '\t' << text ;
        if (isOpenFile && (int)mode < 2)
        {
            logFile << "["<<lastTime<<"]" << "\t" << text ;
        }
    }

    template <typename T>
    void PrintError(T text)
    {
        lastTime = slb::GetCurrentTimeString();
        if (mode == 0 || mode == 2)
            std::cout << "["<<lastTime<<"]" << "\tERROR\t" << text << std::endl;
        if (isOpenFile && (int)mode < 2)
        {
            logFile << "["<<lastTime<<"]" << "\tERROR\t" << text << std::endl;
        }
    }

    template <typename T>
    void PrintError(T text, T ERROR_CODE)
    {
        lastTime = slb::GetCurrentTimeString();
        if (mode == 0 || mode == 2)
            std::cout << "["<<lastTime<<"]" << "\tERROR\t" << text << "\t ERRORCODE:" << ERROR_CODE << std::endl;
        if (isOpenFile && mode < 2)
        {
            logFile << "["<<lastTime<<"]" << text << "\t ERRORCODE:" << ERROR_CODE << std::endl;
        }
    }

    template <typename T>
    void Screen(T text)
    {
        std::cout << text << std::endl;
    }

    template <typename T>
    void Print(T text)
    {
        if (mode == 0 || mode == 2)
            std::cout << text << std::endl;
        if (isOpenFile && mode < 2)
        {
            logFile << text << std::endl;
        }
    }

    template <typename T>
    SLBLog& operator<<(T text)
    {
        PrintMessage(text);
        return *this;
    }

    template <typename T>
    SLBLog& operator<(T text)
    {
        PrintMessageNL(text);
        return *this;
    }

};

class SLBLOG_DEBUG
{
public:
    SLBLog *logerPoint;
    bool *debugPoint;

    template <typename T>
    SLBLOG_DEBUG& operator<<(T text)
    {
        if (debugPoint)
            (*logerPoint) << text;
        return *this;
    }

    template <typename T>
    SLBLOG_DEBUG& operator<(T text)
    {
        if (debugPoint)
            (*logerPoint) < text;
        return *this;
    }

    SLBLOG_DEBUG(SLBLog *lp,bool *dp)
    {
        logerPoint = lp;
        debugPoint = dp;
    }
};