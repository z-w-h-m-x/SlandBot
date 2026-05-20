#include "SLBLog.h"

#include <ctime>
#include <chrono>
#include <sstream>


using namespace std;

namespace slb
{

std::string GetCurrentTimeString()
{
    using std::chrono::system_clock;

    system_clock::time_point now = system_clock::now();
    std::time_t now_time_t = system_clock::to_time_t(now);
    std::tm* now_tm = localtime(&now_time_t);

    chrono::nanoseconds ns;

    ns = chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()) % 1000000000;

    std::string tmp;

    std::ostringstream oss;
    oss << (now_tm->tm_year + 1900) << "."; // tm_year 是从 1900 年开始的年数
    oss << (now_tm->tm_mon + 1) << ".";     // tm_mon 是从 0 开始的月份
    oss << now_tm->tm_mday << " ";
    oss << now_tm->tm_hour << ":";
    oss << now_tm->tm_min << ":";
    oss << now_tm->tm_sec << ".";
    oss << std::setw(9) << std::setfill('0') << ns.count();

    return oss.str();

}

}

void SLBLog::CloseFileStream()
{
    logFile.close();
}

void SLBLog::Close()
{
    if (isOpenFile)
        CloseFileStream();
}

bool SLBLog::File_Init(const char *filePath)
{
    if (mode == 2) return true;

    logFile.open(filePath,ios::app);
    if (!logFile.good())
    {
        CloseFileStream();
        return false;
    }
    isOpenFile = true;
    return true;
}
