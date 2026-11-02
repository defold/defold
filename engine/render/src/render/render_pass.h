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

#ifndef DM_RENDER_PASS_H
#define DM_RENDER_PASS_H

#include "render.h"
#include "render_frame.h"

namespace dmRender
{
    // Reserved renderer table; component payloads must not use this type.
    static const uint32_t RENDER_FRAME_INFO_TABLE_TYPE = UINT32_MAX - 4;

    /**
     * Producer snapshot for a complete frame. The frame ID is in RenderFrameView.
     * Surface 0 denotes this render context's default window; other IDs are reserved
     * for owner-routed surfaces. Extents describe recording-time dimensions, not a
     * request to resize. Surface lifetime/replacement routing belongs to D2.3/2.4.
     */
    struct RenderFrameInfo
    {
        uint64_t m_SurfaceId;
        uint32_t m_Width;
        uint32_t m_Height;
        float    m_Time;
        float    m_Dt;
    };

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
    RenderFrameResult RecordRenderFrameInfo(HRenderFrame frame, const RenderFrameInfo& info);

    /**
     * Record game-owned Lua updates and copy their pass inputs into a building frame.
     * Script or storage failure invalidates the whole frame; no commands execute here.
     * @param instance Game-owned render-script instance
     * @param dt Simulation delta time
     * @param frame Building frame receiving owned commands
     * @return Recording status
     */
    RenderFrameResult RecordRenderScriptInstance(HRenderScriptInstance instance, float dt, HRenderFrame frame);

    /**
     * Start execution from the published frame metadata. Called before component
     * consumption and pass replay; the consumer never reads producer timing.
     * @param context Render-owner execution context
     * @param frame Published frame with exactly one metadata table
     */
    void BeginRenderFrameConsumption(HRenderContext context, const RenderFrameView& frame);

    /**
     * Record the no-render-script viewport, clear and draw using the same command
     * representation as Lua passes. Resource-free and valid with no component inputs.
     * @param context Producer render context
     * @param frame Building frame
     * @param info Recording-time viewport extent
     * @param clear_color Packed RGBA clear color used by the engine fallback
     * @return Frame recording result
     */
    RenderFrameResult RecordDefaultRenderCommands(HRenderContext context, HRenderFrame frame, const RenderFrameInfo& info, uint32_t clear_color);
}

#endif // DM_RENDER_PASS_H
