#include "res_spine_model.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, SpineModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_SpineScene, (void**) &resource->m_Scene);
        if (result != dmResource::RESULT_OK)
            return result;
        result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &resource->m_Material);
        return result;
    }

    static void ReleaseResources(dmResource::HFactory factory, SpineModelResource* resource)
    {
        if (resource->m_Model != 0x0)
            dmDDF::FreeMessage(resource->m_Model);
        if (resource->m_Scene != 0x0)
            dmResource::Release(factory, resource->m_Scene);
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
    }

    dmResource::Result ResSpineModelPreload(dmResource::HFactory factory, dmResource::HPreloadHintInfo hint_info,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void** preload_data,
            const char* filename)
    {
        dmGameSystemDDF::SpineModelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameSystemDDF_SpineModelDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(hint_info, ddf->m_SpineScene);
        dmResource::PreloadHint(hint_info, ddf->m_Material);

        *preload_data = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpineModelCreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            void* preload_data,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        SpineModelResource* model_resource = new SpineModelResource();
        model_resource->m_Model = (dmGameSystemDDF::SpineModelDesc*) preload_data;
        dmResource::Result r = AcquireResources(factory, model_resource, filename);
        if (r == dmResource::RESULT_OK)
        {
            resource->m_Resource = (void*) model_resource;
        }
        else
        {
            ReleaseResources(factory, model_resource);
            delete model_resource;
        }
        return r;
    }

    dmResource::Result ResSpineModelDestroy(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        SpineModelResource* model_resource = (SpineModelResource*)resource->m_Resource;
        ReleaseResources(factory, model_resource);
        delete model_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSpineModelRecreate(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {

        dmGameSystemDDF::SpineModelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmGameSystemDDF_SpineModelDesc_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        SpineModelResource* model_resource = (SpineModelResource*)resource->m_Resource;
        ReleaseResources(factory, model_resource);
        model_resource->m_Model = ddf;
        return AcquireResources(factory, model_resource, filename);
    }
}
