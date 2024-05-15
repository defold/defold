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

#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <script/script.h>

#include "gamesys.h"
#include <gamesys/gamesys_ddf.h>
#include "../gamesys_private.h"

#include "script_sprite.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}


namespace dmGameSystem
{
    /*# Sprite API documentation
     *
     * Functions, messages and properties used to manipulate sprite components.
     *
     * @document
     * @name Sprite
     * @namespace sprite
     */

    /*# [type:vector3] sprite size
     *
     * The size of the sprite, not allowing for any additional scaling that may be applied.
     * The type of the property is vector3. It is not possible to set the size if the size mode
     * of the sprite is set to auto.
     *
     * @name size
     * @property
     *
     * @examples
     *
     * How to query a sprite's size, either as a vector or selecting a specific dimension:
     *
     * ```lua
     * function init(self)
     *   -- get size from component "sprite"
     *   local size = go.get("#sprite", "size")
     *   local sx = go.get("#sprite", "size.x")
     *   -- do something useful
     *   assert(size.x == sx)
     * end
     * ```
     */

    /*# [type:vector3] sprite slice
     *
     * The slice values of the sprite. The type of the property is a vector4 that corresponds to
     * the left, top, right, bottom values of the sprite in the editor.
     * It is not possible to set the slice property if the size mode of the sprite is set to auto.
     *
     * @name slice
     * @property
     *
     * @examples
     *
     * How to query a sprite's slice values, either as a vector or selecting a specific dimension:
     *
     * ```lua
     * function init(self)
     *   local slice = go.get("#sprite", "slice")
     *   local slicex = go.get("#sprite", "slice.x")
     *   assert(slice.x == slicex)
     * end
     * ```
     *
     * Animate the slice property with go.animate:
     *
     * ```lua
     * function init(self)
     *   -- animate the entire slice vector at once
     *   go.animate("#sprite", "slice", go.PLAYBACK_LOOP_PINGPONG, vmath.vector4(96, 96, 96, 96), go.EASING_INCUBIC, 2)
     *   -- or animate a single component
     *   go.animate("#sprite", "slice.y", go.PLAYBACK_LOOP_PINGPONG, 32, go.EASING_INCUBIC, 8)
     * end
     * ```
     */

    /*# [type:vector3] sprite scale
     *
     * The non-uniform scale of the sprite. The type of the property is vector3.
     *
     * @name scale
     * @property
     *
     * @examples
     *
     * How to scale a sprite independently along the X and Y axis:
     *
     * ```lua
     * function init(self)
     *   -- Double the y-axis scaling on component "sprite"
     * 	 local yscale = go.get("#sprite", "scale.y")
     * 	 go.set("#sprite", "scale.y", yscale * 2)
     * end
     * ```
     */

    /*# [type:hash] sprite image
     *
     * The image used when rendering the sprite. The type of the property is hash.
     *
     * @name image
     * @property
     *
     * @examples
     *
     * How to set image using a script property (see [ref:resource.atlas])
     *
     * ```lua
     * go.property("my_image", resource.atlas("/atlas.atlas"))
     * function init(self)
     *   go.set("#sprite", "image", self.my_image)
     * end
     * ```
     *
     * See [ref:resource.set_texture] for an example on how to set the texture of an atlas.
     */

    /*# [type:hash] sprite material
     *
     * The material used when rendering the sprite. The type of the property is hash.
     *
     * @name material
     * @property
     *
     * @examples
     *
     * How to set material using a script property (see [ref:resource.material])
     *
     * ```lua
     * go.property("my_material", resource.material("/material.material"))
     * function init(self)
     *   go.set("#sprite", "material", self.my_material)
     * end
     * ```
     */

    /*# [type:number] sprite cursor
    *
    * The normalized animation cursor. The type of the property is number.
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
    *   -- Get the cursor value on component "sprite"
    *   cursor = go.get("#sprite", "cursor")
    * end
    * ```
    *
    * How to animate the cursor from 0.0 to 1.0 using linear easing for 2.0 seconds:
    *
    * ```lua
    * function init(self)
    *   -- Get the current value on component "sprite"
    *   go.set("#sprite", "cursor", 0.0)
    *   -- Animate the cursor value
    *   go.animate("#sprite", "cursor", go.PLAYBACK_LOOP_FORWARD, 1.0, go.EASING_LINEAR, 2)
    * end
    * ```
    */

    /*# [type:number] sprite playback_rate
    *
    * The animation playback rate. A multiplier to the animation playback rate. The type of the property is [type:number].
    *
    * The playback_rate is a non-negative number, a negative value will be clamped to 0.
    *
    * @name playback_rate
    * @property
    *
    * @examples
    *
    * How to set the playback_rate on component "sprite" to play at double the current speed:
    *
    * ```lua
    * function init(self)
    *   -- Get the current value on component "sprite"
    *   playback_rate = go.get("#sprite", "playback_rate")
    *   -- Set the playback_rate to double the previous value.
    *   go.set("#sprite", "playback_rate", playback_rate * 2)
    * end
    * ```
    */

