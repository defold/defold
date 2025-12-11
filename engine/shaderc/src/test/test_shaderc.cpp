// Copyright 2020-2025 The Defold Foundation
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

#include "shaderc.h"

#include "shaderc_private.h"

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

static void* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* file = fopen(path, "rb");
    if (!file)
    {
        printf("Failed to load %s\n", path);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    size_t size = (size_t)ftell(file);
    fseek(file, 0, SEEK_SET);

    void* mem = malloc(size);

    size_t nread = fread(mem, 1, size, file);
    fclose(file);

    if (nread != size)
    {
        printf("Failed to read %u bytes from %s\n", (uint32_t)size, path);
        free(mem);
        return 0;
    }

    if (file_size)
    {
        *file_size = size;
    }

    return mem;
}

TEST(Shaderc, TestSimpleShader)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/simple.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_VERTEX, data, data_size);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, TestShaderReflection)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/reflection.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

    ASSERT_EQ(3, reflection->m_UniformBuffers.Size());

    ASSERT_EQ(dmHashString64("ubo_global_one"), reflection->m_UniformBuffers[0].m_NameHash);
    ASSERT_EQ(0,                                reflection->m_UniformBuffers[0].m_InstanceNameHash);

    ASSERT_EQ(dmHashString64("ubo_global_two"), reflection->m_UniformBuffers[1].m_NameHash);
    ASSERT_EQ(0,                                reflection->m_UniformBuffers[1].m_InstanceNameHash);

    ASSERT_EQ(dmHashString64("ubo_inst"),       reflection->m_UniformBuffers[2].m_NameHash);
    ASSERT_EQ(dmHashString64("inst_variable"),  reflection->m_UniformBuffers[2].m_InstanceNameHash);

    dmShaderc::DeleteShaderContext(shader_ctx);
}

static const dmShaderc::ShaderResource* GetShaderResourceByNameHash(const dmArray<dmShaderc::ShaderResource>& resource_list, dmhash_t name_hash)
{
    for (uint32_t i = 0; i < resource_list.Size(); ++i)
    {
        if (resource_list[i].m_NameHash == name_hash)
        {
            return &resource_list[i];
        }
    }
    return 0;
}

static inline const char* FindFirstOccurance(const char* src, const char* text)
{
    return strstr(src, text);
}

TEST(Shaderc, ModifyBindings)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/bindings.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_VERTEX, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

    const dmShaderc::ShaderResource* position  = GetShaderResourceByNameHash(reflection->m_Inputs, dmHashString64("position"));
    const dmShaderc::ShaderResource* normal    = GetShaderResourceByNameHash(reflection->m_Inputs, dmHashString64("normal"));
    const dmShaderc::ShaderResource* tex_coord = GetShaderResourceByNameHash(reflection->m_Inputs, dmHashString64("tex_coord"));

    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_GLSL);

#if 0
    dmShaderc::DebugPrintReflection(reflection);
