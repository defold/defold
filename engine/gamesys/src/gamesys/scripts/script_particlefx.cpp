#include "script_particlefx.h"

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <particle/particle.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "resources/res_particlefx.h"
#include "resources/res_tileset.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# start playing a particle FX
     * Particle FX started this way need to be manually stopped through particlefx.stop.
     * Which particle FX to play is identified by the URL.
     *
     * @name particlefx.play
     * @param url the particle fx that should start playing (url)
     * @examples
     * <p>
     * How to play a particle fx when a game object is created:
     * </p>
     * <pre>
     * function init(self)
     *     particlefx.play("#particlefx")
     * end
     * </pre>
     */
    int ParticleFX_Play(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            if (top != 1)
            {
                return luaL_error(L, "particlefx.play only takes a URL as parameter");
            }
            dmGameSystemDDF::PlayParticleFX msg;
            uint32_t msg_size = sizeof(dmGameSystemDDF::PlayParticleFX);

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor, (void*)&msg, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "particlefx.play is not available from this script-type.");
        }
    }

    /*# stop playing a particle fx
     * Stopping a particle FX does not remove the already spawned particles.
     * Which particle fx to stop is identified by the URL.
     *
     * @name particlefx.stop
     * @param url the particle fx that should stop playing (url)
     * @examples
     * <p>
     * How to stop a particle fx when a game object is deleted:
     * </p>
     * <pre>
     * function final(self)
     *     particlefx.stop("#particlefx")
     * end
     * </pre>
     */
    int ParticleFX_Stop(lua_State* L)
    {
        int top = lua_gettop(L);

        dmMessage::URL sender;
        uintptr_t user_data;
        if (dmScript::GetURL(L, &sender) && dmScript::GetUserData(L, &user_data) && user_data != 0)
        {
            if (top != 1)
            {
                return luaL_error(L, "particlefx.stop only takes a URL as parameter");
            }
            dmGameSystemDDF::StopParticleFX msg;
            uint32_t msg_size = sizeof(dmGameSystemDDF::StopParticleFX);

            dmMessage::URL receiver;

            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::StopParticleFX::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::StopParticleFX::m_DDFDescriptor, (void*)&msg, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "particlefx.stop is not available from this script-type.");
        }
    }

    static const luaL_reg PARTICLEFX_FUNCTIONS[] =
    {
        {"play",            ParticleFX_Play},
        {"stop",            ParticleFX_Stop},
        {0, 0}
    };

    void ScriptParticleFXRegister(void* context)
    {
        lua_State* L = (lua_State*)context;
        luaL_register(L, "particlefx", PARTICLEFX_FUNCTIONS);
        lua_pop(L, 1);
    }
}
