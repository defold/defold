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

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <sound/sound.h>

#include "gamesys.h"
#include <gamesys/gamesys_ddf.h>
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


    /*# [type:number] sound gain
     *
     * The gain on the sound-component. Note that gain is in linear scale,
     * between 0 and 1.
     *
     * @name gain
     * @property
     *
     * @examples
     *
     * ```lua
     * function init(self)
     *   local gain = go.get("#sound", "gain")
     *   go.set("#sound", "gain", gain * 1.5)
     * end
     * ```
     */

    /*# [type:number] sound pan
     *
     * The pan on the sound-component. The valid range is from -1.0 to 1.0,
     * representing -45 degrees left, to +45 degrees right.
     *
     * @name pan
     * @property
     *
     * @examples
     *
     * ```lua
     * function init(self)
     *   local pan = go.get("#sound", "pan")
     *   go.set("#sound", "pan", pan * -1)
     * end
     * ```
     */

    /*# [type:number] sound speed
     *
     * The speed on the sound-component where 1.0 is normal speed, 0.5 is half
     * speed and 2.0 is double speed.
     *
     * @name speed
     * @property
     *
     * @examples
     *
     * ```lua
     * function init(self)
     *   local speed = go.get("#sound", "speed")
     *   go.set("#sound", "speed", speed * 0.5)
     * end
     * ```
     */

    /*# [type:hash] sound data
     *
     * The sound data used when playing the sound. The type of the property is hash.
     *
     * @name sound
     * @property
     *
     * @examples
     *
     * How to change the sound:
     *
     * ```lua
     * function init(self)
     *   -- load a wav file bundled as a custom resource
     *   local wav = sys.load_resource("foo.wav")
     *   -- get resource path to the sound component
     *   local resource_path = go.get("#sound", "sound")
     *   -- update the resource with the loaded wav file
     *   resource.set_sound(resource_path, wav)
     *   -- play the updated sound
     *   sound.play("#sound")
     * end
     * ```
     */

    /*# check if background music is playing
     * Checks if background music is playing, e.g. from iTunes.
     *
     * [icon:macOS][icon:windows][icon:linux][icon:html5] On non mobile platforms,
     * this function always return `false`.
     *
     * [icon:attention][icon:android] On Android you can only get a correct reading
     * of this state if your game is not playing any sounds itself. This is a limitation
     * in the Android SDK. If your game is playing any sounds, *even with a gain of zero*, this
     * function will return `false`.
     *
     * The best time to call this function is:
     *
     * - In the `init` function of your main collection script before any sounds are triggered
     * - In a window listener callback when the window.WINDOW_EVENT_FOCUS_GAINED event is received
     *
     * Both those times will give you a correct reading of the state even when your application is
     * swapped out and in while playing sounds and it works equally well on Android and iOS.
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
    static int Sound_IsMusicPlaying(lua_State* L)
    {
        lua_pushboolean(L, (int) dmSound::IsMusicPlaying());
        return 1;
    }

    static dmhash_t CheckGroupName(lua_State* L, int index) {
        return dmScript::CheckHashOrString(L, index);
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
    static int Sound_GetRMS(lua_State* L)
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
    static int Sound_GetPeak(lua_State* L)
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
    static int Sound_SetGroupGain(lua_State* L)
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
    static int Sound_GetGroupGain(lua_State* L)
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
    static int Sound_GetGroups(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);

        dmhash_t groups[dmSound::MAX_GROUPS];
        uint32_t count = dmSound::MAX_GROUPS;
        dmSound::GetGroupHashes(&count, groups);

        lua_createtable(L, count, 0);
        for (uint32_t i = 0; i < count; i++) {
            dmScript::PushHash(L, groups[i]);
            lua_rawseti(L, -2, i + 1);
        }
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
    static int Sound_GetGroupName(lua_State* L)
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
    static int Sound_IsPhoneCallActive(lua_State* L)
    {
        int top = lua_gettop(L);
        lua_pushboolean(L, (int) dmSound::IsAudioInterrupted());
        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# plays a sound
     * Make the sound component play its sound. Multiple voices are supported. The limit is set to 32 voices per sound component.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * [icon:attention] A sound will continue to play even if the game object the sound component belonged to is deleted. You can call `sound.stop()` to stop the sound.
     *
     * @note Sounds are panned using a constant power panning (non linear fade). 0 means left/right channels are balanced at 71%/71% each.
     * At -1 (full left) the channels are at 100%/0%, and 1 they're at 0%/100%.
     *
     * @name sound.play
     * @param url [type:string|hash|url] the sound that should play
     * @param [play_properties] [type:table] optional table with properties:
     * `delay`
     * : [type:number] delay in seconds before the sound starts playing, default is 0.
     *
     * `gain`
     * : [type:number] sound gain between 0 and 1, default is 1. The final gain of the sound will be a combination of this gain, the group gain and the master gain.
     *
     * `pan`
     * : [type:number] sound pan between -1 and 1, default is 0. The final pan of the sound will be an addition of this pan and the sound pan.
     *
     * `speed`
     * : [type:number] sound speed where 1.0 is normal speed, 0.5 is half speed and 2.0 is double speed. The final speed of the sound will be a multiplication of this speed and the sound speed.
     *
     * @param [complete_function] [type:function(self, message_id, message, sender))] function to call when the sound has finished playing.
     *
     * `self`
     * : [type:object] The current object.
     *
     * `message_id`
     * : [type:hash] The name of the completion message, `"sound_done"`.
     *
     * `message`
     * : [type:table] Information about the completion:
     *
     * - [type:number] `play_id` - the sequential play identifier that was given by the sound.play function.
     *
     * `sender`
     * : [type:url] The invoker of the callback: the sound component.
     *
     * @return id [type:number] The identifier for the sound voice
     *
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component play its sound after 1 second:
     *
     * ```lua
     * sound.play("#sound", { delay = 1, gain = 0.5, pan = -1.0 } )
     * ```
     *
     * Using the callback argument, you can chain several sounds together:
     *
     * ```lua
     * local function sound_done(self, message_id, message, sender)
     *   -- play 'boom' sound fx when the countdown has completed
     *   if message_id == hash("sound_done") and message.play_id == self.countdown_id then
     *     sound.play("#boom", nil, sound_done)
     *   end
     * end
     *
     * function init(self)
     *   self.countdown_id = sound.play("#countdown", nil, sound_done)
     * end
     * ```
     */
    static int Sound_Play(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);
        float delay = 0.0f, gain = 1.0f, pan = 0.0f, speed = 1.0f;
        uint32_t play_id = dmSound::INVALID_PLAY_ID;

        if (top > 1 && !lua_isnil(L,2)) // table with args
        {
            luaL_checktype(L, 2, LUA_TTABLE);
            lua_pushvalue(L, 2);

            lua_getfield(L, -1, "delay");
            delay = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "gain");
            gain = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "pan");
            pan = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "speed");
            speed = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        int functionref = 0;
        if (top > 2) // completed cb
        {
            if (lua_isfunction(L, 3))
            {
                lua_pushvalue(L, 3);
                play_id = dmSound::GetAndIncreasePlayCounter();
                // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, in order to have 0 for "no function"
                functionref = dmScript::RefInInstance(L) - LUA_NOREF;
            }
        }

        dmGameSystemDDF::PlaySound msg;
        msg.m_Delay  = delay;
        msg.m_Gain   = gain;
        msg.m_Pan    = pan;
        msg.m_Speed = speed;
        msg.m_PlayId = play_id;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PlaySound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)functionref, (uintptr_t)dmGameSystemDDF::PlaySound::m_DDFDescriptor, &msg, sizeof(msg), 0);

        lua_pushnumber(L, (double) msg.m_PlayId);

        return 1;
    }

    /*# stop a playing a sound(s)
     * Stop playing all active voices
     *
     * @name sound.stop
     * @param url [type:string|hash|url] the sound that should stop
     *
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component stop all playing voices:
     *
     * ```lua
     * sound.stop("#sound")
     * ```
     */
    static int Sound_Stop(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmGameSystemDDF::StopSound msg;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::StopSound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::StopSound::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    /*# pause a playing a sound(s)
     * Pause all active voices
     *
     * @name sound.pause
     * @param url [type:string|hash|url] the sound that should pause
     * @param pause [type:bool] true if the sound should pause
     *
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will make the component pause all playing voices:
     *
     * ```lua
     * sound.pause("#sound", true)
     * ```
     */
    static int Sound_Pause(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmGameSystemDDF::PauseSound msg;
        msg.m_Pause = dmScript::CheckBoolean(L, 2);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PauseSound::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::PauseSound::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    /*# set sound gain
     * Set gain on all active playing voices of a sound.
     *
     * [icon:attention] Note that gain is in linear scale, between 0 and 1.
     * To get the dB value from the gain, use the formula `20 * log(gain)`.
     * Inversely, to find the linear value from a dB value, use the formula
     * <code>10<sup>db/20</sup></code>.
     *
     * @name sound.set_gain
     * @param url [type:string|hash|url] the sound to set the gain of
     * @param [gain] [type:number] sound gain between 0 and 1. The final gain of the sound will be a combination of this gain, the group gain and the master gain.
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will set the gain to 0.5
     *
     * ```lua
     * sound.set_gain("#sound", 0.5)
     * ```
     */
    static int Sound_SetGain(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        float gain = luaL_checknumber(L, 2);

        dmGameSystemDDF::SetGain msg;
        msg.m_Gain = gain;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetGain::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetGain::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    /*# set sound pan
     * Set panning on all active playing voices of a sound.
     *
     * The valid range is from -1.0 to 1.0, representing -45 degrees left, to +45 degrees right.
     *
     * @note Sounds are panned using a constant power panning (non linear fade). 0 means left/right channels are balanced at 71%/71% each.
     * At -1 (full left) the channels are at 100%/0%, and 1 they're at 0%/100%.
     *
     * @name sound.set_pan
     * @param url [type:string|hash|url] the sound to set the panning value to
     * @param [pan] [type:number] sound panning between -1.0 and 1.0
     * @examples
     *
     * Assuming the script belongs to an instance with a sound-component with id "sound", this will set the gain to 0.5
     *
     * ```lua
     * sound.set_pan("#sound", 0.5) -- pan to the right
     * ```
     */
    static int Sound_SetPan(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        float pan = luaL_checknumber(L, 2);

        dmGameSystemDDF::SetPan msg;
        msg.m_Pan = pan;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetPan::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetPan::m_DDFDescriptor, &msg, sizeof(msg), 0);
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
        {"play", Sound_Play},
        {"stop", Sound_Stop},
        {"pause", Sound_Pause},
        {"set_gain", Sound_SetGain},
        {"set_pan", Sound_SetPan},
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

    void ScriptSoundOnWindowFocus(bool focus)
    {
        dmSound::OnWindowFocus(focus);
    }
}
