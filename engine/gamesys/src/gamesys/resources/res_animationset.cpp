#include "res_animationset.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    dmResource::Result AcquireResources(dmResource::HFactory factory, AnimationSetResource* resource, const char* filename)
    {
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, AnimationSetResource* resource)
    {
        if (resource->m_AnimationSet != 0x0)
            dmDDF::FreeMessage(resource->m_AnimationSet);
    }

    dmResource::Result ResAnimationSetPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRigDDF::AnimationSet* AnimationSet;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_AnimationSet_DESCRIPTOR, (void**) &AnimationSet);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params.m_PreloadData = AnimationSet;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResAnimationSetCreate(const dmResource::ResourceCreateParams& params)
    {
        AnimationSetResource* ss_resource = new AnimationSetResource();
        ss_resource->m_AnimationSet = (dmRigDDF::AnimationSet*) params.m_PreloadData;
        dmResource::Result r = AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) ss_resource;
            params.m_Resource->m_ResourceSize = sizeof(AnimationSetResource) + params.m_BufferSize;
        }
        else
        {
            ReleaseResources(params.m_Factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    dmResource::Result ResAnimationSetDestroy(const dmResource::ResourceDestroyParams& params)
    {
        AnimationSetResource* ss_resource = (AnimationSetResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResAnimationSetRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRigDDF::AnimationSet* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_AnimationSet_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        AnimationSetResource* ss_resource = (AnimationSetResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        ss_resource->m_AnimationSet = spine_scene;
        params.m_Resource->m_ResourceSize = sizeof(AnimationSetResource) + params.m_BufferSize;
        return AcquireResources(params.m_Factory, ss_resource, params.m_Filename);
    }
}
