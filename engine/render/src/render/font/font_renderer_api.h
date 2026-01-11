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

#ifndef DM_FONT_RENDERER_API_H
#define DM_FONT_RENDERER_API_H

#include <stdint.h>

#include <graphics/graphics.h>

#include "../render_private.h"             // TextEntry

/**
 * Internal font rendering api
 * The design is to use a single backend library within the engine.
 */
namespace dmRender
{
    struct FontRenderBackend;
    typedef FontRenderBackend* HFontRenderBackend;

    /**
     * Called once during the application lifecycle
     * @name CreateFontRenderBackend
     * @return backend [type: HFontRenderBackend] the backend
     */
    HFontRenderBackend CreateFontRenderBackend();

    /**
     * Called when shutting down the engine
     * @name DestroyFontRenderBackend
     * @param backend [type: HFontRenderBackend] the backend
     */
    void DestroyFontRenderBackend(HFontRenderBackend backend);

    /**
     * Gets the vertex size
     * @param backend [type: HFontRenderBackend] the backend
     * @return size [type: uint32_t] the size of a vertex
     */
    uint32_t GetFontVertexSize(HFontRenderBackend backend);

    /**
     * Creates the vertex declaration
     * @param backend [type: HFontRenderBackend] the backend
     * @param context [type: dmGraphics::HContext] the graphics context
     * @return decl [type: dmGraphics::HVertexDeclaration] the vertex declaration, or 0 if it failed
     */
    dmGraphics::HVertexDeclaration CreateVertexDeclaration(HFontRenderBackend backend, dmGraphics::HContext context);

    /**
     * Calculates the metrics of the text
     * @param backend [type: HFontRenderBackend] the backend
     * @param font_map [type: HFontMap] the font
     * @param text [type: const char*] the text
     * @param settings [type: TextMetricsSettings*] settings like leading/tracking/linebreaks etc
     * @param metrics [out] [type: TextMetrics*] the resultinf metrics
     */
    void GetTextMetrics(HFontRenderBackend backend, HFontMap font_map, const char* text, TextLayoutSettings* settings, TextMetrics* metrics);

    /**
     * Outputs a triangle list.
     * It will check the font for glyphs, and if they're missing, request to add the glyph to the cache.
     *
     * @param backend [type: HFontRenderBackend] the backend
     * @param font_map [type: HFontMap] the fon
     * @param frame [type: uint32_t] the current frame. Used when caching new glyphs.t
     * @param text [type: const char*] the text
     * @param recip_w [type: float] 1/texture_width
     * @param recip_h [type: float] 1/texture_height
     * @param vertices [type: uint8_t*] the vertex buffer
     * @param num_vertices [type: uint32_t] the number of vertices that will fit into the vertex buffer
     * @return num_vertices [type: uint32_t] Returns number of vertices consumed from the vertex buffer
     */
    uint32_t CreateFontVertexData(HFontRenderBackend backend, HFontMap font_map, uint32_t frame, const char* text, const TextEntry& te, float recip_w, float recip_h, uint8_t* vertices, uint32_t num_vertices);
}

#endif // DM_FONT_RENDERER_API_H
