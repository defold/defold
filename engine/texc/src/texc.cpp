// Copyright 2020-2024 The Defold Foundation
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

#include "texc.h"
#include "texc_private.h"
#include "texc_enc_basis.h"
#include "texc_enc_default.h"

#include <assert.h>

#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/dstrings.h>

namespace dmTexc
{
    static bool GetEncoder(CompressionType compression_type, Encoder* encoder)
    {
        switch(compression_type)
        {
            case dmTexc::CT_BASIS_UASTC:
            case dmTexc::CT_BASIS_ETC1S:    GetEncoderBasis(encoder); return true;
            case dmTexc::CT_DEFAULT:        GetEncoderDefault(encoder); return true;
            default:
                return false;
        }
    }

    Texture* CreateTexture(const char* name, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, CompressionType compression_type, void* data)
    {
        TextureImpl* t = new TextureImpl;
        if (!GetEncoder(compression_type, &t->m_Encoder))
        {
            dmLogError("Failed to get an encoder for compression type: %d", (int)compression_type);
            delete t;
            return 0;

        }

        t->m_CompressionType = compression_type;
        if (!t->m_Encoder.m_FnCreate(t, width, height, pixel_format, color_space, compression_type, data))
        {
            delete t;
            dmLogError("Create failed");
            return 0;
        }
        t->m_Name = name != 0 ? strdup(name) : 0;

        if (t->m_Name == 0)
        {
            static int counter = 0;
            char buffer[65];
            dmSnPrintf(buffer, sizeof(buffer), "image%d_%dx%d", counter++, width, height);
            t->m_Name = strdup(buffer);
        }

        Texture* out = new Texture;
        out->m_Impl = t;
        return out;
    }

    void DestroyTexture(Texture* texture)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        free((void*)t->m_Name);
        t->m_Encoder.m_FnDestroy(t);
        delete t;
        delete texture;
    }

    // For testing
    bool GetHeader(Texture* texture, Header* out_header)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnGetHeader(t, out_header);
    }

    uint32_t GetDataSizeCompressed(Texture* texture, uint32_t mip_map)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return mip_map < t->m_Mips.Size() ? t->m_Mips[mip_map].m_DataCount : 0;
    }

    uint32_t GetDataSizeUncompressed(Texture* texture, uint32_t mip_map)
    {
        // Since we don't add any extra compression on top of the resource, the
        // amount of memory required to store the
        return GetDataSizeCompressed(texture, mip_map);
    }

    uint32_t GetTotalDataSize(Texture* texture)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnGetTotalDataSize(t);
    }

    uint32_t GetData(Texture* texture, void* out_data, uint32_t out_data_size)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnGetData(t, out_data, out_data_size);
    }

    uint64_t GetCompressionFlags(Texture* texture)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_CompressionFlags;
    }

    bool Resize(Texture* texture, uint32_t width, uint32_t height)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnResize(t, width, height);
    }

    bool PreMultiplyAlpha(Texture* texture)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnPreMultiplyAlpha(t);
    }

    bool GenMipMaps(Texture* texture)
    {
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnGenMipMaps(t);
    }

    bool Flip(Texture* texture, FlipAxis flip_axis)
    {
        if (flip_axis == FLIP_AXIS_Z)
        {
            dmLogWarning("FLIP_AXIS_Z not supported");
            return true;
        }
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnFlip(t, flip_axis);
    }

    // bool Dither(HTexture texture, PixelFormat pixel_format)
    // {
    //     Texture* t = (Texture*) texture;

    //     if (pixel_format == PF_R4G4B4A4) {
    //         DitherRGBA4444((uint8_t*)t->m_, w, h);
    //     }
    //     else if(pixel_format == PF_R5G6B5) {
    //         DitherRGBx565((uint8_t*)pixels, w, h);
    //     }
    // }

    static uint32_t GetNumThreads(int max_threads)
    {
        uint32_t num_threads = max_threads;
        if (max_threads > 1)
        {
            num_threads = std::thread::hardware_concurrency();
            if (num_threads < 1)
                num_threads = 1;
            if (num_threads > max_threads)
                num_threads = max_threads;
        }
        return num_threads;
    }

    bool Encode(Texture* texture, PixelFormat pixel_format, ColorSpace color_space,
                CompressionLevel compression_level, CompressionType compression_type, bool mipmaps, int max_threads)
    {
        uint32_t num_threads = GetNumThreads(max_threads);
        TextureImpl* t = (TextureImpl*)texture->m_Impl;
        return t->m_Encoder.m_FnEncode(t, num_threads, pixel_format, compression_type, compression_level);
    }
}
