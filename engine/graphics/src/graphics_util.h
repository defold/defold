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

#ifndef DM_GRAPHICS_UTIL_H
#define DM_GRAPHICS_UTIL_H

#include <dlib/endian.h>
#include <dmsdk/dlib/vmath.h>

#include "graphics.h"

namespace dmGraphics
{
    inline uint32_t PackRGBA(const dmVMath::Vector4& in_color)
    {
        uint8_t r = (uint8_t)(in_color.getX() * 255.0f);
        uint8_t g = (uint8_t)(in_color.getY() * 255.0f);
        uint8_t b = (uint8_t)(in_color.getZ() * 255.0f);
        uint8_t a = (uint8_t)(in_color.getW() * 255.0f);

#if DM_ENDIAN == DM_ENDIAN_LITTLE
        uint32_t c = a << 24 | b << 16 | g << 8 | r;
#else
        uint32_t c = r << 24 | g << 16 | b << 8 | a;
#endif
        return c;
    }

    inline const dmVMath::Vector4 UnpackRGBA(uint32_t in_color)
    {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        float r = (float)((in_color & 0x000000FF)      ) / 255.0f;
        float g = (float)((in_color & 0x0000FF00) >>  8) / 255.0f;
        float b = (float)((in_color & 0x00FF0000) >> 16) / 255.0f;
        float a = (float)((in_color & 0xFF000000) >> 24) / 255.0f;
#else
        float r = (float)((in_color & 0xFF000000) >> 24) / 255.0f;
        float g = (float)((in_color & 0x00FF0000) >> 16) / 255.0f;
        float b = (float)((in_color & 0x0000FF00) >>  8) / 255.0f;
        float a = (float)((in_color & 0x000000FF)      ) / 255.0f;
#endif
        return dmVMath::Vector4(r,g,b,a);
    }
}



#endif // #ifndef DM_GRAPHICS_UTIL_H
