// Copyright 2020-2023 The Defold Foundation
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

#include "gamesys.h"

#include <dlib/log.h>
#include <physics/physics.h>

#include <gamesys/gamesys_ddf.h>
#include "gamesys.h"
#include "gamesys_private.h"

#include "scripts/script_label.h"
#include "scripts/script_particlefx.h"
#include "scripts/script_tilemap.h"
#include "scripts/script_physics.h"
#include "scripts/script_sound.h"
#include "scripts/script_sprite.h"
#include "scripts/script_factory.h"
#include "scripts/script_collection_factory.h"
#include "scripts/script_resource.h"
#include "scripts/script_model.h"
#include "scripts/script_window.h"
#include "scripts/script_collectionproxy.h"
#include "scripts/script_buffer.h"
#include "components/comp_gui.h"

#include <dmsdk/gamesys/script.h>
#include <gameobject/script.h>

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmScript {
    static inline dmGameObject::HInstance GetGOInstance(lua_State* L)
    {
        dmGameObject::HInstance instance = dmGameObject::GetInstanceFromLua(L);
        if (instance == 0) {
            dmGui::HScene scene = dmGui::GetSceneFromLua(L);
            if (scene != 0) {
                instance = (dmGameObject::HInstance)dmGameSystem::GuiGetUserDataCallback(scene);
            }
        }
        return instance;
    }

    dmGameObject::HInstance CheckGOInstance(lua_State* L) {
        dmGameObject::HInstance instance = GetGOInstance(L);
        // No instance for render scripts, ignored
        if (instance == 0) {
            luaL_error(L, "no instance could be found in the current script environment");
        }
        return instance;
    }

    // Inspired by the internal function dmGameObject::ResolveInstance
    // Modified to support both gameobject/gui scripts
    dmGameObject::HInstance CheckGOInstance(lua_State* L, int instance_arg)
    {
        dmGameObject::HInstance instance = GetGOInstance(L);

        if (!lua_isnil(L, instance_arg)) {
            dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);

            dmMessage::URL receiver;
            dmScript::ResolveURL(L, instance_arg, &receiver, 0x0);
            if (receiver.m_Socket != dmGameObject::GetMessageSocket(collection))
            {
                luaL_error(L, "function called can only access instances within the same collection.");
            }

            instance = dmGameObject::GetInstanceFromIdentifier(collection, receiver.m_Path);
            if (!instance)
            {
                luaL_error(L, "Instance %s not found", lua_tostring(L, instance_arg));
                return 0; // Actually never reached
            }
        }
        return instance;
    }

    dmGameObject::HCollection CheckCollection(lua_State* L)
    {
        dmGameObject::HInstance instance = GetGOInstance(L);
        if (!instance)
            luaL_error(L, "Script context doesn't have a game object set");
        return instance ? dmGameObject::GetCollection(instance) : 0;
    }

    void GetComponentFromLua(lua_State* L, int index, const char* component_type, void** out_world, void** component, dmMessage::URL* url)
    {
        dmGameObject::HInstance instance = CheckGOInstance(L, index);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
        GetComponentUserDataFromLua(L, index, collection, component_type, (uintptr_t*)component, url, out_world);
    }
}


namespace dmGameSystem
{

    ScriptLibContext::ScriptLibContext()
    {
        memset(this, 0, sizeof(*this));
    }

    bool InitializeScriptLibs(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;

        int top = lua_gettop(L);
        (void)top;

        bool result = true;

        ScriptBufferRegister(context);
        ScriptLabelRegister(context);
        ScriptParticleFXRegister(context);
        ScriptTileMapRegister(context);
        ScriptPhysicsRegister(context);
        ScriptFactoryRegister(context);
        ScriptCollectionFactoryRegister(context);
        ScriptSpriteRegister(context);
        ScriptSoundRegister(context);
        ScriptResourceRegister(context);
        ScriptModelRegister(context);
        ScriptWindowRegister(context);
        ScriptCollectionProxyRegister(context);

        assert(top == lua_gettop(L));
        return result;
    }

    void FinalizeScriptLibs(const ScriptLibContext& context)
    {
        ScriptCollectionProxyFinalize(context);
        ScriptLabelFinalize(context);
        ScriptPhysicsFinalize(context);
        ScriptResourceFinalize(context);
        ScriptWindowFinalize(context);
    }

    dmGameObject::HInstance CheckGoInstance(lua_State* L) {
        return dmScript::CheckGOInstance(L);
    }


    void OnWindowFocus(bool focus)
    {
        ScriptWindowOnWindowFocus(focus);
        // We need to call ScriptWindowOnWindowFocus before ScriptSoundOnWindowFocus to
        // allow the is_music_playing() script function to return the correct result.
        // When the window activation is received the application sound is not yet playing
        // any sounds and the Android platform function will return the correct result.
        // Once ScriptSoundOnWindowFocus is called when focus is gained we always say
        // that background music is off if the game is playing music and the app has focus
        ScriptSoundOnWindowFocus(focus);
    }

    void OnWindowIconify(bool iconify)
    {
        ScriptWindowOnWindowIconify(iconify);
    }

    void OnWindowResized(int width, int height)
    {
        ScriptWindowOnWindowResized(width, height);
    }

    void OnWindowCreated(int width, int height)
    {
        ScriptWindowOnWindowCreated(width, height);
    }
}
