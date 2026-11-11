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

#include "font_effect.h"
#include "font_effect_index.h"
#include "font_render.h"

#include <assert.h>
#include <string.h>
#include <dlib/hash.h>
#include <dlib/array.h>
#include <dlib/math.h>
#include <dlib/log.h>
#include <graphics/graphics.h>

struct FontEffectCache
{
    dmGraphics::HContext      m_Context;
    dmGraphics::HTexture      m_Texture;
    dmArray<FontEffectRecord> m_Records;
    FontEffectIndex           m_Index;
    dmArray<uint32_t>         m_Frames;
    uint32_t                  m_MaxHeight;
    uint32_t                  m_Count;
    uint32_t                  m_LastIndex;
    uint32_t                  m_EvictFrame;
    uint32_t                  m_EvictCursor;
    uint8_t                   m_OverflowReported : 1;
    uint8_t                   m_Dirty : 1;
};

static void ResizeEffectCache(FontEffectCache* cache, uint32_t height)
{
    const uint32_t previous = cache->m_Records.Size();
    cache->m_Records.EnsureSize(height);
    FontResizeEffectIndex(&cache->m_Index, height);
    cache->m_Frames.EnsureSize(height);
    memset(cache->m_Records.Begin() + previous, 0, (height - previous) * sizeof(FontEffectRecord));
    FontSetEffectTextureHeight(&cache->m_Records[0], height);
    cache->m_Dirty = true;
}

FontEffectCache* FontCreateEffectCache(dmGraphics::HContext context)
{
    FontEffectCache* cache = new FontEffectCache;
    cache->m_Context = context;
    cache->m_Count = 1;
    cache->m_LastIndex = 0;
    cache->m_EvictFrame = 0;
    cache->m_EvictCursor = 1;
    cache->m_MaxHeight = dmMath::Min(dmGraphics::GetMaxTextureSize(context), FONT_EFFECT_MAX_TEXTURE_HEIGHT);
    cache->m_OverflowReported = false;
    ResizeEffectCache(cache, dmMath::Min(FONT_EFFECT_TEXTURE_HEIGHT, cache->m_MaxHeight));
    dmGraphics::TextureCreationParams creation;
    creation.m_Type = dmGraphics::TEXTURE_TYPE_2D;
    creation.m_Width = creation.m_OriginalWidth = FONT_EFFECT_TEXTURE_WIDTH;
    creation.m_Height = creation.m_OriginalHeight = cache->m_Records.Size();
    creation.m_MipMapCount = 1;
    cache->m_Texture = dmGraphics::NewTexture(context, creation);
    cache->m_Dirty = true;
    return cache;
}

void FontDeleteEffectCache(FontEffectCache* cache)
{
    if (cache)
    {
        dmGraphics::DeleteTexture(cache->m_Context, cache->m_Texture);
        delete cache;
    }
}

uint32_t FontCacheEffect(FontEffectCache* cache, const FontEffectRecord& record, uint32_t frame)
{
    // Text normally emits many adjacent glyphs with the same effect.
    if (memcmp(&record, &cache->m_Records[cache->m_LastIndex], sizeof(record)) == 0)
    {
        cache->m_Frames[cache->m_LastIndex] = frame;
        return cache->m_LastIndex;
    }
    const uint64_t hash = dmHashBuffer64(&record, sizeof(record));
    const uint32_t found = FontFindEffect(cache->m_Index, hash, record, cache->m_Records.Begin());
    if (found)
    {
        cache->m_Frames[found] = frame;
        cache->m_LastIndex = found;
        return found;
    }
    uint32_t available = UINT32_MAX;
    if (cache->m_Count == cache->m_Records.Size())
    {
        // Scan each pinned row at most once per frame, even when many new
        // effects arrive after the cache fills up.
        if (cache->m_EvictFrame != frame)
        {
            cache->m_EvictFrame = frame;
            cache->m_EvictCursor = 1;
        }
        while (cache->m_EvictCursor < cache->m_Count && cache->m_Frames[cache->m_EvictCursor] == frame)
        {
            ++cache->m_EvictCursor;
        }
        if (cache->m_EvictCursor < cache->m_Count)
        {
            available = cache->m_EvictCursor++;
            FontEraseEffect(&cache->m_Index, dmHashBuffer64(&cache->m_Records[available], sizeof(record)), available);
        }
        else if (cache->m_Count < cache->m_MaxHeight)
        {
            ResizeEffectCache(cache, dmMath::Min(cache->m_Count * 2, cache->m_MaxHeight));
        }
    }
    if (cache->m_Count < cache->m_Records.Size())
    {
        available = cache->m_Count++;
    }
    if (available == UINT32_MAX)
    {
        if (!cache->m_OverflowReported)
        {
            dmLogError("Font gradient texture exhausted its %u records; further effects use approximate vertex colors.", cache->m_MaxHeight - 1);
        }
        cache->m_OverflowReported = true;
        return UINT32_MAX;
    }
    cache->m_Records[available] = record;
    FontInsertEffect(&cache->m_Index, hash, available);
    cache->m_Frames[available] = frame;
    cache->m_LastIndex = available;
    cache->m_Dirty = true;
    return available;
}

void FontUploadEffects(FontEffectCache* cache)
{
    if (!cache->m_Dirty)
    {
        return;
    }
    dmGraphics::TextureParams params;
    params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
    params.m_Width = FONT_EFFECT_TEXTURE_WIDTH;
    params.m_Height = cache->m_Records.Size();
    params.m_Data = cache->m_Records.Begin();
    params.m_DataSize = cache->m_Records.Size() * sizeof(FontEffectRecord);
    params.m_MinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
    params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
    params.m_UWrap = dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE;
    params.m_VWrap = dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE;
    dmGraphics::SetTexture(cache->m_Context, cache->m_Texture, params);
    cache->m_Dirty = false;
}

dmGraphics::HTexture FontGetEffectTexture(FontEffectCache* cache)
{
    return cache->m_Texture;
}

uint32_t FontGetEffectCacheResourceSize(FontEffectCache* cache)
{
    const uint32_t index_capacity = cache->m_Index.m_Rows.Capacity();
    return sizeof(*cache) + cache->m_Records.Capacity() * sizeof(FontEffectRecord) +
           dmHashTable64<uint32_t>::GetMemorySize(index_capacity, index_capacity) +
           cache->m_Index.m_Next.Capacity() * sizeof(uint32_t) + cache->m_Frames.Capacity() * sizeof(uint32_t) +
           dmGraphics::GetTextureResourceSize(cache->m_Context, cache->m_Texture);
}
