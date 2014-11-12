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

        dmhash_t anim_id;
        if (lua_isstring(L, 2))
        {
            anim_id = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            anim_id = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "animation_id must be either a hash or a string");
        }
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

        const uint32_t buffer_size = 256;
        uint8_t buffer[buffer_size];
        dmGameSystemDDF::SpinePlayAnimation* play = (dmGameSystemDDF::SpinePlayAnimation*)buffer;

        uint32_t msg_size = sizeof(dmGameSystemDDF::SpinePlayAnimation);

        play->m_AnimationId = anim_id;
        play->m_Playback = playback;
        play->m_BlendDuration = blend_duration;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpinePlayAnimation::m_DDFDescriptor, buffer, msg_size);
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

        dmGameSystemDDF::SpineCancelAnimation cancel;

        uint32_t msg_size = sizeof(dmGameSystemDDF::SpineCancelAnimation);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SpineCancelAnimation::m_DDFDescriptor, &cancel, msg_size);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# retrieve the game object corresponding to a spine model skeleton bone
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity O(n), where n is the number of bones in the spine model skeleton.
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

        uintptr_t user_data;
        dmMessage::URL receiver;
        dmGameObject::GetComponentUserDataFromLua(L, 1, SPINE_MODEL_EXT, &user_data, &receiver);
        SpineModelComponent* component = (SpineModelComponent*) user_data;

        dmhash_t bone_id;
        if (lua_isstring(L, 2))
        {
            bone_id = dmHashString64(lua_tostring(L, 2));
        }
        else if (dmScript::IsHash(L, 2))
        {
            bone_id = dmScript::CheckHash(L, 2);
        }
        else
        {
            return luaL_error(L, "bone_id must be either a hash or a string");
        }

        dmGameSystemDDF::Skeleton* skeleton = &component->m_Resource->m_Scene->m_SpineScene->m_Skeleton;
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
        dmhash_t instance_id = component->m_NodeIds[bone_index];
        if (instance_id == 0x0)
        {
            return luaL_error(L, "no game object found for the bone '%s'", lua_tostring(L, 2));
        }
        dmScript::PushHash(L, instance_id);

        assert(top + 1 == lua_gettop(L));
        return 1;
    }

    static const luaL_reg SPINE_COMP_FUNCTIONS[] =
    {
            {"play",    SpineComp_Play},
            {"cancel",  SpineComp_Cancel},
            {"get_go",  SpineComp_GetGO},
            {0, 0}
    };

    void ScriptSpineModelRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "spine", SPINE_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
