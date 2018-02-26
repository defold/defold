#include "res_meshset.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmResource::HFactory factory, MeshSetResource* resource, const char* filename)
    {
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshSetResource* resource)
    {
        if (resource->m_MeshSet != 0x0)
            dmDDF::FreeMessage(resource->m_MeshSet);
    }

    dmResource::Result ResMeshSetPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRigDDF::MeshSet* MeshSet;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_MeshSet_DESCRIPTOR, (void**) &MeshSet);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params.m_PreloadData = MeshSet;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshSetCreate(const dmResource::ResourceCreateParams& params)
    {
        MeshSetResource* ss_resource = new MeshSetResource();
        ss_resource->m_MeshSet = (dmRigDDF::MeshSet*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            ss_resource->m_DDFSize = params.m_BufferSize;
            params.m_Resource->m_Resource = (void*) ss_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    dmResource::Result ResMeshSetDestroy(const dmResource::ResourceDestroyParams& params)
    {
        MeshSetResource* ss_resource = (MeshSetResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshSetRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRigDDF::MeshSet* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_MeshSet_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        MeshSetResource* ss_resource = (MeshSetResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        ss_resource->m_MeshSet = spine_scene;
        ss_resource->m_DDFSize = params.m_BufferSize;
        return AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
    }

    dmResource::Result ResMeshSetGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        MeshSetResource* res = (MeshSetResource*)params.m_Resource->m_Resource;
        params.m_DataSize = sizeof(MeshSetResource) + res->m_DDFSize;
        return dmResource::RESULT_OK;
    }

}
