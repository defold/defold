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

    dmResource::Result ResCreateMesh(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     void *preload_data,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename);

    dmResource::Result ResDestroyMesh(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource);

    dmResource::Result ResRecreateMesh(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename);
}

#endif // DM_GAMESYS_RES_MESH_H
