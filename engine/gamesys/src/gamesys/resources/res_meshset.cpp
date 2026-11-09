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

    static dmResource::Result AcquireResources(dmResource::HFactory factory, MeshSetResource* resource, const char* filename)
    {
        // Keep hinted morph textures alive for the lifetime of the meshset.
        for (uint32_t mi = 0; mi < resource->m_MeshSet->m_Models.m_Count; ++mi)
        {
            const dmRigDDF::Model& model = resource->m_MeshSet->m_Models[mi];
            for (uint32_t mesh_i = 0; mesh_i < model.m_Meshes.m_Count; ++mesh_i)
            {
                const dmRigDDF::Mesh& mesh = model.m_Meshes[mesh_i];
                if (mesh.m_MorphTargetTexture && mesh.m_MorphTargetTexture[0])
                {
                    TextureResource* texture;
                    dmResource::Result result = dmResource::Get(factory, mesh.m_MorphTargetTexture, (void**)&texture);
                    if (result != dmResource::RESULT_OK)
                    {
                        return result;
                    }
                    if (resource->m_MorphTargetTextures.Full())
                    {
                        resource->m_MorphTargetTextures.OffsetCapacity(4);
                    }
                    resource->m_MorphTargetTextures.Push(texture);
                }
            }
        }
        return dmResource::RESULT_OK;
    }

    static void ReleaseResources(dmResource::HFactory factory, MeshSetResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_MorphTargetTextures.Size(); ++i)
        {
            dmResource::Release(factory, resource->m_MorphTargetTextures[i]);
        }
        resource->m_MorphTargetTextures.SetSize(0);
        if (resource->m_MeshSet != 0x0)
        {
            dmDDF::FreeMessage(resource->m_MeshSet);
        }
        resource->m_MeshSet = 0x0;
    }

    dmResource::Result ResMeshSetPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRigDDF::MeshSet* MeshSet;
        dmDDF::Result e = dmDDF::LoadMessage(params->m_Buffer, params->m_BufferSize, &dmRigDDF_MeshSet_DESCRIPTOR, (void**) &MeshSet);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        for (uint32_t mi = 0; mi < MeshSet->m_Models.m_Count; ++mi)
        {
            const dmRigDDF::Model* model = &MeshSet->m_Models[mi];
            for (uint32_t mesh_i = 0; mesh_i < model->m_Meshes.m_Count; ++mesh_i)
            {
                const dmRigDDF::Mesh* mesh = &model->m_Meshes[mesh_i];
                if (mesh->m_MorphTargetTexture && mesh->m_MorphTargetTexture[0])
                {
                    dmResource::PreloadHint(params->m_HintInfo, mesh->m_MorphTargetTexture);
                }
            }
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

        MeshSetResource replacement;
        replacement.m_MeshSet = spine_scene;
        dmResource::Result result = AcquireResources(params->m_Factory, &replacement, params->m_Filename);
        if (result != dmResource::RESULT_OK)
        {
            ReleaseResources(params->m_Factory, &replacement);
            return result;
        }

        MeshSetResource* ss_resource = (MeshSetResource*)dmResource::GetResource(params->m_Resource);
        ReleaseResources(params->m_Factory, ss_resource);
        ss_resource->m_MeshSet = spine_scene;
        ss_resource->m_MorphTargetTextures.Swap(replacement.m_MorphTargetTextures);
        return dmResource::RESULT_OK;
    }
}