#endif

    ASSERT_NE((void*) 0, position);
    ASSERT_EQ(0, position->m_Location);

    ASSERT_NE((void*) 0, normal);
    ASSERT_EQ(1, normal->m_Location);

    ASSERT_NE((void*) 0, tex_coord);
    ASSERT_EQ(2, tex_coord->m_Location);

    dmShaderc::SetResourceLocation(shader_ctx, compiler, dmHashString64("position"), 3);
    dmShaderc::SetResourceLocation(shader_ctx, compiler, dmHashString64("normal"), 4);
    dmShaderc::SetResourceLocation(shader_ctx, compiler, dmHashString64("tex_coord"), 5);

    dmShaderc::SetResourceBinding(shader_ctx, compiler, dmHashString64("matrices"), 3);
    dmShaderc::SetResourceBinding(shader_ctx, compiler, dmHashString64("extra"), 4);

    dmShaderc::ShaderCompilerOptions options;
    options.m_Version = 460;

    dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst->m_Data.Begin());

    ASSERT_NE((const char*) 0, FindFirstOccurance((const char*) dst->m_Data.Begin(), "layout(location = 3) in vec4 position;"));
    ASSERT_NE((const char*) 0, FindFirstOccurance((const char*) dst->m_Data.Begin(), "layout(location = 4) in vec3 normal;"));
    ASSERT_NE((const char*) 0, FindFirstOccurance((const char*) dst->m_Data.Begin(), "layout(location = 5) in vec2 tex_coord;"));

    ASSERT_NE((const char*) 0, FindFirstOccurance((const char*) dst->m_Data.Begin(), "layout(binding = 3, std140) uniform matrices"));
    ASSERT_NE((const char*) 0, FindFirstOccurance((const char*) dst->m_Data.Begin(), "layout(binding = 4, std140) uniform extra"));

    dmShaderc::DeleteShaderCompiler(compiler);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, TestCompilerSPIRV)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/simple.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);
    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_SPIRV);

    dmShaderc::ShaderCompilerOptions options;
    dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst->m_Data.Begin());

    ASSERT_EQ(data_size, dst->m_Data.Size());
    ASSERT_EQ(0, memcmp(data, dst->m_Data.Begin(), data_size));

    dmShaderc::DeleteShaderCompiler(compiler);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, ModifyBindingsSPIRV)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/bindings.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_VERTEX, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_SPIRV);

    dmShaderc::SetResourceLocation(shader_ctx, compiler, dmHashString64("position"),  3);
    dmShaderc::SetResourceLocation(shader_ctx, compiler, dmHashString64("normal"),    4);
    dmShaderc::SetResourceLocation(shader_ctx, compiler, dmHashString64("tex_coord"), 5);

    dmShaderc::ShaderCompilerOptions options = {};

    dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst->m_Data.Begin());

    {
        dmShaderc::HShaderContext spirv_shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_VERTEX, dst->m_Data.Begin(), dst->m_Data.Size());
        const dmShaderc::ShaderReflection* spirv_reflection = dmShaderc::GetReflection(spirv_shader_ctx);
        ASSERT_NE((void*) 0, reflection);

        const dmShaderc::ShaderResource* position  = GetShaderResourceByNameHash(spirv_reflection->m_Inputs, dmHashString64("position"));
        const dmShaderc::ShaderResource* normal    = GetShaderResourceByNameHash(spirv_reflection->m_Inputs, dmHashString64("normal"));
        const dmShaderc::ShaderResource* tex_coord = GetShaderResourceByNameHash(spirv_reflection->m_Inputs, dmHashString64("tex_coord"));

        ASSERT_EQ(3, position->m_Location);
        ASSERT_EQ(4, normal->m_Location);
        ASSERT_EQ(5, tex_coord->m_Location);

        dmShaderc::DeleteShaderContext(spirv_shader_ctx);
    }

    dmShaderc::DeleteShaderCompiler(compiler);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

static const dmShaderc::ResourceTypeInfo* GetType(const dmShaderc::ShaderReflection* reflection, dmhash_t name_hash)
{
    for (uint32_t i = 0; i < reflection->m_Types.Size(); ++i)
    {
        if (reflection->m_Types[i].m_NameHash == name_hash)
        {
            return &reflection->m_Types[i];
        }
    }
    return 0;
}

static const dmShaderc::ShaderResource* GetShaderResource(const dmArray<dmShaderc::ShaderResource>& resources, dmhash_t name_hash)
{
    for (uint32_t i = 0; i < resources.Size(); ++i)
    {
        if (resources[i].m_NameHash == name_hash)
        {
            return &resources[i];
        }
    }
    return 0;
}

static void AssertResourceMember(const dmShaderc::ResourceMember* member, const char* name, uint32_t offset, dmShaderc::BaseType base_type, uint32_t vector_size)
{
    ASSERT_EQ(dmHashString64(name), member->m_NameHash);
    ASSERT_EQ(offset,               member->m_Offset);
    ASSERT_EQ(base_type,            member->m_Type.m_BaseType);
    ASSERT_EQ(vector_size,          member->m_Type.m_VectorSize);
}

static void AssertTexture(const dmShaderc::ShaderReflection* reflection, const char* name, dmShaderc::BaseType base_type, dmShaderc::DimensionType dimension, bool is_arrayed)
{
    const dmShaderc::ShaderResource* tex = GetShaderResource(reflection->m_Textures, dmHashString64(name));
    ASSERT_NE((void*) 0, tex);
    ASSERT_EQ(base_type, tex->m_Type.m_BaseType);
    ASSERT_EQ(dimension, tex->m_Type.m_DimensionType);
    ASSERT_EQ(is_arrayed, tex->m_Type.m_ImageIsArrayed);
}

TEST(Shaderc, Types)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/types.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

#if 0
    dmShaderc::DebugPrintReflection(reflection);
