// Copyright 2020-2024 The Defold Foundation
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

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <gameobject/script.h>

#include "gamesys.h"
#include "../gamesys_private.h"
#include "../components/comp_model.h"
#include "../resources/res_model.h"
#include "../resources/res_skeleton.h"
#include "../resources/res_rig_scene.h"

#include <gamesys/gamesys_ddf.h>
#include <gamesys/model_ddf.h>

#include "script_model.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

#define MODEL_MODULE_NAME "model"

namespace dmGameSystem
{
    /*# Model API documentation
     *
     * Functions and messages for interacting with model components.
     *
     * @document
     * @name Model
     * @namespace model
     */

    /*# [type:number] model cursor
     *
     * The normalized animation cursor. The type of the property is number.
     *
     * [icon:attention] Please note that model events may not fire as expected when the cursor is manipulated directly.
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
     *   -- Get the cursor value on component "model"
     *   cursor = go.get("#model", "cursor")
     * end
     * ```
     *
     * How to animate the cursor from 0.0 to 1.0 using linear easing for 2.0 seconds:
     *
     * ```lua
     * function init(self)
     *   -- Get the current value on component "model"
     *   go.set("#model", "cursor", 0.0)
     *   -- Animate the cursor value
     *   go.animate("#model", "cursor", go.PLAYBACK_LOOP_FORWARD, 1.0, go.EASING_LINEAR, 2)
     * end
     * ```
     */

    /*# [type:number] model playback_rate
     *
     * The animation playback rate. A multiplier to the animation playback rate. The type of the property is number.
     *
     * @name playback_rate
     * @property
     *
     * @examples
     *
     * How to set the playback_rate on component "model" to play at double the current speed:
     *
     * ```lua
     * function init(self)
     *   -- Get the current value on component "model"
     *   playback_rate = go.get("#model", "playback_rate")
     *   -- Set the playback_rate to double the previous value.
     *   go.set("#model", "playback_rate", playback_rate * 2)
     * end
     * ```
     *
     * The playback_rate is a non-negative number, a negative value will be clamped to 0.
     */

     /*# [type:hash] model animation
     *
     * The current animation set on the component. The type of the property is hash.
     *
     * @name animation
     * @property
     *
     * @examples
     *
     * How to read the current animation from a model component:
     *
     * ```lua
     * function init(self)
     *   -- Get the current animation on component "model"
     *   local animation = go.get("#model", "animation")
     *   if animation == hash("run_left") then
     *     -- Running left. Do something...
     *   end
     * end
     * ```
     */

    /*# [type:hash] model textureN where N is 0-7
     *
     * The texture hash id of the model. Used for getting/setting model texture for unit 0-7
     *
     * @name textureN
     * @property
     *
     * @examples
     *
     * How to set texture using a script property (see [ref:resource.texture]):
     *
     * ```lua
     * go.property("my_texture", texture("/texture.png"))
     * function init(self)
     *   go.set("#model", "texture0", self.my_texture)
     * end
     * ```
     *
     * See [ref:resource.set_texture] for an example on how to set the texture of an atlas.
     */

    /*# [type:hash] model material
     *
     * The material used when rendering the model. The type of the property is hash.
     *
     * @name material
     * @property
     *
     * @examples
     *
     * How to set material using a script property (see [ref:resource.material]):
     *
     * ```lua
     * go.property("my_material", resource.material("/material.material"))
     * function init(self)
     *   go.set("#model", "material", self.my_material)
     * end
     * ```
     */

