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

    static void GenerateDefaultUniformBufferLayout(int max_lights, UniformBufferLayout* out)
    {
        /*
        struct Light
        {
            int   type;            // 0 directional, 1 point, 2 spot (matches dmRender::LightType)
            vec4  color;           // RGBA (matches LightParams order)
            vec3  direction;       // normalized; directional/spot (4-byte padding in std140)
            float intensity;
            float range;
            float innerConeAngle;  // radians, spot only
            float outerConeAngle; // radians, spot only
        };
        uniform LightBuffer
        {
            Light lights[MAX_LIGHTS];
            int   lights_count;
        };
        */

        dmGraphics::ShaderResourceMember light_members[7] = {};
        // int type
        light_members[0].m_Name                 = (char*)"type";
        light_members[0].m_NameHash             = dmHashString64("type"); // TODO: Store statically outside of function
        light_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_INT;
        light_members[0].m_ElementCount         = 1;
        // vec4 color
        light_members[1].m_Name                 = (char*)"color";
        light_members[1].m_NameHash             = dmHashString64("color");
        light_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[1].m_ElementCount         = 1;
        // vec3 direction
        light_members[2].m_Name                 = (char*)"direction";
        light_members[2].m_NameHash             = dmHashString64("direction");
        light_members[2].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_VEC3;
        light_members[2].m_ElementCount         = 1;
        // float intensity
        light_members[3].m_Name                 = (char*)"intensity";
        light_members[3].m_NameHash             = dmHashString64("intensity");
        light_members[3].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_members[3].m_ElementCount         = 1;
        // float range
        light_members[4].m_Name                 = (char*)"range";
        light_members[4].m_NameHash             = dmHashString64("range");
        light_members[4].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_members[4].m_ElementCount         = 1;
        // float innerConeAngle
        light_members[5].m_Name                 = (char*)"innerConeAngle";
        light_members[5].m_NameHash             = dmHashString64("innerConeAngle");
        light_members[5].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_members[5].m_ElementCount         = 1;
        // float outerConeAngle
        light_members[6].m_Name                 = (char*)"outerConeAngle";
        light_members[6].m_NameHash             = dmHashString64("outerConeAngle");
        light_members[6].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_members[6].m_ElementCount         = 1;

        dmGraphics::ShaderResourceMember light_buffer_members[2] = {};
        // lights
        light_buffer_members[0].m_Name                 = (char*)"lights";
        light_buffer_members[0].m_NameHash             = dmHashString64("lights");
        light_buffer_members[0].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_FLOAT;
        light_buffer_members[0].m_ElementCount         = max_lights;
        light_buffer_members[0].m_Type.m_TypeIndex     = 0; // index into ShaderResourceTypeInfo[]
        light_buffer_members[0].m_Type.m_UseTypeIndex  = 1;
        // lights_count
        light_buffer_members[1].m_Name                 = (char*)"lights_count";
        light_buffer_members[1].m_NameHash             = dmHashString64("lights_count");
        light_buffer_members[1].m_Type.m_ShaderType    = dmGraphics::ShaderDesc::SHADER_TYPE_INT;
        light_buffer_members[1].m_ElementCount         = 1

        dmGraphics::ShaderResourceTypeInfo light_types[2];
        // Light (index 0)
        types[0].m_Name        = (char*)"Light";
        types[0].m_NameHash    = dmHashString64("Light");
        types[0].m_Members     = light_members;
        types[0].m_MemberCount = DM_ARRAY_SIZE(light_members);
        // LightBuffer (index 1)
        types[1].m_Name        = (char*)"LightBuffer";
        types[1].m_NameHash    = dmHashString64("LightBuffer");
        types[1].m_Members     = light_buffer_members;
        types[1].m_MemberCount = DM_ARRAY_SIZE(light_buffer_members);

        dmGraphics::UpdateShaderTypesOffsets(light_types, DM_ARRAY_SIZE(light_types));
        dmGraphics::GetUniformBufferLayout(0, light_types, DM_ARRAY_SIZE(light_types), out);
    }

    void InitializeLightBuffer(HRenderContext render_context)
    {
        UniformBufferLayout tmp_layout;
        GenerateDefaultUniformBufferLayout(8, &tmp_layout);

        render_context->m_LightUniformBuffer = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, tmp_layout);
    }

    HLight NewLight(HRenderContext render_context, const LightParams& params)
    {
        switch(params.m_Type)
        {
        case LIGHT_TYPE_DIRECTIONAL:
            {
                DirectionalLight* dr = new DirectionalLight;
                dr->m_BaseLight.m_Type = params.m_Type;
                dr->m_BaseLight.m_Color = params.m_Color;
                dr->m_BaseLight.m_Intensity = params.m_Intensity;
                dr->m_Direction = params.m_Direction;
                return (HLight) dr;
            } break;
        case LIGHT_TYPE_POINT:
            {
                PointLight* pl = new PointLight;
                pl->m_BaseLight.m_Type = params.m_Type;
                pl->m_BaseLight.m_Color = params.m_Color;
                pl->m_BaseLight.m_Intensity = params.m_Intensity;
                pl->m_Range = params.m_Range;
                return (HLight) pl;
            } break;
        case LIGHT_TYPE_SPOT:
            {
                SpotLight* sl = new SpotLight;
                sl->m_BaseLight.m_Type = params.m_Type;
                sl->m_BaseLight.m_Color = params.m_Color;
                sl->m_BaseLight.m_Intensity = params.m_Intensity;
                sl->m_Range = params.m_Range;
                sl->m_InnerConeAngle = params.m_InnerConeAngle;
                sl->m_OuterConeAngle = params.m_OuterConeAngle;
                return (HLight) sl;
            } break;
        }

        return 0;
    }

    void DeleteLight(HRenderContext render_context, HLight light)
    {
        delete light;
    }

    HLightInstance NewLightInstance(HRenderContext render_context, HLight light_prototype)
    {
        if (render_context->m_RenderLights.Full())
        {
            render_context->m_RenderLights.Allocate(4);
        }

        LightInstance* light_instance    = new LightInstance;
        light_instance->m_Position       = dmVMath::Point3();
        light_instance->m_LightPrototype = light_prototype;

        return render_context->m_RenderLights.Put(light_instance);
    }

    void DeleteLightInstance(HRenderContext render_context, HLightInstance instance)
    {
        LightInstance* light_instance = render_context->m_RenderLights.Get(instance);
        if (light_instance)
        {
            delete light_instance;
            render_context->m_RenderLights.Release(instance);
        }
    }

    void SetLightInstance(HRenderContext render_context, HLightInstance instance, dmVMath::Point3 position, dmVMath::Quat rotation)
    {
        LightInstance* light_instance = render_context->m_RenderLights.Get(instance);
        if (!light_instance)
            return;
        light_instance->m_Position = position;
        light_instance->m_Rotation = rotation;
    }
}
