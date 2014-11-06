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
    /*# Check if background music is on, e.g. from iTunes
     *
     *
     * @name sound.is_music_playing
     */
    int Sound_IsMusicPlaying(lua_State* L)
    {
        lua_pushboolean(L, (int) dmSound::IsMusicPlaying());
        return 1;
    }

    /*#
     * Get RMS (Root Mean Square) value from mixer group.
     * <p>
     * Note that the returned value might be an approximation and in particular
     * the effective window might be larger than specified.
     * </p>
     *
     * @param group group name [string]
     * @param window window length in seconds [float]
     * @name sound.get_rms
     * @return rms values for left and right channel
     */
    int Sound_GetRMS(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* group = luaL_checkstring(L, 1);
        float window = luaL_checknumber(L, 2);
        float left = 0, right = 0;
        dmSound::Result r = dmSound::GetGroupRMS(group, window, &left, &right);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to get RMS (%d)", r);
        }

        lua_pushnumber(L, left);
        lua_pushnumber(L, right);

        assert(top + 2 == lua_gettop(L));
        return 2;
    }

    /*#
     * Get peak value from mixer group.
     * <p>
     * Note that the returned value might be an approximation and in particular
     * the effective window might be larger than specified.
     * </p>
     *
     * @param group group name [string]
     * @param window window length in seconds [float]
     * @name sound.get_peak
     * @return peak values for left and right channel
     */
    int Sound_GetPeak(lua_State* L)
    {
        int top = lua_gettop(L);

        const char* group = luaL_checkstring(L, 1);
        float window = luaL_checknumber(L, 2);
        float left = 0, right = 0;
        dmSound::Result r = dmSound::GetGroupPeak(group, window, &left, &right);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to get peak (%d)", r);
        }

        lua_pushnumber(L, left);
        lua_pushnumber(L, right);

        assert(top + 2 == lua_gettop(L));
        return 2;
    }

    /*#
     * Set mixer group gain
     * <p>
     * Note that gain is in linear scale.
     * </p>
     *
     * @param group group name [string]
     * @param gain gain in linear scale [float]
     * @name sound.set_group_gain
     */
    int Sound_SetGroupGain(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* group = luaL_checkstring(L, 1);
        float gain = luaL_checknumber(L, 2);

        dmSound::Result r = dmSound::SetGroupGain(group, gain);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to set group gain (%d)", r);
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*#
     * Get mixer group gain
     * <p>
     * Note that gain is in linear scale.
     * </p>
     *
     * @param group group name [string]
     * @name sound.set_group_gain
     * @return gain in linear scale
     */
    int Sound_GetGroupGain(lua_State* L)
    {
        int top = lua_gettop(L);
        const char* group = luaL_checkstring(L, 1);
        float gain = 0;

        dmSound::Result r = dmSound::GetGroupGain(group, &gain);
        if (r != dmSound::RESULT_OK) {
            dmLogWarning("Failed to get group gain (%d)", r);
        }
        lua_pushnumber(L, gain);
        assert(top + 1 == lua_gettop(L));
        return 0;
    }

    /*#
     * Get all mixer group names
     * <p>Note that this function is only guaranteed to work in dev-mode</p>
     *
     * @return table of mixer groups names
     */
    int Sound_GetGroupNames(lua_State* L)
    {
        int top = lua_gettop(L);

        uint32_t count = dmSound::GetGroupCount();
        lua_createtable(L, count, 0);
        for (uint32_t i = 0; i < count; i++) {
            const char* name = 0;
            dmSound::GetGroupName(i, &name);
            lua_pushstring(L, name);
            lua_rawseti(L, -2, i + 1);
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
        {"get_group_names", Sound_GetGroupNames},
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
