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
#include <dmsdk/extension/extension.h>

#include <dlib/job_thread.h>

#include "fontgen.h"

namespace dmGameSystem
{

struct JobStatus
{
    uint64_t    m_TimeGlyphGen;
    uint32_t    m_Count;    // Number of job items pushed
    uint32_t    m_Failures; // Number of failed job items
    const char* m_Error; // First error sets this string
};

struct JobItem
{
    dmMutex::HMutex m_Mutex;

    // input
    FontResource*   m_FontResource; // Incref'd for each job item
    TTFResource*    m_TTFResource;  // Incref'd for each job item

    uint32_t        m_Codepoint;

    float           m_StbttSdfPadding;
    int             m_StbttEdgeValue;
    float           m_Scale;        // Size to pixel scale

    // From the .fontc info
    float           m_OutlineWidth;
    float           m_ShadowBlur;
    uint8_t         m_IsSdf:1;
    uint8_t         m_Deleted:1;
    uint8_t         m_IsLoading:1;  // If set, we're in the prewarming sequence, and can't IncRef the font resource, as it's being loaded!

    // "global"
    JobStatus*      m_Status;       // same for all items in the same job batch
    FGlyphCallback  m_Callback;     // Only set for the last item in the queue
    void*           m_CallbackCtx;  // Only set for the last item in the queue

    // output
    dmGameSystem::FontGlyph m_Glyph;
    uint8_t*                m_Data;     // May be 0. First byte is the compression (0=no compression, 1=deflate)
    uint32_t                m_DataSize;
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

static void ReleaseResources(Context* ctx, JobItem* item)
{
    if (item->m_FontResource && !item->m_IsLoading)
        dmResource::Release(ctx->m_ResourceFactory, item->m_FontResource);
    item->m_FontResource = 0;

    if (item->m_TTFResource)
        dmResource::Release(ctx->m_ResourceFactory, item->m_TTFResource);
    item->m_TTFResource = 0;
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
static int JobGenerateGlyph(dmJobThread::HContext, dmJobThread::HJob job, void* context, void* data)
{
    (void)context;
    JobItem* item = (JobItem*)data;

    // As we've incref'd these resources, they should not have gone out of scope
    if (!item->m_FontResource)
        return 0;
    if (!item->m_TTFResource)
        return 0;

    uint32_t codepoint = item->m_Codepoint;

    uint64_t tstart = dmTime::GetMonotonicTime();

    item->m_Data = 0;
    item->m_DataSize = 0;
    memset(&item->m_Glyph, 0, sizeof(item->m_Glyph));

    bool is_whitespace = dmGameSystem::IsWhiteSpace(codepoint);

    dmGameSystem::TTFResource* ttfresource = item->m_TTFResource;
    dmFont::HFont font = dmGameSystem::GetFont(ttfresource);

    dmFont::GlyphOptions options;
    options.m_Scale = item->m_Scale;
    options.m_GenerateImage = true;

    if (dmFont::GetType(font) == dmFont::FONT_TYPE_STBTTF)
    {
        options.m_StbttSDFPadding       = item->m_StbttSdfPadding;
        options.m_StbttSDFOnEdgeValue   = item->m_StbttEdgeValue;
    }

    dmFont::Glyph glyph;
    dmFont::FontResult fr = dmFont::GetGlyph(font, codepoint, &options, &glyph);
    if (dmFont::RESULT_NOT_SUPPORTED == fr)
    {
        if (is_whitespace)
        {
            return 1; // We deal with white spaces in the next callback
        }
        dmLogError("Codepoint not supported: '%c' 0x%04X", (char)codepoint, codepoint);
        return 0;
    }

    if (item->m_ShadowBlur > 0.0f && glyph.m_Bitmap.m_Data)
    {
        // To support the old shadow algorithm, we need to rescale the values,
        // so that values >outline border are within the shapes

        // TODO: Tbh, it feels like we should be able to use a single distance field channel.
        // We should look into it if we ever choose the new code path as the default.

        // Make a copy
        glyph.m_Bitmap.m_Channels = 3;
        uint32_t w = glyph.m_Bitmap.m_Width;
        uint32_t h = glyph.m_Bitmap.m_Height;
        uint32_t ch = glyph.m_Bitmap.m_Channels;
        uint32_t newsize = w*h*ch;

        uint8_t* rgb = (uint8_t*)malloc(newsize);

        float outline_width      = item->m_OutlineWidth;
        float outline_edge_value = CalcSdfValueU8(options.m_StbttSDFPadding, outline_width);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                uint8_t value = glyph.m_Bitmap.m_Data[y * w + x];
                uint8_t shadow_value = Remap(value, outline_edge_value);

                rgb[y * (w * ch) + (x * ch) + 0] = value;
                rgb[y * (w * ch) + (x * ch) + 1] = 0;
                rgb[y * (w * ch) + (x * ch) + 2] = shadow_value;
            }
        }
        free((void*)glyph.m_Bitmap.m_Data);
        glyph.m_Bitmap.m_Data = rgb;
    }

