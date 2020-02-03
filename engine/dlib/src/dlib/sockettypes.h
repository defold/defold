#ifndef DM_SOCKETTYPES_H
#define DM_SOCKETTYPES_H

#if defined(__linux__) || defined(__MACH__) || defined(ANDROID) || defined(__EMSCRIPTEN__)
#include <sys/select.h>
#elif defined(_WIN32)
#include <winsock2.h>
#else
#error "Unsupported platform"
#endif

namespace dmSocket
{
    struct Selector
    {
        fd_set m_FdSets[3];
        int    m_Nfds;
        Selector();
    };	
}

#endif // DM_SOCKETTYPES_H