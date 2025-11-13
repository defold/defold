// Copyright 2020-2025 The Defold Foundation
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
#include <dmsdk/dlib/hashtable.h>
#include <dmsdk/dlib/log.h>
#include <dmsdk/dlib/mutex.h>
#include <dmsdk/dlib/time.h>
#include <dmsdk/dlib/utf8.h>
#include <dmsdk/script/script.h>
#include <dlib/math.h>

#include <resource/resource.h>
#include <dmsdk/gamesys/resources/res_font.h>
#include <dmsdk/gamesys/resources/res_ttf.h>
#include <dmsdk/font/text_layout.h>
#include <dmsdk/extension/extension.h>

#include <dlib/job_thread.h>

#include "fontgen.h"

//#define FONTGEN_DEBUG

namespace dmGameSystem
{

struct JobStatus
{
    uint64_t    m_TimeStart;        // Time started
    uint64_t    m_TimeGlyphProcess; // Total processing time for all glyphs
    uint64_t    m_TimeGlyphCallback; // Total processing time for all glyphs
    uint32_t    m_Count;    // Number of job items pushed
    uint32_t    m_Failures; // Number of failed job items
    const char* m_Error; // First error sets this string
};

struct JobItem
{
    dmMutex::HMutex m_Mutex;

    // input
    FontResource*   m_FontResource;
    HFont           m_Font;         // The actual font to use

    uint32_t        m_GlyphIndex;

    float           m_StbttSdfPadding;
    int             m_StbttEdgeValue;
    float           m_Scale;        // Size to pixel scale

    // From the .fontc info
    float           m_OutlineWidth;
    float           m_ShadowBlur;
    uint8_t         m_IsSdf:1;
    uint8_t         m_Deleted:1;
    uint8_t         :6;

    // "global"
    JobStatus*      m_Status; // same for all items in the same job batch. Owned by the sentinel job

    dmGameSystem::FPrewarmTextCallback  m_Callback;     // Only set for the last item in the queue
    void*                               m_CallbackCtx;  // Only set for the last item in the queue

    // output
    FontGlyph*      m_Glyph;
};

struct Context
{
    dmMutex::HMutex             m_Mutex;
    HResourceFactory            m_ResourceFactory;
    dmJobThread::HContext       m_Jobs;
    uint8_t                     m_StbttDefaultSdfPadding;
    uint8_t                     m_StbttDefaultSdfEdge;
};

Context* g_FontExtContext = 0;

// Assumes the lock is already held
static void ReleaseResources(Context* ctx, JobItem* item)
{
    // If it's still set, it wasn't successfully transferred to the .fontc resource
    if (item->m_Glyph)
    {
        HFont font = item->m_Font;
        FontFreeGlyph(font, item->m_Glyph);
        item->m_Glyph = 0;
    }

    item->m_FontResource = 0;
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
    (void)context;
    JobItem* item = (JobItem*)data;

    // As we've incref'd these resources, they should not have gone out of scope
    if (!item->m_FontResource)
        return 0;
    if (!item->m_Font)
        return 0;

    uint32_t glyph_index = item->m_GlyphIndex;

    uint64_t tstart = dmTime::GetMonotonicTime();

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

    uint64_t tend = dmTime::GetMonotonicTime();

// TODO: Protect this using a spinlock
    JobStatus* status = item->m_Status;
    status->m_TimeGlyphProcess += tend - tstart;

    return 1;
}

static void SetFailedStatus(JobItem* item, const char* msg)
{
    JobStatus* status = item->m_Status;
    status->m_Failures++;
    if (status->m_Error == 0)
    {
        status->m_Error = strdup(msg);
    }

    dmLogError("%s", msg); // log for each error in a batch
}

static void InvokeCallback(dmJobThread::HContext job_thread, dmJobThread::HJob hjob, dmJobThread::JobStatus job_status, JobItem* item)
{
    JobStatus* status = item->m_Status;
    if (item->m_Callback) // only the last item has this callback
    {
        item->m_Callback(item->m_CallbackCtx, status->m_Failures == 0, status->m_Error);

// // TODO: Hide this behind a verbosity flag
// // TODO: Protect this using a spinlock
//         dmLogInfo("Generated %u glyphs in %.2f ms", status->m_Count, status->m_TimeGlyphProcess/1000.0f);
    }
}

static void DeleteItem(Context* ctx, JobItem* item)
{
    ReleaseResources(ctx, item);

    if (item->m_Callback) // It's the last item
    {
        free((void*)item->m_Status->m_Error);
        delete item->m_Status;
    }
    delete item;
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
    Context* ctx = (Context*)context;
    JobItem* item = (JobItem*)data;
    JobStatus* status = item->m_Status;
    InvokeCallback(job_thread, hjob, job_status, item);

#if defined(FONTGEN_DEBUG)
    uint64_t tend = dmTime::GetMonotonicTime();
    float wall_time = (tend - status->m_TimeStart) / 1000.0f;
    float avg_process = (status->m_TimeGlyphProcess / (float)status->m_Count) / 1000.0f;
    float avg_callback = (status->m_TimeGlyphCallback / (float)status->m_Count) / 1000.0f;
    dmLogWarning("Generating %u glyphs took %.3f ms. Avg (ms/glyph): process: %.3f  callback: %.3f", status->m_Count, wall_time, avg_process, avg_callback);
#endif

    DeleteItem(ctx, item);
}

// Called on the main thread
static void JobPostProcessGlyph(dmJobThread::HContext job_thread, dmJobThread::HJob job, dmJobThread::JobStatus job_status, void* context, void* data, int result)
{
    Context* ctx = (Context*)context;
    JobItem* item = (JobItem*)data;
    JobStatus* status = item->m_Status;

    uint64_t tstart = dmTime::GetMonotonicTime();

    DM_MUTEX_OPTIONAL_SCOPED_LOCK(item->m_Mutex);
    if (!item->m_FontResource || !item->m_Font)
    {
        DeleteItem(ctx, item);
        return;
    }

    uint32_t glyph_index = item->m_GlyphIndex;

    if (!result)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to generate glyph index %u for font '%s'", glyph_index, FontGetPath(item->m_Font));
        SetFailedStatus(item, msg);
        DeleteItem(ctx, item);
        return;
    }

