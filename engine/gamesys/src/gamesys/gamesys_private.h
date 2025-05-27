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

#ifndef DM_GAMESYS_PRIVER_H
#define DM_GAMESYS_PRIVER_H

#include <dlib/message.h>
#include <dlib/object_pool.h>
#include <dlib/buffer.h>

#include <render/render.h>

#include <gameobject/gameobject.h>

namespace dmScript
{
    struct LuaCallbackInfo;
}

namespace dmGameSystem
{
#define EXT_CONSTANTS(prefix, ext)\
    static const char* prefix##_EXT = ext;\
    static const dmhash_t prefix##_EXT_HASH = dmHashString64(ext);\

    EXT_CONSTANTS(COLLECTION_FACTORY, "collectionfactoryc")
    EXT_CONSTANTS(COLLISION_OBJECT, "collisionobjectc")
    EXT_CONSTANTS(FACTORY, "factoryc")
    EXT_CONSTANTS(FONT, "fontc")
    EXT_CONSTANTS(MATERIAL, "materialc")
    EXT_CONSTANTS(BUFFER, "bufferc")
    EXT_CONSTANTS(MODEL, "modelc")
    EXT_CONSTANTS(TEXTURE, "texturec")
    EXT_CONSTANTS(TEXTURE_SET, "texturesetc")
    EXT_CONSTANTS(TILE_MAP, "tilemapc")
    EXT_CONSTANTS(RENDER_TARGET, "render_targetc")
    EXT_CONSTANTS(COLLECTION_PROXY, "collectionproxyc")

#undef EXT_CONSTANTS

