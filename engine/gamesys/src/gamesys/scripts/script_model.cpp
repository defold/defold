#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"
#include "../components/comp_model.h"

#include "script_model.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

namespace dmGameSystem
{
    /*# Model API documentation
     *
     * Functions and messages for interacting with model components.
     *
     * @name Model
     * @namespace model
     */

    /*# model cursor (number)
     *
     * The normalized animation cursor. The type of the property is number.
     *
     * @name cursor
     * @property
     *
     * @examples
     * <p>
     * How to get the normalized cursor value:
     * </p>
     * <pre>
     * function init(self)
     *  -- Get the cursor value on component "model"
     *  cursor = go.get("#model", "cursor")
     * end
     * </pre>
     * <p>
     * How to animate the cursor from 0.0 to 1.0 using linear easing for 2.0 seconds:
     * </p>
     * <pre>
     * function init(self)
     *  -- Get the current value on component "model"
     *  go.set("#model", "cursor", 0.0)
     *  -- Animate the cursor value
     *  go.animate("#model", "cursor", go.PLAYBACK_LOOP_FORWARD, 1.0, go.EASING_LINEAR, 2)
     * end
     * </pre>
     * <p>Please note that model events may not fire as expected when the cursor is manipulated directly.</p>
     */

    /*# model playback_rate (number)
     *
     * The animation playback rate. A multiplier to the animation playback rate. The type of the property is number.
     *
     * @name playback_rate
     * @property
     *
     * @examples
     * <p>
     * How to set the playback_rate on component "model" to play at double the current speed:
     * </p>
     * <pre>
     * function init(self)
     *  -- Get the current value on component "model"
     *  playback_rate = go.get("#model", "playback_rate")
     *  -- Set the playback_rate to double the previous value.
     *  go.set("#model", "playback_rate", playback_rate * 2)
     * end
     * </pre>
     * <p>The playback_rate is a non-negative number, a negative value will be clamped to 0.</p>
     */

     /*# model animation (hash)
     *
     * The current animation set on the component. The type of the property is hash.
     *
     * @name animation
     * @property
     *
     * @examples
     * <p>
     * How to read the current animation from a model component:
     * </p>
     * <pre>
     * function init(self)
     *  -- Get the current animation on component "model"
     *  local animation = go.get("#model", "animation")
     *  if animation == hash("run_left") then
     *    -- Running left. Do something...
     *  end
     * end
     * </pre>
     */


    int LuaModelComp_Play(lua_State* L)
    {
        int top = lua_gettop(L);
        // default values
        float offset = 0.0f;
        float playback_rate = 1.0f;

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t anim_id = dmScript::CheckHashOrString(L, 2);
        lua_Integer playback = luaL_checkinteger(L, 3);
        lua_Number blend_duration = luaL_checknumber(L, 4);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        if (top > 4)
        {
            if (lua_isfunction(L, 5))
            {
                lua_pushvalue(L, 5);
                // see message.h for why 2 is added
                sender.m_Function = luaL_ref(L, LUA_REGISTRYINDEX) + 2;
            }
        }

        dmModelDDF::ModelPlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmModelDDF::ModelPlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmModelDDF::ModelPlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# play an animation on a model
     *
     * @name model.play_anim
     * @param url the model for which to play the animation (url)
     * @param anim_id id of the animation to play (string|hash)
     * @param playback playback mode of the animation (constant)
     * <ul>
     *   <li><code>go.PLAYBACK_ONCE_FORWARD</code></li>
     *   <li><code>go.PLAYBACK_ONCE_BACKWARD</code></li>
     *   <li><code>go.PLAYBACK_ONCE_PINGPONG</code></li>
     *   <li><code>go.PLAYBACK_LOOP_FORWARD</code></li>
     *   <li><code>go.PLAYBACK_LOOP_BACKWARD</code></li>
     *   <li><code>go.PLAYBACK_LOOP_PINGPONG</code></li>
     * </ul>
     * @param [play_properties] optional table with properties (table)
     * <ul>
     *   <li><code>blend_duration</code> duration of a linear blend between the current and new animation (number)</li>
     *   <li><code>offset</code> the normalized initial value of the animation cursor when the animation starts playing (number)</li>
     *   <li><code>playback_rate</code> the rate with which the animation will be played. Must be positive (number)</li>
     * </ul>
     * @examples
     * <p>
     * The following examples assumes that the model has id "model".
     * </p>
     * <p>
     * How to play the "jump" animation followed by the "run" animation:
     * </p>
     * <pre>
     * function init(self)
     *     local url = msg.url("#model")
     *     local play_properties = { blend_duration = 0.1 }
     *     -- first blend during 0.1 sec into the jump, then during 0.2 s into the run animation
     *     model.play_anim(url, "jump", go.PLAYBACK_ONCE_FORWARD, play_properties, function (self)
     *         local properties = { blend_duration = 0.2 }
     *         model.play_anim(url, "run", go.PLAYBACK_LOOP_FORWARD, properties)
     *     end)
     * end
     * </pre>
     */
    int LuaModelComp_PlayAnim(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0)
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t anim_id = dmScript::CheckHashOrString(L, 2);
        lua_Integer playback = luaL_checkinteger(L, 3);
        lua_Number blend_duration = 0.0, offset = 0.0, playback_rate = 1.0;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        if (top > 3) // table with args
        {
            luaL_checktype(L, 4, LUA_TTABLE);
            lua_pushvalue(L, 4);

            lua_getfield(L, -1, "blend_duration");
            blend_duration = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "offset");
            offset = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "playback_rate");
            playback_rate = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        if (top > 4) // completed cb
        {
            if (lua_isfunction(L, 5))
            {
                lua_pushvalue(L, 5);
                // see message.h for why 2 is added
                sender.m_Function = dmScript::Ref(L, LUA_REGISTRYINDEX) + 2;
            }
        }

