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

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/atomic.h>
#include <dlib/jobsystem.h>
#include <dlib/thread.h>
#include <dlib/time.h>

#include "../font.h"
#include "../fontgen.h"

struct TestFont
{
    Font                m_Base;
    int32_atomic_t      m_GenerateCount;
    uint32_t            m_FreeCount;
    uint32_t            m_FailGlyphIndex;
    bool                m_BlockGeneration;
    int32_atomic_t      m_GenerationStarted;
    int32_atomic_t      m_AllowGeneration;
};

struct TestContext
{
    uint32_t            m_CachedGlyphIndex;
    uint32_t            m_CacheLookupCount;
    uint32_t            m_AddCount;
    uint32_t            m_CompleteCount;
    int                 m_CompleteResult;
    bool                m_AcceptGlyph;
    FontGlyph*          m_OwnedGlyph;
    FontGlyph*          m_OwnedGlyphs[4];
    HFont               m_OwningFonts[4];
    char                m_Error[128];
    dmThread::Thread    m_ExpectedThread;
    bool                m_CallbackThreadCorrect;
    int32_atomic_t      m_UnrelatedProcesses;
    uint32_t            m_UnrelatedCallbacks;
    FontGenJobData*     m_DestroyOnComplete;
};

struct CallerBatch
{
    FontGenJobData*     m_JobData;
    TestContext*        m_Context;
    bool                m_Complete;
};

struct WorkerBatch
{
    FontGenJobData*     m_JobData;
    TestContext*        m_Context;
    TextGlyph           m_Glyph;
    int32_atomic_t*     m_Started;
    int32_atomic_t*     m_Finished;
    uint32_t            m_CallerCount;
    bool                m_Complete;
};

static float TestGetScaleFromSize(HFont, uint32_t size)
{
    return size / 10.0f;
}

static FontResult TestGetGlyph(HFont font, uint32_t glyph_index, const FontGlyphOptions*, FontGlyph* glyph)
{
    TestFont* test_font = (TestFont*)font;
    dmAtomicIncrement32(&test_font->m_GenerateCount);
    if (test_font->m_BlockGeneration)
    {
        dmAtomicStore32(&test_font->m_GenerationStarted, 1);
        while (!dmAtomicGet32(&test_font->m_AllowGeneration))
        {
        }
    }
    if (glyph_index == test_font->m_FailGlyphIndex)
        return FONT_RESULT_ERROR;

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
    test->m_CallbackThreadCorrect = test->m_CallbackThreadCorrect && dmThread::GetCurrentThread() == test->m_ExpectedThread;
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
    test->m_CallbackThreadCorrect = test->m_CallbackThreadCorrect && dmThread::GetCurrentThread() == test->m_ExpectedThread;
    ++test->m_CompleteCount;
    test->m_CompleteResult = result;
    dmStrlCpy(test->m_Error, error_message ? error_message : "", sizeof(test->m_Error));
    if (test->m_DestroyOnComplete)
    {
        FontGenDestroyJobData(test->m_DestroyOnComplete);
        test->m_DestroyOnComplete = 0;
    }
}

static int32_t UnrelatedProcess(HJobContext, HJob, void* context, void*)
{
    TestContext* test = (TestContext*)context;
    dmAtomicIncrement32(&test->m_UnrelatedProcesses);
    return 0;
}

static void UnrelatedCallback(HJobContext, HJob, JobSystemStatus, void* context, void*, int32_t)
{
    ++((TestContext*)context)->m_UnrelatedCallbacks;
}

#if defined(DM_HAS_THREADS)
static void UpdateBatchFromThread(void* data)
{
    CallerBatch* batch = (CallerBatch*)data;
    batch->m_Context->m_ExpectedThread = dmThread::GetCurrentThread();
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!batch->m_Complete && dmTime::GetMonotonicTime() < deadline)
        batch->m_Complete = FontGenUpdateGlyphs(batch->m_JobData, 0);
}
#endif

