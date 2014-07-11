#ifndef SCRIPT_PRIVATE_H
#define SCRIPT_PRIVATE_H

#include <dlib/hashtable.h>

#define SCRIPT_RESOLVE_PATH_CALLBACK "__script_resolve_path_callback"
#define SCRIPT_GET_URL_CALLBACK "__script_get_url_callback"
#define SCRIPT_GET_USER_DATA_CALLBACK "__script_get_user_data_callback"
#define SCRIPT_VALIDATE_INSTANCE_CALLBACK "__script_validate_instance_callback"
#define SCRIPT_CONTEXT "__script_context"
#define SCRIPT_MAIN_THREAD "__script_main_thread"

namespace dmScript
{
    struct Module
    {
        char*       m_Script;
        uint32_t    m_ScriptSize;
        char*       m_Name;
        void*       m_UserData;
    };

    #define DM_SCRIPT_MAX_EXTENSIONS (sizeof(uint32_t) * 8 * 16)
    struct Context
    {
        dmConfigFile::HConfig   m_ConfigFile;
        dmResource::HFactory    m_ResourceFactory;
        dmHashTable64<Module>   m_Modules;
        dmHashTable64<int>      m_HashInstances;
        uint32_t                m_InitializedExtensions[DM_SCRIPT_MAX_EXTENSIONS / (8 * sizeof(uint32_t))];
    };
}

#endif // SCRIPT_PRIVATE_H
