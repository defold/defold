#include "comp_model.h"

#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <render/render.h>
#include <render/mesh_ddf.h>
#include <render/model/model.h>
#include <render/model_ddf.h>

#include "../gamesys_private.h"

namespace dmGameSystem
{
    struct ModelWorld;

    struct ModelComponent
    {
        dmModel::HModel m_Model;
        dmGameObject::HInstance m_Instance;
        dmRender::HRenderObject m_RenderObject;
        ModelWorld* m_ModelWorld;
        uint32_t m_Index;
    };

    struct ModelWorld
    {
        dmArray<ModelComponent> m_Components;
        dmIndexPool32 m_ComponentIndices;
    };

    dmGameObject::CreateResult CompModelNewWorld(void* context, void** world)
    {
        ModelWorld* model_world = new ModelWorld();
        // TODO: How to configure?
        const uint32_t max_component_count = 1024;
        model_world->m_Components.SetCapacity(max_component_count);
        model_world->m_Components.SetSize(max_component_count);
        memset((void*)&model_world->m_Components[0], 0, max_component_count * sizeof(ModelComponent));
        model_world->m_ComponentIndices.SetCapacity(max_component_count);
        *world = model_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(void* context, void* world)
    {
        delete (ModelWorld*)world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelCreate(dmGameObject::HCollection collection,
                                            dmGameObject::HInstance instance,
                                            void* resource,
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data)
    {
        ModelWorld* model_world = (ModelWorld*)world;
        if (model_world->m_ComponentIndices.Remaining() > 0)
        {
            dmModel::HModel prototype = (dmModel::HModel)resource;
            uint32_t index = model_world->m_ComponentIndices.Pop();
            dmRender::HRenderObject ro = dmRender::NewRenderObject(g_ModelRenderType, dmModel::GetMaterial(prototype));
            ModelComponent& component = model_world->m_Components[index];
            component.m_Instance = instance;
            component.m_RenderObject = ro;
            component.m_Model = prototype;
            component.m_ModelWorld = model_world;
            component.m_Index = index;
            dmRender::SetUserData(ro, &component);
            *user_data = (uintptr_t)&component;

            return dmGameObject::CREATE_RESULT_OK;
        }
        dmLogError("Could not create model component, out of resources.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompModelDestroy(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data)
    {
        ModelWorld* model_world = (ModelWorld*)world;
        ModelComponent* component = (ModelComponent*)*user_data;
        component->m_RenderObject = dmRender::INVALID_RENDER_OBJECT_HANDLE;
        model_world->m_ComponentIndices.Push(component->m_Index);
        dmRender::DeleteRenderObject(component->m_RenderObject);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        ModelWorld* model_world = (ModelWorld*)world;
        for (uint32_t i = 0; i < model_world->m_Components.Size(); ++i)
        {
            ModelComponent& component = model_world->m_Components[i];
            if (component.m_RenderObject != dmRender::INVALID_RENDER_OBJECT_HANDLE)
            {
                Matrix4 world(dmGameObject::GetWorldRotation(component.m_Instance), Vector3(dmGameObject::GetWorldPosition(component.m_Instance)));
                dmRender::SetWorldTransform(component.m_RenderObject, world);
                dmRender::AddToRender(render_context, component.m_RenderObject);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        ModelComponent* component = (ModelComponent*)*user_data;
        dmRender::HRenderObject ro = component->m_RenderObject;
        if (message_data->m_MessageId == dmHashString32(dmRenderDDF::SetVertexConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmRenderDDF::SetVertexConstant* ddf = (dmRenderDDF::SetVertexConstant*)message_data->m_Buffer;
            dmRender::SetVertexConstant(ro, ddf->m_Register, ddf->m_Value);
        }
        else if (message_data->m_MessageId == dmHashString32(dmRenderDDF::ResetVertexConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmRenderDDF::ResetVertexConstant* ddf = (dmRenderDDF::ResetVertexConstant*)message_data->m_Buffer;
            dmRender::ResetVertexConstant(ro, ddf->m_Register);
        }
        if (message_data->m_MessageId == dmHashString32(dmRenderDDF::SetFragmentConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmRenderDDF::SetFragmentConstant* ddf = (dmRenderDDF::SetFragmentConstant*)message_data->m_Buffer;
            dmRender::SetFragmentConstant(ro, ddf->m_Register, ddf->m_Value);
        }
        if (message_data->m_MessageId == dmHashString32(dmRenderDDF::ResetFragmentConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmRenderDDF::ResetFragmentConstant* ddf = (dmRenderDDF::ResetFragmentConstant*)message_data->m_Buffer;
            dmRender::ResetFragmentConstant(ro, ddf->m_Register);
        }
        else if (message_data->m_MessageId == dmHashString32(dmRender::SetTexture::m_DDFDescriptor->m_ScriptName))
        {
            dmRender::SetTexture* ddf = (dmRender::SetTexture*)message_data->m_Buffer;
            ddf->m_TextureHash = (const char*)((uintptr_t)ddf + (uintptr_t)ddf->m_TextureHash);
            uint32_t hash;
            sscanf(ddf->m_TextureHash, "%X", &hash);
            dmRender::HRenderContext rendercontext = (dmRender::HRenderContext)context;
            dmGraphics::HRenderTarget rendertarget = dmRender::GetRenderTarget(rendercontext, hash);

            if (rendertarget)
            {
                ModelUserData* model_user_data = (ModelUserData*)*user_data;
                dmRender::HRenderObject ro = model_user_data->m_ModelWorld->m_RenderObjects[model_user_data->m_Index];
                dmModel::HModel model = (dmModel::HModel)dmRender::GetUserData(ro);
                dmModel::SetDynamicTexture0(model, dmGraphics::GetRenderTargetTexture(rendertarget));
            }
            else
                dmLogWarning("No such render target: 0x%x (%d)", hash, hash);

        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void RenderTypeModelBegin(dmRender::HRenderContext render_context, void* user_context)
    {
    }

    void RenderTypeModelDraw(dmRender::HRenderContext render_context, void* user_context, dmRender::HRenderObject ro, uint32_t count)
    {
        ModelComponent* component = (ModelComponent*)dmRender::GetUserData(ro);

        dmModel::HMesh mesh = dmModel::GetMesh(component->m_Model);
        if (dmModel::GetPrimitiveCount(mesh) == 0)
            return;

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        // TODO: replace this dynamic texture thingy with proper indexing next
        if (dmModel::GetDynamicTexture0(model))
            dmGraphics::SetTexture(graphics_context, dmModel::GetDynamicTexture0(model));
        else
            dmGraphics::SetTexture(graphics_context, dmModel::GetTexture0(model));

        dmGraphics::EnableVertexDeclaration(graphics_context, dmModel::GetVertexDeclarationBuffer(mesh), dmModel::GetVertexBuffer(mesh));
        dmGraphics::DrawRangeElements(graphics_context, dmGraphics::PRIMITIVE_TRIANGLES, 0, dmModel::GetPrimitiveCount(mesh), dmGraphics::TYPE_UNSIGNED_INT, dmModel::GetIndexBuffer(mesh));
        dmGraphics::DisableVertexDeclaration(graphics_context, dmModel::GetVertexDeclarationBuffer(mesh));
    }

    void RenderTypeModelEnd(const dmRender::HRenderContext render_context, void* user_context)
    {
    }
}
