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

#ifndef DM_PROFILER_H
#define DM_PROFILER_H

#include <stdint.h>

namespace dmRender
{
    typedef struct RenderContext*   HRenderContext;
    typedef struct FontMap*         HFontMap;
} // dmRender

namespace dmGraphics
{
    typedef void* HContext;
} // dmGraphics

namespace dmProfile
{
    typedef void* HProfile;
} // dmProfile

namespace dmProfiler
{
    void SetUpdateFrequency(uint32_t update_frequency);
    void ToggleProfiler();
    void RenderProfiler(dmProfile::HProfile profile, dmGraphics::HContext graphics_context, dmRender::HRenderContext render_context, dmRender::HFontMap system_font_map);

} // dmProfiler

#endif // DM_PROFILER_H
