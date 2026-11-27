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

// Enable DEFOLD_BUILD_VERTEX_ATTRIBUTE_BENCHMARK and build benchmark_graphics_vertices.
// An optional command-line substring selects cases; timings are emitted as CSV.

#include "graphics.h"
#include <dlib/time.h>
#if defined(DM_PLATFORM_WINDOWS)
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace dmGraphics;

static const uint32_t VERTICES = 61440; // divisible by 1, 4, 6, 16, 1024
static const uint32_t SAMPLES = 9;
static const uint32_t MODES = 2;
static const char* MODE_NAMES[] = { "generic", "prepared" };
static volatile uint64_t g_Checksum;

struct ObjectValues
{
    float m_World[16];
    float m_Normal[16];
    float m_Color[4];
    float m_Custom[4];
    float m_Page;
};

struct InputData
{
    float*       m_Positions;
    float*       m_Normals;
    float*       m_UV;
    float*       m_Colors;
    ObjectValues m_Objects[256];
};

struct Fixture
{
    VertexAttributeInfo  m_Attributes[16];
    VertexAttributeInfos m_Infos;
    WriteAttributeParams m_Params;
    const float*         m_PositionChannels[1];
    const float*         m_NormalChannels[1];
    const float*         m_UVChannels[2];
    const float*         m_ColorChannels[1];
    const float*         m_WorldChannels[1];
    const float*         m_NormalMatrixChannels[1];
    const float*         m_PageChannels[1];
    uint32_t             m_Indices[1024];
    uint32_t             m_GroupSize;
    uint32_t             m_PositionComponents;
    int32_t              m_CustomIndex;
    bool                 m_SingleCalls;
    bool                 m_GlobalColor;
    WriteAttributeState  m_State;
};

static double Now()
{
    return dmTime::GetMonotonicTime() * 1e-6;
}

static void AddAttribute(Fixture& fixture, VertexAttribute::SemanticType semantic, VertexAttribute::VectorType shape,
                         VertexAttribute::DataType type = VertexAttribute::TYPE_FLOAT, bool normalized = false)
{
    VertexAttributeInfo& info = fixture.m_Attributes[fixture.m_Infos.m_NumInfos++];
    info.m_SemanticType = semantic;
    info.m_VectorType = shape;
    info.m_ValueVectorType = shape;
    info.m_ElementCount = VectorTypeToElementCount(shape);
    info.m_DataType = type;
    info.m_Normalize = normalized;
    if (semantic == VertexAttribute::SEMANTIC_TYPE_NONE)
    {
        fixture.m_CustomIndex = fixture.m_Infos.m_NumInfos - 1;
    }
    fixture.m_Infos.m_VertexStride += info.m_ElementCount * DataTypeToByteWidth(type);
}

// The same source-pointer rebinding is included in every timed variant. Values
// and pointers change between objects, and shared values also change each frame.
static void BindSources(Fixture& fixture, const InputData& inputs, uint32_t object, uint32_t frame)
{
    uint32_t first = object * fixture.m_GroupSize;
    const ObjectValues& values = inputs.m_Objects[(object + frame) & 255];
    fixture.m_PositionChannels[0] = inputs.m_Positions + first * fixture.m_PositionComponents;
    fixture.m_NormalChannels[0] = inputs.m_Normals + first * 3;
    fixture.m_UVChannels[0] = inputs.m_UV + first * 2;
    fixture.m_UVChannels[1] = inputs.m_UV + first * 2 + 2;
    fixture.m_ColorChannels[0] = fixture.m_GlobalColor ? values.m_Color : inputs.m_Colors + first * 4;
    fixture.m_WorldChannels[0] = values.m_World;
    fixture.m_NormalMatrixChannels[0] = values.m_Normal;
    fixture.m_PageChannels[0] = &values.m_Page;
    if (fixture.m_CustomIndex >= 0)
    {
        fixture.m_Attributes[fixture.m_CustomIndex].m_ValuePtr = (const uint8_t*)values.m_Custom;
    }
}

