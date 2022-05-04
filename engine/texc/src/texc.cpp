// Copyright 2020-2022 The Defold Foundation
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
            case dmTexc::CT_NONE:
            case dmTexc::CT_DEFAULT:        GetEncoderDefault(encoder); return true;
            default:
                return false;
        }
    }

    HTexture Create(const char* name, uint32_t width, uint32_t height, PixelFormat pixel_format, ColorSpace color_space, CompressionType compression_type, void* data)
    {
        Texture* t = new Texture;
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

        return t;
    }

    void Destroy(HTexture texture)
    {
        Texture* t = (Texture *) texture;
        free((void*)t->m_Name);
        t->m_Encoder.m_FnDestroy(t);
        delete t;
    }

    // For testing
    bool GetHeader(HTexture texture, Header* out_header)
    {
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnGetHeader(t, out_header);
    }

    uint32_t GetDataSizeCompressed(HTexture texture, uint32_t mip_map)
    {
        Texture* t = (Texture*) texture;
        return mip_map < t->m_Mips.Size() ? t->m_Mips[mip_map].m_ByteSize : 0;
    }

    uint32_t GetDataSizeUncompressed(HTexture texture, uint32_t mip_map)
    {
        // Since we don't add any extra compression on top of the resource, the
        // amount of memory required to store the
        return GetDataSizeCompressed(texture, mip_map);
    }

    uint32_t GetTotalDataSize(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnGetTotalDataSize(t);
    }

    uint32_t GetData(HTexture texture, void* out_data, uint32_t out_data_size)
    {
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnGetData(t, out_data, out_data_size);
    }

    uint64_t GetCompressionFlags(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return t->m_CompressionFlags;
    }

    bool Resize(HTexture texture, uint32_t width, uint32_t height)
    {
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnResize(t, width, height);
    }

    bool PreMultiplyAlpha(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnPreMultiplyAlpha(t);
    }

    bool GenMipMaps(HTexture texture)
    {
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnGenMipMaps(t);
    }

    bool Flip(HTexture texture, FlipAxis flip_axis)
    {
        if (flip_axis == FLIP_AXIS_Z)
        {
            dmLogWarning("FLIP_AXIS_Z not supported");
            return true;
        }
        Texture* t = (Texture*) texture;
        return t->m_Encoder.m_FnFlip(t, flip_axis);
    }

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

    bool Encode(HTexture texture, PixelFormat pixel_format, ColorSpace color_space,
                CompressionLevel compression_level, CompressionType compression_type, bool mipmaps, int max_threads)
    {
        Texture* t = (Texture*) texture;

        uint32_t num_threads = GetNumThreads(max_threads);
        return t->m_Encoder.m_FnEncode(t, num_threads, pixel_format, compression_type, compression_level);
    }

#define DM_TEXC_TRAMPOLINE1(ret, name, t1) \
    ret TEXC_##name(t1 a1)\
    {\
        return name(a1);\
    }\

#define DM_TEXC_TRAMPOLINE2(ret, name, t1, t2) \
    ret TEXC_##name(t1 a1, t2 a2)\
    {\
        return name(a1, a2);\
    }\

#define DM_TEXC_TRAMPOLINE3(ret, name, t1, t2, t3) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3)\
    {\
        return name(a1, a2, a3);\
    }\

#define DM_TEXC_TRAMPOLINE4(ret, name, t1, t2, t3, t4) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4)\
    {\
        return name(a1, a2, a3, a4);\
    }\

#define DM_TEXC_TRAMPOLINE5(ret, name, t1, t2, t3, t4, t5) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5)\
    {\
        return name(a1, a2, a3, a4, a5);\
    }\

#define DM_TEXC_TRAMPOLINE6(ret, name, t1, t2, t3, t4, t5, t6) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6)\
    {\
        return name(a1, a2, a3, a4, a5, a6);\
    }\

#define DM_TEXC_TRAMPOLINE7(ret, name, t1, t2, t3, t4, t5, t6, t7) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7)\
    {\
        return name(a1, a2, a3, a4, a5, a6, a7);\
    }\

#define DM_TEXC_TRAMPOLINE8(ret, name, t1, t2, t3, t4, t5, t6, t7, t8) \
    ret TEXC_##name(t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7, t8 a8)\
    {\
        return name(a1, a2, a3, a4, a5, a6, a7, a8);\
    }\

    DM_TEXC_TRAMPOLINE7(HTexture, Create, const char*, uint32_t, uint32_t, PixelFormat, ColorSpace, CompressionType, void*);
    DM_TEXC_TRAMPOLINE1(void, Destroy, HTexture);
    DM_TEXC_TRAMPOLINE2(uint32_t, GetDataSizeCompressed, HTexture, uint32_t);
    DM_TEXC_TRAMPOLINE2(uint32_t, GetDataSizeUncompressed, HTexture, uint32_t);
    DM_TEXC_TRAMPOLINE1(uint32_t, GetTotalDataSize, HTexture);
    DM_TEXC_TRAMPOLINE3(uint32_t, GetData, HTexture, void*, uint32_t);
    DM_TEXC_TRAMPOLINE1(uint64_t, GetCompressionFlags, HTexture);
    DM_TEXC_TRAMPOLINE3(bool, Resize, HTexture, uint32_t, uint32_t);
    DM_TEXC_TRAMPOLINE1(bool, PreMultiplyAlpha, HTexture);
    DM_TEXC_TRAMPOLINE1(bool, GenMipMaps, HTexture);
    DM_TEXC_TRAMPOLINE2(bool, Flip, HTexture, FlipAxis);
    DM_TEXC_TRAMPOLINE7(bool, Encode, HTexture, PixelFormat, ColorSpace, CompressionLevel, CompressionType, bool, int);
    DM_TEXC_TRAMPOLINE2(HBuffer, CompressBuffer, void*, uint32_t);
    DM_TEXC_TRAMPOLINE1(uint32_t, GetTotalBufferDataSize, HBuffer);
    DM_TEXC_TRAMPOLINE3(uint32_t, GetBufferData, HBuffer, void*, uint32_t);
    DM_TEXC_TRAMPOLINE1(void, DestroyBuffer, HBuffer);
}
