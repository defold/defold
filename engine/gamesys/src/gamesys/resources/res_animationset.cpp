// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "res_animationset.h"

#include <dmsdk/resource/resource.hpp>
#include <resource/resource.h>
#include <dlib/log.h>
#include <dmsdk/dlib/vmath.h>

namespace dmGameSystem
{
    using namespace dmVMath;

    static dmResource::Result AcquireResources(dmResource::HFactory factory, AnimationSetResource* resource, const char* filename)
    {
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, AnimationSetResource* resource)
    {
        if (resource->m_AnimationSet != 0x0)
            dmDDF::FreeMessage(resource->m_AnimationSet);
    }

    static dmResource::Result ResAnimationSetPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRigDDF::AnimationSet* AnimationSet;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRigDDF_AnimationSet_DESCRIPTOR, (void**) &AnimationSet);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params->m_PreloadData = AnimationSet;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResAnimationSetCreate(const dmResource::ResourceCreateParams* params)
    {
        AnimationSetResource* ss_resource = new AnimationSetResource();
        ss_resource->m_AnimationSet = (dmRigDDF::AnimationSet*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(params->m_Factory, ss_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, (void*) ss_resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    static dmResource::Result ResAnimationSetDestroy(const dmResource::ResourceDestroyParams* params)
    {
        AnimationSetResource* ss_resource = (AnimationSetResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResAnimationSetRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRigDDF::AnimationSet* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRigDDF_AnimationSet_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        AnimationSetResource* ss_resource = (AnimationSetResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, ss_resource);
        ss_resource->m_AnimationSet = spine_scene;
        return AcquireResources(params->m_Factory, ss_resource, params->m_Filename);
    }

    static ResourceResult RegisterResourceTypeAnimationSet(HResourceTypeContext ctx, HResourceType type)
    {
        return (ResourceResult)dmResource::SetupType(ctx,
                                           type,
                                           0,
                                           ResAnimationSetPreload,
                                           ResAnimationSetCreate,
                                           0,
                                           ResAnimationSetDestroy,
                                           ResAnimationSetRecreate);
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeAnimationSet, "animationsetc", dmGameSystem::RegisterResourceTypeAnimationSet, 0);
