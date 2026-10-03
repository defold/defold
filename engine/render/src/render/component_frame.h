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

#ifndef DM_COMPONENT_FRAME_H
#define DM_COMPONENT_FRAME_H
#include "render.h"
#include "render_command.h"
namespace dmRender
{
    struct ComponentFrame;
    typedef ComponentFrame* HComponentFrame;
    // Prepared component output, not graphics packets. No component/world/Lua
    // pointers cross consumption. Resource mutation barriers protect handles.
    HComponentFrame NewComponentFrame();
    void DeleteComponentFrame(HComponentFrame frame);
    void BeginComponentFrameCapture(HRenderContext context, HComponentFrame frame);
    bool EndComponentFrameCapture(HRenderContext context);
    bool CaptureComponentCommands(HRenderContext context, Command* commands, uint32_t count, bool release);
    Result CaptureComponentDraw(HRenderContext context, HPredicate predicate, HNamedConstantBuffer constants);
    HRenderContext NewComponentFrameConsumer(HRenderContext source);
    void DeleteComponentFrameConsumer(HRenderContext context);
    void ConsumeComponentFrame(HRenderContext consumer, HComponentFrame frame);
    uint64_t GetComponentFrameCapacity(HComponentFrame frame);
    uint64_t GetComponentFrameUsedBytes(HComponentFrame frame);
    void EnableComponentFrames(HRenderContext context, bool enabled);
    bool AreComponentFramesEnabled(HRenderContext context);
}
#endif
