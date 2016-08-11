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
    /*# play an animation on a spine model
     *
     * @name spine.play
     * @param url the spine model for which to play the animation (url)
     * @param animation_id id of the animation to play (string|hash)
     * @param playback playback mode of the animation (constant)
     * <ul>
     *   <li><code>go.PLAYBACK_ONCE_FORWARD</code></li>
     *   <li><code>go.PLAYBACK_ONCE_BACKWARD</code></li>
     *   <li><code>go.PLAYBACK_ONCE_PINGPONG</code></li>
     *   <li><code>go.PLAYBACK_LOOP_FORWARD</code></li>
     *   <li><code>go.PLAYBACK_LOOP_BACKWARD</code></li>
     *   <li><code>go.PLAYBACK_LOOP_PINGPONG</code></li>
     * </ul>
     * @param blend_duration duration of a linear blend between the current and new animations
     * @param [complete_function] function to call when the animation has completed (function)
     * @examples
     * <p>
     * The following examples assumes that the spine model has id "spinemodel".
     * </p>
     * <p>
     * How to play the "jump" animation followed by the "run" animation:
     * </p>
     * <pre>
     * function init(self)
     *     local url = msg.url("#spinemodel")
     *     -- first blend during 0.1 sec into the jump, then during 0.2 s into the run animation
     *     spine.play(url, "jump", go.PLAYBACK_ONCE_FORWARD, 0.1, function (self)
     *         spine.play(url, "run", go.PLAYBACK_LOOP_FORWARD, 0.2)
     *     end)
     * end
     * </pre>
     */
    int SpineComp_Play(lua_State* L)
    {
        int top = lua_gettop(L);

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

        dmGameSystemDDF::SpinePlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor, &msg, sizeof(msg));
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# cancel all animation on a spine model
     *
     * @name spine.cancel
     * @param url the spine model for which to cancel the animation (url)
     * @examples
     * <p>
     * The following examples assumes that the spine model has id "spinemodel".
     * </p>
     * <p>
     * How to cancel all animation:
     * </p>
     * <pre>
     * function init(self)
     *     spine.cancel("#spinemodel")
     * end
     * </pre>
     */
    int SpineComp_Cancel(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmGameSystemDDF::SpineCancelAnimation msg;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor, &msg, sizeof(msg));
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# retrieve the game object corresponding to a spine model skeleton bone
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity O(n), where n is the number of bones in the spine model skeleton.
     * Game objects corresponding to a spine model skeleton bone can not be individually deleted.
     * Only available from .script files.
     *
     * @name spine.get_go
     * @param url the spine model to query (url)
     * @param bone_id id of the corresponding bone (string|hash)
     * @return id of the game object
     * @examples
     * <p>
     * The following examples assumes that the spine model has id "spinemodel".
     * <p>
     * How to parent the game object of the calling script to the "right_hand" bone of the spine model in a player game object:
     * </p>
     * <pre>
     * function init(self)
     *     local parent = spine.get_go("player#spinemodel", "right_hand")
     *     msg.post(".", "set_parent", {parent_id = parent})
     * end
     * </pre>
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

        // dmGameSystemDDF::Skeleton* skeleton = component->m_Resource->m_Scene->m_SkeletonRes->m_Skeleton;
        // uint32_t bone_count = skeleton->m_Bones.m_Count;
        uint32_t bone_index = ~0u;
        // for (uint32_t i = 0; i < bone_count; ++i)
        // {
        //     if (skeleton->m_Bones[i].m_Id == bone_id)
        //     {
        //         bone_index = i;
        //         break;
        //     }
        // }
        if (bone_index == ~0u)
        {
            return luaL_error(L, "the bone '%s' could not be found", lua_tostring(L, 2));
        }
        // dmGameObject::HInstance instance = component->m_NodeInstances[bone_index];
        // if (instance == 0x0)
        // {
        //     return luaL_error(L, "no game object found for the bone '%s'", lua_tostring(L, 2));
        // }
        // dmhash_t instance_id = dmGameObject::GetIdentifier(instance);
        // if (instance_id == 0x0)
        // {
        //     return luaL_error(L, "game object contains no identifier for the bone '%s'", lua_tostring(L, 2));
        // }
        // dmScript::PushHash(L, instance_id);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    /*# set the target position of an IK constraint object
     * Only available from .script files.
     *
     * @name spine.set_ik_target_position
     * @param url the spine model containing the object (url)
     * @param ik_constraint_id id of the corresponding IK constraint object (string|hash)
     * @param position (vec3)
     * @examples
     * <p>
     * The following example assumes that the spine model has id "spinemodel".
     * <p>
     * How to set the target IK position of the right_hand_constraint constraint object of the player object
     * </p>
     * <pre>
     * function init(self)
     *     spine.set_ik_target_position("player#spinemodel", "right_hand_constraint", vmath.vector3(1,2,3))
     * end
     * </pre>
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

        if (!CompSpineSetIKTargetPosition(component, ik_constraint_id, 1.0f, (Point3)*position))
        {
            return luaL_error(L, "the IK constraint target '%s' could not be found", lua_tostring(L, 2));
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set the IK constraint object target position to follow position of a game object
     * Only available from .script files.
     *
     * @name spine.set_ik_target
     * @param url the spine model containing the object (url)
     * @param ik_constraint_id id of the corresponding IK constraint object (string|hash)
     * @param target_url target game object (url)
     * @examples
     * <p>
     * The following example assumes that the spine model has id "spinemodel".
     * <p>
     * How to set the target IK position of the right_hand_constraint constraint object of the player object
     * to follow the position of game object with url "some_game_object"
     * </p>
     * <pre>
     * function init(self)
     *     spine.set_ik_target("player#spinemodel", "right_hand_constraint", "some_game_object")
     * end
     * </pre>
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
            luaL_error(L, "spine.set_ik_target can only use instances within the same collection.");
        }
        dmGameObject::HInstance target_instance = dmGameObject::GetInstanceFromIdentifier(collection, target.m_Path);
        if (target_instance == 0)
            return luaL_error(L, "Could not find any instance with id '%s'.", (const char*)dmHashReverse64(target.m_Path, 0x0));

        if (!CompSpineSetIKTargetInstance(component, ik_constraint_id, 1.0f, target.m_Path))
        {
            return luaL_error(L, "the IK constraint target '%s' could not be found", lua_tostring(L, 2));
        }

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set a shader constant for a spine model
     * The constant must be defined in the material assigned to the spine model.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until spine.reset_constant is called.
     * Which spine model to set a constant for is identified by the URL.
     *
     * @name spine.set_constant
     * @param url the spine model that should have a constant set (url)
     * @param name of the constant (string|hash)
     * @param value of the constant (vec4)
     * @examples
     * <p>
     * The following examples assumes that the spine model has id "spinemodel" and that the default-material in builtins is used.
     * If you assign a custom material to the spine model, you can set the constants defined there in the same manner.
     * </p>
     * <p>
     * How to tint a spine model to red:
     * </p>
     * <pre>
     * function init(self)
     *     spine.set_constant("#spinemodel", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * </pre>
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

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstantSpineModel::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstantSpineModel::m_DDFDescriptor, &msg, sizeof(msg));
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# reset a shader constant for a spine model
     * The constant must be defined in the material assigned to the spine model.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which spine model to reset a constant for is identified by the URL.
     *
     * @name spine.reset_constant
     * @param url the spine model that should have a constant reset (url)
     * @param name of the constant (string|hash)
     * @examples
     * <p>
     * The following examples assumes that the spine model has id "spinemodel" and that the default-material in builtins is used.
     * If you assign a custom material to the spine model, you can reset the constants defined there in the same manner.
     * </p>
     * <p>
     * How to reset the tinting of a spine model:
     * </p>
     * <pre>
     * function init(self)
     *     spine.reset_constant("#spinemodel", "tint")
     * end
     * </pre>
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

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstantSpineModel::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::ResetConstantSpineModel::m_DDFDescriptor, &msg, sizeof(msg));
        assert(top == lua_gettop(L));
        return 0;
    }

    static const luaL_reg SPINE_COMP_FUNCTIONS[] =
    {
            {"play",    SpineComp_Play},
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
