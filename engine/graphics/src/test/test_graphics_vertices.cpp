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

#include <jc_test/jc_test.h>
#include "graphics.h"
#include <string.h>

using namespace dmGraphics;

static const VertexAttribute::VectorType SHAPES[] = {
    VertexAttribute::VECTOR_TYPE_SCALAR, VertexAttribute::VECTOR_TYPE_VEC2, VertexAttribute::VECTOR_TYPE_VEC3, VertexAttribute::VECTOR_TYPE_VEC4,
    VertexAttribute::VECTOR_TYPE_MAT2, VertexAttribute::VECTOR_TYPE_MAT3, VertexAttribute::VECTOR_TYPE_MAT4
};
static const VertexAttribute::DataType TYPES[] = {
    VertexAttribute::TYPE_BYTE, VertexAttribute::TYPE_UNSIGNED_BYTE, VertexAttribute::TYPE_SHORT, VertexAttribute::TYPE_UNSIGNED_SHORT,
    VertexAttribute::TYPE_INT, VertexAttribute::TYPE_UNSIGNED_INT, VertexAttribute::TYPE_FLOAT
};

static VertexAttributeInfo MakeAttribute(VertexAttribute::SemanticType semantic, VertexAttribute::VectorType shape, VertexAttribute::DataType type)
{
    VertexAttributeInfo info = {};
    info.m_SemanticType = semantic;
    info.m_VectorType = shape;
    info.m_ValueVectorType = shape;
    info.m_DataType = type;
    info.m_ElementCount = VectorTypeToElementCount(shape);
    return info;
}

static void CompareWriters(WriteAttributeState& state, const WriteAttributeParams& params, uint32_t index, uint32_t count)
{
    uint8_t expected[8192];
    uint8_t actual[8192];
    memset(expected, 0xcd, sizeof(expected));
    memset(actual, 0xcd, sizeof(actual));
    uint8_t* end = WriteAttributes(expected + 1, index, count, params);
    PrepareWriteAttributes(&state, params, count);
    ASSERT_EQ(end - expected, WriteAttributes(actual + 1, index, count, state) - actual);
    ASSERT_EQ(0, memcmp(expected, actual, sizeof(actual)));
    // The same preparation must support an indexed caller issuing single writes.
    memset(actual, 0xcd, sizeof(actual));
    uint8_t* out = actual + 1;
    for (uint32_t v = 0; v < count; ++v)
    {
        out = WriteAttributes(out, index + v, 1, state);
    }
    ASSERT_EQ(end - expected, out - actual);
    ASSERT_EQ(0, memcmp(expected, actual, sizeof(actual)));
}

// Verifies top-left matrix resizing in every storage type, guarding against
// double destination advances, incorrect source offsets and writes past stride.
TEST(VertexAttributeWriter, MatrixResizeStorage)
{
    float source[16];
    for (uint32_t i = 0; i < 16; ++i)
    {
        source[i] = (float)i + 1;
    }
    const float* channel[] = { source };
    VertexAttributeInfo info = MakeAttribute(VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX, VertexAttribute::VECTOR_TYPE_MAT3, VertexAttribute::TYPE_FLOAT);
    VertexAttributeInfos infos;
    infos.m_Infos = &info;
    infos.m_NumInfos = 1;
    WriteAttributeParams params = {};
    params.m_VertexAttributeInfos = &infos;
    WriteAttributeState state;
    for (uint32_t src_dim = 2; src_dim <= 4; ++src_dim)
    {
        SetWriteAttributeStreamDesc(&params.m_WorldMatrix, channel, SHAPES[src_dim + 2], 1, true);
        for (uint32_t dst_dim = 2; dst_dim <= 4; ++dst_dim)
        {
            info.m_VectorType = SHAPES[dst_dim + 2];
            for (uint32_t t = 0; t < DM_ARRAY_SIZE(TYPES); ++t)
            {
                info.m_DataType = TYPES[t];
                uint32_t bytes = DataTypeToByteWidth(TYPES[t]);
                infos.m_VertexStride = dst_dim * dst_dim * bytes;
                uint8_t expected[256];
                uint8_t actual[256];
                memset(expected, 0xcd, sizeof(expected));
                uint8_t* out = expected + 1;
                for (uint32_t row = 0; row < dst_dim; ++row)
                {
                    for (uint32_t col = 0; col < dst_dim; ++col)
                    {
                        float value = row < src_dim && col < src_dim ? source[row * src_dim + col] : row == col ? 1.0f : 0.0f;
                        out = WriteVertexAttributeFromFloat(out, value, TYPES[t]);
                    }
                }
                memset(actual, 0xcd, sizeof(actual));
                ASSERT_EQ(actual + 1 + infos.m_VertexStride, WriteAttributes(actual + 1, 0, 1, params));
                ASSERT_EQ(0, memcmp(expected, actual, sizeof(actual)));
                PrepareWriteAttributes(&state, params, 2);
                memset(actual, 0xcd, sizeof(actual));
                ASSERT_EQ(actual + 1 + infos.m_VertexStride, WriteAttributes(actual + 1, 0, 1, state));
                ASSERT_EQ(0, memcmp(expected, actual, sizeof(actual)));
            }
        }
    }
}

