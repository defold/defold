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

#include "res_meshset.h"

#include <dlib/log.h>
#include <dmsdk/dlib/vmath.h>

namespace dmGameSystem
{
    using namespace dmVMath;

    dmResource::Result AcquireResources(dmResource::HFactory factory, MeshSetResource* resource, const char* filename)
    {
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshSetResource* resource)
    {
        if (resource->m_MeshSet != 0x0)
            dmDDF::FreeMessage(resource->m_MeshSet);
    }

    dmResource::Result ResMeshSetPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRigDDF::MeshSet* MeshSet;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRigDDF_MeshSet_DESCRIPTOR, (void**) &MeshSet);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        *params->m_PreloadData = MeshSet;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshSetCreate(const dmResource::ResourceCreateParams* params)
    {
        MeshSetResource* ss_resource = new MeshSetResource();
        ss_resource->m_MeshSet = (dmRigDDF::MeshSet*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(params->m_Factory, ss_resource, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, ss_resource);
        }
        else
        {
            ReleaseResources(params->m_Factory, ss_resource);
            delete ss_resource;
        }
        return r;
    }

    dmResource::Result ResMeshSetDestroy(const dmResource::ResourceDestroyParams* params)
    {
        MeshSetResource* ss_resource = (MeshSetResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, ss_resource);
        delete ss_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResMeshSetRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRigDDF::MeshSet* spine_scene;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRigDDF_MeshSet_DESCRIPTOR, (void**) &spine_scene);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        MeshSetResource* ss_resource = (MeshSetResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, ss_resource);
        ss_resource->m_MeshSet = spine_scene;
        return AcquireResources(params->m_Factory, ss_resource, params->m_Filename);
    }
}
