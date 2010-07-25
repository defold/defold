#include <string.h>
#include <dlib/array.h>
#include <dlib/log.h>
#include <graphics/graphics_device.h>
#include "model.h"

namespace dmModel
{
    struct Model
    {
        Model()
        {
            memset(this, 0x0, sizeof(*this));
            m_Deleted = false;
        }

        Render::Mesh*           m_Mesh;
        dmGraphics::HMaterial   m_Material;
        dmGraphics::HTexture    m_Texture0;

        void*                   m_GameObject;
        void*                   m_Collection;

        bool                    m_Deleted;
    };

    class ModelWorld
    {
    public:
        void AddModel(HModel model)
        {
            m_ModelList.Push(model);
        }

        void UpdateContext(RenderContext* rendercontext)
        {
            m_RenderContext = *rendercontext;
        }

        dmArray<Model*>         m_ModelList;
        RenderContext           m_RenderContext;
        SetObjectModel          m_SetGameobjectModel;

    };

    HModel NewModel()
    {
        Model* model = new Model;
        return (HModel)model;
    }

#if 0
    HModel NewModel(HModel prototype, void* gameobject, void* collection)
    {
    	assert(false);
        Model* model = NewModel();

        *model = *prototype;
        model->m_GameObject = gameobject;
        model->m_Collection = collection;

        return (HModel)model;
    }

    void DeleteModel(HWorld world, HModel model)
    {
        model->m_Deleted = true;
    }
#endif
    void SetMesh(HModel model, Render::Mesh* mesh)
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

    Render::Mesh* GetMesh(HModel model)
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

    void AddModel(HWorld world, HModel model)
    {
        world->AddModel(model);
    }

#if 0
    void RenderModel(Model* model, RenderContext* rendercontext, Vectormath::Aos::Quat rotation, Point3 position)
    {
        Render::Mesh* mesh = model->m_Mesh;
        if (mesh->m_PrimitiveCount == 0)
            return;

        dmGraphics::HContext context = rendercontext->m_GFXContext;

        dmGraphics::SetTexture(context, model->m_Texture0);

        dmGraphics::SetBlendFunc(context, dmGraphics::BLEND_FACTOR_SRC_ALPHA, dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
        dmGraphics::EnableState(context, dmGraphics::BLEND);

        dmGraphics::SetFragmentProgram(context, dmGraphics::GetMaterialFragmentProgram(model->m_Material) );

        for (uint32_t i=0; i<dmGraphics::MAX_MATERIAL_CONSTANTS; i++)
        {
            uint32_t mask = dmGraphics::GetMaterialFragmentConstantMask(model->m_Material);
            if (mask & (1 << i) )
            {
                Vector4 v = dmGraphics::GetMaterialFragmentProgramConstant(model->m_Material, i);
                dmGraphics::SetFragmentConstant(context, &v, i);
            }
        }


        dmGraphics::SetVertexProgram(context, dmGraphics::GetMaterialVertexProgram(model->m_Material) );

        Matrix4 m(rotation, Vector3(position));

        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&rendercontext->m_ViewProj, 0, 4);
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&m, 4, 4);

        Matrix4 texturmatrix;
        texturmatrix = Matrix4::identity();
        dmGraphics::SetVertexConstantBlock(context, (const Vector4*)&texturmatrix, 8, 4);


        for (uint32_t i=0; i<dmGraphics::MAX_MATERIAL_CONSTANTS; i++)
        {
            uint32_t mask = dmGraphics::GetMaterialVertexConstantMask(model->m_Material);
            if (mask & (1 << i) )
            {
                Vector4 v = dmGraphics::GetMaterialVertexProgramConstant(model->m_Material, i);
                dmGraphics::SetVertexConstantBlock(context, &v, i, 1);
            }
        }


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
#endif

    HWorld NewWorld(uint32_t max_models, SetObjectModel set_object_model)
    {
        ModelWorld* world = new ModelWorld;
        world->m_ModelList.SetCapacity(max_models);
        world->m_SetGameobjectModel = set_object_model;

        return (HWorld)world;
    }

    void DeleteWorld(HWorld world)
    {
        for (size_t i=0; i<world->m_ModelList.Size(); i++)
        {
            Model* model = world->m_ModelList[i];

            if (model->m_Deleted == false)
                dmLogInternal(DM_LOG_SEVERITY_WARNING, "Model not marked for delete\n");

            delete model;

        }

        delete world;
    }

    void UpdateContext(HWorld world, RenderContext* rendercontext)
    {
//        world->UpdateContext(rendercontext);
    }

    void RenderWorld(HWorld world)
    {
//    	return;
        // iterate world and look for deleted models
        uint32_t size = world->m_ModelList.Size();
        for (uint32_t i=0; i<size; i++)
        {
            if (world->m_ModelList[i]->m_Deleted)
            {
                world->m_ModelList.EraseSwap(i);
                size = world->m_ModelList.Size();
            }
        }
#if 0
        for (size_t i=0; i<world->m_ModelList.Size(); i++)
        {
            Model* model = world->m_ModelList[i];

            if (model->m_Deleted)
                continue;

            Quat rot;
            Point3 pos;

            world->m_SetGameobjectModel(model->m_Collection, model->m_GameObject, &rot, &pos);

//            RenderModel(model, &world->m_RenderContext, rot, pos);


        }
#endif
    }

}