// Verifies all scalar/vector/matrix shapes and storage types against the generic
// writer, including normalized packing, W padding, global data and nonzero indices.
TEST(VertexAttributeWriter, FormatConversions)
{
    float source[80];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(source); ++i)
    {
        source[i] = ((int)(i % 23) - 11) * 0.125f;
    }
    const float* channel[] = { source };
    VertexAttributeInfo info = MakeAttribute(VertexAttribute::SEMANTIC_TYPE_COLOR, VertexAttribute::VECTOR_TYPE_VEC4, VertexAttribute::TYPE_FLOAT);
    VertexAttributeInfos infos;
    infos.m_Infos = &info;
    infos.m_NumInfos = 1;
    WriteAttributeParams params = {};
    params.m_VertexAttributeInfos = &infos;
    WriteAttributeState state;
    for (uint32_t src = 0; src < DM_ARRAY_SIZE(SHAPES); ++src)
    {
        for (uint32_t dst = 0; dst < DM_ARRAY_SIZE(SHAPES); ++dst)
        {
            info.m_VectorType = SHAPES[dst];
            for (uint32_t type = 0; type < DM_ARRAY_SIZE(TYPES); ++type)
            {
                info.m_DataType = TYPES[type];
                infos.m_VertexStride = VectorTypeToElementCount(info.m_VectorType) * DataTypeToByteWidth(info.m_DataType);
                for (uint32_t normalized = 0; normalized < 2; ++normalized)
                {
                    info.m_Normalize = normalized != 0;
                    SetWriteAttributeStreamDesc(&params.m_Colors, channel, SHAPES[src], 1, false);
                    CompareWriters(state, params, 2, 3);
                    params.m_Colors.m_IsGlobalData = true;
                    CompareWriters(state, params, 0, 3);
                    // Material overrides already use destination storage. Changing
                    // the source kind must not pack normalized values a second time.
                    uint8_t packed[64];
                    uint8_t* out = packed;
                    for (uint32_t i = 0; i < VectorTypeToElementCount(SHAPES[src]); ++i)
                    {
                        out = WriteVertexAttributeFromFloat(out, source[i], TYPES[type]);
                    }
                    params.m_Colors.m_Data = 0;
                    info.m_ValueVectorType = SHAPES[src];
                    info.m_ValuePtr = packed;
                    CompareWriters(state, params, 0, 3);
                    info.m_ValuePtr = 0;
                }
            }
        }
    }
}

