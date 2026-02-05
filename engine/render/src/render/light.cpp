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

// Access internal graphics program/binding information
#include "../../../graphics/src/graphics_private.h"

namespace dmRender
{
    static const dmhash_t LIGHT_BUFFER_TYPE = dmHashString64("LightBuffer");
    static struct
    {
        dmGraphics::ShaderResourceMember   m_LightBufferMembers[2];
        dmGraphics::ShaderResourceMember   m_LightMembers[4];
        dmGraphics::ShaderResourceTypeInfo m_LightTypes[2];
        dmGraphics::UniformBufferLayout    m_LightBufferLayout;
        bool                               m_Initialized;
    } g_LightBufferContext;

    ////////////////////////////////
    // Light prototype
    ////////////////////////////////

    LightParams::LightParams()
    : m_Type(LIGHT_TYPE_POINT)
    , m_Color(1.0f, 1.0f, 1.0f, 1.0f)
    , m_Direction(0.0f, 0.0f, -1.0f)
    , m_Intensity(1.0f)
    , m_Range(10.0f)
    , m_InnerConeAngle(0.0f)
    , m_OuterConeAngle(3.1415926535f / 4.0f)
    {
    }

    HLight NewLight(HRenderContext render_context, const LightParams& params)
    {
        Light* l = new Light;
        memset(l, 0, sizeof(Light));

        switch(params.m_Type)
        {
        case LIGHT_TYPE_DIRECTIONAL:
            {
                l->m_Type = params.m_Type;
                l->m_Color = params.m_Color;
                l->m_Intensity = params.m_Intensity;
                l->m_Direction = params.m_Direction;
            } break;
        case LIGHT_TYPE_POINT:
            {
                l->m_Type = params.m_Type;
                l->m_Color = params.m_Color;
                l->m_Intensity = params.m_Intensity;
                l->m_Range = params.m_Range;
            } break;
        case LIGHT_TYPE_SPOT:
            {
                l->m_Type = params.m_Type;
                l->m_Color = params.m_Color;
                l->m_Intensity = params.m_Intensity;
                l->m_Range = params.m_Range;
                l->m_InnerConeAngle = params.m_InnerConeAngle;
                l->m_OuterConeAngle = params.m_OuterConeAngle;
            } break;
        }
        return l;
    }

    void DeleteLight(HRenderContext render_context, HLight light)
    {
        delete light;
    }

    ////////////////////////////////
    // Light instance
    ////////////////////////////////

    struct LightSTD140
    {
        dmVMath::Vector4 m_Position;
        dmVMath::Vector4 m_Color;
        dmVMath::Vector4 m_DirectionRange;
        dmVMath::Vector4 m_Params;
    };

