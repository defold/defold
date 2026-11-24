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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/memory.h>
#include <dlib/testutil.h>
#include <string.h>
#include "truetype/font_colr.h"
#include "truetype/font_truetype.h"

struct Capture
{
    dmArray<uint32_t>     m_Glyphs;
    dmArray<FontColrStop> m_Stops;
    FontColrPaint         m_LastPaint;
    uint32_t              m_PaintCount;
    uint32_t              m_ClipDepth;
    uint32_t              m_TransformDepth;
    uint32_t              m_TransformCount;
    uint32_t              m_CompositeDepth;
    uint32_t              m_CompositeCount;
    float                 m_Matrix[6];
    float                 m_Box[4];
    bool                  m_Reject;
    Capture()
        : m_PaintCount(0)
        , m_ClipDepth(0)
        , m_TransformDepth(0)
        , m_TransformCount(0)
        , m_CompositeDepth(0)
        , m_CompositeCount(0)
        , m_Reject(false)
    {
    }
};

static bool PushTransform(void* data, const float matrix[6])
{
    Capture* c = (Capture*)data;
    ++c->m_TransformDepth;
    ++c->m_TransformCount;
    memcpy(c->m_Matrix, matrix, sizeof(c->m_Matrix));
    return !c->m_Reject;
}
static void PopTransform(void* data)
{
    --((Capture*)data)->m_TransformDepth;
}
static bool PushGlyph(void* data, uint32_t glyph)
{
    Capture* c = (Capture*)data;
    ++c->m_ClipDepth;
    if (c->m_Glyphs.Full())
        c->m_Glyphs.OffsetCapacity(32);
    c->m_Glyphs.Push(glyph);
    return !c->m_Reject;
}
static bool PushRectangle(void* data, float x0, float y0, float x1, float y1)
{
    Capture* c = (Capture*)data;
    ++c->m_ClipDepth;
    c->m_Box[0] = x0;
    c->m_Box[1] = y0;
    c->m_Box[2] = x1;
    c->m_Box[3] = y1;
    return !c->m_Reject;
}
static void PopClip(void* data)
{
    --((Capture*)data)->m_ClipDepth;
}
static bool PushComposite(void* data)
{
    Capture* c = (Capture*)data;
    ++c->m_CompositeDepth;
    ++c->m_CompositeCount;
    return !c->m_Reject;
}
static void PopComposite(void* data)
{
    --((Capture*)data)->m_CompositeDepth;
}
static bool Paint(void* data, const FontColrPaint* paint)
{
    Capture* c = (Capture*)data;
    ++c->m_PaintCount;
    c->m_LastPaint = *paint;
    c->m_LastPaint.m_Stops = 0; // The callback's array does not outlive this call.
    for (uint32_t i = 0; i < paint->m_StopCount; ++i)
    {
        if (c->m_Stops.Full())
            c->m_Stops.OffsetCapacity(32);
        c->m_Stops.Push(paint->m_Stops[i]);
    }
    return !c->m_Reject;
}
static const FontColrCallbacks CALLBACKS = { PushTransform, PopTransform, PushGlyph, PushRectangle, PopClip, PushComposite, PopComposite, Paint };
static const uint8_t           CPAL[] = {
    0,
    0,
    0,
    2,
    0,
    1,
    0,
    2,
    0,
    0,
    0,
    14,
    0,
    0,
    10,
    30,
    255,
    128,
    50,
    200,
    20,
    255 // BGRA: translucent red, green.
};
static const uint8_t V0[] = {
    0,
    0,
    0,
    1,
    0,
    0,
    0,
    14,
    0,
    0,
    0,
    20,
    0,
    2,
    0,
    42,
    0,
    0,
    0,
    2, // Base glyph 42: first layer 0, count 2.
    0,
    3,
    0,
    0,
    0,
    4,
    255,
    255
};

// One COLRv1 base glyph. Append a paint graph rooted immediately after the list.
static void V1(dmArray<uint8_t>& bytes, const uint8_t* paint, uint32_t size)
{
    const uint8_t header[] = {
        0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 42, 0, 0, 0, 10
    };
    bytes.SetCapacity(sizeof(header) + size);
    bytes.SetSize(bytes.Capacity());
    memcpy(bytes.Begin(), header, sizeof(header));
    memcpy(bytes.Begin() + sizeof(header), paint, size);
}