    item->m_Glyph.m_Width = glyph.m_Width;
    item->m_Glyph.m_Height = glyph.m_Height;
    item->m_Glyph.m_Advance = glyph.m_Advance;
    item->m_Glyph.m_Ascent = glyph.m_Ascent;
    item->m_Glyph.m_Descent = glyph.m_Descent;
    item->m_Glyph.m_LeftBearing = glyph.m_LeftBearing;

    if (!glyph.m_Bitmap.m_Data) // Some glyphs (e.g. ' ') don't have an image, which is ok
    {
        if (!is_whitespace)
            return 0; // Something went wrong
    }
    else
    {
        item->m_Glyph.m_ImageWidth = glyph.m_Bitmap.m_Width;
        item->m_Glyph.m_ImageHeight = glyph.m_Bitmap.m_Height;
        item->m_Glyph.m_Channels = glyph.m_Bitmap.m_Channels;

        // TODO: avoid this copy altogether!
        //     * copy the pointer as-is
        //     * pass the "flags" parameter separately
        //     * preferably we could pass the dmFont::Glyph as is

        item->m_DataSize = 1 + glyph.m_Bitmap.m_Width * glyph.m_Bitmap.m_Height * glyph.m_Bitmap.m_Channels;
        item->m_Data = (uint8_t*)malloc(item->m_DataSize);
        memcpy(item->m_Data+1, glyph.m_Bitmap.m_Data, item->m_DataSize-1);
        item->m_Data[0] = glyph.m_Bitmap.m_Flags;
    }

    dmFont::FreeGlyph(font, &glyph);

