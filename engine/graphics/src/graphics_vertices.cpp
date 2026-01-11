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
    };

    struct UnpackAttributeData
    {
        VertexAttribute::VectorType m_VectorType;
        VertexAttribute::DataType   m_DataType;
        const uint8_t*              m_ValuePtr;
        uint32_t                    m_ElementCount;
        uint8_t                     m_IsMatrix : 1;
    };

    void GetAttributeValues(const dmGraphics::VertexAttribute& attribute, const uint8_t** data_ptr, uint32_t* data_size)
    {
        *data_ptr  = attribute.m_Values.m_BinaryValues.m_Data;
        *data_size = attribute.m_Values.m_BinaryValues.m_Count;
    }

    uint8_t* WriteVertexAttributeFromFloat(uint8_t* value_write_ptr, float value, dmGraphics::VertexAttribute::DataType data_type)
    {
        switch (data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:
                *((int8_t*) value_write_ptr) = (int8_t) dmMath::Clamp(value, -128.0f, 127.0f);
                return value_write_ptr + 1;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:
                *value_write_ptr = (uint8_t) dmMath::Clamp(value, 0.0f, 255.0f);
                return value_write_ptr + 1;
            case dmGraphics::VertexAttribute::TYPE_SHORT:
                *((int16_t*) value_write_ptr) = (int16_t) dmMath::Clamp(value, -32768.0f, 32767.0f);
                return value_write_ptr + 2;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT:
                *((uint16_t*) value_write_ptr) = (uint16_t) dmMath::Clamp(value, 0.0f, 65535.0f);
                return value_write_ptr + 2;
            case dmGraphics::VertexAttribute::TYPE_INT:
                *((int32_t*) value_write_ptr) = (int32_t) dmMath::Clamp(value, -2147483648.0f, 2147483647.0f);
                return value_write_ptr + 4;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:
                *((uint32_t*) value_write_ptr) = (uint32_t) dmMath::Clamp(value, 0.0f, 4294967295.0f);
                return value_write_ptr + 4;
            case dmGraphics::VertexAttribute::TYPE_FLOAT:
                *((float*) value_write_ptr) = value;
                return value_write_ptr + 4;
            default:break;
        }
        return 0;
    }

    float VertexAttributeDataTypeToFloat(const dmGraphics::VertexAttribute::DataType data_type, const uint8_t* value_ptr)
    {
        switch (data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:           return (float) ((int8_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:  return (float) value_ptr[0];
            case dmGraphics::VertexAttribute::TYPE_SHORT:          return (float) ((int16_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT: return (float) ((uint16_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_INT:            return (float) ((int32_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:   return (float) ((uint32_t*) value_ptr)[0];
            case dmGraphics::VertexAttribute::TYPE_FLOAT:          return (float) ((float*) value_ptr)[0];
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
                    float float_value = (src_data.m_DataType == VertexAttribute::TYPE_FLOAT)
                                        ? ((float*) src_data.m_ValuePtr)[i]
                                        : VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr + i * src_element_byte_width);

                    write_ptr = WriteVertexAttributeFromFloat(write_ptr, float_value, dst_data.m_DataType);
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
                        float float_value = (src_data.m_DataType == VertexAttribute::TYPE_FLOAT)
                                            ? ((float*) src_data.m_ValuePtr)[src_index]
                                            : VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr + src_index * src_element_byte_width);

                        write_ptr = WriteVertexAttributeFromFloat(write_ptr, float_value, dst_data.m_DataType);
                    }

                    write_ptr += dst_element_byte_width;
                }
            }
        }
        // Case 3: Converting from a matrix to a larger matrix will write into the top-left portion of the destination matrix, and fill the remaining space with the identity matrix.
        else
        {
            uint8_t* read_ptr = (uint8_t*) src_data.m_ValuePtr;
            for (int row = 0; row < dst_row_col_count; ++row)
            {
                for (int col = 0; col < dst_row_col_count; ++col)
                {
                    // Use source matrix values in the top-left portion
                    if (row < src_row_col_count && col < src_row_col_count)
                    {
                        const uint32_t src_index = row * src_row_col_count + col;

                        if (src_data.m_DataType == dst_data.m_DataType)
                        {
                            memcpy(write_ptr, read_ptr, dst_element_byte_width);
                        }
                        else
                        {
                            float float_value = (src_data.m_DataType == VertexAttribute::TYPE_FLOAT)
                                            ? ((float*) read_ptr)[src_index]
                                            : VertexAttributeDataTypeToFloat(src_data.m_DataType, read_ptr + src_index * src_element_byte_width);

                            WriteVertexAttributeFromFloat(write_ptr, float_value, dst_data.m_DataType);
                        }
                        read_ptr += dst_element_byte_width;
                    }
                    // Fill the rest with the identity matrix
                    else
                    {
                        WriteVertexAttributeFromFloat(write_ptr, (row == col) ? 1.0f : 0.0f, dst_data.m_DataType);
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
            const float* float_value_ptr = (const float*) src_data.m_ValuePtr;

            for (uint32_t i = 0; i < num_src_elements_to_write; ++i)
            {
                float float_value = (src_data.m_DataType == VertexAttribute::TYPE_FLOAT)
                                    ? float_value_ptr[i]
                                    : VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr + i * src_element_byte_width);

                write_ptr = WriteVertexAttributeFromFloat(write_ptr, float_value, dst_data.m_DataType);
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
            float float_value = (src_data.m_DataType == VertexAttribute::TYPE_FLOAT) ?
                                *((const float*) src_data.m_ValuePtr) :
                                VertexAttributeDataTypeToFloat(src_data.m_DataType, src_data.m_ValuePtr);

            WriteVertexAttributeFromFloat(src_value_buffer, float_value, dst_data.m_DataType);
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

            case VertexAttribute::SEMANTIC_TYPE_NONE:
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
                            WriteVertexAttributeFromFloat(v4_one_as_w_backing + 3 * dst_element_byte_width, 1.0, dst_data.m_DataType);
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
}
