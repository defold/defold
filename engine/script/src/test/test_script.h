#ifndef DM_SCRIPT_TEST_UTIL_H
#define DM_SCRIPT_TEST_UTIL_H

#include <jc_test/jc_test.h>

#include "script.h"

#include <dlib/array.h>
#include <dlib/configfile.h>
#include <resource/resource.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScriptTest
{
    bool RunFile(lua_State* L, const char* filename);
    bool RunString(lua_State* L, const char* script);

    class ScriptTest : public jc_test_base_class
    {
    public:
        virtual void SetUp();

        void FinalizeLogs();

        virtual void TearDown();
        char* GetLog();
        void AppendToLog(const char* log);

        bool RunFile(lua_State* L, const char* filename);
        bool RunString(lua_State* L, const char* script);

        dmScript::HContext m_Context;
        dmConfigFile::HConfig m_ConfigFile;
        dmResource::HFactory m_ResourceFactory;
        lua_State* L;
        dmArray<char> m_Log;
    };

}

#endif
