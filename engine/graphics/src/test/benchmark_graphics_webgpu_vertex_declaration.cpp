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


// Build target: benchmark_graphics_webgpu_vertex_declaration (WITH_WEBGPU=ON, BUILD_TESTS=ON).
// Run with optional bind-count and sample-count arguments, e.g. 100000 15.
// Emits CSV after three warmups, with alternating baseline/cache order and matching checksums.
// This measures CPU layout binding only; it does not measure frame time or GPU work.
// Misses also pay for the corrected resolved-layout hash, which the old implementation omitted.
#include "../webgpu/graphics_webgpu.cpp"

#include <dlib/time.h>
#include <stdio.h>
#include <stdlib.h>

using namespace dmGraphics;

struct VertexDeclarationBenchmark
{
    WebGPUProgram                      m_Programs[2];
    VertexDeclaration::Stream          m_SourceStreams[32][8];
    VertexDeclaration                  m_Declarations[32];
    VertexDeclaration                  m_BaselineDeclaration;
    dmArray<VertexDeclaration::Stream> m_BaselineStreams;
    WebGPUContext                      m_Context;

    void Initialize(uint32_t attribute_count)
    {
        for (uint32_t p = 0; p < DM_ARRAY_SIZE(m_Programs); ++p)
        {
            m_Programs[p].m_BaseProgram.m_ShaderMeta.m_Inputs.EnsureSize(attribute_count);
            for (uint32_t i = 0; i < attribute_count; ++i)
            {
                ShaderResourceBinding& input = m_Programs[p].m_BaseProgram.m_ShaderMeta.m_Inputs[i];
                memset(&input, 0, sizeof(input));
                input.m_NameHash = i + 1;
                input.m_Binding = i + p;
                input.m_StageFlags = SHADER_STAGE_FLAG_VERTEX;
            }
        }

        for (uint32_t d = 0; d < DM_ARRAY_SIZE(m_Declarations); ++d)
        {
            VertexDeclaration& declaration = m_Declarations[d];
            memset(&declaration, 0, sizeof(declaration));
            declaration.m_Streams = m_SourceStreams[d];
            declaration.m_StreamCount = attribute_count;
            // Leave room for every offset used by the mutation and alternating-layout cases.
            declaration.m_Stride = 2048;
            declaration.m_StepFunction = VERTEX_STEP_FUNCTION_VERTEX;
            for (uint32_t i = 0; i < attribute_count; ++i)
            {
                VertexDeclaration::Stream& stream = m_SourceStreams[d][i];
                memset(&stream, 0, sizeof(stream));
                stream.m_NameHash = i + 1;
                stream.m_Location = -1;
                stream.m_Type = TYPE_FLOAT;
                stream.m_Size = 4;
                stream.m_Offset = i * 16 + d * 4;
            }
        }
    }

};

// The pre-cache EnableVertexDeclaration implementation, with only its per-binding
// scratch storage moved into the benchmark fixture. Retained for reproducible A/B runs.
static void EnableBaseline(VertexDeclarationBenchmark* benchmark, HVertexDeclaration declaration, uint32_t base_offset, WebGPUProgram* program)
{
    WebGPUContext* context = &benchmark->m_Context;
    VertexDeclaration& resolved = benchmark->m_BaselineDeclaration;
    resolved = {};
    resolved.m_Stride = declaration->m_Stride;
    resolved.m_StepFunction = declaration->m_StepFunction;
    resolved.m_PipelineHash = declaration->m_PipelineHash;
    context->m_EnabledVertexDeclarations[0] = declaration;
    context->m_CurrentVertexDeclaration[0] = &resolved;
    context->m_CurrentVertexBufferOffsets[0] = base_offset;
    dmArray<VertexDeclaration::Stream>& streams = benchmark->m_BaselineStreams;
    streams.EnsureSize(declaration->m_StreamCount);
    memset(streams.Begin(), 0, sizeof(VertexDeclaration::Stream) * declaration->m_StreamCount);
    resolved.m_Streams = streams.Begin();
    uint32_t num_inputs = program->m_BaseProgram.m_ShaderMeta.m_Inputs.Size();
    for (uint32_t i = 0; i < declaration->m_StreamCount; ++i)
    {
        for (uint32_t j = 0; j < num_inputs; ++j)
        {
            ShaderResourceBinding& input = program->m_BaseProgram.m_ShaderMeta.m_Inputs[j];
            if (input.m_StageFlags & SHADER_STAGE_FLAG_VERTEX && input.m_NameHash == declaration->m_Streams[i].m_NameHash)
            {
                VertexDeclaration::Stream& stream = streams[resolved.m_StreamCount++];
                stream.m_NameHash = input.m_NameHash;
                stream.m_Location = input.m_Binding;
                stream.m_Type = declaration->m_Streams[i].m_Type;
                stream.m_Offset = declaration->m_Streams[i].m_Offset;
                stream.m_Size = declaration->m_Streams[i].m_Size;
                stream.m_Normalize = declaration->m_Streams[i].m_Normalize;
                break;
            }
        }
    }
}

