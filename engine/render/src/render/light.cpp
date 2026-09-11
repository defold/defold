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

#include "render.h"
#include "render_private.h"

#include <stddef.h>

#include <dmsdk/dlib/static_assert.h>

namespace dmRender
{
    static const dmhash_t LIGHT_BUFFER_TYPE = dmHashString64("LightBuffer");
    static const dmhash_t LIGHT_MEMBER_TYPE = dmHashString64("lights");

    static void CommitLightInstance(HRenderContext render_context, const LightInstance* instance, dmVMath::Point3 position, dmVMath::Vector3 direction, float scale);
    static void CommitLightInfo(HRenderContext render_context);
    static void FillLightInstanceSTD140(const LightPrototype* prototype, dmVMath::Point3 position, dmVMath::Vector3 world_direction, float scale, LightSTD140* out_light);
    static bool LightSTD140Equals(const LightSTD140& a, const LightSTD140& b);
    static LightUniformBuffer* EnsureLightUniformBuffer(HRenderContext render_context, uint16_t capacity);

    DM_STATIC_ASSERT(sizeof(LightSTD140) == LIGHT_BUFFER_LIGHT_STRIDE, Invalid_LightSTD140_Size);
    DM_STATIC_ASSERT(offsetof(LightSTD140, m_Position) == 0, Invalid_LightSTD140_Position_Offset);
    DM_STATIC_ASSERT(offsetof(LightSTD140, m_Color) == 16, Invalid_LightSTD140_Color_Offset);
    DM_STATIC_ASSERT(offsetof(LightSTD140, m_DirectionRange) == 32, Invalid_LightSTD140_DirectionRange_Offset);
    DM_STATIC_ASSERT(offsetof(LightSTD140, m_Params) == 48, Invalid_LightSTD140_Params_Offset);

    static inline dmVMath::Vector3 GetLightForwardDirection()
    {
        return dmVMath::Vector3(0.0f, 0.0f, -1.0f);
    }

    ////////////////////////////////
    // Light prototype
    ////////////////////////////////

    LightPrototypeParams::LightPrototypeParams()
    : m_Type(LIGHT_TYPE_POINT)
    , m_Color(1.0f, 1.0f, 1.0f, 1.0f)
    , m_Intensity(1.0f)
    , m_Range(10.0f)
    , m_InnerConeAngle(0.0f)
    , m_OuterConeAngle(M_PI_4)
    {
    }

    HLightPrototype NewLightPrototype(HRenderContext render_context, const LightPrototypeParams& params)
    {
        if (render_context->m_LightPrototypes.Full())
        {
            render_context->m_LightPrototypes.Allocate(32);
        }

        LightPrototype* lp = new LightPrototype;
        HLightPrototype handle = render_context->m_LightPrototypes.Put(lp);
        SetLightPrototype(render_context, handle, params);
        return handle;
    }

    void SetLightPrototype(HRenderContext render_context, HLightPrototype light_prototype, const LightPrototypeParams& params)
    {
        LightPrototype* lp = render_context->m_LightPrototypes.Get(light_prototype);
        if (!lp)
        {
            return;
        }

        memset(lp, 0, sizeof(LightPrototype));
        lp->m_Type = params.m_Type;
        lp->m_Color = params.m_Color;
        lp->m_Intensity = params.m_Intensity;
        lp->m_Range = params.m_Range;
        lp->m_InnerConeAngle = params.m_InnerConeAngle;
        lp->m_OuterConeAngle = params.m_OuterConeAngle;
    }

    void DeleteLightPrototype(HRenderContext render_context, HLightPrototype light_prototype)
    {
        LightPrototype* lp = render_context->m_LightPrototypes.Get(light_prototype);
        if (lp)
        {
            render_context->m_LightPrototypes.Release(light_prototype);
            delete lp;
        }
    }

    const LightPrototype* GetLightPrototype(HRenderContext render_context, HLightPrototype light_prototype)
    {
        return render_context->m_LightPrototypes.Get(light_prototype);
    }

    LightType GetLightType(HRenderContext render_context, HLightPrototype light_prototype)
    {
        LightPrototype* prototype = render_context->m_LightPrototypes.Get(light_prototype);
        return prototype ? prototype->m_Type : LIGHT_TYPE_DIRECTIONAL;
    }