    static const dmhash_t PROP_FONT = dmHashString64("font");
    static const dmhash_t PROP_FONTS = dmHashString64("fonts");
    static const dmhash_t PROP_IMAGE = dmHashString64("image");
    static const dmhash_t PROP_MATERIAL = dmHashString64("material");
    static const dmhash_t PROP_MATERIALS = dmHashString64("materials");
    static const dmhash_t PROP_TEXTURE[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {
        dmHashString64("texture0"),
        dmHashString64("texture1"),
        dmHashString64("texture2"),
        dmHashString64("texture3"),
        dmHashString64("texture4"),
        dmHashString64("texture5"),
        dmHashString64("texture6"),
        dmHashString64("texture7")
    };
    static const dmhash_t PROP_TEXTURES = dmHashString64("textures");
    static const dmhash_t PROP_TILE_SOURCE = dmHashString64("tile_source");

    static const dmGraphics::TextureFormat BIND_POSE_CACHE_TEXTURE_FORMAT = dmGraphics::TEXTURE_FORMAT_RGBA32F;

    struct EmitterStateChangedScriptData
    {
        EmitterStateChangedScriptData()
        {
            memset(this, 0, sizeof(*this));
        }

        dmhash_t m_ComponentId;
        dmScript::LuaCallbackInfo* m_CallbackInfo;
    };

    // A wrapper for dmScript::CheckGoInstance
    dmGameObject::HInstance CheckGoInstance(lua_State* L);

    // Logs an error for when a component buffer is full
    void ShowFullBufferError(const char* object_name, const char* config_key, int max_count);

    // Logs an error for when a component buffer is full, but the buffer in question cannot be changed by the user
    void ShowFullBufferError(const char* object_name, int max_count);

    /**
     * Log message error. The function will send a formatted printf-style string to dmLogError
     * and append message sender/receiver information on the following format:
     * Message <MESSAGE-ID> sent from <SENDER> to <RECEIVER>. For format-string should be a complete sentence including
     * a terminating period character but without trailing space.
     * @param message message
     * @param format printf-style format string
     */
    void LogMessageError(dmMessage::Message* message, const char* format, ...);

    dmRender::RenderResourceType ResourcePathToRenderResourceType(const char* path);

    // Vertex attributes
    static const uint16_t INVALID_DYNAMIC_ATTRIBUTE_INDEX  = 0xFFFF;
    static const uint8_t  DYNAMIC_ATTRIBUTE_INCREASE_COUNT = 16;

    struct DynamicAttributeInfo
    {
        struct Info
        {
            dmhash_t m_NameHash;
            float    m_Values[4]; // Enough to store a float vec4 property (no support for mat4 yet)
        };

        Info*   m_Infos;
        uint8_t m_NumInfos;
    };

    typedef dmObjectPool<DynamicAttributeInfo> DynamicAttributePool;
    typedef bool (*CompGetMaterialAttributeCallback)(void* user_data, dmhash_t name_hash, const dmGraphics::VertexAttribute** attribute);

    int32_t FindAttributeIndex(const dmGraphics::VertexAttribute* attributes, uint32_t attributes_count, dmhash_t name_hash);
    void    FillMaterialAttributeInfos(dmRender::HMaterial material, dmGraphics::HVertexDeclaration vx_decl, dmGraphics::VertexAttributeInfos* infos, dmGraphics::CoordinateSpace default_coordinate_space);
    void    FillAttributeInfos(DynamicAttributePool* dynamic_attribute_pool, uint16_t component_dynamic_attribute_index, const dmGraphics::VertexAttribute* component_attributes, uint32_t num_component_attributes, dmGraphics::VertexAttributeInfos* material_infos, dmGraphics::VertexAttributeInfos* component_infos);

    int32_t                      FindMaterialAttributeIndex(const DynamicAttributeInfo& info, dmhash_t name_hash);
    void                         GetMaterialAttributeValues(const DynamicAttributeInfo& info, uint32_t dynamic_attribute_index, uint32_t max_value_size, const uint8_t** value_ptr, uint32_t* value_size);
    void                         ConvertMaterialAttributeValuesToDataType(const DynamicAttributeInfo& info, uint32_t dynamic_attribute_index, const dmGraphics::VertexAttribute* attribute, uint8_t* value_ptr);
    void                         InitializeMaterialAttributeInfos(DynamicAttributePool& pool, uint32_t initial_capacity);
    void                         DestroyMaterialAttributeInfos(DynamicAttributePool& pool);
    void                         FreeMaterialAttribute(DynamicAttributePool& pool, uint16_t dynamic_attribute_index);
    dmGameObject::PropertyResult ClearMaterialAttribute(DynamicAttributePool& pool, uint32_t dynamic_attribute_index, dmhash_t name_hash);
    dmGameObject::PropertyResult SetMaterialAttribute(DynamicAttributePool& pool, uint16_t* dynamic_attribute_index, dmRender::HMaterial material, dmhash_t name_hash, const dmGameObject::PropertyVar& var, CompGetMaterialAttributeCallback callback, void* callback_user_data);
    dmGameObject::PropertyResult GetMaterialAttribute(DynamicAttributePool& pool, uint16_t dynamic_attribute_index, dmRender::HMaterial material, dmhash_t name_hash, dmGameObject::PropertyDesc& out_desc, CompGetMaterialAttributeCallback callback, void* callback_user_data);

    // gamesys_resource.cpp
    struct CreateTextureResourceParams
    {
        const char*                               m_Path;
        dmhash_t                                  m_PathHash;
        dmGameObject::HCollection                 m_Collection;
        dmGraphics::TextureType                   m_Type;
        dmGraphics::TextureFormat                 m_Format;
        dmGraphics::TextureImage::Type            m_TextureType;
        dmGraphics::TextureImage::TextureFormat   m_TextureFormat;
        dmGraphics::TextureImage::CompressionType m_CompressionType;
        dmBuffer::HBuffer                         m_Buffer;
        const void*                               m_Data;
        uint16_t                                  m_Width;
        uint16_t                                  m_Height;
        uint16_t                                  m_Depth;
        uint16_t                                  m_LayerCount;
        uint16_t                                  m_MaxMipMaps;
        uint16_t                                  m_TextureBpp;
        uint16_t                                  m_UsageFlags;
    };

    struct SetTextureResourceParams
    {
        dmhash_t                                  m_PathHash;
        dmGraphics::TextureType                   m_TextureType;
        dmGraphics::TextureFormat                 m_TextureFormat;
        dmGraphics::TextureImage::CompressionType m_CompressionType;
        const void*                               m_Data;
        size_t                                    m_DataSize;
        uint16_t                                  m_Width;
        uint16_t                                  m_Height;
        uint16_t                                  m_Depth;
        uint16_t                                  m_X;
        uint16_t                                  m_Y;
        uint16_t                                  m_Z;
        uint16_t                                  m_Slice;
        uint16_t                                  m_MipMap : 15;
        bool                                      m_SubUpdate : 1;
    };

    dmGraphics::TextureImage::TextureFormat GraphicsTextureFormatToImageFormat(dmGraphics::TextureFormat textureformat);
    dmGraphics::TextureImage::Type GraphicsTextureTypeToImageType(dmGraphics::TextureType texturetype);
    void MakeTextureImage(CreateTextureResourceParams params, dmGraphics::TextureImage* texture_image);
    void DestroyTextureImage(dmGraphics::TextureImage& texture_image, bool destroy_image_data);
    void FillTextureResourceBuffer(const dmGraphics::TextureImage* texture_image, dmArray<uint8_t>& texture_resource_buffer);
    dmResource::Result CreateTextureResource(dmResource::HFactory factory, const CreateTextureResourceParams& create_params, void** resource_out);
    dmResource::Result SetTextureResource(dmResource::HFactory factory, const SetTextureResourceParams& params);
    dmResource::Result ReleaseDynamicResource(dmResource::HFactory factory, dmGameObject::HCollection collection, dmhash_t path_hash);
}

#endif // DM_GAMESYS_PRIVER_H
