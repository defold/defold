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

#include "graphics.h"

#include <dlib/math.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>

namespace dmGraphics
{
	static const float g_SourceDataDefaultVectorEmpty[4]  = {};
    static const float g_SourceDataDefaultVectorOneAsW[4] = {0.0, 0.0, 0.0, 1.0};
    static const float g_SourceDataDefaultVector[4]       = {1.0, 1.0, 1.0, 1.0};
    static const float g_SourceDataDefaultMatrix[16]      = {
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0
    };

    struct UnpackAttributeDataState
    {
        uint8_t m_ChannelIndexWorldMatrix;
        uint8_t m_ChannelIndexNormalMatrix;
        uint8_t m_ChannelIndexPositionsLocalSpace;
        uint8_t m_ChannelIndexPositionsWorldSpace;
        uint8_t m_ChannelIndexNormals;
        uint8_t m_ChannelIndexTangents;
        uint8_t m_ChannelIndexColors;
        uint8_t m_ChannelIndexTexCoords;
        uint8_t m_ChannelIndexPageIndices;
        uint8_t m_ChannelIndexTextureTransform2D;
        uint8_t m_ChannelIndexMorphTargetWeights;
    };

    struct UnpackAttributeData
    {
        VertexAttribute::VectorType m_VectorType;
        VertexAttribute::DataType   m_DataType;
        const uint8_t*              m_ValuePtr;
        uint32_t                    m_ElementCount;
        // Separate bytes avoid bitfield read/modify/write dependencies in the
        // per-attribute loop, including the float-only path.
        uint8_t                     m_IsMatrix;
        uint8_t                     m_Normalize;
    };

    void GetAttributeValues(const dmGraphics::VertexAttribute& attribute, const uint8_t** data_ptr, uint32_t* data_size)
    {
        *data_ptr  = attribute.m_Values.m_BinaryValues.m_Data;
        *data_size = attribute.m_Values.m_BinaryValues.m_Count;
    }

    void GetAttributeValues(const dmGraphics::VertexAttributeInfo& info, const uint8_t** data_ptr, uint32_t* data_size)
    {
        *data_ptr  = info.m_ValuePtr;
        *data_size = info.m_ValuePtr ? (info.m_ElementCount * dmGraphics::DataTypeToByteWidth(info.m_DataType)) : 0;
    }

    uint8_t* WriteVertexAttributeFromFloat(uint8_t* value_write_ptr, float value, dmGraphics::VertexAttribute::DataType data_type)
    {
        switch (data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:
            {
                int8_t typed_value = (int8_t) dmMath::Clamp(value, -128.0f, 127.0f);
                memcpy(value_write_ptr, &typed_value, sizeof(typed_value));
                return value_write_ptr + 1;
            }
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:
            {
                uint8_t typed_value = (uint8_t) dmMath::Clamp(value, 0.0f, 255.0f);
                memcpy(value_write_ptr, &typed_value, sizeof(typed_value));
                return value_write_ptr + 1;
            }
            case dmGraphics::VertexAttribute::TYPE_SHORT:
            {
                int16_t typed_value = (int16_t) dmMath::Clamp(value, -32768.0f, 32767.0f);
                memcpy(value_write_ptr, &typed_value, sizeof(typed_value));
                return value_write_ptr + 2;
            }
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT:
            {
                uint16_t typed_value = (uint16_t) dmMath::Clamp(value, 0.0f, 65535.0f);
                memcpy(value_write_ptr, &typed_value, sizeof(typed_value));
                return value_write_ptr + 2;
            }
            case dmGraphics::VertexAttribute::TYPE_INT:
            {
                int32_t typed_value = (int32_t) dmMath::Clamp((double) value, -2147483648.0, 2147483647.0);
                memcpy(value_write_ptr, &typed_value, sizeof(typed_value));
                return value_write_ptr + 4;
            }
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:
            {
                uint32_t typed_value = (uint32_t) dmMath::Clamp((double) value, 0.0, 4294967295.0);
                memcpy(value_write_ptr, &typed_value, sizeof(typed_value));
                return value_write_ptr + 4;
            }
            case dmGraphics::VertexAttribute::TYPE_FLOAT:
                memcpy(value_write_ptr, &value, sizeof(value));
                return value_write_ptr + 4;
            default:break;
        }
        return 0;
    }

    // Engine streams and generated defaults are floats. For normalized integer
    // attributes, the CPU must pack these values into the storage range first
    // (e.g. 0.5 becomes 128 for UNORM8). The GPU normalizes the stored integers
    // when reading vertices; it does not pack our source floats during upload.
    // Material values already use the destination storage type and are copied directly.
    static uint8_t* WriteVertexAttributeFromFloatToStorage(uint8_t* write_ptr, float value, const UnpackAttributeData& dst_data)
    {
        if (!dst_data.m_Normalize || dst_data.m_DataType == VertexAttribute::TYPE_FLOAT)
            return WriteVertexAttributeFromFloat(write_ptr, value, dst_data.m_DataType);

        float scale;
        switch (dst_data.m_DataType)
        {
            case VertexAttribute::TYPE_BYTE:           scale = value > 0.0f ? 127.0f : 128.0f; break;
            case VertexAttribute::TYPE_UNSIGNED_BYTE:  scale = 255.0f; break;
            case VertexAttribute::TYPE_SHORT:          scale = value > 0.0f ? 32767.0f : 32768.0f; break;
            case VertexAttribute::TYPE_UNSIGNED_SHORT: scale = 65535.0f; break;
            // Float cannot represent the positive 32-bit integer endpoints exactly.
            // Keep scaling and rounding in double only for these storage formats.
            // Legacy wasm conversion lowering rejects the exact positive endpoint.
            // Store it directly and clamp the lower endpoint before scaling.
            case VertexAttribute::TYPE_INT:
            {
                double scale = value > 0.0f ? 2147483647.0 : 2147483648.0;
                int32_t packed_value = value >= 1.0f ? 0x7fffffff :
                    (int32_t) floor(dmMath::Max(value, -1.0f) * scale + 0.5);
                memcpy(write_ptr, &packed_value, sizeof(packed_value));
                return write_ptr + sizeof(packed_value);
            }
            case VertexAttribute::TYPE_UNSIGNED_INT:
            {
                uint32_t packed_value = value >= 1.0f ? 0xffffffffu :
                    (uint32_t) (dmMath::Max(value, 0.0f) * 4294967295.0 + 0.5);
                memcpy(write_ptr, &packed_value, sizeof(packed_value));
                return write_ptr + sizeof(packed_value);
            }
            default: return 0;
        }
        float packed_value = dmMath::Clamp(value, -1.0f, 1.0f) * scale + 0.5f;
        // Unsigned storage clamps to a nonnegative range before the cast, so
        // truncation already rounds down. Signed storage still needs floorf.
        if (dst_data.m_DataType == VertexAttribute::TYPE_BYTE || dst_data.m_DataType == VertexAttribute::TYPE_SHORT)
        {
            packed_value = floorf(packed_value);
        }
        return WriteVertexAttributeFromFloat(write_ptr, packed_value, dst_data.m_DataType);
    }

