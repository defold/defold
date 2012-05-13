#ifndef DM_PROFILE_RENDER_H
#define DM_PROFILE_RENDER_H

#include <render/font_renderer.h>

namespace dmProfileRender
{
    void Draw(dmProfile::HProfile profile, dmRender::HRenderContext render_context, dmRender::HFontMap font_map);
}

#endif
