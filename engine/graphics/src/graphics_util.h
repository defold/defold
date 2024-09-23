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

#ifndef DM_GRAPHICS_UTIL_H
#define DM_GRAPHICS_UTIL_H

#include <dlib/endian.h>
#include <dmsdk/dlib/vmath.h>

#include "graphics.h"

namespace dmGraphics
{
    struct VertexAttributeInfoMetadata
    {
        uint32_t m_HasAttributeWorldPosition : 1;
        uint32_t m_HasAttributeLocalPosition : 1;
        uint32_t m_HasAttributeNormal        : 1;
        uint32_t m_HasAttributeTangent       : 1;
        uint32_t m_HasAttributeColor         : 1;
        uint32_t m_HasAttributeTexCoord      : 1;
        uint32_t m_HasAttributePageIndex     : 1;
        uint32_t m_HasAttributeWorldMatrix   : 1;
        uint32_t m_HasAttributeNormalMatrix  : 1;
        uint32_t m_HasAttributeNone          : 1;
    };

    inline uint32_t PackRGBA(const dmVMath::Vector4& in_color)
    {
        uint8_t r = (uint8_t)(in_color.getX() * 255.0f);
        uint8_t g = (uint8_t)(in_color.getY() * 255.0f);
        uint8_t b = (uint8_t)(in_color.getZ() * 255.0f);
        uint8_t a = (uint8_t)(in_color.getW() * 255.0f);

#if DM_ENDIAN == DM_ENDIAN_LITTLE
        uint32_t c = a << 24 | b << 16 | g << 8 | r;
#else
        uint32_t c = r << 24 | g << 16 | b << 8 | a;
#endif
        return c;
    }

    inline const dmVMath::Vector4 UnpackRGBA(uint32_t in_color)
    {
#if DM_ENDIAN == DM_ENDIAN_LITTLE
        float r = (float)((in_color & 0x000000FF)      ) / 255.0f;
        float g = (float)((in_color & 0x0000FF00) >>  8) / 255.0f;
        float b = (float)((in_color & 0x00FF0000) >> 16) / 255.0f;
        float a = (float)((in_color & 0xFF000000) >> 24) / 255.0f;
#else
        float r = (float)((in_color & 0xFF000000) >> 24) / 255.0f;
        float g = (float)((in_color & 0x00FF0000) >> 16) / 255.0f;
        float b = (float)((in_color & 0x0000FF00) >>  8) / 255.0f;
        float a = (float)((in_color & 0x000000FF)      ) / 255.0f;
#endif
        return dmVMath::Vector4(r,g,b,a);
    }

    static inline HAssetHandle MakeAssetHandle(HOpaqueHandle opaque_handle, AssetType asset_type)
    {
        assert(asset_type != ASSET_TYPE_NONE && "Invalid asset type");
        uint64_t handle = ((uint64_t) asset_type) << 32 | opaque_handle;
        assert(handle <= MAX_ASSET_HANDLE_VALUE);
        return handle;
    }

    static inline AssetType GetAssetType(HAssetHandle asset_handle)
    {
        return (AssetType) (asset_handle >> 32);
    }

    static inline HOpaqueHandle GetOpaqueHandle(HAssetHandle asset_handle)
    {
        return (HOpaqueHandle) asset_handle & 0xFFFFFFFF;
    }

    static inline bool IsColorBufferType(BufferType buffer_type)
    {
        return buffer_type == BUFFER_TYPE_COLOR0_BIT ||
               buffer_type == BUFFER_TYPE_COLOR1_BIT ||
               buffer_type == BUFFER_TYPE_COLOR2_BIT ||
               buffer_type == BUFFER_TYPE_COLOR3_BIT;
    }

    static inline void VertexAttributeInfoMetadataMember(VertexAttributeInfoMetadata& metadata, VertexAttribute::SemanticType semantic_type, CoordinateSpace coordinate_space)
    {
        switch(semantic_type)
        {
        case VertexAttribute::SEMANTIC_TYPE_POSITION:
            metadata.m_HasAttributeWorldPosition |= coordinate_space == COORDINATE_SPACE_WORLD;
            metadata.m_HasAttributeLocalPosition |= coordinate_space == COORDINATE_SPACE_LOCAL;
            break;
        case VertexAttribute::SEMANTIC_TYPE_TEXCOORD:
            metadata.m_HasAttributeTexCoord = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_PAGE_INDEX:
            metadata.m_HasAttributePageIndex = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_NORMAL:
            metadata.m_HasAttributeNormal = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_TANGENT:
            metadata.m_HasAttributeTangent = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_COLOR:
            metadata.m_HasAttributeColor = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX:
            metadata.m_HasAttributeWorldMatrix = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX:
            metadata.m_HasAttributeNormalMatrix = true;
            break;
        case VertexAttribute::SEMANTIC_TYPE_NONE:
            metadata.m_HasAttributeNone = true;
            break;
        default:
            break;
        }
    }

    static inline VertexAttributeInfoMetadata GetVertexAttributeInfosMetaData(const VertexAttributeInfos& attribute_infos)
    {
        VertexAttributeInfoMetadata metadata = {};
        for (int i = 0; i < attribute_infos.m_NumInfos; ++i)
        {
            const VertexAttributeInfo& info = attribute_infos.m_Infos[i];
            VertexAttributeInfoMetadataMember(metadata, info.m_SemanticType, info.m_CoordinateSpace);
        }
        return metadata;
    }

    static inline bool IsTypeTextureType(Type type)
    {
        return type == TYPE_SAMPLER_2D ||
               type == TYPE_SAMPLER_CUBE ||
               type == TYPE_SAMPLER_2D_ARRAY ||
               type == TYPE_IMAGE_2D;
    }

    static inline uint32_t GetLayerCount(TextureType type)
    {
        return type == TEXTURE_TYPE_CUBE_MAP ? 6 : 1;
    }

    static inline uint32_t VectorTypeToElementCount(VertexAttribute::VectorType vector_type)
    {
        switch(vector_type)
        {
            case VertexAttribute::VECTOR_TYPE_SCALAR: return 1;
            case VertexAttribute::VECTOR_TYPE_VEC2:   return 2;
            case VertexAttribute::VECTOR_TYPE_VEC3:   return 3;
            case VertexAttribute::VECTOR_TYPE_VEC4:   return 4;
            case VertexAttribute::VECTOR_TYPE_MAT2:   return 4;
            case VertexAttribute::VECTOR_TYPE_MAT3:   return 9;
            case VertexAttribute::VECTOR_TYPE_MAT4:   return 16;
        }
        return 0;
    }

    static inline uint32_t DataTypeToByteWidth(VertexAttribute::DataType data_type)
    {
        switch(data_type)
        {
            case dmGraphics::VertexAttribute::TYPE_BYTE:           return 1;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_BYTE:  return 1;
            case dmGraphics::VertexAttribute::TYPE_SHORT:          return 2;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_SHORT: return 2;
            case dmGraphics::VertexAttribute::TYPE_INT:            return 4;
            case dmGraphics::VertexAttribute::TYPE_UNSIGNED_INT:   return 4;
            case dmGraphics::VertexAttribute::TYPE_FLOAT:          return 4;
            default: break;
        }
        return 0;
    }
}



#endif // #ifndef DM_GRAPHICS_UTIL_H
