// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_GAMEOBJECT_SCRIPT_H
#define DM_GAMEOBJECT_SCRIPT_H

#include <dmsdk/gameobject/gameobject.h>

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}

namespace dmGameObject
{
    /*#
     * Get component user data from lua-argument. This function is typically used from lua-bindings
     * and can only be used from protected lua-calls as luaL_error might be invoked
     * @name GetComponentFromLua
     * @param L lua-state
     * @param index index to argument
     * @param collection in which to search
     * @param component_ext when specified, the call will fail if the found component does not have the specified extension
     * @param user_data will be overwritten component user-data output if available
     * @param url will be overwritten with a URL to the component when specified
     * @param world world associated when specified
     */
    void GetComponentFromLua(lua_State* L, int index, HCollection collection, const char* component_ext, dmGameObject::HComponent* out_user_data, dmMessage::URL* out_url, dmGameObject::HComponentWorld* world);

    /**
     * Get current game object instance from the lua state, if any.
     * The lua state has an instance while the script callbacks are being run on the state.
     * @param L lua-state
     * @return current game object instance
     */
    HInstance GetInstanceFromLua(lua_State* L);

}

#endif // DM_GAMEOBJECT_SCRIPT_H
