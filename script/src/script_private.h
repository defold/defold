#ifndef SCRIPT_PRIVATE_H
#define SCRIPT_PRIVATE_H

#include <dlib/hashtable.h>

#define SCRIPT_GET_URLS_CALLBACK "__script_get_urls_callback"
#define SCRIPT_CONTEXT "__script_context"

namespace dmScript
{
    struct Context
    {
        dmHashTable64<const dmDDF::Descriptor*> m_Descriptors;
    };
}

#endif // SCRIPT_PRIVATE_H
