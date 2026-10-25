#include "Processor/ProgramStop.h"

#include <cstdlib>
#include <iostream>

/*
    发生影响程序运行的错误时停止。

    注意：这里不能走 exit()/ProgramQuit()，因为此时 main 里的终端线程、
    网关线程可能还处于 joinable 状态，正常的静态析构会触发
    "terminate called without an active exception" 并 core dump。
    用 _Exit 直接结束进程，语义上就是"错误终止"。
*/
void Stop_Error()
{
    std::cout.flush();
    std::cerr.flush();

#ifdef _WIN32
    // Windows 下保留 pause，方便双击运行的用户看到报错
    std::system("pause");
#endif

    std::_Exit(EXIT_FAILURE);
}
