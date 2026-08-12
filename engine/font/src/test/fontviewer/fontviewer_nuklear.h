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

#ifndef DM_FONT_VIEWER_NUKLEAR_H
#define DM_FONT_VIEWER_NUKLEAR_H

#include <stdbool.h>
#include <stdint.h>

#include <platform/window.hpp>

#define FONT_VIEWER_ZOOM_MIN 0.01f
#define FONT_VIEWER_ZOOM_MAX 20.0f
#define FONT_VIEWER_NUKLEAR_MAX_DRAW_COMMANDS 256

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct FontViewerNuklearBox
    {
        float m_X;
        float m_Y;
        float m_Width;
        float m_Height;
    } FontViewerNuklearBox;

    typedef struct FontViewerNuklearVertex
    {
        float   m_Position[2];
        float   m_TexCoord[2];
        uint8_t m_Color[4];
    } FontViewerNuklearVertex;

    typedef struct FontViewerNuklearDrawCommand
    {
        FontViewerNuklearBox m_Clip;
        uint32_t             m_ElementCount;
    } FontViewerNuklearDrawCommand;

    typedef struct FontViewerNuklearInput
    {
        int32_t m_MouseX;
        int32_t m_MouseY;
        float   m_ScrollY;
        bool    m_LeftMouseDown;
        bool    m_CopyDown;
        bool    m_SelectAllDown;
    } FontViewerNuklearInput;

    typedef struct FontViewerProperties
    {
        float m_Alpha;
        float m_OutlineAlpha;
        float m_OutlineWidth;
        float m_ShadowAlpha;
        float m_ShadowBlur;
        float m_ShadowX;
        float m_ShadowY;
        float m_FaceColor[3];
        float m_OutlineColor[3];
        float m_ShadowColor[3];
        float m_BackgroundColor[3];
    } FontViewerProperties;

    typedef struct FontViewerNuklearFonts
    {
        char*    m_LoadedFontText;
        uint32_t m_LoadedFontCount;
        uint32_t m_LoadedFontTextSize;
        uint64_t m_LoadedFontDataSize;
    } FontViewerNuklearFonts;

    typedef struct FontViewerNuklearLayout
    {
        const FontViewerNuklearVertex*      m_Vertices;
        const uint16_t*                     m_Indices;
        const FontViewerNuklearDrawCommand* m_DrawCommands;
        uint32_t                            m_VertexDataSize;
        uint32_t                            m_IndexDataSize;
        uint32_t                            m_DrawCommandCount;
        FontViewerNuklearBox                m_TextField;
        float                               m_ContentWidth;
        float                               m_TextContentHeight;
        float                               m_TextViewportHeight;
    } FontViewerNuklearLayout;

    bool FontViewerNuklearInitialize(HWindow window, uint32_t width, uint32_t height);
    void FontViewerNuklearFinalize(void);
    bool FontViewerNuklearGetAtlas(const void** pixels, uint32_t* width, uint32_t* height);
    void FontViewerNuklearBuild(uint32_t width, uint32_t height, const char* text, float text_content_height, const FontViewerNuklearInput* input, float* text_scroll_y, float* font_size, float* zoom, FontViewerProperties* properties, FontViewerNuklearFonts* fonts, bool* shape_text, bool* show_baselines, bool* show_quads, FontViewerNuklearLayout* layout);

#ifdef __cplusplus
}
#endif

#endif // DM_FONT_VIEWER_NUKLEAR_H