static int32_t GenerateBatchFromJob(HJobContext, HJob, void* context, void*)
{
    WorkerBatch* batch = (WorkerBatch*)context;
    batch->m_Context->m_ExpectedThread = dmThread::GetCurrentThread();
    dmAtomicIncrement32(batch->m_Started);
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    // Occupy every worker before any of them can submit glyph work.
    while ((uint32_t)dmAtomicGet32(batch->m_Started) != batch->m_CallerCount && dmTime::GetMonotonicTime() < deadline)
    {
    }
    if ((uint32_t)dmAtomicGet32(batch->m_Started) == batch->m_CallerCount && FontGenStartGlyphs(batch->m_JobData, &batch->m_Glyph, 1))
    {
        deadline = dmTime::GetMonotonicTime() + 5000000;
        while (!batch->m_Complete && dmTime::GetMonotonicTime() < deadline)
            batch->m_Complete = FontGenUpdateGlyphs(batch->m_JobData, 0);
    }
    FontGenDestroyJobData(batch->m_JobData);
    dmAtomicIncrement32(batch->m_Finished);
    return 0;
}

class FontGenTest : public jc_test_params_class<uint32_t>
{
protected:
    HJobContext m_Jobs;
    TestFont    m_Font;

    virtual void SetUp() override
    {
        JobSystemCreateParams job_params = {};
        job_params.m_ThreadCount = GetParam();
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
        context->m_ExpectedThread = dmThread::GetCurrentThread();
        context->m_CallbackThreadCorrect = true;
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

    void RunOwnedUntilComplete(FontGenJobData* job_data, TestContext* context)
    {
        uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
        bool complete = false;
        while (!complete && dmTime::GetMonotonicTime() < deadline)
            complete = FontGenUpdateGlyphs(job_data, 0);
        ASSERT_TRUE(complete);
        ASSERT_EQ(1u, context->m_CompleteCount);
        ASSERT_TRUE(context->m_CallbackThreadCorrect);
    }
};

// Glyph generation skips cached duplicates and transfers each inserted glyph to the caller.
TEST_P(FontGenTest, GeneratesUniqueUncachedGlyphsAndTransfersOwnership)
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

// Failed cache insertion leaves glyph ownership with the generator for cleanup.
TEST_P(FontGenTest, RetainsAndFreesGlyphWhenInsertionFails)
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

// Equal glyph indices from different fonts remain distinct generation requests.
TEST_P(FontGenTest, KeepsSameGlyphIndexFromDifferentFonts)
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

// A resource-owned batch completes without running unrelated callbacks; zero workers also leave unrelated processing untouched.
TEST_P(FontGenTest, CallerConsumesOwnResultsAndCanDestroyBatchInCallback)
{
    TestContext context = {};
    context.m_AcceptGlyph = true;
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 2);
    context.m_DestroyOnComplete = job_data;
    Job unrelated = {};
    unrelated.m_Process = UnrelatedProcess;
    unrelated.m_Callback = UnrelatedCallback;
    unrelated.m_Context = &context;
    JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &unrelated));

    TextGlyph glyphs[2] = {};
    for (uint32_t i = 0; i < 2; ++i)
    {
        glyphs[i].m_Font = (HFont)&m_Font;
        glyphs[i].m_GlyphIndex = i + 1;
    }
    ASSERT_TRUE(FontGenStartGlyphs(job_data, glyphs, 2));
    RunOwnedUntilComplete(job_data, &context);
    ASSERT_EQ((FontGenJobData*)0, context.m_DestroyOnComplete);
    ASSERT_EQ(2u, context.m_AddCount);
    ASSERT_EQ(1, context.m_CompleteResult);
    ASSERT_EQ(0u, context.m_UnrelatedCallbacks);
    if (!JobSystemGetWorkerCount(m_Jobs))
        ASSERT_EQ(0, dmAtomicGet32(&context.m_UnrelatedProcesses));

    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!context.m_UnrelatedCallbacks && dmTime::GetMonotonicTime() < deadline)
        JobSystemUpdate(m_Jobs, 0);
    ASSERT_EQ(1u, context.m_UnrelatedCallbacks);
    for (uint32_t i = 0; i < context.m_AddCount; ++i)
    {
        FontFreeGlyph(context.m_OwningFonts[i], context.m_OwnedGlyphs[i]);
        delete context.m_OwnedGlyphs[i];
    }
    ASSERT_EQ(2u, m_Font.m_FreeCount);
}

