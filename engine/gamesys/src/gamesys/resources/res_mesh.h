#ifndef DM_GAMESYS_RES_MESH_H
#define DM_GAMESYS_RES_MESH_H

#include <stdint.h>

#include <resource/resource.h>

#include <graphics/graphics.h>

namespace dmGameSystem
{
    struct Mesh
    {
        dmGraphics::HVertexBuffer m_VertexBuffer;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        uint32_t m_VertexCount;
    };

    dmResource::Result ResCreateMesh(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResDestroyMesh(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResRecreateMesh(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_MESH_H
