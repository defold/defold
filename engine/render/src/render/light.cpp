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

#include <dlib/profile.h>
#include <algorithm>

DM_PROPERTY_GROUP(rmtp_Lighting, "Lighting", 0);
DM_PROPERTY_U32(rmtp_ActiveLights, 0, PROFILE_PROPERTY_FRAME_RESET, "# active lights uploaded", &rmtp_Lighting);
DM_PROPERTY_U32(rmtp_LightBufferUploadBytes, 0, PROFILE_PROPERTY_FRAME_RESET, "LightBuffer bytes uploaded", &rmtp_Lighting);
DM_PROPERTY_U32(rmtp_LightBufferUploadCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# LightBuffer uploads", &rmtp_Lighting);

namespace dmRender
{
    static const uint16_t INVALID_SHADOW_SLOT = 0xFFFF;
    static const uint32_t SHADOW_SLOT_SPOT = 0;
    static const uint32_t SHADOW_SLOT_POINT = 1;
    static const dmhash_t LIGHT_BUFFER_TYPE = dmHashString64("LightBuffer");
    static const dmhash_t LIGHT_MEMBER_TYPE = dmHashString64("lights");

    static void CommitLightInstance(HRenderContext render_context, const LightInstance* instance, dmVMath::Point3 position, dmVMath::Vector3 direction, float scale);
    static void CommitLightInfo(HRenderContext render_context);
    static void FillLightInstanceSTD140(const LightPrototype* prototype, dmVMath::Point3 position, dmVMath::Vector3 world_direction, float scale, LightSTD140* out_light);
    static bool LightSTD140Equals(const LightSTD140& a, const LightSTD140& b);
    static bool EnsureLightUniformBuffer(HRenderContext render_context);

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

        // Ambient lights are folded into light_info.xyz and do not allocate
        // entries in the per-light buffer.
        if (prototype->m_Type == LIGHT_TYPE_AMBIENT)
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
        render_context->m_LightShadowEligibility[light_buffer_index] = 1;
        render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][light_buffer_index] = INVALID_SHADOW_SLOT;
        render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][light_buffer_index] = INVALID_SHADOW_SLOT;
        uint32_t& shadow_revision = render_context->m_LightShadowRevisions[light_buffer_index];
        if (++shadow_revision == 0)
            shadow_revision = 1;
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
            light_instance->m_LightPrototype = 0;
            render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][light_buffer_index] = INVALID_SHADOW_SLOT;
            render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][light_buffer_index] = INVALID_SHADOW_SLOT;
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
        // Shadow selection owns position.w. Preserve it for transform-change
        // detection so a stable shadow tag does not make an otherwise static
        // light look dirty every frame.
        updated_light.m_Position.setW(render_context->m_LightBufferScratch[light_instance->m_LightBufferIndex].m_Position.getW());
        bool needs_commit = !LightSTD140Equals(updated_light, render_context->m_LightBufferScratch[light_instance->m_LightBufferIndex]);

        if (needs_commit)
        {
            CommitLightInstance(render_context, light_instance, position, direction, clamped_scale);
            uint32_t& shadow_revision = render_context->m_LightShadowRevisions[light_buffer_index];
            if (++shadow_revision == 0)
                shadow_revision = 1;
        }
    }

    void SetLightInstanceShadowEligible(HRenderContext render_context, HLightInstance instance, bool eligible)
    {
        const uint16_t light_buffer_index = instance & 0xFFFF;
        LightInstance* light_instance = light_buffer_index < render_context->m_RenderLights.Size() ? &render_context->m_RenderLights[light_buffer_index] : 0;
        if (light_instance && light_instance->m_LightPrototype != 0 && light_instance->m_Version == (instance >> 16))
            render_context->m_LightShadowEligibility[light_buffer_index] = eligible ? 1 : 0;
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

        // The engine owns the LightBuffer contract and writes a buffer sized by
        // game.project. Shaders may declare a smaller lights[] array, so the
        // light-specific layout hash intentionally ignores only that array size.
        light_buffer_members[1].m_ElementCount = 1;
        return dmGraphics::GetUniformBufferLayout(0, light_types, DM_ARRAY_SIZE(light_types));
    }

    static void GenerateUniformBuffer(HRenderContext render_context, int max_lights)
    {
        uint32_t buffer_size = 0;
        dmGraphics::UniformBufferLayout layout = GetLightBufferLayout((uint32_t) max_lights, &buffer_size, &render_context->m_LightBufferInfoWriteStart, &render_context->m_LightBufferDataWriteStart);
        render_context->m_LightUniformBuffer = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, layout, buffer_size);
    }

    static inline void FillLightInstanceSTD140(const LightPrototype* prototype, dmVMath::Point3 position, dmVMath::Vector3 world_direction, float scale, LightSTD140* out_light)
    {
        out_light->m_Position = dmVMath::Vector4(position);
        // Reserved for render-path-specific per-light metadata. Clustered
        // spotlight shadows use zero for unshadowed lights and slot + 1 for
        // lights assigned to a shadow atlas.
        out_light->m_Position.setW(0.0f);
        out_light->m_Color    = prototype->m_Color;

        dmVMath::Vector3 direction(0.0f, 0.0f, 0.0f);
        float range      = 0.0f;
        float inner_cone = 0.0f;
        float outer_cone = 0.0f;

        switch (prototype->m_Type)
        {
        case LIGHT_TYPE_AMBIENT:
            assert(false && "Ambient lights are accumulated in light_info.xyz and should not be written as light instances");
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

        // Mark dirty range
        render_context->m_LightBufferDirtyStart = dmMath::Min(render_context->m_LightBufferDirtyStart, (uint32_t) instance->m_LightBufferIndex);
        render_context->m_LightBufferDirtyEnd = dmMath::Max(render_context->m_LightBufferDirtyEnd, (uint32_t) (instance->m_LightBufferIndex + 1));
    }

    static inline void CommitLightInfo(HRenderContext render_context)
    {
        render_context->m_LightBufferDirtyInfo = 1;
    }

    static uint32_t CompactLightBufferScratch(HRenderContext render_context)
    {
        uint32_t active_light_count = render_context->m_RenderLightsIndices.Size();
        render_context->m_LightBufferUploadScratch.SetSize(0);

        if (active_light_count == 0)
        {
            return 0;
        }

        if (render_context->m_LightBufferUploadScratch.Capacity() < active_light_count)
        {
            render_context->m_LightBufferUploadScratch.SetCapacity(active_light_count);
        }

        uint32_t render_light_count = render_context->m_RenderLights.Size();
        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            const LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype != 0)
            {
                render_context->m_LightBufferUploadScratch.Push(render_context->m_LightBufferScratch[instance->m_LightBufferIndex]);
            }
        }

        return render_context->m_LightBufferUploadScratch.Size();
    }

    static void WriteLightInstanceData(HRenderContext render_context)
    {
        DM_PROFILE("LightBufferUpload");
        uint32_t active_light_count = CompactLightBufferScratch(render_context);
        uint32_t uploaded_bytes = 0;

        if (render_context->m_LightBufferDirtyInfo)
        {
            dmVMath::Vector4 info(render_context->m_AmbientLight, (float) active_light_count);
            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext,
                                         render_context->m_LightUniformBuffer,
                                         render_context->m_LightBufferInfoWriteStart,
                                         sizeof(info),
                                         &info);
            uploaded_bytes += sizeof(info);
        }

        // Write compacted light data from the scratch buffer. The shader loops
        // over [0..light_info.w), while light buffer indices may be reused.
        bool light_data_dirty = render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart;
        if (active_light_count > 0 && (light_data_dirty || render_context->m_LightBufferDirtyInfo))
        {
            uint32_t write_size = active_light_count * sizeof(LightSTD140);
            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext,
                                         render_context->m_LightUniformBuffer,
                                         render_context->m_LightBufferDataWriteStart,
                                         write_size,
                                         render_context->m_LightBufferUploadScratch.Begin());
            uploaded_bytes += write_size;
        }

        DM_PROPERTY_ADD_U32(rmtp_ActiveLights, active_light_count);
        DM_PROPERTY_ADD_U32(rmtp_LightBufferUploadBytes, uploaded_bytes);
        DM_PROPERTY_ADD_U32(rmtp_LightBufferUploadCount, 1);

        // Reset all dirty flags
        render_context->m_LightBufferDirtyStart = render_context->m_LightBufferScratch.Size();
        render_context->m_LightBufferDirtyEnd   = 0;
        render_context->m_LightBufferDirtyInfo  = 0;
    }

    static inline bool IsLightBufferDirty(HRenderContext render_context)
    {
        return render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart || render_context->m_LightBufferDirtyInfo;
    }

    static bool EnsureLightUniformBuffer(HRenderContext render_context)
    {
        if (render_context->m_LightUniformBuffer)
        {
            return true;
        }

        GenerateUniformBuffer(render_context, (int) render_context->m_MaxLightCount);
        if (!render_context->m_LightUniformBuffer)
        {
            return false;
        }

        // The GPU buffer is created lazily, so it needs a full initial upload.
        render_context->m_LightBufferDirtyStart = 0;
        render_context->m_LightBufferDirtyEnd   = render_context->m_LightBufferScratch.Size();
        render_context->m_LightBufferDirtyInfo  = 1;
        return true;
    }

    void SetAmbientLight(HRenderContext render_context, dmVMath::Vector3 color)
    {
        if (render_context->m_AmbientLight.getX() != color.getX() ||
            render_context->m_AmbientLight.getY() != color.getY() ||
            render_context->m_AmbientLight.getZ() != color.getZ())
        {
            render_context->m_AmbientLight = color;
            CommitLightInfo(render_context);
        }
    }

    void SetLightBufferCount(HRenderContext render_context, uint32_t max_lights)
    {
        assert(render_context);
        assert(render_context->m_RenderLightsIndices.Size() == 0);

        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
            render_context->m_LightUniformBuffer = 0;
        }

        render_context->m_MaxLightCount               = (uint16_t) max_lights;
        render_context->m_LightBufferDirtyStart       = 0;
        render_context->m_LightBufferDirtyEnd         = 0;
        render_context->m_LightBufferDirtyInfo        = 0;
        render_context->m_LightBufferInfoWriteStart   = 0;
        render_context->m_LightBufferDataWriteStart   = 0;
        render_context->m_AmbientLight                = dmVMath::Vector3(0.0f, 0.0f, 0.0f);

        if (render_context->m_RenderLightsIndices.Capacity() < max_lights)
        {
            render_context->m_RenderLightsIndices.SetCapacity(max_lights);
        }
        render_context->m_RenderLightsIndices.Clear();

        uint32_t old_light_count = render_context->m_RenderLights.Size();
        if (render_context->m_RenderLights.Capacity() < max_lights)
        {
            render_context->m_RenderLights.SetCapacity(max_lights);
        }
        render_context->m_RenderLights.SetSize(max_lights);
        for (uint32_t slot_set = 0; slot_set < 2; ++slot_set)
        {
            render_context->m_LightShadowSlots[slot_set].SetCapacity(max_lights);
            render_context->m_LightShadowSlots[slot_set].SetSize(max_lights);
        }
        render_context->m_LightShadowRevisions.SetCapacity(max_lights);
        render_context->m_LightShadowRevisions.SetSize(max_lights);
        render_context->m_LightShadowEligibility.SetCapacity(max_lights);
        render_context->m_LightShadowEligibility.SetSize(max_lights);
        for (uint32_t i = 0; i < max_lights; ++i)
        {
            render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][i] = INVALID_SHADOW_SLOT;
            render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][i] = INVALID_SHADOW_SLOT;
            render_context->m_LightShadowRevisions[i] = 0;
            render_context->m_LightShadowEligibility[i] = 1;
        }
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
    }

    struct SpotShadowCandidate
    {
        uint32_t m_InstanceIndex;
        uint32_t m_CompactedLightIndex;
        float    m_Score;
    };

    static bool CompareSpotShadowCandidates(const SpotShadowCandidate& a, const SpotShadowCandidate& b)
    {
        if (a.m_Score != b.m_Score)
            return a.m_Score > b.m_Score;
        return a.m_InstanceIndex < b.m_InstanceIndex;
    }

    static float ScoreSpotShadowCandidate(const LightPrototype* prototype, const LightSTD140& light, uint16_t previous_shadow_slot, uint32_t max_shadows, const SpotLightShadowSelectionParams* params)
    {
        const dmVMath::Vector4 color = prototype->m_Color;
        const float brightness = dmMath::Max(color.getX(), dmMath::Max(color.getY(), color.getZ())) * prototype->m_Intensity;
        float relevance = 1.0f;

        if (params)
        {
            const dmVMath::Vector4 world_position(light.m_Position.getX(), light.m_Position.getY(), light.m_Position.getZ(), 1.0f);
            const dmVMath::Vector4 view_position = params->m_CameraView * world_position;
            const float distance = dmVMath::Length(view_position.getXYZ());
            const float range = dmMath::Max(light.m_DirectionRange.getW(), 0.001f);
            const float distance_weight = range / (distance + range);

            const dmVMath::Vector4 clip_position = params->m_CameraProjection * view_position;
            const float clip_w = clip_position.getW();
            float visibility = 0.05f;
            float coverage = 0.0f;
            if (clip_w > 0.001f)
            {
                const float projection_scale = dmMath::Max(fabsf(params->m_CameraProjection.getElem(0, 0)), fabsf(params->m_CameraProjection.getElem(1, 1)));
                const float projected_radius = dmMath::Min(2.0f, range * projection_scale / clip_w);
                const float ndc_x = clip_position.getX() / clip_w;
                const float ndc_y = clip_position.getY() / clip_w;
                const bool overlaps_view = fabsf(ndc_x) <= 1.0f + projected_radius && fabsf(ndc_y) <= 1.0f + projected_radius;
                visibility = overlaps_view ? 1.0f : 0.1f;
                coverage = projected_radius * projected_radius;
            }
            relevance = visibility * (0.25f + coverage) * (0.25f + distance_weight);
        }

        // A small hysteresis bonus prevents similarly ranked lights from
        // exchanging atlas tiles as the camera moves.
        if (previous_shadow_slot < max_shadows)
            relevance *= 1.15f;
        return brightness * relevance;
    }

    uint32_t SelectSpotLightShadows(HRenderContext render_context, uint32_t max_shadows, SpotLightShadowData* out_shadows, const SpotLightShadowSelectionParams* selection_params)
    {
        max_shadows = dmMath::Min(max_shadows, 256u);
        dmArray<SpotShadowCandidate> candidates;
        candidates.SetCapacity(render_context->m_RenderLightsIndices.Size());

        uint32_t compacted_light_index = 0;
        const uint32_t render_light_count = render_context->m_RenderLights.Size();

        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0)
                continue;

            const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
            if (prototype && prototype->m_Type == LIGHT_TYPE_SPOT && render_context->m_LightShadowEligibility[i])
            {
                SpotShadowCandidate candidate;
                candidate.m_InstanceIndex = i;
                candidate.m_CompactedLightIndex = compacted_light_index;
                candidate.m_Score = ScoreSpotShadowCandidate(prototype, render_context->m_LightBufferScratch[instance->m_LightBufferIndex], render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][i], max_shadows, selection_params);
                candidates.Push(candidate);
            }
            else
            {
                render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][i] = INVALID_SHADOW_SLOT;
            }
            ++compacted_light_index;
        }

        if (candidates.Size() > 1)
            std::sort(candidates.Begin(), candidates.End(), CompareSpotShadowCandidates);
        const uint32_t shadow_count = dmMath::Min(max_shadows, candidates.Size());
        bool used_slots[256] = {};

        // Preserve valid slots for lights that remain selected.
        for (uint32_t i = 0; i < shadow_count; ++i)
        {
            uint16_t& shadow_slot = render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][candidates[i].m_InstanceIndex];
            if (shadow_slot < max_shadows && !used_slots[shadow_slot])
                used_slots[shadow_slot] = true;
            else
                shadow_slot = INVALID_SHADOW_SLOT;
        }

        // New selections take the lowest available atlas slot.
        uint32_t next_free_slot = 0;
        for (uint32_t i = 0; i < shadow_count; ++i)
        {
            uint16_t& shadow_slot = render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][candidates[i].m_InstanceIndex];
            if (shadow_slot == INVALID_SHADOW_SLOT)
            {
                while (used_slots[next_free_slot])
                    ++next_free_slot;
                shadow_slot = (uint16_t) next_free_slot;
                used_slots[next_free_slot] = true;
            }
        }

        // Clear rejected lights, update GPU tags, and emit selected light data.
        for (uint32_t i = shadow_count; i < candidates.Size(); ++i)
            render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][candidates[i].m_InstanceIndex] = INVALID_SHADOW_SLOT;

        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0)
                continue;
            const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
            if (!prototype || prototype->m_Type != LIGHT_TYPE_SPOT)
                continue;
            LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
            const uint16_t shadow_slot = render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][i];
            const float shadow_tag = shadow_slot == INVALID_SHADOW_SLOT ? 0.0f : (float) (shadow_slot + 1);
            if (light.m_Position.getW() != shadow_tag)
            {
                light.m_Position.setW(shadow_tag);
                render_context->m_LightBufferDirtyStart = dmMath::Min(render_context->m_LightBufferDirtyStart, (uint32_t) instance->m_LightBufferIndex);
                render_context->m_LightBufferDirtyEnd = dmMath::Max(render_context->m_LightBufferDirtyEnd, (uint32_t) (instance->m_LightBufferIndex + 1));
            }
        }

        if (out_shadows)
        {
            for (uint32_t i = 0; i < shadow_count; ++i)
            {
                const SpotShadowCandidate& candidate = candidates[i];
                const LightInstance* instance = &render_context->m_RenderLights[candidate.m_InstanceIndex];
                const LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
                SpotLightShadowData& shadow = out_shadows[i];
                shadow.m_Position = dmVMath::Point3(light.m_Position.getX(), light.m_Position.getY(), light.m_Position.getZ());
                shadow.m_Direction = light.m_DirectionRange.getXYZ();
                shadow.m_Range = light.m_DirectionRange.getW();
                shadow.m_OuterConeAngle = light.m_Params.getW();
                shadow.m_LightIndex = candidate.m_CompactedLightIndex;
                shadow.m_ShadowIndex = render_context->m_LightShadowSlots[SHADOW_SLOT_SPOT][candidate.m_InstanceIndex];
                shadow.m_LightId = (uint32_t) instance->m_Version << 16 | instance->m_LightBufferIndex;
                shadow.m_Revision = render_context->m_LightShadowRevisions[candidate.m_InstanceIndex];

                dmVMath::Vector3 world_up(0.0f, 1.0f, 0.0f);
                if (fabsf(dmVMath::Dot(shadow.m_Direction, world_up)) > 0.99f)
                    world_up = dmVMath::Vector3(0.0f, 0.0f, 1.0f);

                const float near_z = dmMath::Min(0.1f, dmMath::Max(0.02f, shadow.m_Range * 0.01f));
                const float far_z = dmMath::Max(shadow.m_Range, near_z + 0.01f);
                const float fov = dmMath::Min((float) M_PI - 0.01f, dmMath::Max(0.01f, shadow.m_OuterConeAngle));
                const dmVMath::Point3 target = shadow.m_Position + shadow.m_Direction;
                shadow.m_View = dmVMath::Matrix4::lookAt(shadow.m_Position, target, world_up);
                shadow.m_Projection = dmVMath::Matrix4::perspective(fov, 1.0f, near_z, far_z);
                shadow.m_ViewProjection = shadow.m_Projection * shadow.m_View;
            }
        }

        return shadow_count;
    }

    uint32_t SelectPointLightShadows(HRenderContext render_context, uint32_t max_shadows, PointLightShadowData* out_shadows, const SpotLightShadowSelectionParams* selection_params)
    {
        max_shadows = dmMath::Min(max_shadows, 256u);
        dmArray<SpotShadowCandidate> candidates;
        candidates.SetCapacity(render_context->m_RenderLightsIndices.Size());

        uint32_t compacted_light_index = 0;
        const uint32_t render_light_count = render_context->m_RenderLights.Size();
        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0)
                continue;
            const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
            if (prototype && prototype->m_Type == LIGHT_TYPE_POINT && render_context->m_LightShadowEligibility[i])
            {
                SpotShadowCandidate candidate;
                candidate.m_InstanceIndex = i;
                candidate.m_CompactedLightIndex = compacted_light_index;
                candidate.m_Score = ScoreSpotShadowCandidate(prototype, render_context->m_LightBufferScratch[instance->m_LightBufferIndex], render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][i], max_shadows, selection_params);
                candidates.Push(candidate);
            }
            else
            {
                render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][i] = INVALID_SHADOW_SLOT;
            }
            ++compacted_light_index;
        }

        if (candidates.Size() > 1)
            std::sort(candidates.Begin(), candidates.End(), CompareSpotShadowCandidates);
        const uint32_t shadow_count = dmMath::Min(max_shadows, candidates.Size());
        bool used_slots[256] = {};
        for (uint32_t i = 0; i < shadow_count; ++i)
        {
            uint16_t& slot = render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][candidates[i].m_InstanceIndex];
            if (slot < max_shadows && !used_slots[slot])
                used_slots[slot] = true;
            else
                slot = INVALID_SHADOW_SLOT;
        }
        uint32_t next_free_slot = 0;
        for (uint32_t i = 0; i < shadow_count; ++i)
        {
            uint16_t& slot = render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][candidates[i].m_InstanceIndex];
            if (slot == INVALID_SHADOW_SLOT)
            {
                while (used_slots[next_free_slot])
                    ++next_free_slot;
                slot = (uint16_t) next_free_slot;
                used_slots[next_free_slot] = true;
            }
        }
        for (uint32_t i = shadow_count; i < candidates.Size(); ++i)
            render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][candidates[i].m_InstanceIndex] = INVALID_SHADOW_SLOT;

        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0)
                continue;
            const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
            if (!prototype || prototype->m_Type != LIGHT_TYPE_POINT)
                continue;
            LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
            const uint16_t slot = render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][i];
            const float shadow_tag = slot == INVALID_SHADOW_SLOT ? 0.0f : (float) (slot + 1);
            if (light.m_Position.getW() != shadow_tag)
            {
                light.m_Position.setW(shadow_tag);
                render_context->m_LightBufferDirtyStart = dmMath::Min(render_context->m_LightBufferDirtyStart, (uint32_t) instance->m_LightBufferIndex);
                render_context->m_LightBufferDirtyEnd = dmMath::Max(render_context->m_LightBufferDirtyEnd, (uint32_t) (instance->m_LightBufferIndex + 1));
            }
        }

        if (out_shadows)
        {
            for (uint32_t i = 0; i < shadow_count; ++i)
            {
                const SpotShadowCandidate& candidate = candidates[i];
                const LightInstance* instance = &render_context->m_RenderLights[candidate.m_InstanceIndex];
                const LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
                PointLightShadowData& shadow = out_shadows[i];
                shadow.m_Position = dmVMath::Point3(light.m_Position.getX(), light.m_Position.getY(), light.m_Position.getZ());
                shadow.m_Range = light.m_DirectionRange.getW();
                shadow.m_LightIndex = candidate.m_CompactedLightIndex;
                shadow.m_ShadowIndex = render_context->m_LightShadowSlots[SHADOW_SLOT_POINT][candidate.m_InstanceIndex];
                shadow.m_LightId = (uint32_t) instance->m_Version << 16 | instance->m_LightBufferIndex;
                shadow.m_Revision = render_context->m_LightShadowRevisions[candidate.m_InstanceIndex];
            }
        }
        return shadow_count;
    }

    bool SelectDirectionalLightShadow(HRenderContext render_context, DirectionalLightShadowData* out_shadow)
    {
        int32_t selected_instance_index = -1;
        uint32_t selected_compacted_index = 0;
        float selected_score = -1.0f;
        uint32_t compacted_index = 0;
        const uint32_t render_light_count = render_context->m_RenderLights.Size();
        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0)
                continue;
            const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
            if (prototype && prototype->m_Type == LIGHT_TYPE_DIRECTIONAL && render_context->m_LightShadowEligibility[i])
            {
                const float score = dmMath::Max(prototype->m_Color.getX(), dmMath::Max(prototype->m_Color.getY(), prototype->m_Color.getZ())) * prototype->m_Intensity;
                if (score > selected_score)
                {
                    selected_score = score;
                    selected_instance_index = (int32_t) i;
                    selected_compacted_index = compacted_index;
                }
            }
            ++compacted_index;
        }

        for (uint32_t i = 0; i < render_light_count; ++i)
        {
            LightInstance* instance = &render_context->m_RenderLights[i];
            if (instance->m_LightPrototype == 0)
                continue;
            const LightPrototype* prototype = render_context->m_LightPrototypes.Get(instance->m_LightPrototype);
            if (!prototype || prototype->m_Type != LIGHT_TYPE_DIRECTIONAL)
                continue;
            LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
            const float tag = (int32_t) i == selected_instance_index ? 1.0f : 0.0f;
            if (light.m_Position.getW() != tag)
            {
                light.m_Position.setW(tag);
                render_context->m_LightBufferDirtyStart = dmMath::Min(render_context->m_LightBufferDirtyStart, (uint32_t) instance->m_LightBufferIndex);
                render_context->m_LightBufferDirtyEnd = dmMath::Max(render_context->m_LightBufferDirtyEnd, (uint32_t) (instance->m_LightBufferIndex + 1));
            }
        }

        if (selected_instance_index < 0)
            return false;
        if (out_shadow)
        {
            const LightInstance* instance = &render_context->m_RenderLights[(uint32_t) selected_instance_index];
            const LightSTD140& light = render_context->m_LightBufferScratch[instance->m_LightBufferIndex];
            out_shadow->m_Direction = light.m_DirectionRange.getXYZ();
            out_shadow->m_LightIndex = selected_compacted_index;
            out_shadow->m_LightId = (uint32_t) instance->m_Version << 16 | instance->m_LightBufferIndex;
            out_shadow->m_Revision = render_context->m_LightShadowRevisions[(uint32_t) selected_instance_index];
        }
        return true;
    }

    void FinalizeLightData(HRenderContext render_context)
    {
        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
            render_context->m_LightUniformBuffer = 0;
        }

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
        dmGraphics::ShaderResourceBindingFamily m_Family;
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

        uint32_t shader_light_count = 0;
        uint32_t lights_member_index = UINT32_MAX;
        for (uint32_t i = 0; i < root_type->m_MemberCount; ++i)
        {
            if (root_type->m_Members[i].m_NameHash == LIGHT_MEMBER_TYPE)
            {
                shader_light_count = root_type->m_Members[i].m_ElementCount;
                lights_member_index = i;
                break;
            }
        }

        const bool storage_buffer = cb_ctx->m_Family == dmGraphics::BINDING_FAMILY_STORAGE_BUFFER;
        if (!storage_buffer && shader_light_count == 0)
        {
            dmLogOnceWarning("The light buffer must declare a lights array with at least one element.");
            return;
        }

        if (!storage_buffer && shader_light_count > cb_ctx->m_Context->m_MaxLightCount)
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

        if (layout)
        {
            *layout = light_buffer_layout;
        }

        cb_ctx->m_HasLightBuffer = true;
        cb_ctx->m_Set            = set;
        cb_ctx->m_Binding        = binding;
        cb_ctx->m_Capacity       = storage_buffer ? cb_ctx->m_Context->m_MaxLightCount : (uint16_t) shader_light_count;
    }

    void GetProgramLightBufferBinding(HRenderContext render_context, dmGraphics::HProgram program, bool* out_has_light_buffer, dmGraphics::ShaderResourceBindingFamily* out_family, uint16_t* out_set, uint16_t* out_binding, uint16_t* out_capacity)
    {
        LightBufferBindingCallbackContext cb_ctx;
        cb_ctx.m_Context         = render_context;
        cb_ctx.m_HasLightBuffer  = false;
        cb_ctx.m_Family          = dmGraphics::BINDING_FAMILY_UNIFORM_BUFFER;
        cb_ctx.m_Set             = 0;
        cb_ctx.m_Binding         = 0;
        cb_ctx.m_Capacity        = 0;

        dmGraphics::IterateProgramResourceBindings(program, dmGraphics::BINDING_FAMILY_UNIFORM_BUFFER, LightBufferBindingCallback, &cb_ctx);

        dmGraphics::AdapterFamily adapter_family = dmGraphics::GetInstalledAdapterFamily();
        bool storage_light_buffer_supported = adapter_family == dmGraphics::ADAPTER_FAMILY_VULKAN || adapter_family == dmGraphics::ADAPTER_FAMILY_WEBGPU || adapter_family == dmGraphics::ADAPTER_FAMILY_NULL;
        if (!cb_ctx.m_HasLightBuffer && storage_light_buffer_supported && dmGraphics::IsContextFeatureSupported(render_context->m_GraphicsContext, dmGraphics::CONTEXT_FEATURE_STORAGE_BUFFER))
        {
            cb_ctx.m_Family = dmGraphics::BINDING_FAMILY_STORAGE_BUFFER;
            dmGraphics::IterateProgramResourceBindings(program, dmGraphics::BINDING_FAMILY_STORAGE_BUFFER, LightBufferBindingCallback, &cb_ctx);
        }

        *out_has_light_buffer = cb_ctx.m_HasLightBuffer;
        *out_family           = cb_ctx.m_Family;
        *out_capacity         = cb_ctx.m_Capacity;
        if (cb_ctx.m_HasLightBuffer)
        {
            *out_set     = cb_ctx.m_Set;
            *out_binding = cb_ctx.m_Binding;
        }
    }

    static void ApplyLightBufferForBinding(HRenderContext render_context, dmGraphics::ShaderResourceBindingFamily family, uint16_t light_buffer_set, uint16_t light_buffer_binding)
    {
        if (!EnsureLightUniformBuffer(render_context))
        {
            return;
        }

        if (IsLightBufferDirty(render_context))
        {
            WriteLightInstanceData(render_context);
        }

        if (family == dmGraphics::BINDING_FAMILY_STORAGE_BUFFER)
        {
            dmGraphics::EnableUniformBufferAsStorage(render_context->m_GraphicsContext,
                                                     render_context->m_LightUniformBuffer,
                                                     light_buffer_set,
                                                     light_buffer_binding);
        }
        else
        {
            dmGraphics::EnableUniformBuffer(render_context->m_GraphicsContext,
                                            render_context->m_LightUniformBuffer,
                                            light_buffer_set,
                                            light_buffer_binding);
        }
    }

    static inline void UnbindLightBuffer(HRenderContext render_context)
    {
        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DisableUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
            dmGraphics::DisableUniformBufferAsStorage(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
        }
    }

    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (!material->m_HasLightBuffer)
        {
            UnbindLightBuffer(render_context);
            return;
        }
        ApplyLightBufferForBinding(render_context, material->m_LightBufferBindingFamily, material->m_LightBufferSet, material->m_LightBufferBinding);
    }

    void ApplyComputeProgramLightBuffers(HRenderContext render_context, HComputeProgram compute_program)
    {
        if (!compute_program->m_HasLightBuffer)
        {
            UnbindLightBuffer(render_context);
            return;
        }
        ApplyLightBufferForBinding(render_context, compute_program->m_LightBufferBindingFamily, compute_program->m_LightBufferSet, compute_program->m_LightBufferBinding);
    }
}
