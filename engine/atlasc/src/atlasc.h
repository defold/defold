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
        int x, y;
    };

    struct Vec2f
    {
        float x, y;
    };
    struct Rect
    {
        Vec2i m_Pos;
        Vec2i m_Size;
    };

    // Input format
    struct SourceImage
    {
        const uint8_t*  m_Data; // The texels
        const char*     m_Path; // The source path
        Vec2i           m_Size;
        int             m_NumChannels;
    };

    // Output format
    struct PackedImage
    {
        Vec2i m_Pos;
        Vec2i m_Size;
    };

    // Output format
    struct AtlasPage
    {
        int m_Index;

        dmArray<PackedImage*> m_Images;
    };

    struct Atlas
    {
        dmArray<AtlasPage*> m_Pages;

        // uint32_t    m_NumIndices;
        // uint32_t*   m_Indices;
    };

    struct Options
    {
        PackingAlgorithm    m_Algorithm;  // default: PA_TILEPACK_AUTO
        int                 m_PageSize;   // The max size in texels. default: 0 means all images are stored in the same atlas

        // general packer options
        bool    m_PackerNoRotate;

        // tile packer options
        int     m_TilePackerTileSize;   // The size in texels. Default 16
        int     m_TilePackerPadding;    // Internal padding for each image. Default 1
        int     m_TIlePackerAlphaThreshold;       // Values below or equal to this threshold are considered transparent. (range 0-255)

        // bin packer options (currently none)

        Options();
    };

    Atlas*  CreateAtlas(const Options& options, SourceImage* source_images, uint32_t num_source_images);
    void    DestroyAtlas(Atlas* atlas);

    const char* GetLastError();

}

#endif // DM_ATLASC_H
