#ifndef SCRIPT_PRIVATE_H
#define SCRIPT_PRIVATE_H

#include <dlib/hashtable.h>

#define SCRIPT_RESOLVE_PATH_CALLBACK "__script_resolve_path_callback"
#define SCRIPT_GET_URL_CALLBACK "__script_get_url_callback"
#define SCRIPT_GET_USER_DATA_CALLBACK "__script_get_user_data_callback"
#define SCRIPT_CONTEXT "__script_context"

namespace dmScript
{
    struct Context
    {
        dmHashTable64<const dmDDF::Descriptor*> m_Descriptors;
    };
}

#endif // SCRIPT_PRIVATE_H
