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

#include <dmsdk/gamesys/resources/res_font.h>
#include <dmsdk/extension/extension.h>

#include <dlib/job_thread.h>

#include <dmsdk/gamesys/resources/res_font.h>

#include "res_ttf.h"
#include "fontgen.h"

namespace dmFontGen
{

struct FontInfo
{
    dmMutex::HMutex             m_Mutex;
    dmGameSystem::FontResource* m_FontResource;
    dmFont::TTFResource*        m_TTFResource;
    int                         m_Padding;
    int                         m_EdgeValue;
    float                       m_Scale;

    uint8_t                     m_IsSdf:1;
    uint8_t                     m_HasShadow:1;
    uint8_t                     m_Deleted:1;
};

struct Context
{
    dmMutex::HMutex             m_Mutex;
    HResourceFactory            m_ResourceFactory;
    dmHashTable64<FontInfo*>    m_FontInfos;        // Loaded .fontc files
    dmHashTable64<FontInfo*>    m_DeletedFontInfos; // Unloaded .fontc files about to be deleted
    dmJobThread::HContext       m_Jobs;
    uint8_t                     m_DefaultSdfPadding;
    uint8_t                     m_DefaultSdfEdge;
};

Context* g_FontExtContext = 0;

static bool CheckType(HResourceFactory factory, const char* path, const char** types, uint32_t num_types)
{
    HResourceDescriptor rd;
    dmResource::Result r = dmResource::GetDescriptor(factory, path, &rd);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get descriptor for resource %s", path);
        return 0;
    }

    HResourceType type = ResourceDescriptorGetType(rd);
    const char* type_name = ResourceTypeGetName(type);

    for (uint32_t i = 0; i < num_types; ++i)
    {
        if (strcmp(type_name, types[i]) == 0)
            return true;
    }
    return false;
}

static dmFont::TTFResource* LoadFontData(Context* ctx, const char* path)
{
    dmFont::TTFResource* resource = 0;
    dmResource::Result r = dmResource::Get(ctx->m_ResourceFactory, path, (void**)&resource);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get resource '%s'", path);
        return 0;
    }

    const char* types[] = { "ttf" };
    if (!CheckType(ctx->m_ResourceFactory, path, types, 1))
    {
        dmLogError("Wrong type of resource %s (expected %s)", path, types[0]);
        dmResource::Release(ctx->m_ResourceFactory, resource);
        return 0;
    }

    dmLogInfo("Loaded font data'%s", path);
    return resource;
}

static void ReleaseResources(Context* ctx, FontInfo* info)
{
    if (info->m_FontResource)
        dmResource::Release(ctx->m_ResourceFactory, info->m_FontResource);
    info->m_FontResource = 0;

    if (info->m_TTFResource)
        dmResource::Release(ctx->m_ResourceFactory, info->m_TTFResource);
    info->m_TTFResource = 0;
}

static void DeleteFontNoLock(Context* ctx, FontInfo* info)
{
    ReleaseResources(ctx, info);
    delete info;
}

static void DeleteFont(Context* ctx, FontInfo* info)
{
    DM_MUTEX_SCOPED_LOCK(info->m_Mutex);
    DeleteFontNoLock(ctx, info);
}

static void DelayDeleteFont(Context* ctx, FontInfo* info, dmhash_t path_hash)
{
    DM_MUTEX_SCOPED_LOCK(info->m_Mutex);
    // Release resources as early as possible
    // This will also stop any pending jobs from doing work
    ReleaseResources(ctx, info);
    info->m_Deleted = 1;
    if (ctx->m_DeletedFontInfos.Full())
    {
        uint32_t cap = ctx->m_DeletedFontInfos.Capacity() + 8;
        ctx->m_DeletedFontInfos.SetCapacity((cap*3/2), cap);
    }
    ctx->m_DeletedFontInfos.Put(path_hash, info);
}

static bool UnloadFont(Context* ctx, dmhash_t fontc_path_hash)
{
    FontInfo** infop = ctx->m_FontInfos.Get(fontc_path_hash);
    if (!infop)
    {
        dmLogError("Font not loaded: %s", dmHashReverseSafe64(fontc_path_hash));
        return false;
    }

    FontInfo* info = *infop;
    DelayDeleteFont(ctx, info, fontc_path_hash);
    ctx->m_FontInfos.Erase(fontc_path_hash);
    return true;
}