// Verifies that preparation refreshes pointers, shared values, source shapes,
// availability and defaults without requiring the caller to invalidate a cache.
TEST(VertexAttributeWriter, RebindSourcesAndOverrides)
{
    float values[] = { 0.0f, 0.5f, 1.0f, -1.0f, 0.2f, 0.4f, 0.6f, 0.8f };
    const float* channel[] = { values };
    VertexAttributeInfo info = MakeAttribute(VertexAttribute::SEMANTIC_TYPE_COLOR, VertexAttribute::VECTOR_TYPE_VEC4, VertexAttribute::TYPE_UNSIGNED_BYTE);
    info.m_Normalize = true;
    VertexAttributeInfos infos;
    infos.m_Infos = &info;
    infos.m_NumInfos = 1;
    infos.m_VertexStride = 4;
    WriteAttributeParams params = {};
    params.m_VertexAttributeInfos = &infos;
    WriteAttributeState state;
    SetWriteAttributeStreamDesc(&params.m_Colors, channel, VertexAttribute::VECTOR_TYPE_VEC4, 1, true);
    CompareWriters(state, params, 0, 2);
    values[0] = 0.8f;
    CompareWriters(state, params, 0, 2);
    channel[0] = values + 4;
    CompareWriters(state, params, 0, 2);
    params.m_Colors.m_VectorType = VertexAttribute::VECTOR_TYPE_VEC2;
    CompareWriters(state, params, 0, 2);
    params.m_Colors.m_IsGlobalData = false;
    CompareWriters(state, params, 0, 2);
    params.m_Colors.m_StreamCount = 0;
    CompareWriters(state, params, 0, 2);
    const uint8_t packed[] = { 10, 20, 30, 40 };
    info.m_ValuePtr = packed;
    CompareWriters(state, params, 0, 2);
    info.m_ValueVectorType = VertexAttribute::VECTOR_TYPE_SCALAR;
    CompareWriters(state, params, 0, 2);
    info.m_ValueVectorType = VertexAttribute::VECTOR_TYPE_VEC2;
    CompareWriters(state, params, 0, 2);
    params.m_Colors.m_StreamCount = 1;
    CompareWriters(state, params, 0, 2);
    channel[0] = 0;
    CompareWriters(state, params, 0, 2);
    info.m_ValuePtr = 0;
    CompareWriters(state, params, 0, 2);
    info.m_DataType = VertexAttribute::TYPE_FLOAT;
    infos.m_VertexStride = 16;
    CompareWriters(state, params, 0, 2);
}

// Verifies layout growth and reuse, shrinking to empty,
// semantic/coordinate changes and channel numbering across skipped step functions.
TEST(VertexAttributeWriter, LayoutChangesAndChannels)
{
    float values[32];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(values); ++i)
    {
        values[i] = (float)i;
    }
    const float* channels[] = { values, values + 8 };
    VertexAttributeInfo attributes[40];
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(attributes); ++i)
    {
        attributes[i] = MakeAttribute(VertexAttribute::SEMANTIC_TYPE_NONE, VertexAttribute::VECTOR_TYPE_VEC4, VertexAttribute::TYPE_FLOAT);
        attributes[i].m_ValuePtr = (const uint8_t*)values;
    }
    VertexAttributeInfos infos;
    infos.m_Infos = attributes;
    WriteAttributeParams params = {};
    params.m_VertexAttributeInfos = &infos;
    SetWriteAttributeStreamDesc(&params.m_TexCoords, channels, VertexAttribute::VECTOR_TYPE_VEC2, 2, false);
    SetWriteAttributeStreamDesc(&params.m_PositionsWorldSpace, channels + 1, VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
    SetWriteAttributeStreamDesc(&params.m_PositionsLocalSpace, channels, VertexAttribute::VECTOR_TYPE_VEC3, 1, false);
    WriteAttributeState state;
    const uint32_t counts[] = { 0, 1, 40, 2, 0, 4 };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(counts); ++i)
    {
        infos.m_NumInfos = counts[i];
        infos.m_VertexStride = counts[i] * 16;
        CompareWriters(state, params, 0, 3);
    }
    for (uint32_t i = 0; i < 4; ++i)
    {
        attributes[i].m_SemanticType = VertexAttribute::SEMANTIC_TYPE_TEXCOORD;
    }
    attributes[0].m_StepFunction = VERTEX_STEP_FUNCTION_INSTANCE;
    infos.m_VertexStride = 48;
    CompareWriters(state, params, 0, 2);
    params.m_StepFunction = VERTEX_STEP_FUNCTION_INSTANCE;
    infos.m_VertexStride = 16;
    CompareWriters(state, params, 0, 2);
    params.m_StepFunction = VERTEX_STEP_FUNCTION_VERTEX;
    infos.m_VertexStride = 48;
    attributes[1].m_SemanticType = VertexAttribute::SEMANTIC_TYPE_POSITION;
    CompareWriters(state, params, 0, 2);
    attributes[1].m_CoordinateSpace = COORDINATE_SPACE_WORLD;
    CompareWriters(state, params, 0, 2);
    attributes[2].m_VectorType = VertexAttribute::VECTOR_TYPE_SCALAR;
    infos.m_VertexStride -= 12;
    CompareWriters(state, params, 0, 2);
}