    /*# [type:hash] sprite animation
    *
    * [mark:READ ONLY] The current animation id. An animation that plays currently for the sprite. The type of the property is [type:hash].
    *
    * @name animation
    * @property
    *
    * @examples
    *
    * How to get the `animation` on component "sprite":
    *
    * ```lua
    * function init(self)
    *   local animation = go.get("#sprite", "animation")
    * end
    * ```
    */

    /*# [type:hash] sprite frame_count
    *
    * [mark:READ ONLY] The frame count of the currently playing animation.
    *
    * @name frame_count
    * @property
    *
    * @examples
    *
    * How to get the `frame_count` on component "sprite":
    *
    * ```lua
    * function init(self)
    *   local frame_count = go.get("#sprite", "frame_count")
    * end
    * ```
    */

    /*# set horizontal flipping on a sprite's animations
     * Sets horizontal flipping of the provided sprite's animations.
     * The sprite is identified by its URL.
     * If the currently playing animation is flipped by default, flipping it again will make it appear like the original texture.
     *
     * @name sprite.set_hflip
     * @param url [type:string|hash|url] the sprite that should flip its animations
     * @param flip [type:boolean] `true` if the sprite should flip its animations, `false` if not
     * @examples
     *
     * How to flip a sprite so it faces the horizontal movement:
     *
     * ```lua
     * function update(self, dt)
     *   -- calculate self.velocity somehow
     *   sprite.set_hflip("#sprite", self.velocity.x < 0)
     * end
     * ```
     *
     * It is assumed that the sprite component has id "sprite" and that the original animations faces right.
     */
    int SpriteComp_SetHFlip(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmGameSystemDDF::SetFlipHorizontal msg;
        msg.m_Flip = (uint32_t)lua_toboolean(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# set vertical flipping on a sprite's animations
     * Sets vertical flipping of the provided sprite's animations.
     * The sprite is identified by its URL.
     * If the currently playing animation is flipped by default, flipping it again will make it appear like the original texture.
     *
     * @name sprite.set_vflip
     * @param url [type:string|hash|url] the sprite that should flip its animations
     * @param flip [type:boolean] `true` if the sprite should flip its animations, `false` if not
     * @examples
     *
     * How to flip a sprite in a game which negates gravity as a game mechanic:
     *
     * ```lua
     * function update(self, dt)
     *   -- calculate self.up_side_down somehow, then:
     *   sprite.set_vflip("#sprite", self.up_side_down)
     * end
     * ```
     *
     * It is assumed that the sprite component has id "sprite" and that the original animations are up-right.
     */
    int SpriteComp_SetVFlip(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmGameSystemDDF::SetFlipVertical msg;
        msg.m_Flip = (uint32_t)lua_toboolean(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /** DEPRECATED! set a shader constant for a sprite
     * Sets a shader constant for a sprite component.
     * The constant must be defined in the material assigned to the sprite.
     * Setting a constant through this function will override the value set for that constant in the material.
     * The value will be overridden until `sprite.reset_constant` is called.
     * Which sprite to set a constant for is identified by the URL.
     *
     * @name sprite.set_constant
     * @param url [type:string|hash|url] the sprite that should have a constant set
     * @param constant [type:string|hash] name of the constant
     * @param value [type:vector4] of the constant
     * @examples
     *
     * The following examples assumes that the sprite has id "sprite" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can set the constants defined there in the same manner.
     *
     * How to tint a sprite red:
     *
     * ```lua
     * function init(self)
     *   sprite.set_constant("#sprite", "tint", vmath.vector4(1, 0, 0, 1))
     * end
     * ```
     */
    int SpriteComp_SetConstant(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);
        dmVMath::Vector4* value = dmScript::CheckVector4(L, 3);

        dmGameSystemDDF::SetConstant msg;
        msg.m_NameHash = name_hash;
        msg.m_Value = *value;
        msg.m_Index = 0;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /** DEPRECATED! reset a shader constant for a sprite
     * Resets a shader constant for a sprite component.
     * The constant must be defined in the material assigned to the sprite.
     * Resetting a constant through this function implies that the value defined in the material will be used.
     * Which sprite to reset a constant for is identified by the URL.
     *
     * @name sprite.reset_constant
     * @param url [type:string|hash|url] the sprite that should have a constant reset
     * @param constant [type:string|hash] name of the constant
     * @examples
     *
     * The following examples assumes that the sprite has id "sprite" and that the default-material in builtins is used, which defines the constant "tint".
     * If you assign a custom material to the sprite, you can reset the constants defined there in the same manner.
     *
     * How to reset the tinting of a sprite:
     *
     * ```lua
     * function init(self)
     *   sprite.reset_constant("#sprite", "tint")
     * end
     * ```
     */
    int SpriteComp_ResetConstant(lua_State* L)
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

    // Docs intentionally left out until we decide to go public with this function
    int SpriteComp_SetScale(lua_State* L)
    {
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);

        dmVMath::Vector3* scale = dmScript::CheckVector3(L, 2);

        dmGameSystemDDF::SetScale msg;
        msg.m_Scale = *scale;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)dmGameSystemDDF::SetScale::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    /*# Play an animation on a sprite component
     * Play an animation on a sprite component from its tile set
     *
     * An optional completion callback function can be provided that will be called when
     * the animation has completed playing. If no function is provided,
     * a [ref:animation_done] message is sent to the script that started the animation.
     *
     * @name sprite.play_flipbook
     * @param url [type:string|hash|url] the sprite that should play the animation
     * @param id [type:string|hash] hashed id of the animation to play
     * @param [complete_function] [type:function(self, message_id, message, sender)] function to call when the animation has completed.
     *
     * `self`
     * : [type:object] The current object.
     *
     * `message_id`
     * : [type:hash] The name of the completion message, `"animation_done"`.
     *
     * `message`
     * : [type:table] Information about the completion:
     *
     * - [type:number] `current_tile` - the current tile of the sprite.
     * - [type:hash] `id` - id of the animation that was completed.
     *
     * `sender`
     * : [type:url] The invoker of the callback: the sprite component.
     *
     * @param [play_properties] [type:table] optional table with properties:
     *
     * `offset`
     * : [type:number] the normalized initial value of the animation cursor when the animation starts playing.
     *
     * `playback_rate`
     * : [type:number] the rate with which the animation will be played. Must be positive.
     *
     * @examples
     *
     * The following examples assumes that the model has id "sprite".
     *
     * How to play the "jump" animation followed by the "run" animation:
     *
     *```lua
     * local function anim_done(self, message_id, message, sender)
     *   if message_id == hash("animation_done") then
     *     if message.id == hash("jump") then
     *       -- jump animation done, chain with "run"
     *       sprite.play_flipbook(url, "run")
     *     end
     *   end
     * end
     * ```
     *
     * ```lua
     * function init(self)
     *   local url = msg.url("#sprite")
     *   sprite.play_flipbook(url, "jump", anim_done)
     * end
     * ```
     */
    int SpriteComp_PlayFlipBook(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 0);
        int top = lua_gettop(L);

        dmGameObject::HInstance instance = CheckGoInstance(L);
        dmhash_t id_hash = dmScript::CheckHashOrString(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        lua_Number offset = 0.0, playback_rate = 1.0;

        if (top > 3) // table with args
        {
            luaL_checktype(L, 4, LUA_TTABLE);
            lua_pushvalue(L, 4);

            lua_getfield(L, -1, "offset");
            offset = lua_isnil(L, -1) ? 0.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "playback_rate");
            playback_rate = lua_isnil(L, -1) ? 1.0 : luaL_checknumber(L, -1);
            lua_pop(L, 1);

            lua_pop(L, 1);
        }

        int functionref = 0;
        if (top > 2)
        {
            if (lua_isfunction(L, 3))
            {
                lua_pushvalue(L, 3);
                // NOTE: By convention m_FunctionRef is offset by LUA_NOREF, in order to have 0 for "no function"
                functionref = dmScript::RefInInstance(L) - LUA_NOREF;
            }
        }


        dmGameSystemDDF::PlayAnimation msg;
        msg.m_Id = id_hash;
        msg.m_Offset = offset;
        msg.m_PlaybackRate = playback_rate;

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PlayAnimation::m_DDFDescriptor->m_NameHash, (uintptr_t)instance, (uintptr_t)functionref, (uintptr_t)dmGameSystemDDF::PlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    static const luaL_reg SPRITE_COMP_FUNCTIONS[] =
    {
            {"set_hflip",       SpriteComp_SetHFlip},
            {"set_vflip",       SpriteComp_SetVFlip},
            {"set_constant",    SpriteComp_SetConstant},
            {"reset_constant",  SpriteComp_ResetConstant},
            {"set_scale",       SpriteComp_SetScale},
            {"play_flipbook",   SpriteComp_PlayFlipBook},
            {0, 0}
    };

    void ScriptSpriteRegister(const ScriptLibContext& context)
    {
        lua_State* L = context.m_LuaState;
        luaL_register(L, "sprite", SPRITE_COMP_FUNCTIONS);
        lua_pop(L, 1);
    }
}