static FontInfo* LoadFont(Context* ctx, const char* fontc_path, const char* ttf_path)
{
    dmhash_t path_hash = dmHashString64(fontc_path);

    FontInfo* info = new FontInfo;
    memset(info, 0, sizeof(*info));

    dmResource::Result r = dmResource::Get(ctx->m_ResourceFactory, fontc_path, (void**)&info->m_FontResource);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get .fontc resource '%s'", fontc_path);
        DeleteFontNoLock(ctx, info);
        return 0;
    }

    const char* types[] = { "fontc" };
    if (!CheckType(ctx->m_ResourceFactory, fontc_path, types, 1))
    {
        DeleteFontNoLock(ctx, info);
        return 0;
    }

    info->m_TTFResource = LoadFontData(ctx, ttf_path);
    if (!info->m_TTFResource)
    {
        DeleteFontNoLock(ctx, info);
        return 0;
    }

    dmGameSystem::FontInfo font_info;
    r = dmGameSystem::ResFontGetInfo(info->m_FontResource, &font_info);
    if (dmResource::RESULT_OK != r)
    {
        dmLogError("Failed to get font info from '%s'", fontc_path);
        DeleteFontNoLock(ctx, info);
        return 0;
    }

    if (dmRenderDDF::TYPE_DISTANCE_FIELD != font_info.m_OutputFormat)
    {
        dmLogError("Currently only distance field fonts are supported: %s", fontc_path);
        DeleteFontNoLock(ctx, info);
        return 0;
    }

    info->m_Mutex = ctx->m_Mutex;

    info->m_Padding = ctx->m_DefaultSdfPadding;
    if (dmRenderDDF::MODE_MULTI_LAYER == font_info.m_RenderMode)
    {
        // see Fontc.java
        const float rootOf2 = sqrtf(2.0f);

        // x2 to make it more visually equal to our previous generation
        if (font_info.m_OutlineWidth > 0)
            info->m_Padding += 2.0f * (font_info.m_OutlineWidth + rootOf2);
        if (font_info.m_ShadowBlur > 0)
            info->m_Padding += 2.0f * (font_info.m_ShadowBlur + rootOf2);
    }

    // See Fontc.java. If we have shadow blur, we need 3 channels
    info->m_HasShadow    = font_info.m_ShadowAlpha > 0.0f && font_info.m_ShadowBlur > 0.0f;

    info->m_EdgeValue    = ctx->m_DefaultSdfEdge;
    info->m_Scale        = dmFont::SizeToScale(info->m_TTFResource, font_info.m_Size);

    // TODO: Support bitmap fonts
    info->m_IsSdf        = dmRenderDDF::TYPE_DISTANCE_FIELD == font_info.m_OutputFormat;

    // In our system, both ascent/descent are positive distances from the baseline
    float max_ascent = dmFont::GetAscent(info->m_TTFResource, info->m_Scale);
    float max_descent = -dmFont::GetDescent(info->m_TTFResource, info->m_Scale);
    dmGameSystem::ResFontSetLineHeight(info->m_FontResource, max_ascent, max_descent);

    // This returns a too large size, which is impractical, and causes the cache to be filled too quickly
    // Instead, we do the cell size check in the engine, when adding more dybnamic glyphs
    // uint32_t cell_width = 0;
    // uint32_t cell_height = 0;
    // uint32_t max_ascent = 0;
    // dmFont::GetCellSize(info->m_TTFResource, &cell_width, &cell_height, &max_ascent);

    if (ctx->m_FontInfos.Full())
    {
        uint32_t cap = ctx->m_FontInfos.Capacity() + 8;
        ctx->m_FontInfos.SetCapacity((cap*2)/3, cap);
    }
    ctx->m_FontInfos.Put(path_hash, info);

    dmLogInfo("Loaded font '%s", fontc_path);
    return info;
}

// Only called at shutdown of the extension
static void DeleteFontInfoIter(Context* ctx, const dmhash_t* hash, FontInfo** infop)
{
    FontInfo* info = *infop;
    if (!info->m_Deleted)
    {
        dmLogWarning("Font resource wasn't released: %s", dmHashReverseSafe64(*hash));
    }
    DeleteFont(ctx, info);
}

// ****************************************************************************************************

struct JobStatus
{
    uint64_t    m_TimeGlyphGen;
    uint32_t    m_Count;    // Number of job items pushed
    uint32_t    m_Failures; // Number of failed job items
    const char* m_Error; // First error sets this string
};

struct JobItem
{
    // input
    FontInfo*       m_FontInfo;
    uint32_t        m_Codepoint;
    //
    JobStatus*      m_Status;
    FGlyphCallback  m_Callback;
    void*           m_CallbackCtx;
    bool            m_LastItem; // true if it's the last item in the batch
    // output
    dmGameSystem::FontGlyph m_Glyph;
    uint8_t*                m_Data;     // May be 0. First byte is the compression (0=no compression, 1=deflate)
    uint32_t                m_DataSize;
};

