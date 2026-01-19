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
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/utf8.h>
#include <dmsdk/script/script.h>
#include <dmsdk/gamesys/resources/res_font.h>
#include <dmsdk/gamesys/resources/res_ttf.h>
#include <dmsdk/font/text_layout.h>
#include <dmsdk/extension/extension.h>

#include <dlib/job_thread.h>
#include <dlib/set.h>
#include <resource/resource.h>

#include "fontgen.h"

//#define FONTGEN_DEBUG

namespace dmGameSystem
{

struct JobStatus
{
    uint64_t    m_TimeStart;        // Time started
    uint64_t    m_TimeGlyphProcess; // Total processing time for all glyphs
    uint64_t    m_TimeGlyphCallback; // Total processing time for all glyphs
    uint32_t    m_Failures; // Number of failed job items
    char        m_Error[128]; // First error sets this string

    JobStatus()
    {
        memset(this, 0, sizeof(*this));
    }
};

struct JobItem
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
    HResourceFactory            m_ResourceFactory;
    dmJobThread::HContext       m_Jobs;
    uint8_t                     m_StbttDefaultSdfPadding;
    uint8_t                     m_StbttDefaultSdfEdge;
};

struct FontGenJobData
{
    dmArray<JobItem>        m_Items;
    JobStatus               m_Status;
    dmGameSystem::FontInfo  m_FontInfo; // Metrics for the FontResource

    FontResource*           m_FontResource; // Handle to the .fontc resource

    dmJobThread::HContext               m_Jobs;
    dmGameSystem::FPrewarmTextCallback  m_Callback;     // Only set for the last item in the queue
    void*                               m_CallbackCtx;  // Only set for the last item in the queue
};

Context* g_FontExtContext = 0;

static void ReleaseJobItem(JobItem* item)
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

FontGenJobData* FontGenCreateJobData(FontResource* font, uint32_t num_glyphs)
{
    FontGenJobData* jobdata = new FontGenJobData;
    memset(jobdata, 0, sizeof(*jobdata));

    jobdata->m_Items.SetCapacity(num_glyphs);
    jobdata->m_Items.SetSize(jobdata->m_Items.Capacity());

    jobdata->m_FontResource = font;

    dmGameSystem::ResFontGetInfo(jobdata->m_FontResource, &jobdata->m_FontInfo);
    return jobdata;
}

// this should only be called when the jobs have either finished or been canceled
// In both cases, we expect this to happen on the main thread
//   * From the dmJobThread::Update() - flushing finished/canceled jobs
//   * from ResFontDestroy() - cancelling the jobs in a loop.
//                             Once all have been cancelled, they can be cleared, as it happens before the next dmJobThread::Update()
void FontGenDestroyJobData(FontGenJobData* jobdata)
{
    uint32_t size = jobdata->m_Items.Size();
    for (uint32_t i = 0; i < size; ++i)
    {
        ReleaseJobItem(&jobdata->m_Items[i]);
    }
    delete jobdata;
}

static void FontGenJobDataSetup(FontGenJobData* jobdata, uint32_t num_glyphs, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    jobdata->m_Callback = cbk;
    jobdata->m_CallbackCtx = cbk_ctx;
    jobdata->m_Jobs = g_FontExtContext->m_Jobs;

#if defined(FONTGEN_DEBUG)
    jobdata->m_Status.m_TimeStart = dmTime::GetMonotonicTime();
#endif
}


static float CalcSdfValueU8(float padding, float width)
{
    float on_edge_value = dmGameSystem::FontGenGetEdgeValue(); // [0 .. 255] e.g. 191
    const float base_edge = SDF_EDGE_VALUE * 255.0f;

    // Described in the stb_truetype.h as "what value the SDF should increase by when moving one SDF "pixel" away from the edge"
    float pixel_dist_scale = (float)on_edge_value/padding;

    return (base_edge - (pixel_dist_scale * width));
}

static float Remap(float value, float outline_edge)
{
    float unit = value / outline_edge;
    return dmMath::Clamp(unit, 0.0f, 1.0f) * SDF_EDGE_VALUE * 255.0f;
}

