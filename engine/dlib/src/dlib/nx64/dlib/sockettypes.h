#ifndef DM_SOCKETTYPES_H
#define DM_SOCKETTYPES_H

#include <nn/socket/socket_Api.h>

namespace dmSocket
{
    struct Selector
    {
    	nn::socket::FdSet m_FdSets[3];;
        int    m_Nfds;
        Selector();
    };
}

#endif // DM_SOCKETTYPES_H
