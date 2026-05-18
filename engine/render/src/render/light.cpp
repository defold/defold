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
    static const dmhash_t LIGHT_BUFFER_TYPE = dmHashString64("LightBuffer");
    static const dmhash_t LIGHT_MEMBER_TYPE = dmHashString64("lights");

    static void CommitLightInstance(HRenderContext render_context, const LightInstance* instance, dmVMath::Point3 position, dmVMath::Vector3 direction, float scale);
    static void CommitLightCount(HRenderContext render_context);
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

    ////////////////////////////////
    // Light instance
    ////////////////////////////////

    HLightInstance NewLightInstance(HRenderContext render_context, HLightPrototype light_prototype)
    {
        if (!render_context->m_LightPrototypes.Get(light_prototype))
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
        CommitLightCount(render_context);

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
            CommitLightCount(render_context);
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

    static inline void CommitLightCount(HRenderContext render_context)
    {
        render_context->m_LightBufferDirtyCount = 1;
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
        uint32_t active_light_count = CompactLightBufferScratch(render_context);

        // The light count has changed, we need to write that separately
        if (render_context->m_LightBufferDirtyCount)
        {
            // ... but only if it is actually different from last write.
            if (active_light_count != render_context->m_LightBufferLastWrittenCount)
            {
                dmVMath::Vector4 count((float) active_light_count, 0.0f, 0.0f, 0.0f);
                dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, 0, sizeof(count), &count);
            }
            render_context->m_LightBufferLastWrittenCount = active_light_count;
        }

        // Write compacted light data from the scratch buffer. The shader loops
        // over [0..lights_count), while light buffer indices may be reused.
        bool light_data_dirty = render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart;
        if (active_light_count > 0 && (light_data_dirty || render_context->m_LightBufferDirtyCount))
        {
            uint32_t write_size = active_light_count * sizeof(LightSTD140);
            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext,
                                         render_context->m_LightUniformBuffer,
                                         render_context->m_LightBufferDataWriteStart,
                                         write_size,
                                         render_context->m_LightBufferUploadScratch.Begin());
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
        render_context->m_LightBufferDirtyCount = 1;
        return true;
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
        render_context->m_LightBufferDirtyCount       = 0;
        render_context->m_LightBufferDataWriteStart   = 0;
        render_context->m_LightBufferLastWrittenCount = 0;

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

    void FinalizeLightData(HRenderContext render_context)
    {
        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
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
        uint16_t       m_Set;
        uint16_t       m_Binding;
    };

    static void LightBufferBindingCallback(uint16_t set, uint16_t binding, const dmGraphics::ShaderResourceTypeInfo* root_type, void* user_data)
    {
        LightBufferBindingCallbackContext* cb_ctx = (LightBufferBindingCallbackContext*) user_data;

        if (cb_ctx->m_HasLightBuffer || root_type->m_NameHash != LIGHT_BUFFER_TYPE)
        {
            return;
        }

        uint32_t ubo_light_count = 0;
        for (uint32_t i = 0; i < root_type->m_MemberCount; ++i)
        {
            if (root_type->m_Members[i].m_NameHash == LIGHT_MEMBER_TYPE)
            {
                ubo_light_count = root_type->m_Members[i].m_ElementCount;
                break;
            }
        }

        if (cb_ctx->m_Context->m_MaxLightCount != ubo_light_count)
        {
            dmLogOnceWarning("The size of the light buffer must match the project configuration. You should use the same size everywhere for the uniform buffer!");
            return;
        }

        cb_ctx->m_HasLightBuffer = true;
        cb_ctx->m_Set            = set;
        cb_ctx->m_Binding        = binding;
    }

    void GetProgramLightBufferBinding(HRenderContext render_context, dmGraphics::HProgram program, bool* out_has_light_buffer, uint16_t* out_set, uint16_t* out_binding)
    {
        LightBufferBindingCallbackContext cb_ctx;
        cb_ctx.m_Context         = render_context;
        cb_ctx.m_HasLightBuffer  = false;
        cb_ctx.m_Set             = 0;
        cb_ctx.m_Binding         = 0;

        dmGraphics::IterateProgramResourceBindings(program, dmGraphics::BINDING_FAMILY_UNIFORM_BUFFER, LightBufferBindingCallback, &cb_ctx);

        *out_has_light_buffer = cb_ctx.m_HasLightBuffer;
        if (cb_ctx.m_HasLightBuffer)
        {
            *out_set     = cb_ctx.m_Set;
            *out_binding = cb_ctx.m_Binding;
        }
    }

    static void ApplyLightBufferForBinding(HRenderContext render_context, uint16_t light_buffer_set, uint16_t light_buffer_binding)
    {
        if (!EnsureLightUniformBuffer(render_context))
        {
            return;
        }

        if (IsLightBufferDirty(render_context))
        {
            WriteLightInstanceData(render_context);
        }

        dmGraphics::EnableUniformBuffer(render_context->m_GraphicsContext,
                                        render_context->m_LightUniformBuffer,
                                        light_buffer_set,
                                        light_buffer_binding);
    }

    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (!material->m_HasLightBuffer)
        {
            return;
        }
        ApplyLightBufferForBinding(render_context, material->m_LightBufferSet, material->m_LightBufferBinding);
    }

    void ApplyComputeProgramLightBuffers(HRenderContext render_context, HComputeProgram compute_program)
    {
        if (!compute_program->m_HasLightBuffer)
        {
            return;
        }
        ApplyLightBufferForBinding(render_context, compute_program->m_LightBufferSet, compute_program->m_LightBufferBinding);
    }
}