    uint64_t tend = dmTime::GetMonotonicTime();
// TODO: Protect this using a spinlock
    JobStatus* status = item->m_Status;
    status->m_TimeGlyphGen += tend - tstart;

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

static void InvokeCallback(dmJobThread::HContext, dmJobThread::HJob hjob, JobItem* item)
{
    JobStatus* status = item->m_Status;
    if (item->m_Callback) // only the last item has this callback
    {
        item->m_Callback(item->m_CallbackCtx, status->m_Failures == 0, status->m_Error);

// // TODO: Hide this behind a verbosity flag
// // TODO: Protect this using a spinlock
//         dmLogInfo("Generated %u glyphs in %.2f ms", status->m_Count, status->m_TimeGlyphGen/1000.0f);
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

// Called on the main thread
static void JobPostProcessGlyph(dmJobThread::HContext job_thread, dmJobThread::HJob job, dmJobThread::JobStatus status, void* context, void* data, int result)
{
    Context* ctx = (Context*)context;
    JobItem* item = (JobItem*)data;

    DM_MUTEX_OPTIONAL_SCOPED_LOCK(item->m_Mutex);
    if (!item->m_FontResource || !item->m_TTFResource)
    {
        DeleteItem(ctx, item);
        return;
    }

    uint32_t codepoint = item->m_Codepoint;

    if (!result)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to generate glyph '%c' 0x%04X", codepoint, codepoint);
        SetFailedStatus(item, msg);
        InvokeCallback(job_thread, job, item);
        DeleteItem(ctx, item);
        return;
    }

    // The font system takes ownership of the image data
    dmResource::Result r = dmGameSystem::ResFontAddGlyph(item->m_FontResource, codepoint, &item->m_Glyph, item->m_Data, item->m_DataSize);

    if (dmResource::RESULT_OK != r)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to add glyph '%c': result: %d", codepoint, r);
        SetFailedStatus(item, msg);
    }

    InvokeCallback(job_thread, job, item); // reports either first error, or success
    DeleteItem(ctx, item);
}

// ****************************************************************************************************

static void GenerateGlyph(Context* ctx,
                            FontResource* fontresource, TTFResource* ttfresource, uint32_t codepoint,
                            float scale, float stbtt_padding, int stbtt_edge,
                            bool is_sdf, float outline_width, float shadow_blur,
                            bool is_loading,
                            JobStatus* status, FGlyphCallback cbk, void* cbk_ctx)
{
    JobItem* item = new JobItem;
    item->m_FontResource = fontresource;
    item->m_TTFResource = ttfresource;
    item->m_Codepoint = codepoint;
    item->m_Callback = cbk;
    item->m_CallbackCtx = cbk_ctx;
    item->m_Status = status;
    item->m_Scale = scale;
    item->m_IsSdf = is_sdf;
    item->m_IsLoading = is_loading;
    item->m_OutlineWidth = outline_width;
    item->m_ShadowBlur = shadow_blur;
    item->m_StbttSdfPadding = stbtt_padding;
    item->m_StbttEdgeValue = stbtt_edge;
    item->m_Mutex = is_loading ? 0 : ctx->m_Mutex;

    dmJobThread::Job job = {0};
    job.m_Process = JobGenerateGlyph;
    job.m_Callback = JobPostProcessGlyph;
    job.m_Context = ctx;
    job.m_Data = item;

    dmJobThread::HJob hjob = dmJobThread::CreateJob(ctx->m_Jobs, &job);
    dmJobThread::PushJob(ctx->m_Jobs, hjob);
}

static bool GenerateGlyphs(Context* ctx, FontResource* fontresource,
                            const char* text, bool loading, FGlyphCallback cbk, void* cbk_ctx)
{
    uint32_t len = dmUtf8::StrLen(text);

    dmGameSystem::FontInfo font_info;
    dmGameSystem::ResFontGetInfo(fontresource, &font_info);

    // TODO: Support bitmap fonts
    bool is_sdf = dmRenderDDF::TYPE_DISTANCE_FIELD == font_info.m_OutputFormat;
    if (!is_sdf)
    {
        dmLogError("Only SDF fonts are supported");
        return false;
    }


    JobStatus* status      = new JobStatus;
    status->m_TimeGlyphGen = 0;
    status->m_Count        = len;
    status->m_Failures     = 0;
    status->m_Error        = 0;

    int stbtt_edge = ctx->m_StbttDefaultSdfEdge;
    float stbtt_padding = ctx->m_StbttDefaultSdfPadding + font_info.m_OutlineWidth;

    // See Fontc.java. If we have shadow blur, we need 3 channels
    bool has_shadow = font_info.m_ShadowAlpha > 0.0f && font_info.m_ShadowBlur > 0.0f;

    if (dmRenderDDF::MODE_MULTI_LAYER == font_info.m_RenderMode)
    {
        stbtt_padding += has_shadow ? font_info.m_ShadowBlur : 0.0f;
    }

    const char* cursor = text;
    uint32_t c = 0;
    uint32_t index  = 0;
    while ((c = dmUtf8::NextChar(&cursor)))
    {
        ++index;
        bool last_item = len == index;

        FGlyphCallback callback = 0;
        void*          callback_ctx = 0;
        if (last_item) // Only the last item needs the callback
        {
            callback = cbk;
            callback_ctx = cbk_ctx;
        }

        TTFResource* ttfresource = dmGameSystem::ResFontGetTTFResourceFromCodepoint(fontresource, c);
        if (!ttfresource)
            continue;

        // Incref the resources so that they're not accidentally removed during this threaded work
        if (!loading) // If we're in the process of creating this resource, then we cannot incref it!
            dmResource::IncRef(ctx->m_ResourceFactory, fontresource);
        dmResource::IncRef(ctx->m_ResourceFactory, ttfresource);

        dmFont::HFont font = dmGameSystem::GetFont(ttfresource);
        float scale = dmFont::GetPixelScaleFromSize(font, font_info.m_Size);

        // TODO: Pass dmFont::HFont instead of ttfresource
        GenerateGlyph(ctx, fontresource, ttfresource, c, scale, stbtt_padding, stbtt_edge, is_sdf,
                        font_info.m_OutlineWidth, has_shadow ? font_info.m_ShadowBlur : 0.0f,
                        loading,
                        status, callback, callback_ctx);
    }
    return true;
}

static void RemoveGlyphs(Context* ctx, FontResource* resource, const char* text)
{
    DM_MUTEX_SCOPED_LOCK(ctx->m_Mutex);

    const char* cursor = text;
    uint32_t c = 0;
    while ((c = dmUtf8::NextChar(&cursor)))
    {
        dmGameSystem::ResFontRemoveGlyph(resource, c);
    }
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


// Scripting

bool FontGenAddGlyphs(FontResource* fontresource, const char* text, bool loading, FGlyphCallback cbk, void* cbk_ctx)
{
    Context* ctx = g_FontExtContext;
    return GenerateGlyphs(ctx, fontresource, text, loading, cbk, cbk_ctx);
}


bool FontGenRemoveGlyphs(FontResource* fontresource, const char* text)
{
    Context* ctx = g_FontExtContext;
    RemoveGlyphs(ctx, fontresource, text);
    return true;
}



} // namespace