#endif

    const dmShaderc::ResourceTypeInfo* data_types = GetType(reflection, dmHashString64("data_types"));
    ASSERT_NE((void*) 0, data_types);
    ASSERT_EQ(dmHashString64("data_types"), data_types->m_NameHash);
    ASSERT_EQ(12, data_types->m_Members.Size());

    AssertResourceMember(&data_types->m_Members[0], "type_int", 0, dmShaderc::BASE_TYPE_INT32, 1);
    AssertResourceMember(&data_types->m_Members[1], "type_uint", 4, dmShaderc::BASE_TYPE_UINT32, 1);
    AssertResourceMember(&data_types->m_Members[2], "type_float", 8, dmShaderc::BASE_TYPE_FP32, 1);
    AssertResourceMember(&data_types->m_Members[3], "type_vec2", 16, dmShaderc::BASE_TYPE_FP32, 2);
    AssertResourceMember(&data_types->m_Members[4], "type_vec3", 32, dmShaderc::BASE_TYPE_FP32, 3);
    AssertResourceMember(&data_types->m_Members[5], "type_vec4", 48, dmShaderc::BASE_TYPE_FP32, 4);
    AssertResourceMember(&data_types->m_Members[6], "type_mat2", 64, dmShaderc::BASE_TYPE_FP32, 2);
    AssertResourceMember(&data_types->m_Members[7], "type_mat3", 96, dmShaderc::BASE_TYPE_FP32, 3);
    AssertResourceMember(&data_types->m_Members[8], "type_mat4", 144, dmShaderc::BASE_TYPE_FP32, 4);
    AssertResourceMember(&data_types->m_Members[9], "type_uvec2", 208, dmShaderc::BASE_TYPE_UINT32, 2);
    AssertResourceMember(&data_types->m_Members[10], "type_uvec3", 224, dmShaderc::BASE_TYPE_UINT32, 3);
    AssertResourceMember(&data_types->m_Members[11], "type_uvec4", 240, dmShaderc::BASE_TYPE_UINT32, 4);

    ASSERT_EQ(12, reflection->m_Textures.Size());
    AssertTexture(reflection, "type_sampler2D", dmShaderc::BASE_TYPE_SAMPLED_IMAGE, dmShaderc::DIMENSION_TYPE_2D, false);
    AssertTexture(reflection, "type_sampler3D", dmShaderc::BASE_TYPE_SAMPLED_IMAGE, dmShaderc::DIMENSION_TYPE_3D, false);
    AssertTexture(reflection, "type_samplerCube", dmShaderc::BASE_TYPE_SAMPLED_IMAGE, dmShaderc::DIMENSION_TYPE_CUBE, false);
    AssertTexture(reflection, "type_sampler2DArray", dmShaderc::BASE_TYPE_SAMPLED_IMAGE, dmShaderc::DIMENSION_TYPE_2D, true);
    AssertTexture(reflection, "type_texture2D", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_2D, false);
    AssertTexture(reflection, "type_texture3D", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_3D, false);
    AssertTexture(reflection, "type_textureCube", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_CUBE, false);
    AssertTexture(reflection, "type_texture2DArray", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_2D, true);
    AssertTexture(reflection, "type_utexture2D", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_2D, false);
    AssertTexture(reflection, "type_uimage2D", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_2D, false);
    AssertTexture(reflection, "type_image2D", dmShaderc::BASE_TYPE_IMAGE, dmShaderc::DIMENSION_TYPE_2D, false);
    AssertTexture(reflection, "type_sampler", dmShaderc::BASE_TYPE_SAMPLER, (dmShaderc::DimensionType) 0, false);

    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, SSBO)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/ssbo.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

#if 0
    dmShaderc::DebugPrintReflection(reflection);
#endif

    ASSERT_EQ(1, reflection->m_StorageBuffers.Size());

    const dmShaderc::ResourceTypeInfo* type_ssbo = GetType(reflection, dmHashString64("Test"));
    ASSERT_NE((void*) 0, type_ssbo);
    ASSERT_EQ(2, type_ssbo->m_Members.Size());

    const dmShaderc::ResourceMember* member = &type_ssbo->m_Members[0];
    ASSERT_EQ(dmHashString64("my_data_one"), member->m_NameHash);
    ASSERT_EQ(1, member->m_Type.m_TypeIndex);
    ASSERT_EQ(1, member->m_Type.m_UseTypeIndex);

    member = &type_ssbo->m_Members[1];
    ASSERT_EQ(dmHashString64("my_data_two"), member->m_NameHash);
    ASSERT_EQ(1, member->m_Type.m_TypeIndex);
    ASSERT_EQ(1, member->m_Type.m_UseTypeIndex);

    const dmShaderc::ResourceTypeInfo* type_data = GetType(reflection, dmHashString64("Data"));
    ASSERT_NE((void*) 0, type_data);

    member = &type_data->m_Members[0];
    ASSERT_EQ(dmHashString64("member1"), member->m_NameHash);
    ASSERT_EQ(0, member->m_Type.m_TypeIndex);
    ASSERT_EQ(0, member->m_Type.m_UseTypeIndex);
    ASSERT_EQ(4, member->m_Type.m_VectorSize);
    ASSERT_EQ(dmShaderc::BASE_TYPE_FP32, member->m_Type.m_BaseType);

    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, LegacyPipeline)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/legacy_pipeline.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

