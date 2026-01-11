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

#include <stdlib.h>
#include <string.h>

#include <spirv/spirv_cross_c.h>

#include <dmsdk/dlib/log.h>

#include "shaderc_private.h"

// Reference API impl:
// https://github.com/KhronosGroup/SPIRV-Cross/blob/main/tests-other/c_api_test.c

namespace dmShaderc
{
    static void _spvc_error_callback(void *userdata, const char *error)
    {
        dmLogError("Shaderc error: '%s'", error);
    }

    static struct BaseTypeMapping
    {
        BaseType      m_BaseType;
        const char*   m_Str;
    } BASE_TYPE_MAPPING[] = {
        {BASE_TYPE_UNKNOWN, "UNKNOWN"},
        {BASE_TYPE_VOID, "VOID"},
        {BASE_TYPE_BOOLEAN, "BOOLEAN"},
        {BASE_TYPE_INT8, "INT8"},
        {BASE_TYPE_UINT8, "UINT8"},
        {BASE_TYPE_INT16, "INT16"},
        {BASE_TYPE_UINT16, "UINT16"},
        {BASE_TYPE_INT32, "INT32"},
        {BASE_TYPE_UINT32, "UINT32"},
        {BASE_TYPE_INT64, "INT64"},
        {BASE_TYPE_UINT64, "UINT64"},
        {BASE_TYPE_ATOMIC_COUNTER, "ATOMIC_COUNTER"},
        {BASE_TYPE_FP16, "FP16"},
        {BASE_TYPE_FP32, "FP32"},
        {BASE_TYPE_FP64, "FP64"},
        {BASE_TYPE_STRUCT, "STRUCT"},
        {BASE_TYPE_IMAGE, "IMAGE"},
        {BASE_TYPE_SAMPLED_IMAGE, "SAMPLED_IMAGE"},
        {BASE_TYPE_SAMPLER, "SAMPLER"},
        {BASE_TYPE_ACCELERATION_STRUCTURE, "ACCELERATION_STRUCTURE"},
    };

    static struct DimensionTypeMapping
    {
        DimensionType  m_DimensionType;
        const char*    m_Str;
    } DIMENSION_TYPE_MAPPING[] {
        {DIMENSION_TYPE_1D, "1D"},
        {DIMENSION_TYPE_2D, "2D"},
        {DIMENSION_TYPE_3D, "3D"},
        {DIMENSION_TYPE_CUBE, "CUBE"},
        {DIMENSION_TYPE_RECT, "RECT"},
        {DIMENSION_TYPE_BUFFER, "BUFFER"},
        {DIMENSION_TYPE_SUBPASS_DATA, "SUBPASS_DATA"},
    };

    static struct ImageAccessQualifierMapping
    {
        ImageAccessQualifier m_ImageAccessQualifier;
        const char*          m_Str;
    } IMAGE_ACCESS_QUALIFIER_MAPPING[] = {
        {IMAGE_ACCESS_QUALIFIER_READ_ONLY, "READ_ONLY"},
        {IMAGE_ACCESS_QUALIFIER_WRITE_ONLY, "RITE_ONLY"},
        {IMAGE_ACCESS_QUALIFIER_READ_WRITE, "EAD_WRITE"},
    };

