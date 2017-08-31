#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include "gamesys.h"
#include "gamesys_ddf.h"
#include "../gamesys_private.h"
#include "../components/comp_spine_model.h"

#include "script_spine_model.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}


namespace dmGameSystem
{
    /*# Spine model API documentation
     *
     * Functions and messages for interacting with the 'Spine' 2D bone
     * animation system.
     *
     * @document
     * @name Spine
     * @namespace spine
     */

     /*# [type:number] spine cursor
     *
     * The normalized animation cursor. The type of the property is number.
     *
     * [icon:attention] Please note that spine events may not fire as expected when the cursor is manipulated directly.
     *
     * @name cursor
     * @property
     *
     * @examples
     *
     * How to get the normalized cursor value:
     *
     * ```lua
     * function init(self)
     *   -- Get the cursor value on component "spine"
     *   cursor = go.get("#spine", "cursor")
     * end
     * ```
     *
     * How to animate the cursor from 0.0 to 1.0 using linear easing for 2.0 seconds:
     *
     * ```lua
     * function init(self)
     *   -- Get the current value on component "spine"
     *   go.set("#spine", "cursor", 0.0)
     *   -- Animate the cursor value
     *   go.animate("#spine", "cursor", go.PLAYBACK_LOOP_FORWARD, 1.0, go.EASING_LINEAR, 2)
     * end
     * ```
     */

     /*# [type:number] spine playback_rate
     *
     * The animation playback rate. A multiplier to the animation playback rate. The type of the property is number.
     *
     * The playback_rate is a non-negative number, a negative value will be clamped to 0.
     *
     * @name playback_rate
     * @property
     *
     * @examples
     *
     * How to set the playback_rate on component "spine" to play at double the current speed:
     *
     * ```lua
     * function init(self)
     *   -- Get the current value on component "spine"
     *   playback_rate = go.get("#spine", "playback_rate")
     *   -- Set the playback_rate to double the previous value.
     *   go.set("#spine", "playback_rate", playback_rate * 2)
     * end
     * ```
     */

     /*# [type:hash] spine animation
     *
     * The current animation set on the component. The type of the property is hash.
     *
     * @name animation
     * @property
     *
     * @examples
     *
     * How to read the current animation from a spinemodel component:
     *
     * ```lua
     * function init(self)
     *   -- Get the current animation on component "spinemodel"
     *   local animation = go.get("#spinemodel", "animation")
     * end
     * ```
     */

     /*# [type:hash] spine skin
     *
     * The current skin on the component. The type of the property is hash.
     * If setting the skin property the skin must be present on the spine
     * model or a runtime error is signalled.
     *
     * @name skin
     * @property
     *
     * @examples
     *
     * How to read and write the current skin from a spinemodel component:
     *
     * ```lua
     * function init(self)
     *   -- If the hero skin is set to "bruce_banner", turn him green
     *   local skin = go.get("#hero", "skin")
     *   if skin == hash("bruce_banner") then
     *     go.set("#hero", "skin", hash("green"))
     *   end
     * end
     * ```
     */

