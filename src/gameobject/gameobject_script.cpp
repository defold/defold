#include <assert.h>
#include <string.h>
#include <ddf/ddf.h>
#include <dlib/log.h>
#include <dlib/hash.h>
#include <dlib/hashtable.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/profile.h>
#include "gameobject.h"
#include "gameobject_script.h"
#include "gameobject_common.h"
#include "../script/script_util.h"

extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
#include <lua/lualib.h>
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
        else if (strcmp(key, "Rotation") == 0)
        {
            lua_newtable(L);

            Quat& rot = i->m_Instance->m_Rotation;

            lua_pushliteral(L, "X");
            lua_pushnumber(L, rot.getX());
            lua_rawset(L, -3);

            lua_pushliteral(L, "Y");
            lua_pushnumber(L, rot.getY());
            lua_rawset(L, -3);

            lua_pushliteral(L, "Z");
            lua_pushnumber(L, rot.getZ());
            lua_rawset(L, -3);

            lua_pushliteral(L, "W");
            lua_pushnumber(L, rot.getW());
            lua_rawset(L, -3);
            return 1;
        }
        else if (strcmp(key, "WorldPosition") == 0)
        {
            lua_newtable(L);

            Point3 pos = dmGameObject::GetWorldPosition(i->m_Instance);

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
        else if (strcmp(key, "WorldRotation") == 0)
        {
            lua_newtable(L);

            Quat rot = dmGameObject::GetWorldRotation(i->m_Instance);

            lua_pushliteral(L, "X");
            lua_pushnumber(L, rot.getX());
            lua_rawset(L, -3);

            lua_pushliteral(L, "Y");
            lua_pushnumber(L, rot.getY());
            lua_rawset(L, -3);

            lua_pushliteral(L, "Z");
            lua_pushnumber(L, rot.getZ());
            lua_rawset(L, -3);

            lua_pushliteral(L, "W");
            lua_pushnumber(L, rot.getW());
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
            if (strcmp(key, "DT") == 0)
            {
                lua_pushnumber(L, update_context->m_DT);
                return 1;
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

            lua_getfield(L, 3, "X");
            if (lua_isnil(L, -1))
            {
                // X not present, assume { x, y, z }
                lua_pop(L, 1);
                lua_rawgeti(L, 3, 1);
                lua_rawgeti(L, 3, 2);
                lua_rawgeti(L, 3, 3);
            }
            else
            {
                // X present, assume { X = x, ... }
                lua_pushliteral(L, "X");
                lua_rawget(L, 3);
                lua_pushliteral(L, "Y");
                lua_rawget(L, 3);
                lua_pushliteral(L, "Z");
                lua_rawget(L, 3);
            }

            float x = luaL_checknumber(L, -3);
            float y = luaL_checknumber(L, -2);
            float z = luaL_checknumber(L, -1);

            i->m_Instance->m_Position = Point3(x, y, z);
        }
        else if (strcmp(key, "Rotation") == 0)
        {
            luaL_checktype(L, 3, LUA_TTABLE);

            lua_getfield(L, 3, "X");
            if (lua_isnil(L, -1))
            {
                // X not present, assume { x, y, z, w }
                lua_pop(L, 1);
                lua_rawgeti(L, 3, 1);
                lua_rawgeti(L, 3, 2);
                lua_rawgeti(L, 3, 3);
                lua_rawgeti(L, 3, 4);
            }
            else
            {
                // X present, assume { X = x, ... }
                lua_pushliteral(L, "X");
                lua_rawget(L, 3);
                lua_pushliteral(L, "Y");
                lua_rawget(L, 3);
                lua_pushliteral(L, "Z");
                lua_rawget(L, 3);
                lua_pushliteral(L, "W");
                lua_rawget(L, 3);
            }

            float x = luaL_checknumber(L, -4);
            float y = luaL_checknumber(L, -3);
            float z = luaL_checknumber(L, -2);
            float w = luaL_checknumber(L, -1);

            i->m_Instance->m_Rotation = Quat(x, y, z, w);
        }
        else
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_ScriptDataReference);
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_settable(L, -3);
        }

        // TODO: Check top...

        return 1;
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
    extern uint32_t g_Socket;
    extern uint32_t g_ReplySocket;
    extern uint32_t g_EventID;

    static void PullDDFTable(lua_State* L, const dmDDF::Descriptor* d,
                             char* message, char** buffer, char** buffer_last);

    static void PullDDFValue(lua_State* L, const dmDDF::FieldDescriptor* f,
                             char* message, char** buffer, char** buffer_last)
    {
    	bool nil_val = lua_isnil(L, -1);
		switch (f->m_Type)
		{
			case dmDDF::TYPE_INT32:
			{
				if (nil_val)
					*((int32_t *) &message[f->m_Offset]) = 0;
				else
					*((int32_t *) &message[f->m_Offset]) = (int32_t) luaL_checkinteger(L, -1);
			}
			break;

			case dmDDF::TYPE_UINT32:
			{
				if (nil_val)
					*((uint32_t *) &message[f->m_Offset]) = 0;
				else
					*((uint32_t *) &message[f->m_Offset]) = (uint32_t) luaL_checkinteger(L, -1);
			}
			break;

			case dmDDF::TYPE_FLOAT:
			{
				if (nil_val)
					*((float *) &message[f->m_Offset]) = 0.0f;
				else
					*((float *) &message[f->m_Offset]) = (float) luaL_checknumber(L, -1);
			}
			break;

			case dmDDF::TYPE_STRING:
			{
				const char* s = "";
				if (!nil_val)
					s = luaL_checkstring(L, -1);
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
				if (!nil_val)
				{
					const dmDDF::Descriptor* d = f->m_MessageDescriptor;
					PullDDFTable(L, d, &message[f->m_Offset], buffer, buffer_last);
				}
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
            if (lua_isnil(L, -1) && f->m_Label != dmDDF::LABEL_OPTIONAL)
            {
                luaL_error(L, "Field %s not specified in table", f->m_Name);
            }
            else
            {
            	PullDDFValue(L, f, message, buffer, buffer_last);
            }
            lua_pop(L, 1);
        }
    }

    int Script_Hash(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* str = luaL_checkstring(L, 1);
        char buf[9];
        DM_SNPRINTF(buf, sizeof(buf), "%X", dmHashString32(str));
        lua_pushstring(L, buf);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    int Script_PostNamedTo(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* instance_name = luaL_checkstring(L, 1);
        const char* component_name = luaL_checkstring(L, 2);
        const char* event_name = luaL_checkstring(L, 3);

        const dmDDF::Descriptor* desc = 0x0;
        char ddf_data[SCRIPT_EVENT_MAX - sizeof(ScriptEventData)];

        // Passing ddf data is optional atm
        if (top >= 4)
        {
            const char* type_name = event_name;
            uint64_t h = dmHashBuffer64(type_name, strlen(type_name));
            const dmDDF::Descriptor** desc_tmp = g_Descriptors->Get(h);
            if (desc_tmp != 0)
            {
                desc = *desc_tmp;
                if (desc->m_Size > SCRIPT_EVENT_MAX - sizeof(ScriptEventData))
                {
                    luaL_error(L, "sizeof(%s) > %d", type_name, SCRIPT_EVENT_MAX - sizeof(ScriptEventData));
                    return 0;
                }
                luaL_checktype(L, 4, LUA_TTABLE);

                lua_pushvalue(L, 4);
                dmScriptUtil::LuaTableToDDF(L, desc, ddf_data, SCRIPT_EVENT_MAX - sizeof(ScriptEventData));
                lua_pop(L, 1);
            }
            else
            {
                luaL_error(L, "DDF type %s has not been registered through dmGameObject::RegisterDDFType.", type_name);
            }
        }

        lua_pushstring(L, "__collection__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        HCollection collection = (HCollection) lua_touserdata(L, -1);
        assert(collection);
        lua_pop(L, 1);

        HInstance instance = dmGameObject::GetInstanceFromIdentifier(collection, dmHashString32(instance_name));
        if (instance)
        {
            dmGameObject::Result r = dmGameObject::PostNamedEvent(instance, component_name, event_name, desc, ddf_data);
            if (r != dmGameObject::RESULT_OK)
            {
                // TODO: Translate r to string
                luaL_error(L, "Error sending event '%s' to %s/%s", event_name, instance_name, component_name);
            }
        }
        else
        {
            luaL_error(L, "Error sending event. Unknown instance: %s", instance_name);
        }
        assert(top == lua_gettop(L));

        return 0;
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

        lua_pushstring(L, "__instance__");
        lua_rawget(L, LUA_GLOBALSINDEX);
        script_event_data->m_Instance = (HInstance) lua_touserdata(L, -1);
        assert(script_event_data->m_Instance);
        lua_pop(L, 1);

        char* p = buf + sizeof(ScriptEventData);
        lua_pushvalue(L, 2);
        dmScriptUtil::LuaTableToDDF(L, d, p, SCRIPT_EVENT_MAX - sizeof(ScriptEventData));
        lua_pop(L, 1);

        assert(top == lua_gettop(L));

        dmMessage::Post(g_Socket, g_EventID, buf, SCRIPT_EVENT_MAX);

        return 0;
    }

    int Script_Transform(lua_State* L)
    {
        int top = lua_gettop(L);

        // check args
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TTABLE);

        // retrieve position
        lua_getfield(L, 1, "Position");
        Vectormath::Aos::Point3 position(0.0f, 0.0f, 0.0f);
        if (lua_istable(L, 3))
        {
            lua_getfield(L, 3, "X");
            position.setX((float)lua_tonumber(L, 4));
            lua_getfield(L, 3, "Y");
            position.setY((float)lua_tonumber(L, 5));
            lua_getfield(L, 3, "Z");
            position.setZ((float)lua_tonumber(L, 6));
            lua_pop(L, 3);
        }
        lua_pop(L, 1);

        // retrieve rotation
        lua_getfield(L, 1, "Rotation");
        Vectormath::Aos::Quat rotation(0.0f, 0.0f, 0.0f, 1.0f);
        if (lua_istable(L, 3))
        {
            lua_getfield(L, 3, "X");
            rotation.setX((float)lua_tonumber(L, 4));
            lua_getfield(L, 3, "Y");
            rotation.setY((float)lua_tonumber(L, 5));
            lua_getfield(L, 3, "Z");
            rotation.setZ((float)lua_tonumber(L, 6));
            lua_getfield(L, 3, "W");
            rotation.setW((float)lua_tonumber(L, 7));
            lua_pop(L, 4);
        }
        lua_pop(L, 1);

        // retrieve point to transform
        Vectormath::Aos::Vector3 p;
        lua_getfield(L, 2, "X");
        p.setX((float)lua_tonumber(L, 3));
        lua_getfield(L, 2, "Y");
        p.setY((float)lua_tonumber(L, 4));
        lua_getfield(L, 2, "Z");
        p.setZ((float)lua_tonumber(L, 5));
        lua_pop(L, 3);

        // calculate!
        Vectormath::Aos::Point3 result = position + Vectormath::Aos::rotate(rotation, p);

        // write result
        lua_newtable(L);
        lua_pushnumber(L, result.getX());
        lua_setfield(L, 3, "X");
        lua_pushnumber(L, result.getY());
        lua_setfield(L, 3, "Y");
        lua_pushnumber(L, result.getZ());
        lua_setfield(L, 3, "Z");

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    int Script_Rotate(lua_State* L)
    {
        int top = lua_gettop(L);

        // check args
        luaL_checktype(L, 1, LUA_TTABLE);
        luaL_checktype(L, 2, LUA_TTABLE);

        // retrieve rotation
        Vectormath::Aos::Quat r;
        lua_getfield(L, 1, "X");
        r.setX((float)lua_tonumber(L, 3));
        lua_getfield(L, 1, "Y");
        r.setY((float)lua_tonumber(L, 4));
        lua_getfield(L, 1, "Z");
        r.setZ((float)lua_tonumber(L, 5));
        lua_getfield(L, 1, "W");
        r.setW((float)lua_tonumber(L, 6));
        lua_pop(L, 4);

        // retrieve rotation
        Vectormath::Aos::Vector3 v;
        lua_getfield(L, 2, "X");
        v.setX((float)lua_tonumber(L, 3));
        lua_getfield(L, 2, "Y");
        v.setY((float)lua_tonumber(L, 4));
        lua_getfield(L, 2, "Z");
        v.setZ((float)lua_tonumber(L, 5));
        lua_pop(L, 3);

        // calculate!
        Vectormath::Aos::Vector3 result = Vectormath::Aos::rotate(r, v);

        // write result
        lua_newtable(L);
        lua_pushnumber(L, result.getX());
        lua_setfield(L, 3, "X");
        lua_pushnumber(L, result.getY());
        lua_setfield(L, 3, "Y");
        lua_pushnumber(L, result.getZ());
        lua_setfield(L, 3, "Z");

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    ScriptWorld::ScriptWorld()
    : m_Instances()
    {
        m_Instances.SetCapacity(512);
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

        lua_pushliteral(L, "PostNamedTo");
        lua_pushcfunction(L, Script_PostNamedTo);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "Hash");
        lua_pushcfunction(L, Script_Hash);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "Transform");
        lua_pushcfunction(L, Script_Transform);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "Rotate");
        lua_pushcfunction(L, Script_Rotate);
        lua_rawset(L, LUA_GLOBALSINDEX);
    }

    void FinalizeScript()
    {
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

    static int LoadScript(lua_State* L, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        int top = lua_gettop(L);
        (void) top;

        int functions;

        LuaData data;
        data.m_Buffer = (const char*)buffer;
        data.m_Size = buffer_size;
        int ret = lua_load(L, &ReadScript, &data, filename);
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
            dmLogError("No function table found in script: %s", filename);
            lua_pop(L, 1);
            goto bail;
        }

        functions = luaL_ref( L, LUA_REGISTRYINDEX );
        assert(top == lua_gettop(L));

        return functions;
bail:
        assert(top == lua_gettop(L));
        return -1;
    }

    HScript NewScript(const void* buffer, uint32_t buffer_size, const char* filename)
    {
        lua_State* L = g_LuaState;

        int functions = LoadScript(L, buffer, buffer_size, filename);
        if (functions != -1)
        {
            HScript script = new Script();
            script->m_FunctionsReference = functions;
            return script;
        }
        else
        {
            return 0;
        }
    }

    bool ReloadScript(HScript script, const void* buffer, uint32_t buffer_size, const char* filename)
    {
        lua_State* L = g_LuaState;

        int functions = LoadScript(L, buffer, buffer_size, filename);
        if (functions != -1)
        {
            script->m_FunctionsReference = functions;
            return true;
        }
        else
        {
            return false;
        }
    }

    void DeleteScript(HScript script)
    {
        lua_State* L = g_LuaState;
        luaL_unref(L, LUA_REGISTRYINDEX, script->m_FunctionsReference);
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

    bool RunScript(HCollection collection, HScript script, const char* function_name, HScriptInstance script_instance, const UpdateContext* update_context)
    {
        DM_PROFILE(Script, "RunScript");
        lua_State* L = g_LuaState;
        int top = lua_gettop(L);
        (void) top;
        int ret;

        lua_pushliteral(L, "__collection__");
        lua_pushlightuserdata(L, (void*) collection);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "__update_context__");
        lua_pushlightuserdata(L, (void*) update_context);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_pushliteral(L, "__instance__");
        lua_pushlightuserdata(L, (void*) script_instance->m_Instance);
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

    dmResource::CreateResult ResCreateScript(dmResource::HFactory factory,
                                             void* context,
                                             const void* buffer, uint32_t buffer_size,
                                             dmResource::SResourceDescriptor* resource,
                                             const char* filename)
    {
        HScript script = NewScript(buffer, buffer_size, filename);
        if (script)
        {
            resource->m_Resource = (void*) script;
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    dmResource::CreateResult ResDestroyScript(dmResource::HFactory factory,
                                              void* context,
                                              dmResource::SResourceDescriptor* resource)
    {
        DeleteScript((HScript) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateScript(dmResource::HFactory factory,
                                               void* context,
                                               const void* buffer, uint32_t buffer_size,
                                               dmResource::SResourceDescriptor* resource,
                                               const char* filename)
    {
        HScript script = (HScript) resource->m_Resource;
        if (ReloadScript(script, buffer, buffer_size, filename))
        {
            return dmResource::CREATE_RESULT_OK;
        }
        else
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
    }

    CreateResult ScriptNewWorld(void* context, void** world)
    {
        if (world != 0x0)
        {
            *world = new ScriptWorld();
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult ScriptDeleteWorld(void* context, void* world)
    {
        if (world != 0x0)
        {
            delete (ScriptWorld*)world;
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult ScriptCreateComponent(HCollection collection,
            HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        HScript script = (HScript)resource;
        HScriptInstance script_instance = NewScriptInstance(script, instance);
        if (script_instance != 0x0)
        {
            instance->m_ScriptInstancePOOOOP = script_instance;
            ScriptWorld* script_world = (ScriptWorld*)world;
            script_world->m_Instances.Push(script_instance);
            *user_data = (uintptr_t)script_instance;
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult ScriptInitComponent(HCollection collection,
            HInstance instance,
            void* context,
            uintptr_t* user_data)
    {
        Prototype* proto = instance->m_Prototype;
        HScriptInstance script_instance = (HScriptInstance)*user_data;
        bool ret = RunScript(collection, script_instance->m_Script, "Init", script_instance, 0x0);
        if (ret)
        {
            return CREATE_RESULT_OK;
        }
        else
        {
            dmLogError("The script for prototype %s failed to run.", proto->m_Name);
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult ScriptDestroyComponent(HCollection collection,
            HInstance instance,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        ScriptWorld* script_world = (ScriptWorld*)world;
        HScriptInstance script_instance = (HScriptInstance)*user_data;
        for (uint32_t i = 0; i < script_world->m_Instances.Size(); ++i)
        {
            if (script_instance == script_world->m_Instances[i])
            {
                script_world->m_Instances.EraseSwap(i);
                break;
            }
        }
        instance->m_ScriptInstancePOOOOP = 0x0;
        DeleteScriptInstance(script_instance);
        return CREATE_RESULT_OK;
    }

    UpdateResult ScriptUpdateComponent(HCollection collection,
            const UpdateContext* update_context,
            void* world,
            void* context)
    {
        UpdateResult result = UPDATE_RESULT_OK;
        ScriptWorld* script_world = (ScriptWorld*)world;
        uint32_t size = script_world->m_Instances.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            HScriptInstance script_instance = script_world->m_Instances[i];
            Prototype* proto = script_instance->m_Instance->m_Prototype;
            bool ret = RunScript(collection, script_instance->m_Script, "Update", script_instance, update_context);
            if (!ret)
            {
                dmLogError("The script for prototype %s failed to run.", proto->m_Name);
                result = UPDATE_RESULT_UNKNOWN_ERROR;
            }
        }
        return result;
    }

    UpdateResult ScriptOnEventComponent(HCollection collection,
            HInstance instance,
            const ScriptEventData* script_event_data,
            void* context,
            uintptr_t* user_data)
    {
        ScriptInstance* script_instance = (ScriptInstance*)*user_data;
        assert(script_event_data->m_Instance);

        lua_State* L = g_LuaState;
        int top = lua_gettop(L);
        (void) top;
        int ret;

        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_Script->m_FunctionsReference);
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            dmLogError("Invalid 'SenderFunctionsReference' (%d) in event", script_instance->m_Script->m_FunctionsReference);
            return UPDATE_RESULT_UNKNOWN_ERROR;
        }

        const char* function_name = "OnEvent";
        lua_getfield(L, -1, function_name);
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            dmLogError("No '%s' function found", function_name);
            lua_pop(L, 2);
            goto bail;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

        char buf[9];
        DM_SNPRINTF(buf, sizeof(buf), "%X", script_event_data->m_EventHash);
        lua_pushstring(L, buf);

        if (script_event_data->m_DDFDescriptor)
        {
            // adjust char ptrs to global mem space
            char* data = (char*)script_event_data->m_DDFData;
            for (uint8_t i = 0; i < script_event_data->m_DDFDescriptor->m_FieldCount; ++i)
            {
                dmDDF::FieldDescriptor* field = &script_event_data->m_DDFDescriptor->m_Fields[i];
                uint32_t field_type = field->m_Type;
                if (field_type == dmDDF::TYPE_STRING)
                {
                    *((uintptr_t*)&data[field->m_Offset]) = (uintptr_t)data + (uintptr_t)data[field->m_Offset];
                }
            }
            // TODO: setjmp/longjmp here... how to handle?!!! We are not running "from lua" here
            // lua_cpcall?
            dmScriptUtil::DDFToLuaTable(L, script_event_data->m_DDFDescriptor, (const char*) script_event_data->m_DDFData);
        }
        else
        {
            // Named event
            lua_newtable(L);
        }

        ret = lua_pcall(L, 3, LUA_MULTRET, 0);
        if (ret != 0)
        {
            dmLogError("Error running script: %s", lua_tostring(L,-1));
            lua_pop(L, 2);
            goto bail;
        }

        lua_pop(L, 1);

        assert(top == lua_gettop(L));
        return UPDATE_RESULT_OK;
bail:
        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);
        assert(top == lua_gettop(L));
        return UPDATE_RESULT_UNKNOWN_ERROR;
    }

    InputResult ScriptOnInputComponent(HCollection collection,
            HInstance instance,
            const InputAction* input_action,
            void* context,
            uintptr_t* user_data)
    {
        ScriptInstance* script_instance = (ScriptInstance*)*user_data;

        lua_State* L = g_LuaState;
        int top = lua_gettop(L);
        int ret;

        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_Script->m_FunctionsReference);
        if (lua_type(L, -1) != LUA_TTABLE)
        {
            dmLogError("Invalid 'SenderFunctionsReference' (%d) in OnInput.", script_instance->m_Script->m_FunctionsReference);
            return INPUT_RESULT_UNKNOWN_ERROR;
        }

        const char* function_name = "OnInput";
        lua_getfield(L, -1, function_name);
        if (lua_type(L, -1) != LUA_TFUNCTION)
        {
            dmLogError("No '%s' function found", function_name);
            lua_pop(L, 2);
            goto bail;
        }

        lua_rawgeti(L, LUA_REGISTRYINDEX, script_instance->m_InstanceReference);

        lua_createtable(L, 0, 5);
        {
            int action_table = lua_gettop(L);

            char buf[9];
            DM_SNPRINTF(buf, sizeof(buf), "%X", input_action->m_ActionId);
            lua_pushliteral(L, "Id");
            lua_pushstring(L, buf);
            lua_settable(L, action_table);

            lua_pushliteral(L, "Value");
            lua_pushnumber(L, input_action->m_Value);
            lua_settable(L, action_table);

            lua_pushliteral(L, "Pressed");
            lua_pushboolean(L, input_action->m_Pressed);
            lua_settable(L, action_table);

            lua_pushliteral(L, "Released");
            lua_pushboolean(L, input_action->m_Released);
            lua_settable(L, action_table);

            lua_pushliteral(L, "Repeated");
            lua_pushboolean(L, input_action->m_Repeated);
            lua_settable(L, action_table);
        }
        {
            int input_ret = lua_gettop(L) - 2;
            ret = lua_pcall(L, 2, LUA_MULTRET, 0);
            if (ret != 0)
            {
                dmLogError("Error running script %s: %s", function_name, lua_tostring(L, lua_gettop(L)));
                lua_pop(L, 2);
                goto bail;
            }
            else if (input_ret == lua_gettop(L))
            {
                if (!lua_isboolean(L, -1))
                {
                    dmLogError("Script %s must return a boolean value (true/false), or no value at all.", function_name);
                    lua_pop(L, 2);
                    goto bail;
                }
                else
                {
                    int input_ret = lua_toboolean(L, -1);
                    lua_pop(L, 1);
                    assert(top == lua_gettop(L));
                    if (input_ret)
                        return INPUT_RESULT_CONSUMED;
                    else
                        return INPUT_RESULT_IGNORED;
                }
            }
            else
            {
                lua_pop(L, 1);
                return INPUT_RESULT_IGNORED;
            }
        }
bail:
        lua_pushliteral(L, "__update_context__");
        lua_pushnil(L);
        lua_rawset(L, LUA_GLOBALSINDEX);
        assert(top == lua_gettop(L));
        return INPUT_RESULT_UNKNOWN_ERROR;
    }
}
