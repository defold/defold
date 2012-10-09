#include "comp_model.h"

#include <float.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <render/render.h>

#include "../resources/res_model.h"
#include "../gamesys_private.h"

#include "model_ddf.h"

namespace dmGameSystem
{
    using namespace Vectormath::Aos;

    struct ModelWorld;

    struct ModelComponent
    {
        ModelComponent()
        : m_Model(0x0)
        , m_Instance(0)
        , m_RenderObject()
        , m_ModelWorld(0x0)
        {}

        Model* m_Model;
        dmGameObject::HInstance m_Instance;
        Point3 m_Position;
        Quat   m_Rotation;
        dmRender::RenderObject m_RenderObject;
        ModelWorld* m_ModelWorld;
    };

    struct ModelWorld
    {
        dmArray<ModelComponent> m_Components;
    };

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelWorld* model_world = new ModelWorld();
        // TODO: How to configure?
        const uint32_t max_component_count = 1024;
        model_world->m_Components.SetCapacity(max_component_count);
        *params.m_World = model_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        delete (ModelWorld*)params.m_World;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelCreate(const dmGameObject::ComponentCreateParams& params)
    {
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        if (!model_world->m_Components.Full())
        {
            Model* model = (Model*)params.m_Resource;
            ModelComponent component;

            component.m_Instance = params.m_Instance;
            component.m_Position = params.m_Position;
            component.m_Rotation = params.m_Rotation;
            component.m_Model = model;
            component.m_ModelWorld = model_world;
            component.m_RenderObject.m_Material = 0;
            model_world->m_Components.Push(component);
            *params.m_UserData = (uintptr_t)params.m_Instance;

            return dmGameObject::CREATE_RESULT_OK;
        }
        dmLogError("Could not create model component, out of resources.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        dmGameObject::HInstance instance = (dmGameObject::HInstance)*params.m_UserData;
        dmArray<ModelComponent>& components = model_world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (components[i].m_Instance == instance)
            {
                components.EraseSwap(i);
                break;
            }
        }

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)params.m_Context;
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        dmArray<ModelComponent>& components = model_world->m_Components;
        uint32_t n = components.Size();
        Model* model;
        Mesh* mesh;
        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent& component = components[i];
            model = component.m_Model;
            mesh = model->m_Mesh;
            dmRender::RenderObject& ro = component.m_RenderObject;

            Point3 world_pos = dmGameObject::GetWorldPosition(component.m_Instance);
            Quat world_rot = dmGameObject::GetWorldRotation(component.m_Instance);
            const Quat& local_rot = component.m_Rotation;
            const Point3& local_pos = component.m_Position;
            Quat rotation = world_rot * local_rot;
            Point3 position = rotate(world_rot, Vector3(local_pos)) + world_pos;
            Matrix4 world = Matrix4::rotation(rotation);
            world.setCol3(Vector4(position));

            ro.m_Material = model->m_Material;
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                ro.m_Textures[i] = model->m_Textures[i];
            ro.m_VertexBuffer = mesh->m_VertexBuffer;
            ro.m_VertexDeclaration = mesh->m_VertexDeclaration;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = mesh->m_VertexCount;
            ro.m_TextureTransform = Matrix4::identity();
            ro.m_WorldTransform = world;
            ro.m_CalculateDepthKey = 1;
            dmRender::AddToRender(render_context, &component.m_RenderObject);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        dmGameObject::HInstance instance = (dmGameObject::HInstance)*params.m_UserData;
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        dmArray<ModelComponent>& components = model_world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent* component = &components[i];
            if (component->m_Instance == instance)
            {
                dmRender::RenderObject* ro = &component->m_RenderObject;

                // Return if pre first update.
                // Perhaps we should initialize stuff in Create instead of Update?
                // EnableRenderObjectConstant asserts on valid material
                if (!ro->m_Material)
                    return dmGameObject::UPDATE_RESULT_OK;

                if (params.m_Message->m_Id == dmModelDDF::SetConstant::m_DDFDescriptor->m_NameHash)
                {
                    dmModelDDF::SetConstant* ddf = (dmModelDDF::SetConstant*)params.m_Message->m_Data;
                    dmRender::EnableRenderObjectConstant(ro, ddf->m_NameHash, ddf->m_Value);
                }
                else if (params.m_Message->m_Id == dmModelDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
                {
                    dmModelDDF::ResetConstant* ddf = (dmModelDDF::ResetConstant*)params.m_Message->m_Data;
                    dmRender::DisableRenderObjectConstant(ro, ddf->m_NameHash);
                }
                else if (params.m_Message->m_Id == dmModelDDF::SetTexture::m_DDFDescriptor->m_NameHash)
                {
                    dmModelDDF::SetTexture* ddf = (dmModelDDF::SetTexture*)params.m_Message->m_Data;
                    uint32_t unit = ddf->m_TextureUnit;
                    dmRender::HRenderContext rendercontext = (dmRender::HRenderContext)params.m_Context;
                    dmGraphics::HRenderTarget rendertarget = dmRender::GetRenderTarget(rendercontext, ddf->m_TextureHash);
                    if (rendertarget)
                    {
                        ro->m_Textures[unit] = dmGraphics::GetRenderTargetTexture(rendertarget, dmGraphics::BUFFER_TYPE_COLOR_BIT);
                    }
                    else {
                        LogMessageError(params.m_Message, "No such render target: %s.",
                                     (const char*) dmHashReverse64(ddf->m_TextureHash, 0));
                    }
                }
                break;
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
