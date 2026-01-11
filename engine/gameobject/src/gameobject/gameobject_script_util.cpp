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

#include "gameobject.h"
#include "gameobject_private.h"
#include <stdint.h>
#include <ddf/ddf.h>
#include <resource/resource.h>
#include <dmsdk/gameobject/res_lua.h>
#include "../proto/gameobject/lua_ddf.h"
#include "gameobject_script_util.h"
#include "gameobject_props_lua.h"

namespace dmGameObject
{
    bool RegisterSubModules(dmResource::HFactory factory, dmScript::HContext script_context, dmLuaDDF::LuaModule* lua_module)
    {
        uint32_t n_modules = lua_module->m_Modules.m_Count;
        for (uint32_t i = 0; i < n_modules; ++i)
        {
            const char* module_resource = lua_module->m_Resources[i];
            const char* module_name = lua_module->m_Modules[i];
            LuaScript* module_script = 0;
            dmResource::Result r = dmResource::Get(factory, module_resource, (void**) (&module_script));
            if (r == dmResource::RESULT_OK)
            {
                HResourceDescriptor desc;
                r = dmResource::GetDescriptor(factory, module_resource, &desc);
                assert(r == dmResource::RESULT_OK);
                dmhash_t name_hash = ResourceDescriptorGetNameHash(desc);
                if (dmScript::ModuleLoaded(script_context, name_hash))
                {
                    dmResource::Release(factory, module_script);
                    continue;
                }

                if (!RegisterSubModules(factory, script_context, module_script->m_LuaModule))
                {
                    dmResource::Release(factory, module_script);
                    return false;
                }

                dmScript::Result sr = dmScript::AddModule(script_context, &module_script->m_LuaModule->m_Source, module_name, module_script, name_hash);
                if (sr != dmScript::RESULT_OK)
                {
                    dmResource::Release(factory, module_script);
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    Result LuaLoad(dmResource::HFactory factory, dmScript::HContext context, dmLuaDDF::LuaModule* module)
    {
        if (!dmGameObject::RegisterSubModules(factory, context, module) ) {
            dmLogError("Failed to load sub modules to module %s", module->m_Source.m_Filename);
            return dmGameObject::RESULT_COMPONENT_NOT_FOUND;
        }

        lua_State* L = dmScript::GetLuaState(context);
        int ret = dmScript::LuaLoad(L, &module->m_Source);
        if (ret != 0)
            return dmGameObject::RESULT_UNKNOWN_ERROR;

        dmScript::PCall(L, 0, 0);
        return dmGameObject::RESULT_OK;
    }

    int LuaToPropertyOptions(lua_State* L, int index, PropertyOptions* property_options, dmhash_t property_id, bool* index_requested)
    {
        luaL_checktype(L, index, LUA_TTABLE);
        lua_pushvalue(L, index);

        lua_getfield(L, -1, "key");
        if (!lua_isnil(L, -1))
        {
            property_options->m_Key = dmScript::CheckHashOrString(L, -1);
            property_options->m_HasKey = 1;
        }
        lua_pop(L, 1);

        lua_getfield(L, -1, "index");
        if (!lua_isnil(L, -1)) // make it optional
        {
            if (property_options->m_HasKey)
            {
                return luaL_error(L, "Options table cannot contain both 'key' and 'index'.");
            }
            if (!lua_isnumber(L, -1))
            {
                return luaL_error(L, "Invalid number passed as index argument in options table.");
            }

            property_options->m_Index = luaL_checkinteger(L, -1) - 1;

            if (property_options->m_Index < 0)
            {
                return luaL_error(L, "Negative numbers passed as index argument in options table (%d).", property_options->m_Index);
            }

            if (index_requested)
            {
                *index_requested = true;
            }
        }
        lua_pop(L, 1);

        lua_pop(L, 1);

        return 0;
    }

    int CheckGetPropertyResult(lua_State* L, const char* module_name, dmGameObject::PropertyResult result, const PropertyDesc& property_desc, dmhash_t property_id, const dmMessage::URL& target, const dmGameObject::PropertyOptions& property_options, bool index_requested)
    {
        DM_HASH_REVERSE_MEM(hash_ctx, 512);
        switch (result)
        {
        case dmGameObject::PROPERTY_RESULT_OK:
            {
                if (index_requested && (property_desc.m_ValueType != dmGameObject::PROP_VALUE_ARRAY))
                {
                    return luaL_error(L, "Options table contains index, but property '%s' is not an array.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                else if (property_options.m_HasKey && (property_desc.m_ValueType != dmGameObject::PROP_VALUE_HASHTABLE))
                {
                    return luaL_error(L, "Options table contains key, but property '%s' is not a hashtable.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }

                dmGameObject::LuaPushVar(L, property_desc.m_Variant);

                return 1;
            }
        case dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND:
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Resource `%s` for property '%s' not found!", dmHashReverseSafe64Alloc(&hash_ctx, property_options.m_Key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                else
                {
                    return luaL_error(L, "Property '%s' not found!", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
            }
        case dmGameObject::PROPERTY_RESULT_INVALID_INDEX:
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is an array, but in options table specified key instead of index.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid index %d for property '%s'", property_options.m_Index+1, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case dmGameObject::PROPERTY_RESULT_INVALID_KEY:
            {
                if (!property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is a hashtable, but in options table specified index instead of key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid key '%s' for property '%s'", dmHashReverseSafe64Alloc(&hash_ctx, property_options.m_Key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case dmGameObject::PROPERTY_RESULT_NOT_FOUND:
            {
                const char* path = dmHashReverseSafe64Alloc(&hash_ctx, target.m_Path);
                const char* property = dmHashReverseSafe64Alloc(&hash_ctx, property_id);
                if (target.m_Fragment)
                {
                    return luaL_error(L, "'%s#%s' does not have any property called '%s'", path, dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), property);
                }
                return luaL_error(L, "'%s' does not have any property called '%s'", path, property);
            }
        case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
            return luaL_error(L, "Could not find component '%s' when resolving '%s'", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), lua_tostring(L, 1));
        default:
            // Should never happen, programmer error
            return luaL_error(L, "%s.get failed with error code %d", module_name, result);
        }
        return 0;
    }

    int HandleGoSetResult(lua_State* L, dmGameObject::PropertyResult result, dmhash_t property_id, dmGameObject::HInstance target_instance, const dmMessage::URL& target, const dmGameObject::PropertyOptions& property_options)
    {
        DM_HASH_REVERSE_MEM(hash_ctx, 512);

        switch (result)
        {
            case dmGameObject::PROPERTY_RESULT_OK:
                return 0;
            case PROPERTY_RESULT_NOT_FOUND:
            {
                // The supplied URL parameter don't need to be a string,
                // we let Lua handle the "conversion" to string using concatenation.
                const char* name = "nil";
                if (!lua_isnil(L, 1))
                {
                    lua_pushliteral(L, "");
                    lua_pushvalue(L, 1);
                    lua_concat(L, 2);
                    name = lua_tostring(L, -1);
                    lua_pop(L, 1);
                }
                return luaL_error(L, "'%s' does not have any property called '%s'", name, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case PROPERTY_RESULT_UNSUPPORTED_TYPE:
            case PROPERTY_RESULT_TYPE_MISMATCH:
            {
                dmGameObject::PropertyDesc property_desc;
                dmGameObject::GetProperty(target_instance, target.m_Fragment, property_id, property_options, property_desc);
                return luaL_error(L, "the property '%s' of '%s' must be a %s", dmHashReverseSafe64Alloc(&hash_ctx, property_id), lua_tostring(L, 1), dmGameObject::TYPE_NAMES[property_desc.m_Variant.m_Type]);
            }
            case PROPERTY_RESULT_READ_ONLY:
            {
                return luaL_error(L, "Unable to set the property '%s' since it is read only", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_INVALID_INDEX:
            {
                if (property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is an array, but in options table specified key instead of index.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid index %d for property '%s'", property_options.m_Index+1, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_INVALID_KEY:
            {
                if (!property_options.m_HasKey)
                {
                    return luaL_error(L, "Property '%s' is a hashtable, but in options table specified index instead of key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid key '%s' for property '%s'", dmHashReverseSafe64Alloc(&hash_ctx, property_options.m_Key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_COMP_NOT_FOUND:
                return luaL_error(L, "could not find component '%s' when resolving '%s'", dmHashReverseSafe64Alloc(&hash_ctx, target.m_Fragment), lua_tostring(L, 1));
            case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE:
                return luaL_error(L, "go.set failed because the value is unsupported");
            case dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION:
                return luaL_error(L, "could not perform unsupported operation on '%s'", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            default:
                // Should never happen, programmer error
                return luaL_error(L, "go.set failed with error code %d", result);
        }

        return 0;
    }
}