// Failed worker generation reaches the resource owner and releases its glyph without a job-system update.
TEST_P(FontGenTest, CallerReceivesGenerationFailure)
{
    TestContext context = {};
    context.m_AcceptGlyph = true;
    m_Font.m_FailGlyphIndex = 11;
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 1);
    TextGlyph glyph = {};
    glyph.m_Font = (HFont)&m_Font;
    glyph.m_GlyphIndex = 11;
    ASSERT_TRUE(FontGenStartGlyphs(job_data, &glyph, 1));
    RunOwnedUntilComplete(job_data, &context);
    ASSERT_EQ(0u, context.m_AddCount);
    ASSERT_EQ(0, context.m_CompleteResult);
    ASSERT_STRNE("", context.m_Error);
    ASSERT_EQ(1u, m_Font.m_FreeCount);
    FontGenDestroyJobData(job_data);
}

// Completion belongs to the requesting resource thread even when it does not own the shared job context.
TEST_P(FontGenTest, CallerCanCompleteOnAnotherThread)
{
#if defined(DM_HAS_THREADS)
    TestContext context = {};
    context.m_AcceptGlyph = true;
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 1);
    TextGlyph glyph = {};
    glyph.m_Font = (HFont)&m_Font;
    glyph.m_GlyphIndex = 11;
    ASSERT_TRUE(FontGenStartGlyphs(job_data, &glyph, 1));
    CallerBatch batch = { job_data, &context, false };
    dmThread::Thread thread = dmThread::New(UpdateBatchFromThread, 0x20000, &batch, "FontOwner");
    dmThread::Join(thread);
    ASSERT_TRUE(batch.m_Complete);
    ASSERT_EQ(1u, context.m_AddCount);
    ASSERT_EQ(1u, context.m_CompleteCount);
    ASSERT_TRUE(context.m_CallbackThreadCorrect);
    FontGenDestroyJobData(job_data);
    FontFreeGlyph((HFont)&m_Font, context.m_OwnedGlyph);
    delete context.m_OwnedGlyph;
#endif
}

