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
    /*# Particle effects API documentation
     *
     * Functions for controlling particle effect component playback and
     * shader constants.
     *
     * @document
     * @name Particle effects
     * @namespace particlefx
     */

    /*# sleeping state
     * The emitter does not have any living particles and will not spawn any particles in this state.
     *
     * @name particlefx.EMITTER_STATE_SLEEPING
     * @variable
     */

    /*# prespawn state
     * The emitter will be in this state when it has been started but before spawning any particles. Normally the emitter is in this state for a short time, depending on if a start delay has been set for this emitter or not.
     *
     * @name particlefx.EMITTER_STATE_PRESPAWN
     * @variable
     */

    /*# spawning state
     * The emitter is spawning particles.
     *
     * @name particlefx.EMITTER_STATE_SPAWNING
     * @variable
     */

    /*# postspawn state
     * The emitter is not spawning any particles, but has particles that are still alive.
     *
     * @name particlefx.EMITTER_STATE_POSTSPAWN
     * @variable
     */

    void EmitterStateChangedCallback(uint32_t num_awake_emitters, dmhash_t emitter_id, dmParticle::EmitterState emitter_state, void* user_data)
    {
        EmitterStateChangedScriptData data = *(EmitterStateChangedScriptData*)(user_data);

        if (data.m_CallbackInfo)
        {
            if (!dmScript::IsValidCallback(data.m_CallbackInfo))
            {
                dmLogError("Could not run particlefx callback because the instance has been deleted.");
                return;
            }

            struct Args
            {
                Args(dmhash_t component_id, dmhash_t emitter_id, dmParticle::EmitterState emitter_state)
                    : m_ComponentId(component_id), m_EmitterId(emitter_id), m_EmitterState(emitter_state)
                {}
                dmhash_t m_ComponentId;
                dmhash_t m_EmitterId;
                dmParticle::EmitterState m_EmitterState;

                static void LuaCallbackCustomArgs(lua_State* L, void* user_args)
                {
                    Args* args = (Args*)user_args;
                    dmScript::PushHash(L, args->m_ComponentId);
                    dmScript::PushHash(L, args->m_EmitterId);
                    lua_pushnumber(L, args->m_EmitterState);
                }
            };

            Args args(data.m_ComponentId, emitter_id, emitter_state);

            if (!dmScript::InvokeCallback(data.m_CallbackInfo, Args::LuaCallbackCustomArgs, &args))
            {
                dmLogError("Could not run particlefx callback because the instance has been deleted.");
            }

            // The last emitter belonging to this particlefx har gone to sleep, release lua reference.
            if(num_awake_emitters == 0 && emitter_state == dmParticle::EMITTER_STATE_SLEEPING)
            {
                dmScript::DeleteCallback(data.m_CallbackInfo);
                data.m_CallbackInfo = 0x0;
            }
        }
        else
        {
            dmLogError("No callback set for particlefx.");
        }
    }

    /*# start playing a particle FX
     * Starts playing a particle FX component.
     * Particle FX started this way need to be manually stopped through `particlefx.stop()`.
     * Which particle FX to play is identified by the URL.
     *
     * @name particlefx.play
     * @param url [type:string|hash|url] the particle fx that should start playing.
     * @param [emitter_state_function] [type:function(self, id, emitter, state)] optional callback function that will be called when an emitter attached to this particlefx changes state.
     *
     * `self`
     * : [type:object] The current object
     *
     * `id`
     * : [type:hash] The id of the particle fx component
     *
     * `emitter`
     * : [type:hash] The id of the emitter
     *
     * `state`
     * : [type:constant] the new state of the emitter:
     *
     * - `particlefx.EMITTER_STATE_SLEEPING`
     * - `particlefx.EMITTER_STATE_PRESPAWN`
     * - `particlefx.EMITTER_STATE_SPAWNING`
     * - `particlefx.EMITTER_STATE_POSTSPAWN`
     *
     * @examples
     *
     * How to play a particle fx when a game object is created.
     * The callback receives the hash of the path to the particlefx, the hash of the id
     * of the emitter, and the new state of the emitter as particlefx.EMITTER_STATE_<STATE>.
     *
     * ```lua
     * local function emitter_state_change(self, id, emitter, state)
     *   if emitter == hash("exhaust") and state == particlefx.EMITTER_STATE_POSTSPAWN then
     *     -- exhaust is done spawning particles...
     *   end
     * end
     *
     * function init(self)
     *     particlefx.play("#particlefx", emitter_state_change)
     * end
     * ```
     */
    int ParticleFX_Play(lua_State* L)
    {
        dmGameObject::HInstance instance = CheckGoInstance(L);

        int top = lua_gettop(L);

        if (top < 1)
        {
            return luaL_error(L, "particlefx.play expects atleast URL as parameter");
        }

        DM_LUA_STACK_CHECK(L, 0);

        EmitterStateChangedScriptData data;
        char msg_buf[sizeof(dmParticle::EmitterStateChanged) + sizeof(EmitterStateChangedScriptData)];
        uint32_t msg_size = 0;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        sender.m_Function = 0;
        receiver.m_Function = 0;

        if (top > 1 && !lua_isnil(L, 2))
        {
            data.m_CallbackInfo = dmScript::CreateCallback(dmScript::GetMainThread(L), -1);
            if (data.m_CallbackInfo == 0x0)
            {
                return luaL_error(L, "particlefx.play failed to crate callback");
                return 0;
            }

            // path-only url (e.g. "/level/particlefx") has empty fragment, and relative path (e.g. "#particlefx") has non-empty fragment.
            if(receiver.m_Fragment == 0)
            {
                data.m_ComponentId = receiver.m_Path;
            }
            else
            {
                data.m_ComponentId = receiver.m_Fragment;
            }

            dmParticle::EmitterStateChanged fun;
            fun = EmitterStateChangedCallback;

            msg_size = sizeof(dmParticle::EmitterStateChanged) + sizeof(EmitterStateChangedScriptData);

            memcpy(msg_buf, &fun, sizeof(dmParticle::EmitterStateChanged));
            memcpy(msg_buf + sizeof(dmParticle::EmitterStateChanged), &data, sizeof(EmitterStateChangedScriptData));
        }

        dmMessage::Post(
            &sender,
            &receiver,
            dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash,
            (uintptr_t)instance,
            (uintptr_t)dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor,
            (void*)msg_buf,
            msg_size,
            0);

        return 0;
    }

    /*# stop playing a particle fx
     * Stops a particle FX component from playing.
     * Stopping a particle FX does not remove already spawned particles.
     * Which particle FX to stop is identified by the URL.
     *
     * @name particlefx.stop
     * @param url [type:string|hash|url] the particle fx that should stop playing
     * @examples
     *
     * How to stop a particle fx when a game object is deleted:
     *
     * ```lua
     * function final(self)
     *     particlefx.stop("#particlefx")
     * end
     * ```
     */
    int ParticleFX_Stop(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        if (top != 1)
        {
            return luaL_error(L, "particlefx.stop only takes a URL as parameter");
        }
        dmGameSystemDDF::StopParticleFX msg;
        uint32_t msg_size = sizeof(dmGameSystemDDF::StopParticleFX);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::StopParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::StopParticleFX::m_DDFDescriptor, (void*)&msg, msg_size, 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a shader constant for a particle FX component emitter
     * Sets a shader constant for a particle FX component emitter.
     * The constant must be defined in the material assigned to the emitter.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until particlefx.reset_constant is called.
     * Which particle FX to set a constant for is identified by the URL.
     *
     * @name particlefx.set_constant
     * @param url [type:string|hash|url] the particle FX that should have a constant set
     * @param emitter [type:string|hash] the id of the emitter
     * @param constant [type:string|hash] the name of the constant
     * @param value [type:vector4] the value of the constant
     * @examples
     *
     * The following examples assumes that the particle FX has id "particlefx", it
     * contains an emitter with the id "emitter" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     *
     * How to tint particles from an emitter red:
     *
     * ```lua
     * function init(self)
     *     particlefx.set_constant("#particlefx", "emitter", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * ```
     */
    int ParticleFX_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t emitter_id = dmScript::CheckHashOrString(L, 2);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 3);
        Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 4);

        dmGameSystemDDF::SetConstantParticleFX msg;
        msg.m_EmitterId = emitter_id;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a particle FX component emitter
     * Resets a shader constant for a particle FX component emitter.
     * The constant must be defined in the material assigned to the emitter.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which particle FX to reset a constant for is identified by the URL.
     *
     * @name particlefx.reset_constant
     * @param url [type:string|hash|url] the particle FX that should have a constant reset
     * @param emitter [type:string|hash] the id of the emitter
     * @param constant [type:string|hash] the name of the constant
     * @examples
     *
     * The following examples assumes that the particle FX has id "particlefx", it
     * contains an emitter with the id "emitter" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     *
     * How to reset the tinting of particles from an emitter:
     *
     * ```lua
     * function init(self)
     *     particlefx.reset_constant("#particlefx", "emitter", "tint")
     * end
     * ```
     */
    int ParticleFX_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t emitter_id = dmScript::CheckHashOrString(L, 2);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 3);

        dmGameSystemDDF::ResetConstantParticleFX msg;
        msg.m_EmitterId = emitter_id;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
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
        int top = lua_gettop(L);
        luaL_register(L, "particlefx", PARTICLEFX_FUNCTIONS);

        #define SETCONSTANT(name, val) \
            lua_pushnumber(L, (lua_Number) val); \
            lua_setfield(L, -2, #name);\

        SETCONSTANT(EMITTER_STATE_SLEEPING, dmParticle::EMITTER_STATE_SLEEPING);
        SETCONSTANT(EMITTER_STATE_PRESPAWN, dmParticle::EMITTER_STATE_PRESPAWN);
        SETCONSTANT(EMITTER_STATE_SPAWNING, dmParticle::EMITTER_STATE_SPAWNING);
        SETCONSTANT(EMITTER_STATE_POSTSPAWN, dmParticle::EMITTER_STATE_POSTSPAWN);

        #undef SETCONSTANT

        // pop table "particle_fx"
        lua_pop(L, 1);
        assert(top == lua_gettop(L));
    }
}
