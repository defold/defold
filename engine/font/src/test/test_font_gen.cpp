// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/jobsystem.h>
#include <dlib/time.h>

#include "../font.h"
#include "../fontgen.h"

struct TestFont
{
    Font        m_Base;
    uint32_t    m_GenerateCount;
    uint32_t    m_FreeCount;
};

struct TestContext
{
    uint32_t    m_CachedGlyphIndex;
    uint32_t    m_CacheLookupCount;
    uint32_t    m_AddCount;
    uint32_t    m_CompleteCount;
    int         m_CompleteResult;
    bool        m_AcceptGlyph;
    FontGlyph*  m_OwnedGlyph;
    FontGlyph*  m_OwnedGlyphs[4];
    HFont       m_OwningFonts[4];
    char        m_Error[128];
};

static float TestGetScaleFromSize(HFont, uint32_t size)
{
    return size / 10.0f;
}

static FontResult TestGetGlyph(HFont font, uint32_t glyph_index, const FontGlyphOptions*, FontGlyph* glyph)
{
    TestFont* test_font = (TestFont*)font;
    ++test_font->m_GenerateCount;

    glyph->m_GlyphIndex = glyph_index;
    glyph->m_Outline.m_Commands = new FontCurveCommand[1];
    glyph->m_Outline.m_CommandCount = 1;
    glyph->m_Outline.m_Commands[0].m_Type = FONT_CURVE_QUADRATIC_TO;
    return FONT_RESULT_OK;
}

static FontResult TestFreeGlyph(HFont font, FontGlyph* glyph)
{
    TestFont* test_font = (TestFont*)font;
    ++test_font->m_FreeCount;
    delete[] glyph->m_Outline.m_Commands;
    glyph->m_Outline.m_Commands = 0;
    glyph->m_Outline.m_CommandCount = 0;
    return FONT_RESULT_OK;
}

static bool TestIsGlyphCached(void* context, HFont, uint32_t glyph_index)
{
    TestContext* test = (TestContext*)context;
    ++test->m_CacheLookupCount;
    return glyph_index == test->m_CachedGlyphIndex;
}

static FontResult TestAddGlyph(void* context, HFont font, FontGlyph* glyph)
{
    TestContext* test = (TestContext*)context;
    uint32_t index = test->m_AddCount++;
    if (!test->m_AcceptGlyph || index >= DM_ARRAY_SIZE(test->m_OwnedGlyphs))
        return FONT_RESULT_ERROR;
    test->m_OwnedGlyph = glyph;
    test->m_OwnedGlyphs[index] = glyph;
    test->m_OwningFonts[index] = font;
    return FONT_RESULT_OK;
}

static void TestComplete(void* context, int result, const char* error_message)
{
    TestContext* test = (TestContext*)context;
    ++test->m_CompleteCount;
    test->m_CompleteResult = result;
    dmStrlCpy(test->m_Error, error_message ? error_message : "", sizeof(test->m_Error));
}

class FontGenTest : public jc_test_base_class
{
protected:
    HJobContext m_Jobs;
    TestFont    m_Font;

    virtual void SetUp() override
    {
        JobSystemCreateParams job_params = {};
        job_params.m_ThreadCount = 1;
        m_Jobs = JobSystemCreate(&job_params);

        memset(&m_Font, 0, sizeof(m_Font));
        m_Font.m_Base.m_Path = "test-font.ttf";
        m_Font.m_Base.m_PathHash = dmHashString32(m_Font.m_Base.m_Path);
        m_Font.m_Base.m_GetScaleFromSize = TestGetScaleFromSize;
        m_Font.m_Base.m_GetGlyph = TestGetGlyph;
        m_Font.m_Base.m_FreeGlyph = TestFreeGlyph;
    }

    virtual void TearDown() override
    {
        JobSystemDestroy(m_Jobs);
    }

    FontGenParams MakeParams(TestContext* context)
    {
        FontGenParams params = {};
        params.m_UserContext = context;
        params.m_IsGlyphCached = TestIsGlyphCached;
        params.m_AddGlyph = TestAddGlyph;
        params.m_Complete = TestComplete;
        params.m_Jobs = m_Jobs;
        params.m_Size = 20.0f;
        params.m_IsSdf = 1;
        params.m_IsVector = 1;
        return params;
    }

    void RunUntilComplete(TestContext* context)
    {
        uint64_t timeout = dmTime::GetMonotonicTime() + 2 * 1000000;
        while (context->m_CompleteCount == 0 && dmTime::GetMonotonicTime() < timeout)
        {
            JobSystemUpdate(m_Jobs, 1000);
            dmTime::Sleep(1000);
        }
        ASSERT_EQ(1u, context->m_CompleteCount);
    }
};

