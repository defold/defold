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

#ifndef DM_FONT_COLR_H
#define DM_FONT_COLR_H

#include <stdint.h>
#include <dmsdk/font/font.h>

// Borrowed COLR/CPAL tables must outlive the view and paint traversal.
struct FontColr
{
    const uint8_t* m_Data;
    uint32_t       m_Size;
    const uint8_t* m_Colors; // Palette zero's BGRA records.
    uint32_t       m_ColorCount;
    uint32_t       m_BaseGlyphs;
    uint32_t       m_BaseGlyphCount;
    uint32_t       m_Layers;
    uint32_t       m_LayerCount;
    uint32_t       m_BaseGlyphList;
    uint32_t       m_LayerList;
    uint32_t       m_ClipList;
};

enum FontColrPaintType
{
    FONT_COLR_SOLID,
    FONT_COLR_LINEAR_GRADIENT,
    FONT_COLR_RADIAL_GRADIENT,
};

struct FontColrStop
{
    float m_Offset;
    float m_Color[4]; // Straight RGBA; alpha includes the COLR alpha multiplier.
    bool  m_Foreground;
};

struct FontColrPaint
{
    FontColrPaintType   m_Type;
    uint32_t            m_Extend; // 0 = pad, 1 = repeat, 2 = reflect.
    const FontColrStop* m_Stops;  // Borrowed for this callback only, stably sorted by offset.
    uint32_t            m_StopCount;
    float               m_Points[6]; // Linear: x0,y0,x1,y1,x2,y2. Radial: x0,y0,r0,x1,y1,r1.
};

// Push/pop callbacks are balanced even on failure. A callback returning false
// aborts traversal. Consumers must discard partial output on a non-OK result.
struct FontColrCallbacks
{
    bool (*m_PushTransform)(void* context, const float matrix[6]); // xx,yx,xy,yy,dx,dy
    void (*m_PopTransform)(void* context);
    bool (*m_PushClipGlyph)(void* context, uint32_t glyph);
    bool (*m_PushClipRectangle)(void* context, float x0, float y0, float x1, float y1);
    void (*m_PopClip)(void* context);
    // Isolate source-over operands from the current clip's edge coverage.
    bool (*m_PushComposite)(void* context);
    void (*m_PopComposite)(void* context);
    bool (*m_Paint)(void* context, const FontColrPaint* paint);
};

// Initializes palette zero and validates the table directories. Failure clears
// the view. Paint records are checked during traversal, with bounded work/depth.
bool FontColrInitialize(FontColr* font, const void* colr, uint32_t colr_size, const void* cpal, uint32_t cpal_size);
bool FontColrHasGlyph(const FontColr* font, uint32_t glyph);
// Returns raw outline bounds in font units, excluding the glyph's COLR graph.
typedef bool (*FontColrGlyphBoxCallback)(void* context, uint32_t glyph, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1);
// Reads a static clip box or bounds the transformed/clipped paint outlines.
// False means no finite bounds or an unsupported/malformed graph.
bool FontColrGetGlyphBox(const FontColr* font, uint32_t glyph, FontColrGlyphBoxCallback get_box, void* context, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1);
// Walks COLRv0 layers or non-variable COLRv1 solids, linear/radial gradients,
// transforms, clips and source-over compositions. Other paints return
// FONT_RESULT_NOT_SUPPORTED; malformed offsets/cycles return FONT_RESULT_ERROR.
FontResult FontColrPaintGlyph(const FontColr* font, uint32_t glyph, const FontColrCallbacks* callbacks, void* context);

#endif // DM_FONT_COLR_H
