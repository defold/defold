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
