// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_GAMEOBJECT_RES_LUA_H
#define DMSDK_GAMEOBJECT_RES_LUA_H

#include <stdint.h>

#include <ddf/ddf.h>
#include <gameobject/lua_ddf.h>

namespace dmGameObject
{
    struct LuaScript
    {
        LuaScript(dmLuaDDF::LuaModule* lua_module) :
            m_LuaModule(lua_module) {}

        dmLuaDDF::LuaModule* m_LuaModule;
    };
}

#endif // DMSDK_GAMEOBJECT_RES_LUA_H
