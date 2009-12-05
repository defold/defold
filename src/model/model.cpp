#define _VECTORMATH_DEBUG
#include <vector>
#include <graphics/graphics_device.h>

#include "model.h"

using namespace Vectormath::Aos;

namespace Model
{
    struct SModel
    {
        Render::Mesh*           m_Mesh;
        SMaterial*              m_Material;
        dmGraphics::HTexture    m_Texture0;

        dmGameObject::HInstance   m_GameObject;
        dmGameObject::HCollection m_Collection;
    };

    class ModelWorld
    {
    public:
        void AddModel(HModel model)
        {
            m_ModelList.push_back(model);
        }

        void UpdateContext(RenderContext* rendercontext)
        {
            m_RenderContext = *rendercontext;
        }

        std::vector<SModel*>    m_ModelList;
        RenderContext           m_RenderContext;

    };

    HModel NewModel()
    {
        SModel* model = new SModel;
        return (HModel)model;
    }

    HModel NewModel(HModel prototype, dmGameObject::HInstance gameobject, dmGameObject::HCollection collection)
    {
        SModel* model = NewModel();

        *model = *prototype;
        model->m_GameObject = gameobject;
        model->m_Collection = collection;

        return (HModel)model;
    }

    void DeleteModel(HWorld world, HModel model)
    {
        // TODO:: delete model properly!

    }

    void SetMesh(HModel model, Render::Mesh* mesh)
    {
        model->m_Mesh = mesh;
    }

    void SetTexture0(HModel model, dmGraphics::HTexture texture)
    {
        model->m_Texture0 = texture;
    }

    void SetMaterial(HModel model, SMaterial* material)
    {
        model->m_Material = material;
    }

    void AddModel(HWorld world, HModel model)
    {
        world->AddModel(model);
    }

    void RenderModel(SModel* model, RenderContext* rendercontext, Vectormath::Aos::Quat rotation, Point3 position)
    {
        Render::Mesh* mesh = model->m_Mesh;
        if (mesh->m_PrimitiveCount == 0)
            return;

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        dmGraphics::SetTexture(context, model->m_Texture0);

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::EnableState(context, dmGraphics::BLEND);
        dmGraphics::SetFragmentProgram(context, model->m_Material->m_FragmentProgram);

        dmGraphics::SetVertexProgram(context, model->m_Material->m_VertexProgram);

        Matrix4 m(rotation, Vector3(position));

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&m, 4, 4);

        Matrix4 texturmatrix;
        texturmatrix = Matrix4::identity();
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&texturmatrix, 8, 4);



        dmGraphics::SetVertexStream(context, 0, 3, dmGraphics::TYPE_FLOAT, 0, (void*) &mesh->m_Positions[0]);

        if (mesh->m_Texcoord0.m_Count > 0)
        {
            dmGraphics::SetVertexStream(context, 1, 2, dmGraphics::TYPE_FLOAT, 0, (void*) &mesh->m_Texcoord0[0]);
        }
        else
        {
            dmGraphics::DisableVertexStream(context, 1);
        }


        if (mesh->m_Normals.m_Count > 0)
        {
            dmGraphics::SetVertexStream(context, 2, 3, dmGraphics::TYPE_FLOAT, 0, (void*) &mesh->m_Normals[0]);
        }
        else
        {
            dmGraphics::DisableVertexStream(context, 2);
        }

        dmGraphics::DrawElements(context, dmGraphics::PRIMITIVE_TRIANGLES, mesh->m_PrimitiveCount*3, dmGraphics::TYPE_UNSIGNED_INT, (void*) &mesh->m_Indices[0]);

        dmGraphics::DisableState(context, dmGraphics::BLEND);
        dmGraphics::DisableVertexStream(context, 0);
        dmGraphics::DisableVertexStream(context, 1);
        dmGraphics::DisableVertexStream(context, 2);
    }


    HWorld NewWorld(uint32_t max_models)
    {
        ModelWorld* world = new ModelWorld;
        world->m_ModelList.reserve(max_models);

        return (HWorld)world;
    }

    void UpdateContext(HWorld world, RenderContext* rendercontext)
    {
        world->UpdateContext(rendercontext);
    }

    void RenderWorld(HWorld world)
    {
        for (int i=0; i<world->m_ModelList.size(); i++)
        {
            SModel* model = (SModel*)world->m_ModelList[i];
            dmGameObject::HInstance go = (dmGameObject::HInstance) model->m_GameObject;
            dmGameObject::HCollection collection = (dmGameObject::HCollection) model->m_Collection;
            Point3 pos = dmGameObject::GetPosition(collection, go);
            Quat rot = dmGameObject::GetRotation(collection, go);

            RenderModel(model, &world->m_RenderContext, rot, pos);
        }

    }

    void DeleteWorld(HWorld world)
    {
        delete world;
    }
}
