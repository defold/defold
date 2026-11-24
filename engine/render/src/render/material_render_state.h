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

#ifndef DM_RENDER_MATERIAL_RENDER_STATE_H
#define DM_RENDER_MATERIAL_RENDER_STATE_H

#include <dlib/array.h>
#include <dlib/hash.h>
#include <render/material_ddf.h>

namespace dmRender
{
    // Owns the portable state. No resource paths, backend handles or locations.
    struct MaterialRenderState
    {
        dmArray<dmRenderDDF::MaterialRenderState::Constant> m_Constants;
        dmArray<dmMath::Vector4> m_Values;
        dmArray<dmRenderDDF::MaterialRenderState::Sampler> m_Samplers;
        dmArray<dmRenderDDF::MaterialRenderState::Attribute> m_Attributes;
        dmRenderDDF::MaterialDesc::VertexSpace m_VertexSpace;
        dmRenderDDF::MaterialDesc::PbrParameters m_PbrParameters;

        MaterialRenderState();
    };

    void BuildMaterialRenderState(const dmRenderDDF::MaterialDesc& material, const dmGraphics::ShaderDesc::ShaderReflection& reflection, MaterialRenderState* state);
    void CopyMaterialRenderState(const dmRenderDDF::MaterialRenderState& source, MaterialRenderState* state);
    // Returns a borrowed view, valid while state remains alive and unmodified.
    dmRenderDDF::MaterialRenderState GetMaterialRenderStateDDF(MaterialRenderState* state);
    uint64_t HashMaterialRenderState(const MaterialRenderState& state, uint32_t sampler_exclusions = 0);
    bool IsMaterialRenderSamplerExcluded(dmhash_t name_hash, uint32_t sampler_exclusions);

    void AddMaterialRenderConstant(MaterialRenderState* state, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type, uint32_t count);
    void SetMaterialRenderConstantType(MaterialRenderState* state, dmhash_t name_hash, dmRenderDDF::MaterialDesc::ConstantType type);
    bool SetMaterialRenderConstantValues(MaterialRenderState* state, dmhash_t name_hash, const dmMath::Vector4* values, uint32_t count);
    void SetMaterialRenderSampler(MaterialRenderState* state, const dmRenderDDF::MaterialRenderState::Sampler& sampler);
    void SetMaterialRenderAttribute(MaterialRenderState* state, const dmRenderDDF::MaterialRenderState::Attribute& attribute);
    dmRenderDDF::MaterialRenderState::Attribute MakeMaterialRenderAttribute(const dmGraphics::VertexAttribute& attribute);
    dmRenderDDF::MaterialRenderState::Attribute MakeDefaultMaterialRenderAttribute(dmhash_t name_hash, dmGraphics::VertexAttribute::VectorType vector_type, uint32_t element_count);
}
#endif
