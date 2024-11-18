
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

        spvc_compiler  m_CompilerNone;
        spvc_resources m_Resources;

        ShaderReflection m_Reflection;
    };

    struct ShaderCompiler
    {
        spvc_compiler  m_SPVCCompiler;
        ShaderLanguage m_Language;
    };

    static void _spvc_error_callback(void *userdata, const char *error)
    {
        dmLogError("Shaderc error: '%s'", error);
    }

    static void SetReflectionResourceForType(spvc_compiler compiler, spvc_resources resources, spvc_resource_type type, dmArray<ShaderResource>& resources_out)
    {
        const spvc_reflected_resource *list = NULL;
        size_t count = 0;
        spvc_resources_get_resource_list_for_type(resources, type, &list, &count);

        resources_out.SetCapacity(count);
        resources_out.SetSize(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            resources_out[i].m_Name             = list[i].name;
            resources_out[i].m_NameHash         = dmHashString64(list[i].name);
            resources_out[i].m_InstanceName     = spvc_compiler_get_name(compiler, list[i].id);
            resources_out[i].m_InstanceNameHash = dmHashString64(resources_out[i].m_InstanceName);
            resources_out[i].m_Set              = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet);
            resources_out[i].m_Binding          = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);;
            resources_out[i].m_Location         = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationLocation);
        }
    }

    static void SetReflectionInternal(HShaderContext context)
    {
        SetReflectionResourceForType(context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, context->m_Reflection.m_Inputs);
        SetReflectionResourceForType(context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, context->m_Reflection.m_Outputs);
        SetReflectionResourceForType(context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, context->m_Reflection.m_UniformBuffers);
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

        // Create a reflection compiler
        spvc_context_create_compiler(context->m_SPVCContext, SPVC_BACKEND_NONE, context->m_ParsedIR, SPVC_CAPTURE_MODE_COPY, &context->m_CompilerNone);
        spvc_compiler_create_shader_resources(context->m_CompilerNone, &context->m_Resources);

        SetReflectionInternal(context);

        return context;
    }

    void DeleteShaderContext(HShaderContext context)
    {
        spvc_context_destroy(context->m_SPVCContext);

        free(context);
    }

    const ShaderReflection* GetReflection(HShaderContext context)
    {
        return &context->m_Reflection;
    }

    static void SetLocationForType(spvc_compiler& compiler, spvc_resources resources, spvc_resource_type type, dmhash_t name_hash, uint8_t location)
    {
        const spvc_reflected_resource *list = NULL;
        size_t count = 0;
        spvc_resources_get_resource_list_for_type(resources, type, &list, &count);

        for (uint32_t i = 0; i < count; ++i)
        {
            dmhash_t list_name_hash = dmHashString64(list[i].name);
            if (list_name_hash == name_hash)
            {
                spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationLocation, location);
            }
        }
    }

    HShaderCompiler NewShaderCompiler(HShaderContext context, ShaderLanguage language)
    {
        ShaderCompiler* compiler = (ShaderCompiler*) malloc(sizeof(ShaderCompiler));
        memset(compiler, 0, sizeof(ShaderCompiler));

        compiler->m_Language = language;

        spvc_backend backend = SPVC_BACKEND_NONE;
        switch(language)
        {
            case SHADER_LANGUAGE_NONE:
                backend = SPVC_BACKEND_NONE;
                break;
            case SHADER_LANGUAGE_GLSL:
                backend = SPVC_BACKEND_GLSL;
                break;
            case SHADER_LANGUAGE_HLSL:
                backend = SPVC_BACKEND_HLSL;
                break;
            default:break;
        }

        spvc_context_create_compiler(context->m_SPVCContext, backend, context->m_ParsedIR, SPVC_CAPTURE_MODE_COPY, &compiler->m_SPVCCompiler);

        return compiler;
    }

    void DeleteShaderCompiler(HShaderCompiler compiler)
    {
        free(compiler);
    }

    void SetLocation(HShaderContext context, HShaderCompiler compiler, dmhash_t name_hash, uint8_t location)
    {
        SetLocationForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, name_hash, location);
        SetLocationForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, name_hash, location);
    }

    const char* Compile(HShaderContext context, HShaderCompiler compiler, const ShaderCompilerOptions& options)
    {
        spvc_compiler_options spv_options = NULL;
        spvc_compiler_create_compiler_options(compiler->m_SPVCCompiler, &spv_options);

        if (options.m_RemoveUnusedVariables)
        {
            spvc_set active_variables;
            spvc_resources active_resources;
            spvc_compiler_get_active_interface_variables(compiler->m_SPVCCompiler, &active_variables);
            spvc_compiler_create_shader_resources_for_active_variables(compiler->m_SPVCCompiler, &active_resources, active_variables);
            spvc_compiler_set_enabled_interface_variables(compiler->m_SPVCCompiler, active_variables);
        }

        if (compiler->m_Language == SHADER_LANGUAGE_GLSL)
        {
            spvc_compiler_options_set_uint(spv_options, SPVC_COMPILER_OPTION_GLSL_VERSION,                  options.m_Version);
            spvc_compiler_options_set_bool(spv_options, SPVC_COMPILER_OPTION_GLSL_ES,                       options.m_GlslEs);
            spvc_compiler_options_set_bool(spv_options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, !options.m_No420PackExtension);
        }
        else
        {
            assert(0 && "Shader langauge not yet supported");
        }

        spvc_compiler_install_compiler_options(compiler->m_SPVCCompiler, spv_options);

        SpvExecutionModel stage;

        switch(options.m_Stage)
        {
        case SHADER_STAGE_VERTEX:
            stage = SpvExecutionModelVertex;
            break;
        case SHADER_STAGE_FRAGMENT:
            stage = SpvExecutionModelFragment;
            break;
        case SHADER_STAGE_COMPUTE:
            stage = SpvExecutionModelGLCompute;
            break;
        default: break;
        }

        spvc_compiler_set_entry_point(compiler->m_SPVCCompiler, options.m_EntryPoint, stage);

        const char *result = NULL;
        spvc_compiler_compile(compiler->m_SPVCCompiler, &result);
        return result;
    }

    void DebugPrintReflection(const ShaderReflection* reflection)
    {
        dmLogInfo("Inputs: %d", reflection->m_Inputs.Size());
        for (uint32_t i = 0; i < reflection->m_Inputs.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s Location: %d", reflection->m_Inputs[i].m_Name, reflection->m_Inputs[i].m_InstanceName, reflection->m_Inputs[i].m_Location);
        }

        dmLogInfo("Outputs: %d", reflection->m_Outputs.Size());
        for (uint32_t i = 0; i < reflection->m_Outputs.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s Location: %d", reflection->m_Outputs[i].m_Name, reflection->m_Outputs[i].m_InstanceName, reflection->m_Outputs[i].m_Location);
        }

        dmLogInfo("Uniform buffers: %d", reflection->m_UniformBuffers.Size());
        for (uint32_t i = 0; i < reflection->m_UniformBuffers.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s, Set: %d, Binding: %d", reflection->m_UniformBuffers[i].m_Name, reflection->m_UniformBuffers[i].m_InstanceName, reflection->m_UniformBuffers[i].m_Set, reflection->m_UniformBuffers[i].m_Binding);
        }
    }
}
