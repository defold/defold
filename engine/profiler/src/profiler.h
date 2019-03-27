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
    typedef struct Context* HContext;
} // dmGraphics

namespace dmProfile
{
    typedef struct Profile* HProfile;
} // dmProfile

namespace dmProfiler
{
    void SetUpdateFrequency(uint32_t update_frequency);
    void ToggleProfiler();
    void RenderProfiler(dmProfile::HProfile profile, dmGraphics::HContext graphics_context, dmRender::HRenderContext render_context, dmRender::HFontMap system_font_map);

} // dmProfiler

#endif // DM_PROFILER_H