#if 0
    dmShaderc::DebugPrintReflection(reflection);
#endif

    ASSERT_EQ(1, reflection->m_UniformBuffers.Size());
    ASSERT_EQ(2, reflection->m_Types.Size());

    const dmShaderc::ResourceTypeInfo* type_uniforms = GetType(reflection, dmHashString64("uniforms"));
    ASSERT_NE((void*) 0, type_uniforms);
    ASSERT_EQ(3, type_uniforms->m_Members.Size());
    ASSERT_EQ(dmHashString64("u_test"), type_uniforms->m_Members[0].m_NameHash);
    ASSERT_EQ(1, type_uniforms->m_Members[0].m_Type.m_UseTypeIndex);
    ASSERT_EQ(1, type_uniforms->m_Members[0].m_Type.m_TypeIndex);

    ASSERT_EQ(dmHashString64("u_test2"), type_uniforms->m_Members[1].m_NameHash);
    ASSERT_EQ(1, type_uniforms->m_Members[1].m_Type.m_UseTypeIndex);
    ASSERT_EQ(1, type_uniforms->m_Members[1].m_Type.m_TypeIndex);

    ASSERT_EQ(dmHashString64("u_lights"), type_uniforms->m_Members[2].m_NameHash);
    ASSERT_EQ(1, type_uniforms->m_Members[2].m_Type.m_UseTypeIndex);
    ASSERT_EQ(1, type_uniforms->m_Members[2].m_Type.m_TypeIndex);

    const dmShaderc::ResourceTypeInfo* type_light = GetType(reflection, dmHashString64("Light"));
    ASSERT_NE((void*) 0, type_light);
    ASSERT_EQ(3, type_light->m_Members.Size());
    ASSERT_EQ(dmHashString64("type"), type_light->m_Members[0].m_NameHash);
    ASSERT_EQ(dmShaderc::BASE_TYPE_INT32, type_light->m_Members[0].m_Type.m_BaseType);
    ASSERT_EQ(0, type_light->m_Members[0].m_Type.m_UseTypeIndex);
    ASSERT_EQ(1, type_light->m_Members[0].m_Type.m_VectorSize);

    ASSERT_EQ(dmHashString64("position"), type_light->m_Members[1].m_NameHash);
    ASSERT_EQ(dmShaderc::BASE_TYPE_FP32, type_light->m_Members[1].m_Type.m_BaseType);
    ASSERT_EQ(0, type_light->m_Members[1].m_Type.m_UseTypeIndex);
    ASSERT_EQ(3, type_light->m_Members[1].m_Type.m_VectorSize);

    ASSERT_EQ(dmHashString64("color"), type_light->m_Members[2].m_NameHash);
    ASSERT_EQ(dmShaderc::BASE_TYPE_FP32, type_light->m_Members[2].m_Type.m_BaseType);
    ASSERT_EQ(0, type_light->m_Members[2].m_Type.m_UseTypeIndex);
    ASSERT_EQ(4, type_light->m_Members[2].m_Type.m_VectorSize);

    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, Structs)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/structs.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

#if 0
    dmShaderc::DebugPrintReflection(reflection);
