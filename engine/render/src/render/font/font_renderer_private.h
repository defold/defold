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

#ifndef DM_FONT_RENDERER_PRIVATE_H
#define DM_FONT_RENDERER_PRIVATE_H

#include "font_renderer.h"
#include "../render_private.h"

#include <dlib/align.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/utf8.h>

#include <graphics/graphics.h>

namespace dmRender
{
    enum RenderLayerMask
    {
        FACE    = 0x1,
        OUTLINE = 0x2,
        SHADOW  = 0x4
    };

    ///////////////////////////////////////////////////////////////////////////////

    // Helper to calculate horizontal pivot point
    static inline float OffsetX(uint32_t align, float width)
    {
        switch (align)
        {
            case TEXT_ALIGN_LEFT:   return 0.0f;
            case TEXT_ALIGN_RIGHT:  return width;
            case TEXT_ALIGN_CENTER: return width * 0.5f;
            default:                return 0.0f;
        }
    }

    // Helper to calculate vertical pivot point
    static inline float OffsetY(uint32_t valign, float height, float ascent, float descent, float leading, uint32_t line_count)
    {
        float line_height = ascent + descent;
        switch (valign)
        {
            case TEXT_VALIGN_TOP:
                return height - ascent;
            case TEXT_VALIGN_MIDDLE:
                return height * 0.5f + (line_count * (line_height * leading) - line_height * (leading - 1.0f)) * 0.5f - ascent;
            case TEXT_VALIGN_BOTTOM:
                return (line_height * leading * (line_count - 1)) + descent;
            default:
                return height - ascent;
        }
    }
}

#endif // #ifndef DM_FONT_RENDERER_PRIVATE_H
