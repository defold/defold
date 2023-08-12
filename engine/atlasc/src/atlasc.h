// Copyright 2020-2023 The Defold Foundation
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

#ifndef DM_ATLASC_H
#define DM_ATLASC_H

// #include <dmsdk/dlib/align.h>
// #include <dmsdk/dlib/shared_library.h>

#include <dmsdk/dlib/array.h>

#include <stdint.h>

/**
 * Texture processing
 */
namespace dmAtlasc
{
    // Matches the enums in graphics_ddf.proto

    enum PackingAlgorithm
    {
        PA_TILEPACK_AUTO,       // The default
        PA_TILEPACK_TILE,
        PA_TILEPACK_CONVEXHULL,
        PA_BINPACK_SKYLINE_BL,
    };

    struct Vec2i
    {
        Vec2i() : x(0), y(0) {}
        Vec2i(int _x, int _y) : x(_x), y(_y) {}
        int x, y;
    };

    struct Sizei
    {
        Sizei() : width(0), height(0) {}
        Sizei(int _width, int _height) : width(_width), height(_height) {}
        int width, height;
    };

    struct Vec2f
    {
        Vec2f() : x(0.0f), y(0.0f) {}
        Vec2f(float _x, float _y) : x(_x), y(_y) {}
        float x, y;
    };
    struct Rect
    {
        Vec2i m_Pos;
        Sizei m_Size;
    };

    // Input format
    struct SourceImage
    {
        SourceImage();
        ~SourceImage();

        const char*     m_Path; // The source path
        const uint8_t*  m_Data; // The texels
        uint32_t        m_DataCount;
        Sizei           m_Size;
        int             m_NumChannels;
    };

    // Output format
    struct PackedImage
    {
        PackedImage();
        ~PackedImage();

        dmArray<Vec2f>  m_Vertices;  // If empty, no hull was generated
        Rect            m_Placement; // The covered area in the texture
        int             m_Rotation;  // Degrees CCW: 0, 90, 180, 270

        int             m_Width;
        int             m_Height;
        int             m_NumChannels;

        const char*     m_Path;
        const uint8_t*  m_Data; // The texels
        uint32_t        m_DataCount;
    };

    // Output format
    struct AtlasPage
    {
        AtlasPage();
        ~AtlasPage();
        Sizei   m_Dimensions;
        int     m_NumChannels;
        int     m_Index;

        dmArray<PackedImage*> m_Images;
    };

    struct Atlas
    {
        Atlas();
        ~Atlas();

        dmArray<AtlasPage*> m_Pages;
    };

    struct RenderedPage
    {
        RenderedPage();
        ~RenderedPage();

        uint8_t* m_Data;
        uint32_t m_DataCount;
        Sizei    m_Dimensions;
        int      m_NumChannels;
    };

    struct Options
    {
        PackingAlgorithm    m_Algorithm;  // default: PA_TILEPACK_AUTO
        int                 m_PageSize;   // The max size in texels. default: 0 means all images are stored in the same atlas

        // general packer options
        bool    m_PackerNoRotate;

        // tile packer options
        int     m_TilePackerTileSize;       // The size in texels. Default 16
        int     m_TilePackerPadding;        // Internal padding for each image. Default 1
        int     m_TIlePackerAlphaThreshold; // Values below or equal to this threshold are considered transparent. (range 0-255)

        // bin packer options (currently none)

        Options();
    };

    enum Result
    {
        RESULT_OK       = 0,
        RESULT_WARNING  = 1,
        RESULT_INVAL    = -1,
    };

    typedef void (*FOnError)(void* ctx, Result result, const char* error_string);

    Atlas*  CreateAtlas(const Options& options, SourceImage* source_images, uint32_t num_source_images, FOnError error_cbk, void* error_cbk_ctx);
    void    DestroyAtlas(Atlas* atlas);

    void    RenderPage(const AtlasPage* ap_page, RenderedPage* output);
    int     SavePage(const char* path, const AtlasPage* ap_page);
}

#endif // DM_ATLASC_H
