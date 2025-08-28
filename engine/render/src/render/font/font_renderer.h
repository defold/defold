// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_FONT_RENDERER_H
#define DM_FONT_RENDERER_H

#include <stdint.h>

#include <dmsdk/dlib/vmath.h>

#include <graphics/graphics.h>

#include "../render.h"
#include "fontmap.h"

namespace dmRender
{
    // The first byte of the texture data, is the compression
    enum FontGlyphCompression
    {
        FONT_GLYPH_COMPRESSION_NONE = 0,
        FONT_GLYPH_COMPRESSION_DEFLATE = 1,
    };

    void InitializeTextContext(HRenderContext render_context, uint32_t max_characters, uint32_t max_batches);
    void FinalizeTextContext(HRenderContext render_context);

    const int MAX_FONT_RENDER_CONSTANTS = 16;
    /**
     * Draw text params.
     */
    struct DrawTextParams
    {
        DrawTextParams();

        /// Transform from font space to world (origo in font space is the base line of the first glyph)
        dmVMath::Matrix4 m_WorldTransform;
        /// Color of the font face
        dmVMath::Vector4 m_FaceColor;
        /// Color of the outline
        dmVMath::Vector4 m_OutlineColor;
        /// Color of the shadow
        dmVMath::Vector4 m_ShadowColor;
        /// Text to draw in utf8-format
        const char* m_Text;
        /// Render constants
        dmRender::HConstant m_RenderConstants[MAX_FONT_RENDER_CONSTANTS];
        /// The source blend factor
        dmGraphics::BlendFactor m_SourceBlendFactor;
        /// The destination blend factor
        dmGraphics::BlendFactor m_DestinationBlendFactor;
        /// Render order value. Passed to the render-key
        uint16_t    m_RenderOrder;
        /// Number render constants
        uint8_t     m_NumRenderConstants;
        /// Text render box width. Used for alignment and when m_LineBreak is true
        float       m_Width;
        /// Text render box height. Used for vertical alignment
        float       m_Height;
        /// Text line spacing
        float       m_Leading;
        /// Text letter spacing
        float       m_Tracking;
        /// True for linebreak
        bool        m_LineBreak;
        /// Horizontal alignment
        TextAlign m_Align;
        /// Vertical alignment
        TextVAlign m_VAlign;
        /// Stencil parameters
        StencilTestParams m_StencilTestParams;
        /// Stencil parameters set or not
        uint8_t m_StencilTestParamsSet : 1;
    };

    /**
     * Draw text
     * @param render_context Context to use when rendering
     * @param font_map Font map handle
     * @param material Material handle (0 to use font_map internal material)
     * @param batch_key Rendering order batch key
     * @param params Parameters to use when rendering
     */
    void DrawText(HRenderContext render_context, HFontMap font_map, HMaterial material, uint64_t batch_key, const DrawTextParams& params);

    /**
     * Produces render list entries for all the previously DrawText:ed texts.
     * Multiple calls can be made with final=false, but one (last) call
     * with final=true must be made, so that the vertex buffers will be
     * written.
     *
     * @param final If this is the last call.
     * @param render_order Render order to write for the rendering
     * @param render_context Context to use when rendering
     */
    void FlushTexts(HRenderContext render_context, uint32_t major_order, uint32_t render_order, bool final);
}

#endif // DM_FONT_RENDERER_H
