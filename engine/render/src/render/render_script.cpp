// Copyright 2020-2022 The Defold Foundation
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

#include "render_script.h"

#include <string.h>
#include <new>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/profile.h>

#include <script/script.h>
#include <script/lua_source_ddf.h>

#include "font_renderer.h"
#include "render/render_ddf.h"

namespace dmRender
{
    /*# Rendering API documentation
     *
     * Rendering functions, messages and constants. The "render" namespace is
     * accessible only from render scripts.
     *
     * The rendering API was originally built on top of OpenGL ES 2.0, and it uses a subset of the
     * OpenGL computer graphics rendering API for rendering 2D and 3D computer
     * graphics. Our current target is OpenGLES 3.0 with fallbacks to 2.0 on some platforms.
     *
     * [icon:attention] It is possible to create materials and write shaders that
     * require features not in OpenGL ES 2.0, but those will not work cross platform.
     *
     * @document
     * @name Render
     * @namespace render
     */

    #define RENDER_SCRIPT_INSTANCE "RenderScriptInstance"
    #define RENDER_SCRIPT "RenderScript"

    #define RENDER_SCRIPT_CONSTANTBUFFER "RenderScriptConstantBuffer"
    #define RENDER_SCRIPT_CONSTANTBUFFER_ARRAY "RenderScriptConstantBufferArray"

    #define RENDER_SCRIPT_PREDICATE "RenderScriptPredicate"

    #define RENDER_SCRIPT_LIB_NAME "render"
    #define RENDER_SCRIPT_FORMAT_NAME "format"
    #define RENDER_SCRIPT_WIDTH_NAME "width"
    #define RENDER_SCRIPT_HEIGHT_NAME "height"
    #define RENDER_SCRIPT_MIN_FILTER_NAME "min_filter"
    #define RENDER_SCRIPT_MAG_FILTER_NAME "mag_filter"
    #define RENDER_SCRIPT_U_WRAP_NAME "u_wrap"
    #define RENDER_SCRIPT_V_WRAP_NAME "v_wrap"

    static uint32_t RENDER_SCRIPT_TYPE_HASH = 0;
    static uint32_t RENDER_SCRIPT_INSTANCE_TYPE_HASH = 0;
    static uint32_t RENDER_SCRIPT_CONSTANTBUFFER_TYPE_HASH = 0;
    static uint32_t RENDER_SCRIPT_PREDICATE_TYPE_HASH = 0;
    static uint32_t RENDER_SCRIPT_CONSTANTBUFFER_ARRAY_TYPE_HASH = 0;

    const char* RENDER_SCRIPT_FUNCTION_NAMES[MAX_RENDER_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "update",
        "on_message",
        "on_reload"
    };

    static HNamedConstantBuffer* RenderScriptConstantBuffer_Check(lua_State *L, int index)
    {
        return (HNamedConstantBuffer*)dmScript::CheckUserType(L, index, RENDER_SCRIPT_CONSTANTBUFFER_TYPE_HASH, "Expected a constant buffer (acquired from a render.* function)");
    }

    struct ConstantBufferTableEntry
    {
        HNamedConstantBuffer m_ConstantBuffer;
        dmhash_t             m_ConstantName;
        int32_t              m_LuaRef;
    };

    struct NamedConstantBufferTable
    {
        HNamedConstantBuffer                    m_ConstantBuffer;
        dmHashTable64<ConstantBufferTableEntry> m_ConstantArrayEntries;
    };

    static int RenderScriptConstantBuffer_gc (lua_State *L)
    {
        NamedConstantBufferTable* cb = (NamedConstantBufferTable*) lua_touserdata(L, 1);
        DeleteNamedConstantBuffer(cb->m_ConstantBuffer);
        cb->m_ConstantArrayEntries.~dmHashTable64<dmRender::ConstantBufferTableEntry>();
        cb->m_ConstantBuffer = 0;
        return 0;
    }

    static int RenderScriptConstantBuffer_tostring (lua_State *L)
    {
        lua_pushfstring(L, "ConstantBuffer: %p", lua_touserdata(L, 1));
        return 1;
    }

    static bool RenderScriptPushValueFromConstantType(lua_State *L, dmVMath::Vector4* value_ptr, uint32_t value_count, uint32_t table_index, dmRenderDDF::MaterialDesc::ConstantType type)
    {
        uint32_t value_index = table_index;

        if (type == dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4)
        {
            value_index *= 4;
        }

        if (value_index >= value_count)
        {
            return false;
        }

        switch(type)
        {
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER:
                dmScript::PushVector4(L, value_ptr[value_index]);
                break;
            case dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4:
                dmScript::PushMatrix4(L, ((dmVMath::Matrix4*) &value_ptr[value_index])[0]);
                break;
            default: return false;

        }

        return true;
    }