// Callers occupying every job worker can finish their own glyphs without waiting for free workers.
TEST_P(FontGenTest, CallerCompletesFromSaturatedJobPool)
{
    uint32_t count = JobSystemGetWorkerCount(m_Jobs);
    if (!count)
        count = 1;
    TestContext contexts[4] = {};
    WorkerBatch batches[4] = {};
    int32_atomic_t started = 0;
    int32_atomic_t finished = 0;
    ASSERT_LE(count, DM_ARRAY_SIZE(batches));
    for (uint32_t i = 0; i < count; ++i)
    {
        contexts[i].m_AcceptGlyph = true;
        FontGenParams params = MakeParams(&contexts[i]);
        batches[i].m_JobData = FontGenCreateJobData(&params, 1);
        batches[i].m_Context = &contexts[i];
        batches[i].m_Glyph.m_Font = (HFont)&m_Font;
        batches[i].m_Glyph.m_GlyphIndex = i + 1;
        batches[i].m_Started = &started;
        batches[i].m_Finished = &finished;
        batches[i].m_CallerCount = count;
        Job job = {};
        job.m_Process = GenerateBatchFromJob;
        job.m_Context = &batches[i];
        JobSystemPushJob(m_Jobs, JobSystemCreateJob(m_Jobs, &job));
    }
    uint64_t deadline = dmTime::GetMonotonicTime() + 10000000;
    while ((uint32_t)dmAtomicGet32(&finished) != count && dmTime::GetMonotonicTime() < deadline)
        if (!JobSystemGetWorkerCount(m_Jobs))
            JobSystemUpdate(m_Jobs, 0);
    JobSystemDestroy(m_Jobs);
    m_Jobs = 0;
    ASSERT_EQ(count, (uint32_t)dmAtomicGet32(&finished));
    for (uint32_t i = 0; i < count; ++i)
    {
        ASSERT_TRUE(batches[i].m_Complete);
        ASSERT_EQ(1u, contexts[i].m_AddCount);
        ASSERT_EQ(1u, contexts[i].m_CompleteCount);
        ASSERT_TRUE(contexts[i].m_CallbackThreadCorrect);
        FontFreeGlyph((HFont)&m_Font, contexts[i].m_OwnedGlyph);
        delete contexts[i].m_OwnedGlyph;
    }
    ASSERT_EQ(count, (uint32_t)dmAtomicGet32(&m_Font.m_GenerateCount));
    ASSERT_EQ(count, m_Font.m_FreeCount);
}

// Updating a batch leaves an executing glyph with its worker and consumes its result exactly once.
TEST_P(FontGenTest, CallerWaitsForRunningGlyph)
{
    if (!JobSystemGetWorkerCount(m_Jobs))
        return;
    TestContext context = {};
    context.m_AcceptGlyph = true;
    m_Font.m_BlockGeneration = true;
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 1);
    TextGlyph glyph = {};
    glyph.m_Font = (HFont)&m_Font;
    glyph.m_GlyphIndex = 11;
    ASSERT_TRUE(FontGenStartGlyphs(job_data, &glyph, 1));
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    while (!dmAtomicGet32(&m_Font.m_GenerationStarted) && dmTime::GetMonotonicTime() < deadline)
    {
    }
    bool started = dmAtomicGet32(&m_Font.m_GenerationStarted) != 0;
    bool complete = started && FontGenUpdateGlyphs(job_data, 0);
    dmAtomicStore32(&m_Font.m_AllowGeneration, 1);
    ASSERT_TRUE(started);
    ASSERT_FALSE(complete);
    ASSERT_EQ(0u, context.m_AddCount);
    RunOwnedUntilComplete(job_data, &context);
    ASSERT_EQ(1u, context.m_AddCount);
    ASSERT_EQ(1, dmAtomicGet32(&m_Font.m_GenerateCount));
    FontGenDestroyJobData(job_data);
    FontFreeGlyph((HFont)&m_Font, context.m_OwnedGlyph);
    delete context.m_OwnedGlyph;
    ASSERT_EQ(1u, m_Font.m_FreeCount);
}

// A zero time budget advances at most one glyph, including inline generation on non-threaded platforms.
TEST_P(FontGenTest, CallerUpdateHonorsGlyphBudget)
{
    TestContext context = {};
    context.m_AcceptGlyph = true;
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 3);
    TextGlyph glyphs[3] = {};
    for (uint32_t i = 0; i < 3; ++i)
    {
        glyphs[i].m_Font = (HFont)&m_Font;
        glyphs[i].m_GlyphIndex = i + 1;
    }
    ASSERT_TRUE(FontGenStartGlyphs(job_data, glyphs, 3));
    uint64_t deadline = dmTime::GetMonotonicTime() + 5000000;
    for (uint32_t i = 1; i <= 3; ++i)
    {
        while (context.m_AddCount < i && dmTime::GetMonotonicTime() < deadline)
        {
            bool complete = FontGenUpdateGlyphs(job_data, 0);
            if (i < 3)
                ASSERT_FALSE(complete);
        }
        ASSERT_EQ(i, context.m_AddCount);
    }
    RunOwnedUntilComplete(job_data, &context);
    FontGenDestroyJobData(job_data);
    for (uint32_t i = 0; i < context.m_AddCount; ++i)
    {
        FontFreeGlyph(context.m_OwningFonts[i], context.m_OwnedGlyphs[i]);
        delete context.m_OwnedGlyphs[i];
    }
}

