#include "render_script.h"

#include <string.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/profile.h>

#include <script/script.h>
#include <gameobject/gameobject.h>
#include <gamesys/gamesys_ddf.h>

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmEngine
{
    #define RENDER_SCRIPT_SOCKET_NAME "render"
    #define RENDER_SCRIPT_INSTANCE "RenderScriptInstance"

    #define RENDER_SCRIPT_LIB_NAME "render"
    #define RENDER_SCRIPT_COLOR_NAME "color"
    #define RENDER_SCRIPT_DEPTH_NAME "depth"
    #define RENDER_SCRIPT_STENCIL_NAME "stencil"

    const char* RENDER_SCRIPT_FUNCTION_NAMES[MAX_RENDER_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "update",
        "on_message"
    };

    lua_State* g_LuaState = 0;
    uint32_t g_Socket = 0;

    RenderScriptWorld::RenderScriptWorld()
    : m_Instances()
    {
        m_Instances.SetCapacity(512);
    }

    static RenderScriptInstance* RenderScriptInstance_Check(lua_State *L, int index)
    {
        RenderScriptInstance* i;
        luaL_checktype(L, index, LUA_TUSERDATA);
        i = (RenderScriptInstance*)luaL_checkudata(L, index, RENDER_SCRIPT_INSTANCE);
        if (i == NULL) luaL_typerror(L, index, RENDER_SCRIPT_INSTANCE);
        return i;
    }

    static int RenderScriptInstance_gc (lua_State *L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        memset(i, 0, sizeof(*i));
        (void) i;
        assert(i);
        return 0;
    }

    static int RenderScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "GameObject: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int RenderScriptInstance_index(lua_State *L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void) i;
        assert(i);

        // Try to find value in instance data
        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_RenderScriptDataReference);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        return 1;
    }

    static int RenderScriptInstance_newindex(lua_State *L)
    {
        int top = lua_gettop(L);

        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void) i;
        assert(i);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_RenderScriptDataReference);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return 0;
    }

    static const luaL_reg RenderScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg RenderScriptInstance_meta[] =
    {
        {"__gc",        RenderScriptInstance_gc},
        {"__tostring",  RenderScriptInstance_tostring},
        {"__index",     RenderScriptInstance_index},
        {"__newindex",  RenderScriptInstance_newindex},
        {0, 0}
    };

    int RenderScript_SetViewport(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        uint32_t width = luaL_checknumber(L, 2);
        uint32_t height = luaL_checknumber(L, 3);

        dmGraphics::SetViewport(dmRender::GetGraphicsContext(i->m_RenderContext), width, height);

        return 0;
    }

    int RenderScript_Clear(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);

        int top = lua_gettop(L);

        uint32_t flags = 0;

        Vectormath::Aos::Vector4 color(0.0f, 0.0f, 0.0f, 0.0f);
        float depth = 0.0f;
        uint32_t stencil = 0;

        lua_pushnil(L);
        while (lua_next(L, 2))
        {
            const char* key = luaL_checkstring(L, -2);
            if (strncmp(key, RENDER_SCRIPT_COLOR_NAME, strlen(RENDER_SCRIPT_COLOR_NAME)) == 0)
            {
                flags |= dmGraphics::CLEAR_COLOUR_BUFFER;
                color = *dmScript::CheckVector4(L, -1);
            }
            else if (strncmp(key, RENDER_SCRIPT_DEPTH_NAME, strlen(RENDER_SCRIPT_DEPTH_NAME)) == 0)
            {
                flags |= dmGraphics::CLEAR_DEPTH_BUFFER;
                depth = (float)luaL_checknumber(L, -1);
            }
            else if (strncmp(key, RENDER_SCRIPT_STENCIL_NAME, strlen(RENDER_SCRIPT_STENCIL_NAME)) == 0)
            {
                flags |= dmGraphics::CLEAR_STENCIL_BUFFER;
                stencil = (uint32_t)luaL_checknumber(L, -1);
            }
            else
            {
                lua_pop(L, 2);
                assert(top == lua_gettop(L));
                return luaL_error(L, "Unknown key supplied to %s.clear: %s. Available keys are %s, %s and %s.", RENDER_SCRIPT_LIB_NAME, key, RENDER_SCRIPT_COLOR_NAME, RENDER_SCRIPT_DEPTH_NAME, RENDER_SCRIPT_STENCIL_NAME);
            }
            lua_pop(L, 1);
        }

        dmGraphics::Clear(dmRender::GetGraphicsContext(i->m_RenderContext), flags, (uint8_t)(color.getX() * 255.0f), (uint8_t)(color.getY() * 255.0f), (uint8_t)(color.getZ() * 255.0f), (uint8_t)(color.getW() * 255.0f), depth, stencil);

        assert(top == lua_gettop(L));

        return 0;
    }

    int RenderScript_Draw(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void)i;
        dmRender::Predicate* predicate = 0x0;
