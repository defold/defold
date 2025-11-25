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

#ifndef __GAMEOBJECTSCRIPT_H__
#define __GAMEOBJECTSCRIPT_H__

#include <dlib/array.h>

#include <script/script.h>

#include <resource/resource.h>
#include "gameobject.h"
#include "gameobject_props.h"

#include "../proto/gameobject/lua_ddf.h"

/**
 * Private header for GameObject
 */

namespace dmGameObject
{
    struct Instance;
    struct UpdateContext;
    typedef Instance* HInstance;

    enum ScriptResult
    {
        SCRIPT_RESULT_FAILED = -1,
        SCRIPT_RESULT_NO_FUNCTION = 0,
        SCRIPT_RESULT_OK = 1
    };

    enum ScriptFunction
    {
        SCRIPT_FUNCTION_INIT,
        SCRIPT_FUNCTION_FINAL,
        SCRIPT_FUNCTION_UPDATE,
        SCRIPT_FUNCTION_LATE_UPDATE,
        SCRIPT_FUNCTION_FIXED_UPDATE,
        SCRIPT_FUNCTION_ONMESSAGE,
        SCRIPT_FUNCTION_ONINPUT,
        SCRIPT_FUNCTION_ONRELOAD,
        MAX_SCRIPT_FUNCTION_COUNT
    };

    extern const char* SCRIPT_FUNCTION_NAMES[MAX_SCRIPT_FUNCTION_COUNT];

    struct Script
    {
        lua_State*              m_LuaState;
        int                     m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
        PropertySet             m_PropertySet;
        dmLuaDDF::LuaModule*    m_LuaModule;
        int                     m_InstanceReference;
        // Resources referenced through property values in the script
        dmArray<void*>          m_PropertyResources;
    };

    typedef Script* HScript;

    struct ScriptInstance
    {
        HScript     m_Script;
        Instance*   m_Instance;
        dmScript::HScriptWorld m_ScriptWorld;
        HProperties m_Properties;

        int         m_InstanceReference;
        int         m_ScriptDataReference;
        int         m_ContextTableReference;
        uint32_t    m_UniqueScriptId;

        uint16_t    m_ComponentIndex;
        uint8_t    m_Update       : 1;
        uint8_t    m_Initialized  : 1;
        uint8_t    m_Padding      : 6;
    };

    struct CompScriptWorld
    {
        CompScriptWorld(uint32_t max_instance_count);

        dmArray<ScriptInstance*> m_Instances;
        dmScript::HScriptWorld m_ScriptWorld;
    };

    void    InitializeScript(HRegister regist, dmScript::HContext context);

    HScript NewScript(lua_State* L, dmLuaDDF::LuaModule* lua_module);
    bool    ReloadScript(HScript script, dmLuaDDF::LuaModule* lua_module);
    void    DeleteScript(HScript script);

    HScriptInstance NewScriptInstance(CompScriptWorld* script_world, HScript script, HInstance instance, uint16_t component_index);
    void            DeleteScriptInstance(HScriptInstance script_instance);

    PropertyResult PropertiesToLuaTable(HInstance instance, HScript script, const HProperties properties, lua_State* L, int index);
}

#endif //__GAMEOBJECTSCRIPT_H__
