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
#include "gameobject_props.h"
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

    static int ParsePropertyOptionKey(lua_State* L, int index, PropertyOptions* options, bool* did_parse)
    {
        lua_getfield(L, index, "key");
        if (!lua_isnil(L, -1))
        {
            dmhash_t value = dmScript::CheckHashOrString(L, -1);
            if (!dmGameObject::AddPropertyOptionsKey(options, value))
            {
                return luaL_error(L, "Too many options supplied to options table (max=%d).", MAX_PROPERTY_OPTIONS_COUNT);
            }
            *did_parse = true;
        }
        lua_pop(L, 1);
        return 0;
    }

    static int ParsePropertyOptionIndex(lua_State* L, int index, PropertyOptions* options, bool* did_parse)
    {
        lua_getfield(L, index, "index");
        if (!lua_isnil(L, -1))
        {
            if (!lua_isnumber(L, -1))
            {
                return luaL_error(L, "Invalid number passed as index argument in options table.");
            }

            int32_t value = luaL_checkinteger(L, -1) - 1;
            if (value < 0)
            {
                return luaL_error(L, "Negative or zero number passed as index argument in options table (%d).", value);
            }
            if (!dmGameObject::AddPropertyOptionsIndex(options, value))
            {
                return luaL_error(L, "Too many options supplied to options table (max=%d).", MAX_PROPERTY_OPTIONS_COUNT);
            }
            *did_parse = true;
        }
        lua_pop(L, 1);
        return 0;
    }

    static int ParsePropertyOptionKeys(lua_State* L, int index, PropertyOptions* options, bool* did_parse)
    {
        lua_getfield(L, index, "keys");

        if (!lua_isnil(L, -1))
        {
            if (!lua_istable(L, -1))
            {
                return luaL_error(L, "'keys' must be a table when passed in as options table.");
            }

            int count = (int) lua_objlen(L, -1);
            if (count > MAX_PROPERTY_OPTIONS_COUNT)
            {
                return luaL_error(L, "Too many keys passed in to the 'keys' table: %d (max=%d).",
                    count, MAX_PROPERTY_OPTIONS_COUNT);
            }

            // Parse a simple list of keys, e.g. { "cone_emitter" }
            for (int i = 1; i <= count; ++i)
            {
                lua_rawgeti(L, -1, i);

                dmhash_t key_hash = dmScript::CheckHashOrString(L, -1);

                if (!dmGameObject::AddPropertyOptionsKey(options, key_hash))
                {
                    lua_pop(L, 1);
                    return luaL_error(L, "Too many options supplied to options table (max=%d).", MAX_PROPERTY_OPTIONS_COUNT);
                }

                lua_pop(L, 1);
            }

            *did_parse = true;
        }

        lua_pop(L, 1);
        return 0;
    }

    int LuaToPropertyOptions(lua_State* L, int index, LuaToPropertyOptionsResult* result)
    {
        luaL_checktype(L, index, LUA_TTABLE);
        lua_pushvalue(L, index);

        bool has_keys  = false;
        bool has_key   = false;
        bool has_index = false;
        PropertyOptions tmp_options;

        // All of these keywords are optional, but they cannot be mixed at the top level.
        // I.e, the root options table can only contain a single "key", "index" or "keys".

        ParsePropertyOptionKey(L, -1, &tmp_options, &has_key);
        ParsePropertyOptionIndex(L, -1, &tmp_options, &has_index);
        ParsePropertyOptionKeys(L, -1, &tmp_options, &has_keys);

        if ((has_keys + has_key + has_index) > 1)
        {
            return luaL_error(L, "Options table can only contain a single entry of either 'key', 'index' or 'keys', not multiple.");
        }

        result->m_Options        = tmp_options;
        result->m_IndexRequested = has_index;
        result->m_KeysRequested  = has_keys;

        lua_pop(L, 1);

        return 0;
    }

    int CheckGetPropertyResult(lua_State* L, const char* module_name, dmGameObject::PropertyResult result, const PropertyDesc& property_desc, dmhash_t property_id, const dmMessage::URL& target, const dmGameObject::PropertyOptions& property_options, bool index_requested, bool keys_requested)
    {
        DM_HASH_REVERSE_MEM(hash_ctx, 512);

        dmhash_t key = 0;
        int32_t index = 0;
        bool key_requsted = false;

        if (index_requested)
        {
            GetPropertyOptionsIndex((HPropertyOptions) &property_options, 0, &index);
        }
        else
        {
            key_requsted = GetPropertyOptionsKey((HPropertyOptions) &property_options, 0, &key) == dmGameObject::PROPERTY_RESULT_OK;
        }

        switch (result)
        {
        case dmGameObject::PROPERTY_RESULT_OK:
            {
                if (!keys_requested)
                {
                    if (index_requested && (property_desc.m_ValueType != dmGameObject::PROP_VALUE_ARRAY))
                    {
                        return luaL_error(L, "Options table contains index, but property '%s' is not an array.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                    }
                    else if (key_requsted && (property_desc.m_ValueType != dmGameObject::PROP_VALUE_HASHTABLE))
                    {
                        return luaL_error(L, "Options table contains key, but property '%s' is not a hashtable.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                    }
                }

                dmGameObject::LuaPushVar(L, property_desc.m_Variant);

                return 1;
            }
        case dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND:
            {
                if (key_requsted)
                {
                    return luaL_error(L, "Resource `%s` for property '%s' not found!", dmHashReverseSafe64Alloc(&hash_ctx, key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                else
                {
                    return luaL_error(L, "Property '%s' not found!", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
            }
        case dmGameObject::PROPERTY_RESULT_INVALID_INDEX:
            {
                if (key_requsted)
                {
                    return luaL_error(L, "Property '%s' is an array, but in options table specified key instead of index.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid index %d for property '%s'", index+1, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
        case dmGameObject::PROPERTY_RESULT_INVALID_KEY:
            {
                if (!key_requsted)
                {
                    return luaL_error(L, "Property '%s' is a hashtable, but in options table specified index instead of key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid key '%s' for property '%s'", dmHashReverseSafe64Alloc(&hash_ctx, key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
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

        dmhash_t key = 0;
        bool has_key = GetPropertyOptionsKey((HPropertyOptions) &property_options, 0, &key) == dmGameObject::PROPERTY_RESULT_OK;

        int32_t index = 0;
        GetPropertyOptionsIndex((HPropertyOptions) &property_options, 0, &index);

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
                if (has_key)
                {
                    return luaL_error(L, "Property '%s' is an array, but in options table specified key instead of index.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid index %d for property '%s'", index+1, dmHashReverseSafe64Alloc(&hash_ctx, property_id));
            }
            case dmGameObject::PROPERTY_RESULT_INVALID_KEY:
            {
                if (!has_key)
                {
                    return luaL_error(L, "Property '%s' is a hashtable, but in options table specified index instead of key.", dmHashReverseSafe64Alloc(&hash_ctx, property_id));
                }
                return luaL_error(L, "Invalid key '%s' for property '%s'", dmHashReverseSafe64Alloc(&hash_ctx, key), dmHashReverseSafe64Alloc(&hash_ctx, property_id));
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