enum BenchmarkCase
{
    CASE_SPRITE,
    CASE_SPRITE_PAGED,
    CASE_SLICE9,
    CASE_PARTICLE,
    CASE_PARTICLE_TEXTURE,
    CASE_MESH_FLOAT,
    CASE_MESH_PACKED,
    CASE_MESH_INDEXED,
    CASE_INSTANCE_NORMAL,
    CASE_INSTANCE_WORLD,
    CASE_COLOR_UNORM8,
    CASE_COLOR_SNORM16,
    CASE_COLOR_SHARED,
    CASE_MATRIX_SHARED,
    CASE_FALLBACKS,
    CASE_MIXED_CHANNELS,
    CASE_COUNT
};

static const char* CASE_NAMES[CASE_COUNT] = {
    "sprite_custom_4", "sprite_paged_custom_4", "slice9_custom_16_single_calls",
    "particle_custom_6", "particle_texture_custom_6",
    "mesh_float_1024", "mesh_packed_1024", "mesh_packed_1024_indexed_calls",
    "instance_world_normal3_custom_1", "instance_world_custom_1",
    "color_unorm8_stream_1024", "color_snorm16_stream_1024", "color_unorm8_shared_6",
    "matrix4_to_matrix3_shared_16", "fallbacks_and_vector_expansion_4", "mixed_channels_steps_4"
};