    static int RenderScriptConstantBuffer_index(lua_State *L)
    {
        NamedConstantBufferTable* cb_table = (NamedConstantBufferTable*)lua_touserdata(L, 1);
        HNamedConstantBuffer cb            = cb_table->m_ConstantBuffer;
        assert(cb);

        const char* name         = luaL_checkstring(L, 2);
        dmhash_t name_hash       = dmHashString64(name);
        dmVMath::Vector4* values = 0;
        uint32_t num_values      = 0;

        dmRenderDDF::MaterialDesc::ConstantType constant_type;
        ConstantBufferTableEntry* table_entry = cb_table->m_ConstantArrayEntries.Get(name_hash);
        if (table_entry != 0)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, table_entry->m_LuaRef);
            return 1;
        }
        else if (GetNamedConstant(cb, name_hash, &values, &num_values, &constant_type))
        {
            return RenderScriptPushValueFromConstantType(L, values, num_values, 0, constant_type);
        }
        else
        {
            return luaL_error(L, "Constant %s not set.", dmHashReverseSafe64(name_hash));
        }
        return 0;
    }

    static dmRender::Result RenderScriptSetNamedValueFromLua(lua_State *L, int stack_index, HNamedConstantBuffer buffer, dmhash_t name_hash, int table_index)
    {
        uint32_t value_count                                  = 1;
        uint32_t value_index                                  = table_index;
        dmVMath::Vector4* value_ptr                           = 0;
        dmRenderDDF::MaterialDesc::ConstantType constant_type = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER;

        if (dmScript::IsMatrix4(L, stack_index))
        {
            value_ptr     = (dmVMath::Vector4*) dmScript::CheckMatrix4(L, stack_index);
            value_count  *= 4;
            value_index  *= 4;
            constant_type = dmRenderDDF::MaterialDesc::CONSTANT_TYPE_USER_MATRIX4;
        }
        else
        {
            value_ptr = dmScript::CheckVector4(L, stack_index);
        }
        return SetNamedConstantAtIndex(buffer, name_hash, value_ptr, value_count, value_index, constant_type);
    }

    static int RenderScriptConstantBuffer_newindex(lua_State *L)
    {
        int top = lua_gettop(L);
        NamedConstantBufferTable* cb_table = (NamedConstantBufferTable*) lua_touserdata(L, 1);
        HNamedConstantBuffer cb = cb_table->m_ConstantBuffer;
        assert(cb);

        const char* name = luaL_checkstring(L, 2);
        dmhash_t name_hash = dmHashString64(name);

        if (lua_istable(L, 3))
        {
            ConstantBufferTableEntry* p_table_entry = (ConstantBufferTableEntry*) lua_newuserdata(L, sizeof(ConstantBufferTableEntry));
            luaL_getmetatable(L, RENDER_SCRIPT_CONSTANTBUFFER_ARRAY);
            lua_setmetatable(L, -2);

            lua_pushvalue(L, -1);
            int p_ref = luaL_ref(L, LUA_REGISTRYINDEX);
            lua_pop(L, 1);

            p_table_entry->m_ConstantBuffer = cb;
            p_table_entry->m_ConstantName   = name_hash;
            p_table_entry->m_LuaRef         = p_ref;

            if (cb_table->m_ConstantArrayEntries.Full())
            {
                cb_table->m_ConstantArrayEntries.SetCapacity(4, cb_table->m_ConstantArrayEntries.Size() + 1);
            }

            cb_table->m_ConstantArrayEntries.Put(name_hash, *p_table_entry);

            // If the table contains elements, we add them directly
            lua_pushvalue(L, 3);
            lua_pushnil(L);
            while (lua_next(L, -2) != 0)
            {
                if (!lua_isnumber(L, -2))
                {
                    return luaL_error(L, "Constant %s not set. Indices must be numbers", dmHashReverseSafe64(name_hash));
                }

                int32_t table_index_lua = lua_tonumber(L, -2);

                if (table_index_lua < 1)
                {
                    return luaL_error(L, "Constant %s[%d] not set. Indices must start from 1", dmHashReverseSafe64(name_hash), table_index_lua);
                }

                if (RenderScriptSetNamedValueFromLua(L, -1, cb, name_hash, table_index_lua - 1) != RESULT_OK)
                {
                    return luaL_error(L, "Constant %s[%d] not set. Mixing types in array not allowed", dmHashReverseSafe64(name_hash), table_index_lua);                    
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        else
        {
            RenderScriptSetNamedValueFromLua(L, 3, cb, name_hash, 0);
        }
        assert(top == lua_gettop(L));
        return 0;
    }

    static int RenderScriptConstantBufferArray_index(lua_State *L)
    {
        ConstantBufferTableEntry* table_entry = (ConstantBufferTableEntry*) lua_touserdata(L, 1);
        HNamedConstantBuffer  cb              = table_entry->m_ConstantBuffer;
        dmhash_t name_hash                    = table_entry->m_ConstantName;
        uint32_t table_index_lua              = (uint32_t) luaL_checknumber(L, 2);
        uint32_t table_index                  = table_index_lua - 1;
        dmVMath::Vector4* values              = 0;
        uint32_t num_values                   = 0;
        dmRenderDDF::MaterialDesc::ConstantType constant_type;

        if (GetNamedConstant(cb, name_hash, &values, &num_values, &constant_type))
        {
            if (!RenderScriptPushValueFromConstantType(L, values, num_values, table_index, constant_type))
            {
                return luaL_error(L, "Constant %s[%d] not set.", dmHashReverseSafe64(name_hash), table_index_lua);
            }
            return 1;
        }
        return luaL_error(L, "Constant %s not set.", dmHashReverseSafe64(name_hash));
    }

    static int RenderScriptConstantBufferArray_newindex(lua_State *L)
    {
        int top                               = lua_gettop(L);
        ConstantBufferTableEntry* table_entry = (ConstantBufferTableEntry*) lua_touserdata(L, 1);
        HNamedConstantBuffer cb               = table_entry->m_ConstantBuffer;
        dmhash_t name_hash                    = table_entry->m_ConstantName;

        if (!lua_isnumber(L, 2))
        {
            return luaL_error(L, "Constant %s not set. Indices must be numbers", dmHashReverseSafe64(name_hash));
        }

        int32_t table_index_lua = lua_tonumber(L, 2);

        if (table_index_lua < 1)
        {
            return luaL_error(L, "Constant %s[%d] not set. Indices must start from 1", dmHashReverseSafe64(name_hash), table_index_lua);
        }

        if (RenderScriptSetNamedValueFromLua(L, 3, cb, name_hash, table_index_lua - 1) != RESULT_OK)
        {
            return luaL_error(L, "Constant %s[%d] not set. Mixing types in array not allowed", dmHashReverseSafe64(name_hash), table_index_lua);                    
        }
        assert(top == lua_gettop(L));

        return 0;
    }

    static const luaL_reg RenderScriptConstantBuffer_methods[] =
    {
        {0,0}
    };

    static const luaL_reg RenderScriptConstantBuffer_meta[] =
    {
        {"__gc",        RenderScriptConstantBuffer_gc},
        {"__tostring",  RenderScriptConstantBuffer_tostring},
        {"__index",     RenderScriptConstantBuffer_index},
        {"__newindex",  RenderScriptConstantBuffer_newindex},
        {0, 0}
    };

    static const luaL_reg RenderScriptConstantBufferArray_meta[] =
    {
        {"__index",     RenderScriptConstantBufferArray_index},
        {"__newindex",  RenderScriptConstantBufferArray_newindex},
        {0, 0}
    };

    static HPredicate* RenderScriptPredicate_Check(lua_State *L, int index)
    {
        return (HPredicate*)dmScript::CheckUserType(L, index, RENDER_SCRIPT_PREDICATE_TYPE_HASH, "Expected a render predicate (acquired from the render.predicate function)");
    }

    static int RenderScriptPredicate_gc (lua_State *L)
    {
        HPredicate* p = (HPredicate*)lua_touserdata(L, 1);
        DeletePredicate(*p);
        *p = 0;
        return 0;
    }

    static int RenderScriptPredicate_tostring (lua_State *L)
    {
        lua_pushfstring(L, "Predicate: %p", lua_touserdata(L, 1));
        return 1;
    }

    static const luaL_reg RenderScriptPredicate_methods[] =
    {
        {0,0}
    };

    static const luaL_reg RenderScriptPredicate_meta[] =
    {
        {"__gc",        RenderScriptPredicate_gc},
        {"__tostring",  RenderScriptPredicate_tostring},
        {0, 0}
    };

    /*# create a new constant buffer.
     *
     * Constant buffers are used to set shader program variables and are optionally passed to the `render.draw()` function.
     * The buffer's constant elements can be indexed like an ordinary Lua table, but you can't iterate over them with pairs() or ipairs().
     *
     * @name render.constant_buffer
     * @return buffer [type:constant_buffer] new constant buffer
     * @examples
     *
     * Set a "tint" constant in a constant buffer in the render script:
     *
     * ```lua
     * local constants = render.constant_buffer()
     * constants.tint = vmath.vector4(1, 1, 1, 1)
     * ```
     *
     * Then use the constant buffer when drawing a predicate:
     *
     * ```lua
     * render.draw(self.my_pred, constants)
     * ```
     *
     * The constant buffer also supports array values by specifying constants in a table:
     *
     * ```lua
     * local constants = render.constant_buffer()
     * constants.light_colors    = {}
     * constants.light_colors[1] = vmath.vector4(1, 0, 0, 1)
     * constants.light_colors[2] = vmath.vector4(0, 1, 0, 1)
     * constants.light_colors[3] = vmath.vector4(0, 0, 1, 1)
     * ```
     *
     * You can also create the table by passing the vectors directly when creating the table:
     *
     * ```lua
     * local constants = render.constant_buffer()
     * constants.light_colors    = {
     *      vmath.vector4(1, 0, 0, 1)
     *      vmath.vector4(0, 1, 0, 1)
     *      vmath.vector4(0, 0, 1, 1)
     * }
     * 
     * -- Add more constant to the array
     * constants.light_colors[4] = vmath.vector4(1, 1, 1, 1)
     * ```
     */
    int RenderScript_ConstantBuffer(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        HNamedConstantBuffer cb            = NewNamedConstantBuffer();
        NamedConstantBufferTable* p_buffer = (NamedConstantBufferTable*) lua_newuserdata(L, sizeof(NamedConstantBufferTable));
        memset(p_buffer, 0, sizeof(NamedConstantBufferTable));

        p_buffer->m_ConstantBuffer         = cb;
        p_buffer->m_ConstantArrayEntries.Clear();

        luaL_getmetatable(L, RENDER_SCRIPT_CONSTANTBUFFER);
        lua_setmetatable(L, -2);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int RenderScriptGetURL(lua_State* L)
    {
        RenderScript* script = (RenderScript*)lua_touserdata(L, 1);
        dmMessage::URL url;
        dmMessage::ResetURL(&url);
        url.m_Socket = script->m_RenderContext->m_Socket;
        dmScript::PushURL(L, url);
        return 1;
    }

    static int RenderScriptResolvePath(lua_State* L)
    {
        dmScript::PushHash(L, dmHashString64(luaL_checkstring(L, 2)));
        return 1;
    }

    static int RenderScriptIsValid(lua_State* L)
    {
        RenderScript* script = (RenderScript*)lua_touserdata(L, 1);
        lua_pushboolean(L, script != 0x0);
        return 1;
    }

    static const luaL_reg RenderScript_methods[] =
    {
        {0,0}
    };

    static const luaL_reg RenderScript_meta[] =
    {
        {dmScript::META_TABLE_GET_URL, RenderScriptGetURL},
        {dmScript::META_TABLE_RESOLVE_PATH, RenderScriptResolvePath},
        {dmScript::META_TABLE_IS_VALID, RenderScriptIsValid},
        {0, 0}
    };

    static RenderScriptInstance* RenderScriptInstance_Check(lua_State *L, int index)
    {
        return (RenderScriptInstance*)dmScript::CheckUserType(L, index, RENDER_SCRIPT_INSTANCE_TYPE_HASH, "You can only access render.* functions and values from a render script instance (.render_script file)");
    }

    static RenderScriptInstance* RenderScriptInstance_Check(lua_State *L)
    {
        int top = lua_gettop(L);
        (void) top;

        dmScript::GetInstance(L);
        RenderScriptInstance* i = RenderScriptInstance_Check(L, -1);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));
        return i;
    }

    static int RenderScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "RenderScript: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int RenderScriptInstance_index(lua_State *L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        assert(i);

        // Try to find value in instance data
        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_RenderScriptDataReference);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        lua_remove(L, 3);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static int RenderScriptInstance_newindex(lua_State *L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        assert(i);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_RenderScriptDataReference);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return 0;
    }

    static int RenderScriptInstanceGetURL(lua_State* L)
    {
        RenderScriptInstance* i = (RenderScriptInstance*)lua_touserdata(L, 1);
        dmMessage::URL url;
        dmMessage::ResetURL(&url);
        url.m_Socket = i->m_RenderContext->m_Socket;
        dmScript::PushURL(L, url);
        return 1;
    }

    static int RenderScriptInstanceResolvePath(lua_State* L)
    {
        dmScript::PushHash(L, dmHashString64(luaL_checkstring(L, 2)));
        return 1;
    }

    static int RenderScriptInstanceIsValid(lua_State* L)
    {
        RenderScriptInstance* i = (RenderScriptInstance*)lua_touserdata(L, 1);
        lua_pushboolean(L, i != 0x0 && i->m_RenderContext != 0x0);
        return 1;
    }

    static int RenderScriptGetInstanceContextTableRef(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        const int self_index = 1;

        RenderScriptInstance* i = (RenderScriptInstance*)lua_touserdata(L, self_index);
        lua_pushnumber(L, i ? i->m_ContextTableReference : LUA_NOREF);

        return 1;
    }

    static const luaL_reg RenderScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg RenderScriptInstance_meta[] =
    {
        {"__tostring",                                  RenderScriptInstance_tostring},
        {"__index",                                     RenderScriptInstance_index},
        {"__newindex",                                  RenderScriptInstance_newindex},
        {dmScript::META_TABLE_GET_URL,                  RenderScriptInstanceGetURL},
        {dmScript::META_TABLE_RESOLVE_PATH,             RenderScriptInstanceResolvePath},
        {dmScript::META_TABLE_IS_VALID,                 RenderScriptInstanceIsValid},
        {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, RenderScriptGetInstanceContextTableRef},
        {0, 0}
    };

    bool InsertCommand(RenderScriptInstance* i, const Command& command)
    {
        if (i->m_CommandBuffer.Full())
            return false;
        else
            i->m_CommandBuffer.Push(command);
        return true;
    }

    /*#
     * @name render.STATE_DEPTH_TEST
     * @variable
     */

    /*#
     * @name render.STATE_STENCIL_TEST
     * @variable
     */

    /*#
     * @name render.STATE_BLEND
     * @variable
     */

    /*#
     * @name render.STATE_CULL_FACE
     * @variable
     */

    /*#
     * @name render.STATE_POLYGON_OFFSET_FILL
     * @variable
     */

    /*# enables a render state
     *
     * Enables a particular render state. The state will be enabled until disabled.
     *
     * @name render.enable_state
     * @param state [type:constant] state to enable
     *
     * - `render.STATE_DEPTH_TEST`
     * - `render.STATE_STENCIL_TEST`
     * - `render.STATE_BLEND`
     * - `render.STATE_ALPHA_TEST` ([icon:iOS][icon:android] not available on iOS and Android)
     * - `render.STATE_CULL_FACE`
     * - `render.STATE_POLYGON_OFFSET_FILL`
     *
     * @examples
     *
     * Enable stencil test when drawing the gui predicate, then disable it:
     *
     * ```lua
     * render.enable_state(render.STATE_STENCIL_TEST)
     * render.draw(self.gui_pred)
     * render.disable_state(render.STATE_STENCIL_TEST)
     * ```
     */
    int RenderScript_EnableState(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t state = luaL_checknumber(L, 1);

        if (state != dmGraphics::STATE_DEPTH_TEST &&
            state != dmGraphics::STATE_STENCIL_TEST &&
            state != dmGraphics::STATE_ALPHA_TEST &&
            state != dmGraphics::STATE_BLEND &&
            state != dmGraphics::STATE_CULL_FACE &&
            state != dmGraphics::STATE_POLYGON_OFFSET_FILL)
        {
            return luaL_error(L, "Invalid state: %s.enable_state(%d).", RENDER_SCRIPT_LIB_NAME, state);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_STATE, state))) {
            assert(top == lua_gettop(L));
            return 0;
        } else {
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
    }

    /*# disables a render state
     *
     * Disables a render state.
     *
     * @name render.disable_state
     * @param state [type:constant] state to disable
     *
     * - `render.STATE_DEPTH_TEST`
     * - `render.STATE_STENCIL_TEST`
     * - `render.STATE_BLEND`
     * - `render.STATE_ALPHA_TEST` ([icon:iOS][icon:android] not available on iOS and Android)
     * - `render.STATE_CULL_FACE`
     * - `render.STATE_POLYGON_OFFSET_FILL`
     *
     * @examples
     * Disable face culling when drawing the tile predicate:
     *
     * ```lua
     * render.disable_state(render.STATE_CULL_FACE)
     * render.draw(self.tile_pred)
     * ```
     */
    int RenderScript_DisableState(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t state = luaL_checknumber(L, 1);
        if (state != dmGraphics::STATE_DEPTH_TEST &&
            state != dmGraphics::STATE_STENCIL_TEST &&
            state != dmGraphics::STATE_ALPHA_TEST &&
            state != dmGraphics::STATE_BLEND &&
            state != dmGraphics::STATE_CULL_FACE &&
            state != dmGraphics::STATE_POLYGON_OFFSET_FILL)
        {
            return luaL_error(L, "Invalid state: %s.disable_state(%d).", RENDER_SCRIPT_LIB_NAME, state);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_STATE, state))) {
            assert(top == lua_gettop(L));
            return 0;
        } else {
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
    }

    /*# sets the render viewport
     *
     * Set the render viewport to the specified rectangle.
     *
     * @name render.set_viewport
     * @param x [type:number] left corner
     * @param y [type:number] bottom corner
     * @param width [type:number] viewport width
     * @param height [type:number] viewport height
     * @examples
     *
     * ```lua
     * -- Set the viewport to the window dimensions.
     * render.set_viewport(0, 0, render.get_window_width(), render.get_window_height())
     * ```
     */
    int RenderScript_SetViewport(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        int32_t x = luaL_checknumber(L, 1);
        int32_t y = luaL_checknumber(L, 2);
        int32_t width = luaL_checknumber(L, 3);
        int32_t height = luaL_checknumber(L, 4);
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_VIEWPORT, x, y, width, height)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*#
     * @name render.FORMAT_LUMINANCE
     * @variable
     */

    /*#
     * @name render.FORMAT_RGB
     * @variable
     */

    /*#
     * @name render.FORMAT_RGBA
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_RGB16F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_RGB32F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_RGBA16F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_RGBA32F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_R16F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_RG16F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_R32F
     * @variable
     */

    /*#
     * May be nil if the format isn't supported
     * @name render.FORMAT_RG32F
     * @variable
     */

    /*#
     * @name render.FORMAT_DEPTH
     * @variable
     */

    /*#
     * @name render.FORMAT_STENCIL
     * @variable
     */

    /*#
     * @name render.FILTER_LINEAR
     * @variable
     */

    /*#
     * @name render.FILTER_NEAREST
     * @variable
     */

    /*#
     * @name render.WRAP_CLAMP_TO_BORDER
     * @variable
     */

    /*#
     * @name render.WRAP_CLAMP_TO_EDGE
     * @variable
     */

    /*#
     * @name render.WRAP_MIRRORED_REPEAT
     * @variable
     */

    /*#
     * @name render.WRAP_REPEAT
     * @variable
     */

    /*# creates a new render target
     * Creates a new render target according to the supplied
     * specification table.
     *
     * The table should contain keys specifying which buffers should be created
     * with what parameters. Each buffer key should have a table value consisting
     * of parameters. The following parameter keys are available:
     *
     * Key          | Values
     * ------------ | ----------------------------
     * `format`     |  `render.FORMAT_LUMINANCE`<br/>`render.FORMAT_RGB`<br/>`render.FORMAT_RGBA`<br/>`render.FORMAT_DEPTH`<br/>`render.FORMAT_STENCIL`<br/>`render.FORMAT_RGBA32F`<br/>`render.FORMAT_RGBA16F`<br/>
     * `width`      | number
     * `height`     | number
     * `min_filter` | `render.FILTER_LINEAR`<br/>`render.FILTER_NEAREST`
     * `mag_filter` | `render.FILTER_LINEAR`<br/>`render.FILTER_NEAREST`
     * `u_wrap`     | `render.WRAP_CLAMP_TO_BORDER`<br/>`render.WRAP_CLAMP_TO_EDGE`<br/>`render.WRAP_MIRRORED_REPEAT`<br/>`render.WRAP_REPEAT`<br/>
     * `v_wrap`     | `render.WRAP_CLAMP_TO_BORDER`<br/>`render.WRAP_CLAMP_TO_EDGE`<br/>`render.WRAP_MIRRORED_REPEAT`<br/>`render.WRAP_REPEAT`
     *
     * The render target can be created to support multiple color attachments. Each attachment can have different format settings and texture filters,
     * but attachments must be added in sequence, meaning you cannot create a render target at slot 0 and 3.
     * Instead it has to be created with all four buffer types ranging from [0..3] (as denoted by render.BUFFER_COLORX_BIT where 'X' is the attachment you want to create).
     *
     * @name render.render_target
     * @param name [type:string] render target name
     * @param parameters [type:table] table of buffer parameters, see the description for available keys and values
     * @return render_target [type:render_target] new render target
     * @examples
     *
     * How to create a new render target and draw to it:
     *
     * ```lua
     * function init(self)
     *     -- render target buffer parameters
     *     local color_params = { format = render.FORMAT_RGBA,
     *                            width = render.get_window_width(),
     *                            height = render.get_window_height(),
     *                            min_filter = render.FILTER_LINEAR,
     *                            mag_filter = render.FILTER_LINEAR,
     *                            u_wrap = render.WRAP_CLAMP_TO_EDGE,
     *                            v_wrap = render.WRAP_CLAMP_TO_EDGE }
     *     local depth_params = { format = render.FORMAT_DEPTH,
     *                            width = render.get_window_width(),
     *                            height = render.get_window_height(),
     *                            u_wrap = render.WRAP_CLAMP_TO_EDGE,
     *                            v_wrap = render.WRAP_CLAMP_TO_EDGE }
     *     self.my_render_target = render.render_target({[render.BUFFER_COLOR_BIT] = color_params, [render.BUFFER_DEPTH_BIT] = depth_params })
     * end
     *
     * function update(self, dt)
     *     -- enable target so all drawing is done to it
     *     render.set_render_target(self.my_render_target)
     *
     *     -- draw a predicate to the render target
     *     render.draw(self.my_pred)
     * end
     * ```
     *
     * How to create a render target with multiple outputs:
     *
     * ```lua
     * function init(self)
     *     -- render target buffer parameters
     *     local color_params_rgba = { format = render.FORMAT_RGBA,
     *                                 width = render.get_window_width(),
     *                                 height = render.get_window_height(),
     *                                 min_filter = render.FILTER_LINEAR,
     *                                 mag_filter = render.FILTER_LINEAR,
     *                                 u_wrap = render.WRAP_CLAMP_TO_EDGE,
     *                                 v_wrap = render.WRAP_CLAMP_TO_EDGE }
     *     local color_params_float = { format = render.FORMAT_RG32F,
     *                            width = render.get_window_width(),
     *                            height = render.get_window_height(),
     *                            min_filter = render.FILTER_LINEAR,
     *                            mag_filter = render.FILTER_LINEAR,
     *                            u_wrap = render.WRAP_CLAMP_TO_EDGE,
     *                            v_wrap = render.WRAP_CLAMP_TO_EDGE }
     *
     *
     *     -- Create a render target with three color attachments
     *     -- Note: No depth buffer is attached here
     *     self.my_render_target = render.render_target({
     *            [render.BUFFER_COLOR0_BIT] = color_params_rgba,
     *            [render.BUFFER_COLOR1_BIT] = color_params_rgba,
     *            [render.BUFFER_COLOR2_BIT] = color_params_float, })
     * end
     *
     * function update(self, dt)
     *     -- enable target so all drawing is done to it
     *     render.enable_render_target(self.my_render_target)
     *
     *     -- draw a predicate to the render target
     *     render.draw(self.my_pred)
     * end
     * ```
     *
     */
    int RenderScript_RenderTarget(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);

        // Legacy support
        int table_index = 2;
        if (lua_istable(L, 1))
        {
            table_index = 1;
        }

        const char* required_keys[] = { "format", "width", "height" };
        uint32_t buffer_type_flags = 0;
        uint32_t max_tex_size = dmGraphics::GetMaxTextureSize(i->m_RenderContext->m_GraphicsContext);
        luaL_checktype(L, table_index, LUA_TTABLE);
        dmGraphics::TextureCreationParams creation_params[dmGraphics::MAX_BUFFER_TYPE_COUNT];
        dmGraphics::TextureParams params[dmGraphics::MAX_BUFFER_TYPE_COUNT];
        lua_pushnil(L);
        while (lua_next(L, table_index))
        {
            bool required_found[]  = { false, false, false };
            uint32_t buffer_type   = (uint32_t)luaL_checknumber(L, -2);
            buffer_type_flags     |= buffer_type;

            uint32_t index                        = dmGraphics::GetBufferTypeIndex((dmGraphics::BufferType)buffer_type);
            dmGraphics::TextureParams* p          = &params[index];
            dmGraphics::TextureCreationParams* cp = &creation_params[index];
            luaL_checktype(L, -1, LUA_TTABLE);
            lua_pushnil(L);

            // Verify that required keys are supplied
            while (lua_next(L, -2))
            {
                const char* key = luaL_checkstring(L, -2);
                for (uint32_t i = 0; i < sizeof(required_found) / sizeof(required_found[0]); ++i)
                {
                    if (strncmp(key, required_keys[i], strlen(required_keys[i])) == 0)
                    {
                        required_found[i] = true;
                    }
                }
                lua_pop(L, 1);
            }
            for (uint32_t i = 0; i < sizeof(required_found) / sizeof(required_found[0]); ++i)
            {
                if (!required_found[i])
                {
                    return luaL_error(L, "Required parameter key not found: '%s'", required_keys[i]);
                }
            }
            lua_pushnil(L);

            while (lua_next(L, -2))
            {
                const char* key = luaL_checkstring(L, -2);
                if (lua_isnil(L, -1) != 0)
                {
                    return luaL_error(L, "nil value supplied to %s.render_target: %s.", RENDER_SCRIPT_LIB_NAME, key);
                }

                if (strncmp(key, RENDER_SCRIPT_FORMAT_NAME, strlen(RENDER_SCRIPT_FORMAT_NAME)) == 0)
                {
                    p->m_Format = (dmGraphics::TextureFormat)(int)luaL_checknumber(L, -1);
                    if(buffer_type == dmGraphics::BUFFER_TYPE_DEPTH_BIT)
                    {
                        if(p->m_Format != dmGraphics::TEXTURE_FORMAT_DEPTH)
                        {
                            return luaL_error(L, "The only valid format for depth buffers is FORMAT_DEPTH.");
                        }
                    }
                    if(buffer_type == dmGraphics::BUFFER_TYPE_STENCIL_BIT)
                    {
                        if(p->m_Format != dmGraphics::TEXTURE_FORMAT_STENCIL)
                        {
                            return luaL_error(L, "The only valid format for stencil buffers is FORMAT_STENCIL.");
                        }
                    }
                }
                else if (strncmp(key, RENDER_SCRIPT_WIDTH_NAME, strlen(RENDER_SCRIPT_WIDTH_NAME)) == 0)
                {
                    p->m_Width = luaL_checknumber(L, -1);
                    cp->m_Width = p->m_Width;
                }
                else if (strncmp(key, RENDER_SCRIPT_HEIGHT_NAME, strlen(RENDER_SCRIPT_HEIGHT_NAME)) == 0)
                {
                    p->m_Height = luaL_checknumber(L, -1);
                    cp->m_Height = p->m_Height;
                }
                else if (strncmp(key, RENDER_SCRIPT_MIN_FILTER_NAME, strlen(RENDER_SCRIPT_MIN_FILTER_NAME)) == 0)
                {
                    p->m_MinFilter = (dmGraphics::TextureFilter)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_MAG_FILTER_NAME, strlen(RENDER_SCRIPT_MAG_FILTER_NAME)) == 0)
                {
                    p->m_MagFilter = (dmGraphics::TextureFilter)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_U_WRAP_NAME, strlen(RENDER_SCRIPT_U_WRAP_NAME)) == 0)
                {
                    p->m_UWrap = (dmGraphics::TextureWrap)(int)luaL_checknumber(L, -1);
                }
                else if (strncmp(key, RENDER_SCRIPT_V_WRAP_NAME, strlen(RENDER_SCRIPT_V_WRAP_NAME)) == 0)
                {
                    p->m_VWrap = (dmGraphics::TextureWrap)(int)luaL_checknumber(L, -1);
                }
                else
                {
                    lua_pop(L, 2);
                    assert(top == lua_gettop(L));
                    return luaL_error(L, "Unknown key supplied to %s.rendertarget: %s. Available keys are: %s, %s, %s, %s, %s, %s, %s.",
                        RENDER_SCRIPT_LIB_NAME, key,
                        RENDER_SCRIPT_FORMAT_NAME,
                        RENDER_SCRIPT_WIDTH_NAME,
                        RENDER_SCRIPT_HEIGHT_NAME,
                        RENDER_SCRIPT_MIN_FILTER_NAME,
                        RENDER_SCRIPT_MAG_FILTER_NAME,
                        RENDER_SCRIPT_U_WRAP_NAME,
                        RENDER_SCRIPT_V_WRAP_NAME);
                }
                lua_pop(L, 1);
            }
            lua_pop(L, 1);

            if (creation_params[index].m_Width > max_tex_size || creation_params[index].m_Height > max_tex_size)
            {
                lua_pop(L, 1);
                assert(top == lua_gettop(L));
                return luaL_error(L, "Render target (type %s) of width %d and height %d is greater than max supported texture size %d for this platform.",
                    dmGraphics::GetBufferTypeLiteral((dmGraphics::BufferType)buffer_type), creation_params[index].m_Width, creation_params[index].m_Height, max_tex_size);
            }
        }

        dmGraphics::HRenderTarget render_target = dmGraphics::NewRenderTarget(i->m_RenderContext->m_GraphicsContext, buffer_type_flags, creation_params, params);

        lua_pushlightuserdata(L, (void*)render_target);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# deletes a render target
     *
     * Deletes a previously created render target.
     *
     * @name render.delete_render_target
     * @param render_target [type:render_target] render target to delete
     * @examples
     *
     * How to delete a render target:
     *
     * ```lua
     *  render.delete_render_target(self.my_render_target)
     * ```
     */
    int RenderScript_DeleteRenderTarget(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        dmGraphics::HRenderTarget render_target = 0x0;

        if (lua_islightuserdata(L, 1))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 1);
        }
        if (render_target == 0x0)
            return luaL_error(L, "Invalid render target (nil) supplied to %s.enable_render_target.", RENDER_SCRIPT_LIB_NAME);

        dmGraphics::DeleteRenderTarget(render_target);
        return 0;
    }

    /*#
     * @name render.RENDER_TARGET_DEFAULT
     * @variable
     */

    /*# sets a render target
     *
     * Sets a render target. Subsequent draw operations will be to the
     * render target until it is replaced by a subsequent call to set_render_target.
     *
     * @name render.set_render_target
     * @param render_target [type:render_target] render target to set. render.RENDER_TARGET_DEFAULT to set the default render target
     * @param [options] [type:table] optional table with behaviour parameters
     *
     * `transient`
     * : [type:table] Transient frame buffer types are only valid while the render target is active, i.e becomes undefined when a new target is set by a subsequent call to set_render_target.
     *  Default is all non-transient. Be aware that some hardware uses a combined depth stencil buffer and when this is the case both are considered non-transient if exclusively selected!
     *  A buffer type defined that doesn't exist in the render target is silently ignored.
     *
     * - `render.BUFFER_COLOR_BIT`
     * - `render.BUFFER_DEPTH_BIT`
     * - `render.BUFFER_STENCIL_BIT`
     *
     * @examples
     *
     * How to set a render target and draw to it and then switch back to the default render target
     * The render target defines the depth/stencil buffers as transient, when set_render_target is called the next time the buffers may be invalidated and allow for optimisations depending on driver support
     *
     * ```lua
     * function update(self, dt)
     *     -- set render target so all drawing is done to it
     *     render.set_render_target(self.my_render_target, { transient = { render.BUFFER_DEPTH_BIT, render.BUFFER_STENCIL_BIT } } )
     *
     *     -- draw a predicate to the render target
     *     render.draw(self.my_pred)
     *
     *     -- set default render target. This also invalidates the depth and stencil buffers of the current target (self.my_render_target)
     *     --  which can be an optimisation on some hardware
     *     render.set_render_target(render.RENDER_TARGET_DEFAULT)
     *
     * end
     * ```
    */
    int RenderScript_SetRenderTarget(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        dmGraphics::HRenderTarget render_target = 0x0;
        DM_LUA_STACK_CHECK(L, 0);

        if (lua_gettop(L) > 0)
        {
            if(lua_islightuserdata(L, 1))
            {
                render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 1);
            }
            else
            {
                if(!lua_isnil(L, 1) && luaL_checkint(L, 1) != 0)
                {
                    return luaL_error(L, "Invalid render target supplied to %s.set_render_target.", RENDER_SCRIPT_LIB_NAME);
                }
            }
        }

        uint32_t transient_buffer_types = 0;
        if (lua_gettop(L) > 1)
        {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_pushvalue(L, 2);
            lua_getfield(L, -1, "transient");
            if(!lua_isnil(L, -1))
            {
                lua_pushnil(L);
                while (lua_next(L, -2))
                {
                    transient_buffer_types |= luaL_checkint(L, -1);
                    lua_pop(L, 1);
                }
            }
            lua_pop(L, 2);
        }

        if (InsertCommand(i, Command(COMMAND_TYPE_SET_RENDER_TARGET, (uintptr_t)render_target, transient_buffer_types)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /* DEPRECATED. NO API DOC GENERATED.
     * enables a render target
     *
     * Enables a render target. Subsequent draw operations will be to the enabled render target until
     * a subsequent call to render.enable_render_target, render.disable_render_target or render.set_render_target.
     *
     * @name render.enable_render_target
     * @param render_target [type:render_target] render target to enable
     *
     * @deprecated Use render.set_render_target() instead
     *
     * @examples
     *
     * How to enable a render target and draw to it:
     *
     * ```lua
     * function update(self, dt)
     *     -- enable target so all drawing is done to it
     *     render.enable_render_target(self.my_render_target)
     *
     *     -- draw a predicate to the render target
     *     render.draw(self.my_pred)
     * end
     * ```
    */
    int RenderScript_EnableRenderTarget(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        dmGraphics::HRenderTarget render_target = 0x0;
        DM_LUA_STACK_CHECK(L, 0);

        if (lua_islightuserdata(L, 1))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 1);
        }
        if (render_target == 0x0)
            return luaL_error(L, "Invalid render target (nil) supplied to %s.enable_render_target.", RENDER_SCRIPT_LIB_NAME);

        if (InsertCommand(i, Command(COMMAND_TYPE_SET_RENDER_TARGET, (uintptr_t)render_target, 0)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /* DEPRECATED. NO API DOC GENERATED.
     * disables a render target
     *
     * Disables a previously enabled render target. Subsequent draw operations
     * will be drawn to the default frame buffer unless another render target is
     * enabled.
     *
     * @name render.disable_render_target
     * @param render_target [type:render_target] render target to disable
     *
     * @deprecated Use render.set_render_target() instead
     *
     * @examples
     *
     * How to disable a render target so we can draw to the screen:
     *
     * ```lua
     * function update(self, dt)
     *     -- enable target so all drawing is done to it
     *     render.enable_render_target(self.my_render_target)
     *
     *     -- draw a predicate to the render target
     *     render.draw(self.my_pred)
     *
     *     -- disable target
     *     render.disable_render_target(self.my_render_target)
     *
     *     -- draw a predicate to the screen
     *     render.draw(self.my_other_pred)
     * end
     * ```
     */
    int RenderScript_DisableRenderTarget(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        DM_LUA_STACK_CHECK(L, 0);

        if (InsertCommand(i, Command(COMMAND_TYPE_SET_RENDER_TARGET, (uintptr_t)0x0, 0)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# sets the render target size
     *
     * @name render.set_render_target_size
     * @param render_target [type:render_target] render target to set size for
     * @param width [type:number] new render target width
     * @param height [type:number] new render target height
     * @examples
     *
     * Set the render target size to the window size:
     *
     * ```lua
     * render.set_render_target_size(self.my_render_target, render.get_window_width(), render.get_window_height())
     * ```
     */
    int RenderScript_SetRenderTargetSize(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        dmGraphics::HRenderTarget render_target = 0x0;

        if (lua_islightuserdata(L, 1))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 1);
            uint32_t width = luaL_checknumber(L, 2);
            uint32_t height = luaL_checknumber(L, 3);
            dmGraphics::SetRenderTargetSize(render_target, width, height);
            return 0;
        }
        else
        {
            return luaL_error(L, "Expected render target as the second argument to %s.set_render_target_size.", RENDER_SCRIPT_LIB_NAME);
        }
    }

    /*# enables a texture for a render target
     *
     * Sets the specified render target's specified buffer to be
     * used as texture with the specified unit.
     * A material shader can then use the texture to sample from.
     *
     * @name render.enable_texture
     * @param unit [type:number] texture unit to enable texture for
     * @param render_target [type:render_target] render target from which to enable the specified texture unit
     * @param buffer_type [type:constant] buffer type from which to enable the texture
     *
     * - `render.BUFFER_COLOR_BIT`
     * - `render.BUFFER_DEPTH_BIT`
     * - `render.BUFFER_STENCIL_BIT`
     *
     * If the render target has been created with multiple color attachments, these buffer types can be used
     * to enable those textures as well. Currently only 4 color attachments are supported.
     *
     * - `render.BUFFER_COLOR0_BIT`
     * - `render.BUFFER_COLOR1_BIT`
     * - `render.BUFFER_COLOR2_BIT`
     * - `render.BUFFER_COLOR3_BIT`
     *
     * @examples
     *
     * ```lua
     * function update(self, dt)
     *     -- enable target so all drawing is done to it
     *     render.set_render_target(self.my_render_target)
     *
     *     -- draw a predicate to the render target
     *     render.draw(self.my_pred)
     *
     *     -- disable target
     *     render.set_render_target(render.RENDER_TARGET_DEFAULT)
     *
     *     render.enable_texture(0, self.my_render_target, render.BUFFER_COLOR_BIT)
     *     -- draw a predicate with the render target available as texture 0 in the predicate
     *     -- material shader.
     *     render.draw(self.my_pred)
     * end
     * ```
     */
    int RenderScript_EnableTexture(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        dmGraphics::HRenderTarget render_target = 0x0;

        uint32_t unit = luaL_checknumber(L, 1);
        if (lua_islightuserdata(L, 2))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 2);
            int buffer_type_value = (int)luaL_checknumber(L, 3);
            dmGraphics::BufferType buffer_type = (dmGraphics::BufferType) buffer_type_value;

            dmGraphics::HTexture texture = dmGraphics::GetRenderTargetTexture(render_target, buffer_type);

            if(texture != 0)
            {
                if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_TEXTURE, unit, (uintptr_t)texture)))
                    return 0;
                else
                    return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
            }
            return luaL_error(L, "Render target does not have a texture for the specified buffer type.");
        }
        else
        {
            return luaL_error(L, "%s.enable_texture(unit, render_target, buffer_type) called with illegal parameters.", RENDER_SCRIPT_LIB_NAME);
        }
    }

    /*# disables a texture for a render target
     *
     * Disables a texture unit for a render target that has previourly been enabled.
     *
     * @name render.disable_texture
     * @param unit [type:number] texture unit to disable
     * @examples
     *
     * ```lua
     * function update(self, dt)
     *     render.enable_texture(0, self.my_render_target, render.BUFFER_COLOR_BIT)
     *     -- draw a predicate with the render target available as texture 0 in the predicate
     *     -- material shader.
     *     render.draw(self.my_pred)
     *     -- done, disable the texture
     *     render.disable_texture(0)
     * end
     * ```
     */
    int RenderScript_DisableTexture(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t unit = luaL_checknumber(L, 1);
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_TEXTURE, unit)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# retrieve the buffer width from a render target
     *
     * Returns the specified buffer width from a render target.
     *
     * @name render.get_render_target_width
     * @param render_target [type:render_target] render target from which to retrieve the buffer width
     * @param buffer_type [type:constant] which type of buffer to retrieve the width from
     *
     * - `render.BUFFER_COLOR_BIT`
     * - `render.BUFFER_DEPTH_BIT`
     * - `render.BUFFER_STENCIL_BIT`
     *
     * @return width [type:number] the width of the render target buffer texture
     * @examples
     *
     * ```lua
     * -- get the width of the render target color buffer
     * local w = render.get_render_target_width(self.target_right, render.BUFFER_COLOR_BIT)
     * ```
     */
    int RenderScript_GetRenderTargetWidth(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        dmGraphics::HRenderTarget render_target = 0x0;

        if (lua_islightuserdata(L, 1))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 1);
        }
        else
        {
            return luaL_error(L, "Expected render target as the first argument to %s.get_render_target_width.", RENDER_SCRIPT_LIB_NAME);
        }
        uint32_t buffer_type = (uint32_t)luaL_checknumber(L, 2);
        if (buffer_type != dmGraphics::BUFFER_TYPE_COLOR0_BIT &&
            buffer_type != dmGraphics::BUFFER_TYPE_DEPTH_BIT &&
            buffer_type != dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            return luaL_error(L, "Unknown buffer type supplied to %s.get_render_target_width.", RENDER_SCRIPT_LIB_NAME);
        }
        uint32_t width, height;
        dmGraphics::GetRenderTargetSize(render_target, (dmGraphics::BufferType)buffer_type, width, height);
        lua_pushnumber(L, width);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# retrieve a buffer height from a render target
     *
     * Returns the specified buffer height from a render target.
     *
     * @name render.get_render_target_height
     * @param render_target [type:render_target] render target from which to retrieve the buffer height
     * @param buffer_type [type:constant] which type of buffer to retrieve the height from
     *
     * - `render.BUFFER_COLOR_BIT`
     * - `render.BUFFER_DEPTH_BIT`
     * - `render.BUFFER_STENCIL_BIT`
     *
     * @return height [type:number] the height of the render target buffer texture
     * @examples
     *
     * ```lua
     * -- get the height of the render target color buffer
     * local h = render.get_render_target_height(self.target_right, render.BUFFER_COLOR_BIT)
     * ```
     */
    int RenderScript_GetRenderTargetHeight(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        dmGraphics::HRenderTarget render_target = 0x0;

        if (lua_islightuserdata(L, 1))
        {
            render_target = (dmGraphics::HRenderTarget)lua_touserdata(L, 1);
        }
        else
        {
            return luaL_error(L, "Expected render target as the first argument to %s.get_render_target_height.", RENDER_SCRIPT_LIB_NAME);
        }
        uint32_t buffer_type = (uint32_t)luaL_checknumber(L, 2);
        if (buffer_type != dmGraphics::BUFFER_TYPE_COLOR0_BIT &&
            buffer_type != dmGraphics::BUFFER_TYPE_DEPTH_BIT &&
            buffer_type != dmGraphics::BUFFER_TYPE_STENCIL_BIT)
        {
            return luaL_error(L, "Unknown buffer type supplied to %s.get_render_target_height.", RENDER_SCRIPT_LIB_NAME);
        }
        uint32_t width, height;
        dmGraphics::GetRenderTargetSize(render_target, (dmGraphics::BufferType)buffer_type, width, height);
        lua_pushnumber(L, height);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*#
     * @name render.BUFFER_COLOR_BIT
     * @variable
     */

    /*#
     * @name render.BUFFER_COLOR0_BIT
     * @variable
     */

    /*#
     * @name render.BUFFER_COLOR1_BIT
     * @variable
     */

    /*#
     * @name render.BUFFER_COLOR2_BIT
     * @variable
     */

    /*#
     * @name render.BUFFER_COLOR3_BIT
     * @variable
     */

    /*#
     * @name render.BUFFER_DEPTH_BIT
     * @variable
     */

    /*#
     * @name render.BUFFER_STENCIL_BIT
     * @variable
     */

    /*# clears the active render target
     * Clear buffers in the currently enabled render target with specified value. If the render target has been created with multiple
     * color attachments, all buffers will be cleared with the same value.
     *
     * @name render.clear
     * @param buffers [type:table] table with keys specifying which buffers to clear and values set to clear values. Available keys are:
     *
     * - `render.BUFFER_COLOR_BIT`
     * - `render.BUFFER_DEPTH_BIT`
     * - `render.BUFFER_STENCIL_BIT`
     *
     * @examples
     *
     * Clear the color buffer and the depth buffer.
     *
     * ```lua
     * render.clear({[render.BUFFER_COLOR_BIT] = vmath.vector4(0, 0, 0, 0), [render.BUFFER_DEPTH_BIT] = 1})
     * ```
     */
    int RenderScript_Clear(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        luaL_checktype(L, 1, LUA_TTABLE);

        int top = lua_gettop(L);
        (void)top;

        uint32_t flags = 0;

        dmVMath::Vector4 color(0.0f, 0.0f, 0.0f, 0.0f);
        float depth = 0.0f;
        uint32_t stencil = 0;

        lua_pushnil(L);
        while (lua_next(L, 1))
        {
            uint32_t buffer_type = luaL_checknumber(L, -2);
            flags |= buffer_type;

            if (buffer_type == dmGraphics::BUFFER_TYPE_COLOR0_BIT)
            {
                color = *dmScript::CheckVector4(L, -1);
            }
            else if (buffer_type == dmGraphics::BUFFER_TYPE_DEPTH_BIT)
            {
                depth = (float)luaL_checknumber(L, -1);
            }
            else if (buffer_type == dmGraphics::BUFFER_TYPE_STENCIL_BIT)
            {
                stencil = (uint32_t)luaL_checknumber(L, -1);
            }
            else
            {
                lua_pop(L, 2);
                assert(top == lua_gettop(L));
                return luaL_error(L, "Unknown buffer type supplied to %s.clear.", RENDER_SCRIPT_LIB_NAME);
            }
            lua_pop(L, 1);
        }
        assert(top == lua_gettop(L));

        uint32_t clear_color = 0;
        clear_color |= ((uint8_t)(color.getX() * 255.0f)) << 0;
        clear_color |= ((uint8_t)(color.getY() * 255.0f)) << 8;
        clear_color |= ((uint8_t)(color.getZ() * 255.0f)) << 16;
        clear_color |= ((uint8_t)(color.getW() * 255.0f)) << 24;

        union float_to_uint32_t {float f; uint32_t i;};
        float_to_uint32_t ftoi;
        ftoi.f = depth;
        if (InsertCommand(i, Command(COMMAND_TYPE_CLEAR, flags, clear_color, ftoi.i, stencil)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# draws all objects matching a predicate
     * Draws all objects that match a specified predicate. An optional constant buffer can be
     * provided to override the default constants. If no constants buffer is provided, a default
     * system constants buffer is used containing constants as defined in materials and set through
     * [ref:go.set] (or [ref:particlefx.set_constant]) on visual components.
     *
     * @name render.draw
     * @param predicate [type:predicate] predicate to draw for
     * @param [options] [type:table] optional table with properties:
     *
     * `frustum`
     * : [type:vmath.matrix4] A frustum matrix used to cull renderable items. (E.g. `local frustum = proj * view`). May be nil.
     *
     * `constants`
     * : [type:constant_buffer] optional constants to use while rendering
     *
     * @examples
     *
     * ```lua
     * function init(self)
     *     -- define a predicate matching anything with material tag "my_tag"
     *     self.my_pred = render.predicate({hash("my_tag")})
     * end
     *
     * function update(self, dt)
     *     -- draw everything in the my_pred predicate
     *     render.draw(self.my_pred)
     * end
     * ```
     *
     * Draw predicate with constants:
     *
     * ```lua
     * local constants = render.constant_buffer()
     * constants.tint = vmath.vector4(1, 1, 1, 1)
     * render.draw(self.my_pred, {constants = constants})
     * ```

     * Draw with predicate and frustum culling:
     *
     * ```lua
     * local frustum = self.proj * self.view
     * render.draw(self.my_pred, {frustum = frustum, constants = constants})
     * ```

     */
    int RenderScript_Draw(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        HPredicate predicate = 0x0;
        if (lua_isuserdata(L, 1))
        {
            HPredicate* tmp = RenderScriptPredicate_Check(L, 1);
            predicate = *tmp;
        }
        else
        {
            return luaL_error(L, "No render predicate specified.");
        }

        dmVMath::Matrix4* frustum_matrix = 0;
        HNamedConstantBuffer constant_buffer = 0;

        if (lua_istable(L, 2))
        {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_pushvalue(L, 2);

            lua_getfield(L, -1, "frustum");
            frustum_matrix = lua_isnil(L, -1) ? 0 : dmScript::CheckMatrix4(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "constants");
            constant_buffer = lua_isnil(L, -1) ? 0 : *RenderScriptConstantBuffer_Check(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }
        else if (lua_isuserdata(L, 2)) // Deprecated
        {
            dmLogOnceWarning("This interface for render.draw() is deprecated. Please see documentation at https://defold.com/ref/stable/render/#render.draw:predicate-[constants]")
            HNamedConstantBuffer* tmp = RenderScriptConstantBuffer_Check(L, 2);
            constant_buffer = *tmp;
        }

        if (frustum_matrix)
        {
            // we need to pass ownership to the command queue
            dmVMath::Matrix4* copy = new dmVMath::Matrix4;
            *copy = *frustum_matrix;
            frustum_matrix = copy;
        }

        if (InsertCommand(i, Command(COMMAND_TYPE_DRAW, (uintptr_t)predicate, (uintptr_t) constant_buffer, (uintptr_t) frustum_matrix)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# draws all 3d debug graphics
     * Draws all 3d debug graphics such as lines drawn with "draw_line" messages and physics visualization.
     * @name render.draw_debug3d
     * @param [options] [type:table] optional table with properties:
     *
     * `frustum`
     * : [type:vmath.matrix4] A frustum matrix used to cull renderable items. (E.g. `local frustum = proj * view`). May be nil.
     *
     * @replaces render.draw_debug2d
     * @examples
     *
     * ```lua
     * function update(self, dt)
     *     -- draw debug visualization
     *     render.draw_debug3d()
     * end
     * ```
     */
    int RenderScript_DrawDebug3d(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        dmVMath::Matrix4* frustum_matrix = 0;

        if (lua_istable(L, 1))
        {
            luaL_checktype(L, 1, LUA_TTABLE);
            lua_pushvalue(L, 1);

            lua_getfield(L, -1, "frustum");
            frustum_matrix = lua_isnil(L, -1) ? 0 : dmScript::CheckMatrix4(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        if (frustum_matrix)
        {
            // we need to pass ownership to the command queue
            dmVMath::Matrix4* copy = new dmVMath::Matrix4;
            *copy = *frustum_matrix;
            frustum_matrix = copy;
        }

        if (InsertCommand(i, Command(COMMAND_TYPE_DRAW_DEBUG3D, (uintptr_t)frustum_matrix)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /* DEPRECATED. NO API DOC GENERATED.
     * draws all 2d debug graphics (Deprecated)
     *
     * @name render.draw_debug2d
     * @deprecated Use render.draw_debug3d() to draw visual debug info.
     */
    int RenderScript_DrawDebug2d(lua_State* L)
    {
        RenderScriptInstance_Check(L);
        dmLogOnceWarning("render.draw_debug2d is deprecated and will be removed in future versions, please use render.draw_debug3d instead.");
        return 0;
    }

    /*# sets the view matrix
     *
     * Sets the view matrix to use when rendering.
     *
     * @name render.set_view
     * @param matrix [type:matrix4] view matrix to set
     * @examples
     *
     * How to set the view and projection matrices according to
     * the values supplied by a camera.
     *
     * ```lua
     * function init(self)
     *   self.view = vmath.matrix4()
     *   self.projection = vmath.matrix4()
     * end
     *
     * function update(self, dt)
     *   -- set the view to the stored view value
     *   render.set_view(self.view)
     *   -- now we can draw with this view
     * end
     *
     * function on_message(self, message_id, message)
     *   if message_id == hash("set_view_projection") then
     *      -- camera view and projection arrives here.
     *      self.view = message.view
     *      self.projection = message.projection
     *   end
     * end
     * ```
     */
    int RenderScript_SetView(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        dmVMath::Matrix4 view = *dmScript::CheckMatrix4(L, 1);

        dmVMath::Matrix4* matrix = new dmVMath::Matrix4;
        *matrix = view;
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_VIEW, (uintptr_t)matrix)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# sets the projection matrix
     * Sets the projection matrix to use when rendering.
     *
     * @name render.set_projection
     * @param matrix [type:matrix4] projection matrix
     * @examples
     *
     * How to set the projection to orthographic with world origo at lower left,
     * width and height as set in project settings and depth (z) between -1 and 1:
     *
     * ```lua
     * render.set_projection(vmath.matrix4_orthographic(0, render.get_width(), 0, render.get_height(), -1, 1))
     * ```
     */
    int RenderScript_SetProjection(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        dmVMath::Matrix4 projection = *dmScript::CheckMatrix4(L, 1);
        dmVMath::Matrix4* matrix = new dmVMath::Matrix4;
        *matrix = projection;
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_PROJECTION, (uintptr_t)matrix)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*#
     * @name render.BLEND_ZERO
     * @variable
     */

    /*#
     * @name render.BLEND_ONE
     * @variable
     */

    /*#
     * @name render.BLEND_SRC_COLOR
     * @variable
     */

    /*#
     * @name render.BLEND_ONE_MINUS_SRC_COLOR
     * @variable
     */

    /*#
     * @name render.BLEND_DST_COLOR
     * @variable
     */

    /*#
     * @name render.BLEND_ONE_MINUS_DST_COLOR
     * @variable
     */

    /*#
     * @name render.BLEND_SRC_ALPHA
     * @variable
     */

    /*#
     * @name render.BLEND_ONE_MINUS_SRC_ALPHA
     * @variable
     */

    /*#
     * @name render.BLEND_DST_ALPHA
     * @variable
     */

    /*#
     * @name render.BLEND_ONE_MINUS_DST_ALPHA
     * @variable
     */

    /*#
     * @name render.BLEND_SRC_ALPHA_SATURATE
     * @variable
     */

    /*#
     * @name render.BLEND_CONSTANT_COLOR
     * @variable
     */

    /*#
     * @name render.BLEND_ONE_MINUS_CONSTANT_COLOR
     * @variable
     */

     /*#
      * @name render.BLEND_CONSTANT_ALPHA
      * @variable
      */

     /*#
      * @name render.BLEND_ONE_MINUS_CONSTANT_ALPHA
      * @variable
      */

     /*# sets the blending function
     *
     * Specifies the arithmetic used when computing pixel values that are written to the frame
     * buffer. In RGBA mode, pixels can be drawn using a function that blends the source RGBA
     * pixel values with the destination pixel values already in the frame buffer.
     * Blending is initially disabled.
     *
     * `source_factor` specifies which method is used to scale the source color components.
     * `destination_factor` specifies which method is used to scale the destination color
     * components.
     *
     * Source color components are referred to as (R<sub>s</sub>,G<sub>s</sub>,B<sub>s</sub>,A<sub>s</sub>).
     * Destination color components are referred to as (R<sub>d</sub>,G<sub>d</sub>,B<sub>d</sub>,A<sub>d</sub>).
     * The color specified by setting the blendcolor is referred to as (R<sub>c</sub>,G<sub>c</sub>,B<sub>c</sub>,A<sub>c</sub>).
     *
     * The source scale factor is referred to as (s<sub>R</sub>,s<sub>G</sub>,s<sub>B</sub>,s<sub>A</sub>).
     * The destination scale factor is referred to as (d<sub>R</sub>,d<sub>G</sub>,d<sub>B</sub>,d<sub>A</sub>).
     *
     * The color values have integer values between 0 and (k<sub>R</sub>,k<sub>G</sub>,k<sub>B</sub>,k<sub>A</sub>), where k<sub>c</sub> = 2<sup>m<sub>c</sub></sup> - 1 and m<sub>c</sub> is the number of bitplanes for that color. I.e for 8 bit color depth, color values are between `0` and `255`.

     * Available factor constants and corresponding scale factors:
     *
     * Factor constant                         | Scale factor (f<sub>R</sub>,f<sub>G</sub>,f<sub>B</sub>,f<sub>A</sub>)
     * --------------------------------------- | -----------------------
     * `render.BLEND_ZERO`                     | (0,0,0,0)
     * `render.BLEND_ONE`                      | (1,1,1,1)
     * `render.BLEND_SRC_COLOR`                | (R<sub>s</sub>/k<sub>R</sub>,G<sub>s</sub>/k<sub>G</sub>,B<sub>s</sub>/k<sub>B</sub>,A<sub>s</sub>/k<sub>A</sub>)
     * `render.BLEND_ONE_MINUS_SRC_COLOR`      | (1,1,1,1) - (R<sub>s</sub>/k<sub>R</sub>,G<sub>s</sub>/k<sub>G</sub>,B<sub>s</sub>/k<sub>B</sub>,A<sub>s</sub>/k<sub>A</sub>)
     * `render.BLEND_DST_COLOR`                | (R<sub>d</sub>/k<sub>R</sub>,G<sub>d</sub>/k<sub>G</sub>,B<sub>d</sub>/k<sub>B</sub>,A<sub>d</sub>/k<sub>A</sub>)
     * `render.BLEND_ONE_MINUS_DST_COLOR`      | (1,1,1,1) - (R<sub>d</sub>/k<sub>R</sub>,G<sub>d</sub>/k<sub>G</sub>,B<sub>d</sub>/k<sub>B</sub>,A<sub>d</sub>/k<sub>A</sub>)
     * `render.BLEND_SRC_ALPHA`                | (A<sub>s</sub>/k<sub>A</sub>,A<sub>s</sub>/k<sub>A</sub>,A<sub>s</sub>/k<sub>A</sub>,A<sub>s</sub>/k<sub>A</sub>)
     * `render.BLEND_ONE_MINUS_SRC_ALPHA`      | (1,1,1,1) - (A<sub>s</sub>/k<sub>A</sub>,A<sub>s</sub>/k<sub>A</sub>,A<sub>s</sub>/k<sub>A</sub>,A<sub>s</sub>/k<sub>A</sub>)
     * `render.BLEND_DST_ALPHA`                | (A<sub>d</sub>/k<sub>A</sub>,A<sub>d</sub>/k<sub>A</sub>,A<sub>d</sub>/k<sub>A</sub>,A<sub>d</sub>/k<sub>A</sub>)
     * `render.BLEND_ONE_MINUS_DST_ALPHA`      | (1,1,1,1) - (A<sub>d</sub>/k<sub>A</sub>,A<sub>d</sub>/k<sub>A</sub>,A<sub>d</sub>/k<sub>A</sub>,A<sub>d</sub>/k<sub>A</sub>)
     * `render.BLEND_CONSTANT_COLOR`           | (R<sub>c</sub>,G<sub>c</sub>,B<sub>c</sub>,A<sub>c</sub>)
     * `render.BLEND_ONE_MINUS_CONSTANT_COLOR` | (1,1,1,1) - (R<sub>c</sub>,G<sub>c</sub>,B<sub>c</sub>,A<sub>c</sub>)
     * `render.BLEND_CONSTANT_ALPHA`           | (A<sub>c</sub>,A<sub>c</sub>,A<sub>c</sub>,A<sub>c</sub>)
     * `render.BLEND_ONE_MINUS_CONSTANT_ALPHA` | (1,1,1,1) - (A<sub>c</sub>,A<sub>c</sub>,A<sub>c</sub>,A<sub>c</sub>)
     * `render.BLEND_SRC_ALPHA_SATURATE`       | (i,i,i,1) where i = min(A<sub>s</sub>, k<sub>A</sub> - A<sub>d</sub>) /k<sub>A</sub>
     *
     * The blended RGBA values of a pixel comes from the following equations:
     *
     * - R<sub>d</sub> = min(k<sub>R</sub>, R<sub>s</sub> * s<sub>R</sub> + R<sub>d</sub> * d<sub>R</sub>)
     * - G<sub>d</sub> = min(k<sub>G</sub>, G<sub>s</sub> * s<sub>G</sub> + G<sub>d</sub> * d<sub>G</sub>)
     * - B<sub>d</sub> = min(k<sub>B</sub>, B<sub>s</sub> * s<sub>B</sub> + B<sub>d</sub> * d<sub>B</sub>)
     * - A<sub>d</sub> = min(k<sub>A</sub>, A<sub>s</sub> * s<sub>A</sub> + A<sub>d</sub> * d<sub>A</sub>)
     *
     * Blend function `(render.BLEND_SRC_ALPHA, render.BLEND_ONE_MINUS_SRC_ALPHA)` is useful for
     * drawing with transparency when the drawn objects are sorted from farthest to nearest.
     * It is also useful for drawing antialiased points and lines in arbitrary order.
     *
     * @name render.set_blend_func
     * @param source_factor [type:constant] source factor
     * @param destination_factor [type:constant] destination factor
     * @examples
     *
     * Set the blend func to the most common one:
     *
     * ```lua
     * render.set_blend_func(render.BLEND_SRC_ALPHA, render.BLEND_ONE_MINUS_SRC_ALPHA)
     * ```
     */
    int RenderScript_SetBlendFunc(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t factors[2];
        for (uint32_t i = 0; i < 2; ++i)
        {
            factors[i] = luaL_checknumber(L, 1+i);
        }
        for (uint32_t i = 0; i < 2; ++i)
        {
            if (factors[i] != dmGraphics::BLEND_FACTOR_ZERO &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE &&
                factors[i] != dmGraphics::BLEND_FACTOR_SRC_COLOR &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_COLOR &&
                factors[i] != dmGraphics::BLEND_FACTOR_DST_COLOR &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR &&
                factors[i] != dmGraphics::BLEND_FACTOR_SRC_ALPHA &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
                factors[i] != dmGraphics::BLEND_FACTOR_DST_ALPHA &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_ALPHA &&
                factors[i] != dmGraphics::BLEND_FACTOR_SRC_ALPHA_SATURATE &&
                factors[i] != dmGraphics::BLEND_FACTOR_CONSTANT_COLOR &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR &&
                factors[i] != dmGraphics::BLEND_FACTOR_CONSTANT_ALPHA &&
                factors[i] != dmGraphics::BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA)
            {
                return luaL_error(L, "Invalid blend types: %s.set_blend_func(self, %d, %d)", RENDER_SCRIPT_LIB_NAME, factors[0], factors[1]);
            }
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_BLEND_FUNC, factors[0], factors[1])))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# sets the color mask
     *
     * Specifies whether the individual color components in the frame buffer is enabled for writing (`true`) or disabled (`false`). For example, if `blue` is `false`, nothing is written to the blue component of any pixel in any of the color buffers, regardless of the drawing operation attempted. Note that writing are either enabled or disabled for entire color components, not the individual bits of a component.
     *
     * The component masks are all initially `true`.
     *
     * @name render.set_color_mask
     * @param red [type:boolean] red mask
     * @param green [type:boolean] green mask
     * @param blue [type:boolean] blue mask
     * @param alpha [type:boolean] alpha mask
     * @examples
     *
     * ```lua
     * -- alpha cannot be written to frame buffer
     * render.set_color_mask(true, true, true, false)
     * ```
     */
    int RenderScript_SetColorMask(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);

        if (lua_isboolean(L, 1) && lua_isboolean(L, 2) && lua_isboolean(L, 3) && lua_isboolean(L, 4))
        {
            bool red = lua_toboolean(L, 1) != 0;
            bool green = lua_toboolean(L, 2) != 0;
            bool blue = lua_toboolean(L, 3) != 0;
            bool alpha = lua_toboolean(L, 4) != 0;
            if (!InsertCommand(i, Command(COMMAND_TYPE_SET_COLOR_MASK, (uintptr_t)red, (uintptr_t)green, (uintptr_t)blue, (uintptr_t)alpha)))
                return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
        else
        {
            return luaL_error(L, "Expected booleans but got %s, %s, %s, %s.", lua_typename(L, lua_type(L, 2)), lua_typename(L, lua_type(L, 3)), lua_typename(L, lua_type(L, 4)), lua_typename(L, lua_type(L, 5)));
        }
        return 0;
    }

    /*# sets the depth mask
     *
     * Specifies whether the depth buffer is enabled for writing. The supplied mask governs
     * if depth buffer writing is enabled (`true`) or disabled (`false`).
     *
     * The mask is initially `true`.
     *
     * @name render.set_depth_mask
     * @param depth [type:boolean] depth mask
     * @examples
     *
     * How to turn off writing to the depth buffer:
     *
     * ```lua
     * render.set_depth_mask(false)
     * ```
     */
    int RenderScript_SetDepthMask(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);

        if (lua_isboolean(L, 1))
        {
            bool mask = lua_toboolean(L, 1) != 0;
            if (!InsertCommand(i, Command(COMMAND_TYPE_SET_DEPTH_MASK, (uintptr_t)mask)))
                return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
        }
        else
        {
            return luaL_error(L, "Expected boolean but got %s.", lua_typename(L, lua_type(L, 2)));
        }
        return 0;
    }

    /*# sets the stencil mask
     *
     * The stencil mask controls the writing of individual bits in the stencil buffer.
     * The least significant `n` bits of the parameter `mask`, where `n` is the number of
     * bits in the stencil buffer, specify the mask.
     *
     * Where a `1` bit appears in the mask, the corresponding
     * bit in the stencil buffer can be written. Where a `0` bit appears in the mask,
     * the corresponding bit in the stencil buffer is never written.
     *
     * The mask is initially all `1`'s.
     *
     * @name render.set_stencil_mask
     * @param mask [type:number] stencil mask
     *
     * @examples
     *
     * ```lua
     * -- set the stencil mask to all 1:s
     * render.set_stencil_mask(0xff)
     * ```
     */
    int RenderScript_SetStencilMask(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);

        uint32_t mask = (uint32_t)luaL_checknumber(L, 1);
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_STENCIL_MASK, mask)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*#
     * @name render.COMPARE_FUNC_NEVER
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_LESS
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_LEQUAL
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_GREATER
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_GEQUAL
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_EQUAL
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_NOTEQUAL
     * @variable
     */

    /*#
     * @name render.COMPARE_FUNC_ALWAYS
     * @variable
     */

    /*# sets the depth test function
    *
    * Specifies the function that should be used to compare each incoming pixel
    * depth value with the value present in the depth buffer.
    * The comparison is performed only if depth testing is enabled and specifies
    * the conditions under which a pixel will be drawn.
    *
    * Function constants:
    *
    * - `render.COMPARE_FUNC_NEVER` (never passes)
    * - `render.COMPARE_FUNC_LESS` (passes if the incoming depth value is less than the stored value)
    * - `render.COMPARE_FUNC_LEQUAL` (passes if the incoming depth value is less than or equal to the stored value)
    * - `render.COMPARE_FUNC_GREATER` (passes if the incoming depth value is greater than the stored value)
    * - `render.COMPARE_FUNC_GEQUAL` (passes if the incoming depth value is greater than or equal to the stored value)
    * - `render.COMPARE_FUNC_EQUAL` (passes if the incoming depth value is equal to the stored value)
    * - `render.COMPARE_FUNC_NOTEQUAL` (passes if the incoming depth value is not equal to the stored value)
    * - `render.COMPARE_FUNC_ALWAYS` (always passes)
    *
    * The depth function is initially set to `render.COMPARE_FUNC_LESS`.
    *
    * @name render.set_depth_func
    * @param func [type:constant] depth test function, see the description for available values
    * @examples
    *
    * Enable depth test and set the depth test function to "not equal".
    *
    * ```lua
    * render.enable_state(render.STATE_DEPTH_TEST)
    * render.set_depth_func(render.COMPARE_FUNC_NOTEQUAL)
    * ```
    */
    int RenderScript_SetDepthFunc(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t func = luaL_checknumber(L, 1);
        if (func != dmGraphics::COMPARE_FUNC_NEVER &&
            func != dmGraphics::COMPARE_FUNC_LESS &&
            func != dmGraphics::COMPARE_FUNC_LEQUAL &&
            func != dmGraphics::COMPARE_FUNC_GREATER &&
            func != dmGraphics::COMPARE_FUNC_GEQUAL &&
            func != dmGraphics::COMPARE_FUNC_EQUAL &&
            func != dmGraphics::COMPARE_FUNC_NOTEQUAL &&
            func != dmGraphics::COMPARE_FUNC_ALWAYS)
        {
            return luaL_error(L, "Invalid depth func: %s.set_depth_func(self, %d)", RENDER_SCRIPT_LIB_NAME, func);
        }

        if (InsertCommand(i, Command(COMMAND_TYPE_SET_DEPTH_FUNC, func)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }


    /*# sets the stencil test function
    *
    * Stenciling is similar to depth-buffering as it enables and disables drawing on a
    * per-pixel basis. First, GL drawing primitives are drawn into the stencil planes.
    * Second, geometry and images are rendered but using the stencil planes to mask out
    * where to draw.
    *
    * The stencil test discards a pixel based on the outcome of a comparison between the
    * reference value `ref` and the corresponding value in the stencil buffer.
    *
    * `func` specifies the comparison function. See the table below for values.
    * The initial value is `render.COMPARE_FUNC_ALWAYS`.
    *
    * `ref` specifies the reference value for the stencil test. The value is clamped to
    * the range [0, 2<sup>n</sup>-1], where n is the number of bitplanes in the stencil buffer.
    * The initial value is `0`.
    *
    * `mask` is ANDed with both the reference value and the stored stencil value when the test
    * is done. The initial value is all `1`'s.
    *
    * Function constant:
    *
    * - `render.COMPARE_FUNC_NEVER` (never passes)
    * - `render.COMPARE_FUNC_LESS` (passes if (ref & mask) < (stencil & mask))
    * - `render.COMPARE_FUNC_LEQUAL` (passes if (ref & mask) <= (stencil & mask))
    * - `render.COMPARE_FUNC_GREATER` (passes if (ref & mask) > (stencil & mask))
    * - `render.COMPARE_FUNC_GEQUAL` (passes if (ref & mask) >= (stencil & mask))
    * - `render.COMPARE_FUNC_EQUAL` (passes if (ref & mask) = (stencil & mask))
    * - `render.COMPARE_FUNC_NOTEQUAL` (passes if (ref & mask) != (stencil & mask))
    * - `render.COMPARE_FUNC_ALWAYS` (always passes)
    *
    * @name render.set_stencil_func
    * @param func [type:constant] stencil test function, see the description for available values
    * @param ref [type:number] reference value for the stencil test
    * @param mask [type:number] mask that is ANDed with both the reference value and the stored stencil value when the test is done
    * @examples
    *
    * ```lua
    * -- let only 0's pass the stencil test
    * render.set_stencil_func(render.COMPARE_FUNC_EQUAL, 0, 1)
    * ```
    *
    */
    int RenderScript_SetStencilFunc(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t func = luaL_checknumber(L, 1);
        if (func != dmGraphics::COMPARE_FUNC_NEVER &&
            func != dmGraphics::COMPARE_FUNC_LESS &&
            func != dmGraphics::COMPARE_FUNC_LEQUAL &&
            func != dmGraphics::COMPARE_FUNC_GREATER &&
            func != dmGraphics::COMPARE_FUNC_GEQUAL &&
            func != dmGraphics::COMPARE_FUNC_EQUAL &&
            func != dmGraphics::COMPARE_FUNC_NOTEQUAL &&
            func != dmGraphics::COMPARE_FUNC_ALWAYS)
        {
            return luaL_error(L, "Invalid stencil func: %s.set_stencil_func(self, %d)", RENDER_SCRIPT_LIB_NAME, func);
        }
        uint32_t ref = luaL_checknumber(L, 2);
        uint32_t mask = luaL_checknumber(L, 3);

        if (InsertCommand(i, Command(COMMAND_TYPE_SET_STENCIL_FUNC, func, ref, mask)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*#
     * @name render.STENCIL_OP_KEEP
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_ZERO
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_REPLACE
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_INCR
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_INCR_WRAP
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_DECR
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_DECR_WRAP
     * @variable
     */

    /*#
     * @name render.STENCIL_OP_INVERT
     * @variable
     */

    /*# sets the stencil operator
    *
    * The stencil test discards a pixel based on the outcome of a comparison between the
    * reference value `ref` and the corresponding value in the stencil buffer.
    * To control the test, call [ref:render.set_stencil_func].
    *
    * This function takes three arguments that control what happens to the stored stencil
    * value while stenciling is enabled. If the stencil test fails, no change is made to the
    * pixel's color or depth buffers, and `sfail` specifies what happens to the stencil buffer
    * contents.
    *
    * Operator constants:
    *
    * - `render.STENCIL_OP_KEEP` (keeps the current value)
    * - `render.STENCIL_OP_ZERO` (sets the stencil buffer value to 0)
    * - `render.STENCIL_OP_REPLACE` (sets the stencil buffer value to `ref`, as specified by [ref:render.set_stencil_func])
    * - `render.STENCIL_OP_INCR` (increments the stencil buffer value and clamp to the maximum representable unsigned value)
    * - `render.STENCIL_OP_INCR_WRAP` (increments the stencil buffer value and wrap to zero when incrementing the maximum representable unsigned value)
    * - `render.STENCIL_OP_DECR` (decrements the current stencil buffer value and clamp to 0)
    * - `render.STENCIL_OP_DECR_WRAP` (decrements the current stencil buffer value and wrap to the maximum representable unsigned value when decrementing zero)
    * - `render.STENCIL_OP_INVERT` (bitwise inverts the current stencil buffer value)
    *
    * `dppass` and `dpfail` specify the stencil buffer actions depending on whether subsequent
    * depth buffer tests succeed (dppass) or fail (dpfail).
    *
    * The initial value for all operators is `render.STENCIL_OP_KEEP`.
    *
    * @name render.set_stencil_op
    * @param sfail [type:constant] action to take when the stencil test fails
    * @param dpfail [type:constant] the stencil action when the stencil test passes
    * @param dppass  [type:constant] the stencil action when both the stencil test and the depth test pass, or when the stencil test passes and either there is no depth buffer or depth testing is not enabled
    * @examples
    *
    * Set the stencil function to never pass and operator to always draw 1's
    * on test fail.
    *
    * ```lua
    * render.set_stencil_func(render.COMPARE_FUNC_NEVER, 1, 0xFF)
    * -- always draw 1's on test fail
    * render.set_stencil_op(render.STENCIL_OP_REPLACE, render.STENCIL_OP_KEEP, render.STENCIL_OP_KEEP)
    * ```
    */
    int RenderScript_SetStencilOp(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t ops[3];
        for (uint32_t i = 0; i < 3; ++i)
        {
            ops[i] = luaL_checknumber(L, 1+i);
        }
        for (uint32_t i = 0; i < 3; ++i)
        {
            if (ops[i] != dmGraphics::STENCIL_OP_KEEP &&
                ops[i] != dmGraphics::STENCIL_OP_ZERO &&
                ops[i] != dmGraphics::STENCIL_OP_REPLACE &&
                ops[i] != dmGraphics::STENCIL_OP_INCR &&
                ops[i] != dmGraphics::STENCIL_OP_INCR_WRAP &&
                ops[i] != dmGraphics::STENCIL_OP_DECR &&
                ops[i] != dmGraphics::STENCIL_OP_DECR_WRAP &&
                ops[i] != dmGraphics::STENCIL_OP_INVERT)
            {
                return luaL_error(L, "Invalid stencil ops: %s.set_stencil_op(self, %d, %d, %d)", RENDER_SCRIPT_LIB_NAME, ops[0], ops[1], ops[2]);
            }
        }

        if (InsertCommand(i, Command(COMMAND_TYPE_SET_STENCIL_OP, ops[0], ops[1], ops[2])))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*#
     * @name render.FACE_FRONT
     * @variable
     */

    /*#
     * @name render.FACE_BACK
     * @variable
     */

    /*#
     * @name render.FACE_FRONT_AND_BACK
     * @variable
     */

    /*# sets the cull face
     *
     * Specifies whether front- or back-facing polygons can be culled
     * when polygon culling is enabled. Polygon culling is initially disabled.
     *
     * If mode is `render.FACE_FRONT_AND_BACK`, no polygons are drawn, but other
     * primitives such as points and lines are drawn. The initial value for
     * `face_type` is `render.FACE_BACK`.
     *
     * @name render.set_cull_face
     * @param face_type [type:constant] face type
     *
     * - `render.FACE_FRONT`
     * - `render.FACE_BACK`
     * - `render.FACE_FRONT_AND_BACK`
     *
     * @examples
     *
     * How to enable polygon culling and set front face culling:
     *
     * ```lua
     * render.enable_state(render.STATE_CULL_FACE)
     * render.set_cull_face(render.FACE_FRONT)
     * ```
     */
    int RenderScript_SetCullFace(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        uint32_t face_type = luaL_checknumber(L, 1);
        if (face_type != dmGraphics::FACE_TYPE_FRONT &&
            face_type != dmGraphics::FACE_TYPE_BACK &&
            face_type != dmGraphics::FACE_TYPE_FRONT_AND_BACK)
        {
            return luaL_error(L, "Invalid face types: %s.set_cull_face(self, %d)", RENDER_SCRIPT_LIB_NAME, face_type);
        }
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_CULL_FACE, (uintptr_t)face_type)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# sets the polygon offset
     *
     * Sets the scale and units used to calculate depth values.
     * If `render.STATE_POLYGON_OFFSET_FILL` is enabled, each fragment's depth value
     * is offset from its interpolated value (depending on the depth value of the
     * appropriate vertices). Polygon offset can be used when drawing decals, rendering
     * hidden-line images etc.
     *
     * `factor` specifies a scale factor that is used to create a variable depth
     * offset for each polygon. The initial value is `0`.
     *
     * `units` is multiplied by an implementation-specific value to create a
     * constant depth offset. The initial value is `0`.
     *
     * The value of the offset is computed as `factor` &times; `DZ` + `r` &times; `units`
     *
     * `DZ` is a measurement of the depth slope of the polygon which is the change in z (depth)
     * values divided by the change in either x or y coordinates, as you traverse a polygon.
     * The depth values are in window coordinates, clamped to the range [0, 1].
     *
     * `r` is the smallest value that is guaranteed to produce a resolvable difference.
     * It's value is an implementation-specific constant.
     *
     * The offset is added before the depth test is performed and before the
     * value is written into the depth buffer.
     *
     * @name render.set_polygon_offset
     * @param factor [type:number] polygon offset factor
     * @param units [type:number] polygon offset units
     * @examples
     *
     * ```lua
     * render.enable_state(render.STATE_POLYGON_OFFSET_FILL)
     * render.set_polygon_offset(1.0, 1.0)
     * ```
     */
    int RenderScript_SetPolygonOffset(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        float factor = luaL_checknumber(L, 1);
        float units = luaL_checknumber(L, 2);
        if (InsertCommand(i, Command(COMMAND_TYPE_SET_POLYGON_OFFSET, (uintptr_t)factor, (uintptr_t)units)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    /*# gets the window width, as specified for the project
     *
     * Returns the logical window width that is set in the "game.project" settings.
     * Note that the actual window pixel size can change, either by device constraints
     * or user input.
     *
     * @name render.get_width
     * @return width [type:number] specified window width (number)
     * @examples
     *
     * Get the width of the window.
     *
     * ```lua
     * local w = render.get_width()
     * ```
     */
    int RenderScript_GetWidth(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWidth(i->m_RenderContext->m_GraphicsContext));
        return 1;
    }

    /*# gets the window height, as specified for the project
     *
     * Returns the logical window height that is set in the "game.project" settings.
     * Note that the actual window pixel size can change, either by device constraints
     * or user input.
     *
     * @name render.get_height
     * @return height [type:number] specified window height
     * @examples
     *
     * Get the height of the window
     *
     * ```lua
     * local h = render.get_height()
     * ```
     */
    int RenderScript_GetHeight(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetHeight(i->m_RenderContext->m_GraphicsContext));
        return 1;
    }

    /*# gets the actual window width
     *
     * Returns the actual physical window width.
     * Note that this value might differ from the logical width that is set in the
     * "game.project" settings.
     *
     * @name render.get_window_width
     * @return width [type:number] actual window width
     * @examples
     *
     * Get the actual width of the window
     *
     * ```lua
     * local w = render.get_window_width()
     * ```
     */
    int RenderScript_GetWindowWidth(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWindowWidth(i->m_RenderContext->m_GraphicsContext));
        return 1;
    }

    /*# gets the actual window height
     *
     * Returns the actual physical window height.
     * Note that this value might differ from the logical height that is set in the
     * "game.project" settings.
     *
     * @name render.get_window_height
     * @return height [type:number] actual window height
     * @examples
     *
     * Get the actual height of the window
     *
     * ```lua
     * local h = render.get_window_height()
     * ```
     */
    int RenderScript_GetWindowHeight(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWindowHeight(i->m_RenderContext->m_GraphicsContext));
        return 1;
    }

    /*# creates a new render predicate
     *
     * This function returns a new render predicate for objects with materials matching
     * the provided material tags. The provided tags are combined into a bit mask
     * for the predicate. If multiple tags are provided, the predicate matches materials
     * with all tags ANDed together.
     *
     * The current limit to the number of tags that can be defined is `64`.
     *
     * @name render.predicate
     * @param tags [type:table] table of tags that the predicate should match. The tags can be of either hash or string type
     * @return predicate [type:predicate] new predicate
     * @examples
     *
     * Create a new render predicate containing all visual objects that
     * have a material with material tags "opaque" AND "smoke".
     *
     * ```
     * local p = render.predicate({hash("opaque"), hash("smoke")})
     * ```
     */
    int RenderScript_Predicate(lua_State* L)
    {
        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        (void)i;
        luaL_checktype(L, 1, LUA_TTABLE);

        HPredicate* p_predicate = (HPredicate*) lua_newuserdata(L, sizeof(HPredicate*));
        *p_predicate = NewPredicate();

        luaL_getmetatable(L, RENDER_SCRIPT_PREDICATE);
        lua_setmetatable(L, -2);

        lua_pushnil(L);  /* first key */
        while (lua_next(L, 1) != 0)
        {
            dmhash_t tag = dmScript::CheckHashOrString(L, -1);
            if (RESULT_OK != AddPredicateTag(*p_predicate, tag))
            {
                dmLogWarning("Unable to add predicate tag. Max number of tags (%i) reached?", dmRender::Predicate::MAX_TAG_COUNT);
            }
            lua_pop(L, 1);
        }
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# enables a material
     * If another material was already enabled, it will be automatically disabled
     * and the specified material is used instead.
     *
     * The name of the material must be specified in the ".render" resource set
     * in the "game.project" setting.
     *
     * @name render.enable_material
     * @param material_id [type:string|hash] material id to enable
     * @examples
     *
     * Enable material named "glow", then draw my_pred with it.
     *
     * ```lua
     * render.enable_material("glow")
     * render.draw(self.my_pred)
     * render.disable_material()
     * ```
     */
    int RenderScript_EnableMaterial(lua_State* L)
    {
        int top = lua_gettop(L);
        (void)top;

        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        if (!lua_isnil(L, 1))
        {
            dmhash_t material_id = dmScript::CheckHashOrString(L, 1);
            dmRender::HMaterial* mat = i->m_Materials.Get(material_id);
            if (mat == 0x0)
            {
                assert(top == lua_gettop(L));
                char str[128];
                char buffer[256];
                dmSnPrintf(buffer, sizeof(buffer), "Could not find material '%s' %llu", dmScript::GetStringFromHashOrString(L, 1, str, sizeof(str)), (unsigned long long)material_id); // since lua doesn't support proper format arguments
                return luaL_error(L, "%s", buffer);
            }
            else
            {
                HMaterial material = *mat;
                if (InsertCommand(i, Command(COMMAND_TYPE_ENABLE_MATERIAL, (uintptr_t)material)))
                {
                    assert(top == lua_gettop(L));
                    return 0;
                }
                else
                {
                    assert(top == lua_gettop(L));
                    return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
                }
            }
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "%s.enable_material was supplied nil as material.", RENDER_SCRIPT_LIB_NAME);
        }
    }

    /*# disables the currently enabled material
     * If a material is currently enabled, disable it.
     *
     * The name of the material must be specified in the ".render" resource set
     * in the "game.project" setting.
     *
     * @name render.disable_material
     * @examples
     *
     * Enable material named "glow", then draw my_pred with it.
     *
     * ```lua
     * render.enable_material("glow")
     * render.draw(self.my_pred)
     * render.disable_material()
     * ```
     */
    int RenderScript_DisableMaterial(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L);
        if (InsertCommand(i, Command(COMMAND_TYPE_DISABLE_MATERIAL)))
            return 0;
        else
            return luaL_error(L, "Command buffer is full (%d).", i->m_CommandBuffer.Capacity());
    }

    static const luaL_reg Render_methods[] =
    {
        {"enable_state",                    RenderScript_EnableState},
        {"disable_state",                   RenderScript_DisableState},
        {"render_target",                   RenderScript_RenderTarget},
        {"delete_render_target",            RenderScript_DeleteRenderTarget},
        {"set_render_target",               RenderScript_SetRenderTarget},
        {"enable_render_target",            RenderScript_EnableRenderTarget},
        {"disable_render_target",           RenderScript_DisableRenderTarget},
        {"set_render_target_size",          RenderScript_SetRenderTargetSize},
        {"enable_texture",                  RenderScript_EnableTexture},
        {"disable_texture",                 RenderScript_DisableTexture},
        {"get_render_target_width",         RenderScript_GetRenderTargetWidth},
        {"get_render_target_height",        RenderScript_GetRenderTargetHeight},
        {"clear",                           RenderScript_Clear},
        {"set_viewport",                    RenderScript_SetViewport},
        {"set_view",                        RenderScript_SetView},
        {"set_projection",                  RenderScript_SetProjection},
        {"set_blend_func",                  RenderScript_SetBlendFunc},
        {"set_color_mask",                  RenderScript_SetColorMask},
        {"set_depth_mask",                  RenderScript_SetDepthMask},
        {"set_depth_func",                  RenderScript_SetDepthFunc},
        {"set_stencil_mask",                RenderScript_SetStencilMask},
        {"set_stencil_func",                RenderScript_SetStencilFunc},
        {"set_stencil_op",                  RenderScript_SetStencilOp},
        {"set_cull_face",                   RenderScript_SetCullFace},
        {"set_polygon_offset",              RenderScript_SetPolygonOffset},
        {"draw",                            RenderScript_Draw},
        {"draw_debug3d",                    RenderScript_DrawDebug3d},
        {"draw_debug2d",                    RenderScript_DrawDebug2d},
        {"get_width",                       RenderScript_GetWidth},
        {"get_height",                      RenderScript_GetHeight},
        {"get_window_width",                RenderScript_GetWindowWidth},
        {"get_window_height",               RenderScript_GetWindowHeight},
        {"predicate",                       RenderScript_Predicate},
        {"constant_buffer",                 RenderScript_ConstantBuffer},
        {"enable_material",                 RenderScript_EnableMaterial},
        {"disable_material",                RenderScript_DisableMaterial},
        {0, 0}
    };

    void InitializeRenderScriptContext(RenderScriptContext& context, dmGraphics::HContext graphics_context, dmScript::HContext script_context, uint32_t command_buffer_size)
    {
        context.m_CommandBufferSize = command_buffer_size;

        lua_State *L = dmScript::GetLuaState(script_context);
        context.m_LuaState = L;

        int top = lua_gettop(L);
        (void)top;

        RENDER_SCRIPT_TYPE_HASH = dmScript::RegisterUserType(L, RENDER_SCRIPT, RenderScript_methods, RenderScript_meta);

        RENDER_SCRIPT_INSTANCE_TYPE_HASH = dmScript::RegisterUserType(L, RENDER_SCRIPT_INSTANCE, RenderScriptInstance_methods, RenderScriptInstance_meta);

        RENDER_SCRIPT_CONSTANTBUFFER_TYPE_HASH = dmScript::RegisterUserType(L, RENDER_SCRIPT_CONSTANTBUFFER, RenderScriptConstantBuffer_methods, RenderScriptConstantBuffer_meta);

        RENDER_SCRIPT_PREDICATE_TYPE_HASH = dmScript::RegisterUserType(L, RENDER_SCRIPT_PREDICATE, RenderScriptPredicate_methods, RenderScriptPredicate_meta);

        RENDER_SCRIPT_CONSTANTBUFFER_ARRAY_TYPE_HASH = dmScript::RegisterUserType(L, RENDER_SCRIPT_CONSTANTBUFFER_ARRAY, RenderScriptConstantBuffer_methods, RenderScriptConstantBufferArray_meta);

        luaL_register(L, RENDER_SCRIPT_LIB_NAME, Render_methods);

#define REGISTER_STATE_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::name); \
        lua_setfield(L, -2, #name);

        REGISTER_STATE_CONSTANT(STATE_DEPTH_TEST);
        REGISTER_STATE_CONSTANT(STATE_STENCIL_TEST);
        REGISTER_STATE_CONSTANT(STATE_ALPHA_TEST);
        REGISTER_STATE_CONSTANT(STATE_BLEND);
        REGISTER_STATE_CONSTANT(STATE_CULL_FACE);
        REGISTER_STATE_CONSTANT(STATE_POLYGON_OFFSET_FILL);

#undef REGISTER_STATE_CONSTANT

#define REGISTER_FORMAT_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::TEXTURE_FORMAT_##name); \
        lua_setfield(L, -2, "FORMAT_"#name);

        REGISTER_FORMAT_CONSTANT(LUMINANCE);
        REGISTER_FORMAT_CONSTANT(RGBA);
        REGISTER_FORMAT_CONSTANT(DEPTH);
        REGISTER_FORMAT_CONSTANT(STENCIL);

#undef REGISTER_FORMAT_CONSTANT

#define REGISTER_FORMAT_CONSTANT(name)\
        if (dmGraphics::IsTextureFormatSupported(graphics_context, dmGraphics::TEXTURE_FORMAT_##name)) { \
            lua_pushnumber(L, (lua_Number) dmGraphics::TEXTURE_FORMAT_##name); \
            lua_setfield(L, -2, "FORMAT_"#name); \
        }

        // These depend on driver support
        REGISTER_FORMAT_CONSTANT(RGB);
        REGISTER_FORMAT_CONSTANT(RGB16F);
        REGISTER_FORMAT_CONSTANT(RGB32F);
        REGISTER_FORMAT_CONSTANT(RGBA16F);
        REGISTER_FORMAT_CONSTANT(RGBA32F);
        REGISTER_FORMAT_CONSTANT(R16F);
        REGISTER_FORMAT_CONSTANT(RG16F);
        REGISTER_FORMAT_CONSTANT(R32F);
        REGISTER_FORMAT_CONSTANT(RG32F);

#undef REGISTER_FORMAT_CONSTANT

#define REGISTER_FILTER_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::TEXTURE_FILTER_##name); \
        lua_setfield(L, -2, "FILTER_"#name);

        REGISTER_FILTER_CONSTANT(LINEAR);
        REGISTER_FILTER_CONSTANT(NEAREST);

#undef REGISTER_FILTER_CONSTANT

#define REGISTER_WRAP_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::TEXTURE_WRAP_##name); \
        lua_setfield(L, -2, "WRAP_"#name);

        REGISTER_WRAP_CONSTANT(CLAMP_TO_BORDER);
        REGISTER_WRAP_CONSTANT(CLAMP_TO_EDGE);
        REGISTER_WRAP_CONSTANT(MIRRORED_REPEAT);
        REGISTER_WRAP_CONSTANT(REPEAT);

#undef REGISTER_WRAP_CONSTANT

#define REGISTER_BLEND_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::BLEND_FACTOR_##name); \
        lua_setfield(L, -2, "BLEND_"#name);

        REGISTER_BLEND_CONSTANT(ZERO);
        REGISTER_BLEND_CONSTANT(ONE);
        REGISTER_BLEND_CONSTANT(SRC_COLOR);
        REGISTER_BLEND_CONSTANT(ONE_MINUS_SRC_COLOR);
        REGISTER_BLEND_CONSTANT(DST_COLOR);
        REGISTER_BLEND_CONSTANT(ONE_MINUS_DST_COLOR);
        REGISTER_BLEND_CONSTANT(SRC_ALPHA);
        REGISTER_BLEND_CONSTANT(ONE_MINUS_SRC_ALPHA);
        REGISTER_BLEND_CONSTANT(DST_ALPHA);
        REGISTER_BLEND_CONSTANT(ONE_MINUS_DST_ALPHA);
        REGISTER_BLEND_CONSTANT(SRC_ALPHA_SATURATE);
        REGISTER_BLEND_CONSTANT(CONSTANT_COLOR);
        REGISTER_BLEND_CONSTANT(ONE_MINUS_CONSTANT_COLOR);
        REGISTER_BLEND_CONSTANT(CONSTANT_ALPHA);
        REGISTER_BLEND_CONSTANT(ONE_MINUS_CONSTANT_ALPHA);

#undef REGISTER_BLEND_CONSTANT

#define REGISTER_COMPARE_FUNC_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::COMPARE_FUNC_##name); \
        lua_setfield(L, -2, "COMPARE_FUNC_"#name);

        REGISTER_COMPARE_FUNC_CONSTANT(NEVER);
        REGISTER_COMPARE_FUNC_CONSTANT(LESS);
        REGISTER_COMPARE_FUNC_CONSTANT(LEQUAL);
        REGISTER_COMPARE_FUNC_CONSTANT(GREATER);
        REGISTER_COMPARE_FUNC_CONSTANT(GEQUAL);
        REGISTER_COMPARE_FUNC_CONSTANT(EQUAL);
        REGISTER_COMPARE_FUNC_CONSTANT(NOTEQUAL);
        REGISTER_COMPARE_FUNC_CONSTANT(ALWAYS);

#undef REGISTER_COMPARE_FUNC_CONSTANT

#define REGISTER_STENCIL_OP_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::STENCIL_OP_##name); \
        lua_setfield(L, -2, "STENCIL_OP_"#name);

        REGISTER_STENCIL_OP_CONSTANT(KEEP);
        REGISTER_STENCIL_OP_CONSTANT(ZERO);
        REGISTER_STENCIL_OP_CONSTANT(REPLACE);
        REGISTER_STENCIL_OP_CONSTANT(INCR);
        REGISTER_STENCIL_OP_CONSTANT(INCR_WRAP);
        REGISTER_STENCIL_OP_CONSTANT(DECR);
        REGISTER_STENCIL_OP_CONSTANT(DECR_WRAP);
        REGISTER_STENCIL_OP_CONSTANT(INVERT);

#undef REGISTER_STENCIL_OP_CONSTANT

#define REGISTER_FACE_CONSTANT(name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::FACE_TYPE_##name); \
        lua_setfield(L, -2, "FACE_"#name);

        REGISTER_FACE_CONSTANT(FRONT);
        REGISTER_FACE_CONSTANT(BACK);
        REGISTER_FACE_CONSTANT(FRONT_AND_BACK);

#undef REGISTER_FACE_CONSTANT

#define REGISTER_BUFFER_CONSTANT(enum_type, name)\
        lua_pushnumber(L, (lua_Number) dmGraphics::BUFFER_TYPE_##enum_type); \
        lua_setfield(L, -2, "BUFFER_"#name);

        REGISTER_BUFFER_CONSTANT(COLOR0_BIT,  COLOR_BIT); // For backwards compatability
        REGISTER_BUFFER_CONSTANT(COLOR0_BIT,  COLOR0_BIT);
        // These depend on driver and context support
        if (dmGraphics::IsMultiTargetRenderingSupported(graphics_context))
        {
            REGISTER_BUFFER_CONSTANT(COLOR1_BIT,  COLOR1_BIT);
            REGISTER_BUFFER_CONSTANT(COLOR2_BIT,  COLOR2_BIT);
            REGISTER_BUFFER_CONSTANT(COLOR3_BIT,  COLOR3_BIT);
        }
        REGISTER_BUFFER_CONSTANT(DEPTH_BIT,   DEPTH_BIT);
        REGISTER_BUFFER_CONSTANT(STENCIL_BIT, STENCIL_BIT);

#undef REGISTER_BUFFER_CONSTANT

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
    }

    void FinalizeRenderScriptContext(RenderScriptContext& context, dmScript::HContext script_context)
    {
        context.m_LuaState = 0;
    }

    static bool LoadRenderScript(lua_State* L, dmLuaDDF::LuaSource *source, RenderScript* script)
    {
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
            script->m_FunctionReferences[i] = LUA_NOREF;

        bool result = false;
        int top = lua_gettop(L);
        (void) top;

        int ret = dmScript::LuaLoad(L, source);
        if (ret == 0)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_InstanceReference);
            dmScript::SetInstance(L);

            ret = dmScript::PCall(L, 0, 0);
            if (ret == 0)
            {
                for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
                {
                    lua_getglobal(L, RENDER_SCRIPT_FUNCTION_NAMES[i]);
                    if (lua_isnil(L, -1) == 0)
                    {
                        if (lua_type(L, -1) == LUA_TFUNCTION)
                        {
                            script->m_FunctionReferences[i] = dmScript::Ref(L, LUA_REGISTRYINDEX);
                        }
                        else
                        {
                            dmLogError("The global name '%s' in '%s' must be a function.", RENDER_SCRIPT_FUNCTION_NAMES[i], source->m_Filename);
                            lua_pop(L, 1);
                            goto bail;
                        }
                    }
                    else
                    {
                        script->m_FunctionReferences[i] = LUA_NOREF;
                        lua_pop(L, 1);
                    }
                }
                result = true;
                script->m_SourceFileName = strdup(source->m_Filename);
            }
            lua_pushnil(L);
            dmScript::SetInstance(L);
        }
        else
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
        }
bail:
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
        {
            lua_pushnil(L);
            lua_setglobal(L, RENDER_SCRIPT_FUNCTION_NAMES[i]);
        }
        assert(top == lua_gettop(L));
        return result;
    }

    static void ResetRenderScript(HRenderScript render_script) {
        memset(render_script, 0, sizeof(RenderScript));
        render_script->m_InstanceReference = LUA_NOREF;
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i) {
            render_script->m_FunctionReferences[i] = LUA_NOREF;
        }
    }

    HRenderScript NewRenderScript(HRenderContext render_context, dmLuaDDF::LuaSource *source)
    {
        lua_State* L = render_context->m_RenderScriptContext.m_LuaState;
        int top = lua_gettop(L);
        (void)top;

        HRenderScript render_script = (HRenderScript)lua_newuserdata(L, sizeof(RenderScript));
        ResetRenderScript(render_script);

        render_script->m_RenderContext = render_context;
        luaL_getmetatable(L, RENDER_SCRIPT);
        lua_setmetatable(L, -2);
        render_script->m_InstanceReference = dmScript::Ref(L, LUA_REGISTRYINDEX);
        render_script->m_SourceFileName = 0;
        if (LoadRenderScript(L, source, render_script))
        {
            assert(top == lua_gettop(L));
            return render_script;
        }
        else
        {
            DeleteRenderScript(render_context, render_script);
            assert(top == lua_gettop(L));
            return 0;
        }
    }

    bool ReloadRenderScript(HRenderContext render_context, HRenderScript render_script, dmLuaDDF::LuaSource *source)
    {
        return LoadRenderScript(render_context->m_RenderScriptContext.m_LuaState, source, render_script);
    }

    void DeleteRenderScript(HRenderContext render_context, HRenderScript render_script)
    {
        lua_State* L = render_script->m_RenderContext->m_RenderScriptContext.m_LuaState;
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (render_script->m_FunctionReferences[i])
                dmScript::Unref(L, LUA_REGISTRYINDEX, render_script->m_FunctionReferences[i]);
        }
        dmScript::Unref(L, LUA_REGISTRYINDEX, render_script->m_InstanceReference);
        free((void*)render_script->m_SourceFileName);
        render_script->~RenderScript();
        ResetRenderScript(render_script);
    }

    static void ResetRenderScriptInstance(HRenderScriptInstance render_script_instance) {
        memset(render_script_instance, 0, sizeof(RenderScriptInstance));
        render_script_instance->m_InstanceReference = LUA_NOREF;
        render_script_instance->m_RenderScriptDataReference = LUA_NOREF;
        render_script_instance->m_ContextTableReference = LUA_NOREF;
    }

    HRenderScriptInstance NewRenderScriptInstance(dmRender::HRenderContext render_context, HRenderScript render_script)
    {
        lua_State* L = render_context->m_RenderScriptContext.m_LuaState;

        int top = lua_gettop(L);
        (void) top;

        RenderScriptInstance* i = (RenderScriptInstance*)lua_newuserdata(L, sizeof(RenderScriptInstance));
        ResetRenderScriptInstance(i);
        i->m_PredicateCount = 0;
        i->m_RenderScript = render_script;
        i->m_ScriptWorld = render_context->m_ScriptWorld;
        i->m_RenderContext = render_context;
        i->m_CommandBuffer.SetCapacity(render_context->m_RenderScriptContext.m_CommandBufferSize);
        i->m_Materials.SetCapacity(16, 8);

        lua_pushvalue(L, -1);
        i->m_InstanceReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_RenderScriptDataReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_ContextTableReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

        luaL_getmetatable(L, RENDER_SCRIPT_INSTANCE);
        lua_setmetatable(L, -2);

        dmScript::SetInstance(L);
        dmScript::InitializeInstance(i->m_ScriptWorld);
        lua_pushnil(L);
        dmScript::SetInstance(L);

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance)
    {
        lua_State* L = render_script_instance->m_RenderContext->m_RenderScriptContext.m_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_rawgeti(L, LUA_REGISTRYINDEX, render_script_instance->m_InstanceReference);
        dmScript::SetInstance(L);
        dmScript::FinalizeInstance(render_script_instance->m_ScriptWorld);
        lua_pushnil(L);
        dmScript::SetInstance(L);

        dmScript::Unref(L, LUA_REGISTRYINDEX, render_script_instance->m_InstanceReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, render_script_instance->m_RenderScriptDataReference);
        dmScript::Unref(L, LUA_REGISTRYINDEX, render_script_instance->m_ContextTableReference);

        assert(top == lua_gettop(L));

        for (uint32_t i = 0; i < render_script_instance->m_PredicateCount; ++i) {
            delete render_script_instance->m_Predicates[i];
        }
        render_script_instance->~RenderScriptInstance();
        ResetRenderScriptInstance(render_script_instance);
    }

    void SetRenderScriptInstanceRenderScript(HRenderScriptInstance render_script_instance, HRenderScript render_script)
    {
        render_script_instance->m_RenderScript = render_script;
    }

    void AddRenderScriptInstanceMaterial(HRenderScriptInstance render_script_instance, const char* material_name, dmRender::HMaterial material)
    {
        if (render_script_instance->m_Materials.Full())
        {
            uint32_t new_capacity = 2 * render_script_instance->m_Materials.Capacity();
            render_script_instance->m_Materials.SetCapacity(2 * new_capacity, new_capacity);
        }
        render_script_instance->m_Materials.Put(dmHashString64(material_name), material);
    }

    void ClearRenderScriptInstanceMaterials(HRenderScriptInstance render_script_instance)
    {
        render_script_instance->m_Materials.Clear();
    }

    RenderScriptResult RunScript(HRenderScriptInstance script_instance, RenderScriptFunction script_function, void* args)
    {
        DM_PROFILE("RenderScript");

        RenderScriptResult result = RENDER_SCRIPT_RESULT_OK;
        HRenderScript script = script_instance->m_RenderScript;
        if (script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            lua_State* L = script_instance->m_RenderContext->m_RenderScriptContext.m_LuaState;
            int top = lua_gettop(L);
            (void) top;

            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
            dmScript::SetInstance(L);

            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[script_function]);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            int arg_count = 1;

            const char* message_name = 0;
            if (script_function == RENDER_SCRIPT_FUNCTION_ONMESSAGE)
            {
                arg_count = 4;

                dmMessage::Message* message = (dmMessage::Message*)args;
                dmScript::PushHash(L, message->m_Id);
                if (message->m_Descriptor != 0)
                {
                    dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
                    // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
                    // lua_cpcall?
                    message_name = descriptor->m_Name;
                    dmScript::PushDDF(L, descriptor, (const char*)message->m_Data, true);
                }
                else
                {
                    if (dmProfile::IsInitialized())
                    {
                        // Try to find the message name via id and reverse hash
                        message_name = (const char*)dmHashReverse64(message->m_Id, 0);
                    }
                    if (message->m_DataSize > 0)
                    {
                        dmScript::PushTable(L, (const char*)message->m_Data, message->m_DataSize);
                    }
                    else
                    {
                        lua_newtable(L);
                    }
                }
                dmScript::PushURL(L, message->m_Sender);
            }
            else if (script_function == RENDER_SCRIPT_FUNCTION_UPDATE)
            {
                float* dt = (float*)args;
                lua_pushnumber(L, (lua_Number) *dt);
                arg_count += 1;
            }

            {
                char buffer[128];
                const char* profiler_string = dmScript::GetProfilerString(L, 0, script->m_SourceFileName, RENDER_SCRIPT_FUNCTION_NAMES[script_function], message_name, buffer, sizeof(buffer));
                DM_PROFILE_DYN(profiler_string, 0);

                if (dmScript::PCall(L, arg_count, 0) != 0)
                {
                    assert(top == lua_gettop(L));
                    result = RENDER_SCRIPT_RESULT_FAILED;
                }
            }

            lua_pushnil(L);
            dmScript::SetInstance(L);

            assert(top == lua_gettop(L));
        }

        return result;
    }

    RenderScriptResult InitRenderScriptInstance(HRenderScriptInstance instance)
    {
        return RunScript(instance, RENDER_SCRIPT_FUNCTION_INIT, 0x0);
    }

    struct DispatchContext
    {
        HRenderScriptInstance m_Instance;
        RenderScriptResult m_Result;
    };

    void DispatchCallback(dmMessage::Message *message, void* user_ptr)
    {
        DispatchContext* context = (DispatchContext*)user_ptr;
        HRenderScriptInstance instance = context->m_Instance;
        if (message->m_Descriptor != 0)
        {
            dmDDF::Descriptor* descriptor = (dmDDF::Descriptor*)message->m_Descriptor;
            if (descriptor == dmRenderDDF::DrawText::m_DDFDescriptor || descriptor == dmRenderDDF::DrawDebugText::m_DDFDescriptor)
            {
                if (instance->m_RenderContext->m_SystemFontMap == 0)
                {
                    dmLogWarning("The text can not be rendered since the system font is not set.");
                    context->m_Result = RENDER_SCRIPT_RESULT_FAILED;
                    return;
                }

                DrawTextParams params;
                if (descriptor == dmRenderDDF::DrawText::m_DDFDescriptor)
                {
                    dmRenderDDF::DrawText* dt = (dmRenderDDF::DrawText*)message->m_Data;
                    const char* text = (const char*) ((uintptr_t) dt + (uintptr_t) dt->m_Text);
                    params.m_Text = text;
                    params.m_WorldTransform.setTranslation(dmVMath::Vector3(dt->m_Position));
                    params.m_FaceColor = dmVMath::Vector4(0.0f, 0.0f, 1.0f, 1.0f);
                }
                else
                {
                    dmRenderDDF::DrawDebugText* dt = (dmRenderDDF::DrawDebugText*)message->m_Data;
                    const char* text = (const char*) ((uintptr_t) dt + (uintptr_t) dt->m_Text);
                    params.m_Text = text;
                    params.m_WorldTransform.setTranslation(dmVMath::Vector3(dt->m_Position));
                    params.m_FaceColor = dt->m_Color;
                }
                DrawText(instance->m_RenderContext, instance->m_RenderContext->m_SystemFontMap, 0, 0, params);
                return;
            }
            else if (descriptor == dmRenderDDF::DrawLine::m_DDFDescriptor)
            {
                dmRenderDDF::DrawLine* dl = (dmRenderDDF::DrawLine*)message->m_Data;
                Line3D(instance->m_RenderContext, dl->m_StartPoint, dl->m_EndPoint, dl->m_Color, dl->m_Color);
                return;
            }
            else if (descriptor == dmRenderDDF::Resize::m_DDFDescriptor)
            {
                dmRenderDDF::Resize* resize_msg = (dmRenderDDF::Resize*)message->m_Data;
                dmGraphics::ResizeWindow(instance->m_RenderContext->m_GraphicsContext, resize_msg->m_Width, resize_msg->m_Height);
                return;
            }
        }
        context->m_Result = RunScript(instance, RENDER_SCRIPT_FUNCTION_ONMESSAGE, message);
    }

    RenderScriptResult DispatchRenderScriptInstance(HRenderScriptInstance instance)
    {
        DM_PROFILE("DispatchRSI");
        DispatchContext context;
        context.m_Instance = instance;
        context.m_Result = RENDER_SCRIPT_RESULT_OK;
        dmMessage::Dispatch(instance->m_RenderContext->m_Socket, DispatchCallback, (void*)&context);
        return context.m_Result;
    }

    RenderScriptResult UpdateRenderScriptInstance(HRenderScriptInstance instance, float dt)
    {
        DM_PROFILE("UpdateRSI");
        instance->m_CommandBuffer.SetSize(0);

        dmScript::UpdateScriptWorld(instance->m_ScriptWorld, dt);

        RenderScriptResult result = RunScript(instance, RENDER_SCRIPT_FUNCTION_UPDATE, (void*)&dt);

        if (instance->m_CommandBuffer.Size() > 0)
            ParseCommands(instance->m_RenderContext, &instance->m_CommandBuffer.Front(), instance->m_CommandBuffer.Size());
        return result;
    }

    void OnReloadRenderScriptInstance(HRenderScriptInstance render_script_instance)
    {
        RunScript(render_script_instance, RENDER_SCRIPT_FUNCTION_ONRELOAD, 0x0);
    }
}