// Called on the worker thread
static int JobGenerateGlyph(void* context, void* data)
{
    (void)context;
    JobItem* item = (JobItem*)data;
    FontInfo* info = item->m_FontInfo;
    DM_MUTEX_SCOPED_LOCK(info->m_Mutex);
    if (!info->m_FontResource)
        return 0;

    uint32_t codepoint = item->m_Codepoint;

    uint64_t tstart = dmTime::GetTime();

    item->m_Data = 0;
    item->m_DataSize = 0;

    bool is_whitespace = dmGameSystem::IsWhiteSpace(codepoint);

    //
    dmFont::TTFResource* ttfresource = info->m_TTFResource;
    uint32_t glyph_index = dmFont::CodePointToGlyphIndex(ttfresource, codepoint);
    if (!glyph_index)
    {
        if (is_whitespace)
        {
            return 1; // We deal with white spaces in the next callback
        }
        dmLogError("Codepoint has no glyph index: '%c' 0x%04X", (char)codepoint, codepoint);
        return 0;
    }

    if (info->m_IsSdf)
    {
        item->m_Data = dmFont::GenerateGlyphSdf(ttfresource, glyph_index, info->m_Scale, info->m_Padding, info->m_EdgeValue, &item->m_Glyph);
        item->m_DataSize = 1 + item->m_Glyph.m_Width * item->m_Glyph.m_Height;
    }

    if (info->m_HasShadow && item->m_Data)
    {
        // Strictly, we can render non-blurred shadow, with a single channel
        // so this case is about blurred shadow

// TODO: Blur the blue channel

        // Make a copy
        item->m_Glyph.m_Channels = 3;
        uint32_t w = item->m_Glyph.m_Width;
        uint32_t h = item->m_Glyph.m_Height;
        uint32_t ch = item->m_Glyph.m_Channels;
        item->m_DataSize = w*h*ch + 1;

        uint8_t* mem = (uint8_t*)malloc(item->m_DataSize);
        uint8_t* rgb = mem + 1;


        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                uint8_t value = item->m_Data[1 + y * w + x];
                rgb[y * (w * ch) + (x * ch) + 0] = value;
                rgb[y * (w * ch) + (x * ch) + 1] = 0;
                rgb[y * (w * ch) + (x * ch) + 2] = value;
            }
        }
        mem[0] = item->m_Data[0]; // compression

        free((void*)item->m_Data);
        item->m_Data = mem;
    }

    if (!item->m_Data) // Some glyphs (e.g. ' ') don't have an image, which is ok
    {
        if (!is_whitespace)
            return 0; // Something went wrong

        item->m_DataSize = 0;
        item->m_Glyph.m_Width = 0;
        item->m_Glyph.m_Height = 0;
        item->m_Glyph.m_Ascent = 0;
        item->m_Glyph.m_Descent = 0;
    }

    uint64_t tend = dmTime::GetTime();
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

static void InvokeCallback(JobItem* item)
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

static void DeleteItem(JobItem* item)
{
    if (item->m_LastItem)
    {
        delete item->m_Status;
    }
    delete item;
}

// Called on the main thread
static void JobPostProcessGlyph(void* context, void* data, int result)
{
    (void)context;
    JobItem* item = (JobItem*)data;
    FontInfo* info = item->m_FontInfo;

    DM_MUTEX_SCOPED_LOCK(info->m_Mutex);
    if (!info->m_FontResource)
    {
        DeleteItem(item);
        return;
    }

    uint32_t codepoint = item->m_Codepoint;

    if (!result)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to generate glyph '%c' 0x%04X", codepoint, codepoint);
        SetFailedStatus(item, msg);
        InvokeCallback(item);
        DeleteItem(item);
        return;
    }

    // The font system takes ownership of the image data
    dmResource::Result r = dmGameSystem::ResFontAddGlyph(info->m_FontResource, codepoint, &item->m_Glyph, item->m_Data, item->m_DataSize);

    if (dmResource::RESULT_OK != r)
    {
        char msg[256];
        dmSnPrintf(msg, sizeof(msg), "Failed to add glyph '%c': result: %d", codepoint, r);
        SetFailedStatus(item, msg);
    }

    InvokeCallback(item); // reports either first error, or success
    DeleteItem(item);
}

// ****************************************************************************************************

static void GenerateGlyph(Context* ctx, FontInfo* info, uint32_t codepoint, bool last_item, JobStatus* status, FGlyphCallback cbk, void* cbk_ctx)
{
    JobItem* item = new JobItem;
    item->m_FontInfo = info;
    item->m_Codepoint = codepoint;
    item->m_Callback = cbk;
    item->m_CallbackCtx = cbk_ctx;
    item->m_Status = status;
    item->m_LastItem = last_item;
    dmJobThread::PushJob(ctx->m_Jobs, JobGenerateGlyph, JobPostProcessGlyph, ctx, item);
}

