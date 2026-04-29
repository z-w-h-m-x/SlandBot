#include "util.h"


#ifdef _WIN32
#include "windows.h"
#else
#include "unistd.h"
#endif


namespace slbUtils
{
    void Sleep(int ms)
    {

#ifdef _WIN32
        Sleep(ms);
#else
        usleep(ms*1000);
#endif
    }
}