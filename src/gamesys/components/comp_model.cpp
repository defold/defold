#include "comp_model.h"

#include <dlib/hash.h>

#include <render/render.h>
#include <render/model/model.h>
#include <render/model_ddf.h>

namespace dmGameSystem
{
    void SetObjectModel(void* context, void* gameobject, Vectormath::Aos::Quat* rotation,
            Vectormath::Aos::Point3* position)
    {
        if (!gameobject) return;
        dmGameObject::HInstance go = (dmGameObject::HInstance) gameobject;
        *position = dmGameObject::GetWorldPosition(go);
        *rotation = dmGameObject::GetWorldRotation(go);
    }

    dmGameObject::CreateResult CompModelNewWorld(void* context, void** world)
    {
        *world = dmRender::NewRenderWorld(1000, 10, SetObjectModel);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(void* context, void* world)
    {
        dmRender::DeleteRenderWorld((dmRender::HRenderWorld)world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelCreate(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* resource,
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data)
    {

        dmModel::HModel prototype = (dmModel::HModel)resource;
        dmRender::HRenderWorld render_world = (dmRender::HRenderWorld)world;


        dmRender::HRenderObject ro = NewRenderObject(render_world, (void*)prototype, (void*)instance, 1, dmRender::RENDEROBJECT_TYPE_MODEL);
        *user_data = (uintptr_t) ro;

        return dmGameObject::CREATE_RESULT_OK;

    }

    dmGameObject::CreateResult CompModelDestroy(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data)
    {
        dmRender::HRenderObject ro = (dmRender::HRenderObject)*user_data;
        dmRender::HRenderWorld render_world = (dmRender::HRenderWorld)world;

        dmRender::DeleteRenderObject(render_world, ro);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        dmRender::HRenderWorld render_collection = (dmRender::HRenderWorld)world;
        dmRender::HRenderWorld render_world = (dmRender::HRenderWorld)context;

        dmRender::AddToRender(render_collection, render_world);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        dmRender::HRenderWorld world = (dmRender::HRenderWorld)context;
        (void)world;

        if (message_data->m_MessageId == dmHashString32(dmRender::SetRenderColor::m_DDFDescriptor->m_ScriptName))
        {
            dmRender::HRenderObject ro = (dmRender::HRenderObject)*user_data;
            dmRender::SetRenderColor* ddf = (dmRender::SetRenderColor*)message_data->m_Buffer;
            dmRender::ColorType color_type = (dmRender::ColorType)ddf->m_ColorType;
            dmRender::SetColor(ro, ddf->m_Color, color_type);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }
}
