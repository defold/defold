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

#ifndef TEST_FONT_BITMAP_GEN_H
#define TEST_FONT_BITMAP_GEN_H

#include <stdint.h>

// Fixed capture coordinates shared with the reference fixtures.
// Origins may lie outside the target when capturing a tight glyph rectangle.
struct FontImageCaptureGeometry
{
    uint32_t m_Width;
    uint32_t m_Height;
    int32_t  m_OriginX;
    int32_t  m_OriginTop;
    uint32_t m_LayoutWidth;
};

extern const FontImageCaptureGeometry g_Capture_single_line;
extern const FontImageCaptureGeometry g_Capture_english;
extern const FontImageCaptureGeometry g_Capture_arabic;
extern const FontImageCaptureGeometry g_Capture_ttf_edge_half;
extern const FontImageCaptureGeometry g_Capture_ttf_edge_one;
extern const FontImageCaptureGeometry g_Capture_ttf_edge_two;
extern const FontImageCaptureGeometry g_Capture_otf_edge_half;
extern const FontImageCaptureGeometry g_Capture_otf_edge_one;
extern const FontImageCaptureGeometry g_Capture_otf_edge_two;

struct FontImageCase
{
    const char* m_Name;
    const char* m_Source;
    const char* m_Text;
    float m_Size;
    float m_Outline;
    float m_OutlineAlpha;
    float m_FaceAlpha;
    float m_ShadowAlpha;
    float m_ShadowBlur;
    float m_ShadowX;
    float m_ShadowY;
    bool  m_Multi;
    bool  m_Markup;
    bool  m_Change;
    float m_EdgeScale; // Nonzero selects an 8x edge capture at this screen scale.
};

#endif