static void GenerateGlyphs(Context* ctx, FontInfo* info, const char* text, FGlyphCallback cbk, void* cbk_ctx)
{
    uint32_t len        = dmUtf8::StrLen(text);

    JobStatus* status      = new JobStatus;
    status->m_TimeGlyphGen = 0;
    status->m_Count        = len;
    status->m_Failures     = 0;
    status->m_Error        = 0;

    const char* cursor = text;
    uint32_t c = 0;
    uint32_t index  = 0;
    while ((c = dmUtf8::NextChar(&cursor)))
    {
        ++index;
        bool last_item = len == index;

        FGlyphCallback last_callback = 0;
        void*          last_callback_ctx = 0;
        if (last_item)
        {
            last_callback = cbk;
            last_callback_ctx = cbk_ctx;
        }
        GenerateGlyph(ctx, info, c, last_item, status, last_callback, last_callback_ctx);
    }
}

static void RemoveGlyphs(FontInfo* info, const char* text)
{
    const char* cursor = text;
    uint32_t c = 0;
    while ((c = dmUtf8::NextChar(&cursor)))
    {
        dmGameSystem::ResFontRemoveGlyph(info->m_FontResource, c);
    }
}

bool Initialize(dmExtension::Params* params)
{
    g_FontExtContext = new Context;
    g_FontExtContext->m_ResourceFactory = params->m_ResourceFactory;
    g_FontExtContext->m_Mutex = dmMutex::New();

    // 3 is arbitrary but resembles the output from out generator
    g_FontExtContext->m_DefaultSdfPadding = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.sdf_base_padding", 3);
    g_FontExtContext->m_DefaultSdfEdge = dmConfigFile::GetInt(params->m_ConfigFile, "fontgen.sdf_edge_value", 190);

    g_FontExtContext->m_Jobs = dmExtension::GetContextAsType<dmJobThread::HContext>(params, "job_thread");
    return true;
}

void Finalize(dmExtension::Params* params)
{
    Context* ctx = g_FontExtContext;

    ctx->m_FontInfos.Iterate(DeleteFontInfoIter, ctx);
    ctx->m_FontInfos.Clear();

    ctx->m_DeletedFontInfos.Iterate(DeleteFontInfoIter, ctx);
    ctx->m_DeletedFontInfos.Clear();

    dmMutex::Delete(ctx->m_Mutex);

    delete ctx;
    ctx = 0;
}


void Update(dmExtension::Params* params)
{
    g_FontExtContext->m_DeletedFontInfos.Iterate(DeleteFontInfoIter, g_FontExtContext);
    g_FontExtContext->m_DeletedFontInfos.Clear();
}

// Scripting

bool LoadFont(const char* fontc_path, const char* ttf_path)
{
    Context* ctx = g_FontExtContext;
    FontInfo** pinfo = ctx->m_FontInfos.Get(dmHashString64(fontc_path));
    if (pinfo)
    {
        dmLogError("Font already loaded %s / %s", fontc_path, ttf_path);
        return false; // Already loaded
    }

    FontInfo* info = LoadFont(ctx, fontc_path, ttf_path);
    return info != 0;
}

bool UnloadFont(dmhash_t fontc_path_hash)
{
    Context* ctx = g_FontExtContext;
    return UnloadFont(ctx, fontc_path_hash);
}


bool AddGlyphs(dmhash_t fontc_path_hash, const char* text, FGlyphCallback cbk, void* cbk_ctx)
{
    Context* ctx = g_FontExtContext;
    FontInfo** pinfo = ctx->m_FontInfos.Get(fontc_path_hash);
    if (!pinfo)
    {
        dmLogError("Font not loaded %s", dmHashReverseSafe64(fontc_path_hash));
        return false;
    }

    GenerateGlyphs(ctx, *pinfo, text, cbk, cbk_ctx);

    //dmGameSystem::ResFontDebugPrint((*pinfo)->m_FontResource);
    return true;
}


bool RemoveGlyphs(dmhash_t fontc_path_hash, const char* text)
{
    Context* ctx = g_FontExtContext;
    FontInfo** pinfo = ctx->m_FontInfos.Get(fontc_path_hash);
    if (!pinfo)
    {
        dmLogError("Font not loaded %s", dmHashReverseSafe64(fontc_path_hash));
        return false;
    }

    RemoveGlyphs(*pinfo, text);

    //dmGameSystem::ResFontDebugPrint((*pinfo)->m_FontResource);
    return true;
}



} // namespace
