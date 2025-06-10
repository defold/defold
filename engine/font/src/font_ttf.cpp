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

#include <dmsdk/dlib/log.h>

#include <stdlib.h> // free

// Making sure we can guarantuee the functions
#define STBTT_malloc(x,u)  ((void)(u),malloc(x))
#define STBTT_free(x,u)    ((void)(u),free(x))

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font_private.h"

namespace dmFont
{

struct TTFFont
{
    dmFont::Font    m_Base;

    stbtt_fontinfo  m_Font;
    const char*     m_Path;
    const void*     m_Data;
    uint32_t        m_DataSize;

    int             m_Ascent;
    int             m_Descent;
    int             m_LineGap;
    uint32_t        m_Allocated:1;
};

static inline TTFFont* ToFont(HFont hfont)
{
    return (TTFFont*)hfont;
}

void DestroyFontTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    if (font->m_Allocated)
    {
        free((void*)font->m_Data);
    }
    free((void*)font);
}

static HFont LoadTTFInternal(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    TTFFont* font = new TTFFont;
    memset(font, 0, sizeof(*font));

    if (allocate)
    {
        font->m_Data = (const void*)malloc(buffer_size);
        memcpy((void*)font->m_Data, buffer, buffer_size);
        font->m_Allocated = 1;
    }
    else
    {
        font->m_Data    = buffer;
        font->m_DataSize= buffer_size;
    }

    int index = stbtt_GetFontOffsetForIndex((const unsigned char*)font->m_Data,0);
    int result = stbtt_InitFont(&font->m_Font, (const unsigned char*)font->m_Data, index);
    if (!result)
    {
        dmLogError("Failed to load font from '%s'", path);
        DestroyFontTTF((HFont)font);
        delete font;
        return 0;
    }

    stbtt_GetFontVMetrics(&font->m_Font, &font->m_Ascent, &font->m_Descent, &font->m_LineGap);
    font->m_Path = strdup(path);
    return (HFont)font;
}

HFont LoadFontFromMemoryTTF(const char* path, const void* buffer, uint32_t buffer_size, bool allocate)
{
    return LoadTTFInternal(path, buffer, buffer_size, allocate);
}


uint32_t GetResourceSizeTTF(HFont hfont)
{
    TTFFont* font = ToFont(hfont);
    return font->m_DataSize;
}

float GetPixelScaleFromSizeTTF(HFont hfont, uint32_t size)
{
    TTFFont* font = ToFont(hfont);
    return stbtt_ScaleForPixelHeight(&font->m_Font, (int)size);
}

float GetAscentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Ascent * scale;
}

float GetDescentTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_Descent * scale;
}

float GetLineGapTTF(HFont hfont, float scale)
{
    TTFFont* font = ToFont(hfont);
    return font->m_LineGap * scale;
}

FontResult FreeGlyphTTF(HFont hfont, Glyph* glyph)
{
    (void)hfont;
    stbtt_FreeSDF(glyph->m_Bitmap.m_Data, 0);
    return RESULT_OK;
}

FontResult GetGlyphTTF(HFont hfont, uint32_t codepoint, const GlyphOptions* options, Glyph* glyph)
{
    TTFFont* font = ToFont(hfont);

    memset(glyph, 0, sizeof(*glyph));
    glyph->m_Codepoint = codepoint;

    stbtt_fontinfo* info = &font->m_Font;
    int glyph_index = stbtt_FindGlyphIndex(info, (int)codepoint);
    if (!glyph_index)
    {
        return RESULT_NOT_SUPPORTED;
    }

    int advx, lsb;
    stbtt_GetGlyphHMetrics(info, glyph_index, &advx, &lsb);

    int x0, y0, x1, y1;
    stbtt_GetGlyphBox(info, glyph_index, &x0, &y0, &x1, &y1);

    float scale = options->m_Scale;
    int padding = options->m_StbttSDFPadding;
    int on_edge_value = options->m_StbttSDFOnEdgeValue;

    int ascent = 0;
    int descent = 0;
    int srcw = 0;
    int srch = 0;
    int offsetx = 0;
    int offsety = 0;

    if (options->m_GenerateImage)
    {
        float pixel_dist_scale = (float)on_edge_value/(float)padding;

        glyph->m_Bitmap.m_Data = stbtt_GetGlyphSDF(info, scale, glyph_index, padding, on_edge_value, pixel_dist_scale,
                                                   &srcw, &srch, &offsetx, &offsety);

        if (glyph->m_Bitmap.m_Data)
        {
            glyph->m_Bitmap.m_Flags = dmFont::GLYPH_BM_FLAG_COMPRESSION_NONE;
            glyph->m_Bitmap.m_Width = srcw;
            glyph->m_Bitmap.m_Height = srch;
            glyph->m_Bitmap.m_Channels = 1;

            // We don't call stbtt_FreeSDF(src, 0);
            // But instead let the user call FreeGlyphTTF()

            ascent = -offsety;
            descent = srch - ascent;
        }
    }

    // The dimensions of the visible area
    if (x0 != x1 && y0 != y1)
    {
        // Only modify non empty glyphs (from stbtt_GetGlyphSDF())
        x0 -= padding;
        y0 -= padding;
        x1 += padding;
        y1 += padding;
    }


    glyph->m_Width = (x1 - x0) * scale;
    glyph->m_Height = (y1 - y0) * scale;
    glyph->m_Advance = advx*scale;;
    glyph->m_LeftBearing = lsb*scale;
    glyph->m_Ascent = ascent;
    glyph->m_Descent = descent;

    return RESULT_OK;
}

} // namespace


