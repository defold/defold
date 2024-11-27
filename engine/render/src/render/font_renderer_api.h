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

#ifndef DM_FONT_RENDERER_API_H
#define DM_FONT_RENDERER_API_H

#include <stdint.h>

#include <graphics/graphics.h>

#include "font_renderer_private.h"

/**
 * Internal font rendering api
 * The design is to use a single backend library within the engine.
 */
namespace dmRender
{
    /**
     * Gets the vertex size
     */
    uint32_t GetFontVertexSize();

    dmGraphics::HVertexDeclaration CreateVertexDeclaration(dmGraphics::HContext context);

    void GetTextMetrics(HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics);

    // Outputs a triangle list
    // Returns number of vertices consumed from the vertex buffer
    uint32_t CreateFontVertexData(TextContext& text_context, HFontMap font_map, const char* text, const TextEntry& te, float recip_w, float recip_h, uint8_t* vertices, uint32_t num_vertices);
}

#endif // DM_FONT_RENDERER_API_H
