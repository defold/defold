#include "res_mesh.h"

#include <render/render.h>
#include <stdlib.h>

#include "mesh_ddf.h"

namespace dmGameSystem
{
    // TODO: will be replaced when we have a proper mesh compiler
    struct MeshVertex
    {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
    };

    void CopyVertexData(dmMeshDDF::MeshDesc* mesh_desc, MeshVertex* vertex_buffer);

    dmResource::Result ResCreateMesh(const dmResource::ResourceCreateParams& params)
    {
        dmGraphics::HContext graphics_context = (dmGraphics::HContext) params.m_Context;
        dmMeshDDF::MeshDesc* mesh_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &mesh_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        Mesh* mesh = new Mesh();

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"normal", 1, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 2, 2, dmGraphics::TYPE_FLOAT, false}
        };
        mesh->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, ve, 3);

        // TODO: move this bit to the mesh compiler
        uint32_t vertex_count = 0;
        for (uint32_t i = 0; i < mesh_desc->m_Components.m_Count; ++i)
        {
            vertex_count += mesh_desc->m_Components[i].m_Positions.m_Count / 3;
            assert(mesh_desc->m_Components[i].m_Positions.m_Count == mesh_desc->m_Components[i].m_Normals.m_Count);
            assert(mesh_desc->m_Components[i].m_Texcoord0.m_Count == 0 || mesh_desc->m_Components[i].m_Positions.m_Count / 3 == mesh_desc->m_Components[i].m_Texcoord0.m_Count / 2);
        }
        void* vertex_buffer = (float*) malloc(vertex_count * sizeof(MeshVertex));
        CopyVertexData(mesh_desc, (MeshVertex*) vertex_buffer);
        mesh->m_VertexBuffer = dmGraphics::NewVertexBuffer(graphics_context, vertex_count * sizeof(MeshVertex), (const void*) vertex_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        mesh->m_VertexCount = vertex_count;
        free((void*) vertex_buffer);
        dmDDF::FreeMessage(mesh_desc);

        params.m_Resource->m_Resource = (void*) mesh;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResDestroyMesh(const dmResource::ResourceDestroyParams& params)
    {
        Mesh* mesh = (Mesh*)params.m_Resource->m_Resource;
        dmGraphics::DeleteVertexDeclaration(mesh->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(mesh->m_VertexBuffer);
        delete mesh;

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResRecreateMesh(const dmResource::ResourceRecreateParams& params)
    {
        dmMeshDDF::MeshDesc* mesh_desc;
        dmDDF::Result e = dmDDF::LoadMessage(params.m_Buffer, params.m_BufferSize, &dmMeshDDF_MeshDesc_DESCRIPTOR, (void**) &mesh_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        Mesh* mesh = (Mesh*)params.m_Resource->m_Resource;
        uint32_t vertex_count = 0;
        for (uint32_t i = 0; i < mesh_desc->m_Components.m_Count; ++i)
        {
            vertex_count += mesh_desc->m_Components[i].m_Positions.m_Count / 3;
            assert(vertex_count * 3 == mesh_desc->m_Components[i].m_Normals.m_Count);
            assert(mesh_desc->m_Components[i].m_Texcoord0.m_Count == 0 || vertex_count * 2 == mesh_desc->m_Components[i].m_Texcoord0.m_Count);
        }

        void* vertex_buffer = (float*) malloc(vertex_count * sizeof(MeshVertex));
        CopyVertexData(mesh_desc, (MeshVertex*) vertex_buffer);
        dmGraphics::SetVertexBufferData(mesh->m_VertexBuffer, vertex_count * sizeof(MeshVertex), vertex_buffer, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        mesh->m_VertexCount = vertex_count;
        free((void*) vertex_buffer);
        dmDDF::FreeMessage(mesh_desc);

        return dmResource::RESULT_OK;
    }

    void CopyVertexData(dmMeshDDF::MeshDesc* mesh_desc, MeshVertex* vertex_buffer)
    {
        MeshVertex* v = (MeshVertex*)vertex_buffer;
        uint32_t vi = 0;
        for (uint32_t i = 0; i < mesh_desc->m_Components.m_Count; ++i)
        {
            dmMeshDDF::MeshComponent& comp = mesh_desc->m_Components[i];
            uint32_t vertex_count = comp.m_Positions.m_Count / 3;
            for (uint32_t j = 0; j < vertex_count; j++)
            {
                v[vi].x = comp.m_Positions.m_Data[j*3+0];
                v[vi].y = comp.m_Positions.m_Data[j*3+1];
                v[vi].z = comp.m_Positions.m_Data[j*3+2];

                v[vi].nx = comp.m_Normals.m_Data[j*3+0];
                v[vi].ny = comp.m_Normals.m_Data[j*3+1];
                v[vi].nz = comp.m_Normals.m_Data[j*3+2];

                if (comp.m_Texcoord0.m_Count)
                {
                    v[vi].u = comp.m_Texcoord0.m_Data[j*2+0];
                    v[vi].v = comp.m_Texcoord0.m_Data[j*2+1];
                }
                ++vi;
            }
        }

    }
}