// COLRv0 layers preserve palette alpha, foreground selection and glyph order.
TEST(FontColr, PaletteAndForegroundLayers)
{
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, V0, sizeof(V0), CPAL, sizeof(CPAL)));
    ASSERT_TRUE(FontColrHasGlyph(&font, 42));
    ASSERT_FALSE(FontColrHasGlyph(&font, 41));
    ASSERT_FALSE(FontColrHasGlyph(&font, 0x1002a));
    Capture c;
    ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(2u, c.m_PaintCount);
    ASSERT_EQ(3u, c.m_Glyphs[0]);
    ASSERT_EQ(4u, c.m_Glyphs[1]);
    ASSERT_FALSE(c.m_Stops[0].m_Foreground);
    ASSERT_EQ(1.0f, c.m_Stops[0].m_Color[0]);
    ASSERT_EQ(30.0f / 255.0f, c.m_Stops[0].m_Color[1]);
    ASSERT_EQ(128.0f / 255.0f, c.m_Stops[0].m_Color[3]);
    ASSERT_TRUE(c.m_Stops[1].m_Foreground);
    ASSERT_EQ(0u, c.m_ClipDepth);
}

// Gradient stops are ordered stably and truncated color lines are rejected.
TEST(FontColr, GradientStopOrderAndAlpha)
{
    const uint8_t graph[] = {
        10, 0, 0, 6, 0, 3, // PaintGlyph 3
        4,
        0,
        0,
        16,
        0,
        10,
        0,
        20,
        0,
        110,
        0,
        20,
        0,
        10,
        0,
        120,
        2,
        0,
        3, // Reflect, three stops, deliberately unsorted and coincident.
        64,
        0,
        0,
        1,
        32,
        0, // 1.0, palette 1, half alpha
        0,
        0,
        0,
        0,
        64,
        0, // 0.0, palette 0, full alpha
        64,
        0,
        255,
        255,
        48,
        0 // 1.0, foreground, 0.75 alpha
    };
    dmArray<uint8_t> bytes;
    V1(bytes, graph, sizeof(graph));
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    Capture c;
    ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(FONT_COLR_LINEAR_GRADIENT, c.m_LastPaint.m_Type);
    ASSERT_EQ(2u, c.m_LastPaint.m_Extend);
    ASSERT_EQ(110.0f, c.m_LastPaint.m_Points[2]);
    ASSERT_EQ(120.0f, c.m_LastPaint.m_Points[5]);
    ASSERT_EQ(3u, c.m_Stops.Size());
    ASSERT_EQ(0.0f, c.m_Stops[0].m_Offset);
    ASSERT_EQ(1.0f, c.m_Stops[1].m_Offset);
    ASSERT_EQ(0.5f, c.m_Stops[1].m_Color[3]);
    ASSERT_FALSE(c.m_Stops[1].m_Foreground);
    ASSERT_TRUE(c.m_Stops[2].m_Foreground);
    ASSERT_EQ(0.75f, c.m_Stops[2].m_Color[3]);
    ASSERT_EQ(0u, c.m_ClipDepth);
    // Every truncated color-line suffix must be rejected during traversal.
    for (uint32_t size = bytes.Size() - 17; size < bytes.Size(); ++size)
    {
        ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), size, CPAL, sizeof(CPAL)));
        Capture truncated;
        ASSERT_EQ(FONT_RESULT_ERROR, FontColrPaintGlyph(&font, 42, &CALLBACKS, &truncated));
        ASSERT_EQ(0u, truncated.m_ClipDepth);
    }
}