    static struct ImageStorageTypeMapping
    {
        ImageStorageType m_ImageStorageType;
        const char*      m_Str;
    } IMAGE_STORAGE_TYPE_MAPPING[] = {
        {IMAGE_STORAGE_TYPE_UNKNOWN, "UNKNOWN"},
        {IMAGE_STORAGE_TYPE_RGBA32F, "RGBA32F"},
        {IMAGE_STORAGE_TYPE_RGBA16F, "RGBA16F"},
        {IMAGE_STORAGE_TYPE_R32F, "R32F"},
        {IMAGE_STORAGE_TYPE_RGBA8, "RGBA8"},
        {IMAGE_STORAGE_TYPE_RGBA8_SNORM, "RGBA8_SNORM"},
        {IMAGE_STORAGE_TYPE_RG32F, "RG32F"},
        {IMAGE_STORAGE_TYPE_RG16F, "RG16F"},
        {IMAGE_STORAGE_TYPE_R11F_G11F_B10F, "R11F_G11F_B10F"},
        {IMAGE_STORAGE_TYPE_R16F, "R16F"},
        {IMAGE_STORAGE_TYPE_RGBA16, "RGBA16"},
        {IMAGE_STORAGE_TYPE_RGB10A2, "RGB10A2"},
        {IMAGE_STORAGE_TYPE_RG16, "RG16"},
        {IMAGE_STORAGE_TYPE_RG8, "RG8"},
        {IMAGE_STORAGE_TYPE_R16, "R16"},
        {IMAGE_STORAGE_TYPE_R8, "R8"},
        {IMAGE_STORAGE_TYPE_RGBA16_SNORM, "RGBA16_SNORM"},
        {IMAGE_STORAGE_TYPE_RG16_SNORM, "RG16_SNORM"},
        {IMAGE_STORAGE_TYPE_RG8_SNORM, "RG8_SNORM"},
        {IMAGE_STORAGE_TYPE_R16_SNORM, "R16_SNORM"},
        {IMAGE_STORAGE_TYPE_R8_SNORM, "R8_SNORM"},
        {IMAGE_STORAGE_TYPE_RGBA32I, "RGBA32I"},
        {IMAGE_STORAGE_TYPE_RGBA16I, "RGBA16I"},
        {IMAGE_STORAGE_TYPE_RGBA8I, "RGBA8I"},
        {IMAGE_STORAGE_TYPE_R32I, "R32I"},
        {IMAGE_STORAGE_TYPE_RG32I, "RG32I"},
        {IMAGE_STORAGE_TYPE_RG16I, "RG16I"},
        {IMAGE_STORAGE_TYPE_RG8I, "RG8I"},
        {IMAGE_STORAGE_TYPE_R16I, "R16I"},
        {IMAGE_STORAGE_TYPE_R8I, "R8I"},
        {IMAGE_STORAGE_TYPE_RGBA32UI, "RGBA32UI"},
        {IMAGE_STORAGE_TYPE_RGBA16UI, "RGBA16UI"},
        {IMAGE_STORAGE_TYPE_RGBA8UI, "RGBA8UI"},
        {IMAGE_STORAGE_TYPE_R32UI, "R32UI"},
        {IMAGE_STORAGE_TYPE_RGb10a2UI, "RGb10a2UI"},
        {IMAGE_STORAGE_TYPE_RG32UI, "RG32UI"},
        {IMAGE_STORAGE_TYPE_RG16UI, "RG16UI"},
        {IMAGE_STORAGE_TYPE_RG8UI, "RG8UI"},
        {IMAGE_STORAGE_TYPE_R16UI, "R16UI"},
        {IMAGE_STORAGE_TYPE_R8UI, "R8UI"},
        {IMAGE_STORAGE_TYPE_R64UI, "R64UI"},
        {IMAGE_STORAGE_TYPE_R64I, "R64I"},
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

        uint32_t member_count = spvc_type_get_num_member_types(type);

        uint32_t type_index = reflection.m_Types.Size();

        reflection.m_Types.OffsetCapacity(1);
        reflection.m_Types.SetSize(reflection.m_Types.Capacity());

        ResourceTypeInfo* type_info = &reflection.m_Types.Back();
        memset(type_info, 0, sizeof(ResourceTypeInfo));

        type_info->m_Name = type_name;
        type_info->m_NameHash = type_name_hash;

        type_info->m_Members.SetCapacity(member_count);
        type_info->m_Members.SetSize(member_count);
        ResourceMember* members = type_info->m_Members.Begin();
        memset(members, 0, sizeof(ResourceMember) * member_count);

        for (int i = 0; i < member_count; ++i)
        {
            spvc_type_id member_type_id    = spvc_type_get_member_type(type, i);
            spvc_type member_type          = spvc_compiler_get_type_handle(compiler, member_type_id);
            spvc_basetype member_base_type = spvc_type_get_basetype(member_type);

            unsigned member_offset = 0;
            spvc_compiler_type_struct_member_offset(compiler, type, i, &member_offset);

            unsigned num_array_dimensions = spvc_type_get_num_array_dimensions(member_type);
            if (num_array_dimensions > 1)
            {
                dmLogWarning("Unsupported array dimension: %u", num_array_dimensions);
            }

            ResourceMember& member    = members[i];
            member.m_Offset           = (uint32_t) member_offset;
            member.m_Name             = spvc_compiler_get_member_name(compiler, type_id, i);
            member.m_NameHash         = dmHashString64(member.m_Name);
            member.m_Type.m_ArraySize = 1;

            // TODO: We only support one array dimension right now
            if (num_array_dimensions > 0)
            {
                member_type_id = spvc_type_get_base_type_id(member_type);
                member.m_Type.m_ArraySize = spvc_type_get_array_dimension(member_type, 0);
            }

            if (member_base_type == SPVC_BASETYPE_STRUCT)
            {
                const char* name             = spvc_compiler_get_name(compiler, member_type_id);
                member.m_Type.m_TypeIndex    = GetTypeIndex(reflection, compiler, name, member_type_id, member_type);
                member.m_Type.m_UseTypeIndex = true;
            }
            else
            {
                member.m_Type.m_BaseType     = BASE_TYPE_MAPPING[member_base_type].m_BaseType;
                member.m_Type.m_UseTypeIndex = false;
                member.m_Type.m_VectorSize   = spvc_type_get_vector_size(member_type);
                member.m_Type.m_ColumnCount  = spvc_type_get_columns(member_type);
            }
        }

        return type_index;
    }

