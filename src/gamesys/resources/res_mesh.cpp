#include "res_mesh.h"

#include <render/mesh_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateMesh(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmRender::MeshDesc* mesh;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRender_MeshDesc_DESCRIPTOR, (void**) &mesh);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }
        resource->m_Resource = (void*) mesh;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyMesh(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmDDF::FreeMessage((void*) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateMesh(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
