#include <assert.h>
#include <string.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/event.h>
#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_common.h"
#include "luasupport.h"

extern "C"
{
#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"
}

namespace dmGameObject
{
    #define SCRIPTINSTANCE "ScriptInstance"

    lua_State* g_LuaState = 0;

    static ScriptInstance* ScriptInstance_Check(lua_State *L, int index)
    {
        ScriptInstance* i;
        luaL_checktype(L, index, LUA_TUSERDATA);
        i = (ScriptInstance*)luaL_checkudata(L, index, SCRIPTINSTANCE);
        if (i == NULL) luaL_typerror(L, index, SCRIPTINSTANCE);
        return i;
    }

    static int ScriptInstance_gc (lua_State *L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        (void) i;
        assert(i);
        return 0;
    }

    static int ScriptInstance_tostring (lua_State *L)
    {
        lua_pushfstring(L, "GameObject: %p", lua_touserdata(L, 1));
        return 1;
    }

    static int ScriptInstance_index(lua_State *L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        (void) i;
        assert(i);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "Position") == 0)
        {
            lua_newtable(L);

            Point3& pos = i->m_Instance->m_Position;

            lua_pushliteral(L, "X");
            lua_pushnumber(L, pos.getX());
            lua_rawset(L, -3);

            lua_pushliteral(L, "Y");
            lua_pushnumber(L, pos.getY());
            lua_rawset(L, -3);

            lua_pushliteral(L, "Z");
            lua_pushnumber(L, pos.getZ());
            lua_rawset(L, -3);
            return 1;
        }

        // Try to find value in globals in update context
        lua_pushstring(L, "__update_context__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        UpdateContext* update_context = (UpdateContext*) lua_touserdata(L, -1);
        lua_pop(L, 1);

        if (update_context)
        {
            if (update_context->m_GlobalData != 0)
            {
                assert(update_context->m_DDFGlobalDataDescriptor);

                const dmDDF::Descriptor* d = update_context->m_DDFGlobalDataDescriptor;
                for (uint32_t i = 0; i < d->m_FieldCount; ++i)
                {
                    const dmDDF::FieldDescriptor* f = &d->m_Fields[i];
                    if (strcmp(f->m_Name, key) == 0)
                    {
                        DDFToLuaValue(L, f, (char*) update_context->m_GlobalData);
                        return 1;
                    }
                }
            }
        }

        // Try to find value in instance data
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
            lua_pushvalue(L, 2);
            lua_gettable(L, -2);
            return 1;
        }
    }

    static int ScriptInstance_newindex(lua_State *L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        (void) i;
        assert(i);

        const char* key = luaL_checkstring(L, 2);
        if (strcmp(key, "Position") == 0)
        {
            luaL_checktype(L, 3, LUA_TTABLE);
            lua_pushliteral(L, "X");
            lua_rawget(L, 3);
            lua_pushliteral(L, "Y");
            lua_rawget(L, 3);
            lua_pushliteral(L, "Z");
            lua_rawget(L, 3);

            float x = luaL_checknumber(L, -3);
            float y = luaL_checknumber(L, -2);
            float z = luaL_checknumber(L, -1);

            i->m_Instance->m_Position = Point3(x, y, z);
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_settable(L, -3);
        }

        return 1;
    }

    static const luaL_reg ScriptInstance_methods[] =
    {
        //{"test", ScriptInstance_Test},
        {0,0}
    };

    static const luaL_reg ScriptInstance_meta[] =
    {
        {"__gc",       ScriptInstance_gc},
        {"__tostring", ScriptInstance_tostring},
        {0, 0}
    };

    extern dmHashTable64<const dmDDF::Descriptor*>* g_Descriptors;
    extern uint32_t g_ScriptSocket;
    extern uint32_t g_ScriptReplySocket;
    extern uint32_t g_ScriptEventID;

    static void PullDDFTable(lua_State* L, const dmDDF::Descriptor* d,
                             char* message, char** buffer, char** buffer_last);

    static void PullDDFValue(lua_State* L, const dmDDF::FieldDescriptor* f,
                             char* message, char** buffer, char** buffer_last)
    {
            switch (f->m_Type)
            {
                case dmDDF::TYPE_INT32:
                {
                    *((int32_t *) &message[f->m_Offset]) = (int32_t) luaL_checkinteger(L, -1);
                }
                break;

                case dmDDF::TYPE_UINT32:
                {
                    *((uint32_t *) &message[f->m_Offset]) = (uint32_t) luaL_checkinteger(L, -1);
                }
                break;

                case dmDDF::TYPE_FLOAT:
                {
                    *((float *) &message[f->m_Offset]) = (float) luaL_checknumber(L, -1);
                }
                break;

                case dmDDF::TYPE_STRING:
                {
                    const char* s = luaL_checkstring(L, -1);
                    int size = strlen(s) + 1;
                    if (*buffer + size > *buffer_last)
                    {
                        luaL_error(L, "Event data doesn't fit (payload max: %d)", SCRIPT_EVENT_MAX);
                    }
                    else
                    {
                        memcpy(*buffer, s, size);
                        // NOTE: We store offset here an relocate later...
                        *((const char**) &message[f->m_Offset]) = (const char*) (*buffer - message);
                    }
                    *buffer += size;
                }
                break;

                case dmDDF::TYPE_MESSAGE:
                {
                    const dmDDF::Descriptor* d = f->m_MessageDescriptor;
                    PullDDFTable(L, d, &message[f->m_Offset], buffer, buffer_last);
                }
                break;

                default:
                {
                    luaL_error(L, "Unsupported type %d in field %s", f->m_Type, f->m_Name);
                }
                break;
            }
    }

    static void PullDDFTable(lua_State* L, const dmDDF::Descriptor* d,
                             char* message, char** buffer, char** buffer_last)
    {
        luaL_checktype(L, -1, LUA_TTABLE);

        for (uint32_t i = 0; i < d->m_FieldCount; ++i)
        {
            const dmDDF::FieldDescriptor* f = &d->m_Fields[i];

            lua_pushstring(L, f->m_Name);
            lua_rawget(L, -2);
            if (lua_isnil(L, -1))
            {
                luaL_error(L, "Field %s not specified in table", f->m_Name);
            }
            PullDDFValue(L, f, message, buffer, buffer_last);
            lua_pop(L, 1);
        }
    }

    int Script_Post(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* type_name = luaL_checkstring(L, 1);
        luaL_checktype(L, 2, LUA_TTABLE);

        uint64_t h = dmHashBuffer64(type_name, strlen(type_name));
        const dmDDF::Descriptor** d_tmp = g_Descriptors->Get(h);
        if (d_tmp == 0)
        {
            luaL_error(L, "Unknown ddf type: %s", type_name);
        }
        const dmDDF::Descriptor* d = *d_tmp;

        uint32_t size = sizeof(ScriptEventData) + d->m_Size;
        if (size > SCRIPT_EVENT_MAX)
        {
            luaL_error(L, "sizeof(%s) > %d", type_name, d->m_Size);
        }

        char buf[SCRIPT_EVENT_MAX];
        ScriptEventData* script_event_data = (ScriptEventData*) buf;
        script_event_data->m_DDFDescriptor = d;

        lua_pushstring(L, "__scriptinstance__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        script_event_data->m_ScriptInstance = (HScriptInstance) lua_touserdata(L, -1);
        assert(script_event_data->m_ScriptInstance);
        lua_pop(L, 1);

        char* p = buf + sizeof(ScriptEventData);
        lua_pushvalue(L, 2);
        LuaTableToDDF(L, d, p, SCRIPT_EVENT_MAX - sizeof(ScriptEventData));
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        dmEvent::Post(g_ScriptSocket, g_ScriptEventID, buf);

        return 0;
    }

    void InitializeScript()
    {
        lua_State *L = lua_open();
        g_LuaState = L;

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        luaL_openlib(L, SCRIPTINSTANCE, ScriptInstance_methods, 0);   // create methods table, add it to the globals
        luaL_newmetatable(L, SCRIPTINSTANCE);                         // create metatable for Image, add it to the Lua registry
        luaL_openlib(L, 0, ScriptInstance_meta, 0);                   // fill metatable

        lua_pushliteral(L, "__index");
        lua_pushcfunction(L, ScriptInstance_index);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__newindex");
        lua_pushcfunction(L, ScriptInstance_newindex);
        lua_rawset(L, -3);

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, -3);                       // dup methods table
        lua_rawset(L, -3);                          // hide metatable: metatable.__metatable = methods
        lua_pop(L, 1);                              // drop metatable

        lua_pushliteral(L, "Post");
        lua_pushcfunction(L, Script_Post);
        lua_rawset(L, LUA_GLOBALSINDEX);
    }

    void FinalizeScript()
    {
        lua_close(g_LuaState);
    }

    HScript NewScript(const void* memory)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        int functions;
        Script* script;

        int ret = luaL_loadstring(L, (const char*) memory);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            goto bail;
        }

        ret = lua_pcall(L, 0, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 1);
            goto bail;
        }

        lua_getglobal(L, "functions");
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            dmLogError("No function table found in script");
            lua_pop(L, -1);
            goto bail;
        }

        functions = luaL_ref( L, LUA_REGISTRYINDEX );
        assert(top == lua_gettop(L));

        script = new Script();
        script->m_FunctionsReference = functions;

        return script;
