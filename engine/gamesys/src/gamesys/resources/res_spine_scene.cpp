#include "res_spine_scene.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, const void* buffer, uint32_t buffer_size, SpineSceneResource* resource, const char* filename)
    {
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameSystemDDF_SpineScene_DESCRIPTOR, (void**) &resource->m_SpineScene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        dmResource::Result result = dmResource::Get(factory, resource->m_SpineScene->m_TextureSet, (void**) &resource->m_TextureSet);
        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, SpineSceneResource* resource)
    {
        if (resource->m_SpineScene != 0x0)
            dmDDF::FreeMessage(resource->m_SpineScene);
        if (resource->m_TextureSet != 0x0)
            dmResource::Release(factory, resource->m_TextureSet);
    }

    dmResource::Result ResSpineSceneCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpineSceneResource* ss_resource = new SpineSceneResource();
        dmResource::Result r = AcquireResources(factory, buffer, buffer_size, ss_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) ss_resource;
        }
        else
        {
            ReleaseResources(factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    dmResource::Result ResSpineSceneDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        SpineSceneResource* ss_resource = (SpineSceneResource*)resource->m_Resource;
        ReleaseResources(factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpineSceneRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpineSceneResource* ss_resource = (SpineSceneResource*)resource->m_Resource;
        ReleaseResources(factory, ss_resource);
        return AcquireResources(factory, buffer, buffer_size, ss_resource, filename);
    }
}
