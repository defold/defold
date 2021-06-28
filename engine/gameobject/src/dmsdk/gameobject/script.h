// Copyright 2020 The Defold Foundation
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

#ifndef DMSDK_GAMEOBJECT_SCRIPT_H
#define DMSDK_GAMEOBJECT_SCRIPT_H

extern "C"
{
#include <dmsdk/lua/lua.h>
#include <dmsdk/lua/lauxlib.h>
}

namespace dmMessage {
    struct URL;
}

namespace dmGameObject
{
    /*# SDK GameObject script API documentation
     * [file:<dmsdk/gameobject/script.h>]
     *
     * @document
     * @name Script
     * @namespace dmGameObject
     */

    /*#
     * Get component user data from a url.
     * The object referenced by the url must be in the same collection as the caller.
     *
     * @name GetComponentFromLua
     * @param L [type:lua_State*] Lua state
     * @param index [type:lua_State*] index to argument (a url)
     * @param component_type [type:const char*] the call will fail if the found component does not have the specified extension
     * @param world [type:void**] The world associated owning the component. May be 0
     * @param component [type:void**] The component data associated with the url. May be 0
     * @param url [type:dmMessage::URL*] The resolved url. May be 0
     */
    void GetComponentFromLua(lua_State* L, int index, const char* component_type, void** world, void** component, dmMessage::URL* url);
}

#endif // DMSDK_GAMEOBJECT_SCRIPT_H