// Called on the worker thread
static int JobGenerateGlyph(dmJobThread::HContext job_thread, dmJobThread::HJob hjob, void* context, void* data)
{
    FontGenJobData* jobdata = (FontGenJobData*)context;
    JobItem* item = (JobItem*)data;
    (void)jobdata;

    uint32_t glyph_index = item->m_GlyphIndex;

#if defined(FONTGEN_DEBUG)
    uint64_t tstart = dmTime::GetMonotonicTime();
#endif

    item->m_Glyph = new FontGlyph;
    memset(item->m_Glyph, 0, sizeof(FontGlyph));

    HFont font = item->m_Font;

    FontGlyphOptions options;
    options.m_Scale = item->m_Scale;
    options.m_GenerateImage = true;

    if (FontGetType(font) == FONT_TYPE_STBTTF)
    {
        options.m_StbttSDFPadding       = item->m_StbttSdfPadding;
        options.m_StbttSDFOnEdgeValue   = item->m_StbttEdgeValue;
    }

    FontGlyph* glyph = item->m_Glyph;
    FontResult fr = FontGetGlyphByIndex(font, glyph_index, &options, glyph);
    if (FONT_RESULT_NOT_SUPPORTED == fr)
    {
        dmLogError("Glyph index %u not found in font '%s'", glyph_index, FontGetPath(font));
        return 1;
    }

    if (item->m_ShadowBlur > 0.0f && glyph->m_Bitmap.m_Data)
    {
        // To support the old shadow algorithm, we need to rescale the values,
        // so that values >outline border are within the shapes

        // TODO: Tbh, it feels like we should be able to use a single distance field channel.
        // We should look into it if we ever choose the new code path as the default.

        // Make a copy
        glyph->m_Bitmap.m_Channels = 3;
        uint32_t w = glyph->m_Bitmap.m_Width;
        uint32_t h = glyph->m_Bitmap.m_Height;
        uint32_t ch = glyph->m_Bitmap.m_Channels;
        uint32_t newsize = w*h*ch;

        uint8_t* rgb = (uint8_t*)malloc(newsize);

        float outline_width      = item->m_OutlineWidth;
        float outline_edge_value = CalcSdfValueU8(options.m_StbttSDFPadding, outline_width);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                uint8_t value = glyph->m_Bitmap.m_Data[y * w + x];
                uint8_t shadow_value = Remap(value, outline_edge_value);

                rgb[y * (w * ch) + (x * ch) + 0] = value;
                rgb[y * (w * ch) + (x * ch) + 1] = 0;
                rgb[y * (w * ch) + (x * ch) + 2] = shadow_value;
            }
        }
        free((void*)glyph->m_Bitmap.m_Data);
        glyph->m_Bitmap.m_Data = rgb;
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
    JobStatus* status = &jobdata->m_Status;
    status->m_Failures++;
    if (status->m_Error[0] != 0)
    {
        dmSnPrintf(status->m_Error, sizeof(status->m_Error), "FONTGEN: %s", msg);
    }

    dmLogError("%s", msg); // log for each error in a batch
}

static void InvokeCallback(FontGenJobData* jobdata)
{
    JobStatus* status = &jobdata->m_Status;
    if (jobdata->m_Callback)
    {
        jobdata->m_Callback(jobdata->m_CallbackCtx, status->m_Failures == 0, status->m_Error);
    }
}

static int JobProcessSentinelGlyph(dmJobThread::HContext job_thread, dmJobThread::HJob hjob, void* context, void* data)
{
    (void)job_thread;
    (void)hjob;
    (void)context;
    (void)data;
    return 1;
}

static void JobPostProcessSentinelGlyph(dmJobThread::HContext job_thread, dmJobThread::HJob hjob, dmJobThread::JobStatus job_status, void* context, void* data, int result)
{
    FontGenJobData* jobdata = (FontGenJobData*)context;
    (void)data;

#if defined(FONTGEN_DEBUG)
    uint32_t count = jobdata->m_Items.Size();
    JobStatus* status = &jobdata->m_Status;
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
static void JobPostProcessGlyph(dmJobThread::HContext job_thread, dmJobThread::HJob job, dmJobThread::JobStatus job_status, void* context, void* data, int result)
{
    FontGenJobData* jobdata = (FontGenJobData*)context;
    JobItem* item = (JobItem*)data;

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

    // The font system takes ownership of the image data
    HFont font = item->m_Font;
    dmResource::Result r = dmGameSystem::ResFontAddGlyph(jobdata->m_FontResource, font, item->m_Glyph);
    if (dmResource::RESULT_OK != r)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to add glyph index %u for font '%s'. Result: %d", glyph_index, FontGetPath(item->m_Font), r);
        SetFailedStatus(jobdata, msg);
    }

    if (dmResource::RESULT_OK == r)
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

static dmJobThread::HJob CreateSentinelJob(FontGenJobData* jobdata)
{
    dmJobThread::Job job = {0};
    job.m_Process = JobProcessSentinelGlyph;
    job.m_Callback = JobPostProcessSentinelGlyph;
    job.m_Context = jobdata;
    job.m_Data = 0;

    dmJobThread::HJob hjob = dmJobThread::CreateJob(jobdata->m_Jobs, &job);
    return hjob;
}

static void GenerateGlyphJobByIndex(FontGenJobData* jobdata,
                                    JobItem* item,
                                    HFont font,
                                    uint32_t glyph_index,
                                    float scale, float stbtt_padding, int stbtt_edge,
                                    bool is_sdf, float outline_width, float shadow_blur,
                                    dmJobThread::HJob job_sentinel)
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

    dmJobThread::Job job = {0};
    job.m_Process = JobGenerateGlyph;
    job.m_Callback = JobPostProcessGlyph;
    job.m_Context = jobdata;
    job.m_Data = (void*)item;

    dmJobThread::HJob hjob = dmJobThread::CreateJob(jobdata->m_Jobs, &job);
    SetParent(jobdata->m_Jobs, hjob, job_sentinel);

    dmJobThread::PushJob(jobdata->m_Jobs, hjob);
}