// Transforms are decoded correctly and callback failure unwinds both stacks.
TEST(FontColr, TransformAndCallbackFailure)
{
    const uint8_t graph[] = {
        18, 0, 0, 12, 32, 0, 96, 0, 0, 100, 0, 200, // Scale (.5, 1.5) around (100,200).
        10,
        0,
        0,
        6,
        0,
        3,
        2,
        0,
        0,
        64,
        0
    };
    dmArray<uint8_t> bytes;
    V1(bytes, graph, sizeof(graph));
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    Capture c;
    ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(0.5f, c.m_Matrix[0]);
    ASSERT_EQ(1.5f, c.m_Matrix[3]);
    ASSERT_EQ(50.0f, c.m_Matrix[4]);
    ASSERT_EQ(-100.0f, c.m_Matrix[5]);
    ASSERT_EQ(0u, c.m_TransformDepth);
    c.m_Reject = true;
    ASSERT_EQ(FONT_RESULT_NOT_SUPPORTED, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(0u, c.m_TransformDepth);
    ASSERT_EQ(0u, c.m_ClipDepth);
}

// Source-over visits the backdrop first, preserves composite boundaries, and
// balances callbacks even when the consumer rejects a group or its source fails.
TEST(FontColr, SourceOverPaintOrder)
{
    const uint8_t graph[] = {
        32, 0, 0, 19, 3, 0, 0, 8, // Source-over: backdrop at 8, source at 19.
        10,
        0,
        0,
        6,
        0,
        3,
        2,
        0,
        0,
        64,
        0,
        10,
        0,
        0,
        6,
        0,
        4,
        2,
        0,
        1,
        64,
        0
    };
    dmArray<uint8_t> bytes;
    V1(bytes, graph, sizeof(graph));
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    Capture c;
    ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(2u, c.m_Glyphs.Size());
    ASSERT_EQ(3u, c.m_Glyphs[0]);
    ASSERT_EQ(4u, c.m_Glyphs[1]);
    ASSERT_EQ(0u, c.m_ClipDepth);
    ASSERT_EQ(1u, c.m_CompositeCount);
    ASSERT_EQ(0u, c.m_CompositeDepth);
    c.m_Reject = true;
    ASSERT_EQ(FONT_RESULT_NOT_SUPPORTED, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(2u, c.m_CompositeCount);
    ASSERT_EQ(0u, c.m_CompositeDepth);
    c.m_Reject = false;
    bytes[44 + 19] = 255;
    ASSERT_EQ(FONT_RESULT_NOT_SUPPORTED, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(3u, c.m_CompositeCount);
    ASSERT_EQ(0u, c.m_CompositeDepth);
    bytes[44 + 19] = 10;
    bytes[44 + 4] = 5; // SRC_IN requires a compositor; do not approximate it.
    ASSERT_EQ(FONT_RESULT_NOT_SUPPORTED, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
}

// Malformed offsets, cycles and unsupported paints fail without reading past tables.
TEST(FontColr, RejectsCyclesOffsetsAndUnsupportedPaints)
{
    const uint8_t    cycle[] = { 11, 0, 42 }; // PaintColrGlyph refers to itself.
    dmArray<uint8_t> bytes;
    V1(bytes, cycle, sizeof(cycle));
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    Capture c;
    ASSERT_EQ(FONT_RESULT_ERROR, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    bytes[44] = 8; // Sweep gradient is explicitly unsupported.
    ASSERT_EQ(FONT_RESULT_NOT_SUPPORTED, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    bytes[40] = 255; // Base paint offset runs beyond the table.
    ASSERT_EQ(FONT_RESULT_ERROR, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    bytes[14] = 255; // Base list offset runs beyond the table.
    ASSERT_FALSE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    ASSERT_FALSE(FontColrHasGlyph(&font, 42));
    ASSERT_FALSE(FontColrInitialize(&font, V0, sizeof(V0), CPAL, sizeof(CPAL) - 1));
    for (uint32_t size = 0; size < sizeof(V0); ++size)
        ASSERT_FALSE(FontColrInitialize(&font, V0, size, CPAL, sizeof(CPAL)));
}

// Radial gradients retain unsigned radii and glyph clip boxes stay balanced.
TEST(FontColr, RadialGradientAndClipBox)
{
    const uint8_t graph[] = {
        10, 0, 0, 6, 0, 3, 6, 0, 0, 16, 255, 246, 0, 20, 128, 0, 0, 110, 255, 226, 255, 255, 1, 0, 2, 0, 0, 0, 0, 64, 0, 64, 0, 0, 1, 64, 0
    }; // Repeat radial gradient, unsigned radii 32768 and 65535.
    dmArray<uint8_t> bytes;
    V1(bytes, graph, sizeof(graph));
    uint32_t offset = bytes.Size();
    bytes[25] = (uint8_t)offset; // ClipList offset in the COLRv1 header.
    const uint8_t clips[] = { 1, 0, 0, 0, 1, 0, 42, 0, 42, 0, 0, 12, 1, 255, 236, 255, 226, 0, 200, 1, 44 };
    bytes.OffsetCapacity(sizeof(clips));
    bytes.SetSize(offset + sizeof(clips));
    memcpy(bytes.Begin() + offset, clips, sizeof(clips));
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    Capture c;
    ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(FONT_COLR_RADIAL_GRADIENT, c.m_LastPaint.m_Type);
    ASSERT_EQ(1u, c.m_LastPaint.m_Extend);
    const float points[] = { -10, 20, 32768, 110, -30, 65535 };
    for (uint32_t i = 0; i < 6; ++i)
        ASSERT_EQ(points[i], c.m_LastPaint.m_Points[i]);
    ASSERT_EQ(-20.0f, c.m_Box[0]);
    ASSERT_EQ(-30.0f, c.m_Box[1]);
    ASSERT_EQ(200.0f, c.m_Box[2]);
    ASSERT_EQ(300.0f, c.m_Box[3]);
    ASSERT_EQ(0u, c.m_ClipDepth);
    c.m_Reject = true;
    ASSERT_EQ(FONT_RESULT_NOT_SUPPORTED, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
    ASSERT_EQ(0u, c.m_ClipDepth);
}

// All supported static transform formats match independently derived matrices.
TEST(FontColr, StaticTransformFormats)
{
    // OpenType COLR formats 12,14,16,18,20,22,24,26,28,30.
    // Matrices are hand-derived from the specified 16.16/F2DOT14 inputs.
    const uint8_t transforms[][31] = {
        { 12, 0, 0, 31, 0, 0, 7, 0, 2, 0, 0, 0, 0, 128, 0, 255, 255, 0, 0, 0, 1, 128, 0, 255, 246, 0, 0, 0, 20, 0, 0 },
        { 14, 0, 0, 8, 255, 246, 0, 20 },
        { 16, 0, 0, 8, 32, 0, 96, 0 },
        { 18, 0, 0, 12, 32, 0, 96, 0, 0, 100, 0, 200 },
        { 20, 0, 0, 6, 32, 0 },
        { 22, 0, 0, 10, 32, 0, 0, 100, 0, 200 },
        { 24, 0, 0, 6, 32, 0 },
        { 26, 0, 0, 10, 32, 0, 0, 100, 0, 200 },
        { 28, 0, 0, 8, 16, 0, 240, 0 },
        { 30, 0, 0, 12, 16, 0, 240, 0, 0, 100, 0, 200 }
    };
    const float expected[][6] = {
        { 2, .5f, -1, 1.5f, -10, 20 }, { 1, 0, 0, 1, -10, 20 }, { .5f, 0, 0, 1.5f, 0, 0 }, { .5f, 0, 0, 1.5f, 50, -100 }, { .5f, 0, 0, .5f, 0, 0 }, { .5f, 0, 0, .5f, 50, 100 }, { 0, 1, -1, 0, 0, 0 }, { 0, 1, -1, 0, 300, 100 }, { 1, -1, -1, 1, 0, 0 }, { 1, -1, -1, 1, 200, 100 }
    };
    const uint8_t child[] = { 10, 0, 0, 6, 0, 3, 2, 0, 0, 64, 0 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(transforms); ++i)
    {
        uint8_t  graph[42];
        uint32_t size = transforms[i][3];
        memcpy(graph, transforms[i], size);
        memcpy(graph + size, child, sizeof(child));
        dmArray<uint8_t> bytes;
        V1(bytes, graph, size + sizeof(child));
        FontColr font;
        ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
        Capture c;
        ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(&font, 42, &CALLBACKS, &c));
        ASSERT_EQ(1u, c.m_TransformCount);
        for (uint32_t j = 0; j < 6; ++j)
            ASSERT_NEAR(expected[i][j], c.m_Matrix[j], .0001f);
        ASSERT_EQ(0u, c.m_TransformDepth);
        ASSERT_EQ(0u, c.m_ClipDepth);
    }
}

static bool GetOutlineBox(void* data, uint32_t glyph, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    return FontTrueTypeGetGlyphBox((FontTrueType*)data, glyph, x0, y0, x1, y1);
}

static bool FixtureBox(void*, uint32_t glyph, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1)
{
    if (glyph != 3 && glyph != 4)
        return false;
    *x0 = glyph == 3 ? -10 : 200;
    *y0 = glyph == 3 ? -20 : 10;
    *x1 = glyph == 3 ? 100 : 300;
    *y1 = glyph == 3 ? 200 : 60;
    return true;
}

// Bounds without a ClipList must union v0 layers and transform v1 outline
// clips, while transforms inside PaintGlyph affect only its paint.
TEST(FontColr, BoundsWithoutClipList)
{
    FontColr font;
    ASSERT_TRUE(FontColrInitialize(&font, V0, sizeof(V0), CPAL, sizeof(CPAL)));
    int32_t x0, y0, x1, y1;
    ASSERT_TRUE(FontColrGetGlyphBox(&font, 42, FixtureBox, 0, &x0, &y0, &x1, &y1));
    ASSERT_EQ(-10, x0);
    ASSERT_EQ(-20, y0);
    ASSERT_EQ(300, x1);
    ASSERT_EQ(200, y1);
    const uint8_t graph[] = {
        18, 0, 0, 12, 32, 0, 96, 0, 0, 100, 0, 200, // Scale around (100,200).
        10, 0, 0, 6, 0, 3,                         // Outline 3.
        14, 0, 0, 8, 0, 100, 0, 100,              // Paint-only translation.
        2, 0, 0, 64, 0
    };
    dmArray<uint8_t> bytes;
    V1(bytes, graph, sizeof(graph));
    ASSERT_TRUE(FontColrInitialize(&font, bytes.Begin(), bytes.Size(), CPAL, sizeof(CPAL)));
    ASSERT_TRUE(FontColrGetGlyphBox(&font, 42, FixtureBox, 0, &x0, &y0, &x1, &y1));
    ASSERT_EQ(45, x0);
    ASSERT_EQ(-130, y0);
    ASSERT_EQ(100, x1);
    ASSERT_EQ(200, y1);
    bytes[bytes.Size() - 2] = 0; // Transparent paint has no ink bounds.
    ASSERT_FALSE(FontColrGetGlyphBox(&font, 42, FixtureBox, 0, &x0, &y0, &x1, &y1));
    bytes[44] = 11;
    bytes[45] = 0;
    bytes[46] = 42; // Cyclic PaintColrGlyph.
    ASSERT_FALSE(FontColrGetGlyphBox(&font, 42, FixtureBox, 0, &x0, &y0, &x1, &y1));
}

// The real emoji fixture retains its supported glyph set, layers and bounds.
TEST(FontColr, NotoEmojiLayersAndBounds)
{
    char path[512];
    dmTestUtil::MakeHostPath(path, sizeof(path), "src/test/data/NotoColorEmoji-Regular.ttf");
    uint32_t size;
    uint8_t* data = dmTestUtil::ReadFile(path, &size);
    ASSERT_NE((uint8_t*)0, data);
    FontTrueType* font = FontTrueTypeCreate(data, size, 0);
    ASSERT_NE((FontTrueType*)0, font);
    uint32_t        glyph = FontTrueTypeGetGlyphIndex(font, 0x1f600);
    const FontColr* colr = FontTrueTypeGetColr(font);
    ASSERT_TRUE(FontColrHasGlyph(colr, glyph));
    Capture c;
    ASSERT_EQ(FONT_RESULT_OK, FontColrPaintGlyph(colr, glyph, &CALLBACKS, &c));
    ASSERT_GT(c.m_PaintCount, 1u);
    ASSERT_GT(c.m_TransformCount, 0u);
    ASSERT_EQ(0u, c.m_ClipDepth);
    ASSERT_EQ(0u, c.m_TransformDepth);
    for (uint32_t i = 0; i < c.m_Glyphs.Size(); ++i)
    {
        FontOutline outline;
        ASSERT_EQ(FONT_RESULT_OK, FontTrueTypeGetGlyphOutline(font, c.m_Glyphs[i], &outline));
        ASSERT_GT(outline.m_CommandCount, 0u);
        FontFreeGlyphOutline(&outline);
    }
    int32_t x0, y0, x1, y1;
    ASSERT_TRUE(FontColrGetGlyphBox(colr, glyph, GetOutlineBox, font, &x0, &y0, &x1, &y1));
    ASSERT_EQ((float)x0, c.m_Box[0]);
    ASSERT_EQ((float)y0, c.m_Box[1]);
    ASSERT_EQ((float)x1, c.m_Box[2]);
    ASSERT_EQ((float)y1, c.m_Box[3]);
    ASSERT_GT(x1, x0);
    ASSERT_GT(y1, y0);
    // Exercise every paint graph in the fixture, including fonts' shared
    // layer lists. Unsupported compositing is expected; malformed data is not.
    uint32_t color_glyphs = 0, supported = 0;
    uint64_t support_hash = 14695981039346656037ULL;
    for (uint32_t id = 0; id <= 0xffff; ++id)
    {
        if (!FontColrHasGlyph(colr, id))
            continue;
        ++color_glyphs;
        Capture    graph;
        FontResult result = FontColrPaintGlyph(colr, id, &CALLBACKS, &graph);
        ASSERT_NE(FONT_RESULT_ERROR, result);
        if (result == FONT_RESULT_OK)
        {
            ++supported;
            support_hash = (support_hash ^ id) * 1099511628211ULL;
        }
        ASSERT_EQ(0u, graph.m_ClipDepth);
        ASSERT_EQ(0u, graph.m_TransformDepth);
    }
    ASSERT_EQ(3845u, color_glyphs);
    ASSERT_EQ(3549u, supported);
    // Fingerprint the supported glyph set, not just the number of successes.
    // Any previously working graph becoming unsupported fails this check.
    ASSERT_EQ(9974027943221373762ULL, support_hash);
    FontTrueTypeDestroy(font);
    dmMemory::AlignedFree(data);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
