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
    void EmitterStateChangedCallback(dmhash_t component_id, dmhash_t emitter_id, int emitter_state, int lua_callback_ref, lua_State* L)
    {
        int arg_count = 4;

        lua_rawgeti(L, LUA_REGISTRYINDEX, lua_callback_ref);

        lua_pushvalue(L, -2); //self
        lua_pushnumber(L, component_id);
        lua_pushnumber(L, emitter_id);
        lua_pushnumber(L, emitter_state);

        int ret = dmScript::PCall(L, arg_count, LUA_MULTRET);
        //int ret = lua_pcall(L, arg_count, 0, 0);
        if(ret != 0)
        {
            dmLogError("error calling particle emitter callback, error: %s", lua_tostring(L, -1));
        }

        //unref lua function ref?
        //lua_unref(L, LUA_REGISTRYINDEX, lua_callback_ref);

        // Reset count of lua stack? Push nil values?
    }

    

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
        //g_L = L;
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        if (top < 1)
        {
            return luaL_error(L, "particlefx.play expects atleast URL as parameter");
        }
        
        //dmGameSystemDDF::PlayParticleFX msg;
        //uint32_t msg_size = sizeof(dmGameSystemDDF::PlayParticleFX);
        char* msg_buf = 0x0;
        uint32_t msg_size = 0;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        sender.m_Function = 0;
        receiver.m_Function = 0;

        dmParticle::EmitterStateChanged fun;

        // TODO extract cb function ref and attach it to a buffer containing both msg and cb function?
        // See script_http.cpp Http_Request(...) hos they read to var "int callback" (row 80). I think the
        // reference to the LUA cb is attached to the URL obj ("sender" in script_http.cpp). Decoded in http_service.cpp:245.

        // Second arg is ref to lua callback func for changed emitter state, append ref to sender object
        if (top > 1 && !lua_isnil(L, 2))
        {
            // NOTE: + 2 as LUA_NOREF is defined as - 2 and 0 is interpreted as uninitialized (see script_http.cpp:80)
            int callback = luaL_ref(L, LUA_REGISTRYINDEX); // pops value from lua stack
            lua_pushnil(L); // push nil to lua stack to restore stack size (necessary? or just ditch the assert below?)

            fun = EmitterStateChangedCallback;
            /*
            dmLogInfo("1 Address to fun: %p", fun);
            dmLogInfo("1 Extracted callback ref = %i", callback);
            dmLogInfo("1 Address to lua state: %p", L);
            */
            msg_size = sizeof(dmParticle::EmitterStateChanged) + sizeof(int) + sizeof(&L);
            msg_buf = new char[msg_size];
            memcpy(msg_buf, &fun, sizeof(dmParticle::EmitterStateChanged));
            memcpy(msg_buf + sizeof(dmParticle::EmitterStateChanged), &callback, sizeof(int));
            memcpy(msg_buf + sizeof(dmParticle::EmitterStateChanged) + sizeof(int), &L, sizeof(&L));
            
            //fun(555, 1337, 1, callback);
        }
        
        // Debugging, read funcptr and cb ref from buffer before posting
        if(msg_buf != 0x0)
        {
            dmParticle::EmitterStateChangedData data;
            memcpy(&(data.m_StateChangedCallback), msg_buf, sizeof(dmParticle::EmitterStateChanged));
            data.m_LuaCallbackRef = *(int*)(msg_buf + sizeof(dmParticle::EmitterStateChanged));
            memcpy(&(data.m_L), msg_buf + sizeof(dmParticle::EmitterStateChanged) + sizeof(int), sizeof(lua_State*));
            /*
            dmLogInfo("-------");
            dmLogInfo("1 BUF address to func ptr from buf: %p", data.m_StateChangedCallback);
            dmLogInfo("1 BUF cb ref from buf: %i", data.m_LuaCallbackRef);
            dmLogInfo("1 BUF Address to lua state from buf: %p", data.m_L);
            */
            //data.m_StateChangedCallback(1,2,3,data.m_LuaCallbackRef, data.m_L);
        }

        dmMessage::Post(
            &sender, 
            &receiver, 
            dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor->m_NameHash, 
            (uintptr_t)instance, 
            (uintptr_t)dmGameSystemDDF::PlayParticleFX::m_DDFDescriptor, 
            (void*)msg_buf, 
            msg_size);

        //delete[] msg_buf;

        assert(top == lua_gettop(L));
        return 0;
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

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::StopParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::StopParticleFX::m_DDFDescriptor, (void*)&msg, msg_size);
        assert(top == lua_gettop(L));
        return 0;
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

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstantParticleFX::m_DDFDescriptor, &msg, sizeof(msg));
        assert(top == lua_gettop(L));
        return 0;
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

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t emitter_id = dmScript::CheckHashOrString(L, 2);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 3);

        dmGameSystemDDF::ResetConstantParticleFX msg;
        msg.m_EmitterId = emitter_id;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstantParticleFX::m_DDFDescriptor, &msg, sizeof(msg));
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
        luaL_register(L, "particlefx", PARTICLEFX_FUNCTIONS);
        lua_pop(L, 1);
    }
}