    static int LuaModelComp_Play(lua_State* L)
    {
        dmLogOnceWarning(dmScript::DEPRECATION_FUNCTION_FMT, MODEL_MODULE_NAME, "play", MODEL_MODULE_NAME, "play_anim");

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

        int functionref = 0;
        if (top > 4)
        {
            if (lua_isfunction(L, 5))
            {
                lua_pushvalue(L, 5);
                functionref = dmScript::RefInInstance(L) - LUA_NOREF;
            }
        }

        dmModelDDF::ModelPlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmModelDDF::ModelPlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)functionref, (uintptr_t)dmModelDDF::ModelPlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# play an animation on a model
     * Plays an animation on a model component with specified playback
     * mode and parameters.
     *
     * An optional completion callback function can be provided that will be called when
     * the animation has completed playing. If no function is provided,
     * a [ref:model_animation_done] message is sent to the script that started the animation.
     *
     * [icon:attention] The callback is not called (or message sent) if the animation is
     * cancelled with [ref:model.cancel]. The callback is called (or message sent) only for
     * animations that play with the following playback modes:
     *
     * - `go.PLAYBACK_ONCE_FORWARD`
     * - `go.PLAYBACK_ONCE_BACKWARD`
     * - `go.PLAYBACK_ONCE_PINGPONG`
     *
     * @name model.play_anim
     * @param url [type:string|hash|url] the model for which to play the animation
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
     * @param [play_properties] [type:table] optional table with properties
     *
     * Play properties table:
     *
     * `blend_duration`
     * : [type:number] Duration of a linear blend between the current and new animation.
     *
     * `offset`
     * : [type:number] The normalized initial value of the animation cursor when the animation starts playing.
     *
     * `playback_rate`
     * : [type:number] The rate with which the animation will be played. Must be positive.
     *
     * @param [complete_function] [type:function(self, message_id, message, sender)] function to call when the animation has completed.
     *
     * `self`
     * : [type:object] The current object.
     *
     * `message_id`
     * : [type:hash] The name of the completion message, `"model_animation_done"`.
     *
     * `message`
     * : [type:table] Information about the completion:
     *
     * - [type:hash] `animation_id` - the animation that was completed.
     * - [type:constant] `playback` - the playback mode for the animation.
     *
     * `sender`
     * : [type:url] The invoker of the callback: the model component.
     *
     * @examples
     *
     * The following examples assumes that the model has id "model".
     *
     * How to play the "jump" animation followed by the "run" animation:
     *
     * ```lua
     * local function anim_done(self, message_id, message, sender)
     *   if message_id == hash("model_animation_done") then
     *     if message.animation_id == hash("jump") then
     *       -- open animation done, chain with "run"
     *       local properties = { blend_duration = 0.2 }
     *       model.play_anim(url, "run", go.PLAYBACK_LOOP_FORWARD, properties, anim_done)
     *     end
     *   end
     * end
     *
     * function init(self)
     *     local url = msg.url("#model")
     *     local play_properties = { blend_duration = 0.1 }
     *     -- first blend during 0.1 sec into the jump, then during 0.2 s into the run animation
     *     model.play_anim(url, "jump", go.PLAYBACK_ONCE_FORWARD, play_properties, anim_done)
     * end
     * ```
     */
    static int LuaModelComp_PlayAnim(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
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

        int functionref = 0;
        if (top > 4) // completed cb
        {
            if (lua_isfunction(L, 5))
            {
                lua_pushvalue(L, 5);
                // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, in order to have 0 for "no function"
                functionref = dmScript::RefInInstance(L) - LUA_NOREF;
            }
        }

        dmModelDDF::ModelPlayAnimation msg;
        msg.m_AnimationId = anim_id;
        msg.m_Playback = playback;
        msg.m_BlendDuration = blend_duration;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmModelDDF::ModelPlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)functionref, (uintptr_t)dmModelDDF::ModelPlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    /*# cancel all animation on a model
     * Cancels all animation on a model component.
     *
     * @name model.cancel
     * @param url [type:string|hash|url] the model for which to cancel the animation
     */
    static int LuaModelComp_Cancel(lua_State* L)
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
     * Gets the id of the game object that corresponds to a model skeleton bone.
     * The returned game object can be used for parenting and transform queries.
     * This function has complexity `O(n)`, where `n` is the number of bones in the model skeleton.
     * Game objects corresponding to a model skeleton bone can not be individually deleted.
     *
     * @name model.get_go
     * @param url [type:string|hash|url] the model to query
     * @param bone_id [type:string|hash] id of the corresponding bone
     * @return id [type:hash] id of the game object
     * @examples
     *
     * The following examples assumes that the model component has id "model".
     *
     * How to parent the game object of the calling script to the "right_hand" bone of the model in a player game object:
     *
     * ```lua
     * function init(self)
     *     local parent = model.get_go("player#model", "right_hand")
     *     msg.post(".", "set_parent", {parent_id = parent})
     * end
     * ```
     */
    static int LuaModelComp_GetGO(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        ModelComponent* component;
        dmGameObject::GetComponentFromLua(L, 1, collection, MODEL_EXT, (dmGameObject::HComponent*)&component, 0, 0);
        if (!component)
        {
            return luaL_error(L, "the component '%s' could not be found", lua_tostring(L, 1));
        }
        ModelResource* resource = CompModelGetModelResource(component);
        if (!resource || !resource->m_RigScene->m_SkeletonRes)
        {
            return luaL_error(L, "the bone '%s' could not be found", lua_tostring(L, 2));
        }

        dmhash_t bone_id = dmScript::CheckHashOrString(L, 2);

        const dmHashTable64<uint32_t>& bone_indices = resource->m_RigScene->m_SkeletonRes->m_BoneIndices;
        const uint32_t* bone_index = bone_indices.Get(bone_id);

        if (!bone_index)
        {
            return luaL_error(L, "the bone '%s' could not be found", lua_tostring(L, 2));
        }

        dmGameObject::HInstance instance = CompModelGetNodeInstance(component, *bone_index);
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

        assert((top + 1) == lua_gettop(L));
        return 1;
    }

