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
    void* data = ReadFile("./src/test/data/simple.frag.spv", &data_size);
    ASSERT_NE((void*) 0, data);

	dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(data, data_size);
	dmShaderc::DeleteShaderContext(shader_ctx);
}

TEST(Shaderc, TestShaderReflection)
{
    uint32_t data_size;
    void* data = ReadFile("./src/test/data/reflection.frag.spv", &data_size);
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

    dmShaderc::DebugPrintReflection(reflection);

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

TEST(Shaderc, ModifyBindings)
{
    uint32_t data_size;
    void* data = ReadFile("./src/test/data/bindings.vert.spv", &data_size);
    ASSERT_NE((void*) 0, data);

    dmShaderc::HShaderContext shader_ctx = dmShaderc::NewShaderContext(data, data_size);
    const dmShaderc::ShaderReflection* reflection = dmShaderc::GetReflection(shader_ctx);

    dmShaderc::DebugPrintReflection(reflection);

    const dmShaderc::ShaderResource* position  = GetShaderResourceByNameHash(reflection->m_Inputs, dmHashString64("position"));
    const dmShaderc::ShaderResource* normal    = GetShaderResourceByNameHash(reflection->m_Inputs, dmHashString64("normal"));
    const dmShaderc::ShaderResource* tex_coord = GetShaderResourceByNameHash(reflection->m_Inputs, dmHashString64("tex_coord"));

    dmShaderc::HShaderCompiler compiler = dmShaderc::NewShaderCompiler(shader_ctx, dmShaderc::SHADER_LANGUAGE_GLSL);

    ASSERT_NE((void*) 0, position);
    ASSERT_EQ(0, position->m_Location);

    ASSERT_NE((void*) 0, normal);
    ASSERT_EQ(1, normal->m_Location);

    ASSERT_NE((void*) 0, tex_coord);
    ASSERT_EQ(2, tex_coord->m_Location);

    dmShaderc::SetLocation(shader_ctx, compiler, dmHashString64("position"), 3);
    dmShaderc::SetLocation(shader_ctx, compiler, dmHashString64("normal"), 4);
    dmShaderc::SetLocation(shader_ctx, compiler, dmHashString64("tex_coord"), 5);

    dmShaderc::ShaderCompilerOptions options;
    options.m_Stage   = dmShaderc::SHADER_STAGE_VERTEX;
    options.m_Version = 100;
    options.m_GlslEs = 1;

    const char* dst = dmShaderc::Compile(shader_ctx, compiler, options);
    ASSERT_NE((void*) 0, dst);

    dmLogInfo("%s", dst);

    dmShaderc::DeleteShaderContext(shader_ctx);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

