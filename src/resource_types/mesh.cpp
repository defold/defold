#include "mesh.h"

#include <render/mesh_ddf.h>

namespace dmMesh
{
    dmResource::CreateResult CreateResource(dmResource::HFactory factory,
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

    dmResource::CreateResult DestroyResource(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmDDF::FreeMessage((void*) resource->m_Resource);
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult RecreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
