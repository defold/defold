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

#include <math.h>

#include "render.h"
#include "render_private.h"

namespace dmRender
{
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
    , m_OuterConeAngle(M_PI_4)
    {
    }

    HLightPrototype NewLightPrototype(HRenderContext render_context, const LightPrototypeParams& params)
    {
        LightPrototype* lp = new LightPrototype;
        SetLightPrototype(render_context, lp, params);
        return lp;
    }

    void SetLightPrototype(HRenderContext render_context, HLightPrototype light_prototype, const LightPrototypeParams& params)
    {
        LightPrototype* lp = (LightPrototype*) light_prototype;
        memset(lp, 0, sizeof(LightPrototype));
        lp->m_Type = params.m_Type;
        lp->m_Color = params.m_Color;
        lp->m_Intensity = params.m_Intensity;
        lp->m_Direction = params.m_Direction;
        lp->m_Range = params.m_Range;
        lp->m_InnerConeAngle = params.m_InnerConeAngle;
        lp->m_OuterConeAngle = params.m_OuterConeAngle;
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
        // Reached max count.
        if (render_context->m_RenderLightsIndices.Remaining() == 0)
        {
            return 0;
        }

        if (render_context->m_RenderLights.Full())
        {
            render_context->m_RenderLights.Allocate(32);
        }

        LightInstance* light_instance      = new LightInstance;
        light_instance->m_Position         = dmVMath::Point3();
        light_instance->m_Direction        = light_prototype->m_Direction;
        light_instance->m_LightPrototype   = light_prototype;
        light_instance->m_LightBufferIndex = render_context->m_RenderLightsIndices.Pop();

        if (light_instance->m_LightBufferIndex >= render_context->m_LightBufferScratch.Size())
        {
            render_context->m_LightBufferScratch.SetSize(light_instance->m_LightBufferIndex+1);
        }

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
        {
            return;
        }

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

    static void GenerateUniformBuffer(HRenderContext render_context, int max_lights)
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

        dmGraphics::UniformBufferLayout layout;
        dmGraphics::UpdateShaderTypesOffsets(light_types, DM_ARRAY_SIZE(light_types));
        dmGraphics::GetUniformBufferLayout(0, light_types, DM_ARRAY_SIZE(light_types), &layout);

        render_context->m_LightUniformBuffer = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, layout);
        render_context->m_LightBufferDataWriteStart = light_buffer_members[1].m_Offset;
    }

    static inline void CommitLightInstance(HRenderContext render_context, LightInstance* instance)
    {
        const LightPrototype* prototype = instance->m_LightPrototype;

        LightSTD140& light_std140 = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
        light_std140.m_Position = dmVMath::Vector4(instance->m_Position);
        light_std140.m_Color    = prototype->m_Color;

        dmVMath::Vector3 direction(0.0f, 0.0f, 0.0f);
        float range      = 0.0f;
        float inner_cone = 0.0f;
        float outer_cone = 0.0f;

        switch (prototype->m_Type)
        {
        case LIGHT_TYPE_DIRECTIONAL:
            direction = instance->m_Direction;
            break;
        case LIGHT_TYPE_POINT:
            range = prototype->m_Range;
            break;
        case LIGHT_TYPE_SPOT:
            direction  = instance->m_Direction;
            range      = prototype->m_Range;
            inner_cone = prototype->m_InnerConeAngle;
            outer_cone = prototype->m_OuterConeAngle;
            break;
        default:
            assert("Light type not supported!");
            break;
        }

        light_std140.m_DirectionRange = dmVMath::Vector4(direction, range);
        light_std140.m_Params = dmVMath::Vector4((float) prototype->m_Type, prototype->m_Intensity, inner_cone, outer_cone);

        // Mark dirty range
        render_context->m_LightBufferDirtyStart = dmMath::Min(render_context->m_LightBufferDirtyStart, (uint32_t) instance->m_LightBufferIndex);
        render_context->m_LightBufferDirtyEnd = dmMath::Max(render_context->m_LightBufferDirtyEnd, (uint32_t) (instance->m_LightBufferIndex + 1));
    }

    static inline void CommitLightCount(HRenderContext render_context)
    {
        render_context->m_LightBufferDirtyCount = 1;
    }

    static void WriteLightInstanceData(HRenderContext render_context)
    {
        // The light count has changed, we need to write that separately
        if (render_context->m_LightBufferDirtyCount)
        {
            // ... but only if it is actually different from last write.
            float count = (float) render_context->m_RenderLightsIndices.Size();
            if (count != render_context->m_LightBufferLastWrittenCount)
            {
                dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, 0, sizeof(float), &count);
            }
            render_context->m_LightBufferLastWrittenCount = count;
        }

        // Write the light data from the scratch buffer
        if (render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart)
        {
            // Convert indices to byte offsets
            uint32_t write_light_data_start = render_context->m_LightBufferDataWriteStart;
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

    void InitializeLightData(HRenderContext render_context, uint32_t max_light_count)
    {
        render_context->m_MaxLightCount = max_light_count;
        render_context->m_LightUniformBuffer = 0;
        render_context->m_LightBufferDirtyStart  = 0;
        render_context->m_LightBufferDirtyEnd  = 0;
        render_context->m_LightBufferDirtyCount  = 0;
        render_context->m_LightBufferDataWriteStart  = 0;
        render_context->m_LightBufferLastWrittenCount = 0;

        if (max_light_count > 0)
        {
            render_context->m_RenderLightsIndices.SetCapacity(max_light_count);
            render_context->m_LightBufferScratch.SetCapacity(max_light_count);
            GenerateUniformBuffer(render_context, max_light_count);
        }
    }

    void FinalizeLightData(HRenderContext render_context)
    {
        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
        }
    }

    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (material->m_HasLightBuffer)
        {
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