static void SetupFixture(Fixture& fixture, const InputData& inputs, BenchmarkCase kind)
{
    memset(fixture.m_Attributes, 0, sizeof(fixture.m_Attributes));
    memset(&fixture.m_Params, 0, sizeof(fixture.m_Params));
    fixture.m_Infos.m_Infos = fixture.m_Attributes;
    fixture.m_Infos.m_NumInfos = 0;
    fixture.m_Infos.m_VertexStride = 0;
    fixture.m_Params.m_VertexAttributeInfos = &fixture.m_Infos;
    switch (kind)
    {
        case CASE_SPRITE:
        case CASE_SPRITE_PAGED:
        case CASE_FALLBACKS:
        case CASE_MIXED_CHANNELS:
        {
            fixture.m_GroupSize = 4;
            break;
        }
        case CASE_SLICE9:
        case CASE_MATRIX_SHARED:
        {
            fixture.m_GroupSize = 16;
            break;
        }
        case CASE_PARTICLE:
        case CASE_PARTICLE_TEXTURE:
        case CASE_COLOR_SHARED:
        {
            fixture.m_GroupSize = 6;
            break;
        }
        case CASE_INSTANCE_NORMAL:
        case CASE_INSTANCE_WORLD:
        {
            fixture.m_GroupSize = 1;
            break;
        }
        default:
        {
            fixture.m_GroupSize = 1024;
            break;
        }
    }
    fixture.m_SingleCalls = kind == CASE_SLICE9 || kind == CASE_MESH_INDEXED;
    fixture.m_GlobalColor = kind == CASE_PARTICLE || kind == CASE_PARTICLE_TEXTURE || kind == CASE_COLOR_SHARED;
    fixture.m_PositionComponents = (kind >= CASE_MESH_FLOAT && kind <= CASE_MESH_INDEXED) || kind == CASE_FALLBACKS ? 3 : 4;
    fixture.m_CustomIndex = -1;
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_PositionsLocalSpace, fixture.m_PositionChannels, fixture.m_PositionComponents == 3 ? VertexAttribute::VECTOR_TYPE_VEC3 : VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_PositionsWorldSpace, fixture.m_PositionChannels, fixture.m_PositionComponents == 3 ? VertexAttribute::VECTOR_TYPE_VEC3 : VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_Normals, fixture.m_NormalChannels, VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_Tangents, fixture.m_ColorChannels, VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_TexCoords, fixture.m_UVChannels, VertexAttribute::VECTOR_TYPE_VEC2, 2, false);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_Colors, fixture.m_ColorChannels, VertexAttribute::VECTOR_TYPE_VEC4, 1, fixture.m_GlobalColor);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_WorldMatrix, fixture.m_WorldChannels, VertexAttribute::VECTOR_TYPE_MAT4, 1, true);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_NormalMatrix, fixture.m_NormalMatrixChannels, VertexAttribute::VECTOR_TYPE_MAT4, 1, true);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_PageIndices, fixture.m_PageChannels, VertexAttribute::VECTOR_TYPE_SCALAR, 1, true);
    SetWriteAttributeStreamDesc(&fixture.m_Params.m_TextureTransform2D, fixture.m_WorldChannels, VertexAttribute::VECTOR_TYPE_MAT3, 1, true);

    if (kind <= CASE_PARTICLE_TEXTURE)
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_POSITION, kind == CASE_SPRITE_PAGED ? VertexAttribute::VECTOR_TYPE_VEC3 : VertexAttribute::VECTOR_TYPE_VEC4);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TEXCOORD, VertexAttribute::VECTOR_TYPE_VEC2);
        if (kind == CASE_PARTICLE || kind == CASE_PARTICLE_TEXTURE)
        {
            AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_COLOR, VertexAttribute::VECTOR_TYPE_VEC4, VertexAttribute::TYPE_UNSIGNED_BYTE, true);
        }
        if (kind == CASE_SPRITE_PAGED || kind == CASE_PARTICLE || kind == CASE_PARTICLE_TEXTURE)
        {
            AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX, VertexAttribute::VECTOR_TYPE_SCALAR);
        }
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NONE, VertexAttribute::VECTOR_TYPE_VEC4);
        if (kind == CASE_PARTICLE_TEXTURE)
        {
            AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D, VertexAttribute::VECTOR_TYPE_MAT3);
        }
    }
    else if (kind <= CASE_MESH_INDEXED)
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_POSITION, VertexAttribute::VECTOR_TYPE_VEC3);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NORMAL, VertexAttribute::VECTOR_TYPE_VEC3);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TANGENT, VertexAttribute::VECTOR_TYPE_VEC4);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TEXCOORD, VertexAttribute::VECTOR_TYPE_VEC2);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_COLOR, VertexAttribute::VECTOR_TYPE_VEC4, kind == CASE_MESH_FLOAT ? VertexAttribute::TYPE_FLOAT : VertexAttribute::TYPE_UNSIGNED_BYTE, kind != CASE_MESH_FLOAT);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NONE, VertexAttribute::VECTOR_TYPE_VEC4);
    }
    else if (kind <= CASE_INSTANCE_WORLD)
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX, VertexAttribute::VECTOR_TYPE_MAT4);
        if (kind == CASE_INSTANCE_NORMAL)
        {
            AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX, VertexAttribute::VECTOR_TYPE_MAT3);
        }
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NONE, VertexAttribute::VECTOR_TYPE_VEC4);
        fixture.m_Params.m_StepFunction = VERTEX_STEP_FUNCTION_INSTANCE;
        for (uint32_t i = 0; i < fixture.m_Infos.m_NumInfos; ++i)
        {
            fixture.m_Attributes[i].m_StepFunction = VERTEX_STEP_FUNCTION_INSTANCE;
        }
    }
    else if (kind <= CASE_COLOR_SHARED)
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_COLOR, VertexAttribute::VECTOR_TYPE_VEC4, kind == CASE_COLOR_SNORM16 ? VertexAttribute::TYPE_SHORT : VertexAttribute::TYPE_UNSIGNED_BYTE, true);
    }
    else if (kind == CASE_MATRIX_SHARED)
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX, VertexAttribute::VECTOR_TYPE_MAT3);
    }
    else if (kind == CASE_FALLBACKS)
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_POSITION, VertexAttribute::VECTOR_TYPE_VEC4);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_COLOR, VertexAttribute::VECTOR_TYPE_VEC4, VertexAttribute::TYPE_UNSIGNED_BYTE, true);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX, VertexAttribute::VECTOR_TYPE_MAT3);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NONE, VertexAttribute::VECTOR_TYPE_VEC4);
        fixture.m_Attributes[fixture.m_CustomIndex].m_ValueVectorType = VertexAttribute::VECTOR_TYPE_SCALAR;
        fixture.m_Params.m_Colors.m_Data = 0;
        fixture.m_Params.m_NormalMatrix.m_Data = 0;
    }
    else
    {
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TEXCOORD, VertexAttribute::VECTOR_TYPE_VEC2);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TEXCOORD, VertexAttribute::VECTOR_TYPE_VEC2);
        fixture.m_Attributes[1].m_StepFunction = VERTEX_STEP_FUNCTION_INSTANCE;
        fixture.m_Infos.m_VertexStride -= 8;
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_TEXCOORD, VertexAttribute::VECTOR_TYPE_VEC2);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_POSITION, VertexAttribute::VECTOR_TYPE_VEC4);
        fixture.m_Attributes[3].m_CoordinateSpace = COORDINATE_SPACE_WORLD;
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_POSITION, VertexAttribute::VECTOR_TYPE_VEC3);
        AddAttribute(fixture, VertexAttribute::SEMANTIC_TYPE_NONE, VertexAttribute::VECTOR_TYPE_VEC4);
    }
    for (uint32_t i = 0; i < fixture.m_GroupSize; ++i)
    {
        fixture.m_Indices[i] = kind == CASE_MESH_INDEXED ? (i * 17 + 7) % fixture.m_GroupSize : i;
    }
    BindSources(fixture, inputs, 0, 0);
}

