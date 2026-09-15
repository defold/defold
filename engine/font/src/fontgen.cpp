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

#include <dmsdk/dlib/dstrings.h>
#include <dmsdk/dlib/array.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/jobsystem.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/utf8.h>
#include <dmsdk/font/text_layout.h>
#include <dmsdk/extension/extension.h>

#include <dlib/jobsystem.h>
#include <dlib/set.h>
#include <font/internal/glyph_gen.h>

#include "fontgen.h"

//#define FONTGEN_DEBUG

struct FontJobStatus
{
    uint64_t    m_TimeStart;        // Time started
    uint64_t    m_TimeGlyphProcess; // Total processing time for all glyphs
    uint64_t    m_TimeGlyphCallback; // Total processing time for all glyphs
    uint32_t    m_Failures; // Number of failed job items
    char        m_Error[128]; // First error sets this string

    FontJobStatus()
    {
        memset(this, 0, sizeof(*this));
    }
};

struct FontGenJobItem
{
    // input
    HFont           m_Font;         // The actual font to use
    uint32_t        m_GlyphIndex;

    float           m_StbttSdfPadding;
    int             m_StbttEdgeValue;
    float           m_Scale;        // Size to pixel scale

    // From the .fontc info
    float           m_OutlineWidth;
    float           m_ShadowBlur;
    uint8_t         m_IsSdf:1;
    uint8_t         :7;

    // output
    FontGlyph*      m_Glyph;
};

struct Context
{
    uint8_t                     m_StbttDefaultSdfPadding;
    uint8_t                     m_StbttDefaultSdfEdge;
};

struct FontGenJobData
{
    dmArray<FontGenJobItem> m_Items;
    FontJobStatus           m_Status;
    FontGenParams           m_Params;

    HJobContext             m_Jobs;
};

static Context g_FontGenContext = { 3, 191 };

static void ReleaseJobItem(FontGenJobItem* item)
{
    // If it's still set, it wasn't successfully transferred to the .fontc resource
    if (item->m_Glyph)
    {
        HFont font = item->m_Font;
        FontFreeGlyph(font, item->m_Glyph);
        delete item->m_Glyph;
        item->m_Glyph = 0;
    }
}

FontGenJobData* FontGenCreateJobData(const FontGenParams* params, uint32_t num_glyphs)
{
    if (!params || !params->m_Jobs)
        return 0;

    FontGenJobData* jobdata = new FontGenJobData;
    memset(jobdata, 0, sizeof(*jobdata));

    jobdata->m_Items.SetCapacity(num_glyphs);
    jobdata->m_Items.SetSize(jobdata->m_Items.Capacity());

    jobdata->m_Params = *params;
    return jobdata;
}

// this should only be called when the jobs have either finished or been canceled
// In both cases, we expect this to happen on the main thread
//   * From the JobSystemUpdate() - flushing finished/canceled jobs
//   * from ResFontDestroy() - cancelling the jobs in a loop.
//                             Once all have been cancelled, they can be cleared, as it happens before the next JobSystemUpdate()
void FontGenDestroyJobData(FontGenJobData* jobdata)
{
    if (!jobdata)
        return;

    uint32_t size = jobdata->m_Items.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ReleaseJobItem(&jobdata->m_Items[i]);
    }
    delete jobdata;
}

static void FontGenJobDataSetup(FontGenJobData* jobdata)
{
    jobdata->m_Jobs = jobdata->m_Params.m_Jobs;

#if defined(FONTGEN_DEBUG)
    jobdata->m_Status.m_TimeStart = dmTime::GetMonotonicTime();
#endif
}