    int SpineComp_Play(lua_State* L)
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
                sender.m_Function = dmScript::Ref(L, LUA_REGISTRYINDEX) + 2;
            }
        }

        dmGameSystemDDF::SpinePlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# play an animation on a spine model
     * Plays a specified animation on a spine model component with specified playback
     * mode and parameters.
     *
     * An optional completion callback function can be provided that will be called when
     * the animation has completed playing. If no function is provided,
     * a [ref:spine_animation_done] message is sent to the script that started the animation.
     *
     * [icon:attention] The callback is not called (or message sent) if the animation is
     * cancelled with [ref:spine.cancel]. The callback is called (or message sent) only for
     * animations that play with the following playback modes:
     *
     * - `go.PLAYBACK_ONCE_FORWARD`
     * - `go.PLAYBACK_ONCE_BACKWARD`
     * - `go.PLAYBACK_ONCE_PINGPONG`
     *
     * @name spine.play_anim
     * @param url [type:string|hash|url] the spine model for which to play the animation
     * @param anim_id [type:string|hash] id of the animation to play
     * @param playback [type:constant] playback mode of the animation
     *
     * - `go.PLAYBACK_ONCE_FORWARD`
     * - `go.PLAYBACK_ONCE_BACKWARD`
     * - `go.PLAYBACK_ONCE_PINGPONG`
     * - `go.PLAYBACK_LOOP_FORWARD`
     * - `go.PLAYBACK_LOOP_BACKWARD`
     * - `go.PLAYBACK_LOOP_PINGPONG`
     *
     * @param [play_properties] [type:table] optional table with properties:
     *
     * `blend_duration`
     * : [type:number] duration of a linear blend between the current and new animation.
     *
     * `offset`
     * : [type:number] the normalized initial value of the animation cursor when the animation starts playing.
     *
     * `playback_rate`
     * : [type:number] the rate with which the animation will be played. Must be positive.
     *
     * @param [complete_function] [type:function(self, message_id, message, sender))] function to call when the animation has completed.
     *
     * `self`
     * : [type:object] The current object.
     *
     * `message_id`
     * : [type:hash] The name of the completion message, `"spine_animation_done"`.
     *
     * `message`
     * : [type:table] Information about the completion:
     *
     * - [type:hash] `animation_id` - the animation that was completed.
     * - [type:constant] `playback` - the playback mode for the animation.
     *
     * `sender`
     * : [type:url] The invoker of the callback: the spine model component.
     *
     * @examples
     *
     * The following examples assumes that the spine model has id "spinemodel".
     *
     * How to play the "jump" animation followed by the "run" animation:
     *
     * ```lua
     * local function anim_done(self, message_id, message, sender)
     *   if message_id == hash("spine_animation_done") then
     *     if message.animation_id == hash("jump") then
     *       -- open animation done, chain with "run"
     *       local properties = { blend_duration = 0.2 }
     *       spine.play_anim(url, "run", go.PLAYBACK_LOOP_FORWARD, properties, anim_done)
     *     end
     *   end
     * end
     *
     * function init(self)
     *     local url = msg.url("#spinemodel")
     *     local play_properties = { blend_duration = 0.1 }
     *     -- first blend during 0.1 sec into the jump, then during 0.2 s into the run animation
     *     spine.play_anim(url, "open", go.PLAYBACK_ONCE_FORWARD, play_properties, anim_done)
     * end
     * ```
     */
    int SpineComp_PlayAnim(lua_State* L)
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

        dmGameSystemDDF::SpinePlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);

        return 0;
    }

    /*# cancel all animation on a spine model
     * Cancels all running animations on a specified spine model component.
     *
     * @name spine.cancel
     * @param url [type:string|hash|url] the spine model for which to cancel the animation
     * @examples
     *
     * The following examples assumes that the spine model has id "spinemodel".
     *
     * How to cancel all animation:
     *
     * ```lua
     * function init(self)
     *   spine.cancel("#spinemodel")
     * end
     * ```
     */
    int SpineComp_Cancel(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmGameSystemDDF::SpineCancelAnimation msg;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# retrieve the game object corresponding to a spine model skeleton bone
     * Returns the id of the game object that corresponds to a specified skeleton bone.
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity `O(n)`, where `n` is the number of bones in the spine model skeleton.
     * Game objects corresponding to a spine model skeleton bone can not be individually deleted.
     *
     * @name spine.get_go
     * @param url [type:string|hash|url] the spine model to query
     * @param bone_id [type:string|hash] id of the corresponding bone
     * @return id [type:hash] id of the game object
     * @examples
     *
     * The following examples assumes that the spine model has id "spinemodel".
     *
     * How to parent the game object of the calling script to the "right_hand" bone of the spine model in a player game object:
     *
     * ```lua
     * function init(self)
     *   local parent = spine.get_go("player#spinemodel", "right_hand")
     *   msg.post(".", "set_parent", {parent_id = parent})
     * end
     * ```
     */
    int SpineComp_GetGO(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        SpineModelWorld* world = 0;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, SPINE_MODEL_EXT, &user_data, &receiver, (void**) &world);

        SpineModelComponent* component = world->m_Components.Get(user_data);

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
        if(bone_index >= component->m_NodeInstances.Size())
        {
            return luaL_error(L, "no game object found for the bone '%s'", lua_tostring(L, 2));
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

    /*# set the target position of an IK constraint object
     *
     * Sets a static (vector3) target position of an inverse kinematic (IK) object.
     *
     * @name spine.set_ik_target_position
     * @param url [type:string|hash|url] the spine model containing the object
     * @param ik_constraint_id [type:string|hash] id of the corresponding IK constraint object
     * @param position [type:vector3] target position
     * @examples
     *
     * The following example assumes that the spine model has id "spinemodel".
     *
     * How to set the target IK position of the right_hand_constraint constraint object of the player object
     *
     * ```lua
     * function init(self)
     *   local pos = vmath.vector3(1, 2, 3)
     *   spine.set_ik_target_position("player#spinemodel", "right_hand_constraint", pos)
     * end
     * ```
     */
    int SpineComp_SetIKTargetPosition(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        SpineModelWorld* world = 0;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, SPINE_MODEL_EXT, &user_data, &receiver, (void**) &world);
        SpineModelComponent* component = world->m_Components.Get(user_data);

        dmhash_t ik_constraint_id = dmScript::CheckHashOrString(L, 2);
        Vectormath::Aos::Vector3* position = dmScript::CheckVector3(L, 3);

        if (!CompSpineModelSetIKTargetPosition(component, ik_constraint_id, 1.0f, (Point3)*position))
        {
            return luaL_error(L, "the IK constraint target '%s' could not be found", lua_tostring(L, 2));
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set the IK constraint object target position to follow position of a game object
     *
     * Sets a game object as target position of an inverse kinematic (IK) object. As the
     * target game object's position is updated, the constraint object is updated with the
     * new position.
     *
     * @name spine.set_ik_target
     * @param url [type:string|hash|url] the spine model containing the object
     * @param ik_constraint_id [type:string|hash] id of the corresponding IK constraint object
     * @param target_url [type:string|hash|url] target game object
     * @examples
     *
     * The following example assumes that the spine model has id "spinemodel".
     *
     * How to set the target IK position of the right_hand_constraint constraint object of the player object
     * to follow the position of game object with url "some_game_object"
     *
     * ```lua
     * function init(self)
     *   spine.set_ik_target("player#spinemodel", "right_hand_constraint", "some_game_object")
     * end
     * ```
     */
    int SpineComp_SetIKTarget(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        uintptr_t user_data;
        dmMessage::URL receiver;
        SpineModelWorld* world = 0;
        dmGameObject::GetComponentUserDataFromLua(L, 1, collection, SPINE_MODEL_EXT, &user_data, &receiver, (void**) &world);
        SpineModelComponent* component = world->m_Components.Get(user_data);

        dmhash_t ik_constraint_id = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL sender;
        dmScript::GetURL(L, &sender);
        dmMessage::URL target;
        dmScript::ResolveURL(L, 3, &target, &sender);
        if (target.m_Socket != dmGameObject::GetMessageSocket(collection))
        {
            return luaL_error(L, "spine.set_ik_target can only use instances within the same collection.");
        }
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(collection, target.m_Path);
        if (target_instance == 0)
            return luaL_error(L, "Could not find any instance with id '%s'.", dmHashReverseSafe64(target.m_Path));

        if (!CompSpineModelSetIKTargetInstance(component, ik_constraint_id, 1.0f, target.m_Path))
        {
            char str[128];
            return luaL_error(L, "the IK constraint target '%s' could not be found", dmScript::GetStringFromHashOrString(L, 2, str, sizeof(str)));
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a shader constant for a spine model
     * Sets a shader constant for a spine model component.
     * The constant must be defined in the material assigned to the spine model.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until spine.reset_constant is called.
     * Which spine model to set a constant for is identified by the URL.
     *
     * @name spine.set_constant
     * @param url [type:string|hash|url] the spine model that should have a constant set
     * @param constant [type:string|hash] name of the constant
     * @param value [type:vector4] value of the constant
     * @examples
     *
     * The following examples assumes that the spine model has id "spinemodel" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     *
     * How to tint a spine model to red:
     *
     * ```lua
     * function init(self)
     *   spine.set_constant("#spinemodel", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * ```
     */
    int SpineComp_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        Vectormath::Aos::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstantSpineModel msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantSpineModel::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstantSpineModel::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a spine model
     * Resets a shader constant for a spine model component.
     * The constant must be defined in the material assigned to the spine model.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which spine model to reset a constant for is identified by the URL.
     *
     * @name spine.reset_constant
     * @param url [type:string|hash|url] the spine model that should have a constant reset
     * @param constant [type:string|hash] name of the constant
     * @examples
     *
     * The following examples assumes that the spine model has id "spinemodel" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     *
     * How to reset the tinting of a spine model:
     *
     * ```lua
     * function init(self)
     *   spine.reset_constant("#spinemodel", "tint")
     * end
     * ```
     */
    int SpineComp_ResetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmGameSystemDDF::ResetConstantSpineModel msg;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantSpineModel::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstantSpineModel::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg SPINE_COMP_FUNCTIONS[] =
    {
            {"play",    SpineComp_Play},
            {"play_anim", SpineComp_PlayAnim},
            {"cancel",  SpineComp_Cancel},
            {"get_go",  SpineComp_GetGO},
            {"set_ik_target_position", SpineComp_SetIKTargetPosition},
            {"set_ik_target",   SpineComp_SetIKTarget},
            {"set_constant",    SpineComp_SetConstant},
            {"reset_constant",  SpineComp_ResetConstant},
            {0, 0}
    };

    void ScriptSpineModelRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "spine", SPINE_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