    dmVMath::Vector4 GetLightColor(HRenderContext render_context, HLightPrototype light_prototype)
    {
        LightPrototype* prototype = render_context->m_LightPrototypes.Get(light_prototype);
        return prototype ? prototype->m_Color : dmVMath::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    float GetLightIntensity(HRenderContext render_context, HLightPrototype light_prototype)
    {
        LightPrototype* prototype = render_context->m_LightPrototypes.Get(light_prototype);
        return prototype ? prototype->m_Intensity : 0.0f;
    }

    ////////////////////////////////
    // Light instance
    ////////////////////////////////

    HLightInstance NewLightInstance(HRenderContext render_context, HLightPrototype light_prototype)
    {
        LightPrototype* prototype = render_context->m_LightPrototypes.Get(light_prototype);
        if (!prototype)
        {
            return 0;
        }

        // Reached max count.
        if (render_context->m_RenderLightsIndices.Size() >= render_context->m_MaxLightCount || render_context->m_RenderLightsIndices.Remaining() == 0)
        {
            return 0;
        }

        uint16_t light_buffer_index = render_context->m_RenderLightsIndices.Pop();
        LightInstance* light_instance = &render_context->m_RenderLights[light_buffer_index];
        light_instance->m_LightPrototype   = light_prototype;
        light_instance->m_LightBufferIndex = light_buffer_index;
        light_instance->m_Version++;
        if (light_instance->m_Version == 0 || light_instance->m_Version == 0xFFFF)
        {
            light_instance->m_Version = 1;
        }

        if (light_instance->m_LightBufferIndex >= render_context->m_LightBufferScratch.Size())
        {
            render_context->m_LightBufferScratch.SetSize(light_instance->m_LightBufferIndex+1);
        }

        CommitLightInstance(render_context, light_instance, dmVMath::Point3(0.0f, 0.0f, 0.0f), GetLightForwardDirection(), 1.0f);
        CommitLightInfo(render_context);

        return light_instance->m_Version << 16 | light_buffer_index;
    }

    void DeleteLightInstance(HRenderContext render_context, HLightInstance instance)
    {
        uint16_t light_buffer_index = instance & 0xFFFF;
        LightInstance* light_instance = light_buffer_index < render_context->m_RenderLights.Size() ? &render_context->m_RenderLights[light_buffer_index] : 0;
        if (light_instance)
        {
            if (light_instance->m_LightPrototype == 0 || light_instance->m_Version != (instance >> 16))
            {
                return;
            }
            render_context->m_RenderLightsIndices.Push(light_instance->m_LightBufferIndex);
            render_context->m_LightBufferSubmitted[light_instance->m_LightBufferIndex] = 0;
            light_instance->m_LightPrototype = 0;
            CommitLightInfo(render_context);
        }
    }

    void SubmitLightInstance(HRenderContext render_context, HLightInstance instance)
    {
        uint16_t light_buffer_index = instance & 0xFFFF;
        LightInstance* light_instance = light_buffer_index < render_context->m_RenderLights.Size() ? &render_context->m_RenderLights[light_buffer_index] : 0;
        if (!light_instance || light_instance->m_LightPrototype == 0 || light_instance->m_Version != (instance >> 16))
        {
            return;
        }

        if (!render_context->m_LightBufferSubmitted[light_buffer_index])
        {
            render_context->m_LightBufferSubmitted[light_buffer_index] = 1;
            CommitLightInfo(render_context);
        }
    }

    void SetLightInstance(HRenderContext render_context, HLightInstance instance, dmVMath::Point3 position, dmVMath::Quat rotation, float scale)
    {
        uint16_t light_buffer_index = instance & 0xFFFF;
        LightInstance* light_instance = light_buffer_index < render_context->m_RenderLights.Size() ? &render_context->m_RenderLights[light_buffer_index] : 0;
        if (!light_instance || light_instance->m_LightPrototype == 0 || light_instance->m_Version != (instance >> 16))
        {
            return;
        }

        dmVMath::Vector3 direction = dmVMath::Rotate(rotation, GetLightForwardDirection());
        float clamped_scale = dmMath::Max(0.0f, scale);
        const LightPrototype* prototype = render_context->m_LightPrototypes.Get(light_instance->m_LightPrototype);
        if (!prototype)
        {
            return;
        }

        LightSTD140 updated_light;
        FillLightInstanceSTD140(prototype, position, direction, clamped_scale, &updated_light);
        bool needs_commit = !LightSTD140Equals(updated_light, render_context->m_LightBufferScratch[light_instance->m_LightBufferIndex]);

        if (needs_commit)
        {
            CommitLightInstance(render_context, light_instance, position, direction, clamped_scale);
        }
    }

    ////////////////////////////////
    // Light buffer
    ////////////////////////////////

    static dmGraphics::UniformBufferLayout GetLightBufferLayout(uint32_t light_count, uint32_t* out_size, uint32_t* out_info_offset, uint32_t* out_data_offset)
    {
        /*
        struct Light
        {
            vec4 position;        // xyz: position, w: unused
            vec4 color;           // RGBA (matches LightParams order)
            vec4 direction_range; // xyz: normalized direction; w: range
            vec4 params;          // x: type (0 dir, 1 point, 2 spot; matches dmRender::LightType)
                                  // y: intensity
                                  // z: innerConeAngle (radians, spot only)
                                  // w: outerConeAngle (radians, spot only)
        };
        uniform LightBuffer
        {
            vec4  light_info;     // xyz: accumulated ambient color, w: number of active lights
            Light lights[MAX_LIGHTS];
        };
        */

        dmGraphics::ShaderResourceMember   light_buffer_members[2];
        dmGraphics::ShaderResourceMember   light_members[4];
        dmGraphics::ShaderResourceTypeInfo light_types[2];

        // Ensure all fields (including bitfields) are initialized
        memset(light_buffer_members, 0, sizeof(light_buffer_members));
        memset(light_members, 0, sizeof(light_members));
        memset(light_types, 0, sizeof(light_types));

        // light_info (vec4)
        light_buffer_members[0].m_Name                 = (char*)"light_info";
        light_buffer_members[0].m_NameHash             = dmHashString64("light_info");
        light_buffer_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_buffer_members[0].m_Type.m_UseTypeIndex  = 0;
        light_buffer_members[0].m_ElementCount         = 1;
        // lights
        light_buffer_members[1].m_Name                 = (char*)"lights";
        light_buffer_members[1].m_NameHash             = dmHashString64("lights");
        light_buffer_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_buffer_members[1].m_ElementCount         = light_count;
        light_buffer_members[1].m_Type.m_TypeIndex     = 1; // index into ShaderResourceTypeInfo[]
        light_buffer_members[1].m_Type.m_UseTypeIndex  = 1;

        // vec4 position
        light_members[0].m_Name                 = (char*)"position";
        light_members[0].m_NameHash             = dmHashString64("position");
        light_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[0].m_Type.m_UseTypeIndex  = 0;
        light_members[0].m_ElementCount         = 1;
        // vec4 color
        light_members[1].m_Name                 = (char*)"color";
        light_members[1].m_NameHash             = dmHashString64("color");
        light_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[1].m_Type.m_UseTypeIndex  = 0;
        light_members[1].m_ElementCount         = 1;
        // vec4 direction (xyz: direction, w: range)
        light_members[2].m_Name                 = (char*)"direction_range";
        light_members[2].m_NameHash             = dmHashString64("direction_range");
        light_members[2].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[2].m_Type.m_UseTypeIndex  = 0;
        light_members[2].m_ElementCount         = 1;
        // vec4 params (x: type, y: intensity, z: innerConeAngle, w: outerConeAngle)
        light_members[3].m_Name                 = (char*)"params";
        light_members[3].m_NameHash             = dmHashString64("params");
        light_members[3].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[3].m_Type.m_UseTypeIndex  = 0;
        light_members[3].m_ElementCount         = 1;

        // LightBuffer (index 1)
        light_types[0].m_Name        = (char*)"LightBuffer";
        light_types[0].m_NameHash    = dmHashString64("LightBuffer");
        light_types[0].m_Members     = light_buffer_members;
        light_types[0].m_MemberCount = DM_ARRAY_SIZE(light_buffer_members);
        // Light (index 0)
        light_types[1].m_Name        = (char*)"Light";
        light_types[1].m_NameHash    = dmHashString64("Light");
        light_types[1].m_Members     = light_members;
        light_types[1].m_MemberCount = DM_ARRAY_SIZE(light_members);

        dmGraphics::UpdateShaderTypesOffsets(light_types, DM_ARRAY_SIZE(light_types));

        if (out_size)
        {
            *out_size = dmGraphics::GetUniformBufferTypeSize(0, light_types, DM_ARRAY_SIZE(light_types));
        }
        if (out_info_offset)
        {
            *out_info_offset = light_buffer_members[0].m_Offset;
        }
        if (out_data_offset)
        {
            *out_data_offset = light_buffer_members[1].m_Offset;
        }

        // The engine owns the LightBuffer contract. Programs may declare
        // different fixed lights[] capacities, so the light-specific layout
        // hash intentionally ignores only that array size.
        light_buffer_members[1].m_ElementCount = 1;
        return dmGraphics::GetUniformBufferLayout(0, light_types, DM_ARRAY_SIZE(light_types));
    }

    static LightUniformBuffer* GenerateUniformBuffer(HRenderContext render_context, uint16_t capacity)
    {
        uint32_t buffer_size = 0;
        uint32_t info_offset = 0;
        uint32_t data_offset = 0;
        dmGraphics::UniformBufferLayout layout = GetLightBufferLayout(capacity, &buffer_size, &info_offset, &data_offset);
        dmGraphics::HUniformBuffer buffer = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, layout, buffer_size);
        if (!buffer)
        {
            return 0;
        }

        assert(info_offset == render_context->m_LightBufferInfoWriteStart);
        assert(data_offset == render_context->m_LightBufferDataWriteStart);

        if (render_context->m_LightUniformBuffers.Full())
        {
            render_context->m_LightUniformBuffers.OffsetCapacity(4);
        }

        LightUniformBuffer light_buffer;
        light_buffer.m_Buffer   = buffer;
        light_buffer.m_Version  = 0;
        light_buffer.m_Capacity = capacity;
        render_context->m_LightUniformBuffers.Push(light_buffer);
        return &render_context->m_LightUniformBuffers.Back();
    }

