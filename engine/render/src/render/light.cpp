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

namespace dmRender
{
    static struct
    {
        dmGraphics::UniformBufferLayout m_LightBufferLayout;
        uint32_t                        m_LightBufferDataWriteStart;
        uint32_t                        m_MaxLightCount;
        bool                            m_Initialized;
    } g_LightBufferContext;

    static void ResizeLightBuffer(HRenderContext render_context, uint32_t max_light_count);
    static void CommitLightInstance(HRenderContext render_context, LightInstance* instance);
    static void CommitLightCount(HRenderContext render_context);

    ////////////////////////////////
    // Light prototype
    ////////////////////////////////

    LightPrototypeParams::LightPrototypeParams()
    : m_Type(LIGHT_TYPE_POINT)
    , m_Color(1.0f, 1.0f, 1.0f, 1.0f)
    , m_Direction(0.0f, 0.0f, -1.0f)
    , m_Intensity(1.0f)
    , m_Range(10.0f)
    , m_InnerConeAngle(0.0f)
    , m_OuterConeAngle(3.1415926535f / 4.0f)
    {
    }

    HLightPrototype NewLightPrototype(HRenderContext render_context, const LightPrototypeParams& params)
    {
        LightPrototype* lp = new LightPrototype;
        memset(lp, 0, sizeof(LightPrototype));
        lp->m_Type = params.m_Type;
        lp->m_Color = params.m_Color;
        lp->m_Intensity = params.m_Intensity;
        lp->m_Direction = params.m_Direction;
        lp->m_Range = params.m_Range;
        lp->m_InnerConeAngle = params.m_InnerConeAngle;
        lp->m_OuterConeAngle = params.m_OuterConeAngle;
        return lp;
    }

    void DeleteLightPrototype(HRenderContext render_context, HLightPrototype light_prototype)
    {
        delete light_prototype;
    }

    ////////////////////////////////
    // Light instance
    ////////////////////////////////

    HLightInstance NewLightInstance(HRenderContext render_context, HLightPrototype light_prototype)
    {
        const uint32_t lights_to_add = 32;

        if (render_context->m_RenderLightsIndices.Remaining() == 0)
        {
            render_context->m_RenderLightsIndices.SetCapacity(render_context->m_RenderLightsIndices.Capacity() + lights_to_add);
        }
        if (render_context->m_RenderLights.Full())
        {
            render_context->m_RenderLights.Allocate(lights_to_add);
        }
        uint32_t scratch_size = render_context->m_RenderLightsIndices.Capacity();
        if (render_context->m_LightBufferScratch.Capacity() < scratch_size)
        {
            render_context->m_LightBufferScratch.SetCapacity(scratch_size);
            render_context->m_LightBufferScratch.SetSize(scratch_size);
        }

        LightInstance* light_instance      = new LightInstance;
        light_instance->m_Position         = dmVMath::Point3();
        light_instance->m_Direction        = light_prototype->m_Direction;
        light_instance->m_LightPrototype   = light_prototype;
        light_instance->m_LightBufferIndex = render_context->m_RenderLightsIndices.Pop();

        CommitLightInstance(render_context, light_instance);
        CommitLightCount(render_context);

        return render_context->m_RenderLights.Put(light_instance);
    }

    void DeleteLightInstance(HRenderContext render_context, HLightInstance instance)
    {
        LightInstance* light_instance = render_context->m_RenderLights.Get(instance);
        if (light_instance)
        {
            render_context->m_RenderLightsIndices.Push(light_instance->m_LightBufferIndex);
            render_context->m_RenderLights.Release(instance);
            delete light_instance;
            CommitLightCount(render_context);
        }
    }

    static inline bool Vector3Equals(const dmVMath::Vector3& a, const dmVMath::Vector3& b)
    {
        const float eps = 1e-4f;
        dmVMath::Vector3 diff = a - b;
        float dist2 = dmVMath::LengthSqr(diff);
        return dist2 <= eps * eps;
    }

    void SetLightInstance(HRenderContext render_context, HLightInstance instance, dmVMath::Point3 position, dmVMath::Quat rotation)
    {
        LightInstance* light_instance = render_context->m_RenderLights.Get(instance);
        if (!light_instance)
            return;

        dmVMath::Vector3 direction = dmVMath::Rotate(rotation, light_instance->m_LightPrototype->m_Direction);
        bool needs_commit = !Vector3Equals(dmVMath::Vector3(position), dmVMath::Vector3(light_instance->m_Position)) || !Vector3Equals(direction, light_instance->m_Direction);

        light_instance->m_Position = position;
        light_instance->m_Direction = direction;

        if (needs_commit)
        {
            CommitLightInstance(render_context, light_instance);
        }
    }

    ////////////////////////////////
    // Light buffer
    ////////////////////////////////

    static void GenerateDefaultUniformBufferLayout(int max_lights)
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
            vec4  lights_count;   // x: number of active lights
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

        // lights_count (vec4)
        light_buffer_members[0].m_Name                 = (char*)"lights_count";
        light_buffer_members[0].m_NameHash             = dmHashString64("lights_count");
        light_buffer_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_buffer_members[0].m_Type.m_UseTypeIndex  = 0;
        light_buffer_members[0].m_ElementCount         = 1;
        // lights
        light_buffer_members[1].m_Name                 = (char*)"lights";
        light_buffer_members[1].m_NameHash             = dmHashString64("lights");
        light_buffer_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_buffer_members[1].m_ElementCount         = max_lights;
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