// namespace dmGameSystem
// {

// struct TTFResource
// {
//     stbtt_fontinfo  m_Font;
//     const char*     m_Path;
//     void*           m_Data; // The raw ttf font

//     int             m_Ascent;
//     int             m_Descent;
//     int             m_LineGap;
// };

// static void DeleteResource(TTFResource* resource)
// {
//     free((void*)resource->m_Data);
//     free((void*)resource->m_Path);
//     delete resource;
// }

// static TTFResource* CreateFont(const char* path, const void* buffer, uint32_t buffer_size)
// {
//     TTFResource* resource = new TTFResource;
//     memset(resource, 0, sizeof(*resource));

//     // Until we can rely on memory being uncompressed and memory mapped, we need to make a single copy here
//     resource->m_Data = malloc(buffer_size);
//     memcpy(resource->m_Data, buffer, buffer_size);

//     int index = stbtt_GetFontOffsetForIndex((const unsigned char*)resource->m_Data,0);
//     int result = stbtt_InitFont(&resource->m_Font, (const unsigned char*)resource->m_Data, index);
//     if (!result)
//     {
//         dmLogError("Failed to load font from '%s'", path);
//         DeleteResource(resource);
//         return 0;
//     }

//     stbtt_GetFontVMetrics(&resource->m_Font, &resource->m_Ascent, &resource->m_Descent, &resource->m_LineGap);
//     resource->m_Path = strdup(path);

//     return resource;
// }

// static dmResource::Result TTF_Create(const dmResource::ResourceCreateParams* params)
// {
//     TTFResource* resource = CreateFont(params->m_Filename, params->m_Buffer, params->m_BufferSize);
//     if (!resource)
//         return dmResource::RESULT_INVALID_DATA;

//     dmResource::SetResource(params->m_Resource, resource);
//     dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize + sizeof(*resource));

//     return dmResource::RESULT_OK;
// }

// static dmResource::Result TTF_Destroy(const dmResource::ResourceDestroyParams* params)
// {
//     TTFResource* resource = (TTFResource*)dmResource::GetResource(params->m_Resource);
//     DeleteResource(resource);
//     return dmResource::RESULT_OK;
// }

// static dmResource::Result TTF_Recreate(const dmResource::ResourceRecreateParams* params)
// {
//     TTFResource* new_resource = CreateFont(params->m_Filename, params->m_Buffer, params->m_BufferSize);
//     if (!new_resource)
//         return dmResource::RESULT_INVALID_DATA;

//     // We need to keep the original pointer as it may be used elsewhere
//     TTFResource* old_resource = (TTFResource*)dmResource::GetResource(params->m_Resource);

//     // So, we need to swap items

//     // There is no desctructor for this structure
//     memcpy(&old_resource->m_Font, &new_resource->m_Font, sizeof(old_resource->m_Font));
//     void* old_data = old_resource->m_Data;
//     old_resource->m_Data = new_resource->m_Data;
//     new_resource->m_Data = old_data;
//     const char* old_path = old_resource->m_Path;
//     old_resource->m_Path = new_resource->m_Path;
//     new_resource->m_Path = old_path;

//     DeleteResource(new_resource);

//     dmResource::SetResource(params->m_Resource, old_resource);
//     dmResource::SetResourceSize(params->m_Resource, params->m_BufferSize + sizeof(*old_resource));

//     return dmResource::RESULT_OK;
// }

// static ResourceResult RegisterResourceType_TTFFont(HResourceTypeContext ctx, HResourceType type)
// {
//     return (ResourceResult)dmResource::SetupType(ctx,
//                                                  type,
//                                                  0, // context
//                                                  0, // preload
//                                                  TTF_Create,
//                                                  0, // post create
//                                                  TTF_Destroy,
//                                                  TTF_Recreate);

// }

