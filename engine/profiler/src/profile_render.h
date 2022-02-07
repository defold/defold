// Copyright 2020 The Defold Foundation
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

#ifndef DM_PROFILE_RENDER_H
#define DM_PROFILE_RENDER_H

namespace dmRender
{
    typedef struct FontMap*         HFontMap;
    typedef struct RenderContext*   HRenderContext;
}

namespace dmProfile
{
    typedef struct Profile* HProfile;
}

namespace dmProfileRender
{
    typedef struct RenderProfile* HRenderProfile;

    enum ProfilerMode {
        PROFILER_MODE_RUN = 1,
        PROFILER_MODE_PAUSE = 2,
        PROFILER_MODE_SHOW_PEAK_FRAME = 3,
        PROFILER_MODE_RECORD = 4
    };

    enum ProfilerViewMode
    {
        PROFILER_VIEW_MODE_FULL = 1,
        PROFILER_VIEW_MODE_MINIMIZED = 2
    };

    HRenderProfile NewRenderProfile(float fps);
    void UpdateRenderProfile(HRenderProfile render_profile, dmProfile::HProfile profile);
    void SetMode(HRenderProfile render_profile, ProfilerMode mode);
    void SetViewMode(HRenderProfile render_profile, ProfilerViewMode view_mode);
    void SetWaitTime(HRenderProfile render_profile, bool include_wait_time);
    void AdjustShownFrame(HRenderProfile render_profile, int distance);
    void ShowRecordedFrame(HRenderProfile render_profile, int frame);
    int GetRecordedFrameCount(HRenderProfile render_profile);
    void DeleteRenderProfile(HRenderProfile render_profile);

    void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map);
}

#endif