template <int Mode>
static uint8_t* WriteFrames(uint8_t* output, Fixture& fixture, const InputData& inputs, uint32_t frames)
{
    uint8_t* out = output;
    for (uint32_t frame = 0; frame < frames; ++frame)
    {
        out = output;
        for (uint32_t object = 0; object < VERTICES / fixture.m_GroupSize; ++object)
        {
            BindSources(fixture, inputs, object, frame);
            if (Mode != 0)
            {
                PrepareWriteAttributes(&fixture.m_State, fixture.m_Params, fixture.m_GroupSize);
            }
            if (fixture.m_SingleCalls)
            {
                for (uint32_t i = 0; i < fixture.m_GroupSize; ++i)
                {
                    if (Mode == 0)
                    {
                        out = WriteAttributes(out, fixture.m_Indices[i], 1, fixture.m_Params);
                    }
                    else
                    {
                        out = WriteAttributes(out, fixture.m_Indices[i], 1, fixture.m_State);
                    }
                }
            }
            else
            {
                if (Mode == 0)
                {
                    out = WriteAttributes(out, 0, fixture.m_GroupSize, fixture.m_Params);
                }
                else
                {
                    out = WriteAttributes(out, 0, fixture.m_GroupSize, fixture.m_State);
                }
            }
        }
    }
    return out;
}

typedef uint8_t* (*RunFunction)(uint8_t*, Fixture&, const InputData&, uint32_t);
static RunFunction RUNNERS[] = { WriteFrames<0>, WriteFrames<1> };

static double Measure(uint32_t mode, uint8_t* output, Fixture& fixture, const InputData& inputs, uint32_t frames)
{
    double start = Now();
    uint8_t* end = RUNNERS[mode](output, fixture, inputs, frames);
    double elapsed = Now() - start;
    if (end != output + VERTICES * fixture.m_Infos.m_VertexStride)
    {
        fprintf(stderr, "Wrong end pointer in mode %u\n", mode);
        exit(1);
    }
    uint64_t checksum = 0;
    for (uint32_t i = 0; i < VERTICES * fixture.m_Infos.m_VertexStride; i += 64)
    {
        checksum += output[i];
    }
    g_Checksum += checksum;
    return elapsed;
}

static int CompareDouble(const void* a, const void* b)
{
    double aa = *(const double*)a;
    double bb = *(const double*)b;
    return (aa > bb) - (aa < bb);
}

