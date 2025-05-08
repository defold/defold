// Copyright 2021 The Defold Foundation
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

#include "res_ttf.h"
#include "util.h" // DebugPrintBitmap
#include <dmsdk/dlib/log.h>
#include <dmsdk/resource/resource.h>

#include <stdlib.h> // free
#include <stdio.h> // printf

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace dmFontGen
{

struct TTFResource
{
    stbtt_fontinfo  m_Font;
    const char*     m_Path;
    void*           m_Data; // The raw ttf font

    int             m_Ascent;
    int             m_Descent;
    int             m_LineGap;
};

static void DeleteResource(TTFResource* resource)
{
    free((void*)resource->m_Data);
    free((void*)resource->m_Path);
    delete resource;
}

static TTFResource* CreateFont(const char* path, const void* buffer, uint32_t buffer_size)
{
    TTFResource* resource = new TTFResource;
    memset(resource, 0, sizeof(*resource));

    // Until we can rely on memory being uncompressed and memory mapped, we need to make a single copy here
    resource->m_Data = malloc(buffer_size);
    memcpy(resource->m_Data, buffer, buffer_size);

    int index = stbtt_GetFontOffsetForIndex((const unsigned char*)resource->m_Data,0);
    int result = stbtt_InitFont(&resource->m_Font, (const unsigned char*)resource->m_Data, index);
    if (!result)
    {
        dmLogError("Failed to load font from '%s'", path);
        DeleteResource(resource);
        return 0;
    }

    stbtt_GetFontVMetrics(&resource->m_Font, &resource->m_Ascent, &resource->m_Descent, &resource->m_LineGap);
    resource->m_Path = strdup(path);

    return resource;
}

static dmResource::Result TTF_Create(const dmResource::ResourceCreateParams* params)
{
    TTFResource* resource = CreateFont(params->m_Filename, params->m_Buffer, params->m_BufferSize);
    if (!resource)
        return dmResource::RESULT_INVALID_DATA;

    dmResource::SetResource(params->m_Resource, resource);
    dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize + sizeof(*resource));

    return dmResource::RESULT_OK;
}

static dmResource::Result TTF_Destroy(const dmResource::ResourceDestroyParams* params)
{
    TTFResource* resource = (TTFResource*)dmResource::GetResource(params->m_Resource);
    DeleteResource(resource);
    return dmResource::RESULT_OK;
}

static dmResource::Result TTF_Recreate(const dmResource::ResourceRecreateParams* params)
{
    TTFResource* new_resource = CreateFont(params->m_Filename, params->m_Buffer, params->m_BufferSize);
    if (!new_resource)
        return dmResource::RESULT_INVALID_DATA;

    // We need to keep the original pointer as it may be used elsewhere
    TTFResource* old_resource = (TTFResource*)dmResource::GetResource(params->m_Resource);

    // So, we need to swap items

    // There is no desctructor for this structure
    memcpy(&old_resource->m_Font, &new_resource->m_Font, sizeof(old_resource->m_Font));
    void* old_data = old_resource->m_Data;
    old_resource->m_Data = new_resource->m_Data;
    new_resource->m_Data = old_data;
    const char* old_path = old_resource->m_Path;
    old_resource->m_Path = new_resource->m_Path;
    new_resource->m_Path = old_path;

    DeleteResource(new_resource);

    dmResource::SetResource(params->m_Resource, old_resource);
    dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize + sizeof(*old_resource));

    return dmResource::RESULT_OK;
}

static ResourceResult RegisterResourceType_TTFFont(HResourceTypeContext ctx, HResourceType type)
{
    return (ResourceResult)dmResource::SetupType(ctx,
                                                 type,
                                                 0, // context
                                                 0, // preload
                                                 TTF_Create,
                                                 0, // post create
                                                 TTF_Destroy,
                                                 TTF_Recreate);

}

static ResourceResult DeregisterResourceType_TTFFont(HResourceTypeContext ctx, HResourceType type)
{
    //void* context = (void*)ResourceTypeGetContext(type);
    return RESOURCE_RESULT_OK;
}

const char* GetFontPath(TTFResource* resource)
{
    return resource->m_Path;
}

int CodePointToGlyphIndex(TTFResource* resource, int codepoint)
{
    return stbtt_FindGlyphIndex(&resource->m_Font, codepoint);
}

float SizeToScale(TTFResource* resource, int size)
{
    return stbtt_ScaleForPixelHeight(&resource->m_Font, size);
}

float GetAscent(TTFResource* resource, float scale)
{
    return resource->m_Ascent * scale;
}

float GetDescent(TTFResource* resource, float scale)
{
    return resource->m_Descent * scale;
}

void GetCellSize(TTFResource* resource, uint32_t* width, uint32_t* height, uint32_t* max_ascent)
{
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&resource->m_Font, &x0, &y0, &x1, &y1);

    *width = x1 - x0;
    *height = y1 - y0;
    *max_ascent = resource->m_Ascent;
}

uint8_t* GenerateGlyphSdf(TTFResource* ttfresource, uint32_t glyph_index,
                            float scale, int padding, int edge,
                            dmGameSystem::FontGlyph* out)
{

    float pixel_dist_scale = (float)edge/(float)padding;

    int advx, lsb;
    stbtt_GetGlyphHMetrics(&ttfresource->m_Font, glyph_index, &advx, &lsb);

    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&ttfresource->m_Font, glyph_index, scale, scale, &x0, &y0, &x1, &y1);

    int ascent = 0;
    int descent = 0;
    int srcw = 0;
    int srch = 0;
    int offsetx, offsety;
    uint8_t* src = stbtt_GetGlyphSDF(&ttfresource->m_Font, scale, glyph_index, padding, edge, pixel_dist_scale,
                                        &srcw, &srch, &offsetx, &offsety);

    uint8_t* mem = 0;
    if (src)
    {
        uint32_t memsize = srcw*srch + 1;
        mem = (uint8_t*)malloc(memsize);
        memset(mem, 0, memsize);
        uint8_t* bm = mem + 1;

        memcpy(bm, src, srcw*srch);

        stbtt_FreeSDF(src, 0);

        ascent = -offsety;
        descent = srch - ascent;
    }

    out->m_Width = srcw;
    out->m_Height = srch;
    out->m_Channels = 1;
    out->m_Advance = advx*scale;;
    out->m_LeftBearing = lsb*scale;
    out->m_Ascent = ascent;
    out->m_Descent = descent;

    // int gi = dmFontGen::CodePointToGlyphIndex(ttfresource, 'T');
    // int debug = glyph_index == gi;
    // if (debug)
    //     DebugPrintBitmap((mem+1), srcw, srch);

    return mem;
}

} // namespace


DM_DECLARE_RESOURCE_TYPE(ResourceTypeTTFFont, "ttf", dmFontGen::RegisterResourceType_TTFFont, dmFontGen::DeregisterResourceType_TTFFont);
