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
        if (mode == 0 || mode == 2)
            std::cout << "["<<slb::GetCurrentTimeString()<<"]" << '\t' << text << std::endl;
        if (isOpenFile && (int)mode < 2)
        {
            logFile << "["<<slb::GetCurrentTimeString()<<"]" << "\t" << text << std::endl;
        }
    }

    template <typename T>
    void PrintError(T text)
    {
        if (mode == 0 || mode == 2)
            std::cout << "["<<slb::GetCurrentTimeString()<<"]" << "\tERROR\t" << text << std::endl;
        if (isOpenFile && (int)mode < 2)
        {
            logFile << "["<<slb::GetCurrentTimeString()<<"]" << "\tERROR\t" << text << std::endl;
        }
    }

    template <typename T>
    void PrintError(T text, T ERROR_CODE)
    {
        if (mode == 0 || mode == 2)
            std::cout << "["<<slb::GetCurrentTimeString()<<"]" << "\tERROR\t" << text << "\t ERRORCODE:" << ERROR_CODE << std::endl;
        if (isOpenFile && mode < 2)
        {
            logFile << "["<<slb::GetCurrentTimeString()<<"]" << text << "\t ERRORCODE:" << ERROR_CODE << std::endl;
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
    SLBLog* operator<<(T text)
    {
        PrintMessage(text);
        return this;
    }

};