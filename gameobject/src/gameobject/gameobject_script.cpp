#include <assert.h>
#include <string.h>
#include <inttypes.h>

#include <ddf/ddf.h>

#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>

#include <script/script.h>

#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_private.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameObject
{
    #define SCRIPTINSTANCE "ScriptInstance"

    const char* SCRIPT_FUNCTION_NAMES[MAX_SCRIPT_FUNCTION_COUNT] =
    {
        "init",
        "final",
        "update",
        "on_message",
        "on_input",
        "on_reload"
    };

    lua_State* g_LuaState = 0;

    ScriptWorld::ScriptWorld()
    : m_Instances()
    {
        // TODO: How to configure? It should correspond to collection instance count
        m_Instances.SetCapacity(1024);
    }

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
        memset(i, 0, sizeof(*i));
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

        // Try to find value in instance data
        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
        lua_pushvalue(L, 2);
        lua_gettable(L, -2);
        return 1;
    }

    static int ScriptInstance_newindex(lua_State *L)
    {
        int top = lua_gettop(L);

        ScriptInstance* i = ScriptInstance_Check(L, 1);
        (void) i;
        assert(i);

        lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        return 0;
    }

    static const luaL_reg ScriptInstance_methods[] =
    {
        {0,0}
    };

    static const luaL_reg ScriptInstance_meta[] =
    {
        {"__gc",        ScriptInstance_gc},
        {"__tostring",  ScriptInstance_tostring},
        {"__index",     ScriptInstance_index},
        {"__newindex",  ScriptInstance_newindex},
        {0, 0}
    };

    extern dmHashTable64<const dmDDF::Descriptor*>* g_Descriptors;
    extern uint32_t g_Socket;
    extern uint32_t g_ReplySocket;
    extern uint32_t g_MessageID;

    int Script_PostTo(lua_State* L)
    {
        int top = lua_gettop(L);

        ScriptInstance* i = ScriptInstance_Check(L, 1);
        dmhash_t id = dmScript::CheckHash(L, 2);
        const char* component_id_str = luaL_checkstring(L, 3);
        const char* message_id_str = luaL_checkstring(L, 4);

        HInstance instance = dmGameObject::GetInstanceFromIdentifier(i->m_Instance->m_Collection, id);
        if (instance)
        {
            InstanceMessageParams params;
            params.m_MessageId = dmHashString64(message_id_str);
            params.m_SenderInstance = i->m_Instance;
            params.m_SenderComponent = i->m_ComponentIndex;
            params.m_ReceiverInstance = instance;

            dmhash_t component_id = dmHashString64(component_id_str);
            dmGameObject::Result result = dmGameObject::GetComponentIndex(instance, component_id, &params.m_ReceiverComponent);
            if (result == dmGameObject::RESULT_COMPONENT_NOT_FOUND)
            {
                return luaL_error(L, "The component '%s' could not be found when posting message '%s'.", component_id_str, message_id_str);
            }

            const uint32_t buffer_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
            char buffer[buffer_size];
            params.m_Buffer = buffer;

            // Passing ddf data is optional atm
            if (result == dmGameObject::RESULT_OK)
            {
                if (top >= 5)
                {
                    const dmDDF::Descriptor** desc = g_Descriptors->Get(params.m_MessageId);
                    if (desc != 0)
                    {
                        params.m_DDFDescriptor = *desc;
                        if (params.m_DDFDescriptor->m_Size > buffer_size)
                        {
                            return luaL_error(L, "The message %s is to big to be sent.", message_id_str);
                        }
                        luaL_checktype(L, -1, LUA_TTABLE);
                        lua_pushvalue(L, -1);
                        params.m_BufferSize = dmScript::CheckDDF(L, *desc, buffer, buffer_size, -1);
                        lua_pop(L, 1);
                    }
                    else
                    {
                        params.m_BufferSize = dmScript::CheckTable(L, buffer, buffer_size, -1);
                    }
                }

                result = dmGameObject::PostInstanceMessage(params);
            }
            if (result != dmGameObject::RESULT_OK)
            {
                // TODO: Translate result to string
                luaL_error(L, "Error %d when posting message '%s' to %p/%s", result, message_id_str, (void*)id, component_id_str);
            }
        }
        else
        {
            dmLogError("Error sending message. Unknown instance: %llu", id);
            return luaL_error(L, "Error sending message. Unknown instance.");
        }
        assert(top == lua_gettop(L));

        return 0;
    }

    int Script_BroadcastTo(lua_State* L)
    {
        int top = lua_gettop(L);

        ScriptInstance* i = ScriptInstance_Check(L, 1);
        dmhash_t id = dmScript::CheckHash(L, 2);
        const char* message_id_str = luaL_checkstring(L, 3);

        HInstance instance = dmGameObject::GetInstanceFromIdentifier(i->m_Instance->m_Collection, id);
        if (instance)
        {
            InstanceMessageParams params;
            params.m_MessageId = dmHashString64(message_id_str);
            params.m_SenderInstance = i->m_Instance;
            params.m_SenderComponent = i->m_ComponentIndex;
            params.m_ReceiverInstance = instance;

            const uint32_t buffer_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
            char buffer[buffer_size];
            params.m_Buffer = buffer;

            // Passing ddf data is optional atm
            if (top >= 4)
            {
                const dmDDF::Descriptor** desc = g_Descriptors->Get(params.m_MessageId);
                if (desc != 0)
                {
                    params.m_DDFDescriptor = *desc;
                    if (params.m_DDFDescriptor->m_Size > buffer_size)
                    {
                        return luaL_error(L, "The message %s is to big to be sent.", message_id_str);
                    }
                    luaL_checktype(L, -1, LUA_TTABLE);
                    lua_pushvalue(L, -1);
                    params.m_BufferSize = dmScript::CheckDDF(L, *desc, buffer, buffer_size, -1);
                    lua_pop(L, 1);
                }
                else
                {
                    params.m_BufferSize = dmScript::CheckTable(L, buffer, buffer_size, -1);
                }
            }

            dmGameObject::Result result = dmGameObject::BroadcastInstanceMessage(params);
            if (result != dmGameObject::RESULT_OK)
            {
                // TODO: Translate result to string
                return luaL_error(L, "Error %d when broadcasting message '%s' to %p", result, message_id_str, (void*)id);
            }
        }
        else
        {
            dmLogError("Error sending message. Unknown instance: %llu", id);
            return luaL_error(L, "Error sending message. Unknown instance.");
        }
        assert(top == lua_gettop(L));

        return 0;
    }

    int Script_PostToCollection(lua_State* L)
    {
        int top = lua_gettop(L);

        ScriptInstance* i = ScriptInstance_Check(L, 1);
        const char* collection_id_str = luaL_checkstring(L, 2);
        const char* instance_id_str = luaL_checkstring(L, 3);
        const char* component_id_str = luaL_checkstring(L, 4);
        const char* message_id_str = luaL_checkstring(L, 5);

        HCollection collection = i->m_Instance->m_Collection;
        HRegister regist = collection->m_Register;
        HCollection to_collection = 0;
        dmhash_t collection_id = dmHashString64(collection_id_str);
        for (uint32_t i = 0; i < regist->m_Collections.Size(); ++i)
        {
            if (regist->m_Collections[i]->m_NameHash == collection_id)
            {
                to_collection = regist->m_Collections[i];
                break;
            }
        }

        if (to_collection == 0)
        {
            return luaL_error(L, "The collection '%s' could not be found.", collection_id_str);
        }

        dmhash_t instance_id = dmHashString64(instance_id_str);
        HInstance instance = dmGameObject::GetInstanceFromIdentifier(to_collection, instance_id);
        if (instance)
        {
            InstanceMessageParams params;
            params.m_MessageId = dmHashString64(message_id_str);
            params.m_SenderInstance = i->m_Instance;
            params.m_SenderComponent = i->m_ComponentIndex;
            params.m_ReceiverInstance = instance;

            const uint32_t buffer_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
            char buffer[buffer_size];
            params.m_Buffer = buffer;

            dmhash_t component_id = dmHashString64(component_id_str);
            dmGameObject::Result result = dmGameObject::GetComponentIndex(instance, component_id, &params.m_ReceiverComponent);
            if (result == dmGameObject::RESULT_OK)
            {
                // Passing data is optional atm
                if (top >= 6)
                {
                    const dmDDF::Descriptor** desc = g_Descriptors->Get(params.m_MessageId);
                    if (desc != 0)
                    {
                        params.m_DDFDescriptor = *desc;
                        if ((*desc)->m_Size > buffer_size)
                        {
                            return luaL_error(L, "The message '%s' is too large to be posted.", message_id_str);
                        }
                        luaL_checktype(L, -1, LUA_TTABLE);

                        lua_pushvalue(L, -1);
                        params.m_BufferSize = dmScript::CheckDDF(L, *desc, buffer, buffer_size, -1);
                        lua_pop(L, 1);
                    }
                    else
                    {
                        params.m_BufferSize = dmScript::CheckTable(L, buffer, buffer_size, -1);
                    }
                }

                result = dmGameObject::PostInstanceMessage(params);
            }
            if (result != dmGameObject::RESULT_OK)
            {
                luaL_error(L, "The component '%s' could not be found.", component_id_str);
            }
        }
        else
        {
            return luaL_error(L, "The instance '%s' could not be found.", instance_id_str);
        }
        assert(top == lua_gettop(L));

        return 0;
    }

    int Script_Post(lua_State* L)
    {
        int top = lua_gettop(L);

        ScriptInstance* i = ScriptInstance_Check(L, 1);
        const char* message_id_str = luaL_checkstring(L, 2);

        char buf[INSTANCE_MESSAGE_MAX];
        InstanceMessageData* message = (InstanceMessageData*) buf;
        message->m_MessageId = dmHashString64(message_id_str);
        message->m_DDFDescriptor = 0x0;
        message->m_SenderInstance = i->m_Instance;
        message->m_SenderComponent = i->m_ComponentIndex;

        if (top > 2)
        {
            const dmDDF::Descriptor** desc = g_Descriptors->Get(message->m_MessageId);
            if (desc == 0)
            {
                return luaL_error(L, "Unknown ddf type: %s", message_id_str);
            }
            message->m_DDFDescriptor = *desc;

            uint32_t size = sizeof(InstanceMessageData) + message->m_DDFDescriptor->m_Size;
            if (size > INSTANCE_MESSAGE_MAX)
            {
                return luaL_error(L, "sizeof(%s) > %d", message_id_str, message->m_DDFDescriptor->m_Size);
            }
            char* p = buf + sizeof(InstanceMessageData);
            message->m_BufferSize = dmScript::CheckDDF(L, message->m_DDFDescriptor, p, INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData), -1);
        }

        assert(top == lua_gettop(L));

        HCollection collection = i->m_Instance->m_Collection;
        dmGameObject::HRegister reg = dmGameObject::GetRegister(collection);
        dmMessage::Post(dmGameObject::GetMessageSocket(collection), dmGameObject::GetMessageId(reg), buf, message->m_BufferSize + sizeof(InstanceMessageData));

        return 0;
    }

    int Script_GetPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        HInstance instance = i->m_Instance;
        if (lua_gettop(L) > 1)
        {
            dmhash_t id = 0;
            if (lua_isstring(L, 2))
                id = GetAbsoluteIdentifier(i->m_Instance, luaL_checkstring(L, 2));
            else
                id = dmScript::CheckHash(L, 2);
            instance = GetInstanceFromIdentifier(i->m_Instance->m_Collection, id);
        }
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetPosition(instance)));
        return 1;
    }

    int Script_GetRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        HInstance instance = i->m_Instance;
        if (lua_gettop(L) > 1)
        {
            dmhash_t id = 0;
            if (lua_isstring(L, 2))
                id = GetAbsoluteIdentifier(i->m_Instance, luaL_checkstring(L, 2));
            else
                id = dmScript::CheckHash(L, 2);
            instance = GetInstanceFromIdentifier(i->m_Instance->m_Collection, id);
        }
        dmScript::PushQuat(L, dmGameObject::GetRotation(instance));
        return 1;
    }

    int Script_SetPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        Vectormath::Aos::Vector3* v = dmScript::CheckVector3(L, 2);
        dmGameObject::SetPosition(i->m_Instance, Vectormath::Aos::Point3(*v));
        return 0;
    }

    int Script_SetRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        Vectormath::Aos::Quat* q = dmScript::CheckQuat(L, 2);
        dmGameObject::SetRotation(i->m_Instance, *q);
        return 0;
    }

    int Script_GetWorldPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        HInstance instance = i->m_Instance;
        if (lua_gettop(L) > 1)
        {
            dmhash_t id = 0;
            if (lua_isstring(L, 2))
                id = GetAbsoluteIdentifier(i->m_Instance, luaL_checkstring(L, 2));
            else
                id = dmScript::CheckHash(L, 2);
            instance = GetInstanceFromIdentifier(i->m_Instance->m_Collection, id);
        }
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetWorldPosition(instance)));
        return 1;
    }

    int Script_GetWorldRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        HInstance instance = i->m_Instance;
        if (lua_gettop(L) > 1)
        {
            dmhash_t id = 0;
            if (lua_isstring(L, 2))
                id = GetAbsoluteIdentifier(i->m_Instance, luaL_checkstring(L, 2));
            else
                id = dmScript::CheckHash(L, 2);
            instance = GetInstanceFromIdentifier(i->m_Instance->m_Collection, id);
        }
        dmScript::PushQuat(L, dmGameObject::GetWorldRotation(instance));
        return 1;
    }

    int Script_GetId(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        if (lua_gettop(L) > 1)
        {
            const char* ident = luaL_checkstring(L, 2);
            dmScript::PushHash(L, GetAbsoluteIdentifier(i->m_Instance, ident));
        }
        else
        {
            dmScript::PushHash(L, i->m_Instance->m_Identifier);
        }
        return 1;
    }

    int Script_Delete(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);

        lua_pushstring(L, "__collection__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        HCollection collection = (HCollection)lua_touserdata(L, -1);
        assert(collection);
        lua_pop(L, 1);

        dmGameObject::Delete(collection, i->m_Instance);

        return 0;
    }

    static const luaL_reg Script_methods[] =
    {
        {"post",                Script_Post},
        {"post_to",             Script_PostTo},
        {"broadcast_to",        Script_BroadcastTo},
        {"post_to_collection",  Script_PostToCollection},
        {"get_position",        Script_GetPosition},
        {"get_rotation",        Script_GetRotation},
        {"set_position",        Script_SetPosition},
        {"set_rotation",        Script_SetRotation},
        {"get_world_position",  Script_GetWorldPosition},
        {"get_world_rotation",  Script_GetWorldRotation},
        {"get_id",              Script_GetId},
        {"delete",              Script_Delete},
        {0, 0}
    };

    void InitializeScript()
    {
        lua_State *L = lua_open();
        g_LuaState = L;

        int top = lua_gettop(L);

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        // Pop all stack values generated from luaopen_*
        lua_pop(L, lua_gettop(L));

        luaL_register(L, SCRIPTINSTANCE, ScriptInstance_methods);   // create methods table, add it to the globals
        int methods = lua_gettop(L);
        luaL_newmetatable(L, SCRIPTINSTANCE);                         // create metatable for Image, add it to the Lua registry
        int metatable = lua_gettop(L);
        luaL_register(L, 0, ScriptInstance_meta);                   // fill metatable

        lua_pushliteral(L, "__metatable");
        lua_pushvalue(L, methods);                       // dup methods table
        lua_settable(L, metatable);

        lua_pop(L, 2);

        luaL_register(L, "go", Script_methods);
        lua_pop(L, 1);

        dmScript::Initialize(L);

        assert(top == lua_gettop(L));
    }

    void FinalizeScript()
    {
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

    static bool LoadScript(lua_State* L, const void* buffer, uint32_t buffer_size, const char* filename, Script* script)
    {
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
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
                for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
                {
                    lua_getglobal(L, SCRIPT_FUNCTION_NAMES[i]);
                    if (lua_isnil(L, -1) == 0)
                    {
                        if (lua_type(L, -1) == LUA_TFUNCTION)
                        {
                            script->m_FunctionReferences[i] = luaL_ref(L, LUA_REGISTRYINDEX);
                        }
                        else
                        {
                            dmLogError("The global name '%s' in '%s' must be a function.", SCRIPT_FUNCTION_NAMES[i], filename);
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
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            lua_pushnil(L);
            lua_setglobal(L, SCRIPT_FUNCTION_NAMES[i]);
        }
        assert(top == lua_gettop(L));
        return result;
    }

    HScript NewScript(const void* buffer, uint32_t buffer_size, const char* filename)
    {
        lua_State* L = g_LuaState;

        Script temp_script;
        if (LoadScript(L, buffer, buffer_size, filename, &temp_script))
        {
            HScript script = new Script();
            *script = temp_script;
            return script;
        }
        else
        {
            return 0;
        }
    }

    bool ReloadScript(HScript script, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        return LoadScript(g_LuaState, buffer, buffer_size, filename, script);
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = g_LuaState;
        for (uint32_t i = 0; i < MAX_SCRIPT_FUNCTION_COUNT; ++i)
        {
            if (script->m_FunctionReferences[i])
                luaL_unref(L, LUA_REGISTRYINDEX, script->m_FunctionReferences[i]);
        }
        delete script;
    }

    HScriptInstance NewScriptInstance(HScript script, HInstance instance, uint8_t component_index)
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
        i->m_ComponentIndex = component_index;
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

        luaL_unref(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        luaL_unref(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        assert(top == lua_gettop(L));
    }
}
