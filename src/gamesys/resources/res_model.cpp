#include "res_model.h"

#include <render/mesh_ddf.h>
#include <render/model/model.h>
#include <render/model_ddf.h>

namespace dmGameSystem
{
    dmResource::CreateResult ResCreateModel(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
    {
        dmRenderDDF::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRenderDDF_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmModel::HMesh mesh = 0;
        dmRender::HMaterial material = 0;
        dmGraphics::HTexture texture0 = 0;

        dmResource::Get(factory, model_desc->m_Mesh, (void**) &mesh);
        dmResource::Get(factory, model_desc->m_Material, (void**) &material);
        dmResource::Get(factory, model_desc->m_Texture0, (void**) &texture0);

        dmDDF::FreeMessage((void*) model_desc);
        if (mesh == 0 || material == 0 || texture0 == 0)
        {
            if (mesh) dmResource::Release(factory, (void*) mesh);
            if (material) dmResource::Release(factory, (void*) material);
            if (texture0) dmResource::Release(factory, (void*) texture0);

            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmModel::HModel model = dmModel::NewModel();

        dmModel::SetMesh(model, mesh);
        dmModel::SetMaterial(model, material);
        dmModel::SetTexture0(model, texture0);

        resource->m_Resource = (void*) model;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResDestroyModel(dmResource::HFactory factory,
            void* context,
            dmResource::SResourceDescriptor* resource)
    {
        dmModel::HModel model = (dmModel::HModel) resource->m_Resource;

        dmResource::Release(factory, (void*) dmModel::GetMesh(model));
        dmResource::Release(factory, (void*) dmModel::GetMaterial(model));
        dmResource::Release(factory, (void*) dmModel::GetTexture0(model));

        dmModel::DeleteModel(model);

        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult ResRecreateModel(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}
