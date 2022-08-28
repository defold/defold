// Copyright 2020-2022 The Defold Foundation
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

#include "res_model.h"
#include "res_meshset.h"

#include <dlib/log.h>
#include <dlib/path.h>
#include <dlib/dstrings.h>
#include <dlib/memory.h>
#include <dmsdk/dlib/transform.h>
#include <rig/rig.h>
#include <algorithm> // std::sort


namespace dmGameSystem
{
    struct MeshSortPred
    {
        inline bool operator() (const MeshInfo& p1, const MeshInfo& p2)
        {
            return p1.m_Mesh->m_MaterialIndex < p2.m_Mesh->m_MaterialIndex;
        }
    };

    // Flattens the meshes into a list, sorted on material
    static void FlattenMeshes(ModelResource* resource, dmRigDDF::MeshSet* mesh_set)
    {
        for (uint32_t i = 0; i < mesh_set->m_Models.m_Count; ++i)
        {
            dmRigDDF::Model* model = &mesh_set->m_Models[i];

            if (resource->m_Meshes.Remaining() < model->m_Meshes.m_Count)
            {
                resource->m_Meshes.OffsetCapacity(model->m_Meshes.m_Count - resource->m_Meshes.Remaining());
            }

            for (uint32_t j = 0; j < model->m_Meshes.m_Count; ++j)
            {
                dmRigDDF::Mesh* mesh = &model->m_Meshes[j];

                MeshInfo info;
                info.m_Model = model;
                info.m_Mesh = mesh;
                info.m_Buffers = 0;

                resource->m_Meshes.Push(info);
            }
        }

        // Sort the meshes by material
        std::sort(resource->m_Meshes.Begin(), resource->m_Meshes.End(), MeshSortPred());
    }

    static dmRig::RigModelVertex* CreateVertexData(const dmRigDDF::Mesh* mesh, dmRig::RigModelVertex* out_write_ptr)
    {
        uint32_t vertex_count = mesh->m_Positions.m_Count / 3;

        const float* positions = mesh->m_Positions.m_Count ? mesh->m_Positions.m_Data : 0;
        const float* normals = mesh->m_Normals.m_Count ? mesh->m_Normals.m_Data : 0;
        const float* tangents = mesh->m_Tangents.m_Count ? mesh->m_Tangents.m_Data : 0;
        const float* colors = mesh->m_Colors.m_Count ? mesh->m_Colors.m_Data : 0;
        const float* uv0 = mesh->m_Texcoord0.m_Count ? mesh->m_Texcoord0.m_Data : 0;
        const float* uv1 = mesh->m_Texcoord1.m_Count ? mesh->m_Texcoord1.m_Data : 0;

        for (uint32_t i = 0; i < vertex_count; ++i)
        {
            for (int c = 0; c < 3; ++c)
            {
                out_write_ptr->pos[c] = *positions++;
                out_write_ptr->normal[c] = normals ? *normals++ : 0.0f;
                out_write_ptr->tangent[c] = tangents ? *tangents++ : 0.0f;
            }

            for (int c = 0; c < 4; ++c)
            {
                out_write_ptr->color[c] = colors ? *colors++ : 1.0f;
            }

            for (int c = 0; c < 2; ++c)
            {
                out_write_ptr->uv0[c] = uv0 ? *uv0++ : 0.0f;
                out_write_ptr->uv1[c] = uv1 ? *uv1++ : 0.0f;
            }

            out_write_ptr++;
        }

        return out_write_ptr;
    }

    static ModelResourceBuffers* CreateBuffers(dmGraphics::HContext context, const dmRigDDF::Mesh* ddf_mesh, dmArray<dmRig::RigModelVertex>& scratch_buffer)
    {
        ModelResourceBuffers* buffers = new ModelResourceBuffers;
        memset(buffers, 0, sizeof(ModelResourceBuffers));

        uint32_t num_vertices = ddf_mesh->m_Positions.m_Count / 3;

        bool support_32bit_indices = dmGraphics::IsIndexBufferFormatSupported(context, dmGraphics::INDEXBUFFER_FORMAT_32);

        dmGraphics::Type index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;
        void* index_buffer = ddf_mesh->m_Indices.m_Data;
        uint32_t num_indices = ddf_mesh->m_Indices.m_Count / 2;
        if (ddf_mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
        {
            num_indices = ddf_mesh->m_Indices.m_Count / 4;
            index_element_type = dmGraphics::TYPE_UNSIGNED_INT;

            // We've moved the runtime check to the build step instead
            if (!support_32bit_indices)
            {
                dmLogError("The platform doesn't support 32 bit index buffers. See the setting 'model.split_large_meshes'");
                return buffers;
            }
        }

        if (scratch_buffer.Capacity() < num_vertices)
            scratch_buffer.SetCapacity(num_vertices);
        scratch_buffer.SetSize(num_vertices);

        CreateVertexData(ddf_mesh, scratch_buffer.Begin());

        buffers->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, num_vertices * sizeof(dmRig::RigModelVertex), scratch_buffer.Begin(), dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        buffers->m_VertexCount = num_vertices;

        buffers->m_IndexBuffer = 0;
        buffers->m_IndexCount = 0;
        if (index_buffer != 0)
        {
            uint32_t index_type_size = dmGraphics::TYPE_UNSIGNED_INT == index_element_type ? 4 : 2;
            buffers->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, num_indices * index_type_size, index_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            buffers->m_IndexBufferElementType = index_element_type;
            buffers->m_IndexCount = num_indices;
        }

        return buffers;
    }

    static void CreateBuffers(dmGraphics::HContext context, ModelResource* resource)
    {
        dmArray<dmRig::RigModelVertex> scratch_buffer;
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            info.m_Buffers = CreateBuffers(context, info.m_Mesh, scratch_buffer);
        }
    }

