#ifndef SCRIPT_PRIVATE_H
#define SCRIPT_PRIVATE_H

#include <dlib/hashtable.h>

#define SCRIPT_RESOLVE_PATH_CALLBACK "__script_resolve_path_callback"
#define SCRIPT_GET_URL_CALLBACK "__script_get_url_callback"
#define SCRIPT_GET_USER_DATA_CALLBACK "__script_get_user_data_callback"
#define SCRIPT_CONTEXT "__script_context"

namespace dmScript
{
    struct Module
    {
        char*       m_Script;
        uint32_t    m_ScriptSize;
        char*       m_Name;
        void*       m_UserData;
    };

    struct Context
    {
        dmConfigFile::HConfig   m_ConfigFile;
        dmHashTable64<Module>   m_Modules;
    };
}

#endif // SCRIPT_PRIVATE_H