    static void SetReflectionResourceForType(ShaderStage stage, ShaderReflection& reflection, spvc_compiler compiler, spvc_resources resources, spvc_resource_type type, dmArray<ShaderResource>& resources_out)
    {
        const spvc_reflected_resource *list = NULL;
        size_t count = 0;
        spvc_resources_get_resource_list_for_type(resources, type, &list, &count);

        if (count == 0)
            return;

        uint32_t write_index = resources_out.Size();

        if (resources_out.Remaining() < count)
        {
            resources_out.OffsetCapacity(count);
            resources_out.SetSize(resources_out.Capacity());
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            spvc_type type          = spvc_compiler_get_type_handle(compiler, list[i].type_id);
            spvc_basetype base_type = spvc_type_get_basetype(type);

            ShaderResource& resource = resources_out[write_index + i];
            memset(&resource, 0, sizeof(ShaderResource));

            resource.m_Id               = list[i].id;
            resource.m_Name             = list[i].name;
            resource.m_NameHash         = dmHashString64(list[i].name);
            resource.m_InstanceName     = spvc_compiler_get_name(compiler, list[i].id);
            resource.m_InstanceNameHash = dmHashString64(resource.m_InstanceName);
            resource.m_Set              = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationDescriptorSet);
            resource.m_Binding          = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);
            resource.m_Location         = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationLocation);
            resource.m_StageFlags       = (int) stage;

            if (base_type == SPVC_BASETYPE_STRUCT)
            {
                size_t size = 0;
                spvc_compiler_get_declared_struct_size(compiler, type, &size);

                resource.m_BlockSize           = (uint32_t) size;
                resource.m_Type.m_TypeIndex    = GetTypeIndex(reflection, compiler, resource.m_Name, list[i].base_type_id, type);
                resource.m_Type.m_UseTypeIndex = true;
            }
            else
            {
                resource.m_Type.m_BaseType      = BASE_TYPE_MAPPING[base_type].m_BaseType;
                resource.m_Type.m_UseTypeIndex  = false;
                resource.m_Type.m_VectorSize    = spvc_type_get_vector_size(type);
                resource.m_Type.m_ColumnCount   = spvc_type_get_columns(type);
                resource.m_Type.m_DimensionType = (DimensionType) 0;

                if (base_type == SPVC_BASETYPE_IMAGE || base_type == SPVC_BASETYPE_SAMPLED_IMAGE)
                {
                    resource.m_Type.m_ImageIsArrayed   = spvc_type_get_image_arrayed(type);
                    resource.m_Type.m_ImageIsStorage   = spvc_type_get_image_is_storage(type);
                    resource.m_Type.m_DimensionType    = DIMENSION_TYPE_MAPPING[spvc_type_get_image_dimension(type)].m_DimensionType;
                    resource.m_Type.m_ImageStorageType = IMAGE_STORAGE_TYPE_MAPPING[spvc_type_get_image_storage_format(type)].m_ImageStorageType;

                    SpvAccessQualifier qualifier = spvc_type_get_image_access_qualifier(type);
                    if (qualifier != SpvAccessQualifierMax)
                    {
                        resource.m_Type.m_ImageAccessQualifier = IMAGE_ACCESS_QUALIFIER_MAPPING[qualifier].m_ImageAccessQualifier;
                    }

                    spvc_type_id sampled_type_it    = spvc_type_get_image_sampled_type(type);
                    spvc_type sample_type           = spvc_compiler_get_type_handle(compiler, sampled_type_it);
                    spvc_basetype sampled_base_type = spvc_type_get_basetype(sample_type);
                    resource.m_Type.m_ImageBaseType = BASE_TYPE_MAPPING[sampled_base_type].m_BaseType;
                }
            }
        }
    }

    static void SetReflectionInternal(HShaderContext context)
    {
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, context->m_Reflection.m_Inputs);
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, context->m_Reflection.m_Outputs);
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, context->m_Reflection.m_UniformBuffers);
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, context->m_Reflection.m_StorageBuffers);

        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, context->m_Reflection.m_Textures);
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, context->m_Reflection.m_Textures);
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, context->m_Reflection.m_Textures);
        SetReflectionResourceForType(context->m_Stage, context->m_Reflection, context->m_CompilerNone, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, context->m_Reflection.m_Textures);
    }

    HShaderContext NewShaderContext(ShaderStage stage, const void* source, uint32_t source_size)
    {
        ShaderContext* context = (ShaderContext*) malloc(sizeof(ShaderContext));
        memset(context, 0, sizeof(ShaderContext));

        context->m_ShaderCode     = (uint8_t*) malloc(source_size);
        context->m_ShaderCodeSize = source_size;
        context->m_Stage          = stage;
        memcpy(context->m_ShaderCode, source, source_size);

        switch(context->m_Stage)
        {
        case SHADER_STAGE_VERTEX:
            context->m_ExecutionModel = SpvExecutionModelVertex;
            break;
        case SHADER_STAGE_FRAGMENT:
            context->m_ExecutionModel = SpvExecutionModelFragment;
            break;
        case SHADER_STAGE_COMPUTE:
            context->m_ExecutionModel = SpvExecutionModelGLCompute;
            break;
        default: break;
        }

        SpvId* as_spv_ptr = (SpvId*) source;
        size_t word_count = source_size / sizeof(SpvId);

        spvc_context_create(&context->m_SPVCContext);
        spvc_context_set_error_callback(context->m_SPVCContext, _spvc_error_callback, NULL);
        spvc_context_parse_spirv(context->m_SPVCContext, as_spv_ptr, word_count, &context->m_ParsedIR);

        // Create a reflection compiler
        spvc_context_create_compiler(context->m_SPVCContext, SPVC_BACKEND_NONE, context->m_ParsedIR, SPVC_CAPTURE_MODE_COPY, &context->m_CompilerNone);
        spvc_compiler_create_shader_resources(context->m_CompilerNone, &context->m_Resources);

        SetReflectionInternal(context);

        return (HShaderContext) context;
    }

    static void DeleteReflection(ShaderReflection* reflection)
    {
        reflection->m_Inputs.SetCapacity(0);
        reflection->m_Outputs.SetCapacity(0);
        reflection->m_UniformBuffers.SetCapacity(0);
        reflection->m_StorageBuffers.SetCapacity(0);
        reflection->m_Textures.SetCapacity(0);

        for (uint32_t i = 0; i < reflection->m_Types.Size(); ++i)
        {
            ResourceTypeInfo* type = &reflection->m_Types[i];
            type->m_Members.SetCapacity(0);
        }
        reflection->m_Types.SetCapacity(0);
    }

    void DeleteShaderContext(HShaderContext context)
    {
        DeleteReflection(&context->m_Reflection);
        spvc_context_destroy(context->m_SPVCContext);
        free(context->m_ShaderCode);
        free(context);
    }

    ShaderCompilerSPVC* NewShaderCompilerSPVC(HShaderContext context, ShaderLanguage language)
    {
        ShaderCompilerSPVC* compiler = (ShaderCompilerSPVC*) malloc(sizeof(ShaderCompilerSPVC));
        memset(compiler, 0, sizeof(ShaderCompilerSPVC));

        compiler->m_BaseCompiler.m_Language = language;

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
            case SHADER_LANGUAGE_MSL:
                backend = SPVC_BACKEND_MSL;
                break;
            default:break;
        }

        spvc_context_create_compiler(context->m_SPVCContext, backend, context->m_ParsedIR, SPVC_CAPTURE_MODE_COPY, &compiler->m_SPVCCompiler);

        return compiler;
    }

    void DeleteShaderCompilerSPVC(ShaderCompilerSPVC* compiler)
    {
        free(compiler);
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

    void SetResourceLocationSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, dmhash_t name_hash, uint8_t location)
    {
        SetLocationForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_INPUT, name_hash, location);
        SetLocationForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STAGE_OUTPUT, name_hash, location);
    }

    static void SetBindingOrSetForType(spvc_compiler& compiler, spvc_resources resources, spvc_resource_type type, dmhash_t name_hash, uint8_t* binding, uint8_t* set)
    {
        const spvc_reflected_resource *list = NULL;
        size_t count = 0;
        spvc_resources_get_resource_list_for_type(resources, type, &list, &count);

        for (uint32_t i = 0; i < count; ++i)
        {
            dmhash_t list_name_hash = dmHashString64(list[i].name);
            if (list_name_hash == name_hash)
            {
                if (binding)
                {
                    spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationBinding, *binding);
                }
                if (set)
                {
                    spvc_compiler_set_decoration(compiler, list[i].id, SpvDecorationDescriptorSet, *set);
                }
            }
        }
    }

    void SetResourceBindingSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, dmhash_t name_hash, uint8_t binding)
    {
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, name_hash, &binding, 0);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, name_hash, &binding, 0);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, name_hash, &binding, 0);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, name_hash, &binding, 0);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, name_hash, &binding, 0);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, name_hash, &binding, 0);
    }

    void SetResourceSetSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, dmhash_t name_hash, uint8_t set)
    {
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, name_hash, 0, &set);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, name_hash, 0, &set);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, name_hash, 0, &set);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, name_hash, 0, &set);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, name_hash, 0, &set);
        SetBindingOrSetForType(compiler->m_SPVCCompiler, context->m_Resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, name_hash, 0, &set);
    }

    void GetCombinedSamplerMapSPIRV(HShaderContext context, ShaderCompilerSPVC* compiler, dmArray<CombinedSampler>& combined_samplers)
    {
        const spvc_combined_image_sampler* samplers;
        size_t sampler_count;
        spvc_compiler_get_combined_image_samplers(compiler->m_SPVCCompiler, &samplers, &sampler_count);

        combined_samplers.SetCapacity(sampler_count);
        combined_samplers.SetSize(sampler_count);

        for (size_t i = 0; i < sampler_count; ++i)
        {
            const spvc_combined_image_sampler* s = &samplers[i];
            combined_samplers[i].m_CombinedName = spvc_compiler_get_name(compiler->m_SPVCCompiler, s->combined_id);
            combined_samplers[i].m_CombinedId   = s->combined_id;
            combined_samplers[i].m_ImageName    = spvc_compiler_get_name(compiler->m_SPVCCompiler, s->image_id);
            combined_samplers[i].m_ImageId      = s->image_id;
            combined_samplers[i].m_SamplerName  = spvc_compiler_get_name(compiler->m_SPVCCompiler, s->sampler_id);
            combined_samplers[i].m_SamplerId    = s->sampler_id;
        }
    }

    #define MAX_BINDINGS 128
    bool GetFirstFreeBindingIndex(HShaderContext context, uint32_t* binding)
    {
        uint8_t used[MAX_BINDINGS] = {0};

        for (int i = 0; i < context->m_Reflection.m_UniformBuffers.Size(); ++i)
        {
            if (context->m_Reflection.m_UniformBuffers[i].m_Binding < MAX_BINDINGS)
                used[context->m_Reflection.m_UniformBuffers[i].m_Binding] = 1;
        }

        for (int i = 0; i < context->m_Reflection.m_StorageBuffers.Size(); ++i)
        {
            if (context->m_Reflection.m_StorageBuffers[i].m_Binding < MAX_BINDINGS)
                used[context->m_Reflection.m_StorageBuffers[i].m_Binding] = 1;
        }

        for (int i = 0; i < context->m_Reflection.m_Textures.Size(); ++i)
        {
            if (context->m_Reflection.m_Textures[i].m_Binding < MAX_BINDINGS)
                used[context->m_Reflection.m_Textures[i].m_Binding] = 1;
        }

        for (uint32_t i = 0; i < MAX_BINDINGS; ++i)
        {
            if (!used[i])
            {
                *binding = i;
                return true;
            }
        }

        return false; // No free binding index available
    }
    #undef MAX_BINDINGS

    ShaderCompileResult* CompileSPVC(HShaderContext context, ShaderCompilerSPVC* compiler, const ShaderCompilerOptions& options)
    {
        spvc_compiler_options spv_options = NULL;
        spvc_compiler_create_compiler_options(compiler->m_SPVCCompiler, &spv_options);

        bool can_remove_unused_variables = true;
        uint8_t hlsl_num_workgroups_id_binding = 0xff;

        spvc_compiler_set_entry_point(compiler->m_SPVCCompiler, options.m_EntryPoint, context->m_ExecutionModel);
        spvc_compiler_build_combined_image_samplers(compiler->m_SPVCCompiler);

        if (compiler->m_BaseCompiler.m_Language == SHADER_LANGUAGE_GLSL)
        {
            spvc_compiler_options_set_uint(spv_options, SPVC_COMPILER_OPTION_GLSL_VERSION,                  options.m_Version);
            spvc_compiler_options_set_bool(spv_options, SPVC_COMPILER_OPTION_GLSL_ES,                       options.m_GlslEs);
            spvc_compiler_options_set_bool(spv_options, SPVC_COMPILER_OPTION_GLSL_ENABLE_420PACK_EXTENSION, !options.m_No420PackExtension);

            // TODO:
            // SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_FLOAT_PRECISION_HIGHP
            // SPVC_COMPILER_OPTION_GLSL_ES_DEFAULT_INT_PRECISION_HIGHP

        }
        else if (compiler->m_BaseCompiler.m_Language == SHADER_LANGUAGE_HLSL)
        {
            spvc_compiler_options_set_uint(spv_options, SPVC_COMPILER_OPTION_HLSL_SHADER_MODEL, options.m_Version);

            if (context->m_Stage == SHADER_STAGE_COMPUTE)
            {
                spvc_variable_id num_workgroups_id = spvc_compiler_hlsl_remap_num_workgroups_builtin(compiler->m_SPVCCompiler);

                if (num_workgroups_id)
                {
                    // If we remove unused variables, the cbuffer that contains the gl_NumWorkGroups data will be removed..
                    can_remove_unused_variables = false;

                    // Find the lowest unused register
                    uint32_t free_binding;

                    if (GetFirstFreeBindingIndex(context, &free_binding))
                    {
                        spvc_compiler_set_decoration(compiler->m_SPVCCompiler, num_workgroups_id, SpvDecorationBinding, free_binding);
                        spvc_compiler_set_decoration(compiler->m_SPVCCompiler, num_workgroups_id, SpvDecorationDescriptorSet, HLSL_NUM_WORKGROUPS_SET);

                        hlsl_num_workgroups_id_binding = free_binding;
                    }
                    else
                    {
                        dmLogInfo("HLSL: No free cbuffer binding found for gl_NumWorkGroups");
                    }
                }
            }
        }
        else if (compiler->m_BaseCompiler.m_Language == SHADER_LANGUAGE_MSL)
        {
            uint32_t msl_version = SPVC_MAKE_MSL_VERSION(2, 2, 0);

            switch(options.m_Version)
            {
                case 11: msl_version = SPVC_MAKE_MSL_VERSION(1, 1, 0); break;
                case 12: msl_version = SPVC_MAKE_MSL_VERSION(1, 2, 0); break;
                case 20: msl_version = SPVC_MAKE_MSL_VERSION(2, 0, 0); break;
                case 21: msl_version = SPVC_MAKE_MSL_VERSION(2, 1, 0); break;
                case 22: msl_version = SPVC_MAKE_MSL_VERSION(2, 2, 0); break;
                case 23: msl_version = SPVC_MAKE_MSL_VERSION(2, 3, 0); break;
                case 24: msl_version = SPVC_MAKE_MSL_VERSION(2, 4, 0); break;
                case 30: msl_version = SPVC_MAKE_MSL_VERSION(3, 0, 0); break;
                case 31: msl_version = SPVC_MAKE_MSL_VERSION(3, 1, 0); break;
                case 32: msl_version = SPVC_MAKE_MSL_VERSION(3, 2, 0); break;
                default: dmLogWarning("MSL version %d not supported for crosscompiling, defaulting to 2.2", options.m_Version);
            }

            spvc_compiler_options_set_uint(spv_options, SPVC_COMPILER_OPTION_MSL_VERSION, msl_version);
            spvc_compiler_options_set_uint(spv_options, SPVC_COMPILER_OPTION_MSL_PLATFORM, SPVC_MSL_PLATFORM_MACOS);
            spvc_compiler_options_set_bool(spv_options, SPVC_COMPILER_OPTION_MSL_ARGUMENT_BUFFERS, SPVC_TRUE);
            spvc_compiler_options_set_bool(spv_options, SPVC_COMPILER_OPTION_MSL_EMULATE_CUBEMAP_ARRAY, SPVC_FALSE);
        }
        else
        {
            assert(0 && "Shader langauge not yet supported");
        }

        if (options.m_RemoveUnusedVariables && can_remove_unused_variables)
        {
            spvc_set active_variables;
            spvc_resources active_resources;
            spvc_compiler_get_active_interface_variables(compiler->m_SPVCCompiler, &active_variables);
            spvc_compiler_create_shader_resources_for_active_variables(compiler->m_SPVCCompiler, &active_resources, active_variables);
            spvc_compiler_set_enabled_interface_variables(compiler->m_SPVCCompiler, active_variables);
        }

        spvc_compiler_install_compiler_options(compiler->m_SPVCCompiler, spv_options);

        const char *compile_result = NULL;
        spvc_compiler_compile(compiler->m_SPVCCompiler, &compile_result);

        uint32_t compile_result_size = 0;
        if (compile_result)
        {
            compile_result_size = strlen(compile_result);
        }

        ShaderCompileResult* result = (ShaderCompileResult*) malloc(sizeof(ShaderCompileResult));
        memset(result, 0, sizeof(ShaderCompileResult));
        result->m_Data.SetCapacity(compile_result_size);
        result->m_Data.SetSize(compile_result_size);
        result->m_LastError = "";
        result->m_HLSLNumWorkGroupsId = hlsl_num_workgroups_id_binding;

        if (compile_result)
        {
            memcpy(result->m_Data.Begin(), compile_result, result->m_Data.Size());
        }
        else
        {
            result->m_LastError = spvc_context_get_last_error_string(context->m_SPVCContext);
        }

        return result;
    }

    static const char* ResolveTypeName(const ShaderReflection* reflection, const ResourceType& type)
    {
        if (type.m_UseTypeIndex)
        {
            return reflection->m_Types[type.m_TypeIndex].m_Name;
        }
        else
        {
            return BASE_TYPE_MAPPING[type.m_BaseType].m_Str;
        }
    }

    void DebugPrintReflection(const ShaderReflection* reflection)
    {
        dmLogInfo("Inputs: %d", reflection->m_Inputs.Size());
        for (uint32_t i = 0; i < reflection->m_Inputs.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s, Id: %d, Location: %d, Type: %s",
                reflection->m_Inputs[i].m_Name,
                reflection->m_Inputs[i].m_InstanceName,
                reflection->m_Inputs[i].m_Id,
                reflection->m_Inputs[i].m_Location,
                ResolveTypeName(reflection, reflection->m_Inputs[i].m_Type));
        }

        dmLogInfo("Outputs: %d", reflection->m_Outputs.Size());
        for (uint32_t i = 0; i < reflection->m_Outputs.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s, Id: %d, Location: %d, Type: %s",
                reflection->m_Outputs[i].m_Name,
                reflection->m_Outputs[i].m_InstanceName,
                reflection->m_Outputs[i].m_Id,
                reflection->m_Outputs[i].m_Location,
                ResolveTypeName(reflection, reflection->m_Outputs[i].m_Type));
        }

        dmLogInfo("Uniform buffers: %d", reflection->m_UniformBuffers.Size());
        for (uint32_t i = 0; i < reflection->m_UniformBuffers.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s, Id: %d, Set: %d, Binding: %d, Type: %s, BlockSize: %d",
                reflection->m_UniformBuffers[i].m_Name,
                reflection->m_UniformBuffers[i].m_InstanceName,
                reflection->m_UniformBuffers[i].m_Id,
                reflection->m_UniformBuffers[i].m_Set,
                reflection->m_UniformBuffers[i].m_Binding,
                ResolveTypeName(reflection, reflection->m_UniformBuffers[i].m_Type),
                reflection->m_UniformBuffers[i].m_BlockSize);
        }

        dmLogInfo("Storage buffers: %d", reflection->m_StorageBuffers.Size());
        for (uint32_t i = 0; i < reflection->m_StorageBuffers.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Instance-name: %s, Set: %d, Binding: %d, Type: %s, BlockSize: %d",
                reflection->m_StorageBuffers[i].m_Name,
                reflection->m_StorageBuffers[i].m_InstanceName,
                reflection->m_StorageBuffers[i].m_Set,
                reflection->m_StorageBuffers[i].m_Binding,
                ResolveTypeName(reflection, reflection->m_StorageBuffers[i].m_Type),
                reflection->m_StorageBuffers[i].m_BlockSize);
        }

        dmLogInfo("Textures: %d", reflection->m_Textures.Size());
        for (uint32_t i = 0; i < reflection->m_Textures.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Set: %d, Binding: %d, Id: %d, BaseType: %s, Type: %s, DimensionType: %s, IsArrayed: %s, IsStorage: %s, StorageType: %s, AccessQualifier: %s",
                reflection->m_Textures[i].m_Name,
                reflection->m_Textures[i].m_Set,
                reflection->m_Textures[i].m_Binding,
                reflection->m_Textures[i].m_Id,
                BASE_TYPE_MAPPING[reflection->m_Textures[i].m_Type.m_ImageBaseType].m_Str,
                ResolveTypeName(reflection, reflection->m_Textures[i].m_Type),
                DIMENSION_TYPE_MAPPING[reflection->m_Textures[i].m_Type.m_DimensionType].m_Str,
                reflection->m_Textures[i].m_Type.m_ImageIsArrayed ? "true" : "false",
                reflection->m_Textures[i].m_Type.m_ImageIsStorage ? "true" : "false",
                IMAGE_STORAGE_TYPE_MAPPING[reflection->m_Textures[i].m_Type.m_ImageStorageType].m_Str,
                IMAGE_ACCESS_QUALIFIER_MAPPING[reflection->m_Textures[i].m_Type.m_ImageAccessQualifier].m_Str);
        }

        dmLogInfo("Types: %d", reflection->m_Types.Size());
        for (uint32_t i = 0; i < reflection->m_Types.Size(); ++i)
        {
            dmLogInfo("  Name: %s, Members: %d", reflection->m_Types[i].m_Name, reflection->m_Types[i].m_Members.Size());

            for (int j = 0; j < reflection->m_Types[i].m_Members.Size(); ++j)
            {
                dmLogInfo("    Name: %s, Offset: %d, Type: %s, VectorSize: %d, ArraySize: %d, ColumnCount: %d",
                    reflection->m_Types[i].m_Members[j].m_Name,
                    reflection->m_Types[i].m_Members[j].m_Offset,
                    ResolveTypeName(reflection, reflection->m_Types[i].m_Members[j].m_Type),
                    reflection->m_Types[i].m_Members[j].m_Type.m_VectorSize,
                    reflection->m_Types[i].m_Members[j].m_Type.m_ArraySize,
                    reflection->m_Types[i].m_Members[j].m_Type.m_ColumnCount);
            }
        }
    }
}
