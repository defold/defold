#include "res_mesh.h"

#include <render/render.h>
#include <render/mesh_ddf.h>
#include <render/model/model.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateMesh(dmResource::HFactory factory,
                                     void* context,
                                     const void* buffer, uint32_t buffer_size,
                                     dmResource::SResourceDescriptor* resource,
                                     const char* filename)
    {
        dmRenderDDF::MeshDesc* mesh_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRenderDDF_MeshDesc_DESCRIPTOR, (void**) &mesh_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmModel::HMesh mesh = dmModel::NewMesh(mesh_desc);
        resource->m_Resource = (void*) mesh;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyMesh(dmResource::HFactory factory,
                                      void* context,
                                      dmResource::SResourceDescriptor* resource)
    {
        dmModel::HMesh mesh = (dmModel::HMesh)resource->m_Resource;
        dmModel::DeleteMesh(mesh);

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