static void EnableCached(VertexDeclarationBenchmark* benchmark, HVertexDeclaration declaration, uint32_t base_offset, WebGPUProgram* program)
{
    WebGPUEnableVertexDeclaration((HContext) & benchmark->m_Context, declaration, 0, base_offset, (HProgram)program);
}

typedef void (*EnableLayoutFn)(VertexDeclarationBenchmark*, HVertexDeclaration, uint32_t, WebGPUProgram*);

struct BenchmarkCase
{
    const char* m_Name;
    uint32_t    m_Attributes;
    uint32_t    m_Declarations;
    uint32_t    m_Programs;
    bool        m_MutateOffset;
};

static uint64_t RunSample(VertexDeclarationBenchmark* benchmark, const BenchmarkCase& test, uint32_t iterations, EnableLayoutFn enable)
{
    uint64_t checksum = 0;
    for (uint32_t i = 0; i < iterations; ++i)
    {
        HVertexDeclaration declaration = &benchmark->m_Declarations[i % test.m_Declarations];
        if (test.m_MutateOffset)
        {
            SetStreamOffset(declaration, 0, (i % 256) * 4);
        }

        WebGPUProgram* program = &benchmark->m_Programs[i % test.m_Programs];
        enable(benchmark, declaration, (i % 16) * 16, program);
        const VertexDeclaration* resolved = benchmark->m_Context.m_CurrentVertexDeclaration[0];
        checksum += resolved->m_StreamCount + resolved->m_Streams[0].m_Offset + resolved->m_Streams[0].m_Location;
        WebGPUDisableVertexDeclaration((HContext) & benchmark->m_Context, declaration);
    }

    return checksum;
}

int main(int argc, char** argv)
{
    const uint32_t iterations = argc > 1 ? atoi(argv[1]) : 100000;
    const uint32_t samples = argc > 2 ? atoi(argv[2]) : 15;
    const BenchmarkCase cases[] = {
        { "repeat_1", 1, 1, 1, false },
        { "repeat_8", 8, 1, 1, false },
        { "layouts_2", 8, 2, 1, false },
        { "programs_2", 8, 1, 2, false },
        { "layouts_8", 8, 8, 1, false },
        { "layouts_32", 8, 32, 1, false },
        { "mutate_offset", 8, 1, 1, true },
    };
    printf("case,mode,sample,binds,cpu_us,checksum\n");
    for (uint32_t c = 0; c < DM_ARRAY_SIZE(cases); ++c)
    {
        VertexDeclarationBenchmark* benchmark = new VertexDeclarationBenchmark();
        benchmark->Initialize(cases[c].m_Attributes);
        // Alternate order within each sample to reduce bias from frequency/thermal changes.
        for (int32_t sample = -3; sample < (int32_t)samples; ++sample)
        {
            uint64_t checksums[2];
            for (uint32_t order = 0; order < 2; ++order)
            {
                uint32_t mode = (order + sample + 4) % 2;
                EnableLayoutFn enable = mode ? EnableCached : EnableBaseline;
                uint64_t start = dmTime::GetMonotonicTime();
                checksums[mode] = RunSample(benchmark, cases[c], iterations, enable);
                uint64_t elapsed = dmTime::GetMonotonicTime() - start;
                if (sample >= 0)
                {
                    printf("%s,%s,%d,%u,%llu,%llu\n", cases[c].m_Name, mode ? "cached" : "baseline", sample, iterations,
                           (unsigned long long)elapsed, (unsigned long long)checksums[mode]);
                }
            }

            if (checksums[0] != checksums[1])
            {
                fprintf(stderr, "Resolved layout mismatch in %s\n", cases[c].m_Name);
                return 1;
            }
        }

        delete benchmark;
    }

    return 0;
}
