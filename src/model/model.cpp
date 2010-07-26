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

        bool                    m_Deleted;
    };

    class ModelWorld
    {
    public:
        void AddModel(HModel model)
        {
            m_ModelList.Push(model);
        }

        dmArray<Model*> m_ModelList;
    };

    HModel NewModel()
    {
        Model* model = new Model;
        return (HModel)model;
    }

    void DeleteModel(HModel model)
    {
    	model->m_Deleted = true;
    }

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


    HWorld NewWorld(uint32_t max_models)
    {
        ModelWorld* world = new ModelWorld;
        world->m_ModelList.SetCapacity(max_models);

        return (HWorld)world;
    }

    void DeleteWorld(HWorld world)
    {
        uint32_t size = world->m_ModelList.Size();
        for (uint32_t i=0; i<size; i++)
        {
			Model* model = world->m_ModelList[i];
			delete model;
        }
        delete world;
    }

    void UpdateWorld(HWorld world)
    {
        // iterate world and look for deleted models
    	return;
    }

}