// static ResourceResult DeregisterResourceType_TTFFont(HResourceTypeContext ctx, HResourceType type)
// {
//     //void* context = (void*)ResourceTypeGetContext(type);
//     return RESOURCE_RESULT_OK;
// }

// const char* GetFontPath(TTFResource* resource)
// {
//     return resource->m_Path;
// }

// int CodePointToGlyphIndex(TTFResource* resource, int codepoint)
// {
//     return stbtt_FindGlyphIndex(&resource->m_Font, codepoint);
// }

// float SizeToScale(TTFResource* resource, int size)
// {
//     return stbtt_ScaleForPixelHeight(&resource->m_Font, size);
// }

// float GetAscent(TTFResource* resource, float scale)
// {
//     return resource->m_Ascent * scale;
// }

// float GetDescent(TTFResource* resource, float scale)
// {
//     return resource->m_Descent * scale;
// }

// // UNUSED
// void GetCellSize(TTFResource* resource, uint32_t* width, uint32_t* height, uint32_t* max_ascent)
// {
//     int x0, y0, x1, y1;
//     stbtt_GetFontBoundingBox(&resource->m_Font, &x0, &y0, &x1, &y1);

//     *width = x1 - x0;
//     *height = y1 - y0;
//     *max_ascent = resource->m_Ascent;
// }

// uint8_t* GenerateGlyphSdf(TTFResource* ttfresource, uint32_t glyph_index,
//                             float scale, int padding, int edge,
//                             dmGameSystem::FontGlyph* out)
// {

//     float pixel_dist_scale = (float)edge/(float)padding;

//     int advx, lsb;
//     stbtt_GetGlyphHMetrics(&ttfresource->m_Font, glyph_index, &advx, &lsb);

//     int x0, y0, x1, y1;
//     stbtt_GetGlyphBox(&ttfresource->m_Font, glyph_index, &x0, &y0, &x1, &y1);

//     int ascent = 0;
//     int descent = 0;
//     int srcw = 0;
//     int srch = 0;
//     int offsetx = 0, offsety = 0;
//     uint8_t* src = stbtt_GetGlyphSDF(&ttfresource->m_Font, scale, glyph_index, padding, edge, pixel_dist_scale,
//                                         &srcw, &srch, &offsetx, &offsety);

//     uint8_t* mem = 0;
//     if (src)
//     {
//         uint32_t memsize = srcw*srch + 1;
//         mem = (uint8_t*)malloc(memsize);
//         memset(mem, 0, memsize);
//         uint8_t* bm = mem + 1;

//         memcpy(bm, src, srcw*srch);

//         stbtt_FreeSDF(src, 0);

//         ascent = -offsety;
//         descent = srch - ascent;
//     }

//     // The dimensions of the visible area
//     if (x0 != x1 && y0 != y1)
//     {
//         // Only modify non empty glyphs (from stbtt_GetGlyphSDF())
//         x0 -= padding;
//         y0 -= padding;
//         x1 += padding;
//         y1 += padding;
//     }

//     out->m_Width = (x1 - x0) * scale;
//     out->m_Height = (y1 - y0) * scale;
//     out->m_ImageWidth = srcw;
//     out->m_ImageHeight = srch;
//     out->m_Channels = 1;
//     out->m_Advance = advx*scale;;
//     out->m_LeftBearing = lsb*scale;
//     out->m_Ascent = ascent;
//     out->m_Descent = descent;

//     // printf("glyph: %d  w/h: %f, %f adv: %.2f  lsb: %.2f  asc/dsc: %.2f, %.2f img w/h: %u, %d\n", glyph_index,
//     //         out->m_Width, out->m_Height,
//     //         out->m_Advance, out->m_LeftBearing,
//     //         out->m_Ascent, out->m_Descent,
//     //         out->m_ImageWidth, out->m_ImageHeight);


//     //printf("  box: p0: %f, %f p1: %f, %f\n", x0 * scale, y0 * scale, x1 * scale, y1 * scale);
//     //printf("  offset: %d, %d \n", offsetx, offsety);

//     // int gi_T = dmFontGen::CodePointToGlyphIndex(ttfresource, 'T');
//     // int gi_h = dmFontGen::CodePointToGlyphIndex(ttfresource, 'h');
//     // // int debug = glyph_index == 77 || glyph_index == 75;
//     // int debug = glyph_index == gi_T || glyph_index == gi_h;
//     // if (debug)
//     //     DebugPrintBitmap((mem+1), srcw, srch);

//     return mem;
// }

// } // namespace


// DM_DECLARE_RESOURCE_TYPE(ResourceTypeTTF, "ttf", dmGameSystem::RegisterResourceType_TTFFont, dmGameSystem::DeregisterResourceType_TTFFont);
