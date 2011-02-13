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

    dmResource::CreateResult ResCreateMesh(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename);

    dmResource::CreateResult ResDestroyMesh(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource);

    dmResource::CreateResult ResRecreateMesh(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_MESH_H