int main(int argc, char** argv)
{
#if defined(DM_PLATFORM_WINDOWS)
    if (!SetThreadAffinityMask(GetCurrentThread(), 1))
    {
        fprintf(stderr, "Cannot set CPU affinity: %lu\n", GetLastError());
        return 1;
    }
#endif
    InputData inputs;
    inputs.m_Positions = (float*)malloc((VERTICES * 4 + 16) * sizeof(float));
    inputs.m_Normals = (float*)malloc((VERTICES * 3 + 16) * sizeof(float));
    inputs.m_UV = (float*)malloc((VERTICES * 2 + 16) * sizeof(float));
    inputs.m_Colors = (float*)malloc((VERTICES * 4 + 16) * sizeof(float));
    for (uint32_t i = 0; i < VERTICES * 4 + 16; ++i)
    {
        inputs.m_Positions[i] = ((int)(i % 251) - 125) * 0.01f;
        inputs.m_Colors[i] = ((int)(i % 257) - 128) / 100.0f;
        if (i < VERTICES * 3 + 16)
        {
            inputs.m_Normals[i] = ((int)(i % 127) - 63) * 0.01f;
        }
        if (i < VERTICES * 2 + 16)
        {
            inputs.m_UV[i] = (i % 239) / 239.0f;
        }
    }
    for (uint32_t i = 0; i < 256; ++i)
    {
        ObjectValues& o = inputs.m_Objects[i];
        for (uint32_t j = 0; j < 16; ++j)
        {
            o.m_World[j] = (float)(i + j) / 16.0f;
            o.m_Normal[j] = (float)((int)i - (int)j) / 32.0f;
        }
        for (uint32_t j = 0; j < 4; ++j)
        {
            o.m_Color[j] = ((int)((i * 7 + j * 13) % 257) - 128) / 100.0f;
            o.m_Custom[j] = ((int)i - (int)j) * 0.1f;
        }
        o.m_Page = (float)(i % 8);
    }
    Fixture fixture;
    // Deliberately unaligned destination, with leading and trailing guards.
    uint8_t* allocation = (uint8_t*)malloc(VERTICES * 256 + 128);
    uint8_t* expected = (uint8_t*)malloc(VERTICES * 256 + 128);
    uint8_t* output = allocation + 1;
    fprintf(stderr, "vertices=%u; samples=%u; per-object layout validation, source binding and shared packing included\n", VERTICES, SAMPLES);
    printf("case,vertices_per_object,calls_per_object,stride,mode,median_ns_per_vertex,min_ns,max_ns,median_ns_per_object\n");
    for (uint32_t kind = 0; kind < CASE_COUNT; ++kind)
    {
        if (argc > 1 && !strstr(CASE_NAMES[kind], argv[1]))
        {
            continue;
        }
        SetupFixture(fixture, inputs, (BenchmarkCase)kind);
        uint32_t bytes = VERTICES * fixture.m_Infos.m_VertexStride;
        memset(expected, 0xcd, bytes + 128);
        RUNNERS[0](expected + 1, fixture, inputs, 3);
        for (uint32_t mode = 1; mode < MODES; ++mode)
        {
            memset(allocation, 0xcd, bytes + 128);
            uint8_t* end = RUNNERS[mode](output, fixture, inputs, 3);
            if (memcmp(allocation, expected, bytes + 128) || end != output + bytes)
            {
                uint32_t first = 0;
                while (first < bytes + 128 && allocation[first] == expected[first])
                {
                    ++first;
                }
                fprintf(stderr, "Byte comparison failed: %s mode=%s byte=%u\n", CASE_NAMES[kind], MODE_NAMES[mode], first);
                return 1;
            }
        }
        uint32_t frames[MODES] = { 1, 1 };
        double samples[MODES][SAMPLES];
        for (uint32_t mode = 0; mode < MODES; ++mode)
        {
            while (Measure(mode, output, fixture, inputs, frames[mode]) < 0.012)
            {
                frames[mode] *= 2;
            }
        }
        for (uint32_t sample = 0; sample < SAMPLES; ++sample)
        {
            for (uint32_t order = 0; order < MODES; ++order)
            {
                uint32_t mode = (sample + order) % MODES;
                samples[mode][sample] = Measure(mode, output, fixture, inputs, frames[mode]) * 1e9 / (VERTICES * (double)frames[mode]);
            }
        }
        for (uint32_t mode = 0; mode < MODES; ++mode)
        {
            qsort(samples[mode], SAMPLES, sizeof(double), CompareDouble);
            double median = samples[mode][SAMPLES / 2];
            printf("%s,%u,%u,%u,%s,%.3f,%.3f,%.3f,%.3f\n", CASE_NAMES[kind], fixture.m_GroupSize,
                   fixture.m_SingleCalls ? fixture.m_GroupSize : 1, fixture.m_Infos.m_VertexStride, MODE_NAMES[mode],
                   median, samples[mode][0], samples[mode][SAMPLES - 1], median * fixture.m_GroupSize);
        }
        fflush(stdout);
    }
    fprintf(stderr, "Output comparisons, end pointers and guards passed. checksum=%llu\n", (unsigned long long)g_Checksum);
    free(expected);
    free(allocation);
    free(inputs.m_Positions);
    free(inputs.m_Normals);
    free(inputs.m_UV);
    free(inputs.m_Colors);
    return 0;
}