    static void CommitLightInstance(HRenderContext render_context, LightInstance* light_instance)
    {
        LightSTD140 light_std140;
        light_std140.m_Position = dmVMath::Vector4(light_instance->m_Position);
        light_std140.m_Color = light_instance->m_LightPrototype->m_Color;
        light_std140.m_DirectionRange = dmVMath::Vector4(
            light_instance->m_LightPrototype->m_Direction,
            light_instance->m_LightPrototype->m_Range);
        light_std140.m_Params = dmVMath::Vector4(
            light_instance->m_LightPrototype->m_Type,
            light_instance->m_LightPrototype->m_Intensity,
            light_instance->m_LightPrototype->m_InnerConeAngle,
            light_instance->m_LightPrototype->m_OuterConeAngle);

        uint32_t write_offset = sizeof(LightSTD140) * light_instance->m_LightBufferIndex;
        dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, write_offset, sizeof(LightSTD140), &light_std140);
    }

    static void CommitLightCount(HRenderContext render_context)
    {
        float count = render_context->m_RenderLightsIndices.Size();
        uint32_t write_offset = g_LightBufferContext.m_LightBufferMembers[1].m_Offset;
        dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, write_offset, sizeof(float), &count);
    }

    HLightInstance NewLightInstance(HRenderContext render_context, HLight light_prototype)
    {
        if (render_context->m_RenderLightsIndices.Remaining() == 0)
        {
            dmLogError("Max lights reached.");
            return 0;
        }

        if (render_context->m_RenderLights.Full())
        {
            render_context->m_RenderLights.Allocate(4);
        }

        LightInstance* light_instance      = new LightInstance;
        light_instance->m_Position         = dmVMath::Point3();
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

    void SetLightInstance(HRenderContext render_context, HLightInstance instance, dmVMath::Point3 position, dmVMath::Quat rotation)
    {
        LightInstance* light_instance = render_context->m_RenderLights.Get(instance);
        if (!light_instance)
            return;
        light_instance->m_Position = position;
        light_instance->m_Rotation = rotation;

        CommitLightInstance(render_context, light_instance);

        /*
        dmVMath::Vector3 light_dir = dmVMath::Rotate(rotation, dmVMath::Vector3(0.0f, 0.0f, -1.0f));
        */
    }

    ////////////////////////////////
    // Light utility
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
            Light lights[MAX_LIGHTS];
            vec4  lights_count;   // x: number of active lights
        };
        */

        if (g_LightBufferContext.m_Initialized)
        {
            return;
        }

        // lights
        g_LightBufferContext.m_LightBufferMembers[0].m_Name                 = (char*)"lights";
        g_LightBufferContext.m_LightBufferMembers[0].m_NameHash             = dmHashString64("lights");
        g_LightBufferContext.m_LightBufferMembers[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        g_LightBufferContext.m_LightBufferMembers[0].m_ElementCount         = max_lights;
        g_LightBufferContext.m_LightBufferMembers[0].m_Type.m_TypeIndex     = 1; // index into ShaderResourceTypeInfo[]
        g_LightBufferContext.m_LightBufferMembers[0].m_Type.m_UseTypeIndex  = 1;
        // lights_count (vec4)
        g_LightBufferContext.m_LightBufferMembers[1].m_Name                 = (char*)"lights_count";
        g_LightBufferContext.m_LightBufferMembers[1].m_NameHash             = dmHashString64("lights_count");
        g_LightBufferContext.m_LightBufferMembers[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        g_LightBufferContext.m_LightBufferMembers[1].m_ElementCount         = 1;

        // vec4 position
        g_LightBufferContext.m_LightMembers[0].m_Name                 = (char*)"position";
        g_LightBufferContext.m_LightMembers[0].m_NameHash             = dmHashString64("position");
        g_LightBufferContext.m_LightMembers[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        g_LightBufferContext.m_LightMembers[0].m_ElementCount         = 1;
        // vec4 color
        g_LightBufferContext.m_LightMembers[1].m_Name                 = (char*)"color";
        g_LightBufferContext.m_LightMembers[1].m_NameHash             = dmHashString64("color");
        g_LightBufferContext.m_LightMembers[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        g_LightBufferContext.m_LightMembers[1].m_ElementCount         = 1;
        // vec4 direction (xyz: direction, w: range)
        g_LightBufferContext.m_LightMembers[2].m_Name                 = (char*)"direction_range";
        g_LightBufferContext.m_LightMembers[2].m_NameHash             = dmHashString64("direction_range");
        g_LightBufferContext.m_LightMembers[2].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        g_LightBufferContext.m_LightMembers[2].m_ElementCount         = 1;
        // vec4 params (x: type, y: intensity, z: innerConeAngle, w: outerConeAngle)
        g_LightBufferContext.m_LightMembers[3].m_Name                 = (char*)"params";
        g_LightBufferContext.m_LightMembers[3].m_NameHash             = dmHashString64("params");
        g_LightBufferContext.m_LightMembers[3].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        g_LightBufferContext.m_LightMembers[3].m_ElementCount         = 1;

        // LightBuffer (index 1)
        g_LightBufferContext.m_LightTypes[0].m_Name        = (char*)"LightBuffer";
        g_LightBufferContext.m_LightTypes[0].m_NameHash    = dmHashString64("LightBuffer");
        g_LightBufferContext.m_LightTypes[0].m_Members     = g_LightBufferContext.m_LightBufferMembers;
        g_LightBufferContext.m_LightTypes[0].m_MemberCount = DM_ARRAY_SIZE(g_LightBufferContext.m_LightBufferMembers);
        // Light (index 0)
        g_LightBufferContext.m_LightTypes[1].m_Name        = (char*)"Light";
        g_LightBufferContext.m_LightTypes[1].m_NameHash    = dmHashString64("Light");
        g_LightBufferContext.m_LightTypes[1].m_Members     = g_LightBufferContext.m_LightMembers;
        g_LightBufferContext.m_LightTypes[1].m_MemberCount = DM_ARRAY_SIZE(g_LightBufferContext.m_LightMembers);

        dmGraphics::UpdateShaderTypesOffsets(g_LightBufferContext.m_LightTypes, DM_ARRAY_SIZE(g_LightBufferContext.m_LightTypes));
        // Root type for the uniform buffer is "LightBuffer" (index 0)
        dmGraphics::GetUniformBufferLayout(0, g_LightBufferContext.m_LightTypes, DM_ARRAY_SIZE(g_LightBufferContext.m_LightTypes), &g_LightBufferContext.m_LightBufferLayout);

        g_LightBufferContext.m_Initialized = true;
    }

    // TODO: Rename
    void InitializeLightBuffer(HRenderContext render_context, uint32_t max_light_count)
    {
        GenerateDefaultUniformBufferLayout(max_light_count);

        render_context->m_MaxLightCount = max_light_count;
        render_context->m_LightUniformBuffer = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, g_LightBufferContext.m_LightBufferLayout);
        render_context->m_RenderLightsIndices.SetCapacity(max_light_count);
    }

    // TODO: We should be able to cache this in the material itself
    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (!render_context || !material || !material->m_Program || !render_context->m_LightUniformBuffer)
            return;

        dmGraphics::Program* program = (dmGraphics::Program*) material->m_Program;
        dmGraphics::ProgramResourceBindingIterator it(program);

        const dmGraphics::ProgramResourceBinding* binding;
        while ((binding = it.Next()))
        {
            if (!binding->m_Res)
            {
                continue;
            }

            // We only care about uniform buffer bindings.
            if (binding->m_Res->m_BindingFamily != dmGraphics::BINDING_FAMILY_UNIFORM_BUFFER)
            {
                continue;
            }

            // Fetch the root type for this uniform buffer and check that it is a LightBuffer.
            const dmArray<dmGraphics::ShaderResourceTypeInfo>& type_infos = *binding->m_TypeInfos;
            uint32_t root_type_index = binding->m_Res->m_Type.m_TypeIndex;
            if (root_type_index >= type_infos.Size())
            {
                continue;
            }

            const dmGraphics::ShaderResourceTypeInfo& root_type = type_infos[root_type_index];
            if (root_type.m_NameHash != LIGHT_BUFFER_TYPE)
            {
                continue;
            }

            // The binding user data for uniform buffers is a UniformBufferLayout*
            const dmGraphics::UniformBufferLayout* binding_layout = (const dmGraphics::UniformBufferLayout*) binding->m_BindingUserData;
            if (!binding_layout)
            {
                continue;
            }

            // Only bind if the layouts match (size + hash).
            if (binding_layout->m_Size != g_LightBufferContext.m_LightBufferLayout.m_Size ||
                binding_layout->m_Hash != g_LightBufferContext.m_LightBufferLayout.m_Hash)
            {
                continue;
            }

            // Layout and type match – bind the shared light uniform buffer for this set/binding.
            dmGraphics::EnableUniformBuffer(render_context->m_GraphicsContext,
                                            render_context->m_LightUniformBuffer,
                                            binding->m_Res->m_Set,
                                            binding->m_Res->m_Binding);
        }
    }
}
