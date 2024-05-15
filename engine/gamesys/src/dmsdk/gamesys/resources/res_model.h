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

#ifndef DMSDK_GAMESYS_RES_MODEL_H
#define DMSDK_GAMESYS_RES_MODEL_H

#include <stdint.h>

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/render/render.h>

namespace dmRigDDF
{
    struct RigScene;
    struct Mesh;
    struct Model;
}

namespace dmModelDDF
{
    struct Model;
}

namespace dmGameSystem
{
    struct MaterialResource;
    struct RigSceneResource;
    struct TextureResource;
    struct RenderTargetResource;

    struct ModelResourceBuffers
    {
        dmGraphics::HVertexBuffer   m_VertexBuffer;
        dmGraphics::HIndexBuffer    m_IndexBuffer;
        uint32_t                    m_VertexCount;
        uint32_t                    m_IndexCount;
        dmGraphics::Type            m_IndexBufferElementType;
    };

    struct MeshInfo
    {
        ModelResourceBuffers*   m_Buffers; // Currently a vertex+index buffer per mesh
        dmRigDDF::Model*        m_Model;   // For the transform
        dmRigDDF::Mesh*         m_Mesh;
    };

    struct MaterialTextureInfo
    {
        dmhash_t                m_SamplerNameHash;
        TextureResource*        m_Texture;
        RenderTargetResource*   m_RenderTarget;
    };

    struct MaterialInfo
    {
        const char*                  m_Name;
        MaterialResource*            m_Material;
        dmGraphics::VertexAttribute* m_Attributes;
        uint32_t                     m_AttributeCount;
        MaterialTextureInfo*         m_Textures;
        uint32_t                     m_TexturesCount;
    };

    struct ModelResource
    {
        dmModelDDF::Model*       m_Model;
        uint32_t                 m_ModelsCount;
        RigSceneResource*        m_RigScene;

        dmArray<MeshInfo>        m_Meshes;
        dmArray<MaterialInfo>    m_Materials;    // List matches the list of material names in the dmRigDDF::Model
    };
}

#endif // DMSDK_GAMESYS_RES_MODEL_H