// Called on the worker thread
static int JobGenerateGlyph(HJobContext job_thread, HJob hjob, void* context, void* data)
{
    FontGenJobData* jobdata = (FontGenJobData*)context;
    FontGenJobItem* item = (FontGenJobItem*)data;
    uint32_t glyph_index = item->m_GlyphIndex;

#if defined(FONTGEN_DEBUG)
    uint64_t tstart = dmTime::GetMonotonicTime();
#endif

    item->m_Glyph = new FontGlyph;
    memset(item->m_Glyph, 0, sizeof(FontGlyph));

    HFont font = item->m_Font;

    FontGlyph* glyph = item->m_Glyph;
    FontResult fr;
    if (jobdata->m_Params.m_IsVector)
    {
        FontGlyphOptions options;
        options.m_Scale = item->m_Scale;
        options.m_GenerateImage = jobdata->m_Params.m_GenerateImage != 0;
        options.m_GenerateOutline = true;
        options.m_StbttSDFPadding = item->m_StbttSdfPadding;
        options.m_StbttSDFOnEdgeValue = item->m_StbttEdgeValue;
        if (jobdata->m_Params.m_BitmapEffects)
        {
            FontGlyphGenParams params;
            params.m_Scale = item->m_Scale;
            params.m_SdfPadding = item->m_StbttSdfPadding;
            params.m_SdfEdgeValue = item->m_StbttEdgeValue;
            params.m_OutlineWidth = item->m_OutlineWidth;
            params.m_ShadowBlur = item->m_ShadowBlur;
            params.m_HasOutline = jobdata->m_Params.m_HasOutline != 0;
            params.m_HasShadow = jobdata->m_Params.m_HasShadow != 0;
            fr = FontGenerateVectorGlyph(font, glyph_index, &params, glyph);
        }
        else
            fr = FontGetGlyphByIndex(font, glyph_index, &options, glyph);
    }
    else
    {
        FontGlyphGenParams params;
        params.m_Scale = item->m_Scale;
        params.m_SdfPadding = item->m_StbttSdfPadding;
        params.m_SdfEdgeValue = item->m_StbttEdgeValue;
        params.m_OutlineWidth = item->m_OutlineWidth;
        params.m_ShadowBlur = item->m_ShadowBlur;
        fr = FontGenerateGlyph(font, glyph_index, &params, glyph);
    }
    if (FONT_RESULT_OK != fr)
    {
        dmLogError("Failed to generate glyph index %u for font '%s'. Result: %d", glyph_index, FontGetPath(font), fr);
        return 0;
    }

// TODO: Protect this using an atomic
#if defined(FONTGEN_DEBUG)
    uint64_t tend = dmTime::GetMonotonicTime();
    jobdata->m_Status.m_TimeGlyphProcess += tend - tstart;
#endif

    return 1;
}

// Only called on the main thread currently
static void SetFailedStatus(FontGenJobData* jobdata, const char* msg)
{
    FontJobStatus* status = &jobdata->m_Status;
    status->m_Failures++;
    if (status->m_Error[0] == 0)
    {
        dmSnPrintf(status->m_Error, sizeof(status->m_Error), "FONTGEN: %s", msg);
    }

    dmLogError("%s", msg); // log for each error in a batch
}

static void InvokeCallback(FontGenJobData* jobdata)
{
    FontJobStatus* status = &jobdata->m_Status;
    if (jobdata->m_Params.m_Complete)
    {
        jobdata->m_Params.m_Complete(jobdata->m_Params.m_UserContext, status->m_Failures == 0, status->m_Error);
    }
}

static int JobProcessSentinelGlyph(HJobContext job_thread, HJob hjob, void* context, void* data)
{
    (void)job_thread;
    (void)hjob;
    (void)context;
    (void)data;
    return 1;
}

static void JobPostProcessSentinelGlyph(HJobContext job_thread, HJob hjob, JobSystemStatus job_status, void* context, void* data, int result)
{
    (void)job_thread;
    (void)hjob;
    (void)data;
    (void)result;

    if (job_status != JOBSYSTEM_STATUS_FINISHED)
    {
        return;
    }

    FontGenJobData* jobdata = (FontGenJobData*)context;

#if defined(FONTGEN_DEBUG)
    uint32_t count = jobdata->m_Items.Size();
    FontJobStatus* status = &jobdata->m_Status;
    uint64_t tend = dmTime::GetMonotonicTime();
    float wall_time = (tend - status->m_TimeStart) / 1000.0f;
    float avg_process = (status->m_TimeGlyphProcess / (float)count) / 1000.0f;
    float avg_callback = (status->m_TimeGlyphCallback / (float)count) / 1000.0f;
    dmLogWarning("Generating %u glyphs took: Job: %.3f ms. Avg (ms/glyph): process: %.3f  callback: %.3f", count, wall_time, avg_process, avg_callback);
#endif

    // This notifies the caller, and after this point we shouldn't rely on the job data memory being alive
    InvokeCallback(jobdata);
}