TEST_F(FontGenTest, GeneratesUniqueUncachedGlyphsAndTransfersOwnership)
{
    ASSERT_TRUE(FontGenIsSupported());

    TestContext context = {};
    context.m_CachedGlyphIndex = 8;
    context.m_AcceptGlyph = true;

    TextGlyph glyphs[4] = {};
    glyphs[0].m_Font = (HFont)&m_Font;
    glyphs[0].m_GlyphIndex = 7;
    glyphs[1] = glyphs[0];
    glyphs[2].m_Font = (HFont)&m_Font;
    glyphs[2].m_GlyphIndex = 8;
    glyphs[3].m_Font = (HFont)&m_Font;
    glyphs[3].m_GlyphIndex = 9;
    glyphs[3].m_Flags = TEXT_GLYPH_FLAG_OBJECT;

    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 4);
    ASSERT_NE((FontGenJobData*)0, job_data);

    HJob job = FontGenAddGlyphs(job_data, glyphs, 4);
    ASSERT_NE((HJob)0, job);
    JobSystemPushJob(m_Jobs, job);
    RunUntilComplete(&context);

    ASSERT_EQ(2u, context.m_CacheLookupCount);
    ASSERT_EQ(1u, m_Font.m_GenerateCount);
    ASSERT_EQ(1u, context.m_AddCount);
    ASSERT_EQ(1, context.m_CompleteResult);
    ASSERT_NE((FontGlyph*)0, context.m_OwnedGlyph);
    ASSERT_EQ(7u, context.m_OwnedGlyph->m_GlyphIndex);
    ASSERT_EQ(1u, context.m_OwnedGlyph->m_Outline.m_CommandCount);
    ASSERT_EQ((uint8_t)FONT_CURVE_QUADRATIC_TO, context.m_OwnedGlyph->m_Outline.m_Commands[0].m_Type);
    ASSERT_EQ(0u, m_Font.m_FreeCount);

    FontGenDestroyJobData(job_data);
    ASSERT_EQ(0u, m_Font.m_FreeCount);

    FontFreeGlyph((HFont)&m_Font, context.m_OwnedGlyph);
    delete context.m_OwnedGlyph;
    ASSERT_EQ(1u, m_Font.m_FreeCount);
}

TEST_F(FontGenTest, RetainsAndFreesGlyphWhenInsertionFails)
{
    TestContext context = {};
    context.m_CachedGlyphIndex = 0xffffffff;
    context.m_AcceptGlyph = false;

    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 1);
    ASSERT_NE((FontGenJobData*)0, job_data);

    HJob job = FontGenAddGlyphByIndex(job_data, (HFont)&m_Font, 11);
    ASSERT_NE((HJob)0, job);
    JobSystemPushJob(m_Jobs, job);
    RunUntilComplete(&context);

    ASSERT_EQ(1u, m_Font.m_GenerateCount);
    ASSERT_EQ(1u, context.m_AddCount);
    ASSERT_EQ(0, context.m_CompleteResult);
    ASSERT_STRNE("", context.m_Error);
    ASSERT_EQ((FontGlyph*)0, context.m_OwnedGlyph);
    ASSERT_EQ(1u, m_Font.m_FreeCount);

    FontGenDestroyJobData(job_data);
    ASSERT_EQ(1u, m_Font.m_FreeCount);
}

TEST_F(FontGenTest, KeepsSameGlyphIndexFromDifferentFonts)
{
    TestFont fallback_font = m_Font;
    fallback_font.m_Base.m_Path = "fallback-font.ttf";
    fallback_font.m_Base.m_PathHash = dmHashString32(fallback_font.m_Base.m_Path);
    fallback_font.m_GenerateCount = 0;
    fallback_font.m_FreeCount = 0;

    TestContext context = {};
    context.m_CachedGlyphIndex = 0xffffffff;
    context.m_AcceptGlyph = true;

    TextGlyph glyphs[2] = {};
    glyphs[0].m_Font = (HFont)&m_Font;
    glyphs[0].m_GlyphIndex = 7;
    glyphs[1].m_Font = (HFont)&fallback_font;
    glyphs[1].m_GlyphIndex = 7;

    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 2);
    ASSERT_NE((FontGenJobData*)0, job_data);

    HJob job = FontGenAddGlyphs(job_data, glyphs, 2);
    ASSERT_NE((HJob)0, job);
    JobSystemPushJob(m_Jobs, job);
    RunUntilComplete(&context);

    ASSERT_EQ(2u, context.m_CacheLookupCount);
    ASSERT_EQ(1u, m_Font.m_GenerateCount);
    ASSERT_EQ(1u, fallback_font.m_GenerateCount);
    ASSERT_EQ(2u, context.m_AddCount);
    ASSERT_EQ(1, context.m_CompleteResult);

    FontGenDestroyJobData(job_data);
    for (uint32_t i = 0; i < context.m_AddCount; ++i)
    {
        FontFreeGlyph(context.m_OwningFonts[i], context.m_OwnedGlyphs[i]);
        delete context.m_OwnedGlyphs[i];
    }
    ASSERT_EQ(1u, m_Font.m_FreeCount);
    ASSERT_EQ(1u, fallback_font.m_FreeCount);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
