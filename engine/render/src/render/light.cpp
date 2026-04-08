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

#include <dlib/dstrings.h>

namespace dmRender
{
    // Matches GLSL: uniform Light { ... } lights[MAX];
    static const dmhash_t LIGHT_UNIFORM_BLOCK_TYPE_HASH = dmHashString64("Light");

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
        light_instance->m_Position         = dmVMath::Point3(0.0f, 0.0f, 0.0f);
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
        GLSL (must match shaders and SPIR-V reflection):
            uniform Light
            {
                vec4 position;
                vec4 color;
                vec4 direction_range;
                vec4 params;
            } lights[MAX_LIGHTS];

        UniformBufferLayout::m_Hash is derived only from the std140 layout of type "Light" (four vec4 members).
        The instance count MAX_LIGHTS is on the UBO binding, not part of the type — same as GetUniformBufferLayout for programs.
        Total GPU buffer size = max_lights * sizeof(Light) in std140 (64 bytes per light).
        Active light count is set via lights_count outside this buffer.
        */

        dmGraphics::ShaderResourceMember   light_members[4];
        dmGraphics::ShaderResourceTypeInfo light_type;

        memset(light_members, 0, sizeof(light_members));
        memset(&light_type, 0, sizeof(light_type));

        light_members[0].m_Name                = (char*)"position";
        light_members[0].m_NameHash            = dmHashString64("position");
        light_members[0].m_Type.m_ShaderType   = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[0].m_Type.m_UseTypeIndex = 0;
        light_members[0].m_ElementCount        = 1;
        light_members[1].m_Name                = (char*)"color";
        light_members[1].m_NameHash            = dmHashString64("color");
        light_members[1].m_Type.m_ShaderType   = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[1].m_Type.m_UseTypeIndex = 0;
        light_members[1].m_ElementCount        = 1;
        light_members[2].m_Name                = (char*)"direction_range";
        light_members[2].m_NameHash            = dmHashString64("direction_range");
        light_members[2].m_Type.m_ShaderType   = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[2].m_Type.m_UseTypeIndex = 0;
        light_members[2].m_ElementCount        = 1;
        light_members[3].m_Name                = (char*)"params";
        light_members[3].m_NameHash            = dmHashString64("params");
        light_members[3].m_Type.m_ShaderType   = dmGraphics::ShaderDesc::SHADER_TYPE_VEC4;
        light_members[3].m_Type.m_UseTypeIndex = 0;
        light_members[3].m_ElementCount        = 1;

        light_type.m_Name        = (char*)"Light";
        light_type.m_NameHash    = dmHashString64("Light");
        light_type.m_Members     = light_members;
        light_type.m_MemberCount = DM_ARRAY_SIZE(light_members);

        if (max_lights <= 0)
        {
            render_context->m_LightUniformBuffer      = 0;
            render_context->m_LightBufferDataWriteStart = 0;
            return;
        }

        dmGraphics::UniformBufferLayout layout;
        dmGraphics::UpdateShaderTypesOffsets(&light_type, 1);
        dmGraphics::GetUniformBufferLayout(0, &light_type, 1, &layout);

        assert(layout.m_Size > 0 && "Light struct std140 size");
        layout.m_Size *= (uint32_t) max_lights;

        render_context->m_LightUniformBuffer          = dmGraphics::NewUniformBuffer(render_context->m_GraphicsContext, layout);
        render_context->m_LightBufferDataWriteStart = 0;
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
        if (!render_context->m_LightUniformBuffer)
        {
            return;
        }

        if (render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart)
        {
            uint32_t write_light_data_start = render_context->m_LightBufferDataWriteStart;
            uint32_t write_start            = write_light_data_start + render_context->m_LightBufferDirtyStart * sizeof(LightSTD140);
            uint32_t write_end              = write_light_data_start + render_context->m_LightBufferDirtyEnd * sizeof(LightSTD140);
            uint32_t write_size             = write_end - write_start;
            LightSTD140* write_ptr_start    = &render_context->m_LightBufferScratch[render_context->m_LightBufferDirtyStart];

            dmGraphics::SetUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer, write_start, write_size, (void*) write_ptr_start);

