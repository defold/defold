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
    static inline void GetModelVertex(const dmRigDDF::Mesh& mesh, const dmRigDDF::MeshVertexIndices *in, dmRig::RigModelVertex* out)
    {
        const float* v = &mesh.m_Positions[in->m_Position*3];
        out->x = v[0];
        out->y = v[1];
        out->z = v[2];
        v = &mesh.m_Texcoord0[in->m_Texcoord0*2];
        out->u = v[0];
        out->v = v[1];
        v = &mesh.m_Normals[in->m_Normal*3];
        out->nx = v[0];
        out->ny = v[1];
        out->nz = v[2];
    }

    // struct ModelMeshsortPred
    // {
    //     inline bool operator() (const ModelMesh& p1, const ModelMesh& p2)
    //     {
    //         if ((uintptr_t)p1.m_Model < (uintptr_t)p2.m_Model)
    //             return -1;
    //         if ((uintptr_t)p1.m_Model > (uintptr_t)p2.m_Model)
    //             return 1;
    //         return p1.m_Mesh->m_MaterialIndex < p2.m_Mesh->m_MaterialIndex;
    //     }
    // };
    struct MeshSortPred
    {
        inline bool operator() (const MeshInfo& p1, const MeshInfo& p2)
        {
            return p1.m_Mesh->m_MaterialIndex < p2.m_Mesh->m_MaterialIndex;
        }
    };


    // Flattens the meshes into a sorted list (based on material)
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

                // ModelMesh m;
                // m.m_Model = model;
                // m.m_Mesh  = mesh;
                // m.m_VertexStart = 0;
                // m.m_VertexCount = mesh->m_Vertices.m_Count;
                // m.m_IndexStart = 0;

                // if (mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
                //     m.m_IndexCount = mesh->m_Indices.m_Count / 4;
                // else
                //     m.m_IndexCount = mesh->m_Indices.m_Count / 2;

                MeshInfo info;
                info.m_Model = model;
                info.m_Mesh = mesh;
                info.m_Buffers = 0;

                resource->m_Meshes.Push(info);
            }
        }

        // Sort the meshes by material
        std::sort(resource->m_Meshes.Begin(), resource->m_Meshes.End(), MeshSortPred());

        printf("\n");
        for (int i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            printf("ModelInfo: %u: model id %llu,  mesh: %p  material: %u\n", i, info.m_Model->m_Id, info.m_Mesh, info.m_Mesh->m_MaterialIndex);
        }
    }

    /*
    static void GatherMeshes(ModelResource* resource, dmRigDDF::MeshSet* mesh_set)
    {
        resource->m_Models.SetCapacity(mesh_set->m_Models.m_Count);

        for (uint32_t i = 0; i < mesh_set->m_Models.m_Count; ++i)
        {
            dmRigDDF::Model* model = &mesh_set->m_Models[i];

            ModelMeshGroup group;
            group.m_Model = model;

            for (uint32_t j = 0; j < model->m_Meshes.m_Count; ++j)
            {
                dmRigDDF::Mesh* mesh = &model->m_Meshes[j];

                ModelMesh m;
                m.m_Model = model;
                m.m_Mesh  = mesh;
                m.m_VertexStart = 0;
                m.m_VertexCount = mesh->m_Vertices.m_Count;
                m.m_IndexStart = 0;

                if (mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
                    m.m_IndexCount = mesh->m_Indices.m_Count / 4;
                else
                    m.m_IndexCount = mesh->m_Indices.m_Count / 2;

                if (resource->m_Meshes.Full())
                {
                    resource->m_Meshes.OffsetCapacity(1);
                }

                resource->m_Meshes.Push(m);
            }
        }

        // Sort the meshes by material
        std::sort(resource->m_Meshes.Begin(), resource->m_Meshes.End(), ModelMeshsortPred());

        printf("\n");
        for (int i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            ModelMesh& model = resource->m_Meshes[i];
            printf("ModelMesh: %u: vstart: %u  vcount: %u  istart: %u  icount: %u\n", i, model.m_VertexStart, model.m_VertexCount, model.m_IndexStart, model.m_IndexCount);
        }
    }

    static void CollectMeshRanges(ModelResource* resource, bool support_32bit_indices)
    {
        uint32_t curr_range_start = 0;
        uint32_t curr_index_count = 0;

        if (resource->m_Meshes.Empty())
            return;

        uint32_t prev_material_index = resource->m_Meshes[0].m_Mesh->m_MaterialIndex;

        uint32_t num_meshes = resource->m_Meshes.Size();
        for (uint32_t i = 0; i < num_meshes; ++i)
        {
            ModelMesh* mesh = &resource->m_Meshes[i];
            uint32_t material_index = mesh->m_Mesh->m_MaterialIndex;

            bool put_range = prev_material_index != material_index;

            if (!support_32bit_indices)
            {
                if (mesh->m_Mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
                {
                    // Make sure we put this in its own vertex buffer, as we cannot have a index buffer for it
                    put_range = true;
                }

                if ((curr_index_count + mesh->m_IndexCount) > 65536)
                {
                    put_range = true;
                }

                curr_index_count += mesh->m_IndexCount;
            }


            if (put_range)
            {
                if (resource->m_MeshRanges.Full())
                    resource->m_MeshRanges.OffsetCapacity(4);

                ModelMeshRange range;
                range.m_Start = curr_range_start;
                range.m_Count = i - curr_range_start;
                resource->m_MeshRanges.Push(range);

                curr_range_start = i;
                curr_index_count = 0;
            }

            prev_material_index = material_index;
        }

        for (int i = 0; i < resource->m_MeshRanges.Size(); ++i)
        {
            ModelMeshRange& range = resource->m_MeshRanges[i];
            printf("MeshRange: %u: start: %u  count: %u\n", i, range.m_Start, range.m_Count);
        }
    }
    */

    static void WriteModelMeshVertices(const dmRigDDF::Mesh* ddf_mesh, dmRig::RigModelVertex* out)
    {
        uint32_t num_vertices = ddf_mesh->m_Vertices.m_Count;
        dmRigDDF::MeshVertexIndices* mvi =  ddf_mesh->m_Vertices.m_Data;
        for(uint32_t i = 0; i < num_vertices; ++i, ++mvi, ++out)
        {
            GetModelVertex(*ddf_mesh, mvi, out);
        }
    }

    static void WriteModelMeshVerticesFromIndices(const dmRigDDF::Mesh* ddf_mesh, dmRig::RigModelVertex* out, uint32_t vertex_count)
    {
        // If not supporting 32-bit indices, create triangle list as a fallback
        dmRigDDF::MeshVertexIndices* mvi =  ddf_mesh->m_Vertices.m_Data;
        uint32_t *mi = (uint32_t*) ddf_mesh->m_Indices.m_Data;
        for(uint32_t i = 0; i < vertex_count; ++i, ++out, ++mi)
        {
            GetModelVertex(*ddf_mesh, &mvi[*mi], out);
        }
    }

    static uint32_t* WriteModelMeshIndices32(const dmRigDDF::Mesh* mesh, uint32_t start, uint32_t* out, uint32_t* num_written_indices)
    {
        if(mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
        {
            uint32_t icount = mesh->m_Indices.m_Count / 4;
            uint32_t* idata = (uint32_t*)mesh->m_Indices.m_Data;
            for(uint32_t i = 0; i < icount; ++i)
            {
                out[i] = start + idata[i];
            }
            *num_written_indices = icount;
            return out + icount;
        }
        else
        {
            uint32_t icount = mesh->m_Indices.m_Count / 2;
            uint16_t* idata = (uint16_t*)mesh->m_Indices.m_Data;
            for(uint32_t i = 0; i < icount; ++i)
            {
                out[i] = start + (uint32_t)idata[i];
            }
            *num_written_indices = icount;
            return out + icount;
        }
    }

    static uint16_t* WriteModelMeshIndices16(const dmRigDDF::Mesh* mesh, uint16_t start, uint16_t* out, uint32_t* num_written_indices)
    {
        if(mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
        {
            uint32_t icount = mesh->m_Indices.m_Count / 4;
            uint32_t* idata = (uint32_t*)mesh->m_Indices.m_Data;
            for(uint32_t i = 0; i < icount; ++i)
            {
                out[i] = start + (uint16_t)idata[i];
            }
            *num_written_indices = icount;
            return out + icount;
        }
        else
        {
            uint32_t icount = mesh->m_Indices.m_Count / 2;
            uint16_t* idata = (uint16_t*)mesh->m_Indices.m_Data;
            for(uint32_t i = 0; i < icount; ++i)
            {
                out[i] = start + idata[i];
            }
            *num_written_indices = icount;
            return out + icount;
        }
    }

/*
    static void CreateVertexAndIndexBuffers(dmGraphics::HContext context, ModelResource* resource, dmRigDDF::MeshSet* mesh_set, bool support_32bit_indices)
    {
        uint32_t num_ranges = resource->m_MeshRanges.Size();
        resource->m_Buffers.SetCapacity(num_ranges);

        for (uint32_t i = 0; i < num_ranges; ++i)
        {
            ModelResourceBuffers* current_buffers = new ModelResourceBuffers;
            memset(current_buffers, 0, sizeof(ModelResourceBuffers));

            uint32_t num_vertices = 0;
            uint32_t num_indices = 0;

            ModelMeshRange& range = resource->m_MeshRanges[i];
            for (uint32_t j = 0; j < range.m_Count; ++j)
            {
                ModelMesh* mesh = &resource->m_Meshes[range.m_Start + j];
                num_vertices += mesh->m_VertexCount;
                num_indices += mesh->m_IndexCount;
            }

            dmRig::RigModelVertex* rmv_buffer = new dmRig::RigModelVertex[num_vertices];

            dmGraphics::Type index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;

            void* index_buffer = malloc(num_indices * 4);

            dmRig::RigModelVertex* rmv = rmv_buffer;
            void* pindices = index_buffer;
            for (uint32_t j = 0, index_start = 0; j < range.m_Count; ++j)
            {
                ModelMesh* mesh = &resource->m_Meshes[range.m_Start + j];
                dmRigDDF::Mesh* ddf_mesh = mesh->m_Mesh;

                // Fallback for older devices
                if (!support_32bit_indices && ddf_mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
                {
                    delete[] rmv_buffer;
                    num_vertices = ddf_mesh->m_Indices.m_Count / 4; // the new mesh will have as many vertices as the current indices
                    rmv_buffer = new dmRig::RigModelVertex[num_vertices];
                    WriteModelMeshVerticesFromIndices(mesh, rmv, num_vertices);
                    free(index_buffer);
                    index_buffer = 0;
                }
                else
                {
                    rmv = WriteModelMeshVertices(mesh, rmv);

                    uint32_t num_written_indices = 0;
                    if (!support_32bit_indices)
                    {
                        pindices = (void*)WriteModelMeshIndices16(ddf_mesh, index_start, (uint16_t*)pindices, &num_written_indices);
                        index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;
                    }
                    else
                    {
                        if (num_indices > 65536)
                        {
                            pindices = (void*)WriteModelMeshIndices32(ddf_mesh, index_start, (uint32_t*)pindices, &num_written_indices);
                            index_element_type = dmGraphics::TYPE_UNSIGNED_INT;
                        }
                        else
                        {
                            pindices = (void*)WriteModelMeshIndices16(ddf_mesh, index_start, (uint16_t*)pindices, &num_written_indices);
                            index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;
                        }
                    }

                    index_start += num_written_indices;
                }
            }

            current_buffers->m_VertexBuffer = dmGraphics::NewVertexBuffer(context, num_vertices * sizeof(dmRig::RigModelVertex), rmv_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            current_buffers->m_VertexCount = num_vertices;
            delete[] rmv_buffer;

            current_buffers->m_IndexBuffer = 0;
            current_buffers->m_IndexCount = 0;
            if (index_buffer != 0)
            {
                uint32_t index_type_size = dmGraphics::TYPE_UNSIGNED_INT == index_element_type ? 4 : 2;
                current_buffers->m_IndexBuffer = dmGraphics::NewIndexBuffer(context, num_indices * index_type_size, pindices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                current_buffers->m_IndexBufferElementType = index_element_type;
                current_buffers->m_IndexCount = num_indices;
                free(index_buffer);
            }

            resource->m_Buffers.Push(current_buffers);
        }
    }
    */

    static ModelResourceBuffers* CreateBuffers(dmGraphics::HContext context, const dmRigDDF::Mesh* ddf_mesh, bool support_32bit_indices, dmArray<dmRig::RigModelVertex>& scratch_buffer)
    {
        ModelResourceBuffers* buffers = new ModelResourceBuffers;
        memset(buffers, 0, sizeof(ModelResourceBuffers));

        uint32_t num_vertices = ddf_mesh->m_Positions.m_Count / 3;
        if (num_vertices < ddf_mesh->m_Vertices.m_Count)
            num_vertices = ddf_mesh->m_Vertices.m_Count;

        uint32_t num_indices = ddf_mesh->m_Indices.m_Count / 2;
        if (ddf_mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
            num_indices = ddf_mesh->m_Indices.m_Count / 4;

        dmGraphics::Type index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;

        void* index_buffer = 0;

        if (scratch_buffer.Capacity() < num_vertices)
            scratch_buffer.SetCapacity(num_vertices);
        scratch_buffer.SetSize(num_vertices);


        // Fallback for older devices
        if (!support_32bit_indices && ddf_mesh->m_IndicesFormat == dmRigDDF::INDEXBUFFER_FORMAT_32)
        {
            num_vertices = ddf_mesh->m_Indices.m_Count / 4; // the new mesh will have as many vertices as the current indices
            if (scratch_buffer.Capacity() < num_vertices)
                scratch_buffer.SetCapacity(num_vertices);
            scratch_buffer.SetSize(num_vertices);

            WriteModelMeshVerticesFromIndices(ddf_mesh, scratch_buffer.Begin(), num_vertices);
        }
        else
        {
            WriteModelMeshVertices(ddf_mesh, scratch_buffer.Begin());

            index_buffer = malloc(num_indices * 4); // support both 16/32 bit indices depending on cases below

            uint32_t num_written_indices = 0;
            if (!support_32bit_indices)
            {
                WriteModelMeshIndices16(ddf_mesh, 0, (uint16_t*)index_buffer, &num_written_indices);
                index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;
            }
            else
            {
                if (num_indices > 65536)
                {
                    WriteModelMeshIndices32(ddf_mesh, 0, (uint32_t*)index_buffer, &num_written_indices);
                    index_element_type = dmGraphics::TYPE_UNSIGNED_INT;
                }
                else
                {
                    WriteModelMeshIndices16(ddf_mesh, 0, (uint16_t*)index_buffer, &num_written_indices);
                    index_element_type = dmGraphics::TYPE_UNSIGNED_SHORT;
                }
            }
        }

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
            free(index_buffer);
        }

        return buffers;
    }

    static void CreateBuffers(dmGraphics::HContext context, ModelResource* resource, bool support_32bit_indices)
    {
        dmArray<dmRig::RigModelVertex> scratch_buffer;
        for (uint32_t i = 0; i < resource->m_Meshes.Size(); ++i)
        {
            MeshInfo& info = resource->m_Meshes[i];
            info.m_Buffers = CreateBuffers(context, info.m_Mesh, support_32bit_indices, scratch_buffer);
        }
    }

    dmResource::Result AcquireResources(dmGraphics::HContext context, dmResource::HFactory factory, ModelResource* resource, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, resource->m_Model->m_RigScene, (void**) &resource->m_RigScene);
        if (result != dmResource::RESULT_OK)
            return result;

        dmRender::HMaterial material;
        result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &material);
        if (result != dmResource::RESULT_OK)
            return result;

dmLogWarning("Loading model %s", filename);

        bool support_32bit_indices = dmGraphics::IsIndexBufferFormatSupported(context, dmGraphics::INDEXBUFFER_FORMAT_32);

        dmRigDDF::MeshSet* mesh_set = resource->m_RigScene->m_MeshSetRes->m_MeshSet;
        FlattenMeshes(resource, mesh_set);
        CreateBuffers(context, resource, support_32bit_indices);

        //GatherMeshes(resource, mesh_set);
        //CollectMeshRanges(resource, support_32bit_indices);
        //CreateVertexAndIndexBuffers(context, resource, mesh_set, support_32bit_indices);

        resource->m_Materials.SetCapacity(mesh_set->m_Materials.m_Count);
        for (uint32_t i = 0; i < mesh_set->m_Materials.m_Count; ++i)
        {
            printf("resource->m_Model->m_Material: %s\n", resource->m_Model->m_Material);
            // TODO: Map the model materials to the mesh materials!
            dmRender::HMaterial material;
            result = dmResource::Get(factory, resource->m_Model->m_Material, (void**) &material);
            if (result != dmResource::RESULT_OK)
            {

            printf("Failed to load material: %d\n", result);
                return result;
            }

            printf("pushing material: %p\n", (void*)material);

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

        if(dmRender::GetMaterialVertexSpace(material) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
        {
            if(resource->m_RigScene->m_AnimationSetRes || resource->m_RigScene->m_SkeletonRes)
            {
                dmLogError("Failed to create Model component. Material vertex space option VERTEX_SPACE_LOCAL does not support skinning.");
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }

        printf("Loaded Model: %s: %d\n", filename, result);
        return result;
    }

    static void ReleaseBuffers(ModelResourceBuffers* buffers)
    {
        if (buffers->m_VertexBuffer != 0x0)
            dmGraphics::DeleteVertexBuffer(buffers->m_VertexBuffer);
        if (buffers->m_IndexBuffer != 0x0)
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
