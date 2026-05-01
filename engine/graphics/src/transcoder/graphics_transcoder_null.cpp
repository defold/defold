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

#include "graphics.h"

namespace dmGraphics
{
    bool IsFormatTranscoded(dmGraphics::TextureImage::CompressionType compression_type)
    {
        (void)compression_type;
        return false;
    }

    bool Transcode(const char* path, TextureImage::Image* image, uint8_t image_count, uint8_t* image_bytes, TextureFormat format, uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips)
    {
        (void)path;
        (void)image;
        (void)image_count;
        (void)image_bytes;
        (void)format;
        (void)images;
        (void)sizes;
        (void)num_transcoded_mips;
        return false;
    }
}