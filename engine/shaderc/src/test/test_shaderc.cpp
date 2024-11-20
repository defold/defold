// Copyright 2020-2024 The Defold Foundation
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

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(data, data_size);
    dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, TestShaderReflection)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/reflection.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(data, data_size);
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

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(data, data_size);
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
    options.m_Stage   = dmShaderc::SHADER_STAGE_VERTEX;
    options.m_Version = 460;

    const char* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst);

    ASSERT_NE((const char*) 0, FindFirstOccurance(dst, "layout(location = 3) in vec4 position;"));
    ASSERT_NE((const char*) 0, FindFirstOccurance(dst, "layout(location = 4) in vec3 normal;"));
    ASSERT_NE((const char*) 0, FindFirstOccurance(dst, "layout(location = 5) in vec2 tex_coord;"));

    ASSERT_NE((const char*) 0, FindFirstOccurance(dst, "layout(binding = 3, std140) uniform matrices"));
    ASSERT_NE((const char*) 0, FindFirstOccurance(dst, "layout(binding = 4, std140) uniform extra"));

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

TEST(Shaderc, Types)
{
    uint32_t data_size;
    void* data = ReadFile("./build/src/test/data/types.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

#if 0
    dmShaderc::DebugPrintReflection(reflection);
#endif

    ASSERT_EQ(1, reflection->m_UniformBuffers.Size());
    ASSERT_EQ(3, reflection->m_Types.Size());

    const dmShaderc::ResourceTypeInfo* fs_uniforms = GetType(reflection, dmHashString64("fs_uniforms"));
    ASSERT_NE((void*) 0, fs_uniforms);
    ASSERT_EQ(7, fs_uniforms->m_MemberCount);

    ASSERT_EQ(dmHashString64("base_type_vec4"), fs_uniforms->m_Members[0].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_vec3"), fs_uniforms->m_Members[1].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_vec2"), fs_uniforms->m_Members[2].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_float"), fs_uniforms->m_Members[3].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_int"), fs_uniforms->m_Members[4].m_NameHash);
    ASSERT_EQ(dmHashString64("base_type_bool"), fs_uniforms->m_Members[5].m_NameHash);
    ASSERT_EQ(dmHashString64("base"), fs_uniforms->m_Members[6].m_NameHash);
    ASSERT_EQ(4, fs_uniforms->m_Members[0].m_VectorSize);
    ASSERT_EQ(1, fs_uniforms->m_Members[0].m_ColumnCount);
    ASSERT_EQ(3, fs_uniforms->m_Members[1].m_VectorSize);
    ASSERT_EQ(1, fs_uniforms->m_Members[1].m_ColumnCount);
    ASSERT_EQ(2, fs_uniforms->m_Members[2].m_VectorSize);
    ASSERT_EQ(1, fs_uniforms->m_Members[2].m_ColumnCount);
    ASSERT_TRUE(fs_uniforms->m_Members[6].m_Type.m_UseTypeIndex);

    const dmShaderc::ResourceTypeInfo* base_struct = GetType(reflection, dmHashString64("base_struct"));
    ASSERT_NE((void*) 0, base_struct);
    ASSERT_EQ(2, base_struct->m_MemberCount);
    ASSERT_EQ(dmHashString64("member"), base_struct->m_Members[0].m_NameHash);
    ASSERT_EQ(dmHashString64("nested"), base_struct->m_Members[1].m_NameHash);
    ASSERT_TRUE(base_struct->m_Members[1].m_Type.m_UseTypeIndex);

    const dmShaderc::ResourceTypeInfo* nested_struct = GetType(reflection, dmHashString64("nested_struct"));
    ASSERT_NE((void*) 0, nested_struct);
    ASSERT_EQ(1, nested_struct->m_MemberCount);
    ASSERT_EQ(dmHashString64("nested_member"), nested_struct->m_Members[0].m_NameHash);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