        // Generate a new layout for the light buffer
        dmGraphics::UpdateShaderTypesOffsets(light_types, DM_ARRAY_SIZE(light_types));
        dmGraphics::GetUniformBufferLayout(0, light_types, DM_ARRAY_SIZE(light_types), &g_LightBufferContext.m_LightBufferLayout);

        g_LightBufferContext.m_Initialized = true;
        g_LightBufferContext.m_MaxLightCount = max_lights;
        g_LightBufferContext.m_LightBufferDataWriteStart = light_buffer_members[1].m_Offset;
    }

    static inline void CommitLightInstance(HRenderContext render_context, LightInstance* instance)
    {
        LightSTD140& light_std140 = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
        light_std140.m_Position = dmVMath::Vector4(instance->m_Position);
        light_std140.m_Color = instance->m_LightPrototype->m_Color;
        light_std140.m_DirectionRange = dmVMath::Vector4(
            instance->m_Direction,
            instance->m_LightPrototype->m_Range);
        light_std140.m_Params = dmVMath::Vector4(
            instance->m_LightPrototype->m_Type,
            instance->m_LightPrototype->m_Intensity,
            instance->m_LightPrototype->m_InnerConeAngle,
            instance->m_LightPrototype->m_OuterConeAngle);

        // Mark dirty range
        render_context->m_LightBufferDirtyStart = dmMath::Min(render_context->m_LightBufferDirtyStart, instance->m_LightBufferIndex);
        render_context->m_LightBufferDirtyEnd = dmMath::Max(render_context->m_LightBufferDirtyEnd, (uint16_t) (instance->m_LightBufferIndex + 1));
    }

    static inline void CommitLightCount(HRenderContext render_context)
    {
        render_context->m_LightBufferDirtyCount = 1;
    }

    static void WriteLightInstanceData(HRenderContext render_context)
    {
        if (!render_context->m_LightUniformBuffer)
        {
            uint32_t min_size = dmMath::Max(1u, render_context->m_LightBufferScratch.Size());
            ResizeLightBuffer(render_context, min_size);
        }

        // The light count has changed, we need to write that separately
        if (render_context->m_LightBufferDirtyCount)
        {
            float count = (float) render_context->m_RenderLightsIndices.Size();
            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, 0, sizeof(float), &count);
        }

        // Write the light data from the scratch buffer
        if (render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart)
        {
            // Convert indices to byte offsets
            uint32_t write_light_data_start = g_LightBufferContext.m_LightBufferDataWriteStart;
            uint32_t write_start            = write_light_data_start + render_context->m_LightBufferDirtyStart * sizeof(LightSTD140);
            uint32_t write_end              = write_light_data_start + render_context->m_LightBufferDirtyEnd * sizeof(LightSTD140);
            uint32_t write_size             = write_end - write_start;
            LightSTD140* write_ptr_start    = &render_context->m_LightBufferScratch[render_context->m_LightBufferDirtyStart];

            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, write_start, write_size, (void*) write_ptr_start);
        }

        // Reset all dirty flags
        render_context->m_LightBufferDirtyStart = render_context->m_LightBufferScratch.Size();
        render_context->m_LightBufferDirtyEnd   = 0;
        render_context->m_LightBufferDirtyCount = 0;
    }

    static inline bool IsLightBufferDirty(HRenderContext render_context)
    {
        return render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart || render_context->m_LightBufferDirtyCount;
    }

    static void ResizeLightBuffer(HRenderContext render_context, uint32_t max_light_count)
    {
        if (render_context->m_LightBufferScratch.Capacity() < max_light_count)
        {
            render_context->m_LightBufferScratch.SetCapacity(max_light_count);
            render_context->m_LightBufferScratch.SetSize(max_light_count);
        }

        if (!g_LightBufferContext.m_Initialized)
        {
            GenerateDefaultUniformBufferLayout(max_light_count);
        }
        else if (max_light_count != g_LightBufferContext.m_MaxLightCount)
        {
            dmLogOnceWarning("Using different sizes of light buffers is not supported. You should use the same size everywhere for the uniform buffer!");
        }

        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
        }
        render_context->m_LightUniformBuffer = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, g_LightBufferContext.m_LightBufferLayout);

        // Dirty the entire range of lights
        render_context->m_LightBufferDirtyStart = 0;
        render_context->m_LightBufferDirtyEnd   = render_context->m_LightBufferScratch.Size();
    }

    void InitializeLightData(HRenderContext render_context)
    {
        render_context->m_RenderLightsIndices.SetCapacity(4);
    }

    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (material->m_HasLightBuffer)
        {
            // The lightbuffer needs to match the material
            if (material->m_LightBufferLightsCount != render_context->m_LightBufferScratch.Size())
            {
                ResizeLightBuffer(render_context, material->m_LightBufferLightsCount);
            }

            if (IsLightBufferDirty(render_context))
            {
                WriteLightInstanceData(render_context);
            }

            dmGraphics::EnableUniformBuffer(render_context->m_GraphicsContext,
                                            render_context->m_LightUniformBuffer,
                                            material->m_LightBufferSet,
                                            material->m_LightBufferBinding);
        }
    }
}
