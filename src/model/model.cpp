#include <string.h>
#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <ddf/ddf.h>
#include <graphics/graphics_device.h>
#include "../build/default/proto/render/mesh_ddf.h"
#include "model.h"

namespace dmModel
{
    struct Mesh
    {
        Mesh()
        {
            memset(this, 0x0, sizeof(*this));
            m_Deleted = false;
        }
        dmRender::MeshDesc*             m_Desc;
        dmGraphics::HIndexBuffer        m_IndexBuffer;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmGraphics::HVertexDeclaration  m_VertexDecl;
        bool                            m_Deleted;

    };

    struct Model
    {
        Model()
        {
            memset(this, 0x0, sizeof(*this));
            m_Deleted = false;
        }

        HMesh                   m_Mesh;
        dmGraphics::HMaterial   m_Material;
        dmGraphics::HTexture    m_Texture0;
        dmGraphics::HTexture    m_DynamicTexture0;

        bool                    m_Deleted;
    };


    HModel NewModel()
    {
        Model* model = new Model;
        return model;
    }

    void DeleteModel(HModel model)
    {
        delete model;
    }

    HMesh NewMesh(dmRender::MeshDesc* desc)
    {
        // TODO: will be replaced when we have a proper model compiler
        struct VertexFormat
        {
            float x, y, z;
            float nx, ny, nz;
            float u, v;
        };

        Mesh* mesh = new Mesh;

        // TODO: move this bit to the model compiler
        uint32_t vertex_count = desc->m_Positions.m_Count / 3;
        assert(vertex_count * 3 == desc->m_Normals.m_Count);
        assert(desc->m_Texcoord0.m_Count == 0 || vertex_count * 2 == desc->m_Texcoord0.m_Count);
        VertexFormat* f = (VertexFormat*)malloc(sizeof(VertexFormat) * vertex_count);
        for (uint32_t i = 0; i < vertex_count; i++)
        {
            f[i].x = desc->m_Positions.m_Data[i*3+0];
            f[i].y = desc->m_Positions.m_Data[i*3+1];
            f[i].z = desc->m_Positions.m_Data[i*3+2];

            f[i].nx = desc->m_Normals.m_Data[i*3+0];
            f[i].ny = desc->m_Normals.m_Data[i*3+1];
            f[i].nz = desc->m_Normals.m_Data[i*3+2];

            if (desc->m_Texcoord0.m_Count)
            {
                f[i].u = desc->m_Texcoord0.m_Data[i*2+0];
                f[i].v = desc->m_Texcoord0.m_Data[i*2+1];
            }
        }

        dmGraphics::VertexElement ve[] =
        {
                {0, 3, dmGraphics::TYPE_FLOAT, 0, 0},
                {1, 3, dmGraphics::TYPE_FLOAT, 0, 0},
                {2, 2, dmGraphics::TYPE_FLOAT, 0, 0}
        };

        mesh->m_VertexDecl = dmGraphics::NewVertexDeclaration(ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        mesh->m_IndexBuffer = dmGraphics::NewIndexBuffer(sizeof(uint32_t) * desc->m_Indices.m_Count, desc->m_Indices.m_Data, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        mesh->m_VertexBuffer = dmGraphics::NewVertexBuffer(sizeof(VertexFormat) * vertex_count, (void*)f, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        mesh->m_Desc = desc;
        free(f);
        return mesh;
    }

    void DeleteMesh(HMesh mesh)
    {
        dmGraphics::DeleteVertexDeclaration(mesh->m_VertexDecl);
        dmGraphics::DeleteVertexBuffer(mesh->m_VertexBuffer);
        dmGraphics::DeleteIndexBuffer(mesh->m_IndexBuffer);
        dmDDF::FreeMessage((void*) mesh->m_Desc);

        delete mesh;
    }

    dmGraphics::HVertexBuffer       GetVertexBuffer(HMesh mesh)            { return mesh->m_VertexBuffer; }
    dmGraphics::HIndexBuffer        GetIndexBuffer(HMesh mesh)             { return mesh->m_IndexBuffer;  }
    dmGraphics::HVertexDeclaration  GetVertexDeclarationBuffer(HMesh mesh) { return mesh->m_VertexDecl;   }

    uint32_t        GetPrimitiveCount(HMesh mesh)       { return mesh->m_Desc->m_PrimitiveCount;                 }
    PrimtiveType    GetPrimitiveType(HMesh mesh)        { return (PrimtiveType)mesh->m_Desc->m_PrimitiveType;    }
    uint32_t        GetIndexCount(HMesh mesh)           { return mesh->m_Desc->m_Indices.m_Count;                }

    void SetMesh(HModel model, HMesh mesh)
    {
        model->m_Mesh = mesh;
    }

    void SetTexture0(HModel model, dmGraphics::HTexture texture)
    {
        model->m_Texture0 = texture;
    }

    void SetDynamicTexture0(HModel model, dmGraphics::HTexture texture)
    {
        model->m_DynamicTexture0 = texture;
    }

    void SetMaterial(HModel model, dmGraphics::HMaterial material)
    {
        model->m_Material = material;
    }

    HMesh GetMesh(HModel model)
    {
        return model->m_Mesh;
    }

    dmGraphics::HTexture GetTexture0(HModel model)
    {
        return model->m_Texture0;
    }

    dmGraphics::HTexture GetDynamicTexture0(HModel model)
    {
        return model->m_DynamicTexture0;
    }

    dmGraphics::HMaterial GetMaterial(HModel model)
    {
        return model->m_Material;
    }
}
