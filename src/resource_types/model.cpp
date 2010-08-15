#include "model.h"

#include <dlib/hash.h>

#include <graphics/material.h>
#include "rendercontext.h"
#include "../build/default/proto/render/material_ddf.h"
#include "../build/default/proto/render/mesh_ddf.h"
#include "../build/default/proto/render/model_ddf.h"
#include "../model/model.h"
#include "../render.h"


namespace dmModel
{
    dmResource::CreateResult CreateResource(dmResource::HFactory factory,
                                      void* context,
                                      const void* buffer, uint32_t buffer_size,
                                      dmResource::SResourceDescriptor* resource,
                                      const char* filename)
    {
        dmRender::ModelDesc* model_desc;
        dmDDF::Result e = dmDDF::LoadMessage(buffer, buffer_size, &dmRender_ModelDesc_DESCRIPTOR, (void**) &model_desc);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::CREATE_RESULT_UNKNOWN;
        }

        dmModel::HMesh mesh = 0;
        dmGraphics::HMaterial material = 0;
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

        dmModel::HWorld world = (dmModel::HWorld)context;
        dmModel::HModel model = dmModel::NewModel();
        dmModel::AddModel(world, model);

        dmModel::SetMesh(model, mesh);
        dmModel::SetMaterial(model, material);
        dmModel::SetTexture0(model, texture0);

        resource->m_Resource = (void*) model;
        return dmResource::CREATE_RESULT_OK;
    }

    dmResource::CreateResult DestroyResource(dmResource::HFactory factory,
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

    dmResource::CreateResult RecreateResource(dmResource::HFactory factory,
            void* context,
            const void* buffer, uint32_t buffer_size,
            dmResource::SResourceDescriptor* resource,
            const char* filename)
    {
        // TODO: Implement me!
        return dmResource::CREATE_RESULT_UNKNOWN;
    }

    dmGameObject::CreateResult CreateComponent(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* resource,
                                            void* context,
                                            uintptr_t* user_data)
    {

        dmModel::HModel prototype = (dmModel::HModel)resource;
        dmRender::HRenderObject ro = NewRenderObjectInstance((void*)prototype, (void*)instance, dmRender::RENDEROBJECT_TYPE_MODEL);
        *user_data = (uintptr_t) ro;

        return dmGameObject::CREATE_RESULT_OK;

    }

    dmGameObject::CreateResult DestroyComponent(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* context,
                                             uintptr_t* user_data)
    {
        dmRender::HRenderObject ro = (dmRender::HRenderObject)*user_data;
        dmRender::DeleteRenderObject(ro);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult UpdateComponent(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* context)
    {
        dmModel::HWorld world = (dmModel::HWorld)context;
        dmModel::UpdateWorld(world);
        dmRender::Update();
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult OnEventComponent(dmGameObject::HCollection collection,
            dmGameObject::HInstance instance,
            const dmGameObject::ScriptEventData* event_data,
            void* context,
            uintptr_t* user_data)
    {
        if (event_data->m_EventHash == dmHashString32("SetRenderColor"))
        {
            dmRender::HRenderObject ro = (dmRender::HRenderObject)*user_data;
            dmRender::SetRenderColor* ddf = (dmRender::SetRenderColor*)event_data->m_DDFData;
            dmRender::MaterialDesc::ParameterSemantic color_type = (dmRender::MaterialDesc::ParameterSemantic)ddf->m_ColorType;
            dmRender::SetColor(ro, ddf->m_Color, color_type);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