    /** DEPRECATED! set a shader constant for a model
     * Sets a shader constant for a model component.
     * The constant must be defined in the material assigned to the model.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until model.reset_constant is called.
     * Which model to set a constant for is identified by the URL.
     *
     * @name model.set_constant
     * @param url [type:string|hash|url] the model that should have a constant set
     * @param constant [type:string|hash] name of the constant
     * @param value [type:vector4] value of the constant
     * @examples
     *
     * The following examples assumes that the model has id "model" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the model, you can set the constants defined there in the same manner.
     *
     * How to tint a model to red:
     *
     * ```lua
     * function init(self)
     *     model.set_constant("#model", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * ```
     */
    static int LuaModelComp_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        dmVMath::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstant msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;
        msg.m_Index = 0; // TODO: Pass a real index here?

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /** DEPRECATED! reset a shader constant for a model
     * Resets a shader constant for a model component.
     * The constant must be defined in the material assigned to the model.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which model to reset a constant for is identified by the URL.
     *
     * @name model.reset_constant
     * @param url [type:string|hash|url] the model that should have a constant reset.
     * @param constant [type:string|hash] name of the constant.
     * @examples
     *
     * The following examples assumes that the model has id "model" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the model, you can reset the constants defined there in the same manner.
     *
     * How to reset the tinting of a model:
     *
     * ```lua
     * function init(self)
     *     model.reset_constant("#model", "tint")
     * end
     * ```
     */
    static int LuaModelComp_ResetConstant(lua_State* L)
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

    static void LuaModelComp_GetSetMeshEnabled_Internal(lua_State* L, ModelComponent** out_component, dmhash_t* out_mesh_id)
    {
        dmGameObject::HInstance sender_instance = CheckGoInstance(L);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(sender_instance);

        dmGameObject::GetComponentFromLua(L, 1, collection, MODEL_EXT, (dmGameObject::HComponent*)out_component, 0, 0);
        *out_mesh_id = dmScript::CheckHashOrString(L, 2);
    }

    /*# enable or disable a mesh
     * Enable or disable visibility of a mesh
     *
     * @name model.set_mesh_enabled
     * @param url [type:string|hash|url] the model
     * @param mesh_id [type:string|hash|url] the id of the mesh
     * @param enabled [type:boolean] true if the mesh should be visible, false if it should be hideen
     * @examples
     *
     * ```lua
     * function init(self)
     *     model.set_mesh_enabled("#model", "Sword", false) -- hide the sword
     *     model.set_mesh_enabled("#model", "Axe", true)    -- show the axe
     * end
     * ```
     */
    static int LuaModelComp_SetMeshEnabled(lua_State* L)
    {
        int top = lua_gettop(L);

        ModelComponent* component = 0;
        dmhash_t mesh_id = 0;
        LuaModelComp_GetSetMeshEnabled_Internal(L, &component, &mesh_id);
        if (!component)
        {
            return luaL_error(L, "the component '%s' could not be found", lua_tostring(L, 1));
        }

        bool enable = dmScript::CheckBoolean(L, 3);
        bool result = CompModelSetMeshEnabled(component, mesh_id, enable);
        if (!result)
            return luaL_error(L, "Component %s had no mesh with id %s", lua_tostring(L, 1), lua_tostring(L, 2));

        assert(top == lua_gettop(L));
        return 0;
    }

    /*# get the enabled state of a mesh
     * Get the enabled state of a mesh
     *
     * @name model.get_mesh_enabled
     * @param url [type:string|hash|url] the model
     * @param mesh_id [type:string|hash|url] the id of the mesh
     * @return enabled [type:boolean] true if the mesh is visible, false otherwise
     * @examples
     *
     * ```lua
     * function init(self)
     *     if model.get_mesh_enabled("#model", "Sword") then
     *        -- set properties specific for the sword
     *        self.weapon_properties = game.data.weapons["Sword"]
     *     end
     * end
     * ```
     */
    static int LuaModelComp_GetMeshEnabled(lua_State* L)
    {
        int top = lua_gettop(L);

        ModelComponent* component = 0;
        dmhash_t mesh_id = 0;
        LuaModelComp_GetSetMeshEnabled_Internal(L, &component, &mesh_id);
        if (!component)
        {
            return luaL_error(L, "the component '%s' could not be found", lua_tostring(L, 1));
        }

        bool enabled = true;
        bool result = CompModelGetMeshEnabled(component, mesh_id, &enabled);
        if (!result)
            return luaL_error(L, "Component %s had no mesh with id %s", lua_tostring(L, 1), lua_tostring(L, 2));

        lua_pushboolean(L, enabled);
        assert((top + 1) == lua_gettop(L));
        return 1;
    }

    static const luaL_reg MODEL_COMP_FUNCTIONS[] =
    {
            {"play",    LuaModelComp_Play}, // Deprecated
            {"play_anim", LuaModelComp_PlayAnim},
            {"cancel",  LuaModelComp_Cancel},
            {"get_go",  LuaModelComp_GetGO},
            {"set_constant",    LuaModelComp_SetConstant},
            {"reset_constant",  LuaModelComp_ResetConstant},

            {"set_mesh_enabled",  LuaModelComp_SetMeshEnabled},
            {"get_mesh_enabled",  LuaModelComp_GetMeshEnabled},
            {0, 0}
    };

    void ScriptModelRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, MODEL_MODULE_NAME, MODEL_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }

}