bail:
    assert(top == lua_gettop(L));
        return 0;
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = g_LuaState;
        lua_pushnil(L);
        lua_rawseti(L, LUA_REGISTRYINDEX, script->m_FunctionsReference);
        delete script;
    }

    HScriptInstance NewScriptInstance(HScript script, HInstance instance)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_getglobal(L, "__instances__");

        ScriptInstance* i = (ScriptInstance *)lua_newuserdata(L, sizeof(ScriptInstance));
        i->m_Script = script;

        lua_pushvalue(L, -1);
        i->m_InstanceReference = luaL_ref( L, LUA_REGISTRYINDEX );

        lua_newtable(L);
        i->m_ScriptDataReference = luaL_ref( L, LUA_REGISTRYINDEX );

        i->m_Instance = instance;
        luaL_getmetatable(L, SCRIPTINSTANCE);
        lua_setmetatable(L, -2);

        lua_pop(L, 1);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return i;
    }

    void DeleteScriptInstance(HScriptInstance script_instance)
    {
        lua_State* L = g_LuaState;

        int top = lua_gettop(L);
        (void) top;

        lua_pushnil(L);
        lua_rawseti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

        assert(top == lua_gettop(L));
    }

    bool RunScript(HScript script, const char* function_name, HScriptInstance script_instance, const UpdateContext* update_context)
    {
        lua_State* L = g_LuaState;
        int top = lua_gettop(L);
        (void) top;
        int ret;

        lua_pushliteral(L, "__update_context__");
        lua_pushlightuserdata(L, (void*) update_context);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "__scriptinstance__");
        lua_pushlightuserdata(L, (void*) script_instance);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script->m_FunctionsReference);

        lua_getfield(L, -1, function_name);
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            dmLogError("No '%s' function found", function_name);
            lua_pop(L, 2);
            goto bail;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        ret = lua_pcall(L, 1, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 2);
            goto bail;
        }

        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
        return true;
