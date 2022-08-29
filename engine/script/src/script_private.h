// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef SCRIPT_PRIVATE_H
#define SCRIPT_PRIVATE_H

#include <dlib/hashtable.h>

#define SCRIPT_MAIN_THREAD "__script_main_thread"
#define SCRIPT_ERROR_HANDLER_VAR "__error_handler"

namespace dmScript
{
///////////////////////////////////////////////////////////////
// NOTE: Helper functions to get more logging on issue DEF-3714
#define PUSH_TABLE_LOGGER_CAPACITY 128
#define PUSH_TABLE_LOGGER_STR_SIZE PUSH_TABLE_LOGGER_CAPACITY+1

    struct PushTableLogger
    {
        char m_Log[PUSH_TABLE_LOGGER_STR_SIZE]; // +1 for \0
        const char* m_BufferStart;
        size_t m_BufferSize;
        uint32_t m_Size;
        uint32_t m_Cursor;
        PushTableLogger() {
            memset(m_Log, 0x0, sizeof(m_Log));
            m_BufferStart = 0;
            m_BufferSize = 0;
            m_Size = 0;
            m_Cursor = 0;
        };
    };

#ifdef __GNUC__
    void PushTableLogFormat(PushTableLogger& logger, const char *format, ...)
    __attribute__ ((format (printf, 2, 3)));
#else
    void PushTableLogFormat(PushTableLogger& logger, const char *format, ...);
#endif

    void PushTableLogString(PushTableLogger& logger, const char* s);
    void PushTableLogChar(PushTableLogger& logger, char c);
    void PushTableLogPrint(PushTableLogger& logger, char out[PUSH_TABLE_LOGGER_STR_SIZE]);
// End DEF-3714 helper functions.
///////////////////////////////////////////////////////////////

    struct Module
    {
        char*       m_Script;
        uint32_t    m_ScriptSize;
        char*       m_Name;
        void*       m_Resource;
        char*       m_Filename;
    };

    typedef struct ScriptExtension* HScriptExtension;

    struct Context
    {
        dmConfigFile::HConfig       m_ConfigFile;
        dmResource::HFactory        m_ResourceFactory;
        dmHashTable64<Module>       m_Modules;
        dmHashTable64<Module*>      m_PathToModule;
        dmHashTable64<int>          m_HashInstances;
        dmArray<HScriptExtension>   m_ScriptExtensions;
        lua_State*                  m_LuaState;
        int                         m_ContextTableRef;
        bool                        m_EnableExtensions;
    };

    HContext GetScriptContext(lua_State* L);

    bool ResolvePath(lua_State* L, const char* path, uint32_t path_size, dmhash_t& out_hash);

    bool GetURL(lua_State* L, dmMessage::URL& out_url);

    bool IsValidInstance(lua_State* L);

    /**
     * Remove all modules.
     * @param context script context
     */
    void ClearModules(HContext context);
}

#endif // SCRIPT_PRIVATE_H