    static bool AreAllMaterialsWorldSpace(const ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_Materials.Size(); ++i)
        {
            dmRender::HMaterial material = resource->m_Materials[i];
            if (dmRender::GetMaterialVertexSpace(material) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
                return false;
        }
        return true; // All materials are currently world space
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_RigScene, (void**) &resource->m_RigScene);
        if (result != dmResource::RESULT_OK)
            return result;

        dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        FlattenMeshes(resource, mesh_set);
        CreateBuffers(context, resource);

        resource->m_Materials.SetCapacity(mesh_set->m_Materials.m_Count);
        for (uint32_t i = 0; i < mesh_set->m_Materials.m_Count; ++i)
        {
            // TODO: Map the model materials to the mesh materials!
            dmRender::HMaterial material;
            result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &material);
            if (result != dmResource::RESULT_OK)
            {
                return result;
            }

            // TODO: Get the proper materials
            // Currently the Model only has support for one material
            resource->m_Materials.Push(material);
        }

        if (resource->m_Materials.Empty())
        {
            dmRender::HMaterial material;
            result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &material);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Failed to load material: %s\n", resource->m_Model->m_Material);
                return dmResource::RESULT_INVALID_DATA;
            }

            if (resource->m_Materials.Full())
                resource->m_Materials.OffsetCapacity(1);
            resource->m_Materials.Push(material);
        }

        dmGraphics::HTexture textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        memset(textures, 0, dmRender::RenderObject::MAX_TEXTURE_COUNT * sizeof(dmGraphics::HTexture));
        for (uint32_t i = 0; i < resource->m_Model->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            const char* texture_path = resource->m_Model->m_Textures[i];
            if (*texture_path != 0)
            {
                dmResource::Result r = dmResource::Get(factory, texture_path, (void**) &textures[i]);
                if (r != dmResource::RESULT_OK)
                {
                    if (result == dmResource::RESULT_OK) {
                        result = r;
                    }
                } else {
                    r = dmResource::GetPath(factory, textures[i], &resource->m_TexturePaths[i]);
                    if (r != dmResource::RESULT_OK) {
                       result = r;
                    }
                }
            }
        }
        if (result != dmResource::RESULT_OK)
        {
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                if (textures[i]) dmResource::Release(factory, (void*) textures[i]);
            return result;
        }
        memcpy(resource->m_Textures, textures, sizeof(dmGraphics::HTexture) * dmRender::RenderObject::MAX_TEXTURE_COUNT);

        if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
        {
            if (!AreAllMaterialsWorldSpace(resource))
            {
                dmLogError("Failed to create Model component. Material vertex space option VERTEX_SPACE_LOCAL does not support skinning.");
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }

        return result;
    }

    static void ReleaseBuffers(ModelResourceBuffers* buffers)
    {
        dmGraphics::DeleteVertexBuffer(buffers->m_VertexBuffer);
        dmGraphics::DeleteIndexBuffer(buffers->m_IndexBuffer);
        delete buffers;
    }

    static void ReleaseResources(dmResource::HFactory factory, ModelResource* resource)
    {
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            ReleaseBuffers(info.m_Buffers);
        }
        resource->m_Meshes.SetSize(0);

        if (resource->m_Model != 0x0)
            dmDDF::FreeMessage(resource->m_Model);
        resource->m_Model = 0x0;
        if (resource->m_RigScene != 0x0)
            dmResource::Release(factory, resource->m_RigScene);
        resource->m_RigScene = 0x0;

        for (uint32_t i = 0; i < resource->m_Materials.Size(); ++i)
        {
            dmResource::Release(factory, resource->m_Materials[i]);
        }
        resource->m_Materials.SetSize(0);

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (resource->m_Textures[i])
                dmResource::Release(factory, (void*)resource->m_Textures[i]);
            resource->m_Textures[i] = 0x0;
        }
    }

    dmResource::Result ResModelPreload(const dmResource::ResourcePreloadParams& params)
    {
        dmModelDDF::Model* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_Model_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);
        for (uint32_t i = 0; i < ddf->m_Textures.m_Count && i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            dmResource::PreloadHint(params.m_HintInfo, ddf->m_Textures[i]);
        }

        dmResource::PreloadHint(params.m_HintInfo, ddf->m_RigScene);
        dmResource::PreloadHint(params.m_HintInfo, ddf->m_Material);

        *params.m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelCreate(const dmResource::ResourceCreateParams& params)
    {
        ModelResource* model_resource = new ModelResource();
        memset(model_resource, 0, sizeof(ModelResource));
        model_resource->m_Model = (dmModelDDF::Model*) params.m_PreloadData;
        dmResource::Result r = AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, model_resource, params.m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            params.m_Resource->m_Resource = (void*) model_resource;
        }
        else
        {
            ReleaseResources(params.m_Factory, model_resource);
            delete model_resource;
        }
        return r;
    }

    dmResource::Result ResModelDestroy(const dmResource::ResourceDestroyParams& params)
    {
        ModelResource* model_resource = (ModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, model_resource);
        delete model_resource;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResModelRecreate(const dmResource::ResourceRecreateParams& params)
    {
        dmModelDDF::Model* ddf;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmModelDDF_Model_DESCRIPTOR, (void**) &ddf);
        if (e != dmDDF::RESULT_OK)
        {
            return dmResource::RESULT_DDF_ERROR;
        }
        ModelResource* model_resource = (ModelResource*)params.m_Resource->m_Resource;
        ReleaseResources(params.m_Factory, model_resource);
        model_resource->m_Model = ddf;
        return AcquireResources((dmGraphics::HContext) params.m_Context, params.m_Factory, model_resource, params.m_Filename);
    }
}