    // The font system takes ownership of the image data
    HFont font = item->m_Font;
    dmResource::Result r = dmGameSystem::ResFontAddGlyph(item->m_FontResource, font, item->m_Glyph);
    if (dmResource::RESULT_OK != r)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to add glyph index %u for font '%s'. Result: %d", glyph_index, FontGetPath(item->m_Font), r);
        SetFailedStatus(item, msg);
    }

    if (dmResource::RESULT_OK == r)
    {
        item->m_Glyph = 0; // It was successfully transferred to the .fontc resource (and then the HFontMap)
    }

    uint64_t tend = dmTime::GetMonotonicTime();
    status->m_TimeGlyphCallback += (tend - tstart);

    DeleteItem(ctx, item);
}

// ****************************************************************************************************

static dmJobThread::HJob CreateSentinelJob(Context* ctx,
                                            JobStatus* status, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    JobItem* item = new JobItem;
    memset(item, 0, sizeof(*item));
    item->m_Callback = cbk;
    item->m_CallbackCtx = cbk_ctx;
    item->m_Status = status;

    dmJobThread::Job job = {0};
    job.m_Process = JobProcessSentinelGlyph;
    job.m_Callback = JobPostProcessSentinelGlyph;
    job.m_Context = ctx;
    job.m_Data = (void*)item;

    dmJobThread::HJob hjob = dmJobThread::CreateJob(ctx->m_Jobs, &job);
    return hjob;
}

static void GenerateGlyphJobByIndex(Context* ctx,
                                    FontResource* fontresource,
                                    HFont font,
                                    uint32_t glyph_index,
                                    float scale, float stbtt_padding, int stbtt_edge,
                                    bool is_sdf, float outline_width, float shadow_blur,
                                    JobStatus* status,
                                    dmJobThread::HJob job_sentinel)
{
    JobItem* item = new JobItem;
    memset(item, 0, sizeof(*item));
    item->m_GlyphIndex = glyph_index;
    item->m_FontResource = fontresource;
    item->m_Font = font;
    item->m_Status = status;
    item->m_Scale = scale;
    item->m_IsSdf = is_sdf;
    item->m_OutlineWidth = outline_width;
    item->m_ShadowBlur = shadow_blur;
    item->m_StbttSdfPadding = stbtt_padding;
    item->m_StbttEdgeValue = stbtt_edge;
    item->m_Mutex = ctx->m_Mutex;

    dmJobThread::Job job = {0};
    job.m_Process = JobGenerateGlyph;
    job.m_Callback = JobPostProcessGlyph;
    job.m_Context = ctx;
    job.m_Data = (void*)item;

    dmJobThread::HJob hjob = dmJobThread::CreateJob(ctx->m_Jobs, &job);
    SetParent(ctx->m_Jobs, hjob, job_sentinel);

    dmJobThread::PushJob(ctx->m_Jobs, hjob);
}


