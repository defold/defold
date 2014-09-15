#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <particle/particle.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "../gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"

#include "resources/res_particlefx.h"

#include "script_particlefx.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# ParticleFX documentation
     *
     * ParticleFX documentation
     * @name
     * @package
     */

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

        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data, dmGameObject::SCRIPT_INSTANCE_TYPE_NAME) && user_data != 0)
        {
            if (top != 1)
            {
                return luaL_error(L, "particlefx.play only takes a URL as parameter");
            }
            dmGameSystemDDF::PlayParticleFX msg;
            uint32_t msg_size = sizeof(dmGameSystemDDF::PlayParticleFX);

            dmMessage::URL receiver;
            dmMessage::URL sender;
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

        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data, dmGameObject::SCRIPT_INSTANCE_TYPE_NAME) && user_data != 0)
        {
            if (top != 1)
            {
                return luaL_error(L, "particlefx.stop only takes a URL as parameter");
            }
            dmGameSystemDDF::StopParticleFX msg;
            uint32_t msg_size = sizeof(dmGameSystemDDF::StopParticleFX);

            dmMessage::URL receiver;
            dmMessage::URL sender;
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

    static bool GetHash(lua_State* L, int index, dmhash_t* v)
    {
        if (lua_isstring(L, index))
        {
            *v = dmHashString64(lua_tostring(L, index));
        }
        else if (dmScript::IsHash(L, index))
        {
            *v = dmScript::CheckHash(L, index);
        }
        else
        {
            return false;
        }
        return true;
    }

    /*# set a shader constant for a particle FX emitter
     * The constant must be defined in the material assigned to the emitter.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until particlefx.reset_constant is called.
     * Which particle FX to set a constant for is identified by the URL.
     *
     * @name particlefx.set_constant
     * @param url the particle FX that should have a constant set (url)
     * @param emitter_id the id of the emitter (string|hash)
     * @param name the name of the constant (string|hash)
     * @param value the value of the constant (vec4)
     * @examples
     * <p>
     * The following examples assumes that the particle FX has id "particlefx", contains an emitter with id "emitter" and that the default-material in builtins is used.
     * If you assign a custom material to the emitter, you can set the constants defined there in the same manner.
     * </p>
     * <p>
     * How to tint particles from an emitter red:
     * </p>
     * <pre>
     * function init(self)
     *     particlefx.set_constant("#particlefx", "emitter", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * </pre>
     */
    int ParticleFX_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data, dmGameObject::SCRIPT_INSTANCE_TYPE_NAME) && user_data != 0)
        {
            dmhash_t emitter_id;
            if (!GetHash(L, 2, &emitter_id))
                return luaL_error(L, "emitter_id must be either a hash or a string");

            dmhash_t name_hash;
            if (!GetHash(L, 3, &name_hash))
                return luaL_error(L, "name must be either a hash or a string");

            Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 4);

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::SetConstantParticleFX* request = (dmGameSystemDDF::SetConstantParticleFX*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::SetConstantParticleFX);

            request->m_EmitterId = emitter_id;
            request->m_NameHash = name_hash;
            request->m_Value = *value;

            dmMessage::URL receiver;
            dmMessage::URL sender;
            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "particlefx.set_constant is not available from this script-type.");
        }
    }

    /*# reset a shader constant for a particle FX emitter
     * The constant must be defined in the material assigned to the emitter.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which particle FX to reset a constant for is identified by the URL.
     *
     * @name particlefx.reset_constant
     * @param url the particle FX that should have a constant reset (url)
     * @param emitter_id the id of the emitter (string|hash)
     * @param name the name of the constant (string|hash)
     * @examples
     * <p>
     * The following examples assumes that the particle FX has id "particlefx", contains an emitter with id "emitter" and that the default-material in builtins is used.
     * If you assign a custom material to the emitter, you can reset the constants defined there in the same manner.
     * </p>
     * <p>
     * How to reset the tinting of particles from an emitter:
     * </p>
     * <pre>
     * function init(self)
     *     particlefx.reset_constant("#particlefx", "emitter", "tint")
     * end
     * </pre>
     */
    int ParticleFX_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        uintptr_t user_data;
        if (dmScript::GetUserData(L, &user_data, dmGameObject::SCRIPT_INSTANCE_TYPE_NAME) && user_data != 0)
        {
            dmhash_t emitter_id;
            if (!GetHash(L, 3, &emitter_id))
                return luaL_error(L, "emitter_id must be either a hash or a string");

            dmhash_t name_hash;
            if (!GetHash(L, 3, &name_hash))
                return luaL_error(L, "name must be either a hash or a string");

            const uint32_t buffer_size = 256;
            uint8_t buffer[buffer_size];
            dmGameSystemDDF::ResetConstantParticleFX* request = (dmGameSystemDDF::ResetConstantParticleFX*)buffer;

            uint32_t msg_size = sizeof(dmGameSystemDDF::ResetConstantParticleFX);

            request->m_EmitterId = emitter_id;
            request->m_NameHash = name_hash;

            dmMessage::URL receiver;
            dmMessage::URL sender;
            dmScript::ResolveURL(L, 1, &receiver, &sender);

            dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor->m_NameHash, user_data, (uintptr_t)dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor, buffer, msg_size);
            assert(top == lua_gettop(L));
            return 0;
        }
        else
        {
            assert(top == lua_gettop(L));
            return luaL_error(L, "particlefx.reset_constant is not available from this script-type.");
        }
    }

    static const luaL_reg PARTICLEFX_FUNCTIONS[] =
    {
        {"play",            ParticleFX_Play},
        {"stop",            ParticleFX_Stop},
        {"set_constant",    ParticleFX_SetConstant},
        {"reset_constant",  ParticleFX_ResetConstant},
        {0, 0}
    };

    void ScriptParticleFXRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "particlefx", PARTICLEFX_FUNCTIONS);
        lua_pop(L, 1);
    }
}
