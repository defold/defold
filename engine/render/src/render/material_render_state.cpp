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

#include "material_render_state.h"

#include <algorithm>
#include <string.h>
#include <dlib/dstrings.h>
#include <dmsdk/dlib/math.h>
#include <dmsdk/graphics/graphics.h>

namespace dmRender
{
    typedef dmRenderDDF::MaterialRenderState StateDDF;
    typedef dmRenderDDF::MaterialDesc Desc;

    MaterialRenderState::MaterialRenderState()
    : m_VertexSpace(Desc::VERTEX_SPACE_LOCAL)
    {
        memset(&m_PbrParameters, 0, sizeof(m_PbrParameters));
    }

    template <typename T>
    static void CopyArray(dmArray<T>* out, const T* data, uint32_t count)
    {
        out->SetCapacity(count);
        out->SetSize(count);
        if (count)
            memcpy(out->Begin(), data, count * sizeof(T));
    }

    template <typename T>
    static bool CompareName(const T& a, const T& b)
    {
        return a.m_NameHash < b.m_NameHash;
    }

    template <typename T>
    static T* Find(dmArray<T>* items, dmhash_t name)
    {
        for (uint32_t i = 0; i < items->Size(); ++i)
            if ((*items)[i].m_NameHash == name)
                return &(*items)[i];
        return 0;
    }

    template <typename T>
    static void Put(dmArray<T>* items, const T& item)
    {
        T* existing = Find(items, item.m_NameHash);
        if (existing)
            *existing = item;
        else
        {
            if (items->Full())
                items->OffsetCapacity(8);
            items->Push(item);
            std::sort(items->Begin(), items->End(), CompareName<T>);
        }
    }

    void CopyMaterialRenderState(const StateDDF& source, MaterialRenderState* state)
    {
        CopyArray(&state->m_Constants, source.m_Constants.m_Data, source.m_Constants.m_Count);
        CopyArray(&state->m_Values, source.m_Values.m_Data, source.m_Values.m_Count);
        CopyArray(&state->m_Samplers, source.m_Samplers.m_Data, source.m_Samplers.m_Count);
        state->m_Attributes.SetSize(0);
        for (uint32_t i = 0; i < source.m_Attributes.m_Count; ++i)
            SetMaterialRenderAttribute(state, source.m_Attributes[i]);
        state->m_VertexSpace = source.m_VertexSpace;
        state->m_PbrParameters = source.m_PbrParameters;
    }

    StateDDF GetMaterialRenderStateDDF(MaterialRenderState* state)
    {
        StateDDF result;
        memset(&result, 0, sizeof(result));
        result.m_Constants.m_Data = state->m_Constants.Begin();
        result.m_Constants.m_Count = state->m_Constants.Size();
        result.m_Values.m_Data = state->m_Values.Begin();
        result.m_Values.m_Count = state->m_Values.Size();
        result.m_Samplers.m_Data = state->m_Samplers.Begin();
        result.m_Samplers.m_Count = state->m_Samplers.Size();
        result.m_Attributes.m_Data = state->m_Attributes.Begin();
        result.m_Attributes.m_Count = state->m_Attributes.Size();
        result.m_VertexSpace = state->m_VertexSpace;
        result.m_PbrParameters = state->m_PbrParameters;
        return result;
    }

    void AddMaterialRenderConstant(MaterialRenderState* state, dmhash_t name, Desc::ConstantType type, uint32_t count)
    {
        if (Find(&state->m_Constants, name))
            return;
        StateDDF::Constant constant = {};
        constant.m_NameHash = name;
        constant.m_Type = type;
        constant.m_ValueIndex = state->m_Values.Size();
        constant.m_ValueCount = count;
        state->m_Values.SetCapacity(state->m_Values.Size() + count);
        state->m_Values.SetSize(state->m_Values.Size() + count);
        if (count)
            memset(&state->m_Values[constant.m_ValueIndex], 0, count * sizeof(dmMath::Vector4));
        Put(&state->m_Constants, constant);
    }

    void SetMaterialRenderConstantType(MaterialRenderState* state, dmhash_t name, Desc::ConstantType type)
    {
        StateDDF::Constant* constant = Find(&state->m_Constants, name);
        if (constant)
            constant->m_Type = type;
    }