            render_context->m_LightBufferDirtyStart = render_context->m_LightBufferScratch.Size();
            render_context->m_LightBufferDirtyEnd   = 0;
        }
    }

    static inline bool IsLightBufferDirty(HRenderContext render_context)
    {
        return render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart || render_context->m_LightBufferDirtyCount;
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
        render_context->m_LightBufferScratch.SetCapacity(max_lights);
        render_context->m_LightBufferScratch.SetSize(0);
        GenerateUniformBuffer(render_context, (int) max_lights);
    }

    void FinalizeLightData(HRenderContext render_context)
    {
        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::DeleteUniformBuffer(render_context->m_GraphicsContext, render_context->m_LightUniformBuffer);
        }
    }

    struct LightBufferBindingCallbackContext
    {
        RenderContext* m_Context;
        bool           m_HasLightBuffer;
        uint16_t       m_Set;
        uint16_t       m_Binding;
    };

    static void LightBufferBindingCallback(uint16_t set, uint16_t binding, const dmGraphics::ShaderResourceTypeInfo* root_type, uint32_t binding_element_count, void* user_data)
    {
        LightBufferBindingCallbackContext* cb_ctx = (LightBufferBindingCallbackContext*) user_data;

        if (cb_ctx->m_HasLightBuffer || root_type->m_NameHash != LIGHT_UNIFORM_BLOCK_TYPE_HASH)
        {
            return;
        }

        const uint32_t ubo_light_count = binding_element_count;

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

    static void ApplyLightBufferForBinding(HRenderContext render_context, uint16_t light_buffer_set, uint16_t light_buffer_binding,
        dmGraphics::HProgram program, dmGraphics::HUniformLocation lights_count_location, uint8_t has_lights_count_uniform)
    {
        const bool instance_dirty = render_context->m_LightBufferDirtyEnd > render_context->m_LightBufferDirtyStart;
        const bool count_dirty    = render_context->m_LightBufferDirtyCount != 0;

        if (IsLightBufferDirty(render_context))
        {
            if (render_context->m_LightUniformBuffer)
            {
                WriteLightInstanceData(render_context);
            }
        }

        dmGraphics::HContext gfx         = render_context->m_GraphicsContext;
        const float          active_count = (float) render_context->m_RenderLightsIndices.Size();

        if (has_lights_count_uniform && lights_count_location != dmGraphics::INVALID_UNIFORM_LOCATION)
        {
            if (count_dirty || active_count != render_context->m_LightBufferLastWrittenCount)
            {
                dmVMath::Vector4 count_v4(active_count, 0.0f, 0.0f, 0.0f);
                dmGraphics::SetConstantV4(gfx, &count_v4, 1, lights_count_location);
            }
        }
        render_context->m_LightBufferLastWrittenCount = active_count;
        render_context->m_LightBufferDirtyCount       = 0;

        if (render_context->m_LightUniformBuffer)
        {
            dmGraphics::EnableUniformBuffer(gfx, render_context->m_LightUniformBuffer, light_buffer_set, light_buffer_binding);
        }
        else if (program && instance_dirty)
        {
            const uint32_t n = (uint32_t) active_count;
            char           name[96];
            for (uint32_t i = 0; i < n; ++i)
            {
                const LightSTD140& L = render_context->m_LightBufferScratch[i];
                dmSnPrintf(name, sizeof(name), "lights[%u].position", i);
                dmGraphics::HUniformLocation loc = dmGraphics::FindUniformLocation(program, name);
                if (loc != dmGraphics::INVALID_UNIFORM_LOCATION)
                {
                    dmGraphics::SetConstantV4(gfx, &L.m_Position, 1, loc);
                }
                dmSnPrintf(name, sizeof(name), "lights[%u].color", i);
                loc = dmGraphics::FindUniformLocation(program, name);
                if (loc != dmGraphics::INVALID_UNIFORM_LOCATION)
                {
                    dmGraphics::SetConstantV4(gfx, &L.m_Color, 1, loc);
                }
                dmSnPrintf(name, sizeof(name), "lights[%u].direction_range", i);
                loc = dmGraphics::FindUniformLocation(program, name);
                if (loc != dmGraphics::INVALID_UNIFORM_LOCATION)
                {
                    dmGraphics::SetConstantV4(gfx, &L.m_DirectionRange, 1, loc);
                }
                dmSnPrintf(name, sizeof(name), "lights[%u].params", i);
                loc = dmGraphics::FindUniformLocation(program, name);
                if (loc != dmGraphics::INVALID_UNIFORM_LOCATION)
                {
                    dmGraphics::SetConstantV4(gfx, &L.m_Params, 1, loc);
                }
            }
        }

        if (instance_dirty)
        {
            render_context->m_LightBufferDirtyStart = render_context->m_LightBufferScratch.Size();
            render_context->m_LightBufferDirtyEnd   = 0;
        }
    }

    void ApplyMaterialProgramLightBuffers(HRenderContext render_context, HMaterial material)
    {
        if (!material->m_HasLightBuffer)
        {
            return;
        }
        ApplyLightBufferForBinding(render_context, material->m_LightBufferSet, material->m_LightBufferBinding,
            material->m_Program, material->m_LightsCountLocation, material->m_HasLightsCountUniform);
    }

    void ApplyComputeProgramLightBuffers(HRenderContext render_context, HComputeProgram compute_program)
    {
        if (!compute_program->m_HasLightBuffer)
        {
            return;
        }
        ApplyLightBufferForBinding(render_context, compute_program->m_LightBufferSet, compute_program->m_LightBufferBinding,
            compute_program->m_Program, compute_program->m_LightsCountLocation, compute_program->m_HasLightsCountUniform);
    }
}
