#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

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
     * @return rms values for left and right channel
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
     * @return peak values for left and right channel
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

    static const luaL_reg SOUND_FUNCTIONS[] =
    {
        {"is_music_playing", Sound_IsMusicPlaying},
        {"get_rms", Sound_GetRMS},
        {"get_peak", Sound_GetPeak},
        {"set_group_gain", Sound_SetGroupGain},
        {"get_group_gain", Sound_GetGroupGain},
        {"get_groups", Sound_GetGroups},
        {"get_group_name", Sound_GetGroupName},
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
