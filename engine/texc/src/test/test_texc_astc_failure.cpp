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

#include <jc_test/jc_test.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <astcenc/astcenc.h>
#include "../texc.h"
#include "../texc_private.h"

static uint32_t g_OutputAllocations;

static void* AllocateOutput(size_t size)
{
    ++g_OutputAllocations;
    return malloc(size);
}

static void FreeOutput(void* data)
{
    if (data)
    {
        --g_OutputAllocations;
    }
    free(data);
}

static astcenc_error FailCompression(astcenc_context*, astcenc_image*, const astcenc_swizzle*, uint8_t*, size_t, unsigned int)
{
    return ASTCENC_ERR_BAD_PARAM;
}

// Exercise the real encoder cleanup with a deterministic codec failure after allocation.
#define ASTCEncode ASTCEncodeWithFailure
#define astcenc_compress_image FailCompression
#define malloc AllocateOutput
#define free FreeOutput
#include "../texc_enc_astc.cpp"
#undef free
#undef malloc
#undef astcenc_compress_image
#undef ASTCEncode

// Verify codec failures release the output buffer in both single-threaded and worker paths.
TEST(TexcCompileTestASTC, CompressionFailureReleasesOutput)
{
    uint8_t pixels[4 * 4 * 4] = {};
    dmTexc::ASTCEncodeSettings settings = {};
    settings.m_Width = 4;
    settings.m_Height = 4;
    settings.m_PixelFormat = dmTexc::PF_R8G8B8A8;
    settings.m_OutPixelFormat = dmTexc::PF_RGBA_ASTC_4x4;
    settings.m_Data = pixels;
    settings.m_DataCount = sizeof(pixels);
    for (int threads = 1; threads <= 2; ++threads)
    {
        settings.m_NumThreads = threads;
        uint8_t* out = 0;
        uint32_t out_size = 0;
        ASSERT_FALSE(dmTexc::ASTCEncodeWithFailure(&settings, &out, &out_size));
        ASSERT_EQ((uint8_t*)0, out);
        ASSERT_EQ(0U, out_size);
        ASSERT_EQ(0U, g_OutputAllocations);
    }
}
