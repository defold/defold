#ifndef DM_SOCKET_PRIVATE_H
#define DM_SOCKET_PRIVATE_H

namespace dmSocket
{
#if defined(__linux__) || defined(__MACH__) || defined(__EMSCRIPTEN__) || defined(__AVM2__)
    #define DM_SOCKET_ERRNO errno
    #define DM_SOCKET_HERRNO h_errno
#else
    #define DM_SOCKET_ERRNO WSAGetLastError()
    #define DM_SOCKET_HERRNO WSAGetLastError()
#endif
}

#endif // #ifndef DM_SOCKET_PRIVATE_H

