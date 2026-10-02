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

#ifndef DM_PARTICLE_RENDER_H
#define DM_PARTICLE_RENDER_H

#include <dlib/transform.h>
#include "particle.h"

namespace dmParticle
{
    // Immutable evaluated inputs. No emitter, animation table or resource pointer.
    // Geometry expansion and attribute packing happen only during consumption.
    struct RenderParticle
    {
        dmTransform::TransformS1 m_WorldTransform;
        dmVMath::Vector4 m_Color;
        float m_TexCoords[8];
        float m_HalfWidth;
        float m_HalfHeight;
        float m_PageIndex;
        uint32_t m_Flip;
    };

    // Producer copies the whole emitter or rejects insufficient storage. The
    // caller allocates its frame-owned array and owns retirement of that array.
    bool CaptureRenderParticles(HParticleContext context, HInstance instance, uint32_t emitter,
                                RenderParticle* output, uint32_t capacity, uint32_t* count);
    GenerateVertexDataResult GenerateCapturedParticleVertices(const RenderParticle* particles, uint32_t count,
                                uint32_t start, uint32_t length, const dmGraphics::VertexAttributeInfos& attributes,
                                const dmVMath::Vector4& color, void* buffer, uint32_t capacity, uint32_t* size);
}
#endif
