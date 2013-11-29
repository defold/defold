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

    static const luaL_reg SOUND_FUNCTIONS[] =
    {
        {"is_music_playing", Sound_IsMusicPlaying},
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
