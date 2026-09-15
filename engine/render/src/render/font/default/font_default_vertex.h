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

#ifndef DM_FONT_DEFAULT_VERTEX_H
#define DM_FONT_DEFAULT_VERTEX_H

#include <stdint.h>

#include <dlib/align.h>

namespace dmRender
{
    // Interleaved default-backend vertex format. The renderer treats these
    // records as opaque and obtains their stride from GetFontVertexSize().
    struct DM_ALIGNED(16) FontDefaultVertex
    {
        // The first streams carry vector-font path metadata.
        float   m_Position[4];
        float   m_VectorTexcoord[4];
        float   m_VectorEffectParams[4];
        float   m_VectorBanding[4];
        uint8_t m_VectorColor[4];
        float   m_UV[2];
        float   m_FaceColor[4];
        float   m_OutlineColor[4];
        float   m_ShadowColor[4];
        float   m_SdfParams[4];
        float   m_LayerMasks[3];
    };
}

#endif // DM_FONT_DEFAULT_VERTEX_H
