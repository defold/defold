// Copyright 2020 The Defold Foundation
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

#include "res_skeleton.h"

#include <dlib/log.h>

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    static void ReleaseResources(dmResource::HFactory factory, SkeletonResource* resource)
    {
        if (resource->m_Skeleton != 0x0)
            dmDDF::FreeMessage(resource->m_Skeleton);
    }

    dmResource::Result ResSkeletonPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmRigDDF::Skeleton* skeleton;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_Skeleton_DESCRIPTOR, (void**) &skeleton);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params.m_PreloadData = skeleton;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSkeletonCreate(const dmResource::ResourceCreateParams& params)
    {
        SkeletonResource* ss_resource = new SkeletonResource();
        ss_resource->m_Skeleton = (dmRigDDF::Skeleton*) params.m_PreloadData;
        params.m_Resource->m_Resource = (void*) ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSkeletonDestroy(const dmResource::ResourceDestroyParams& params)
    {
        SkeletonResource* ss_resource = (SkeletonResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResSkeletonRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmRigDDF::Skeleton* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmRigDDF_Skeleton_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        SkeletonResource* ss_resource = (SkeletonResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, ss_resource);
        ss_resource->m_Skeleton = spine_scene;
        return dmResource::RESULT_OK;
    }
}
