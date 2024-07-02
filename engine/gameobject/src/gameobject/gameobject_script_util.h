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

#ifndef DM_GAMEOBJECT_SCRIPT_UTIL_H
#define DM_GAMEOBJECT_SCRIPT_UTIL_H

#include <gameobject/gameobject.h>

namespace dmLuaDDF
{
    struct LuaModule;
}

namespace dmGameObject
{
    bool   RegisterSubModules(dmResource::HFactory factory, dmScript::HContext script_context, dmLuaDDF::LuaModule* lua_module);
    Result LuaLoad(dmResource::HFactory factory, dmScript::HContext context, dmLuaDDF::LuaModule* module);
    int    LuaToPropertyOptions(lua_State* L, int index, PropertyOptions* property_options, dmhash_t property_id, bool* index_requested);
    int    CheckGetPropertyResult(lua_State* L, const char* module_name, dmGameObject::PropertyResult result, const PropertyDesc& property_desc, dmhash_t property_id, const dmMessage::URL& target, const dmGameObject::PropertyOptions& property_options, bool index_requested);
}

#endif // #ifndef DM_GAMEOBJECT_SCRIPT_UTIL_H