// Called on the main thread
static void JobPostProcessGlyph(HJobContext job_thread, HJob job, JobSystemStatus job_status, void* context, void* data, int result)
{
    (void)job_thread;
    (void)job;

    if (job_status != JOBSYSTEM_STATUS_FINISHED)
    {
        return;
    }

    FontGenJobData* jobdata = (FontGenJobData*)context;
    FontGenJobItem* item = (FontGenJobItem*)data;

#if defined(FONTGEN_DEBUG)
    uint64_t tstart = dmTime::GetMonotonicTime();
#endif

    if (!item->m_Font)
    {
        ReleaseJobItem(item);
        return;
    }

    uint32_t glyph_index = item->m_GlyphIndex;

    if (!result)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to generate glyph index %u for font '%s'", glyph_index, FontGetPath(item->m_Font));
        SetFailedStatus(jobdata, msg);
        ReleaseJobItem(item);
        return;
    }

    HFont font = item->m_Font;
    FontResult r = jobdata->m_Params.m_AddGlyph
        ? jobdata->m_Params.m_AddGlyph(jobdata->m_Params.m_UserContext, font, item->m_Glyph)
        : FONT_RESULT_ERROR;
    if (FONT_RESULT_OK != r)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to add glyph index %u for font '%s'. Result: %d", glyph_index, FontGetPath(item->m_Font), r);
        SetFailedStatus(jobdata, msg);
    }

    if (FONT_RESULT_OK == r)
    {
        item->m_Glyph = 0; // It was successfully transferred to the .fontc resource (and then the HFontMap)
    }

#if defined(FONTGEN_DEBUG)
    uint64_t tend = dmTime::GetMonotonicTime();
    jobdata->m_Status.m_TimeGlyphCallback += (tend - tstart);
#endif

    ReleaseJobItem(item);
}

// ****************************************************************************************************

static HJob CreateSentinelJob(FontGenJobData* jobdata)
{
    Job job = {0};
    job.m_Process = JobProcessSentinelGlyph;
    job.m_Callback = JobPostProcessSentinelGlyph;
    job.m_Context = jobdata;
    job.m_Data = 0;

    HJob hjob = JobSystemCreateJob(jobdata->m_Jobs, &job);
    return hjob;
}

static void GenerateGlyphJobByIndex(FontGenJobData* jobdata,
                                    FontGenJobItem* item,
                                    HFont font,
                                    uint32_t glyph_index,
                                    float scale, float stbtt_padding, int stbtt_edge,
                                    bool is_sdf, float outline_width, float shadow_blur,
                                    HJob job_sentinel)
{
    memset(item, 0, sizeof(*item));
    item->m_GlyphIndex = glyph_index;
    item->m_Font = font;
    item->m_Scale = scale;
    item->m_IsSdf = is_sdf;
    item->m_OutlineWidth = outline_width;
    item->m_ShadowBlur = shadow_blur;
    item->m_StbttSdfPadding = stbtt_padding;
    item->m_StbttEdgeValue = stbtt_edge;

    Job job = {0};
    job.m_Process = JobGenerateGlyph;
    job.m_Callback = JobPostProcessGlyph;
    job.m_Context = jobdata;
    job.m_Data = (void*)item;

    HJob hjob = JobSystemCreateJob(jobdata->m_Jobs, &job);
    JobSystemSetParent(jobdata->m_Jobs, hjob, job_sentinel);

    JobSystemPushJob(jobdata->m_Jobs, hjob);
}


static bool GenerateGlyphByIndex(FontGenJobData* jobdata, HFont font, uint32_t jobindex,
                                uint32_t glyph_index, float scale, HJob job_sentinel)
{
    const FontGenParams* params = &jobdata->m_Params;
    bool is_sdf = params->m_IsSdf != 0;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return false;
    }

    int stbtt_edge = g_FontGenContext.m_StbttDefaultSdfEdge;
    float stbtt_padding = g_FontGenContext.m_StbttDefaultSdfPadding + params->m_OutlineWidth;

    // See Fontc.java. If we have shadow blur, we need 3 channels
    bool has_shadow = params->m_ShadowBlur > 0.0f;
    stbtt_padding += has_shadow ? params->m_ShadowBlur : 0.0f;

    FontGenJobItem* item = &jobdata->m_Items[jobindex];

    GenerateGlyphJobByIndex(jobdata, item, font, glyph_index, scale, stbtt_padding, stbtt_edge, is_sdf,
                    params->m_OutlineWidth, has_shadow ? params->m_ShadowBlur : 0.0f, job_sentinel);
    return true;
}