static bool GenerateGlyphByIndex(FontGenJobData* jobdata, HFont font, uint32_t jobindex,
                                uint32_t glyph_index, float scale, dmJobThread::HJob job_sentinel)
{
    const dmGameSystem::FontInfo* font_info = &jobdata->m_FontInfo;

    bool is_sdf = dmRenderDDF::TYPE_DISTANCE_FIELD == font_info->m_OutputFormat;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return false;
    }

    Context* ctx = g_FontExtContext;
    int stbtt_edge = ctx->m_StbttDefaultSdfEdge;
    float stbtt_padding = ctx->m_StbttDefaultSdfPadding + font_info->m_OutlineWidth;

    // See Fontc.java. If we have shadow blur, we need 3 channels
    bool has_shadow = font_info->m_ShadowAlpha > 0.0f && font_info->m_ShadowBlur > 0.0f;

    if (dmRenderDDF::MODE_MULTI_LAYER == font_info->m_RenderMode)
    {
        stbtt_padding += has_shadow ? font_info->m_ShadowBlur : 0.0f;
    }

    JobItem* item = &jobdata->m_Items[jobindex];

    GenerateGlyphJobByIndex(jobdata, item, font, glyph_index, scale, stbtt_padding, stbtt_edge, is_sdf,
                    font_info->m_OutlineWidth, has_shadow ? font_info->m_ShadowBlur : 0.0f, job_sentinel);
    return true;
}


static dmJobThread::HJob GenerateGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs)
{
    dmGameSystem::FontInfo& font_info = jobdata->m_FontInfo;

    // TODO: Support bitmap fonts
    bool is_sdf = dmRenderDDF::TYPE_DISTANCE_FIELD == font_info.m_OutputFormat;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return 0;
    }

    dmJobThread::HJob job_sentinel = CreateSentinelJob(jobdata);

    FontResource* fontresource = jobdata->m_FontResource;

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

        uint32_t glyph_index = glyph->m_GlyphIndex;

        // test and/or add at the same time
        if (!unique.Add(glyph_index))
            continue;

        HFont font = glyph->m_Font;
        float scale = prev_scale;

        if (ResFontIsGlyphIndexCached(fontresource, font, glyph_index))
            continue;

        if (prev_font != font)
        {
            scale = FontGetScaleFromSize(font, font_info.m_Size);
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
    g_FontExtContext = new Context;
    g_FontExtContext->m_ResourceFactory = params->m_ResourceFactory;

    // 3 is arbitrary but resembles the output from our old generator
    g_FontExtContext->m_StbttDefaultSdfPadding = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.stbtt_sdf_base_padding", 3);
    g_FontExtContext->m_StbttDefaultSdfEdge = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.stbtt_sdf_edge_value", 191);

    g_FontExtContext->m_Jobs = dmExtension::GetContextAsType<dmJobThread::HContext>(params, "job_thread");
    return dmExtension::RESULT_OK;
}

dmExtension::Result FontGenFinalize(dmExtension::Params* params)
{
    delete g_FontExtContext;
    g_FontExtContext = 0;
    return dmExtension::RESULT_OK;
}

void FontGenFlushFinishedJobs(uint64_t timeout)
{
    dmJobThread::Update(g_FontExtContext->m_Jobs, timeout);
}

float FontGenGetBasePadding()
{
    return g_FontExtContext->m_StbttDefaultSdfPadding;
}

float FontGenGetEdgeValue()
{
    return g_FontExtContext->m_StbttDefaultSdfEdge;
}

// Resource api

// Called on cache misses by res_font.cpp
dmJobThread::HJob FontGenAddGlyphByIndex(FontGenJobData* jobdata, HFont font, uint32_t glyph_index, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    Context* ctx = g_FontExtContext;
    FontGenJobDataSetup(jobdata, 1, cbk, cbk_ctx);

// TODO: Don't create a sentinel job for a single job!
    dmJobThread::HJob job_sentinel = CreateSentinelJob(jobdata);

    float scale = FontGetScaleFromSize(font, jobdata->m_FontInfo.m_Size);
    GenerateGlyphByIndex(jobdata, font, 0, glyph_index, scale, job_sentinel);
    return job_sentinel;
}

// Called to prewarm text by res_font.cpp
dmJobThread::HJob FontGenAddGlyphs(FontGenJobData* jobdata, TextGlyph* glyphs, uint32_t num_glyphs, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    FontGenJobDataSetup(jobdata, num_glyphs, cbk, cbk_ctx);
    return GenerateGlyphs(jobdata, glyphs, num_glyphs);
}

} // namespace