    static inline void FillLightInstanceSTD140(const LightPrototype* prototype, dmVMath::Point3 position, dmVMath::Vector3 world_direction, float scale, LightSTD140* out_light)
    {
        out_light->m_Position = dmVMath::Vector4(position);
        out_light->m_Color    = prototype->m_Color;

        dmVMath::Vector3 direction(0.0f, 0.0f, 0.0f);
        float range      = 0.0f;
        float inner_cone = 0.0f;
        float outer_cone = 0.0f;

        switch (prototype->m_Type)
        {
        case LIGHT_TYPE_AMBIENT:
            break;
        case LIGHT_TYPE_DIRECTIONAL:
            direction = world_direction;
            break;
        case LIGHT_TYPE_POINT:
            range = prototype->m_Range * scale;
            break;
        case LIGHT_TYPE_SPOT:
            direction  = world_direction;
            range      = prototype->m_Range * scale;
            inner_cone = prototype->m_InnerConeAngle;
            outer_cone = prototype->m_OuterConeAngle;
            break;
        default:
            assert("Light type not supported!");
            break;
        }

        out_light->m_DirectionRange = dmVMath::Vector4(direction, range);
        out_light->m_Params = dmVMath::Vector4((float) prototype->m_Type, prototype->m_Intensity, inner_cone, outer_cone);
    }

