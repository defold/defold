#include "comp_model.h"

#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <render/render.h>
#include <render/mesh_ddf.h>
#include <render/model/model.h>
#include <render/model_ddf.h>

namespace dmGameSystem
{
    struct ModelWorld;

    struct ModelComponent
    {
        ModelComponent()
        : m_Model(0)
        , m_Instance(0)
        , m_RenderObject()
        , m_ModelWorld(0x0)
        , m_Index(0)
        {}

        dmModel::HModel m_Model;
        dmGameObject::HInstance m_Instance;
        dmRender::RenderObject m_RenderObject;
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
            dmModel::HMesh mesh = dmModel::GetMesh(prototype);
            uint32_t index = model_world->m_ComponentIndices.Pop();
            ModelComponent& component = model_world->m_Components[index];
            component.m_Instance = instance;
            component.m_Model = prototype;
            component.m_ModelWorld = model_world;
            component.m_Index = index;
            dmRender::RenderObject& ro = component.m_RenderObject;
            ro.m_Material = dmModel::GetMaterial(prototype);
            ro.m_Texture = dmModel::GetTexture0(prototype);
            ro.m_VertexBuffer = dmModel::GetVertexBuffer(mesh);
            ro.m_VertexDeclaration = dmModel::GetVertexDeclarationBuffer(mesh);
            ro.m_IndexBuffer = dmModel::GetIndexBuffer(mesh);
            ro.m_IndexType = dmGraphics::TYPE_UNSIGNED_INT;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = dmModel::GetPrimitiveCount(mesh);
            ro.m_TextureTransform = Matrix4::identity();
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
        component->m_Model = 0;
        model_world->m_ComponentIndices.Push(component->m_Index);

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
            if (component.m_Model != 0)
            {
                Matrix4 world(dmGameObject::GetWorldRotation(component.m_Instance), Vector3(dmGameObject::GetWorldPosition(component.m_Instance)));
                component.m_RenderObject.m_WorldTransform = world;
                dmRender::AddToRender(render_context, &component.m_RenderObject);
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
        dmRender::RenderObject* ro = &component->m_RenderObject;
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
        else if (message_data->m_MessageId == dmHashString32(dmRenderDDF::SetTexture::m_DDFDescriptor->m_ScriptName))
        {
            dmRenderDDF::SetTexture* ddf = (dmRenderDDF::SetTexture*)message_data->m_Buffer;
            ddf->m_TextureHash = (const char*)((uintptr_t)ddf + (uintptr_t)ddf->m_TextureHash);
            uint32_t hash;
            sscanf(ddf->m_TextureHash, "%X", &hash);
            uint32_t slot = ddf->m_TextureSlot;
            dmRender::HRenderContext rendercontext = (dmRender::HRenderContext)context;
            dmGraphics::HRenderTarget rendertarget = dmRender::GetRenderTarget(rendercontext, hash);
            if (rendertarget)
            {
                ro->m_Texture = dmGraphics::GetRenderTargetTexture(rendertarget);
            }
            else
                dmLogWarning("No such render target: 0x%x (%d)", hash, hash);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
