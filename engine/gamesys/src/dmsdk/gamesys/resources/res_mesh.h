// Copyright 2020-2022 The Defold Foundation
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

#ifndef DMSDK_GAMESYS_RES_MESH_H
#define DMSDK_GAMESYS_RES_MESH_H

#include <stdint.h>
#include <dmsdk/render/render.h>
#include <dmsdk/gamesys/resources/res_buffer.h>
#include <gamesys/buffer_ddf.h>
#include <gamesys/mesh_ddf.h>

namespace dmGameSystem
{
    struct MeshResource
    {
        dmMeshDDF::MeshDesc*    m_MeshDDF;
        BufferResource*         m_BufferResource;
        dmRender::HMaterial     m_Material;

        dmGraphics::HTexture    m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t                m_TexturePaths[dmRender::RenderObject::MAX_TEXTURE_COUNT];

        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;

        dmGraphics::PrimitiveType       m_PrimitiveType;

        /// Stream id and type to know what data to modify when rendering in world space.
        dmhash_t                        m_PositionStreamId;
        dmBufferDDF::ValueType          m_PositionStreamType;
        dmhash_t                        m_NormalStreamId;
        dmBufferDDF::ValueType          m_NormalStreamType;

        /// Needed to keep track of data change on resource reload.
        uint32_t                        m_BufferVersion;
    };
}

#endif // DMSDK_GAMESYS_RES_MESH_H