    static inline bool Vector4Equals(const dmVMath::Vector4& a, const dmVMath::Vector4& b)
    {
        const float eps = 1e-4f;
        dmVMath::Vector4 diff = a - b;
        float dist2 = dmVMath::LengthSqr(diff);
        return dist2 <= eps * eps;
    }

    static inline bool LightSTD140Equals(const LightSTD140& a, const LightSTD140& b)
    {
        return Vector4Equals(a.m_Position, b.m_Position)
            && Vector4Equals(a.m_Color, b.m_Color)
            && Vector4Equals(a.m_DirectionRange, b.m_DirectionRange)
            && Vector4Equals(a.m_Params, b.m_Params);
    }

    static inline void CommitLightInstance(HRenderContext render_context, const LightInstance* instance, dmVMath::Point3 position, dmVMath::Vector3 direction, float scale)
    {
        const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
        if (!prototype)
        {
            return;
        }

        LightSTD140& light_std140 = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
        FillLightInstanceSTD140(prototype, position, direction, scale, &light_std140);

        InvalidateLightBuffer(render_context);
    }

    static inline void CommitLightInfo(HRenderContext render_context)
    {
        InvalidateLightBuffer(render_context);
    }

    void InvalidateLightBuffer(HRenderContext render_context)
    {
        ++render_context->m_LightBufferVersion;

        // Version zero is reserved for a buffer which has never been uploaded.
        // Handle the extremely unlikely wrap without treating old data as current.
        if (render_context->m_LightBufferVersion == 0)
        {
            render_context->m_LightBufferVersion = 1;
            for (uint32_t i = 0; i < render_context->m_LightUniformBuffers.Size(); ++i)
            {
                render_context->m_LightUniformBuffers[i].m_Version = 0;
            }
        }
    }