        dmModelDDF::ModelPlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmModelDDF::ModelPlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmModelDDF::ModelPlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# cancel all animation on a model
     *
     * @name model.cancel
     * @param url the model for which to cancel the animation (url)
     */
    int LuaModelComp_Cancel(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmModelDDF::ModelCancelAnimation msg;

        dmMessage::Post(&sender, &receiver, dmModelDDF::ModelCancelAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmModelDDF::ModelCancelAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# retrieve the game object corresponding to a model skeleton bone
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity O(n), where n is the number of bones in the model skeleton.
     * Game objects corresponding to a model skeleton bone can not be individually deleted.
     * Only available from .script files.
     *
     * @name model.get_go
     * @param url the model to query (url)
     * @param bone_id id of the corresponding bone (string|hash)
     * @return id of the game object
     * @examples
     * <p>
     * The following examples assumes that the model component has id "model".
     * <p>
     * How to parent the game object of the calling script to the "right_hand" bone of the model in a player game object:
     * </p>
     * <pre>
     * function init(self)
     *     local parent = model.get_go("player#model", "right_hand")
     *     msg.post(".", "set_parent", {parent_id = parent})
     * end
     * </pre>
     */
    int LuaModelComp_GetGO(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        ModelWorld* world = 0;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, MODEL_EXT, &user_data, &receiver, (void**) &world);
        ModelComponent* component = world->m_Components.Get(user_data);
        if (!component || !component->m_Resource->m_RigScene->m_SkeletonRes)
        {
            return luaL_error(L, "the bone '%s' could not be found", lua_tostring(L, 2));
        }

        dmhash_t bone_id = dmScript::CheckHashOrString(L, 2);

        dmRigDDF::Skeleton* skeleton = component->m_Resource->m_RigScene->m_SkeletonRes->m_Skeleton;
        uint32_t bone_count = skeleton->m_Bones.m_Count;
        uint32_t bone_index = ~0u;
        for (uint32_t i = 0; i < bone_count; ++i)
        {
            if (skeleton->m_Bones[i].m_Id == bone_id)
            {
                bone_index = i;
                break;
            }
        }
        if (bone_index == ~0u)
        {
            return luaL_error(L, "the bone '%s' could not be found", lua_tostring(L, 2));
        }
        dmGameObject::HInstance instance = component->m_NodeInstances[bone_index];
        if (instance == 0x0)
        {
            return luaL_error(L, "no game object found for the bone '%s'", lua_tostring(L, 2));
        }
        dmhash_t instance_id = dmGameObject::GetIdentifier(instance);
        if (instance_id == 0x0)
        {
            return luaL_error(L, "game object contains no identifier for the bone '%s'", lua_tostring(L, 2));
        }
        dmScript::PushHash(L, instance_id);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# set a shader constant for a model component
     * The constant must be defined in the material assigned to the model.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until model.reset_constant is called.
     * Which model to set a constant for is identified by the URL.
     *
     * @name model.set_constant
     * @param url the model that should have a constant set (url)
     * @param name of the constant (string|hash)
     * @param value of the constant (vec4)
     * @examples
     * <p>
     * The following examples assumes that the model has id "model" and that the default-material in builtins is used.
     * If you assign a custom material to the model, you can set the constants defined there in the same manner.
     * </p>
     * <p>
     * How to tint a model to red:
     * </p>
     * <pre>
     * function init(self)
     *     model.set_constant("#model", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * </pre>
     */
    int LuaModelComp_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstant msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a model
     * The constant must be defined in the material assigned to the model.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which model to reset a constant for is identified by the URL.
     *
     * @name model.reset_constant
     * @param url the model that should have a constant reset (url)
     * @param name of the constant (string|hash)
     * @examples
     * <p>
     * The following examples assumes that the model has id "model" and that the default-material in builtins is used.
     * If you assign a custom material to the model, you can reset the constants defined there in the same manner.
     * </p>
     * <p>
     * How to reset the tinting of a model:
     * </p>
     * <pre>
     * function init(self)
     *     model.reset_constant("#model", "tint")
     * end
     * </pre>
     */
    int LuaModelComp_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmGameSystemDDF::ResetConstant msg;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg MODEL_COMP_FUNCTIONS[] =
    {
            {"play",    LuaModelComp_Play},
            {"play_anim", LuaModelComp_PlayAnim},
            {"cancel",  LuaModelComp_Cancel},
            {"get_go",  LuaModelComp_GetGO},
            {"set_constant",    LuaModelComp_SetConstant},
            {"reset_constant",  LuaModelComp_ResetConstant},
            {0, 0}
    };

    void ScriptModelRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "model", MODEL_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }

}