    float VertexAttributeDataTypeToFloat(const dmGraphics::VertexAttribute::DataType data_type, const uint8_t* value_ptr)
    {
        switch (data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:
            {
                int8_t typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return (float) typed_value;
            }
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:
            {
                uint8_t typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return (float) typed_value;
            }
            case dmGraphics::VertexAttribute::TYPE_SHORT:
            {
                int16_t typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return (float) typed_value;
            }
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT:
            {
                uint16_t typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return (float) typed_value;
            }
            case dmGraphics::VertexAttribute::TYPE_INT:
            {
                int32_t typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return (float) typed_value;
            }
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:
            {
                uint32_t typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return (float) typed_value;
            }
            case dmGraphics::VertexAttribute::TYPE_FLOAT:
            {
                float typed_value;
                memcpy(&typed_value, value_ptr, sizeof(typed_value));
                return typed_value;
            }
            default:break;
        }
        return 0;
    }

    static inline bool VectorTypeIsMatrix(VertexAttribute::VectorType vector_type)
    {
        return vector_type == VertexAttribute::VECTOR_TYPE_MAT2 ||
               vector_type == VertexAttribute::VECTOR_TYPE_MAT3 ||
               vector_type == VertexAttribute::VECTOR_TYPE_MAT4;
    }

    static inline bool SemanticTypeRequiresOneAsW(VertexAttribute::SemanticType semantic_type)
    {
        return semantic_type == VertexAttribute::SEMANTIC_TYPE_POSITION ||
               semantic_type == VertexAttribute::SEMANTIC_TYPE_TANGENT ||
               semantic_type == VertexAttribute::SEMANTIC_TYPE_COLOR;
    }

    static inline uint32_t VectorTypeToMatrixRowColCount(VertexAttribute::VectorType vector_type)
    {
        switch(vector_type)
        {
            case VertexAttribute::VECTOR_TYPE_MAT2: return 2;
            case VertexAttribute::VECTOR_TYPE_MAT3: return 3;
            case VertexAttribute::VECTOR_TYPE_MAT4: return 4;
            default: break;
        }
        return 0;
    }

