#include "res_model.h"

#include "mesh_ddf.h"
#include "model_ddf.h"

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateModel(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture texture = 0;

        dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        dmResource::Get(factory, model_desc->m_Texture0, (void**) &texture);

        dmDDF::FreeMessage((void*) model_desc);
        if (mesh == 0 || material == 0 || texture == 0)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            if (texture) dmResource::Release(factory, (void*) texture);

            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Model* model = new Model();
        model->m_Mesh = mesh;
        model->m_Material = material;
        model->m_Texture = texture;

        resource->m_Resource = (void*) model;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyModel(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        Model* model = (Model*)resource->m_Resource;

        dmResource::Release(factory, (void*)model->m_Mesh);
        dmResource::Release(factory, (void*)model->m_Material);
        dmResource::Release(factory, (void*)model->m_Texture);

        delete model;

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateModel(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        dmModelDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmModelDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Mesh* mesh = 0x0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture texture = 0;

        dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        dmResource::Get(factory, model_desc->m_Texture0, (void**) &texture);

        dmDDF::FreeMessage((void*) model_desc);
        if (mesh == 0 || material == 0 || texture == 0)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            if (texture) dmResource::Release(factory, (void*) texture);

            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        Model* model = (Model*)resource->m_Resource;
        dmResource::Release(factory, (void*)model->m_Mesh);
        dmResource::Release(factory, (void*)model->m_Material);
        dmResource::Release(factory, (void*)model->m_Texture);

        model->m_Mesh = mesh;
        model->m_Material = material;
        model->m_Texture = texture;

        return dmResource::CREATE_RESULT_OK;
    }
}
