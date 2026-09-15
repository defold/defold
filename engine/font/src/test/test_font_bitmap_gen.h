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
#include <dlib/array.h>
#include <graphics/graphics.h>

// Fixed capture coordinates shared with the patched-stable fixture generator.
struct FontImageCaptureGeometry
{
    uint32_t m_Width;
    uint32_t m_Height;
    uint32_t m_OriginX;
    uint32_t m_OriginTop;
    uint32_t m_LayoutWidth;
};

extern const FontImageCaptureGeometry g_Capture_single_line;
extern const FontImageCaptureGeometry g_Capture_vector;
extern const FontImageCaptureGeometry g_Capture_english;
extern const FontImageCaptureGeometry g_Capture_arabic;

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
};

// Test-only bridge to the production Vector backend. The graphics context is borrowed.
void TestFontVectorImage(const FontImageCase& c, dmGraphics::HContext context);
void WriteFontTestImage(const FontImageCase& c, uint32_t width, uint32_t height, dmArray<uint8_t>& pixels, const char* metadata);

#endif
