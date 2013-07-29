#include "extension.h"

namespace dmExtension
{
    Desc* g_FirstExtension = 0;

    void Register(Desc* desc) {
        desc->m_Next = g_FirstExtension;
        g_FirstExtension = desc;
    }

    const Desc* GetFirstExtension()
    {
        return g_FirstExtension;
    }
}