static bool GenerateGlyphByIndex(Context* ctx, FontResource* fontresource, HFont font,
                                dmGameSystem::FontInfo* font_info, uint32_t glyph_index, float scale,
                                JobStatus* status, dmJobThread::HJob job_sentinel)
{
    bool is_sdf = dmRenderDDF::TYPE_DISTANCE_FIELD == font_info->m_OutputFormat;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return false;
    }

    int stbtt_edge = ctx->m_StbttDefaultSdfEdge;
    float stbtt_padding = ctx->m_StbttDefaultSdfPadding + font_info->m_OutlineWidth;

    // See Fontc.java. If we have shadow blur, we need 3 channels
    bool has_shadow = font_info->m_ShadowAlpha > 0.0f && font_info->m_ShadowBlur > 0.0f;

    if (dmRenderDDF::MODE_MULTI_LAYER == font_info->m_RenderMode)
    {
        stbtt_padding += has_shadow ? font_info->m_ShadowBlur : 0.0f;
    }

    GenerateGlyphJobByIndex(ctx, fontresource, font, glyph_index, scale, stbtt_padding, stbtt_edge, is_sdf,
                    font_info->m_OutlineWidth, has_shadow ? font_info->m_ShadowBlur : 0.0f,
                    status, job_sentinel);
    return true;
}


static dmJobThread::HJob GenerateGlyphs(Context* ctx, FontResource* fontresource,
                                    TextGlyph* glyphs, uint32_t num_glyphs,
                                    dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    dmGameSystem::FontInfo font_info;
    dmGameSystem::ResFontGetInfo(fontresource, &font_info);

    // TODO: Support bitmap fonts
    bool is_sdf = dmRenderDDF::TYPE_DISTANCE_FIELD == font_info.m_OutputFormat;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return 0;
    }

    JobStatus* status           = new JobStatus;
    status->m_TimeStart         = dmTime::GetMonotonicTime();
    status->m_TimeGlyphProcess  = 0;
    status->m_TimeGlyphCallback = 0;
    status->m_Count             = 0;
    status->m_Failures          = 0;
    status->m_Error             = 0;

    HFont prev_font = 0;
    float prev_scale = 1;

    dmJobThread::HJob job_sentinel = CreateSentinelJob(ctx, status, cbk, cbk_ctx);

    // Given the prewarm text, it may be that there are a lot of duplicated glyph indices
    // So we only want to push requests for the unique ones
    dmHashTable32<bool> unique;
    unique.OffsetCapacity(num_glyphs);

    for (uint32_t i = 0; i < num_glyphs; ++i)
    {
        TextGlyph* glyph = &glyphs[i];

        uint32_t glyph_index = glyph->m_GlyphIndex;

        bool* is_unique = unique.Get(glyph_index);
        if (is_unique)
            continue; // It means we've already pushed this codepoint in this loop
        unique.Put(glyph_index, true);

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

        GenerateGlyphByIndex(ctx, fontresource, font, &font_info, glyph_index, scale,
                                status, job_sentinel);

        status->m_Count++;
    }

    return job_sentinel;
}

dmExtension::Result FontGenInitialize(dmExtension::Params* params)
{
    g_FontExtContext = new Context;
    g_FontExtContext->m_ResourceFactory = params->m_ResourceFactory;
    // As we're interacting with the resources, it's good to use the resource mutex
    g_FontExtContext->m_Mutex = dmResource::GetLoadMutex(params->m_ResourceFactory);

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

dmExtension::Result FontGenUpdate(dmExtension::Params* params)
{
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
dmJobThread::HJob FontGenAddGlyphByIndex(FontResource* fontresource, HFont font, uint32_t glyph_index, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    Context* ctx = g_FontExtContext;

    dmGameSystem::FontInfo font_info;
    dmGameSystem::ResFontGetInfo(fontresource, &font_info);

    float scale = FontGetScaleFromSize(font, font_info.m_Size);

    JobStatus* status           = new JobStatus;
    status->m_TimeStart         = dmTime::GetMonotonicTime();
    status->m_TimeGlyphProcess  = 0;
    status->m_TimeGlyphCallback = 0;
    status->m_Count             = 1;
    status->m_Failures          = 0;
    status->m_Error             = 0;

// TODO: Don't create a sentinel job for a single job!
    dmJobThread::HJob job_sentinel = CreateSentinelJob(ctx, status, cbk, cbk_ctx);
    GenerateGlyphByIndex(ctx, fontresource, font, &font_info, glyph_index, scale, status, job_sentinel);
    return job_sentinel;
}

// Called to prewarm text by res_font.cpp
dmJobThread::HJob FontGenAddGlyphs(FontResource* fontresource, TextGlyph* glyphs, uint32_t num_glyphs, dmGameSystem::FPrewarmTextCallback cbk, void* cbk_ctx)
{
    Context* ctx = g_FontExtContext;
    return GenerateGlyphs(ctx, fontresource, glyphs, num_glyphs, cbk, cbk_ctx);
}

} // namespace
