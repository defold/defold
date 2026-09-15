// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0.

#ifndef DM_FONT_RENDERER_API_H
#define DM_FONT_RENDERER_API_H

#include <stdint.h>

#include <dlib/safe_windows.h>
#include <graphics/graphics.h>

#include "../render_private.h"
#include "fontmap.h"

namespace dmRender
{
    struct FontRenderBackend;
    typedef FontRenderBackend* HFontRenderBackend;

    HFontRenderBackend CreateFontRenderBackend();
    void DestroyFontRenderBackend(HFontRenderBackend backend);
    uint32_t GetFontVertexSize(HFontRenderBackend backend);
    dmGraphics::HVertexDeclaration CreateVertexDeclaration(HFontRenderBackend backend, dmGraphics::HContext context);
    void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text, TextLayoutSettings* settings, TextMetrics* metrics);
    uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& text_entry, float sdf_scale, float recip_w, float recip_h, uint8_t* vertices, uint32_t num_vertices);
}

#endif // DM_FONT_RENDERER_API_H
