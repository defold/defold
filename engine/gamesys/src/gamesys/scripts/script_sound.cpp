#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <sound/sound.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

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
     * @document
     * @name Sound
     * @namespace sound
     */

    /*# check if background music is playing
     * Checks if background music is playing, e.g. from iTunes.
     *
     * [icon:macOS][icon:windows][icon:linux][icon:html5] On non mobile platforms,
     * this function always return `false`.
     *
     * @name sound.is_music_playing
     * @return playing [type:boolean] `true` if music is playing, otherwise `false`.
     * @examples
     *
     * If music is playing, mute "master":
     *
     * ```lua
     * if sound.is_music_playing() then
     *     -- mute "master"
     *     sound.set_group_gain("master", 0)
     * end
     * ```
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

    /*# get RMS value from mixer group
     * Get RMS (Root Mean Square) value from mixer group. This value is the
     * square root of the mean (average) value of the squared function of
     * the instantaneous values.
     *
     * For instance: for a sinewave signal with a peak gain of -1.94 dB (0.8 linear),
     * the RMS is <code>0.8 &times; 1/sqrt(2)</code> which is about 0.566.
     *
     * [icon:attention] Note the returned value might be an approximation and in particular
     * the effective window might be larger than specified.
     *
     * @param group [type:string|hash] group name
     * @param window [type:number] window length in seconds
     * @name sound.get_rms
     * @return rms_l [type:number] RMS value for left channel
     * @return rms_r [type:number] RMS value for right channel
     * @examples
     *
     * Get the RMS from the "master" group where a mono -1.94 dB sinewave is playing:
     *
     * ```lua
     * local rms = sound.get_rms("master", 0.1) -- throw away right channel.
     * print(rms) --> 0.56555819511414
     * ```
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
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     * Also note that the returned value might be an approximation and in particular
     * the effective window might be larger than specified.
     *
     * @param group [type:string|hash] group name
     * @param window [type:number] window length in seconds
     * @name sound.get_peak
     * @return peak_l [type:number] peak value for left channel
     * @return peak_r [type:number] peak value for right channel
     * @examples
     *
     * Get the peak gain from the "master" group and convert to dB for displaying:
     *
     * ```lua
     * local left_p, right_p = sound.get_peak("master", 0.1)
     * left_p_db = 20 * log(left_p)
     * right_p_db = 20 * log(right_p)
     * ```
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
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @param group [type:string|hash] group name
     * @param gain [type:number] gain in linear scale
     * @name sound.set_group_gain
     * @examples
     *
     * Set mixer group gain on the "soundfx" group to -4 dB:
     *
     * ```lua
     * local gain_db = -4
     * local gain = 10^gain_db/20 -- 0.63095734448019
     * sound.set_group_gain("soundfx", gain)
     * ```
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
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @param group [type:string|hash] group name
     * @name sound.get_group_gain
     * @return gain [type:number] gain in linear scale
     * @examples
     *
     * Get the mixer group gain for the "soundfx" and convert to dB:
     *
     * ```lua
     * local gain = sound.get_group_gain("soundfx")
     * local gain_db = 20 * log(gain)
     * ```
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
     * Get a table of all mixer group names (hashes).
     *
     * @name sound.get_groups
     * @return groups [type:table] table of mixer group names
     * @examples
     *
     * Get the mixer groups, set all gains to 0 except for "master" and "soundfx"
     * where gain is set to 1:
     *
     * ```lua
     * local groups = sound.get_groups()
     * for _,group in ipairs(groups) do
     *     if group == hash("master") or group == hash("soundfx") then
     *         sound.set_group_gain(group, 1)
     *     else
     *         sound.set_group_gain(group, 0)
     *     end
     * end
     * ```
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
     *
     * [icon:attention] This function is to be used for debugging and
     * development tooling only. The function does a reverse hash lookup, which does not
     * return a proper string value when the game is built in release mode.
     *
     * @name sound.get_group_name
     * @param group [type:string|hash] group name
     * @return name [type:string] group name
     * @examples
     *
     * Get the mixer group string names so we can show them as labels on a dev mixer overlay:
     *
     * ```lua
     * local groups = sound.get_groups()
     * for _,group in ipairs(groups) do
     *     local name = sound.get_group_name(group)
     *     msg.post("/mixer_overlay#gui", "set_mixer_label", { group = group, label = name})
     * end
     * ```
     */
    int Sound_GetGroupName(lua_State* L)
    {
        int top = lua_gettop(L);

        dmhash_t group_hash = dmScript::CheckHash(L, 1);
        const char* name = (const char*) dmHashReverse64(group_hash, 0);
        if (name) {
            lua_pushstring(L, name);
        } else {
            lua_pushfstring(L, "unknown_%llu", (unsigned long long)group_hash);
        }

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# check if a phone call is active
     * Checks if a phone call is active. If there is an active phone call all
     * other sounds will be muted until the phone call is finished.
     *
     * [icon:macOS][icon:windows][icon:linux][icon:html5] On non mobile platforms,
     * this function always return `false`.
     *
     * @name sound.is_phone_call_active
     * @return call_active [type:boolean] `true` if there is an active phone call, `false` otherwise.
     * @examples
     *
     * Test if a phone call is on-going:
     *
     * ```lua
     * if sound.is_phone_call_active() then
     *     -- do something sensible.
     * end
     * ```
     */
    int Sound_IsPhoneCallActive(lua_State* L)
    {
        int top = lua_gettop(L);
        lua_pushboolean(L, (int) dmSound::IsPhoneCallActive());
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# plays a sound
     * Make the spound component play its sound. Multiple voices is support. The limit is set to 32 voices per sound component.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @name sound.play_sound
     * @param url [type:string|hash|url] the sound that should play
     * @param [delay] [type:number] delay in seconds before the sound starts playing, default is 0.
     * @param [gain] [type:number] sound gain between 0 and 1, default is 1.
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component play its sound after 1 second:
     *
     * ```lua
     * sound.play_sound("#sound", delay = 1, gain = 0.5)
     * ```
     */
    int Sound_PlaySound(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        dmGameObject::HInstance instance = CheckGoInstance(L);
        float delay = luaL_checknumber(L, 2);
        float gain = luaL_checknumber(L, 3);

        dmGameSystemDDF::PlaySound msg;
        msg.m_Delay = delay;
        msg.m_Gain = gain;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PlaySound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::PlaySound::m_DDFDescriptor, &msg, sizeof(msg), 0);
    }

    /*# stop a playing a sound(s)
     * Stop playing all active voices
     *
     * @name sound.stop_sound
     * @param url [type:string|hash|url] the sound that should stop
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component stop all playing voices:
     *
     * ```lua
     * sound.stop_sound("#sound")
     * ```
     */
    int Sound_StopSound(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmGameSystemDDF::StopSound msg;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::StopSound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::StopSound::m_DDFDescriptor, &msg, sizeof(msg), 0);
    }

    /*# set sound gain
     * Set gain on all active playing voices of a sound.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @message
     * @name sound.set_gain
     * @param url [type:string|hash|url] the sound to set the gain of
     * @param [gain] [type:number] sound gain between 0 and 1, default is 1.
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will set the gain to 0.5
     *
     * ```lua
     * sound.set_gain("#sound", gain = 0.5)
     * ```
     */
    int Sound_SetGain(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t id_hash = dmScript::CheckHashOrString(L, 2);
        float gain = luaL_checknumber(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmGameSystemDDF::SetGain msg;
        msg.m_Gain = gain;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetGain::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetGain::m_DDFDescriptor, &msg, sizeof(msg), 0);
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
        {"is_phone_call_active", Sound_IsPhoneCallActive},
        {"play_sound", Sound_PlaySound},
        {"stop_sound", Sound_StopSound},
        {"set_gain", Sound_SetGain},
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
