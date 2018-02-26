#include "res_label.h"

#include <string.h>

#include <dlib/log.h>

#include "label_ddf.h"

namespace dmGameSystem
{
    dmResource::Result AcquireResources(dmResource::HFactory factory, LabelResource* resource, const char* filename)
    {
        dmResource::Result result;
        result = dmResource::Get(factory, resource->m_DDF->m_Material, (void**)&resource->m_Material);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }
        result = dmResource::Get(factory, resource->m_DDF->m_Font, (void**) &resource->m_FontMap);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        return dmResource::RESULT_OK;
    }

    void ReleaseResources(dmResource::HFactory factory, LabelResource* resource)
    {
        if (resource->m_DDF != 0x0)
            dmDDF::FreeMessage(resource->m_DDF);
        if (resource->m_Material != 0x0)
            dmResource::Release(factory, resource->m_Material);
        if (resource->m_FontMap != 0x0)
            dmResource::Release(factory, resource->m_FontMap);
    }

    dmResource::Result ResLabelPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmGameSystemDDF::LabelDesc* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Font);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLabelCreate(const dmResource::ResourceCreateParams& params)
    {
        LabelResource* resource = new LabelResource();
        memset(resource, 0, sizeof(LabelResource));
        resource->m_DDF = (dmGameSystemDDF::LabelDesc*) params.m_PreloadData;
        resource->m_DDFSize = params.m_BufferSize;

        dmResource::Result r = AcquireResources(params.m_Factory, resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, resource);
            delete resource;
        }
        return r;
    }

    dmResource::Result ResLabelDestroy(const dmResource::ResourceDestroyParams& params)
    {
        LabelResource* resource = (LabelResource*) params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, resource);
        delete resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResLabelRecreate(const dmResource::ResourceRecreateParams& params)
    {
        LabelResource tmp_resource;
        memset(&tmp_resource, 0, sizeof(LabelResource));
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &tmp_resource.m_DDF);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(params.m_Factory, &tmp_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            LabelResource* resource = (LabelResource*)params.m_Resource->m_Resource;
            ReleaseResources(params.m_Factory, resource);
            *resource = tmp_resource;
            resource->m_DDFSize = params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, &tmp_resource);
        }
        return r;
    }

    dmResource::Result ResLabelGetInfo(dmResource::ResourceGetInfoParams& params)
    {
        LabelResource* res  = (LabelResource*) params.m_Resource->m_Resource;
        params.m_DataSize = sizeof(LabelResource) + res->m_DDFSize;
        params.m_SubResourceIds->SetCapacity(2);
        dmhash_t res_hash;
        if(dmResource::GetPath(params.m_Factory, res->m_FontMap, &res_hash)==dmResource::RESULT_OK)
        {
            params.m_SubResourceIds->Push(res_hash);
        }
        if(dmResource::GetPath(params.m_Factory, res->m_Material, &res_hash)==dmResource::RESULT_OK)
        {
            params.m_SubResourceIds->Push(res_hash);
        }
        return dmResource::RESULT_OK;
    }

}
