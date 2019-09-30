#ifndef DM_GAMESYS_RES_MESH_H
#define DM_GAMESYS_RES_MESH_H

#include <stdint.h>

#include <resource/resource.h>
#include <render/render.h>
#include "mesh_ddf.h"

#include "res_buffer.h"

namespace dmGameSystem
{
    struct MeshResource
    {
        MeshResource() {
            // nop?
        }

        dmMeshDDF::MeshDesc*    m_MeshDDF;
        BufferResource*         m_BufferResource;
        dmRender::HMaterial     m_Material;

        dmGraphics::HTexture    m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t                m_TexturePaths[dmRender::RenderObject::MAX_TEXTURE_COUNT];

        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        uint32_t                        m_ElementCount;
        uint32_t                        m_VertSize;

        dmGraphics::PrimitiveType       m_PrimitiveType;

        /// Stread id and type to know what data to modify when rendering in world space.
        dmhash_t                        m_PositionStreamId;
        dmBufferDDF::ValueType          m_PositionStreamType;
        dmhash_t                        m_NormalStreamId;
        dmBufferDDF::ValueType          m_NormalStreamType;

        /// Needed to keep track of data change on resource reload.
        uint32_t                        m_BufferVersion;
    };

    dmResource::Result ResMeshPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResMeshDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResMeshRecreate(const dmResource::ResourceRecreateParams& params);

    bool BuildVertexDeclaration(BufferResource* buffer_resource,
        dmGraphics::HVertexDeclaration* out_vert_decl,
        uint32_t* out_elem_count, uint32_t* out_vert_size);
}

#endif // DM_GAMESYS_RES_MESH_H
