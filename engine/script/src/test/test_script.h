// Copyright 2020-2025 The Defold Foundation
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
    bool RunFile(lua_State* L, const char* filename, const char* base_dir);
    bool RunString(lua_State* L, const char* script);

    class ScriptTest : public jc_test_base_class
    {
    public:
        void SetUp() override;

        void FinalizeLogs();

        void TearDown() override;
        char* GetLog();
        void AppendToLog(const char* log);

        bool RunFile(lua_State* L, const char* filename);
        bool RunFile(lua_State* L, const char* filename, const char* base_dir);
        bool RunString(lua_State* L, const char* script);

        dmScript::HContext m_Context;
        dmConfigFile::HConfig m_ConfigFile;
        dmResource::HFactory m_ResourceFactory;
        lua_State* L;
        dmArray<char> m_Log;
    };

}

#endif
