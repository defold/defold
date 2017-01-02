#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/dstrings.h>

#include "../gamesys.h"
#include "../gamesys_private.h"
#include "gamesys_ddf.h"

#include <sound/sound.h>

#include "../gamesys.h"

#include "script_sound.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# Sound API documentation
     *
     * Functions and messages for controlling sound components and
     * mixer groups.
     *
     * @name Sound
     * @namespace sound
     */

    /*# check if background music is playing
     * Checks if background music is playing, e.g. from iTunes
     *
     * @name sound.is_music_playing
     * @return true if music is playing (bool)
     */
    int Sound_IsMusicPlaying(lua_State* L)
    {
        lua_pushboolean(L, (int) dmSound::IsMusicPlaying());
        return 1;
    }

    static dmhash_t CheckGroupName(lua_State* L, int index) {
        if (lua_isstring(L, index)) {
            return dmHashString64(lua_tostring(L, index));
        } else if (dmScript::IsHash(L, index)) {
            return dmScript::CheckHash(L, index);
        }
        luaL_argerror(L, index, "hash or string expected");
        return (dmhash_t) 0;
    }

    /*# get rms value from mixer group
     * Get RMS (Root Mean Square) value from mixer group.
     * <p>
     * Note that the returned value might be an approximation and in particular
     * the effective window might be larger than specified.
     * </p>
     *
     * @param group group name (hash|string)
     * @param window window length in seconds (number)
     * @name sound.get_rms
     * @return rms value for left channel (number)
     * @return rms value for right channel (number)
     */
    int Sound_GetRMS(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t group_hash = CheckGroupName(L, 1);
        float window = luaL_checknumber(L, 2);
        float left = 0, right = 0;
        dmSound::Result r = dmSound::GetGroupRMS(group_hash, window, &left, &right);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to get RMS (%d)", r);
        }

        lua_pushnumber(L, left);
        lua_pushnumber(L, right);

        assert(top + 2 == lua_gettop(L));
        return 2;
    }

    /*# get peak gain value from mixer group
     * Get peak value from mixer group.
     * <p>
     * Note that the returned value might be an approximation and in particular
     * the effective window might be larger than specified.
     * </p>
     *
     * @param group group name (hash|string)
     * @param window window length in seconds (number)
     * @name sound.get_peak
     * @return peak value for left channel (number)
     * @return peak value for right channel (number)
     */
    int Sound_GetPeak(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t group_hash = CheckGroupName(L, 1);
        float window = luaL_checknumber(L, 2);
        float left = 0, right = 0;
        dmSound::Result r = dmSound::GetGroupPeak(group_hash, window, &left, &right);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to get peak (%d)", r);
        }

        lua_pushnumber(L, left);
        lua_pushnumber(L, right);

        assert(top + 2 == lua_gettop(L));
        return 2;
    }

    /*# set mixer group gain
     * Set mixer group gain
     * <p>
     * Note that gain is in linear scale.
     * </p>
     *
     * @param group group name (hash|string)
     * @param gain gain in linear scale (number)
     * @name sound.set_group_gain
     */
    int Sound_SetGroupGain(lua_State* L)
    {
        int top = lua_gettop(L);
        dmhash_t group_hash = CheckGroupName(L, 1);
        float gain = luaL_checknumber(L, 2);

        dmSound::Result r = dmSound::SetGroupGain(group_hash, gain);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to set group gain (%d)", r);
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# get mixer group gain
     * Get mixer group gain
     * <p>
     * Note that gain is in linear scale.
     * </p>
     *
     * @param group group name (hash|string)
     * @name sound.get_group_gain
     * @return gain in linear scale
     */
    int Sound_GetGroupGain(lua_State* L)
    {
        int top = lua_gettop(L);
        dmhash_t group_hash = CheckGroupName(L, 1);
        float gain = 0;

        dmSound::Result r = dmSound::GetGroupGain(group_hash, &gain);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to get group gain (%d)", r);
        }
        lua_pushnumber(L, gain);
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# get all mixer group names
     * Get all mixer group names
     *
     * @name sound.get_groups
     * @return table of mixer groups names (table)
     */
    int Sound_GetGroups(lua_State* L)
    {
        int top = lua_gettop(L);

        uint32_t count = dmSound::GetGroupCount();
        lua_createtable(L, count, 0);
        for (uint32_t i = 0; i < count; i++) {
            dmhash_t group_hash;
            dmSound::GetGroupHash(i, &group_hash);
            dmScript::PushHash(L, group_hash);
            lua_rawseti(L, -2, i + 1);
        }

        assert(top + 1 == lua_gettop(L));

        return 1;
    }

    /*# get mixer group name string
     * Get a mixer group name as a string.
     * <p>Note that this function does not return correct group name in release mode</p>
     *
     * @name sound.get_group_name
     * @param group group name (hash|string)
     * @return group name (string)
     */
    int Sound_GetGroupName(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t group_hash = dmScript::CheckHash(L, 1);
        const char* name = (const char*) dmHashReverse64(group_hash, 0);
        if (name) {
            lua_pushstring(L, name);
        } else {
            lua_pushfstring(L, "unknown_%llu", group_hash);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# check if a phone call is active
     * Checks if a phone call is active. If there is an active phone call all
     * other sounds will be muted until the phone call is finished.
     *
     * @name sound.is_phone_call_active
     * @return true if there is an active phone call (bool)
     */
    int Sound_IsPhoneCallActive(lua_State* L)
    {
        int top = lua_gettop(L);
        lua_pushboolean(L, (int) dmSound::IsPhoneCallActive());
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static lua_Number CheckTableNumber(lua_State* L, int index, const char* name, lua_Number fallback)
    {
        int top = lua_gettop(L);

        lua_Number result = -1;
        lua_pushstring(L, name);
        lua_gettable(L, index);
        if (lua_isnumber(L, -1)) {
            result = lua_tonumber(L, -1);
        } else if (lua_isnil(L, -1)) {
            result = fallback;
        } else {
            char msg[256];
            DM_SNPRINTF(msg, sizeof(msg), "Wrong type for table attribute '%s'. Expected number, got %s", name, luaL_typename(L, -1) );
            return luaL_error(L, msg);
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return result;
    }

    static int CheckTableFunction(lua_State* L, int index, const char* name, lua_Number fallback)
    {
        int top = lua_gettop(L);

        int result = -1;
        lua_pushstring(L, name);
        lua_gettable(L, index);
        if (lua_isfunction(L, -1)) {
            result = dmScript::Ref(L, LUA_REGISTRYINDEX);
            lua_pushnil(L); // so that stack matches for the check at the end
        } else if (lua_isnil(L, -1)) {
            result = fallback;
        } else {
            char msg[256];
            DM_SNPRINTF(msg, sizeof(msg), "Wrong type for table attribute '%s'. Expected number, got %s", name, luaL_typename(L, -1) );
            return luaL_error(L, msg);
        }
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
        return result;
    }


    static dmSound::StreamResult SoundScriptFetchCallback(dmSound::HSoundData sound_data, uint32_t buffer_size, uint8_t* buffer, uint32_t* nread, void* user_ctx)
    {
        dmGameSystemDDF::LuaCallback& cbk = *(dmGameSystemDDF::LuaCallback*)user_ctx;
        lua_State* L = (lua_State*)cbk.m_LuaState;

        int top = lua_gettop(L);

        lua_rawgeti(L, LUA_REGISTRYINDEX, cbk.m_Function);
        lua_rawgeti(L, LUA_REGISTRYINDEX, cbk.m_Instance);
        lua_pushvalue(L, -1); // SetInstance [-1, +0, e]
        dmScript::SetInstance(L);

        if (!dmScript::IsInstanceValid(L))
        {
            dmLogError("Could not run sound fetch callback because the instance has been deleted.");
            lua_pop(L, 2);
            assert(top == lua_gettop(L));
            return dmSound::STREAM_RESULT_END_OF_STREAM;
        }

        lua_pushnumber(L, 0); // ID
        lua_pushnumber(L, buffer_size); // requested number of bytes


        (void) dmScript::PCall(L, 3, 1);

        if ( !dmScript::IsBuffer(L, -1) )
        {
            dmLogWarning("NO BUFFER -> END OF STREAM\n");
            *nread = 0;

            lua_pop(L, 1);
            assert(top == lua_gettop(L));
            return dmSound::STREAM_RESULT_END_OF_STREAM;
        }

        dmBuffer::HBuffer* soundbuffer = dmScript::CheckBuffer(L, -1);

        uint8_t* data = 0;
        uint32_t datasize = 0;
        dmBuffer::GetBytes(*soundbuffer, (void**)&data, &datasize);

        lua_pop(L, 1);
        assert(top == lua_gettop(L));

        // Due to the wav format, it may be that the reported data chunk size is larger than the actual file
        // Internally we check that we get multiples of four bytes
        datasize = datasize & ~0x3;

        if( datasize == 0 )
        {
            *nread = 0;
            return dmSound::STREAM_RESULT_END_OF_STREAM;
        }

        datasize = dmMath::Min(datasize, buffer_size);

        memcpy(buffer, data, datasize);
        *nread = datasize;

        return dmSound::STREAM_RESULT_OK;
    }



    int Sound_Play(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);
  
        dmGameSystemDDF::PlaySound msg;
        memset(&msg, 0, sizeof(msg));
        msg.m_Gain = 1.0f;

        if ( lua_istable(L, 2) )
        {
            luaL_checktype(L, 2, LUA_TTABLE);
            msg.m_Delay = CheckTableNumber(L, 2, "delay", msg.m_Delay);
            msg.m_Gain = CheckTableNumber(L, 2, "gain", msg.m_Gain);

            msg.m_LuaCallback.m_Function = CheckTableFunction(L, 2, "fetch_function", 0);
            if( msg.m_LuaCallback.m_Function )
            {
                dmScript::GetInstance(L);

                msg.m_FetchFunction = (uint64_t)SoundScriptFetchCallback;
                msg.m_LuaCallback.m_Instance = dmScript::Ref(L, LUA_REGISTRYINDEX);
                msg.m_LuaCallback.m_LuaState = (uint64_t)L;
            }
        }

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PlaySound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::PlaySound::m_DDFDescriptor, &msg, sizeof(msg), 0);

        assert(top == lua_gettop(L));
        return 0;
    }

    int Sound_Stop(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);
  
        dmGameSystemDDF::StopSound msg;
        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::StopSound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::StopSound::m_DDFDescriptor, &msg, sizeof(msg), 0);

        assert(top == lua_gettop(L));
        return 0;
    }

    int Sound_SetGain(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);
  
        dmGameSystemDDF::SetGain msg;
        msg.m_Gain = luaL_checknumber(L, 2);
        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetGain::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetGain::m_DDFDescriptor, &msg, sizeof(msg), 0);

        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg SOUND_FUNCTIONS[] =
    {
        {"is_music_playing", Sound_IsMusicPlaying},
        {"get_rms", Sound_GetRMS},
        {"get_peak", Sound_GetPeak},
        {"set_group_gain", Sound_SetGroupGain},
        {"get_group_gain", Sound_GetGroupGain},
        {"get_groups", Sound_GetGroups},
        {"get_group_name", Sound_GetGroupName},
        {"play", Sound_Play},
        {"stop", Sound_Stop},
        {"set_gain", Sound_SetGain},
        {"is_phone_call_active", Sound_IsPhoneCallActive},
        {0, 0}
    };

    void ScriptSoundRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        int top = lua_gettop(L);
        (void)top;
        luaL_register(L, "sound", SOUND_FUNCTIONS);
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }
}
