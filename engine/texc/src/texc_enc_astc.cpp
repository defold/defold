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

#include <dlib/log.h>

#include <astcenc/astcenc.h>

#include "texc.h"
#include "texc_private.h"

namespace dmTexc
{
    // Implementation taken from https://github.com/ARM-software/astc-encoder/blob/main/Utils/Example/astc_api_example.cpp
    bool ASTCEncode(ASTCEncodeSettings* settings, uint8_t** out, uint32_t* out_size)
    {
        // For the purposes of this sample we hard-code the compressor settings
        static const uint32_t thread_count = 1;
        static const uint32_t block_x = 4; // TODO
        static const uint32_t block_y = 4; // TODO
        static const uint32_t block_z = 1;
        static const astcenc_profile profile = ASTCENC_PRF_LDR;
        static const float quality = ASTCENC_PRE_MEDIUM;
        static const astcenc_swizzle swizzle {
            ASTCENC_SWZ_R,
            ASTCENC_SWZ_G,
            ASTCENC_SWZ_B,
            ASTCENC_SWZ_A
        };

        // Compute the number of ASTC blocks in each dimension
        uint32_t block_count_x = (settings->m_Width + block_x - 1) / block_x;
        uint32_t block_count_y = (settings->m_Height + block_y - 1) / block_y;

        astcenc_config config;
        config.block_x = block_x;
        config.block_y = block_y;
        config.profile = profile;

        astcenc_error status;
        status = astcenc_config_init(profile, block_x, block_y, block_z, quality, 0, &config);
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

        // Compress the image
        astcenc_image image;
        image.dim_x = settings->m_Width;
        image.dim_y = settings->m_Height;
        image.dim_z = 1;
        image.data_type = ASTCENC_TYPE_U8;
        uint8_t* slices = settings->m_Data;
        image.data = reinterpret_cast<void**>(&slices);

        // Space needed for 16 bytes of output per compressed block
        size_t comp_len = block_count_x * block_count_y * 16;
        uint8_t* comp_data = new uint8_t[comp_len];

        status = astcenc_compress_image(context, &image, &swizzle, comp_data, comp_len, 0);
        if (status != ASTCENC_SUCCESS)
        {
            dmLogError("ERROR: Codec compress failed: %s", astcenc_get_error_string(status));
            return false;
        }

        *out = comp_data;
        *out_size = comp_len;

        return true;
    }
}
