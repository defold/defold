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

    HRenderProfile NewRenderProfile(float fps);
    void UpdateRenderProfile(HRenderProfile render_profile, dmProfile::HProfile profile);
    void DeleteRenderProfile(HRenderProfile render_profile);

    void Draw(HRenderProfile render_profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map);
}

#endif
