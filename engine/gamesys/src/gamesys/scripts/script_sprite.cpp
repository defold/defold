// Copyright 2020-2026 The Defold Foundation
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

#include <graphics/graphics_packet.h>
#include <float.h>
#include <stdio.h>
#include <assert.h>

#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <script/script.h>

#include "gamesys.h"
#include <dlib/time.h>
#include <gamesys/gamesys_ddf.h>
#include "../gamesys_private.h"

#include "script_sprite.h"
#include "../components/comp_sprite.h"

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
     * @language Lua
     */

    /*# Sprite flipbook playback properties
     * @struct
     * @name sprite.play_properties
     * @member offset? [type:number] Normalized initial animation cursor.
     * @member playback_rate? [type:number] Positive animation playback rate.
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

    /*# [type:vector4] sprite slice
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
    *   local cursor = go.get("#sprite", "cursor")
    * end
    * ```
    *
    * How to animate the cursor from 0.0 to 1.0 using linear easing for 2.0 seconds:
    *
    * ```lua
    * function init(self)
    *   -- Set the cursor on component "sprite" to make the animation go from 0
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

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

        dmGameSystemDDF::SetFlipHorizontal msg;
        msg.m_Flip = (uint32_t)lua_toboolean(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor, &msg, sizeof(msg), 0);
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

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

        dmGameSystemDDF::SetFlipVertical msg;
        msg.m_Flip = (uint32_t)lua_toboolean(L, 2);

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor, &msg, sizeof(msg), 0);
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

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.
        dmhash_t name_hash = dmScript::CheckHashOrString(L, 2);

        dmGameSystemDDF::ResetConstant msg;
        msg.m_NameHash = name_hash;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::ResetConstant::m_DDFDescriptor, &msg, sizeof(msg), 0);
        assert(top == lua_gettop(L));
        return 0;
    }

    // Docs intentionally left out until we decide to go public with this function
    int SpriteComp_SetScale(lua_State* L)
    {
        int top = lua_gettop(L);

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.

        dmVMath::Vector3* scale = dmScript::CheckVector3(L, 2);

        dmGameSystemDDF::SetScale msg;
        msg.m_Scale = *scale;

        dmMessage::URL receiver;
        dmMessage::URL sender;
        dmScript::ResolveURL(L, 1, &receiver, &sender);

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)dmGameSystemDDF::SetScale::m_DDFDescriptor, &msg, sizeof(msg), 0);
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
     * @param [complete_function] [type:fun(self:script_instance, message_id:hash, message:message.sprite.animation_done, sender:url)] function to call when the animation has completed.
     *
     * `self`
     * : [type:script_instance] The current script instance.
     *
     * `message_id`
     * : [type:hash] The name of the completion message, `"animation_done"`.
     *
     * `message`
     * : [type:message.sprite.animation_done] Information about the completion.
     *
     * `sender`
     * : [type:url] The invoker of the callback: the sprite component.
     *
     * @param [play_properties] [type:sprite.play_properties] optional playback properties
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

        (void)CheckGoInstance(L); // left to check that it's not called from incorrect context.
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

        dmMessage::Post(&sender, &receiver, dmGameSystemDDF::PlayAnimation::m_DDFDescriptor->m_NameHash, 0, (uintptr_t)functionref, (uintptr_t)dmGameSystemDDF::PlayAnimation::m_DDFDescriptor, &msg, sizeof(msg), 0);
        return 0;
    }

    // Experimental diagnostics for the sprite-only inline snapshot benchmark.
    static int SpriteComp_GetSnapshotStats(lua_State* L)
    {
        DM_LUA_STACK_CHECK(L, 1);
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        void* world = dmGameObject::GetWorld(collection, dmGameObject::GetComponentTypeIndex(collection, dmHashString64("spritec")));
        SpriteSnapshotStats stats;
        GetSpriteSnapshotStats(world, &stats);
        SpriteContext* context = (SpriteContext*)dmGameObject::GetContext(collection, dmGameObject::GetComponentTypeIndex(collection, dmHashString64("spritec")));
        lua_newtable(L);
        dmGraphics::GraphicsPacketStats packet;
        dmGraphics::GetGraphicsPacketStats(&packet);
        lua_pushstring(L, dmGraphics::IsExternalGraphicsProducer() ? (context->m_ComponentFrames ? "component-web-snapshots" : stats.m_Threaded ? "component-web-threaded" : "component-web-serialized") : dmGraphics::IsRenderGraphicsOwnerActive() ? (packet.m_Mode == 1 ? "renderframe-inline" : "renderframe-threaded") :
            packet.m_Mode ? (packet.m_Mode == 1 ? "graphics-inline" : "graphics-threaded") :
            (stats.m_Threaded ? "snapshot-threaded" : (stats.m_Inline ? "snapshot-inline" : "existing")));
        lua_setfield(L, -2, "mode");
        lua_pushnumber(L, (lua_Number)packet.m_Mode);
        lua_setfield(L, -2, "graphics_packet_mode");
        lua_pushnumber(L, (lua_Number)packet.m_Submitted);
        lua_setfield(L, -2, "graphics_packet_submitted");
        lua_pushnumber(L, (lua_Number)packet.m_Completed);
        lua_setfield(L, -2, "graphics_packet_completed");
        lua_pushnumber(L, (lua_Number)packet.m_Commands);
        lua_setfield(L, -2, "graphics_packet_commands");
        lua_pushnumber(L, (lua_Number)packet.m_CopiedBytes);
        lua_setfield(L, -2, "graphics_packet_copied_bytes");
        lua_pushnumber(L, (lua_Number)packet.m_WaitUs);
        lua_setfield(L, -2, "graphics_packet_wait_us");
        lua_pushnumber(L, (lua_Number)packet.m_ExecuteUs);
        lua_setfield(L, -2, "graphics_packet_execute_us");
        lua_pushnumber(L, (lua_Number)packet.m_SynchronousCalls);
        lua_setfield(L, -2, "graphics_packet_synchronous_calls");
        lua_pushnumber(L, (lua_Number)packet.m_OverlapFrames);
        lua_setfield(L, -2, "graphics_packet_overlap_frames");
        lua_pushnumber(L, (lua_Number)packet.m_CapacityBytes);
        lua_setfield(L, -2, "graphics_packet_capacity_bytes");
        lua_pushnumber(L, (lua_Number)packet.m_GrowthPeakBytes);
        lua_setfield(L, -2, "graphics_packet_growth_peak_bytes");
        lua_pushnumber(L, (lua_Number)packet.m_LastFrameBytes);
        lua_setfield(L, -2, "graphics_packet_last_frame_bytes");
        lua_pushnumber(L, (lua_Number)packet.m_MaxFrameBytes);
        lua_setfield(L, -2, "graphics_packet_max_frame_bytes");
        lua_pushnumber(L, (lua_Number)packet.m_MaxOutstanding);
        lua_setfield(L, -2, "graphics_packet_max_outstanding");
        lua_pushnumber(L, packet.m_StackReservedBytes);
        lua_setfield(L, -2, "graphics_packet_stack_reserved_bytes");
        lua_pushnumber(L, packet.m_BufferMetadataBytes);
        lua_setfield(L, -2, "graphics_packet_buffer_metadata_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_PayloadUsedBytes);
        lua_setfield(L, -2, "payload_used_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_SlotsPayloadUsedBytes);
        lua_setfield(L, -2, "slots_payload_used_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_FrameCapacityBytes);
        lua_setfield(L, -2, "frame_capacity_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_RendererCpuCapacityBytes);
        lua_setfield(L, -2, "renderer_cpu_capacity_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_ConstantBufferCapacityBytes);
        lua_setfield(L, -2, "constant_buffer_capacity_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_RendererGpuLogicalBytes);
        lua_setfield(L, -2, "renderer_gpu_logical_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_RetainedResourceReportedBytes);
        lua_setfield(L, -2, "retained_resource_reported_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_FrameGrowthPeakBytes);
        lua_setfield(L, -2, "frame_growth_peak_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_CaptureCount);
        lua_setfield(L, -2, "capture_count");
        lua_pushnumber(L, (lua_Number)stats.m_CaptureTotalUs);
        lua_setfield(L, -2, "capture_total_us");
        lua_pushnumber(L, (lua_Number)stats.m_RecordBytes);
        lua_setfield(L, -2, "record_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_BoundBytes);
        lua_setfield(L, -2, "bound_bytes");
        lua_pushnumber(L, (lua_Number)stats.m_SpriteCount);
        lua_setfield(L, -2, "sprite_count");
        lua_pushnumber(L, (lua_Number)stats.m_BindingCount);
        lua_setfield(L, -2, "binding_count");
        lua_pushnumber(L, (lua_Number)stats.m_GeometryCount);
        lua_setfield(L, -2, "geometry_count");
        lua_pushnumber(L, (lua_Number)stats.m_ConstantBlockCount);
        lua_setfield(L, -2, "constant_block_count");
        lua_pushnumber(L, (lua_Number)stats.m_AttributeBlockCount);
        lua_setfield(L, -2, "attribute_block_count");
        lua_pushnumber(L, (lua_Number)stats.m_RetainedReferenceCount);
        lua_setfield(L, -2, "retained_reference_count");
        if (context->m_ComponentFrames)
        {
            lua_pushnumber(L, (lua_Number)context->m_RenderFrameCapacityBytes);
            lua_setfield(L, -2, "component_frame_capacity_bytes");
            lua_pushnumber(L, (lua_Number)context->m_RenderFrameUsedBytes);
            lua_setfield(L, -2, "component_frame_used_bytes");
        }
        if (dmGraphics::IsRenderGraphicsOwnerActive())
        {
            lua_pushnumber(L, (lua_Number)context->m_RenderFrameUsedBytes);
            lua_setfield(L, -2, "renderframe_arena_used_bytes");
            lua_pushnumber(L, (lua_Number)context->m_RenderFrameCapacityBytes);
            lua_setfield(L, -2, "renderframe_capacity_bytes");
            lua_pushnumber(L, (lua_Number)context->m_RenderFrameGrowthPeakBytes);
            lua_setfield(L, -2, "renderframe_growth_peak_bytes");
            lua_pushnumber(L, context->m_RenderFrameReferences);
            lua_setfield(L, -2, "renderframe_retained_references");
        }
        uint64_t device_bytes;
        if (dmGraphics::GetRenderDeviceAllocatedBytes(dmRender::GetGraphicsContext(context->m_RenderContext), &device_bytes))
        {
            lua_pushnumber(L, (lua_Number)device_bytes);
            lua_setfield(L, -2, "metal_device_allocated_bytes");
        }
        lua_pushnumber(L, (lua_Number)context->m_MixedPrepareTime);
        lua_setfield(L, -2, "mixed_prepare_us_total");
        lua_pushnumber(L, (lua_Number)context->m_MixedPrepareCount);
        lua_setfield(L, -2, "mixed_prepare_count");
        lua_pushnumber(L, (lua_Number)context->m_MixedCapacityBytes);
        lua_setfield(L, -2, "mixed_packet_capacity_bytes");
        lua_pushnumber(L, (lua_Number)context->m_MixedUploadCapacityBytes);
        lua_setfield(L, -2, "mixed_upload_capacity_bytes");
        lua_pushnumber(L, (lua_Number)context->m_MixedUploadUsedBytes);
        lua_setfield(L, -2, "mixed_upload_used_bytes");
        lua_pushnumber(L, (lua_Number)context->m_MixedUploadGrowthPeak);
        lua_setfield(L, -2, "mixed_upload_growth_peak_bytes");
        lua_pushnumber(L, context->m_MixedPacketCount);
        lua_setfield(L, -2, "mixed_packet_count");
        lua_pushnumber(L, context->m_SnapshotCommandBytes);
        lua_setfield(L, -2, "captured_command_capacity_bytes");
        if (context->m_RenderThread)
        {
            lua_pushnumber(L, context->m_SnapshotCommandBytes);
            lua_setfield(L, -2, "thread_command_capacity_bytes");
            lua_pushnumber(L, 0x80000);
            lua_setfield(L, -2, "thread_stack_bytes");
            dmRender::RenderThreadStats thread;
            dmRender::GetRenderThreadStats(context->m_RenderThread, &thread);
            lua_pushnumber(L, (lua_Number)thread.m_SimulationsDuringRender);
            lua_setfield(L, -2, "thread_simulations_during_render");
            lua_pushnumber(L, (lua_Number)thread.m_SimulationOverlapUs);
            lua_setfield(L, -2, "thread_simulation_overlap_us");
            lua_pushnumber(L, (lua_Number)thread.m_CapturesWithConsumerOutstanding);
            lua_setfield(L, -2, "thread_captures_with_consumer_outstanding");
            lua_pushnumber(L, (lua_Number)thread.m_Submitted);
            lua_setfield(L, -2, "thread_submitted");
            lua_pushnumber(L, (lua_Number)thread.m_Completed);
            lua_setfield(L, -2, "thread_completed");
            lua_pushnumber(L, (lua_Number)thread.m_ProducerWaitUs);
            lua_setfield(L, -2, "thread_producer_wait_us");
            lua_pushnumber(L, (lua_Number)thread.m_RenderUs);
            lua_setfield(L, -2, "thread_render_us");
            lua_pushnumber(L, (lua_Number)thread.m_FrameAgeUs);
            lua_setfield(L, -2, "thread_frame_age_us");
            lua_pushnumber(L, (lua_Number)thread.m_Controls);
            lua_setfield(L, -2, "thread_controls");
            lua_pushnumber(L, (lua_Number)thread.m_ControlUs);
            lua_setfield(L, -2, "thread_control_us");
            lua_pushnumber(L, (lua_Number)thread.m_MaxOutstanding);
            lua_setfield(L, -2, "thread_max_outstanding");
            lua_pushnumber(L, (lua_Number)thread.m_SlotCount);
            lua_setfield(L, -2, "thread_slot_count");
            lua_pushnumber(L, (lua_Number)thread.m_ControlCapacity);
            lua_setfield(L, -2, "thread_control_capacity");
            lua_pushnumber(L, (lua_Number)thread.m_QueueBytes);
            lua_setfield(L, -2, "thread_queue_bytes");
        }
        return 1;
    }

    static int SpriteComp_SnapshotPause(lua_State* L)
    {
        if (dmGraphics::SetGraphicsPacketsPaused(lua_toboolean(L, 1))) return 0;
        dmGameObject::HCollection collection = dmGameObject::GetCollection(CheckGoInstance(L));
        SpriteContext* context = (SpriteContext*)dmGameObject::GetContext(collection, dmGameObject::GetComponentTypeIndex(collection, dmHashString64("spritec")));
        if (!context->m_SnapshotPause)
            return luaL_error(L, "snapshot pause requires render.sprite_snapshot=2");
        context->m_SnapshotPause(context->m_SnapshotContext, lua_toboolean(L, 1));
        return 0;
    }

    // Internal benchmark clock: independent of wall-clock corrections. On macOS,
    // monotonic elapsed time excludes system sleep, matching engine frame timing.
    static int SpriteComp_SnapshotClock(lua_State* L)
    {
        lua_pushnumber(L, (lua_Number)dmTime::GetMonotonicTime() / 1000000.0);
        return 1;
    }

    static const luaL_reg SPRITE_COMP_FUNCTIONS[] =
    {
            {"_get_snapshot_stats", SpriteComp_GetSnapshotStats},
            {"_snapshot_pause", SpriteComp_SnapshotPause},
            {"_snapshot_clock", SpriteComp_SnapshotClock},
            {"set_hflip",       SpriteComp_SetHFlip},
            {"set_vflip",       SpriteComp_SetVFlip},
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