    // Supports writing a matrix to a matrix, regardless of size of src and dst.
    static void WriteMatrixToMatrix(uint8_t* write_ptr, const UnpackAttributeData& src_data, const UnpackAttributeData& dst_data)
    {
        const uint32_t dst_element_byte_width = DataTypeToByteWidth(dst_data.m_DataType);
        const uint32_t src_element_byte_width = DataTypeToByteWidth(src_data.m_DataType);
        const uint32_t dst_row_col_count      = VectorTypeToMatrixRowColCount(dst_data.m_VectorType);
        const uint32_t src_row_col_count      = VectorTypeToMatrixRowColCount(src_data.m_VectorType);

        // Case 1: No matrix size conversion needed, source and destination element counts are the same
        if (src_data.m_ElementCount == dst_data.m_ElementCount)
        {
            if (src_data.m_DataType == dst_data.m_DataType)
            {
                memcpy(write_ptr, src_data.m_ValuePtr, dst_data.m_ElementCount * dst_element_byte_width);
            }
            else
            {
                for (uint32_t i = 0; i < dst_data.m_ElementCount; ++i)
                {
                    float float_value = VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr + i * src_element_byte_width);

                    write_ptr = WriteVertexAttributeFromFloatToStorage(write_ptr, float_value, dst_data);
                }
            }
        }
        // Case 2: Converting from larger to smaller matrix (use top-left portion)
        else if (src_data.m_ElementCount > dst_data.m_ElementCount)
        {
            for (uint32_t row = 0; row < dst_row_col_count; ++row)
            {
                for (uint32_t col = 0; col < dst_row_col_count; ++col)
                {
                    const uint32_t src_index = row * src_row_col_count + col;

                    if (src_data.m_DataType == dst_data.m_DataType)
                    {
                        memcpy(write_ptr, src_data.m_ValuePtr + src_index * src_element_byte_width, dst_element_byte_width);
                    }
                    else
                    {
                        float float_value = VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr + src_index * src_element_byte_width);

                        WriteVertexAttributeFromFloatToStorage(write_ptr, float_value, dst_data);
                    }

                    write_ptr += dst_element_byte_width;
                }
            }
        }
        // Case 3: Converting from a matrix to a larger matrix will write into the top-left portion of the destination matrix, and fill the remaining space with the identity matrix.
        else
        {
            for (int row = 0; row < dst_row_col_count; ++row)
            {
                for (int col = 0; col < dst_row_col_count; ++col)
                {
                    // Use source matrix values in the top-left portion
                    if (row < src_row_col_count && col < src_row_col_count)
                    {
                        const uint32_t src_index = row * src_row_col_count + col;
                        const uint8_t* read_ptr = src_data.m_ValuePtr + src_index * src_element_byte_width;

                        if (src_data.m_DataType == dst_data.m_DataType)
                        {
                            memcpy(write_ptr, read_ptr, dst_element_byte_width);
                        }
                        else
                        {
                            float float_value = VertexAttributeDataTypeToFloat(src_data.m_DataType, read_ptr);

                            WriteVertexAttributeFromFloatToStorage(write_ptr, float_value, dst_data);
                        }
                    }
                    // Fill the rest with the identity matrix
                    else
                    {
                        WriteVertexAttributeFromFloatToStorage(write_ptr, (row == col) ? 1.0f : 0.0f, dst_data);
                    }

                    write_ptr += dst_element_byte_width;
                }
            }
        }
    }

    // Supports writing a vector to a scalar, matrix, or vector.
    // Regardless of destination type, if we have fewer elements in the source vector than in the destination vector,
    // the rest of the vector is filled with zeroes.
    static void WriteVector(uint8_t* write_ptr, const UnpackAttributeData& src_data, const UnpackAttributeData& dst_data)
    {
        const uint32_t num_src_elements_to_write  = dmMath::Min(src_data.m_ElementCount, dst_data.m_ElementCount);
        const uint32_t num_zero_elements_to_write = dst_data.m_ElementCount - num_src_elements_to_write;
        const uint32_t dst_element_byte_width     = DataTypeToByteWidth(dst_data.m_DataType);

        // Case 1: When src and dst data types match, just copy directly
        if (src_data.m_DataType == dst_data.m_DataType)
        {
            const uint32_t copy_size = num_src_elements_to_write * dst_element_byte_width;
            memcpy(write_ptr, src_data.m_ValuePtr, copy_size);
            write_ptr += copy_size; // Advance pointer
        }
        // Case 2: Convert source data type to float and then write to destination type
        else
        {
            const uint32_t src_element_byte_width = DataTypeToByteWidth(src_data.m_DataType);
            for (uint32_t i = 0; i < num_src_elements_to_write; ++i)
            {
                float float_value = VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr + i * src_element_byte_width);

                write_ptr = WriteVertexAttributeFromFloatToStorage(write_ptr, float_value, dst_data);
            }
        }

        // Zero out any remaining elements in the destination
        if (num_zero_elements_to_write > 0)
        {
            memset(write_ptr, 0, num_zero_elements_to_write * dst_element_byte_width);
        }
    }

    // Supports writing a single scalar as a scalar, matrix, or vector.
    // If the destination is a matrix, the value is written on the diagonal.
    // If the destination is a scalar or a vector, the value is written to each component of the destination.
    static void WriteScalar(uint8_t* write_ptr, const UnpackAttributeData& src_data, const UnpackAttributeData& dst_data)
    {
        uint8_t src_value_buffer[sizeof(float)];
        const uint8_t* src_value_read_ptr = 0x0;  // Use const for read-only
        const uint32_t dst_element_byte_width = DataTypeToByteWidth(dst_data.m_DataType);

        // Case 1: If source and destination data types are identical, skip conversion
        if (src_data.m_DataType == dst_data.m_DataType)
        {
            src_value_read_ptr = src_data.m_ValuePtr;
        }
        // Case 2: Convert source data type to float and then write to intermediate buffer
        else
        {
            float float_value = VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr);

            WriteVertexAttributeFromFloatToStorage(src_value_buffer, float_value, dst_data);
            src_value_read_ptr = src_value_buffer;
        }

        if (dst_data.m_IsMatrix)
        {
            const uint32_t dst_row_col_count = VectorTypeToMatrixRowColCount(dst_data.m_VectorType);
            const uint32_t total_elements = dst_row_col_count * dst_row_col_count;

            // Optimize matrix writing, write the diagonal elements, zero-fill non-diagonal
            for (uint32_t i = 0; i < total_elements; ++i)
            {
                if (i % (dst_row_col_count + 1) == 0)  // Diagonal check
                {
                    memcpy(write_ptr, src_value_read_ptr, dst_element_byte_width);
                }
                else
                {
                    memset(write_ptr, 0, dst_element_byte_width);
                }
                write_ptr += dst_element_byte_width;
            }
        }
        else  // Handle scalar or vector case
        {
            for (uint32_t i = 0; i < dst_data.m_ElementCount; ++i)
            {
                memcpy(write_ptr + i * dst_element_byte_width, src_value_read_ptr, dst_element_byte_width);
            }
        }
    }

    static void UnpackWriteAttributeBySemanticType(
        UnpackAttributeDataState& unpack_state,
        uint32_t vertex_index,
        const WriteAttributeParams& params,
        const VertexAttributeInfo& info,
        UnpackAttributeData* data)
    {
        const WriteAttributeStreamDesc* stream_desc = 0;
        uint8_t channel_index = 0;

        // Handle different semantic types
        switch(info.m_SemanticType)
        {
            case VertexAttribute::SEMANTIC_TYPE_POSITION:
                if (info.m_CoordinateSpace == dmGraphics::COORDINATE_SPACE_WORLD)
                {
                    stream_desc = &params.m_PositionsWorldSpace;
                    channel_index = unpack_state.m_ChannelIndexPositionsWorldSpace++;
                }
                else
                {
                    stream_desc = &params.m_PositionsLocalSpace;
                    channel_index = unpack_state.m_ChannelIndexPositionsLocalSpace++;
                }
                break;

            case VertexAttribute::SEMANTIC_TYPE_TEXCOORD:
                stream_desc = &params.m_TexCoords;
                channel_index = unpack_state.m_ChannelIndexTexCoords++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX:
                stream_desc = &params.m_PageIndices;
                channel_index = unpack_state.m_ChannelIndexPageIndices++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_COLOR:
                stream_desc = &params.m_Colors;
                channel_index = unpack_state.m_ChannelIndexColors++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_NORMAL:
                stream_desc = &params.m_Normals;
                channel_index = unpack_state.m_ChannelIndexNormals++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_TANGENT:
                stream_desc = &params.m_Tangents;
                channel_index = unpack_state.m_ChannelIndexTangents++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX:
                stream_desc = &params.m_WorldMatrix;
                channel_index = unpack_state.m_ChannelIndexWorldMatrix++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX:
                stream_desc = &params.m_NormalMatrix;
                channel_index = unpack_state.m_ChannelIndexNormalMatrix++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D:
                stream_desc = &params.m_TextureTransform2D;
                channel_index = unpack_state.m_ChannelIndexTextureTransform2D++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_MORPH_TARGET_WEIGHTS:
                stream_desc = &params.m_MorphTargetWeights;
                channel_index = unpack_state.m_ChannelIndexMorphTargetWeights++;
                break;

            case VertexAttribute::SEMANTIC_TYPE_NONE:
            case VertexAttribute::SEMANTIC_TYPE_BONE_WEIGHTS:
            case VertexAttribute::SEMANTIC_TYPE_BONE_INDICES:
                data->m_ValuePtr     = info.m_ValuePtr;
                data->m_VectorType   = info.m_ValueVectorType;
                data->m_DataType     = info.m_DataType;
                data->m_IsMatrix     = VectorTypeIsMatrix(info.m_ValueVectorType);
                data->m_ElementCount = VectorTypeToElementCount(info.m_ValueVectorType);
                return;

            default:
                assert(false && "Unknown semantic type");
                return;
        }

        if (stream_desc->m_Data && channel_index < stream_desc->m_StreamCount && stream_desc->m_Data[channel_index])
        {
            // We have semantic type and engine-provided data
            uint32_t element_count_out = VectorTypeToElementCount(stream_desc->m_VectorType);
            data->m_ElementCount       = element_count_out;
            data->m_IsMatrix           = VectorTypeIsMatrix(stream_desc->m_VectorType);
            data->m_VectorType         = stream_desc->m_VectorType;
            data->m_DataType           = VertexAttribute::TYPE_FLOAT;
            data->m_ValuePtr           = (const uint8_t*)(stream_desc->m_Data[channel_index] + (stream_desc->m_IsGlobalData ? 0 : vertex_index * element_count_out));
        }
        else
        {
            // The attribute is configured to use a semantic type != none, but there is no engine provided data.
            // In this case, we take the value from the vertex attribute. This can be zero/invalid, but it will
            // be handled outside of the function.
            data->m_ValuePtr     = info.m_ValuePtr;
            data->m_VectorType   = info.m_ValueVectorType;
            data->m_DataType     = info.m_DataType;
            data->m_IsMatrix     = VectorTypeIsMatrix(info.m_ValueVectorType);
            data->m_ElementCount = VectorTypeToElementCount(info.m_ValueVectorType);
        }
    }

    static inline void SetUnpackAttributeDataDefault(const VertexAttributeInfo& info, UnpackAttributeData& src_data, const UnpackAttributeData& dst_data)
    {
        if (dst_data.m_IsMatrix)
        {
            src_data.m_ValuePtr     = (const uint8_t*) g_SourceDataDefaultMatrix;
            src_data.m_ElementCount = 16;
            src_data.m_VectorType   = VertexAttribute::VECTOR_TYPE_MAT4;
            src_data.m_IsMatrix     = true;
        }
        else
        {
            if (info.m_SemanticType == VertexAttribute::SEMANTIC_TYPE_COLOR)
            {
                src_data.m_ValuePtr = (const uint8_t*) g_SourceDataDefaultVector;
            }
            else if (SemanticTypeRequiresOneAsW(info.m_SemanticType))
            {
                src_data.m_ValuePtr = (const uint8_t*) g_SourceDataDefaultVectorOneAsW;
            }
            else
            {
                src_data.m_ValuePtr = (const uint8_t*) g_SourceDataDefaultVectorEmpty;
            }
            src_data.m_ElementCount = 4;
            src_data.m_VectorType   = VertexAttribute::VECTOR_TYPE_VEC4;
            src_data.m_IsMatrix     = false;
        }
        src_data.m_DataType = VertexAttribute::TYPE_FLOAT;
    }

    // Conversion rules:
    // -----------------
    // Converting from a scalar or vector to a smaller scalar or vector will truncate values from the end of the source value.
    // Converting from a scalar or vector to a larger vector will zero-fill at the end, unless the semantic-type is position, color or tangent, in which case the W component will be one.
    // Converting from a scalar or vector to a larger matrix will zero-fill at the end.
    // Converting from a matrix to a smaller matrix will use the top-left portion of the source matrix.
    // Converting from a matrix to a larger matrix will write into the top-left portion of the destination matrix, and fill the remaining space with the identity matrix.
    // Converting from a matrix to a vector will truncate values from the end of the source matrix.
    uint8_t* WriteAttributes(uint8_t* write_ptr, uint32_t vertex_index, uint32_t vertex_count, const WriteAttributeParams& params)
    {
        UnpackAttributeData dst_data;
        UnpackAttributeData src_data;
        UnpackAttributeDataState unpack_state;

        for (int v = 0; v < vertex_count; ++v) {
            memset(&dst_data, 0, sizeof(dst_data));
            memset(&src_data, 0, sizeof(src_data));
            memset(&unpack_state, 0, sizeof(unpack_state));

            for (int i = 0; i < params.m_VertexAttributeInfos->m_NumInfos; ++i)
            {
                const VertexAttributeInfo& info = params.m_VertexAttributeInfos->m_Infos[i];
                if (info.m_StepFunction != params.m_StepFunction)
                {
                    continue;
                }

                dst_data.m_VectorType   = info.m_VectorType;
                dst_data.m_DataType     = info.m_DataType;
                dst_data.m_ElementCount = VectorTypeToElementCount(dst_data.m_VectorType);
                dst_data.m_IsMatrix     = VectorTypeIsMatrix(dst_data.m_VectorType);
                dst_data.m_Normalize    = info.m_Normalize;

                const uint32_t dst_element_byte_width = DataTypeToByteWidth(dst_data.m_DataType);
                const size_t attribute_stride         = dst_data.m_ElementCount * dst_element_byte_width;

                UnpackWriteAttributeBySemanticType(unpack_state, vertex_index + v, params, info, &src_data);

                // If there is no data available at all, we pick one of the default float arrays
                if (src_data.m_ValuePtr == 0)
                {
                    SetUnpackAttributeDataDefault(info, src_data, dst_data);
                }

                if (src_data.m_VectorType == VertexAttribute::VECTOR_TYPE_SCALAR)
                {
                    WriteScalar(write_ptr, src_data, dst_data);
                }
                else if (src_data.m_IsMatrix && dst_data.m_IsMatrix)
                {
                    WriteMatrixToMatrix(write_ptr, src_data, dst_data);
                }
                else
                {
                    if (src_data.m_ElementCount < dst_data.m_ElementCount && dst_data.m_ElementCount >= 4 && SemanticTypeRequiresOneAsW(info.m_SemanticType))
                    {
                        uint8_t v4_one_as_w_backing[sizeof(float)*4] = {};
                        memcpy(v4_one_as_w_backing, src_data.m_ValuePtr, src_data.m_ElementCount * DataTypeToByteWidth(src_data.m_DataType));

                        if (src_data.m_DataType == VertexAttribute::TYPE_FLOAT)
                        {
                            ((float*) v4_one_as_w_backing)[3] = 1.0;
                        }
                        else
                        {
                            WriteVertexAttributeFromFloatToStorage(v4_one_as_w_backing + 3 * dst_element_byte_width, 1.0f, dst_data);
                        }

                        src_data.m_ValuePtr     = v4_one_as_w_backing;
                        src_data.m_ElementCount = 4;
                        WriteVector(write_ptr, src_data, dst_data);
                    }
                    else
                    {
                        WriteVector(write_ptr, src_data, dst_data);
                    }
                }

                write_ptr += attribute_stride;
            }
        }
        return write_ptr;
    }

    enum AttributeWriteOperation
    {
        COPY_1, COPY_2, COPY_4, COPY_8, COPY_12, COPY_16, COPY_36, COPY_64, COPY_BYTES,
        FLOAT3_TO_FLOAT4_W, FLOAT4_TO_UNORM8, FLOAT4_TO_SNORM16, FLOAT_MAT4_TO_MAT3,
        GENERIC_SCALAR, GENERIC_VECTOR, GENERIC_MATRIX, GENERIC_VECTOR_W
    };

    enum AttributeStream
    {
        ATTRIBUTE_STREAM_NONE = -1,
        ATTRIBUTE_STREAM_POSITION_LOCAL,
        ATTRIBUTE_STREAM_POSITION_WORLD,
        ATTRIBUTE_STREAM_TEXCOORD,
        ATTRIBUTE_STREAM_PAGE_INDEX,
        ATTRIBUTE_STREAM_COLOR,
        ATTRIBUTE_STREAM_NORMAL,
        ATTRIBUTE_STREAM_TANGENT,
        ATTRIBUTE_STREAM_WORLD_MATRIX,
        ATTRIBUTE_STREAM_NORMAL_MATRIX,
        ATTRIBUTE_STREAM_TEXTURE_TRANSFORM,
        ATTRIBUTE_STREAM_MORPH_WEIGHTS,
        ATTRIBUTE_STREAM_COUNT
    };

    struct PreparedAttribute
    {
        UnpackAttributeData               m_SourceFormat;
        UnpackAttributeData               m_DestinationFormat;
        uint32_t                          m_AttributeIndex;
        int32_t                           m_StreamOffset;
        uint32_t                          m_Channel;
        uint32_t                          m_SourceStride;
        uint32_t                          m_DestinationOffset;
        uint32_t                          m_Bytes;
        AttributeWriteOperation           m_ConversionOperation;
        AttributeWriteOperation           m_CopyOperation;
        AttributeWriteOperation           m_WriteOperation;
        uint8_t                           m_Packed[16 * sizeof(float)]; // Largest supported attribute: float mat4.
    };

    WriteAttributeState::WriteAttributeState()
    : m_Data(0), m_Capacity(0), m_NumInfos(0), m_NumOperations(0), m_VertexStride(0), m_StepFunction(VERTEX_STEP_FUNCTION_VERTEX)
    {
    }

    WriteAttributeState::~WriteAttributeState()
    {
        free(m_Data);
    }

    static PreparedAttribute* GetPreparedAttributes(const WriteAttributeState& state)
    {
        // One allocation holds the layout snapshot followed by the operations.
        return (PreparedAttribute*)((VertexAttributeInfo*)state.m_Data + state.m_Capacity);
    }

    // Indexed by AttributeStream.
    static const uint32_t STREAM_OFFSETS[ATTRIBUTE_STREAM_COUNT] = {
        offsetof(WriteAttributeParams, m_PositionsLocalSpace),
        offsetof(WriteAttributeParams, m_PositionsWorldSpace),
        offsetof(WriteAttributeParams, m_TexCoords),
        offsetof(WriteAttributeParams, m_PageIndices),
        offsetof(WriteAttributeParams, m_Colors),
        offsetof(WriteAttributeParams, m_Normals),
        offsetof(WriteAttributeParams, m_Tangents),
        offsetof(WriteAttributeParams, m_WorldMatrix),
        offsetof(WriteAttributeParams, m_NormalMatrix),
        offsetof(WriteAttributeParams, m_TextureTransform2D),
        offsetof(WriteAttributeParams, m_MorphTargetWeights)
    };

    static AttributeStream GetAttributeStream(const VertexAttributeInfo& info)
    {
        switch (info.m_SemanticType)
        {
            case VertexAttribute::SEMANTIC_TYPE_POSITION:
            {
                return info.m_CoordinateSpace == COORDINATE_SPACE_WORLD ? ATTRIBUTE_STREAM_POSITION_WORLD : ATTRIBUTE_STREAM_POSITION_LOCAL;
            }
            case VertexAttribute::SEMANTIC_TYPE_TEXCOORD:
            {
                return ATTRIBUTE_STREAM_TEXCOORD;
            }
            case VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX:
            {
                return ATTRIBUTE_STREAM_PAGE_INDEX;
            }
            case VertexAttribute::SEMANTIC_TYPE_COLOR:
            {
                return ATTRIBUTE_STREAM_COLOR;
            }
            case VertexAttribute::SEMANTIC_TYPE_NORMAL:
            {
                return ATTRIBUTE_STREAM_NORMAL;
            }
            case VertexAttribute::SEMANTIC_TYPE_TANGENT:
            {
                return ATTRIBUTE_STREAM_TANGENT;
            }
            case VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX:
            {
                return ATTRIBUTE_STREAM_WORLD_MATRIX;
            }
            case VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX:
            {
                return ATTRIBUTE_STREAM_NORMAL_MATRIX;
            }
            case VertexAttribute::SEMANTIC_TYPE_TEXTURE_TRANSFORM_2D:
            {
                return ATTRIBUTE_STREAM_TEXTURE_TRANSFORM;
            }
            case VertexAttribute::SEMANTIC_TYPE_MORPH_TARGET_WEIGHTS:
            {
                return ATTRIBUTE_STREAM_MORPH_WEIGHTS;
            }
            case VertexAttribute::SEMANTIC_TYPE_NONE:
            case VertexAttribute::SEMANTIC_TYPE_BONE_WEIGHTS:
            case VertexAttribute::SEMANTIC_TYPE_BONE_INDICES:
            {
                return ATTRIBUTE_STREAM_NONE;
            }
            default:
            {
                assert(false && "Unknown semantic type");
                return ATTRIBUTE_STREAM_NONE;
            }
        }
    }

    static AttributeWriteOperation GetCopyOperation(uint32_t bytes)
    {
        switch (bytes)
        {
            case 1:
            {
                return COPY_1;
            }
            case 2:
            {
                return COPY_2;
            }
            case 4:
            {
                return COPY_4;
            }
            case 8:
            {
                return COPY_8;
            }
            case 12:
            {
                return COPY_12;
            }
            case 16:
            {
                return COPY_16;
            }
            case 36:
            {
                return COPY_36;
            }
            case 64:
            {
                return COPY_64;
            }
            default:
            {
                return COPY_BYTES;
            }
        }
    }

    static void WritePreparedAttribute(AttributeWriteOperation operation, uint8_t* out, const uint8_t* source, const PreparedAttribute& entry)
    {
        switch (operation)
        {
            case COPY_1:
            {
                memcpy(out, source, 1);
                return;
            }
            case COPY_2:
            {
                memcpy(out, source, 2);
                return;
            }
            case COPY_4:
            {
                memcpy(out, source, 4);
                return;
            }
            case COPY_8:
            {
                memcpy(out, source, 8);
                return;
            }
            case COPY_12:
            {
                memcpy(out, source, 12);
                return;
            }
            case COPY_16:
            {
                memcpy(out, source, 16);
                return;
            }
            case COPY_36:
            {
                memcpy(out, source, 36);
                return;
            }
            case COPY_64:
            {
                memcpy(out, source, 64);
                return;
            }
            case COPY_BYTES:
            {
                memcpy(out, source, entry.m_Bytes);
                return;
            }
            case FLOAT3_TO_FLOAT4_W:
            {
                const float one = 1.0f;
                memcpy(out, source, 12);
                memcpy(out + 12, &one, 4);
                return;
            }
            case FLOAT4_TO_UNORM8:
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    float value;
                    memcpy(&value, source + i * 4, 4);
                    float packed = dmMath::Clamp(value, -1.0f, 1.0f) * 255.0f + 0.5f;
                    out[i] = (uint8_t)dmMath::Clamp(packed, 0.0f, 255.0f);
                }
                return;
            }
            case FLOAT4_TO_SNORM16:
            {
                for (uint32_t i = 0; i < 4; ++i)
                {
                    float value;
                    memcpy(&value, source + i * 4, 4);
                    float scale = value > 0.0f ? 32767.0f : 32768.0f;
                    float packed = floorf(dmMath::Clamp(value, -1.0f, 1.0f) * scale + 0.5f);
                    int16_t stored = (int16_t)dmMath::Clamp(packed, -32768.0f, 32767.0f);
                    memcpy(out + i * 2, &stored, 2);
                }
                return;
            }
            case FLOAT_MAT4_TO_MAT3:
            {
                memcpy(out, source, 12);
                memcpy(out + 12, source + 16, 12);
                memcpy(out + 24, source + 32, 12);
                return;
            }
            default:
            {
                break;
            }
        }
        UnpackAttributeData src = entry.m_SourceFormat;
        src.m_ValuePtr = source;
        const UnpackAttributeData& dst = entry.m_DestinationFormat;
        switch (operation)
        {
            case GENERIC_SCALAR:
            {
                WriteScalar(out, src, dst);
                break;
            }
            case GENERIC_VECTOR:
            {
                WriteVector(out, src, dst);
                break;
            }
            case GENERIC_MATRIX:
            {
                WriteMatrixToMatrix(out, src, dst);
                break;
            }
            case GENERIC_VECTOR_W:
            {
                uint8_t backing[16] = {};
                memcpy(backing, source, src.m_ElementCount * DataTypeToByteWidth(src.m_DataType));
                if (src.m_DataType == VertexAttribute::TYPE_FLOAT)
                {
                    const float one = 1.0f;
                    memcpy(backing + 12, &one, 4);
                }
                else
                {
                    WriteVertexAttributeFromFloatToStorage(backing + 3 * DataTypeToByteWidth(dst.m_DataType), 1.0f, dst);
                }
                src.m_ValuePtr = backing;
                src.m_ElementCount = 4;
                WriteVector(out, src, dst);
                break;
            }
            default:
            {
                assert(false);
                break;
            }
        }
    }

    static AttributeWriteOperation SelectWriteOperation(const PreparedAttribute& entry, const VertexAttributeInfo& info)
    {
        const UnpackAttributeData& src = entry.m_SourceFormat;
        const UnpackAttributeData& dst = entry.m_DestinationFormat;
        const bool needs_w = src.m_ElementCount < dst.m_ElementCount && dst.m_ElementCount >= 4 && SemanticTypeRequiresOneAsW(info.m_SemanticType);
        AttributeWriteOperation operation = GENERIC_VECTOR;
        if (src.m_VectorType == VertexAttribute::VECTOR_TYPE_SCALAR)
        {
            operation = GENERIC_SCALAR;
        }
        else if (src.m_IsMatrix && dst.m_IsMatrix)
        {
            operation = GENERIC_MATRIX;
        }
        else if (needs_w)
        {
            operation = GENERIC_VECTOR_W;
        }
        if (src.m_DataType == dst.m_DataType &&
            ((src.m_ElementCount == dst.m_ElementCount) ||
             (operation == GENERIC_VECTOR && src.m_ElementCount >= dst.m_ElementCount)))
        {
            return entry.m_CopyOperation;
        }
        if (src.m_DataType == VertexAttribute::TYPE_FLOAT && dst.m_DataType == VertexAttribute::TYPE_FLOAT &&
            src.m_VectorType == VertexAttribute::VECTOR_TYPE_VEC3 && dst.m_VectorType == VertexAttribute::VECTOR_TYPE_VEC4 && needs_w)
        {
            return FLOAT3_TO_FLOAT4_W;
        }
        if (src.m_VectorType == VertexAttribute::VECTOR_TYPE_VEC4 && dst.m_VectorType == VertexAttribute::VECTOR_TYPE_VEC4 && src.m_DataType == VertexAttribute::TYPE_FLOAT && dst.m_Normalize)
        {
            if (dst.m_DataType == VertexAttribute::TYPE_UNSIGNED_BYTE)
            {
                return FLOAT4_TO_UNORM8;
            }
            if (dst.m_DataType == VertexAttribute::TYPE_SHORT)
            {
                return FLOAT4_TO_SNORM16;
            }
        }
        if (src.m_VectorType == VertexAttribute::VECTOR_TYPE_MAT4 && dst.m_VectorType == VertexAttribute::VECTOR_TYPE_MAT3 && src.m_DataType == VertexAttribute::TYPE_FLOAT && dst.m_DataType == VertexAttribute::TYPE_FLOAT)
        {
            return FLOAT_MAT4_TO_MAT3;
        }
        return operation;
    }

    static bool AttributeLayoutMatches(const WriteAttributeState& state, const WriteAttributeParams& params)
    {
        if (state.m_NumInfos != params.m_VertexAttributeInfos->m_NumInfos || state.m_StepFunction != params.m_StepFunction)
        {
            return false;
        }
        const VertexAttributeInfo* cached = (const VertexAttributeInfo*)state.m_Data;
        for (uint32_t i = 0; i < state.m_NumInfos; ++i)
        {
            const VertexAttributeInfo& info = params.m_VertexAttributeInfos->m_Infos[i];
            if (cached[i].m_SemanticType != info.m_SemanticType || cached[i].m_CoordinateSpace != info.m_CoordinateSpace ||
                cached[i].m_DataType != info.m_DataType || cached[i].m_VectorType != info.m_VectorType ||
                cached[i].m_StepFunction != info.m_StepFunction || cached[i].m_Normalize != info.m_Normalize)
            {
                return false;
            }
        }
        return true;
    }

    static void BuildAttributeLayout(WriteAttributeState* state, const WriteAttributeParams& params)
    {
        const uint32_t count = params.m_VertexAttributeInfos->m_NumInfos;
        if (count > state->m_Capacity)
        {
            free(state->m_Data);
            state->m_Capacity = count;
            state->m_Data = malloc((sizeof(VertexAttributeInfo) + sizeof(PreparedAttribute)) * count);
        }
        state->m_NumInfos = count;
        state->m_StepFunction = params.m_StepFunction;
        state->m_NumOperations = 0;
        state->m_VertexStride = 0;
        if (count == 0)
        {
            return;
        }
        VertexAttributeInfo* cached = (VertexAttributeInfo*)state->m_Data;
        PreparedAttribute* operations = GetPreparedAttributes(*state);
        uint32_t channels[ATTRIBUTE_STREAM_COUNT] = {};
        for (uint32_t i = 0; i < count; ++i)
        {
            const VertexAttributeInfo& info = params.m_VertexAttributeInfos->m_Infos[i];
            cached[i] = info;
            cached[i].m_ValuePtr = 0; // Layout snapshots never retain transient values.
            if (info.m_StepFunction != params.m_StepFunction)
            {
                continue;
            }
            PreparedAttribute& entry = operations[state->m_NumOperations++];
            entry.m_AttributeIndex = i;
            entry.m_StreamOffset = -1;
            entry.m_Channel = 0;
            AttributeStream stream = GetAttributeStream(info);
            if (stream != ATTRIBUTE_STREAM_NONE)
            {
                entry.m_StreamOffset = STREAM_OFFSETS[stream];
                entry.m_Channel = channels[stream]++;
            }
            entry.m_DestinationFormat.m_VectorType = info.m_VectorType;
            entry.m_DestinationFormat.m_DataType = info.m_DataType;
            entry.m_DestinationFormat.m_ElementCount = VectorTypeToElementCount(info.m_VectorType);
            entry.m_DestinationFormat.m_IsMatrix = VectorTypeIsMatrix(info.m_VectorType);
            entry.m_DestinationFormat.m_Normalize = info.m_Normalize;
            entry.m_DestinationFormat.m_ValuePtr = 0;
            entry.m_Bytes = entry.m_DestinationFormat.m_ElementCount * DataTypeToByteWidth(info.m_DataType);
            entry.m_CopyOperation = GetCopyOperation(entry.m_Bytes);
            entry.m_DestinationOffset = state->m_VertexStride;
            state->m_VertexStride += entry.m_Bytes;
            entry.m_SourceFormat.m_VectorType = (VertexAttribute::VectorType)-1;
        }
    }

    void PrepareWriteAttributes(WriteAttributeState* state, const WriteAttributeParams& params, uint32_t vertex_count)
    {
        if (!AttributeLayoutMatches(*state, params))
        {
            BuildAttributeLayout(state, params);
        }
        if (state->m_NumOperations == 0)
        {
            return;
        }
        PreparedAttribute* operations = GetPreparedAttributes(*state);
        for (uint32_t i = 0; i < state->m_NumOperations; ++i)
        {
            PreparedAttribute& entry = operations[i];
            const VertexAttributeInfo& info = params.m_VertexAttributeInfos->m_Infos[entry.m_AttributeIndex];
            const uint8_t* source = 0;
            VertexAttribute::VectorType vector_type = info.m_ValueVectorType;
            VertexAttribute::DataType data_type = info.m_DataType;
            bool per_vertex = false;
            if (entry.m_StreamOffset >= 0)
            {
                const WriteAttributeStreamDesc& stream = *(const WriteAttributeStreamDesc*)((const uint8_t*)&params + entry.m_StreamOffset);
                if (stream.m_Data && entry.m_Channel < stream.m_StreamCount && stream.m_Data[entry.m_Channel])
                {
                    source = (const uint8_t*)stream.m_Data[entry.m_Channel];
                    vector_type = stream.m_VectorType;
                    data_type = VertexAttribute::TYPE_FLOAT;
                    per_vertex = !stream.m_IsGlobalData;
                }
            }
            if (!source)
            {
                source = info.m_ValuePtr;
                if (!source)
                {
                    UnpackAttributeData defaults;
                    SetUnpackAttributeDataDefault(info, defaults, entry.m_DestinationFormat);
                    source = defaults.m_ValuePtr;
                    vector_type = defaults.m_VectorType;
                    data_type = defaults.m_DataType;
                }
            }
            if (entry.m_SourceFormat.m_VectorType != vector_type || entry.m_SourceFormat.m_DataType != data_type)
            {
                entry.m_SourceFormat.m_VectorType = vector_type;
                entry.m_SourceFormat.m_DataType = data_type;
                entry.m_SourceFormat.m_ElementCount = VectorTypeToElementCount(vector_type);
                entry.m_SourceFormat.m_IsMatrix = VectorTypeIsMatrix(vector_type);
                entry.m_SourceFormat.m_Normalize = 0;
                entry.m_ConversionOperation = SelectWriteOperation(entry, info);
            }
            entry.m_SourceStride = per_vertex ? entry.m_SourceFormat.m_ElementCount * sizeof(float) : 0;
            entry.m_SourceFormat.m_ValuePtr = source;
            entry.m_WriteOperation = entry.m_ConversionOperation;
            // Shared conversions are amortized over all writes for this object,
            // including indexed calls. A single write uses the operation directly.
            if (!per_vertex && vertex_count > 1 && entry.m_ConversionOperation != entry.m_CopyOperation)
            {
                WritePreparedAttribute(entry.m_ConversionOperation, entry.m_Packed, source, entry);
                entry.m_SourceFormat.m_ValuePtr = entry.m_Packed;
                entry.m_WriteOperation = entry.m_CopyOperation;
            }
        }
    }

    uint8_t* WriteAttributes(uint8_t* write_ptr, uint32_t vertex_index, uint32_t vertex_count, const WriteAttributeState& state)
    {
        if (state.m_NumOperations == 0)
        {
            return write_ptr;
        }
        const PreparedAttribute* operations = GetPreparedAttributes(state);
        for (uint32_t v = 0; v < vertex_count; ++v)
        {
            for (uint32_t i = 0; i < state.m_NumOperations; ++i)
            {
                const PreparedAttribute& entry = operations[i];
                WritePreparedAttribute(entry.m_WriteOperation, write_ptr + entry.m_DestinationOffset,
                                       entry.m_SourceFormat.m_ValuePtr + (vertex_index + v) * entry.m_SourceStride, entry);
            }
            write_ptr += state.m_VertexStride;
        }
        return write_ptr;
    }
}