    bool SetMaterialRenderConstantValues(MaterialRenderState* state, dmhash_t name, const dmMath::Vector4* values, uint32_t count)
    {
        StateDDF::Constant* constant = Find(&state->m_Constants, name);
        if (!constant || !count)
            return false;
        uint32_t size = dmMath::Min(count, constant->m_ValueCount) * sizeof(dmMath::Vector4);
        dmMath::Vector4* current = &state->m_Values[constant->m_ValueIndex];
        if (memcmp(current, values, size) == 0)
            return false;
        memcpy(current, values, size);
        return true;
    }

    void SetMaterialRenderSampler(MaterialRenderState* state, const StateDDF::Sampler& sampler)
    {
        if (IsMaterialInternalSampler(sampler.m_NameHash))
            return;
        Put(&state->m_Samplers, sampler);
    }

    void SetMaterialRenderAttribute(MaterialRenderState* state, const dmGraphics::VertexAttribute& attribute)
    {
        // Own only layout scalars. Never retain pointers into the loaded DDF.
        dmGraphics::VertexAttribute layout = MakeMaterialRenderAttribute(attribute);
        layout.m_ElementCount = attribute.m_ElementCount;
        Put(&state->m_Attributes, layout);
    }

    static uint32_t ElementCount(dmGraphics::VertexAttribute::VectorType type)
    {
        const uint32_t counts[] = {0, 1, 2, 3, 4, 4, 9, 16};
        assert((uint32_t)type < DM_ARRAY_SIZE(counts));
        return counts[type];
    }

    dmGraphics::VertexAttribute MakeMaterialRenderAttribute(const dmGraphics::VertexAttribute& input)
    {
        dmGraphics::VertexAttribute attribute = {};
        attribute.m_Name = "";
        attribute.m_NameHash = input.m_NameHash;
        attribute.m_SemanticType = input.m_SemanticType;
        attribute.m_DataType = input.m_DataType;
        attribute.m_VectorType = input.m_VectorType;
        attribute.m_StepFunction = input.m_StepFunction;
        attribute.m_CoordinateSpace = input.m_CoordinateSpace;
        attribute.m_ElementCount = ElementCount(input.m_VectorType);
        attribute.m_Normalize = input.m_Normalize;
        return attribute;
    }

