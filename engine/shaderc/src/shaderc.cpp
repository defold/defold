
#include <stdlib.h>
#include <string.h>

#include <spirv/spirv_cross_c.h>

#include <dmsdk/dlib/log.h>

#include "shaderc.h"

// Reference API impl:
// https://github.com/KhronosGroup/SPIRV-Cross/blob/main/tests-other/c_api_test.c

namespace dmShaderc
{
    struct ShaderContext
    {
        spvc_context   m_SPVCContext;
        spvc_parsed_ir m_ParsedIR;
    };

    void _spvc_error_callback(void *userdata, const char *error)
    {
        dmLogError("Shaderc error: '%s'", error);
    }

    HShaderContext NewShaderContext(const void* source, uint32_t source_size)
    {
        ShaderContext* context = (ShaderContext*) malloc(sizeof(ShaderContext));
        memset(context, 0, sizeof(ShaderContext));

        SpvId* as_spv_ptr    = (SpvId*) source;
        u_int32_t word_count = source_size / sizeof(SpvId);

        spvc_context_create(&context->m_SPVCContext);
        spvc_context_set_error_callback(context->m_SPVCContext, _spvc_error_callback, NULL);
        spvc_context_parse_spirv(context->m_SPVCContext, as_spv_ptr, word_count, &context->m_ParsedIR);

        return context;
    }

    void DeleteShaderContext(HShaderContext context)
    {
        spvc_context_destroy(context->m_SPVCContext);

        free(context);
    }

    void GetReflection(HShaderContext context, ShaderReflection* reflection)
    {
        spvc_compiler compiler_none = NULL;
        spvc_resources resources = NULL;

        spvc_context_create_compiler(context->m_SPVCContext, SPVC_BACKEND_NONE, context->m_ParsedIR, SPVC_CAPTURE_MODE_COPY, &compiler_none);
        spvc_compiler_create_shader_resources(compiler_none, &resources);

        // Uniform buffers
        {
            const spvc_reflected_resource *list = NULL;
            size_t count = 0;
            spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);

            reflection->m_UniformBuffers.SetCapacity(count);
            reflection->m_UniformBuffers.SetSize(count);

            for (uint32_t i = 0; i < count; ++i)
            {
                uint32_t set          = spvc_compiler_get_decoration(compiler_none, list[i].id, SpvDecorationDescriptorSet);
                uint32_t binding      = spvc_compiler_get_decoration(compiler_none, list[i].id, SpvDecorationBinding);
                const char* inst_name = spvc_compiler_get_name(compiler_none, list[i].id);

                reflection->m_UniformBuffers[i].m_Name             = list[i].name;
                reflection->m_UniformBuffers[i].m_NameHash         = dmHashString64(list[i].name);
                reflection->m_UniformBuffers[i].m_InstanceName     = inst_name;
                reflection->m_UniformBuffers[i].m_InstanceNameHash = dmHashString64(inst_name);
                reflection->m_UniformBuffers[i].m_Set              = set;
                reflection->m_UniformBuffers[i].m_Binding          = binding;
            }
        }
    }
}
