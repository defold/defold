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

#ifndef DM_PARTICLE_TEST_HPP
#define DM_PARTICLE_TEST_HPP

#include <particle/particle.h>
#include <graphics/graphics_ddf.h>

// Mutable authored attributes for fault-injection tests. The prototype owns the
// returned attribute; callers must restore any replaced allocations before deletion.
namespace dmParticle
{
    uint32_t                     TestGetEmitterAttributeCount(HPrototype prototype, uint32_t emitter);
    dmGraphics::VertexAttribute* TestGetEmitterAttribute(HPrototype prototype, uint32_t emitter, uint32_t attribute);
}

#endif // DM_PARTICLE_TEST_HPP