#endif

    ASSERT_EQ(1, reflection->m_UniformBuffers.Size());
    ASSERT_EQ(3, reflection->m_Types.Size());

    const dmShaderc::ResourceTypeInfo* fs_uniforms = GetType(reflection, dmHashString64("fs_uniforms"));
    ASSERT_NE((void*) 0, fs_uniforms);
    ASSERT_EQ(7, fs_uniforms->m_Members.Size());

    ASSERT_EQ(dmHashString64("base_type_vec4"), fs_uniforms->m_Members[0].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_vec3"), fs_uniforms->m_Members[1].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_vec2"), fs_uniforms->m_Members[2].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_float"), fs_uniforms->m_Members[3].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_int"), fs_uniforms->m_Members[4].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_bool"), fs_uniforms->m_Members[5].m_NameHash);
    ASSERT_EQ(dmHashString64("base"), fs_uniforms->m_Members[6].m_NameHash);
    ASSERT_EQ(4, fs_uniforms->m_Members[0].m_Type.m_VectorSize);
    ASSERT_EQ(1, fs_uniforms->m_Members[0].m_Type.m_ColumnCount);
    ASSERT_EQ(3, fs_uniforms->m_Members[1].m_Type.m_VectorSize);
    ASSERT_EQ(1, fs_uniforms->m_Members[1].m_Type.m_ColumnCount);
    ASSERT_EQ(2, fs_uniforms->m_Members[2].m_Type.m_VectorSize);
    ASSERT_EQ(1, fs_uniforms->m_Members[2].m_Type.m_ColumnCount);
    ASSERT_TRUE(fs_uniforms->m_Members[6].m_Type.m_UseTypeIndex);

    const dmShaderc::ResourceTypeInfo* base_struct = GetType(reflection, dmHashString64("base_struct"));
    ASSERT_NE((void*) 0, base_struct);
    ASSERT_EQ(2, base_struct->m_Members.Size());
    ASSERT_EQ(dmHashString64("member"), base_struct->m_Members[0].m_NameHash);
    ASSERT_EQ(dmHashString64("nested"), base_struct->m_Members[1].m_NameHash);
    ASSERT_TRUE(base_struct->m_Members[1].m_Type.m_UseTypeIndex);

    const dmShaderc::ResourceTypeInfo* nested_struct = GetType(reflection, dmHashString64("nested_struct"));
    ASSERT_NE((void*) 0, nested_struct);
    ASSERT_EQ(1, nested_struct->m_Members.Size());
    ASSERT_EQ(dmHashString64("nested_member"), nested_struct->m_Members[0].m_NameHash);

    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, TestHLSLSimple)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/reflection.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);

    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_HLSL);

    dmShaderc::ShaderCompilerOptions options;
    options.m_Version    = 51;
    options.m_EntryPoint = "main";

    dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst->m_Data.Begin());

    dmShaderc::FreeShaderCompileResult(dst);

    dmShaderc::DeleteShaderCompiler(compiler);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, TestMetal)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/reflection.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(dmShaderc::SHADER_STAGE_FRAGMENT, data, data_size);

    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_MSL);

    dmShaderc::ShaderCompilerOptions options;
    options.m_Version    = 22;
    options.m_EntryPoint = "main";

    dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst->m_Data.Begin());

    dmShaderc::FreeShaderCompileResult(dst);

    dmShaderc::DeleteShaderCompiler(compiler);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

static int TestStandalone(const char* filename, const char* languageStr, const char* stageStr)
{
    uint32_t data_size;
    void* data = ReadFile(filename, &data_size);

    dmShaderc::ShaderStage stage = dmShaderc::SHADER_STAGE_VERTEX;

    if (strcmp(stageStr, "frag") == 0)
    {
        stage = dmShaderc::SHADER_STAGE_FRAGMENT;
    }
    else if (strcmp(stageStr, "comp") == 0)
    {
        stage = dmShaderc::SHADER_STAGE_COMPUTE;
    }

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(stage, data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

    dmShaderc::DebugPrintReflection(reflection);

    if (languageStr)
    {
        if (strcmp(languageStr, "es100") == 0)
        {
            dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_GLSL);
            dmShaderc::ShaderCompilerOptions options;
            options.m_Version                    = 100;
            options.m_No420PackExtension         = 1;
            options.m_GlslEmitUboAsPlainUniforms = 1;
            options.m_GlslEs                     = 1;
            options.m_EntryPoint                 = "main";

            dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
            dmLogInfo("%s", (const char*) dst->m_Data.Begin());

            dmShaderc::FreeShaderCompileResult(dst);
            dmShaderc::DeleteShaderCompiler(compiler);
        }
        else if (strcmp(languageStr, "hlsl") == 0)
        {
            dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_HLSL);
            dmShaderc::ShaderCompilerOptions options;
            options.m_Version                    = 51;
            options.m_No420PackExtension         = 0;
            options.m_GlslEmitUboAsPlainUniforms = 0;
            options.m_GlslEs                     = 0;
            options.m_EntryPoint                 = "main";

            dmShaderc::ShaderCompileResult* dst = dmShaderc::Compile(shader_ctx, compiler, options);
            dmLogInfo("%s", (const char*) dst->m_Data.Begin());

            dmShaderc::FreeShaderCompileResult(dst);
            dmShaderc::DeleteShaderCompiler(compiler);
        }
    }

    dmShaderc::DeleteShaderContext(shader_ctx);

    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strstr(argv[1], ".spv") != 0))
    {
        if (argc > 2)
        {
            if (argc > 3)
            {
                return TestStandalone(argv[1], argv[2], argv[3]);
            }
            return TestStandalone(argv[1], argv[2], "vert");
        }
        return TestStandalone(argv[1], NULL, "vert");
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