    static uint32_t CompactLightBufferScratch(HRenderContext render_context)
    {
        render_context->m_LightBufferUploadScratch.SetSize(0);

        uint32_t allocated_light_count = render_context->m_RenderLightsIndices.Size();
        if (render_context->m_LightBufferUploadScratch.Capacity() < allocated_light_count)
        {
            render_context->m_LightBufferUploadScratch.SetCapacity(allocated_light_count);
        }

        dmVMath::Vector3 ambient_light(0.0f, 0.0f, 0.0f);
        uint32_t render_light_count = render_context->m_RenderLights.Size();
        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            const LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0 || !render_context->m_LightBufferSubmitted[i])
            {
                continue;
            }

            const LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
            LightType type = (LightType) (uint32_t) light.m_Params.getX();
            if (type == LIGHT_TYPE_AMBIENT)
            {
                ambient_light += dmVMath::Vector3(light.m_Color.getXYZ()) * light.m_Params.getY();
            }
            else
            {
                render_context->m_LightBufferUploadScratch.Push(light);
            }
        }

        render_context->m_AmbientLight = ambient_light;
        return render_context->m_LightBufferUploadScratch.Size();
    }

    static void WriteLightInstanceData(HRenderContext render_context, LightUniformBuffer* light_buffer)
    {
        uint32_t active_light_count = CompactLightBufferScratch(render_context);
        uint32_t upload_light_count = dmMath::Min(active_light_count, (uint32_t) light_buffer->m_Capacity);

        dmVMath::Vector4 info(render_context->m_AmbientLight, (float) upload_light_count);
        dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext,
                                     light_buffer->m_Buffer,
                                     render_context->m_LightBufferInfoWriteStart,
                                     sizeof(info),
                                     &info);

        // Write compacted light data from the scratch buffer. The shader loops
        // over [0..light_info.w). Both the count and upload are clamped to this
        // program's declared array capacity.
        if (upload_light_count > 0)
        {
            uint32_t write_size = upload_light_count * sizeof(LightSTD140);
            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext,
                                         light_buffer->m_Buffer,
                                         render_context->m_LightBufferDataWriteStart,
                                         write_size,
                                         render_context->m_LightBufferUploadScratch.Begin());
        }

        light_buffer->m_Version = render_context->m_LightBufferVersion;
    }

    static LightUniformBuffer* EnsureLightUniformBuffer(HRenderContext render_context, uint16_t capacity)
    {
        for (uint32_t i = 0; i < render_context->m_LightUniformBuffers.Size(); ++i)
        {
            if (render_context->m_LightUniformBuffers[i].m_Capacity == capacity)
            {
                return &render_context->m_LightUniformBuffers[i];
            }
        }

        return GenerateUniformBuffer(render_context, capacity);
    }

    void SetLightBufferCount(HRenderContext render_context, uint32_t max_lights)
    {
        assert(render_context);
        assert(render_context->m_RenderLightsIndices.Size() == 0);

        for (uint32_t i = 0; i < render_context->m_LightUniformBuffers.Size(); ++i)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffers[i].m_Buffer);
        }
        render_context->m_LightUniformBuffers.SetSize(0);

        if (max_lights > UINT16_MAX)
        {
            dmLogWarning("The max light count is limited to %u; clamping the requested value %u.", (uint32_t) UINT16_MAX, max_lights);
            max_lights = UINT16_MAX;
        }

        render_context->m_MaxLightCount               = (uint16_t) max_lights;
        render_context->m_LightBufferInfoWriteStart   = 0;
        render_context->m_LightBufferDataWriteStart   = 0;
        render_context->m_LightBufferVersion          = 1;
        render_context->m_AmbientLight                = dmVMath::Vector3(0.0f, 0.0f, 0.0f);

        // These offsets are invariant with capacity and are part of the public ABI.
        uint32_t abi_size = 0;
        GetLightBufferLayout(1, &abi_size, &render_context->m_LightBufferInfoWriteStart, &render_context->m_LightBufferDataWriteStart);
        assert(abi_size == LIGHT_BUFFER_HEADER_SIZE + LIGHT_BUFFER_LIGHT_STRIDE);
        assert(render_context->m_LightBufferInfoWriteStart == 0);
        assert(render_context->m_LightBufferDataWriteStart == LIGHT_BUFFER_HEADER_SIZE);

        if (render_context->m_RenderLightsIndices.Capacity() < max_lights)
        {
            render_context->m_RenderLightsIndices.SetCapacity(max_lights);
        }
        render_context->m_RenderLightsIndices.Clear();

        uint32_t old_light_count = dmMath::Min(render_context->m_RenderLights.Size(), max_lights);
        render_context->m_RenderLights.EnsureSize(max_lights);
        render_context->m_RenderLights.SetSize(max_lights);
        for (uint32_t i = old_light_count; i < max_lights; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            instance->m_LightPrototype = 0;
            instance->m_LightBufferIndex = (uint16_t) i;
            instance->m_Version = 0;
        }

        render_context->m_LightBufferScratch.SetCapacity(max_lights);
        render_context->m_LightBufferScratch.SetSize(0);
        render_context->m_LightBufferUploadScratch.SetSize(0);
        render_context->m_LightBufferUploadScratch.SetCapacity(0);
        if (render_context->m_LightBufferSubmitted.Capacity() < max_lights)
        {
            render_context->m_LightBufferSubmitted.SetCapacity(max_lights);
        }
        render_context->m_LightBufferSubmitted.SetSize(max_lights);
        memset(render_context->m_LightBufferSubmitted.Begin(), 0, max_lights);
    }

    void FinalizeLightData(HRenderContext render_context)
    {
        for (uint32_t i = 0; i < render_context->m_LightUniformBuffers.Size(); ++i)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffers[i].m_Buffer);
        }
        render_context->m_LightUniformBuffers.SetSize(0);

        uint32_t prototype_capacity = render_context->m_LightPrototypes.Capacity();
        for (uint32_t i = 0; i < prototype_capacity; ++i)
        {
            LightPrototype* prototype = render_context->m_LightPrototypes.GetByIndex(i);
            if (prototype)
            {
                render_context->m_LightPrototypes.Release(render_context->m_LightPrototypes.IndexToHandle(i));
                delete prototype;
            }
        }
    }

    struct LightBufferBindingCallbackContext
    {
        RenderContext* m_Context;
        bool           m_HasLightBuffer;
        uint16_t       m_Set;
        uint16_t       m_Binding;
        uint16_t       m_Capacity;
    };

    static dmGraphics::UniformBufferLayout GetShaderLightBufferLayout(const dmGraphics::ShaderResourceTypeInfo* types, uint32_t num_types, uint32_t root_type_index, uint32_t lights_member_index)
    {
        dmGraphics::ShaderResourceTypeInfo* type_infos = (dmGraphics::ShaderResourceTypeInfo*) dmAlloca(sizeof(dmGraphics::ShaderResourceTypeInfo) * num_types);
        memcpy(type_infos, types, sizeof(dmGraphics::ShaderResourceTypeInfo) * num_types);

        const dmGraphics::ShaderResourceTypeInfo& root_type = types[root_type_index];
        dmGraphics::ShaderResourceMember* root_members = (dmGraphics::ShaderResourceMember*) dmAlloca(sizeof(dmGraphics::ShaderResourceMember) * root_type.m_MemberCount);
        memcpy(root_members, root_type.m_Members, sizeof(dmGraphics::ShaderResourceMember) * root_type.m_MemberCount);
        root_members[lights_member_index].m_ElementCount = 1;
        type_infos[root_type_index].m_Members = root_members;

        return dmGraphics::GetUniformBufferLayout(root_type_index, type_infos, num_types);
    }

    static void LightBufferBindingCallback(uint16_t set, uint16_t binding, const dmGraphics::ShaderResourceTypeInfo* types, uint32_t num_types, uint32_t root_type_index, dmGraphics::UniformBufferLayout* layout, void* user_data)
    {
        LightBufferBindingCallbackContext* cb_ctx = (LightBufferBindingCallbackContext*) user_data;
        const dmGraphics::ShaderResourceTypeInfo* root_type = &types[root_type_index];

        if (cb_ctx->m_HasLightBuffer || root_type->m_NameHash != LIGHT_BUFFER_TYPE)
        {
            return;
        }

        uint32_t ubo_light_count = 0;
        uint32_t lights_member_index = UINT32_MAX;
        for (uint32_t i = 0; i < root_type->m_MemberCount; ++i)
        {
            if (root_type->m_Members[i].m_NameHash == LIGHT_MEMBER_TYPE)
            {
                ubo_light_count = root_type->m_Members[i].m_ElementCount;
                lights_member_index = i;
                break;
            }
        }

        if (ubo_light_count == 0)
        {
            dmLogOnceWarning("The light buffer must declare a lights array with at least one element.");
            return;
        }

        if (ubo_light_count > cb_ctx->m_Context->m_MaxLightCount)
        {
            dmLogOnceWarning("The light buffer lights array is larger than the max light count in the project configuration.");
            return;
        }

        dmGraphics::UniformBufferLayout light_buffer_layout = GetLightBufferLayout(1, 0, 0, 0);
        if (GetShaderLightBufferLayout(types, num_types, root_type_index, lights_member_index) != light_buffer_layout)
        {
            dmLogOnceWarning("The light buffer must use the built-in LightBuffer layout.");
            return;
        }

        *layout = light_buffer_layout;

        cb_ctx->m_HasLightBuffer = true;
        cb_ctx->m_Set            = set;
        cb_ctx->m_Binding        = binding;
        cb_ctx->m_Capacity       = (uint16_t) ubo_light_count;
    }

    void GetProgramLightBufferBinding(HRenderContext render_context, dmGraphics::HProgram program, bool* out_has_light_buffer, uint16_t* out_set, uint16_t* out_binding, uint16_t* out_capacity)
    {
        LightBufferBindingCallbackContext cb_ctx;
        cb_ctx.m_Context         = render_context;
        cb_ctx.m_HasLightBuffer  = false;
        cb_ctx.m_Set             = 0;
        cb_ctx.m_Binding         = 0;
        cb_ctx.m_Capacity        = 0;

        dmGraphics::IterateProgramResourceBindings(program, dmGraphics::BINDING_FAMILY_UNIFORM_BUFFER, LightBufferBindingCallback, &cb_ctx);

        *out_has_light_buffer = cb_ctx.m_HasLightBuffer;
        *out_capacity         = cb_ctx.m_Capacity;
        if (cb_ctx.m_HasLightBuffer)
        {
            *out_set     = cb_ctx.m_Set;
            *out_binding = cb_ctx.m_Binding;
        }
    }

    static void ApplyLightBufferForBinding(HRenderContext render_context, uint16_t light_buffer_set, uint16_t light_buffer_binding, uint16_t light_buffer_capacity)
    {
        LightUniformBuffer* light_buffer = EnsureLightUniformBuffer(render_context, light_buffer_capacity);
        if (!light_buffer)
        {
            return;
        }

        if (light_buffer->m_Version != render_context->m_LightBufferVersion)
        {
            WriteLightInstanceData(render_context, light_buffer);
        }

        dmGraphics::EnableUniformBuffer(render_context->m_GraphicsContext,
                                        light_buffer->m_Buffer,
                                        light_buffer_set,
                                        light_buffer_binding);
    }

    static inline void UnbindLightBuffer(HRenderContext render_context)
    {
        for (uint32_t i = 0; i < render_context->m_LightUniformBuffers.Size(); ++i)
        {
            dmGraphics::DisableUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffers[i].m_Buffer);
        }
    }

    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (!material->m_HasLightBuffer)
        {
            UnbindLightBuffer(render_context);
            return;
        }
        ApplyLightBufferForBinding(render_context, material->m_LightBufferSet, material->m_LightBufferBinding, material->m_LightBufferCapacity);
    }

    void ApplyComputeProgramLightBuffers(HRenderContext render_context, HComputeProgram compute_program)
    {
        if (!compute_program->m_HasLightBuffer)
        {
            UnbindLightBuffer(render_context);
            return;
        }
        ApplyLightBufferForBinding(render_context, compute_program->m_LightBufferSet, compute_program->m_LightBufferBinding, compute_program->m_LightBufferCapacity);
    }
}