// Failed insertion retains ownership in the caller-updated batch, as in callback-driven generation.
TEST_P(FontGenTest, CallerRejectsGlyphAndGeneratorFreesIt)
{
    TestContext context = {};
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 1);
    TextGlyph glyph = {};
    glyph.m_Font = (HFont)&m_Font;
    glyph.m_GlyphIndex = 11;
    ASSERT_TRUE(FontGenStartGlyphs(job_data, &glyph, 1));
    RunOwnedUntilComplete(job_data, &context);
    ASSERT_EQ(1u, context.m_AddCount);
    ASSERT_EQ(0, context.m_CompleteResult);
    ASSERT_EQ(1u, m_Font.m_FreeCount);
    FontGenDestroyJobData(job_data);
}

// Batches with only cached glyphs still complete exactly once without shared callback dispatch.
TEST_P(FontGenTest, CallerCompletesEmptyBatchOnce)
{
    TestContext context = {};
    context.m_CachedGlyphIndex = 11;
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 1);
    TextGlyph glyph = {};
    glyph.m_Font = (HFont)&m_Font;
    glyph.m_GlyphIndex = 11;
    ASSERT_TRUE(FontGenStartGlyphs(job_data, &glyph, 1));
    RunOwnedUntilComplete(job_data, &context);
    ASSERT_TRUE(FontGenUpdateGlyphs(job_data, 0));
    ASSERT_EQ(1u, context.m_CompleteCount);
    ASSERT_EQ(0u, context.m_AddCount);
    ASSERT_EQ(0, dmAtomicGet32(&m_Font.m_GenerateCount));
    FontGenDestroyJobData(job_data);
}

// Destroying an unconsumed batch stops its jobs before freeing glyph data and never invokes completion.
TEST_P(FontGenTest, DestroyCallerBatchBeforeCompletion)
{
    TestContext context = {};
    FontGenParams params = MakeParams(&context);
    FontGenJobData* job_data = FontGenCreateJobData(&params, 4);
    TextGlyph glyphs[4] = {};
    for (uint32_t i = 0; i < 4; ++i)
    {
        glyphs[i].m_Font = (HFont)&m_Font;
        glyphs[i].m_GlyphIndex = i + 1;
    }
    ASSERT_TRUE(FontGenStartGlyphs(job_data, glyphs, 4));
    FontGenDestroyJobData(job_data);
    JobSystemUpdate(m_Jobs, 1000);
    ASSERT_EQ(0u, context.m_CompleteCount);
    ASSERT_EQ(0u, context.m_AddCount);
    ASSERT_EQ((uint32_t)dmAtomicGet32(&m_Font.m_GenerateCount), m_Font.m_FreeCount);
}

// Unsupported generation fails before submitting jobs and its empty scratch storage can be destroyed safely.
TEST_P(FontGenTest, RejectsUnsupportedCallerBatch)
{
    TestContext context = {};
    FontGenParams params = MakeParams(&context);
    params.m_IsSdf = 0;
    FontGenJobData* job_data = FontGenCreateJobData(&params, 4);
    ASSERT_FALSE(FontGenStartGlyphs(job_data, 0, 0));
    FontGenDestroyJobData(job_data);
    ASSERT_EQ(0u, context.m_CompleteCount);
}

static uint32_t g_WorkerCounts[] = { 0, 1, 4 };
INSTANTIATE_TEST_CASE_P(Workers, FontGenTest, jc_test_values_in(g_WorkerCounts));

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
