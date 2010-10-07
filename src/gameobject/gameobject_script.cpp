#include <assert.h>
#include <string.h>

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
            "update",
            "on_message",
            "on_input"
    };

    lua_State* g_LuaState = 0;

    ScriptWorld::ScriptWorld()
    : m_Instances()
    {
        m_Instances.SetCapacity(512);
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

    void SetScriptIntProperty(HInstance instance, const char* key, int32_t value)
    {
        if (!instance->m_ScriptInstancePOOOOP)
            return;

        lua_State*L = g_LuaState;

        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, instance->m_ScriptInstancePOOOOP->m_ScriptDataReference);
        lua_pushstring(L, key);
        lua_pushinteger(L, value);
        lua_settable(L, -3);

        assert(top + 1 == lua_gettop(L));
    }

    void SetScriptFloatProperty(HInstance instance, const char* key, float value)
    {
        if (!instance->m_ScriptInstancePOOOOP)
            return;

        lua_State*L = g_LuaState;

        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, instance->m_ScriptInstancePOOOOP->m_ScriptDataReference);
        lua_pushstring(L, key);
        lua_pushnumber(L, value);
        lua_settable(L, -3);

        assert(top + 1 == lua_gettop(L));
    }

    void SetScriptStringProperty(HInstance instance, const char* key, const char* value)
    {
        if (!instance->m_ScriptInstancePOOOOP)
            return;

        lua_State*L = g_LuaState;

        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, instance->m_ScriptInstancePOOOOP->m_ScriptDataReference);
        lua_pushstring(L, key);
        lua_pushstring(L, value);
        lua_settable(L, -3);

        assert(top + 1 == lua_gettop(L));
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

        uint32_t id = dmScript::CheckHash(L, 1);
        const char* component_name = luaL_checkstring(L, 2);
        const char* message_name = luaL_checkstring(L, 3);

        lua_pushstring(L, "__collection__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        HCollection collection = (HCollection) lua_touserdata(L, -1);
        assert(collection);
        lua_pop(L, 1);

        HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, id);
        if (instance)
        {
            const dmDDF::Descriptor* desc = 0x0;
            const uint32_t buffer_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
            char buffer[buffer_size];
            dmGameObject::Result r;
            uint32_t actual_buffer_size = 0;

            // Passing ddf data is optional atm
            if (top >= 4)
            {
                const char* type_name = message_name;
                uint64_t h = dmHashBuffer64(type_name, strlen(type_name));
                const dmDDF::Descriptor** desc_tmp = g_Descriptors->Get(h);
                if (desc_tmp != 0)
                {
                    desc = *desc_tmp;
                    if (desc->m_Size > buffer_size)
                    {
                        luaL_error(L, "sizeof(%s) > %d", type_name, buffer_size);
                        return 0;
                    }
                    luaL_checktype(L, 4, LUA_TTABLE);

                    lua_pushvalue(L, 4);
                    dmScript::CheckDDF(L, desc, buffer, buffer_size, -1);
                    lua_pop(L, 1);
                    r = dmGameObject::PostDDFMessageTo(instance, component_name, desc, buffer);
                }
                else
                {
                    actual_buffer_size = dmScript::CheckTable(L, buffer, sizeof(buffer), 4);
                }
            }

            if (desc == 0x0)
                r = dmGameObject::PostNamedMessageTo(instance, component_name, dmHashString32(message_name), buffer, actual_buffer_size);
            if (r != dmGameObject::RESULT_OK)
            {
                // TODO: Translate r to string
                luaL_error(L, "Error %d when sending message '%s' to %p/%s", r, message_name, (void*)id, component_name);
            }
        }
        else
        {
            luaL_error(L, "Error sending message. Unknown instance: %p", (void*)id);
        }
        assert(top == lua_gettop(L));

        return 0;
    }

    int Script_PostToCollection(lua_State* L)
    {
        int top = lua_gettop(L);

        uint32_t collection_name_hash = dmScript::CheckHash(L, 1);
        uint32_t id = dmScript::CheckHash(L, 2);
        const char* component_name = luaL_checkstring(L, 3);
        const char* message_name = luaL_checkstring(L, 4);

        lua_pushstring(L, "__collection__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        HCollection collection = (HCollection) lua_touserdata(L, -1);
        assert(collection);
        lua_pop(L, 1);

        HRegister regist = collection->m_Register;
        HCollection to_collection = 0;
        for (uint32_t i = 0; i < regist->m_Collections.Size(); ++i)
        {
            if (regist->m_Collections[i]->m_NameHash == collection_name_hash)
            {
                to_collection = regist->m_Collections[i];
                break;
            }
        }

        if (to_collection == 0)
        {
            luaL_error(L, "Collection %d not found", collection_name_hash);
        }

        HInstance instance = dmGameObject::GetInstanceFromIdentifier(to_collection, id);
        if (instance)
        {
            const dmDDF::Descriptor* desc = 0x0;
            const uint32_t buffer_size = INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData);
            char buffer[buffer_size];
            uint32_t actual_buffer_size = 0;

            dmGameObject::Result r;
            // Passing data is optional atm
            if (top >= 5)
            {
                const char* type_name = message_name;
                uint64_t h = dmHashBuffer64(type_name, strlen(type_name));
                const dmDDF::Descriptor** desc_tmp = g_Descriptors->Get(h);
                if (desc_tmp != 0)
                {
                    desc = *desc_tmp;
                    if (desc->m_Size > buffer_size)
                    {
                        luaL_error(L, "sizeof(%s) > %d", type_name, buffer_size);
                        return 0;
                    }
                    luaL_checktype(L, 5, LUA_TTABLE);

                    lua_pushvalue(L, 5);
                    dmScript::CheckDDF(L, desc, buffer, buffer_size, -1);
                    lua_pop(L, 1);
                    r = dmGameObject::PostDDFMessageTo(instance, component_name, desc, buffer);
                }
                else
                {
                    actual_buffer_size = dmScript::CheckTable(L, buffer, buffer_size, 5);
                }
            }

            if (desc == 0x0)
                r = dmGameObject::PostNamedMessageTo(instance, component_name, dmHashString32(message_name), buffer, actual_buffer_size);
            if (r != dmGameObject::RESULT_OK)
            {
                // TODO: Translate r to string
                luaL_error(L, "Error sending message '%s' to %p/%s", message_name, (void*)id, component_name);
            }
        }
        else
        {
            luaL_error(L, "Error sending message. Unknown instance: %p", (void*)id);
        }
        assert(top == lua_gettop(L));

        return 0;
    }

    int Script_Post(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* message_name = luaL_checkstring(L, 1);

        char buf[INSTANCE_MESSAGE_MAX];
        InstanceMessageData* instance_message_data = (InstanceMessageData*) buf;
        instance_message_data->m_MessageId = dmHashString32(message_name);
        instance_message_data->m_DDFDescriptor = 0x0;

        if (top > 1)
        {
            const dmDDF::Descriptor** desc = g_Descriptors->Get(dmHashString64(message_name));
            if (desc == 0)
            {
                luaL_error(L, "Unknown ddf type: %s", message_name);
            }
            instance_message_data->m_DDFDescriptor = *desc;

            uint32_t size = sizeof(InstanceMessageData) + instance_message_data->m_DDFDescriptor->m_Size;
            if (size > INSTANCE_MESSAGE_MAX)
            {
                luaL_error(L, "sizeof(%s) > %d", message_name, instance_message_data->m_DDFDescriptor->m_Size);
            }
            char* p = buf + sizeof(InstanceMessageData);
            dmScript::CheckDDF(L, instance_message_data->m_DDFDescriptor, p, INSTANCE_MESSAGE_MAX - sizeof(InstanceMessageData), -1);
        }

        lua_pushstring(L, "__instance__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        instance_message_data->m_Instance = (HInstance) lua_touserdata(L, -1);
        assert(instance_message_data->m_Instance);
        instance_message_data->m_Component = 0xff;
        lua_pop(L, 1);

        lua_pushstring(L, "__collection__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        HCollection collection = (HCollection)lua_touserdata(L, -1);
        assert(collection);
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        dmGameObject::HRegister reg = dmGameObject::GetRegister(collection);
        dmMessage::Post(dmGameObject::GetMessageSocketId(reg), dmGameObject::GetMessageId(reg), buf, INSTANCE_MESSAGE_MAX);

        return 0;
    }

    int Script_GetPosition(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetPosition(i->m_Instance)));
        return 1;
    }

    int Script_GetRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        dmScript::PushQuat(L, dmGameObject::GetRotation(i->m_Instance));
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
        dmScript::PushVector3(L, Vectormath::Aos::Vector3(dmGameObject::GetWorldPosition(i->m_Instance)));
        return 1;
    }

    int Script_GetWorldRotation(lua_State* L)
    {
        ScriptInstance* i = ScriptInstance_Check(L, 1);
        dmScript::PushQuat(L, dmGameObject::GetWorldRotation(i->m_Instance));
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

    static bool IsPointVisible(const Vectormath::Aos::Point3& p, const Vectormath::Aos::Matrix4 view_proj, float margin)
    {
        Vectormath::Aos::Vector4 r = view_proj * p;
        assert(r.getW() != 0.0f);
        float r_w = 1.0f / r.getW();
        return dmMath::Abs(r.getX() * r_w) <= margin && dmMath::Abs(r.getY() * r_w) <= margin && dmMath::Abs(r.getZ() * r_w) <= margin;
    }

    int Script_IsVisible(lua_State* L)
    {
        // TODO: Let a divine engine coder have a go at this. :)
        Vectormath::Aos::Vector3* min = dmScript::CheckVector3(L, 1);
        Vectormath::Aos::Vector3* max = dmScript::CheckVector3(L, 2);
        float margin = 1.0f;
        if (lua_gettop(L) > 2)
        {
            margin = luaL_checknumber(L, 3);
        }
        // Try to find value in globals in update context
        lua_pushstring(L, "__update_context__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        UpdateContext* update_context = (UpdateContext*) lua_touserdata(L, -1);
        lua_pop(L, 1);
        assert(update_context);
        const Vectormath::Aos::Matrix4& view_proj = update_context->m_ViewProj;
        bool visible = false;
        if (IsPointVisible(Vectormath::Aos::Point3(*min), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(min->getX(), min->getY(), max->getZ()), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(min->getX(), max->getY(), min->getZ()), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(max->getX(), min->getY(), min->getZ()), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(max->getX(), min->getY(), max->getZ()), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(max->getX(), max->getY(), min->getZ()), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(max->getX(), min->getY(), max->getZ()), view_proj, margin)
                && IsPointVisible(Vectormath::Aos::Point3(*max), view_proj, margin))
            visible = true;
        lua_pushboolean(L, visible);
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

    int Script_Spawn(lua_State* L)
    {
        const char* prototype = luaL_checkstring(L, 1);
        Vectormath::Aos::Vector3* position = dmScript::CheckVector3(L, 2);
        Vectormath::Aos::Quat* rotation = dmScript::CheckQuat(L, 3);

        lua_pushstring(L, "__collection__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        HCollection collection = (HCollection)lua_touserdata(L, -1);
        assert(collection);
        lua_pop(L, 1);

        dmGameObject::SpawnMessage spawn_message;
        spawn_message.m_Collection = collection;
        dmStrlCpy(spawn_message.m_Prototype, prototype, sizeof(spawn_message.m_Prototype));
        spawn_message.m_Position = Vectormath::Aos::Point3(*position);
        spawn_message.m_Rotation = *rotation;
        dmMessage::Post(collection->m_Register->m_SpawnSocketId, collection->m_Register->m_SpawnMessageId, &spawn_message, sizeof(dmGameObject::SpawnMessage));

        return 0;
    }

    static const luaL_reg Script_methods[] =
    {
        {"post",                Script_Post},
        {"post_to",             Script_PostTo},
        {"post_to_collection",  Script_PostToCollection},
        {"get_position",        Script_GetPosition},
        {"get_rotation",        Script_GetRotation},
        {"set_position",        Script_SetPosition},
        {"set_rotation",        Script_SetRotation},
        {"get_world_position",  Script_GetWorldPosition},
        {"get_world_rotation",  Script_GetWorldRotation},
        {"get_id",              Script_GetId},
        {"is_visible",          Script_IsVisible},
        {"delete",              Script_Delete},
        {"spawn",               Script_Spawn},
        {0, 0}
    };

    void InitializeScript()
    {
        lua_State *L = lua_open();
        g_LuaState = L;

        luaopen_base(L);
        luaopen_table(L);
        luaopen_string(L);
        luaopen_math(L);
        luaopen_debug(L);

        int top = lua_gettop(L);

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

        luaL_unref(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);
        luaL_unref(L, LUA_REGISTRYINDEX, script_instance->m_ScriptDataReference);

        assert(top == lua_gettop(L));
    }
}