static HJob GenerateGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs)
{
    // TODO: Support bitmap fonts
    bool is_sdf = jobdata->m_Params.m_IsSdf != 0;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return 0;
    }

    HJob job_sentinel = CreateSentinelJob(jobdata);

    // Given the prewarm text, it may be that there are a lot of duplicated glyph indices
    // So we only want to push requests for the unique ones
    dmSet<dmhash_t> unique;
    unique.OffsetCapacity(num_glyphs);

    HFont prev_font = 0;
    float prev_scale = 1;

    uint32_t count = 0;
    for (uint32_t i = 0; i < num_glyphs; ++i)
    {
        TextGlyph* glyph = &glyphs[i];

        if (glyph->m_Flags & TEXT_GLYPH_FLAG_OBJECT)
        {
            continue;
        }

        HFont font = glyph->m_Font;
        uint32_t glyph_index = glyph->m_GlyphIndex;
        dmhash_t glyph_key = ((dmhash_t)FontGetPathHash(font) << 32) | glyph_index;

        // test and/or add at the same time
        if (!unique.Add(glyph_key))
            continue;

        float scale = prev_scale;

        if (jobdata->m_Params.m_IsGlyphCached &&
            jobdata->m_Params.m_IsGlyphCached(jobdata->m_Params.m_UserContext, font, glyph_index))
            continue;

        if (prev_font != font)
        {
            scale = FontGetScaleFromSize(font, jobdata->m_Params.m_Size);
            prev_scale = scale;
            prev_font = font;
        }

        GenerateGlyphByIndex(jobdata, font, count++, glyph_index, scale, job_sentinel);
    }

    jobdata->m_Items.SetSize(count); // The number of valid glyphs

    return job_sentinel;
}

dmExtension::Result FontGenInitialize(dmExtension::Params* params)
{
    // 3 is arbitrary but resembles the output from our old generator
    g_FontGenContext.m_StbttDefaultSdfPadding = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.stbtt_sdf_base_padding", 3);
    g_FontGenContext.m_StbttDefaultSdfEdge = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.stbtt_sdf_edge_value", 191);
    return dmExtension::RESULT_OK;
}

bool FontGenIsSupported()
{
    return true;
}

dmExtension::Result FontGenFinalize(dmExtension::Params*)
{
    g_FontGenContext.m_StbttDefaultSdfPadding = 3;
    g_FontGenContext.m_StbttDefaultSdfEdge = 191;
    return dmExtension::RESULT_OK;
}

void FontGenFlushFinishedJobs(HJobContext jobs, uint64_t timeout)
{
    JobSystemUpdate(jobs, timeout);
}

float FontGenGetBasePadding()
{
    return g_FontGenContext.m_StbttDefaultSdfPadding;
}

float FontGenGetEdgeValue()
{
    return g_FontGenContext.m_StbttDefaultSdfEdge;
}

// Resource api

// Called on cache misses by res_font.cpp
HJob FontGenAddGlyphByIndex(FontGenJobData* jobdata, HFont font, uint32_t glyph_index)
{
    if (!jobdata)
        return 0;
    FontGenJobDataSetup(jobdata);

// TODO: Don't create a sentinel job for a single job!
    HJob job_sentinel = CreateSentinelJob(jobdata);

    float scale = FontGetScaleFromSize(font, jobdata->m_Params.m_Size);
    GenerateGlyphByIndex(jobdata, font, 0, glyph_index, scale, job_sentinel);
    return job_sentinel;
}

// Called to prewarm text by res_font.cpp
HJob FontGenAddGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs)
{
    if (!jobdata)
        return 0;
    FontGenJobDataSetup(jobdata);
    return GenerateGlyphs(jobdata, glyphs, num_glyphs);
}
