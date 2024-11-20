
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

    static struct BaseTypeMapping
    {
        BaseType      m_BaseType;
        spvc_basetype m_SPVCBaseType;
        const char*   m_BaseTypeStr;
    } BASE_TYPE_MAPPING[] = {
        {BASE_TYPE_UNKNOWN, SPVC_BASETYPE_UNKNOWN, "BASE_TYPE_UNKNOWN"},
        {BASE_TYPE_VOID, SPVC_BASETYPE_VOID, "BASE_TYPE_VOID"},
        {BASE_TYPE_BOOLEAN, SPVC_BASETYPE_BOOLEAN, "BASE_TYPE_BOOLEAN"},
        {BASE_TYPE_INT8, SPVC_BASETYPE_INT8, "BASE_TYPE_INT8"},
        {BASE_TYPE_UINT8, SPVC_BASETYPE_UINT8, "BASE_TYPE_UINT8"},
        {BASE_TYPE_INT16, SPVC_BASETYPE_INT16, "BASE_TYPE_INT16"},
        {BASE_TYPE_UINT16, SPVC_BASETYPE_UINT16, "BASE_TYPE_UINT16"},
        {BASE_TYPE_INT32, SPVC_BASETYPE_INT32, "BASE_TYPE_INT32"},
        {BASE_TYPE_UINT32, SPVC_BASETYPE_UINT32, "BASE_TYPE_UINT32"},
        {BASE_TYPE_INT64, SPVC_BASETYPE_INT64, "BASE_TYPE_INT64"},
        {BASE_TYPE_UINT64, SPVC_BASETYPE_UINT64, "BASE_TYPE_UINT64"},
        {BASE_TYPE_ATOMIC_COUNTER, SPVC_BASETYPE_ATOMIC_COUNTER, "BASE_TYPE_ATOMIC_COUNTER"},
        {BASE_TYPE_FP16, SPVC_BASETYPE_FP16, "BASE_TYPE_FP16"},
        {BASE_TYPE_FP32, SPVC_BASETYPE_FP32, "BASE_TYPE_FP32"},
        {BASE_TYPE_FP64, SPVC_BASETYPE_FP64, "BASE_TYPE_FP64"},
        {BASE_TYPE_STRUCT, SPVC_BASETYPE_STRUCT, "BASE_TYPE_STRUCT"},
        {BASE_TYPE_IMAGE, SPVC_BASETYPE_IMAGE, "BASE_TYPE_IMAGE"},
        {BASE_TYPE_SAMPLED_IMAGE, SPVC_BASETYPE_SAMPLED_IMAGE, "BASE_TYPE_SAMPLED_IMAGE"},
        {BASE_TYPE_SAMPLER, SPVC_BASETYPE_SAMPLER, "BASE_TYPE_SAMPLER"},
        {BASE_TYPE_ACCELERATION_STRUCTURE, SPVC_BASETYPE_ACCELERATION_STRUCTURE, "BASE_TYPE_ACCELERATION_STRUCTURE"},
    };

    static uint32_t GetTypeIndex(ShaderReflection& reflection, spvc_compiler compiler, const char* type_name, spvc_type_id type_id, spvc_type type)
    {
        dmhash_t type_name_hash = dmHashString64(type_name);
        for (int i = 0; i < reflection.m_Types.Size(); ++i)
        {
            if (reflection.m_Types[i].m_NameHash == type_name_hash)
            {
                return i;
            }
        }

        /*
        struct ResourceTypeInfo
        {
            const char*             m_Name;
            dmhash_t                m_NameHash;
            dmArray<ResourceMember> m_Members;
        };
        */

        uint32_t member_count = spvc_type_get_num_member_types(type);

        ResourceTypeInfo type_info;
        type_info.m_Name = type_name;
        type_info.m_NameHash = type_name_hash;
        type_info.m_MemberCount = member_count;
        type_info.m_Members = new ResourceMember[member_count];

        for (int i = 0; i < member_count; ++i)
        {
            spvc_type_id member_type_id    = spvc_type_get_member_type(type, i);
            spvc_type member_type          = spvc_compiler_get_type_handle(compiler, member_type_id);
            spvc_basetype member_base_type = spvc_type_get_basetype(member_type);

            ResourceMember& member = type_info.m_Members[i];

            // struct ResourceMember
            // {
            //     const char*     m_Name;
            //     dmhash_t        m_NameHash;
            //     ResourceType    m_Type;
            //     uint32_t        m_ElementCount;
            //     uint32_t        m_Offset;
            // };

            if (member_base_type == SPVC_BASETYPE_STRUCT)
            {
                // resolve index
                member.m_Type.m_UseTypeIndex = true;
            }
            else
            {
                member.m_Type.m_BaseType     = BASE_TYPE_MAPPING[member_base_type].m_BaseType;
                member.m_Type.m_UseTypeIndex = false;
            }

            // const char* name = spvc_compiler_get_name(compiler, member_type_id);
            // dmLogInfo("Member name? %s", name);
        }

        uint32_t type_index = reflection.m_Types.Size();

        reflection.m_Types.OffsetCapacity(1);
        reflection.m_Types.Push(type_info);

        return type_index;
    }

    static void SetReflectionResourceForType(ShaderReflection& reflection, spvc_compiler compiler, spvc_resources resources, spvc_resource_type type, dmArray<ShaderResource>& resources_out)
    {
        const spvc_reflected_resource *list = NULL;
        size_t count = 0;
        spvc_resources_get_resource_list_for_type(resources, type, &list, &count);

        if (count == 0)
            return;

        if (resources_out.Remaining() < count)
        {
            resources_out.OffsetCapacity(count);
            resources_out.SetSize(resources_out.Capacity());
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            spvc_type type          = spvc_compiler_get_type_handle(compiler, list[i].type_id);
            spvc_basetype base_type = spvc_type_get_basetype(type);

            resources_out[i].m_Name             = list[i].name;
            resources_out[i].m_NameHash         = dmHashString64(list[i].name);
            resources_out[i].m_InstanceName     = spvc_compiler_get_name(compiler, list[i].id);
            resources_out[i].m_InstanceNameHash = dmHashString64(resources_out[i].m_InstanceName);
            resources_out[i].m_Set              = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet);
            resources_out[i].m_Binding          = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);
            resources_out[i].m_Location         = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationLocation);

            if (base_type == SPVC_BASETYPE_STRUCT)
            {
                resources_out[i].m_Type.m_TypeIndex    = GetTypeIndex(reflection, compiler, resources_out[i].m_Name, list[i].type_id, type);
                resources_out[i].m_Type.m_UseTypeIndex = true;
            }
            else
            {
                resources_out[i].m_Type.m_BaseType     = BASE_TYPE_MAPPING[base_type].m_BaseType;
                resources_out[i].m_Type.m_UseTypeIndex = false;
            }
        }
    }

    static void SetReflectionInternal(HShaderContext context)
    {
        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, context->m_Reflection.m_Inputs);
        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, context->m_Reflection.m_Outputs);
        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, context->m_Reflection.m_UniformBuffers);

        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, context->m_Reflection.m_Textures);
        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, context->m_Reflection.m_Textures);
        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, context->m_Reflection.m_Textures);
        SetReflectionResourceForType(context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, context->m_Reflection.m_Textures);
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

    static const char* ResolveTypeName(const ShaderReflection* reflection, const ResourceType& type)
    {
        if (type.m_UseTypeIndex)
        {
            return "TODO";
        }
        else
        {
            return BASE_TYPE_MAPPING[type.m_BaseType].m_BaseTypeStr;
        }
    }

    void DebugPrintReflection(const ShaderReflection* reflection)
    {
        dmLogInfo("Inputs: %d", reflection->m_Inputs.Size());
        for (uint32_t i = 0; i < reflection->m_Inputs.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s Location: %d, Type: %s",
                reflection->m_Inputs[i].m_Name,
                reflection->m_Inputs[i].m_InstanceName,
                reflection->m_Inputs[i].m_Location,
                ResolveTypeName(reflection, reflection->m_Inputs[i].m_Type));
        }

        dmLogInfo("Outputs: %d", reflection->m_Outputs.Size());
        for (uint32_t i = 0; i < reflection->m_Outputs.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s Location: %d, Type: %s",
                reflection->m_Outputs[i].m_Name,
                reflection->m_Outputs[i].m_InstanceName,
                reflection->m_Outputs[i].m_Location,
                ResolveTypeName(reflection, reflection->m_Outputs[i].m_Type));
        }

        dmLogInfo("Uniform buffers: %d", reflection->m_UniformBuffers.Size());
        for (uint32_t i = 0; i < reflection->m_UniformBuffers.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s, Set: %d, Binding: %d, Type: %s",
                reflection->m_UniformBuffers[i].m_Name,
                reflection->m_UniformBuffers[i].m_InstanceName,
                reflection->m_UniformBuffers[i].m_Set,
                reflection->m_UniformBuffers[i].m_Binding,
                ResolveTypeName(reflection, reflection->m_UniformBuffers[i].m_Type));
        }

        dmLogInfo("Textures: %d", reflection->m_Textures.Size());
        for (uint32_t i = 0; i < reflection->m_Textures.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Set: %d, Binding: %d, Type: %s",
                reflection->m_Textures[i].m_Name,
                reflection->m_Textures[i].m_Set,
                reflection->m_Textures[i].m_Binding,
                ResolveTypeName(reflection, reflection->m_Textures[i].m_Type));
        }

        dmLogInfo("Types: %d", reflection->m_Types.Size());
        for (uint32_t i = 0; i < reflection->m_Types.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Members: %d", reflection->m_Types[i].m_Name, reflection->m_Types[i].m_MemberCount);

            for (int j = 0; j < reflection->m_Types[i].m_MemberCount; ++j)
            {
                dmLogInfo("    Name: %s, Offset: %d, Type: %s",
                    reflection->m_Types[i].m_Members[j].m_Name,
                    reflection->m_Types[i].m_Members[j].m_Offset,
                    ResolveTypeName(reflection, reflection->m_Types[i].m_Members[j].m_Type));
            }
        }
    }
}
