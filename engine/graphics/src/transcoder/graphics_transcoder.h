// Copyright 2020 The Defold Foundation
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

#ifndef DM_GRAPHICS_TRANSCODER_H
#define DM_GRAPHICS_TRANSCODER_H

#include <graphics/graphics.h>

namespace dmGraphics
{
    bool IsFormatTranscoded(dmGraphics::TextureImage::CompressionType format);
    bool Transcode(const char* path, TextureImage::Image* image, TextureFormat format, uint8_t** images, uint32_t* sizes, uint32_t* num_transcoded_mips);
}



#endif // #ifndef DM_GRAPHICS_TRANSCODER_H
