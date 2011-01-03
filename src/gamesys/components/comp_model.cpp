#include "comp_model.h"

#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>

#include <render/render.h>

#include "../resources/res_model.h"

#include "model_ddf.h"

namespace dmGameSystem
{
    struct ModelWorld;

    struct ModelComponent
    {
        ModelComponent()
        : m_Model(0x0)
        , m_Instance(0)
        , m_RenderObject()
        , m_ModelWorld(0x0)
        , m_Index(0)
        {}

        Model* m_Model;
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
        memset(&model_world->m_Components[0], 0, max_component_count * sizeof(ModelComponent));
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
            Model* model = (Model*)resource;
            Mesh* mesh = model->m_Mesh;
            uint32_t index = model_world->m_ComponentIndices.Pop();
            ModelComponent& component = model_world->m_Components[index];
            component.m_Instance = instance;
            component.m_Model = model;
            component.m_ModelWorld = model_world;
            component.m_Index = index;
            dmRender::RenderObject& ro = component.m_RenderObject;
            ro.m_Material = model->m_Material;
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                ro.m_Textures[i] = model->m_Textures[i];
            ro.m_VertexBuffer = mesh->m_VertexBuffer;
            ro.m_VertexDeclaration = mesh->m_VertexDeclaration;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = mesh->m_VertexCount;
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
        if (message_data->m_MessageId == dmHashString32(dmModelDDF::SetVertexConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmModelDDF::SetVertexConstant* ddf = (dmModelDDF::SetVertexConstant*)message_data->m_Buffer;
            dmRender::SetRenderObjectVertexConstant(ro, ddf->m_Register, ddf->m_Value);
        }
        else if (message_data->m_MessageId == dmHashString32(dmModelDDF::ResetVertexConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmModelDDF::ResetVertexConstant* ddf = (dmModelDDF::ResetVertexConstant*)message_data->m_Buffer;
            dmRender::ResetRenderObjectVertexConstant(ro, ddf->m_Register);
        }
        if (message_data->m_MessageId == dmHashString32(dmModelDDF::SetFragmentConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmModelDDF::SetFragmentConstant* ddf = (dmModelDDF::SetFragmentConstant*)message_data->m_Buffer;
            dmRender::SetRenderObjectFragmentConstant(ro, ddf->m_Register, ddf->m_Value);
        }
        if (message_data->m_MessageId == dmHashString32(dmModelDDF::ResetFragmentConstant::m_DDFDescriptor->m_ScriptName))
        {
            dmModelDDF::ResetFragmentConstant* ddf = (dmModelDDF::ResetFragmentConstant*)message_data->m_Buffer;
            dmRender::ResetRenderObjectFragmentConstant(ro, ddf->m_Register);
        }
        else if (message_data->m_MessageId == dmHashString32(dmModelDDF::SetTexture::m_DDFDescriptor->m_ScriptName))
        {
            dmModelDDF::SetTexture* ddf = (dmModelDDF::SetTexture*)message_data->m_Buffer;
            ddf->m_TextureHash = (const char*)((uintptr_t)ddf + (uintptr_t)ddf->m_TextureHash);
            uint32_t hash;
            sscanf(ddf->m_TextureHash, "%X", &hash);
            uint32_t unit = ddf->m_TextureUnit;
            dmRender::HRenderContext rendercontext = (dmRender::HRenderContext)context;
            dmGraphics::HRenderTarget rendertarget = dmRender::GetRenderTarget(rendercontext, hash);
            if (rendertarget)
            {
                ro->m_Textures[unit] = dmGraphics::GetRenderTargetTexture(rendertarget, dmGraphics::BUFFER_TYPE_COLOR);
            }
            else
                dmLogWarning("No such render target: 0x%x (%d)", hash, hash);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
