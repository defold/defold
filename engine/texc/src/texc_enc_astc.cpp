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

#include <dlib/log.h>
#include <dlib/math.h>

#include <stdio.h>

#include <astcenc/astcenc.h>

#include "texc.h"
#include "texc_private.h"

namespace dmTexc
{
    static bool ParseBlockSizes(PixelFormat pf, uint32_t* x, uint32_t* y)
    {
    #define CASE_AND_SET(a,b) \
        case PF_RGBA_ASTC_##a##x##b: \
            *x = a; \
            *y = b; \
            break;

        switch(pf)
        {
            CASE_AND_SET(4,4);
            CASE_AND_SET(5,4);
            CASE_AND_SET(5,5);
            CASE_AND_SET(6,5);
            CASE_AND_SET(6,6);
            CASE_AND_SET(8,5);
            CASE_AND_SET(8,6);
            CASE_AND_SET(8,8);
            CASE_AND_SET(10,5);
            CASE_AND_SET(10,6);
            CASE_AND_SET(10,8);
            CASE_AND_SET(10,10);
            CASE_AND_SET(12,10);
            CASE_AND_SET(12,12);
            default: return false;
        }
        return true;

    #undef CASE_AND_SET
    }

    // Implementation taken from https://github.com/ARM-software/astc-encoder/blob/main/Utils/Example/astc_api_example.cpp
    bool ASTCEncode(ASTCEncodeSettings* settings, uint8_t** out, uint32_t* out_size)
    {
        // For the purposes of this sample we hard-code the compressor settings
        uint32_t thread_count   = dmMath::Max(1, settings->m_NumThreads);
        uint32_t block_x        = 0;
        uint32_t block_y        = 0;
        uint32_t block_z        = 1;
        astcenc_profile profile = ASTCENC_PRF_LDR;

        if (settings->m_QualityLevel < 0.0 || settings->m_QualityLevel > 100.0f)
        {
            dmLogError("Invalid quality level, range must be [0..100], but is %f", settings->m_QualityLevel);
            return false;
        }

        if (!ParseBlockSizes(settings->m_OutPixelFormat, &block_x, &block_y))
        {
            dmLogError("Unable to parse block sizes from pixel format %d", settings->m_OutPixelFormat);
            return false;
        }

        const astcenc_swizzle swizzle {
            ASTCENC_SWZ_R,
            ASTCENC_SWZ_G,
            ASTCENC_SWZ_B,
            ASTCENC_SWZ_A
        };

        astcenc_config config;
        astcenc_error status = astcenc_config_init(profile, block_x, block_y, block_z, settings->m_QualityLevel, 0, &config);
        if (status != ASTCENC_SUCCESS)
        {
            dmLogError("Codec config init failed: %s", astcenc_get_error_string(status));
            return false;
        }

        // Create a context based on the configuration
        astcenc_context* context;
        status = astcenc_context_alloc(&config, thread_count, &context);
        if (status != ASTCENC_SUCCESS)
        {
            dmLogError("Codec context alloc failed: %s", astcenc_get_error_string(status));
            return false;
        }

        int aligned_width = ((settings->m_Width + block_x - 1) / block_x) * block_x;
        int aligned_height = ((settings->m_Height + block_y - 1) / block_y) * block_y;

        uint8_t* padded_data = settings->m_Data;
        bool create_padded_data = aligned_width != settings->m_Width || aligned_height != settings->m_Height;

        if (create_padded_data)
        {
            padded_data = (unsigned char*) malloc(aligned_width * aligned_height * 4);
            memset(padded_data, 0, aligned_width * aligned_height * 4);

            // Copy the input image to create a pagged ASTC output
            for (int y = 0; y < settings->m_Height; ++y) {
                memcpy(
                    &padded_data[y * aligned_width * 4],
                    &settings->m_Data[y * settings->m_Width * 4],
                    settings->m_Width * 4
                );
            }
        }

        // Compress the image
        astcenc_image image = {};
        image.dim_x         = aligned_width;
        image.dim_y         = aligned_height;
        image.dim_z         = 1;
        image.data_type     = ASTCENC_TYPE_U8;
        image.data          = reinterpret_cast<void**>(&padded_data);

        // Space needed for 16 bytes of output per compressed block
        uint32_t comp_len   = aligned_width * aligned_height * 16 / (block_x * block_y); // Approximate size
        uint8_t* comp_data  = (uint8_t*)malloc(comp_len);

        status = astcenc_compress_image(context, &image, &swizzle, comp_data, comp_len, 0);

        if (create_padded_data)
        {
            free(padded_data);
        }

        astcenc_context_free(context);

        if (status != ASTCENC_SUCCESS)
        {
            dmLogError("ERROR: Codec compress failed: %s", astcenc_get_error_string(status));
            return false;
        }

        *out      = comp_data;
        *out_size = comp_len;

        return true;
    }
}