    dmGraphics::VertexAttribute MakeDefaultMaterialRenderAttribute(dmhash_t name, dmGraphics::VertexAttribute::VectorType type, uint32_t element_count)
    {
        typedef dmGraphics::VertexAttribute A;
        dmGraphics::VertexAttribute attribute = {};
        attribute.m_Name = "";
        attribute.m_NameHash = name;
        attribute.m_DataType = A::TYPE_FLOAT;
        attribute.m_VectorType = type;
        attribute.m_ElementCount = element_count;
        attribute.m_StepFunction = dmGraphics::VERTEX_STEP_FUNCTION_VERTEX;
        attribute.m_CoordinateSpace = dmGraphics::COORDINATE_SPACE_WORLD;
        attribute.m_SemanticType = A::SEMANTIC_TYPE_NONE;

        struct Default
        {
            const char* m_Name;
            A::SemanticType m_Semantic;
            dmGraphics::CoordinateSpace m_Space;
            dmGraphics::VertexStepFunction m_Step;
        };
        const Default defaults[] = {
            {"position", A::SEMANTIC_TYPE_POSITION, dmGraphics::COORDINATE_SPACE_DEFAULT, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"texcoord0", A::SEMANTIC_TYPE_TEXCOORD, dmGraphics::COORDINATE_SPACE_LOCAL, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"texcoord1", A::SEMANTIC_TYPE_TEXCOORD, dmGraphics::COORDINATE_SPACE_LOCAL, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"color", A::SEMANTIC_TYPE_COLOR, dmGraphics::COORDINATE_SPACE_LOCAL, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"page_index", A::SEMANTIC_TYPE_PAGE_INDEX, dmGraphics::COORDINATE_SPACE_LOCAL, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"normal", A::SEMANTIC_TYPE_NORMAL, dmGraphics::COORDINATE_SPACE_DEFAULT, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"tangent", A::SEMANTIC_TYPE_TANGENT, dmGraphics::COORDINATE_SPACE_DEFAULT, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"bone_weights", A::SEMANTIC_TYPE_BONE_WEIGHTS, dmGraphics::COORDINATE_SPACE_WORLD, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"bone_indices", A::SEMANTIC_TYPE_BONE_INDICES, dmGraphics::COORDINATE_SPACE_WORLD, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"mtx_world", A::SEMANTIC_TYPE_WORLD_MATRIX, dmGraphics::COORDINATE_SPACE_WORLD, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE},
            {"mtx_normal", A::SEMANTIC_TYPE_NORMAL_MATRIX, dmGraphics::COORDINATE_SPACE_WORLD, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE},
            {"animation_data", A::SEMANTIC_TYPE_NONE, dmGraphics::COORDINATE_SPACE_WORLD, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE},
            {"texture_transform_2d", A::SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D, dmGraphics::COORDINATE_SPACE_LOCAL, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX},
            {"morph_targets_weights", A::SEMANTIC_TYPE_MORPH_TARGET_WEIGHTS, dmGraphics::COORDINATE_SPACE_WORLD, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE},
        };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(defaults); ++i)
        {
            if (dmHashString64(defaults[i].m_Name) == name)
            {
                attribute.m_SemanticType = defaults[i].m_Semantic;
                attribute.m_CoordinateSpace = defaults[i].m_Space;
                attribute.m_StepFunction = defaults[i].m_Step;
                break;
            }
        }
        return attribute;
    }

    static void AddReflectedConstants(const dmGraphics::ShaderDesc::ShaderReflection& reflection, uint32_t type_index, const char* prefix, MaterialRenderState* state, uint32_t depth)
    {
        // Reflection comes from the shader compiler. Bound recursion for malformed input.
        if (type_index >= reflection.m_Types.m_Count || depth > reflection.m_Types.m_Count)
            return;
        const dmGraphics::ShaderDesc::ResourceTypeInfo& type = reflection.m_Types[type_index];
        for (uint32_t i = 0; i < type.m_Members.m_Count; ++i)
        {
            const dmGraphics::ShaderDesc::ResourceMember& member = type.m_Members[i];
            dmArray<char> name;
            name.SetCapacity(strlen(prefix) + strlen(member.m_Name) + 2);
            name.SetSize(name.Capacity());
            dmSnPrintf(name.Begin(), name.Size(), "%s%s%s", prefix, *prefix ? "." : "", member.m_Name);
            if (member.m_Type.m_UseTypeIndex)
                AddReflectedConstants(reflection, member.m_Type.m_Type.m_TypeIndex, name.Begin(), state, depth + 1);
            else if (member.m_Type.m_Type.m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_VEC4 ||
                     member.m_Type.m_Type.m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_MAT4)
            {
                bool matrix = member.m_Type.m_Type.m_ShaderType == dmGraphics::ShaderDesc::SHADER_TYPE_MAT4;
                AddMaterialRenderConstant(state, dmHashString64(name.Begin()), matrix ? Desc::CONSTANT_TYPE_USER_MATRIX4 : Desc::CONSTANT_TYPE_USER,
                    dmMath::Max(1u, member.m_ElementCount) * (matrix ? 4 : 1));
            }
        }
    }

    static void ApplyConstants(MaterialRenderState* state, const Desc::Constant* constants, uint32_t count)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            dmhash_t name = dmHashString64(constants[i].m_Name);
            SetMaterialRenderConstantType(state, name, constants[i].m_Type);
            SetMaterialRenderConstantValues(state, name, constants[i].m_Value.m_Data, constants[i].m_Value.m_Count);
        }
    }

    void BuildMaterialRenderState(const Desc& material, const dmGraphics::ShaderDesc::ShaderReflection& reflection, MaterialRenderState* state)
    {
        state->m_Constants.SetSize(0);
        state->m_Values.SetSize(0);
        state->m_Samplers.SetSize(0);
        state->m_Attributes.SetSize(0);
        state->m_VertexSpace = material.m_VertexSpace;
        state->m_PbrParameters = material.m_PbrParameters;
        for (uint32_t i = 0; i < reflection.m_UniformBuffers.m_Count; ++i)
        {
            const dmGraphics::ShaderDesc::ResourceBinding& binding = reflection.m_UniformBuffers[i];
            if (binding.m_Type.m_UseTypeIndex)
                AddReflectedConstants(reflection, binding.m_Type.m_Type.m_TypeIndex, binding.m_InstanceName ? binding.m_InstanceName : "", state, 0);
        }
        // Matches resource loading: vertex entries override fragment entries.
        ApplyConstants(state, material.m_FragmentConstants.m_Data, material.m_FragmentConstants.m_Count);
        ApplyConstants(state, material.m_VertexConstants.m_Data, material.m_VertexConstants.m_Count);

        // Record each logical sampler once. Generated array-emulation aliases are
        // backend bindings, not independent material data. Their physical units
        // and settings remain part of the runtime draw key.
        const uint32_t wraps[] = {dmGraphics::TEXTURE_WRAP_REPEAT, dmGraphics::TEXTURE_WRAP_MIRRORED_REPEAT, dmGraphics::TEXTURE_WRAP_CLAMP_TO_EDGE};
        for (uint32_t i = 0; i < material.m_Samplers.m_Count; ++i)
        {
            const Desc::Sampler& source = material.m_Samplers[i];
            dmhash_t name_hash = dmHashString64(source.m_Name);
            // These textures and their fixed sampling settings are engine-owned.
            if (IsMaterialInternalSampler(name_hash))
                continue;
            bool reflected = false;
            for (uint32_t j = 0; j < reflection.m_Textures.m_Count; ++j)
            {
                if (reflection.m_Textures[j].m_NameHash == name_hash)
                {
                    reflected = true;
                    break;
                }
            }
            // Resource loading ignores unused samplers. Array-emulation aliases
            // belong to the reflected base sampler and need not appear in reflection.
            if (!reflected)
                continue;
            StateDDF::Sampler sampler = {};
            sampler.m_NameHash = name_hash;
            sampler.m_WrapU = wraps[source.m_WrapU];
            sampler.m_WrapV = wraps[source.m_WrapV];
            sampler.m_WrapW = wraps[source.m_WrapW];
            sampler.m_FilterMin = source.m_FilterMin == Desc::FILTER_MODE_MIN_DEFAULT ? 0 : source.m_FilterMin + 1;
            sampler.m_FilterMag = source.m_FilterMag == Desc::FILTER_MODE_MAG_DEFAULT ? 0 : source.m_FilterMag + 1;
            sampler.m_MaxAnisotropy = source.m_MaxAnisotropy;
            SetMaterialRenderSampler(state, sampler);
        }
        for (uint32_t i = 0; i < reflection.m_Inputs.m_Count; ++i)
        {
            const dmGraphics::ShaderDesc::ResourceBinding& input = reflection.m_Inputs[i];
            if (input.m_StageFlags && !(input.m_StageFlags & 1))
                continue;
            dmGraphics::VertexAttribute::VectorType type;
            switch (input.m_Type.m_Type.m_ShaderType)
            {
                case dmGraphics::ShaderDesc::SHADER_TYPE_VEC2: type = dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2; break;
                case dmGraphics::ShaderDesc::SHADER_TYPE_VEC3: type = dmGraphics::VertexAttribute::VECTOR_TYPE_VEC3; break;
                case dmGraphics::ShaderDesc::SHADER_TYPE_VEC4: type = dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4; break;
                case dmGraphics::ShaderDesc::SHADER_TYPE_MAT2: type = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT2; break;
                case dmGraphics::ShaderDesc::SHADER_TYPE_MAT3: type = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3; break;
                case dmGraphics::ShaderDesc::SHADER_TYPE_MAT4: type = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4; break;
                default: type = dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR; break;
            }
            SetMaterialRenderAttribute(state, MakeDefaultMaterialRenderAttribute(input.m_NameHash, type, ElementCount(type)));
        }
        for (uint32_t i = 0; i < material.m_Attributes.m_Count; ++i)
        {
            if (Find(&state->m_Attributes, material.m_Attributes[i].m_NameHash))
                SetMaterialRenderAttribute(state, MakeMaterialRenderAttribute(material.m_Attributes[i]));
        }
    }

    bool IsMaterialInternalSampler(dmhash_t name)
    {
        static const dmhash_t pose_matrix_cache = dmHashString64("pose_matrix_cache");
        static const dmhash_t morph_targets = dmHashString64("morph_targets");
        return name == pose_matrix_cache || name == morph_targets;
    }

    static void HashU32(HashState64* hash, uint32_t value)
    {
        uint8_t bytes[4];
        for (uint32_t i = 0; i < 4; ++i)
            bytes[i] = (uint8_t)(value >> (i * 8));
        dmHashUpdateBuffer64(hash, bytes, sizeof(bytes));
    }

    static void HashU64(HashState64* hash, uint64_t value)
    {
        HashU32(hash, (uint32_t)value);
        HashU32(hash, (uint32_t)(value >> 32));
    }

    static void HashFloat(HashState64* hash, float value)
    {
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));
        HashU32(hash, bits);
    }

    uint64_t HashMaterialRenderState(const MaterialRenderState& state)
    {
        HashState64 hash;
        dmHashInit64(&hash, false);
        HashU32(&hash, 1); // Canonical encoding version.
        HashU32(&hash, state.m_VertexSpace);
        HashU32(&hash, state.m_Constants.Size());
        for (uint32_t i = 0; i < state.m_Constants.Size(); ++i)
        {
            const StateDDF::Constant& constant = state.m_Constants[i];
            HashU64(&hash, constant.m_NameHash);
            HashU32(&hash, constant.m_Type);
            HashU32(&hash, constant.m_ValueCount);
            for (uint32_t j = 0; j < constant.m_ValueCount; ++j)
            {
                const dmMath::Vector4& value = state.m_Values[constant.m_ValueIndex + j];
                HashFloat(&hash, value.m_X);
                HashFloat(&hash, value.m_Y);
                HashFloat(&hash, value.m_Z);
                HashFloat(&hash, value.m_W);
            }
        }
        uint32_t sampler_count = 0;
        for (uint32_t i = 0; i < state.m_Samplers.Size(); ++i)
            sampler_count += !IsMaterialInternalSampler(state.m_Samplers[i].m_NameHash);
        HashU32(&hash, sampler_count);
        for (uint32_t i = 0; i < state.m_Samplers.Size(); ++i)
        {
            const StateDDF::Sampler& sampler = state.m_Samplers[i];
            if (IsMaterialInternalSampler(sampler.m_NameHash))
                continue;
            HashU64(&hash, sampler.m_NameHash);
            HashU32(&hash, sampler.m_WrapU);
            HashU32(&hash, sampler.m_WrapV);
            HashU32(&hash, sampler.m_WrapW);
            HashU32(&hash, sampler.m_FilterMin);
            HashU32(&hash, sampler.m_FilterMag);
            HashFloat(&hash, sampler.m_MaxAnisotropy);
        }
        HashU32(&hash, state.m_Attributes.Size());
        for (uint32_t i = 0; i < state.m_Attributes.Size(); ++i)
        {
            const dmGraphics::VertexAttribute& attribute = state.m_Attributes[i];
            HashU64(&hash, attribute.m_NameHash);
            HashU32(&hash, attribute.m_SemanticType);
            HashU32(&hash, attribute.m_DataType);
            HashU32(&hash, attribute.m_VectorType);
            HashU32(&hash, attribute.m_StepFunction);
            HashU32(&hash, attribute.m_CoordinateSpace);
            HashU32(&hash, attribute.m_ElementCount);
            HashU32(&hash, attribute.m_Normalize);
        }
        const Desc::PbrParameters& pbr = state.m_PbrParameters;
        HashU32(&hash, pbr.m_HasParameters);
        HashU32(&hash, pbr.m_HasMetallicRoughness);
        HashU32(&hash, pbr.m_HasSpecularGlossiness);
        HashU32(&hash, pbr.m_HasClearcoat);
        HashU32(&hash, pbr.m_HasTransmission);
        HashU32(&hash, pbr.m_HasIor);
        HashU32(&hash, pbr.m_HasSpecular);
        HashU32(&hash, pbr.m_HasVolume);
        HashU32(&hash, pbr.m_HasSheen);
        HashU32(&hash, pbr.m_HasEmissiveStrength);
        HashU32(&hash, pbr.m_HasIridescence);
        return dmHashFinal64(&hash);
    }
}
