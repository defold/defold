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
        dmRender::MeshDesc      m_Desc;
        bool                    m_Deleted;

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

    HMesh NewMesh()
    {
        Mesh* mesh = new Mesh;
        return mesh;
    }

    void DeleteMesh(HMesh mesh)
    {
        delete mesh;
    }

    uint32_t        GetPrimitiveCount(HMesh mesh)       { return mesh->m_Desc.m_PrimitiveCount;                 }
    PrimtiveType    GetPrimitiveType(HMesh mesh)        { return (PrimtiveType)mesh->m_Desc.m_PrimitiveType;    }
    const void*     GetPositions(HMesh mesh)            { return &mesh->m_Desc.m_Positions.m_Data[0];           }
    uint32_t        GetPositionCount(HMesh mesh)        { return mesh->m_Desc.m_Positions.m_Count;              }

    const void*     GetTexcoord0(HMesh mesh)            { return &mesh->m_Desc.m_Texcoord0.m_Data[0];           }
    uint32_t        GetTexcoord0Count(HMesh mesh)       { return mesh->m_Desc.m_Texcoord0.m_Count;              }

    const void*     GetNormals(HMesh mesh)              { return &mesh->m_Desc.m_Normals.m_Data[0];             }
    uint32_t        GetNormalCount(HMesh mesh)          { return mesh->m_Desc.m_Normals.m_Count;                }

    const void*     GetIndices(HMesh mesh)              { return &mesh->m_Desc.m_Indices.m_Data[0];             }
    uint32_t        GetIndexCount(HMesh mesh)           { return mesh->m_Desc.m_Indices.m_Count;                }

    void SetMesh(HModel model, HMesh mesh)
    {
        model->m_Mesh = mesh;
    }

    void SetTexture0(HModel model, dmGraphics::HTexture texture)
    {
        model->m_Texture0 = texture;
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

    dmGraphics::HMaterial GetMaterial(HModel model)
    {
        return model->m_Material;
    }
}
