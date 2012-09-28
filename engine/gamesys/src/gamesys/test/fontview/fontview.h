#ifndef DM_RENDER_FONTVIEW_H
#define DM_RENDER_FONTVIEW_H

#include <stdint.h>

#include <resource/resource.h>

#include <graphics/graphics.h>

#include <hid/hid.h>

#include <render/render.h>
#include <render/font_renderer.h>

#include <render/render_ddf.h>

namespace dmFontView
{
    struct Context
    {
        const char* m_TestString;
        dmGraphics::HContext m_GraphicsContext;
        dmResource::HFactory m_Factory;
        dmHID::HContext m_HidContext;
        dmRender::HFontMap m_FontMap;
        dmRender::HRenderContext m_RenderContext;
        uint32_t m_ScreenWidth;
        uint32_t m_ScreenHeight;
    };

    bool Init(Context* context, int argc, char *argv[]);
    int32_t Run(Context* context);
    void Finalize(Context* context);
}

#endif // DM_RENDER_FONTVIEW_H
