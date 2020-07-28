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

#include <stdint.h>

namespace dmTexc
{
    void RGB565ToRGB888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            *(color_rgb++) = (c>>8) & 0xf8;
            *(color_rgb++) = (c>>3) & 0xfc;
            *(color_rgb++) = (c<<3) & 0xf8;
        }
    }

    void RGBA4444ToRGBA8888(const uint16_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            const uint16_t c = *(data++);
            *(color_rgba++) = ((c>>8) & 0xf0);
            *(color_rgba++) = ((c>>4) & 0xf0);
            *(color_rgba++) = ((c) & 0xf0);
            *(color_rgba++) = ((c<<4) & 0xf0);
        }
    }

    void L8ToRGB888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgb)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgb++) = *(data);
            *(color_rgb++) = *(data);
            *(color_rgb++) = *(data++);
        }
    }

    void L8A8ToRGBA8888(const uint8_t* data, const uint32_t width, const uint32_t height, uint8_t* color_rgba)
    {
        for(uint32_t i = 0; i < width*height; ++i)
        {
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data);
            *(color_rgba++) = *(data++);
            *(color_rgba++) = *(data++);
        }
    }

}
