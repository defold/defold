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

#ifndef DM_RENDER_FRAME_H
#define DM_RENDER_FRAME_H

#include "render.h"
#include "render_frame_storage.h"

namespace dmRender
{
    // Reserved renderer table; component payloads must not use this type.
    static const uint32_t RENDER_FRAME_INFO_TABLE_TYPE = UINT32_MAX - 4;

    /**
     * Choose who prepares and cleans up a frame. CONSUMER does both on the render
     * side, around component consumption and render passes. LEGACY keeps them on
     * the game side for inline components and extensions that have not migrated.
     * This choice does not enable threading.
     */
    enum RenderFramePreparation
    {
        RENDER_FRAME_PREPARATION_CONSUMER,
        RENDER_FRAME_PREPARATION_LEGACY,
    };

    /**
     * Producer snapshot for a complete frame. The frame ID is in RenderFrameView.
     * Width and height describe the default window when recording, not a resize request.
     */
    struct RenderFrameInfo
    {
        uint32_t m_Width;
        uint32_t m_Height;
        float    m_Time;
        float    m_Dt;
    };

    /**
     * Size fixed frame storage from the renderer's command capacity.
     * Reserves worst-case fixed operands/alignment per command, frame metadata, pass tables
     * and one reference to the render script's declared resource owner.
     * Component capacities are added separately; the resulting arena is shared, not partitioned.
     * Constant snapshots use separate growable storage and do not consume the fixed payload.
     * @param context Game-owned renderer with the configured command capacity
     * @param component_limits Additional component payload, table and reference capacities
     * @param limits Output combined capacities, written only on success
     * @return RENDER_FRAME_OK, or RENDER_FRAME_CAPACITY_EXCEEDED if derived payload/table capacity overflows
     */
    RenderFrameResult CalculateRenderFrameLimits(HRenderContext context, const RenderFrameLimits& component_limits, RenderFrameLimits* limits);

    /**
     * Set game-owned frame timing before component capture and render-script messages.
     * Does not modify execution state or begin a graphics frame.
     * @param context Render context owned by the game owner
     * @param time Accumulated simulation time
     * @param dt Simulation delta time
     */
    void BeginRenderFrameRecording(HRenderContext context, float time, float dt);

    /**
     * Snapshot timing and the default window extent on the game owner. Window queries
     * remain inline-only until the graphics-owner service provides cached surface state.
     * @param context Render context
     * @return Recording metadata for the current default surface
     */
    RenderFrameInfo GetRenderFrameInfo(HRenderContext context);

    /**
     * Append one metadata table before recording passes. Failure poisons the frame.
     * @param frame Building frame
     * @param info Values to copy, with no borrowed game state
     * @return Frame allocation result
     */
    RenderFrameResult RecordRenderFrameInfo(HRenderFrameStorage frame, const RenderFrameInfo& info);

    /**
     * Retain a script's resource owner and its dependencies into an external frame.
     * Called on the game owner before Lua update. The callback adopts references into
     * frame storage, including rollback on failure; it does not expose factory types
     * to the renderer. The resource owner must remain unchanged until those references
     * retire. Direct synchronous scratch updates borrow the caller's existing ownership.
     */
    typedef RenderFrameResult (*FRenderScriptResourceCapture)(void* context, void* resource, HRenderFrameStorage frame);

    /** Set the game-owned resource capture callback; arguments are borrowed while recording. */
    void SetRenderScriptInstanceResourceCapture(HRenderScriptInstance instance, FRenderScriptResourceCapture capture, void* context, void* resource);

    /**
     * Refresh a declared resource after replacement, including matching inherited
     * material/compute and target-attachment texture bindings. Unrelated bindings stay
     * unchanged. Call only with readers drained and both game/render ownership held;
     * resource reload currently uses inline execution. Arrays may be null when count is zero.
     * @param instance Script containing the registered resource name
     * @param name_hash Registered resource name
     * @param resource Replacement native/render handle
     * @param previous_textures Old target attachment handles
     * @param textures Replacement attachment handles; zero clears a removed attachment
     * @param texture_count Number of corresponding attachment handles
     */
    void RefreshRenderScriptInstanceRenderResource(HRenderScriptInstance instance, dmhash_t name_hash, uint64_t resource,
                                                   const dmGraphics::HTexture* previous_textures, const dmGraphics::HTexture* textures, uint32_t texture_count);

    /**
     * Clear matching inherited bindings immediately before destroying their resource.
     * Releasing one of several resource owners must leave these bindings intact.
     * Call only with readers drained and both game/render ownership held, as for refresh.
     * The texture array lists target attachments and may be null when texture_count is zero.
     */
    void ClearRenderResourceBindings(HRenderContext context, RenderResourceType type, uint64_t resource,
                                     const dmGraphics::HTexture* textures, uint32_t texture_count);

    /**
     * Record game-owned Lua updates and copy their pass inputs into a building frame.
     * Script or storage failure invalidates the whole frame; no commands execute here.
     * @param instance Game-owned render-script instance
     * @param dt Simulation delta time
     * @param frame Building frame receiving owned commands
     * @return Recording status
     */
    RenderFrameResult RecordRenderScriptInstance(HRenderScriptInstance instance, float dt, HRenderFrameStorage frame);

    /**
     * Set the renderer's time from the saved frame, before component consumption and
     * render commands run. Uses the captured time, not the game's current time.
     *
     * Consumer preparation clears submitted lights and the render list so components
     * can fill them from captured inputs. Persistent items and their caches are kept.
     * Legacy preparation keeps the lights and items already submitted inline.
     *
     * Does not call dmGraphics::BeginFrame. Inline callers do that before Lua recording;
     * threaded callers do it on the render owner before consuming the frame.
     * @param context Render context, accessed on the render owner
     * @param frame Submitted frame with exactly one metadata table
     * @param preparation Consumer preparation or legacy inline preparation
     */
    void BeginRenderFrameConsumption(HRenderContext context, const RenderFrameView& frame, RenderFramePreparation preparation);

    /**
     * Clear render objects, debug and text data after all render passes and component
     * EndFrame callbacks. Runs on the render owner for consumer preparation only.
     * Does nothing for legacy preparation; the inline caller cleans up after PostUpdate.
     * Canceled frames that were never submitted do not need this call.
     * @param context Render context, accessed on the render owner
     * @param preparation Same choice passed to BeginRenderFrameConsumption
     */
    void EndRenderFrameConsumption(HRenderContext context, RenderFramePreparation preparation);

    /**
     * Record the no-render-script viewport, clear and draw using the same command
     * representation as Lua passes. Resource-free and valid with no component inputs.
     * @param context Producer render context
     * @param frame Building frame
     * @param info Recording-time viewport extent
     * @param clear_color Packed RGBA clear color used by the engine fallback
     * @return Frame recording result
     */
    RenderFrameResult RecordDefaultRenderCommands(HRenderContext context, HRenderFrameStorage frame, const RenderFrameInfo& info, uint32_t clear_color);
}

#endif // DM_RENDER_FRAME_H