bail:
        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);
        assert(top == lua_gettop(L));
        return false;
    }

    struct DispatchScriptEventsContext
    {
        const UpdateContext* m_UpdateContext;
        bool m_Success;
    };

    static void DispatchScriptEventsFunction(dmEvent::Event *event_object, void* user_ptr)
    {
        DispatchScriptEventsContext* context = (DispatchScriptEventsContext*) user_ptr;
        const UpdateContext* update_context = context->m_UpdateContext;

        dmGameObject::ScriptEventData* script_event_data = (dmGameObject::ScriptEventData*) event_object->m_Data;

        assert(script_event_data->m_DDFDescriptor);
        assert(script_event_data->m_DDFData);
        assert(script_event_data->m_ScriptInstance);

        lua_State* L = g_LuaState;
        int top = lua_gettop(L);
        (void) top;
        int ret;

        lua_pushliteral(L, "__update_context__");
        lua_pushlightuserdata(L, (void*) update_context);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_event_data->m_ScriptInstance->m_Script->m_FunctionsReference);
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            dmLogError("Invalid 'SenderFunctionsReference' (%d) in event", script_event_data->m_ScriptInstance->m_Script->m_FunctionsReference);
            return;
        }

        const char* function_name = "OnEvent";
        lua_getfield(L, -1, function_name);
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            dmLogError("No '%s' function found", function_name);
            lua_pop(L, 2);
            goto bail;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_event_data->m_ScriptInstance->m_InstanceReference);
        // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
        // lua_cpcall?
        lua_pushstring(L, script_event_data->m_DDFDescriptor->m_Name);
        DDFToLuaTable(L, script_event_data->m_DDFDescriptor, (const char*) script_event_data->m_DDFData);

        ret = lua_pcall(L, 3, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 2);
            goto bail;
        }

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
        return;
bail:
        context->m_Success = false;
        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);
        assert(top == lua_gettop(L));
        return;
    }

    bool DispatchScriptEvents(const UpdateContext* update_context)
    {
        DispatchScriptEventsContext ctx;
        ctx.m_UpdateContext = update_context;
        ctx.m_Success = true;
        (void) dmEvent::Dispatch(g_ScriptReplySocket, &DispatchScriptEventsFunction, (void*) &ctx);

        return ctx.m_Success;
    }
}
