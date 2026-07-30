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

#ifndef DM_FONT_RENDERER_H
#define DM_FONT_RENDERER_H

#include <stdint.h>

#if defined(_MSC_VER)
#define FONT_RENDERER_API __declspec(dllexport)
#else
#define FONT_RENDERER_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct FontRendererSession* HFontRenderer;

    typedef enum FontRendererResult
    {
        FONT_RENDERER_RESULT_OK = 0,
        FONT_RENDERER_RESULT_INVALID_ARGUMENT = -1,
        FONT_RENDERER_RESULT_FONT_ERROR = -2,
        FONT_RENDERER_RESULT_TEXT_ERROR = -3,
        FONT_RENDERER_RESULT_GLYPH_ERROR = -4,
        FONT_RENDERER_RESULT_OUT_OF_MEMORY = -5,
    } FontRendererResult;

    typedef enum FontRendererLayer
    {
        FONT_RENDERER_LAYER_FACE = 1,
        FONT_RENDERER_LAYER_OUTLINE = 2,
        FONT_RENDERER_LAYER_SHADOW = 4,
    } FontRendererLayer;

    typedef struct FontRendererParams
    {
        float    m_Size;
        uint32_t m_AtlasWidth;
        uint32_t m_AtlasHeight;
        uint32_t m_CellPadding;
        float    m_SdfBasePadding;
        uint32_t m_SdfEdgeValue;
        float    m_SdfSpread;
        float    m_SdfOutline;
        float    m_SdfShadow;
        float    m_OutlineWidth;
        float    m_ShadowBlur;
        float    m_ShadowX;
        float    m_ShadowY;
        uint32_t m_LayerMask;
    } FontRendererParams;

    typedef struct FontRendererLayout
    {
        float    m_Width;
        float    m_Height;
        uint32_t m_LineCount;
        float    m_MaxAscent;
        float    m_MaxDescent;
    } FontRendererLayout;

    typedef struct FontRendererGlyph
    {
        uint32_t m_GlyphIndex;
        uint32_t m_Width;
        uint32_t m_Height;
        uint32_t m_Channels;
        float    m_Advance;
        float    m_LeftBearing;
        float    m_Ascent;
        float    m_Descent;
        uint8_t* m_Pixels;
        uint32_t m_PixelCount;
    } FontRendererGlyph;

    typedef struct FontRendererRenderResult
    {
        uint8_t* m_Vertices;
        uint32_t m_VertexByteCount;
        uint32_t m_VertexCount;
        uint64_t m_AtlasVersion;
        uint32_t m_HasTextureUpdate;
        uint32_t m_TextureX;
        uint32_t m_TextureY;
        uint32_t m_TextureWidth;
        uint32_t m_TextureHeight;
        uint32_t m_TextureChannels;
        uint8_t* m_TexturePixels;
        uint32_t m_TexturePixelCount;
    } FontRendererRenderResult;

    FONT_RENDERER_API FontRendererResult FontRendererCreate(const char*               name,
                                                            const uint8_t*            font_bytes,
                                                            uint32_t                  font_byte_count,
                                                            const FontRendererParams* params,
                                                            HFontRenderer*            renderer);
    FONT_RENDERER_API void               FontRendererDestroy(HFontRenderer renderer);

    FONT_RENDERER_API FontRendererResult FontRendererMeasure(HFontRenderer       renderer,
                                                             const uint32_t*     codepoints,
                                                             uint32_t            codepoint_count,
                                                             uint32_t            line_break,
                                                             float               width,
                                                             float               leading,
                                                             float               tracking,
                                                             FontRendererLayout* layout);

    FONT_RENDERER_API FontRendererResult FontRendererGenerateGlyph(HFontRenderer      renderer,
                                                                   uint32_t           codepoint,
                                                                   FontRendererGlyph* glyph);
    FONT_RENDERER_API void               FontRendererFreeGlyph(FontRendererGlyph* glyph);

    FONT_RENDERER_API FontRendererResult FontRendererRender(HFontRenderer             renderer,
                                                            const uint32_t*           codepoints,
                                                            uint32_t                  codepoint_count,
                                                            uint32_t                  line_break,
                                                            float                     width,
                                                            float                     height,
                                                            float                     leading,
                                                            float                     tracking,
                                                            uint32_t                  align,
                                                            uint32_t                  vertical_align,
                                                            const float*              transform,
                                                            const float*              face_color,
                                                            const float*              outline_color,
                                                            const float*              shadow_color,
                                                            float                     sdf_scale,
                                                            uint64_t                  known_atlas_version,
                                                            FontRendererRenderResult* result);
    FONT_RENDERER_API void               FontRendererFreeRenderResult(FontRendererRenderResult* result);

#ifdef __cplusplus
}
#endif

#endif // DM_FONT_RENDERER_H