//        if (lua_islightuserdata(L, 2))
//        {
//            predicate = (Predicate*)lua_touserdata(L, 2);
//        }
        dmRender::Draw(i->m_RenderContext, predicate);
        return 1;
    }

    int RenderScript_SetView(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        Vectormath::Aos::Matrix4 view = *dmScript::CheckMatrix4(L, 2);
        dmRender::SetViewMatrix(i->m_RenderContext, view);
        return 0;
    }

    int RenderScript_SetProjection(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        Vectormath::Aos::Matrix4 projection = *dmScript::CheckMatrix4(L, 2);
        dmRender::SetProjectionMatrix(i->m_RenderContext, projection);
        return 0;
    }

    int RenderScript_GetWindowWidth(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWindowWidth());
        return 1;
    }

    int RenderScript_GetWindowHeight(lua_State* L)
    {
        RenderScriptInstance* i = RenderScriptInstance_Check(L, 1);
        (void)i;
        lua_pushnumber(L, dmGraphics::GetWindowHeight());
        return 1;
    }

    static const luaL_reg RenderScript_methods[] =
    {
        {"set_viewport",        RenderScript_SetViewport},
        {"clear",               RenderScript_Clear},
        {"draw",                RenderScript_Draw},
        {"set_view",            RenderScript_SetView},
        {"set_projection",      RenderScript_SetProjection},
        {"get_window_width",    RenderScript_GetWindowWidth},
        {"get_window_height",   RenderScript_GetWindowHeight},
        {0, 0}
    };

    void InitializeRenderScript()
    {
        g_Socket = dmHashString32(RENDER_SCRIPT_SOCKET_NAME);
        dmMessage::CreateSocket(g_Socket, 4 * 1024);

        lua_State *L = lua_open();
        g_LuaState = L;

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        int top = lua_gettop(L);

        luaL_register(L, RENDER_SCRIPT_INSTANCE, RenderScriptInstance_methods);   // create methods table, add it to the globals
        int methods = lua_gettop(L);
        luaL_newmetatable(L, RENDER_SCRIPT_INSTANCE);
        int metatable = lua_gettop(L);
        luaL_register(L, 0, RenderScriptInstance_meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, methods);                       // dup methods table
        lua_settable(L, metatable);

        lua_pop(L, 2);

        luaL_register(L, RENDER_SCRIPT_LIB_NAME, RenderScript_methods);
        lua_pop(L, 1);

        dmScript::Initialize(L);

        assert(top == lua_gettop(L));
    }

    void FinalizeRenderScript()
    {
        dmMessage::DestroySocket(g_Socket);
        if (g_LuaState)
            lua_close(g_LuaState);
        g_LuaState = 0;
    }

    struct LuaData
    {
        const char* m_Buffer;
        uint32_t m_Size;
    };

    const char* ReadScript(lua_State *L, void *data, size_t *size)
    {
        LuaData* lua_data = (LuaData*)data;
        if (lua_data->m_Size == 0)
        {
            return 0x0;
        }
        else
        {
            *size = lua_data->m_Size;
            lua_data->m_Size = 0;
            return lua_data->m_Buffer;
        }
    }

    static bool LoadRenderScript(lua_State* L, const void* buffer, uint32_t buffer_size, const char* filename, RenderScript* script)
    {
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
            script->m_FunctionReferences[i] = LUA_NOREF;

        bool result = false;
        int top = lua_gettop(L);
        (void) top;

        LuaData data;
        data.m_Buffer = (const char*)buffer;
        data.m_Size = buffer_size;
        int ret = lua_load(L, &ReadScript, &data, filename);
        if (ret == 0)
        {
            ret = lua_pcall(L, 0, LUA_MULTRET, 0);
            if (ret == 0)
            {
                for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
                {
                    lua_getglobal(L, RENDER_SCRIPT_FUNCTION_NAMES[i]);
                    if (lua_isnil(L, -1) == 0)
                    {
                        if (lua_type(L, -1) == LUA_TFUNCTION)
                        {
                            script->m_FunctionReferences[i] = luaL_ref(L, LUA_REGISTRYINDEX);
                        }
                        else
                        {
                            dmLogError("The global name '%s' in '%s' must be a function.", RENDER_SCRIPT_FUNCTION_NAMES[i], filename);
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
            }
            else
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
            }
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

    HRenderScript NewRenderScript(const void* buffer, uint32_t buffer_size, const char* filename)
    {
        lua_State* L = g_LuaState;

        RenderScript temp_render_script;
        if (LoadRenderScript(L, buffer, buffer_size, filename, &temp_render_script))
        {
            HRenderScript render_script = new RenderScript();
            *render_script = temp_render_script;
            return render_script;
        }
        else
        {
            return 0;
        }
    }

    bool ReloadRenderScript(HRenderScript render_script, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        return LoadRenderScript(g_LuaState, buffer, buffer_size, filename, render_script);
    }

    void DeleteRenderScript(HRenderScript render_script)
    {
        lua_State* L = g_LuaState;
        for (uint32_t i = 0; i < MAX_RENDER_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (render_script->m_FunctionReferences[i])
                luaL_unref(L, LUA_REGISTRYINDEX, render_script->m_FunctionReferences[i]);
        }
        delete render_script;
    }

    HRenderScriptInstance NewRenderScriptInstance(HRenderScript render_script, dmRender::HRenderContext render_context)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__instances__");

        RenderScriptInstance* i = (RenderScriptInstance *)lua_newuserdata(L, sizeof(RenderScriptInstance));
        i->m_RenderScript = render_script;
        i->m_RenderContext = render_context;

        lua_pushvalue(L, -1);
        i->m_InstanceReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_RenderScriptDataReference = luaL_ref( L, LUA_REGISTRYINDEX );

        luaL_getmetatable(L, RENDER_SCRIPT_INSTANCE);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteRenderScriptInstance(HRenderScriptInstance render_script_instance)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        luaL_unref(L, LUA_REGISTRYINDEX, render_script_instance->m_InstanceReference);
        luaL_unref(L, LUA_REGISTRYINDEX, render_script_instance->m_RenderScriptDataReference);

        assert(top == lua_gettop(L));
    }

    RenderScriptResult RunScript(HRenderScriptInstance script_instance, RenderScriptFunction script_function, void* args)
    {
        DM_PROFILE(RenderScript, "RunScript");

        RenderScriptResult result = RENDER_SCRIPT_RESULT_OK;
        HRenderScript script = script_instance->m_RenderScript;
        if (script->m_FunctionReferences[script_function] != LUA_NOREF)
        {
            lua_State* L = g_LuaState;
            int top = lua_gettop(L);
            (void) top;

            lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[script_function]);
            lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

            int arg_count = 1;

            if (script_function == RENDER_SCRIPT_FUNCTION_ONMESSAGE)
            {
                arg_count = 3;

                dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*)args;
                dmScript::PushHash(L, instance_message_data->m_MessageId);
                if (instance_message_data->m_DDFDescriptor)
                {
                    // adjust char ptrs to global mem space
                    char* data = (char*)instance_message_data->m_Buffer;
                    for (uint8_t i = 0; i < instance_message_data->m_DDFDescriptor->m_FieldCount; ++i)
                    {
                        dmDDF::FieldDescriptor* field = &instance_message_data->m_DDFDescriptor->m_Fields[i];
                        uint32_t field_type = field->m_Type;
                        if (field_type == dmDDF::TYPE_STRING)
                        {
                            *((uintptr_t*)&data[field->m_Offset]) = (uintptr_t)data + *((uintptr_t*)(data + field->m_Offset));
                        }
                    }
                    // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
                    // lua_cpcall?
                    dmScript::PushDDF(L, instance_message_data->m_DDFDescriptor, (const char*) instance_message_data->m_Buffer);
                }
                else
                {
                    if (instance_message_data->m_BufferSize > 0)
                        dmScript::PushTable(L, (const char*)instance_message_data->m_Buffer);
                    else
                        lua_newtable(L);
                }
            }
            int ret = lua_pcall(L, arg_count, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script: %s", lua_tostring(L,-1));
                lua_pop(L, 1);
                result = RENDER_SCRIPT_RESULT_FAILED;
            }

            assert(top == lua_gettop(L));
        }

        return result;
    }

    RenderScriptResult InitRenderScriptInstance(HRenderScriptInstance instance)
    {
        return RunScript(instance, RENDER_SCRIPT_FUNCTION_INIT, 0x0);
    }

    static void Dispatch(dmMessage::Message *message, void* user_ptr)
    {
        dmGameObject::InstanceMessageData* instance_message_data = (dmGameObject::InstanceMessageData*) message->m_Data;
        HRenderScriptInstance script_instance = (HRenderScriptInstance)user_ptr;
        RunScript(script_instance, RENDER_SCRIPT_FUNCTION_ONMESSAGE, (void*)instance_message_data);
    }

    RenderScriptResult UpdateRenderScriptInstance(HRenderScriptInstance instance)
    {
        dmMessage::Dispatch(g_Socket, &Dispatch, (void*)instance);
        return RunScript(instance, RENDER_SCRIPT_FUNCTION_UPDATE, 0x0);
    }
}
