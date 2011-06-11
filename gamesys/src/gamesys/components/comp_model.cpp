#include "comp_model.h"

#include <float.h>
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>

#include <render/render.h>

#include "../resources/res_model.h"

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

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelWorld* model_world = new ModelWorld();
        // TODO: How to configure?
        const uint32_t max_component_count = 1024;
        model_world->m_Components.SetCapacity(max_component_count);
        for (uint32_t i = 0; i < max_component_count; ++i)
        {
            model_world->m_Components.Push(ModelComponent());
        }
        model_world->m_ComponentIndices.SetCapacity(max_component_count);
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
        if (model_world->m_ComponentIndices.Remaining() > 0)
        {
            Model* model = (Model*)params.m_Resource;
            uint32_t index = model_world->m_ComponentIndices.Pop();
            ModelComponent& component = model_world->m_Components[index];
            component.m_Instance = params.m_Instance;
            component.m_Model = model;
            component.m_ModelWorld = model_world;
            component.m_Index = index;
            *params.m_UserData = (uintptr_t)&component;

            return dmGameObject::CREATE_RESULT_OK;
        }
        dmLogError("Could not create model component, out of resources.");
        return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        ModelComponent* component = (ModelComponent*)*params.m_UserData;
        component->m_Model = 0;
        model_world->m_ComponentIndices.Push(component->m_Index);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)params.m_Context;
        ModelWorld* model_world = (ModelWorld*)params.m_World;

        float min_z = FLT_MAX;
        float max_z = -FLT_MAX;
        for (uint32_t i = 0; i < model_world->m_Components.Size(); ++i)
        {
            ModelComponent& component = model_world->m_Components[i];
            if (component.m_Model != 0)
            {
                Point3 position = dmGameObject::GetWorldPosition(component.m_Instance);
                min_z = dmMath::Min(min_z, position.getZ());
                max_z = dmMath::Max(max_z, position.getZ());
            }
        }
        float z_range_recip = 1.0f / (max_z - min_z);

        for (uint32_t i = 0; i < model_world->m_Components.Size(); ++i)
        {
            ModelComponent& component = model_world->m_Components[i];
            if (component.m_Model != 0)
            {
                dmRender::RenderObject& ro = component.m_RenderObject;
                Point3 position = dmGameObject::GetWorldPosition(component.m_Instance);
                ro.m_RenderKey.m_Depth = 0xffffffff * dmMath::Clamp((position.getZ() - min_z) * z_range_recip, 0.0f, 1.0f);

                ro.m_Material = component.m_Model->m_Material;
                for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                    ro.m_Textures[i] = component.m_Model->m_Textures[i];
                ro.m_VertexBuffer = component.m_Model->m_Mesh->m_VertexBuffer;
                ro.m_VertexDeclaration = component.m_Model->m_Mesh->m_VertexDeclaration;
                ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
                ro.m_VertexStart = 0;
                ro.m_VertexCount = component.m_Model->m_Mesh->m_VertexCount;
                ro.m_TextureTransform = Matrix4::identity();
                Matrix4 world(dmGameObject::GetWorldRotation(component.m_Instance), Vector3(position));
                ro.m_WorldTransform = world;
                dmRender::AddToRender(render_context, &component.m_RenderObject);
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ModelComponent* component = (ModelComponent*)*params.m_UserData;
        dmRender::RenderObject* ro = &component->m_RenderObject;
        if (params.m_Message->m_Id == dmHashString64(dmModelDDF::SetVertexConstant::m_DDFDescriptor->m_Name))
        {
            dmModelDDF::SetVertexConstant* ddf = (dmModelDDF::SetVertexConstant*)params.m_Message->m_Data;
            dmRender::EnableRenderObjectVertexConstant(ro, ddf->m_Register, ddf->m_Value);
        }
        else if (params.m_Message->m_Id == dmHashString64(dmModelDDF::ResetVertexConstant::m_DDFDescriptor->m_Name))
        {
            dmModelDDF::ResetVertexConstant* ddf = (dmModelDDF::ResetVertexConstant*)params.m_Message->m_Data;
            dmRender::DisableRenderObjectVertexConstant(ro, ddf->m_Register);
        }
        if (params.m_Message->m_Id == dmHashString64(dmModelDDF::SetFragmentConstant::m_DDFDescriptor->m_Name))
        {
            dmModelDDF::SetFragmentConstant* ddf = (dmModelDDF::SetFragmentConstant*)params.m_Message->m_Data;
            dmRender::EnableRenderObjectFragmentConstant(ro, ddf->m_Register, ddf->m_Value);
        }
        if (params.m_Message->m_Id == dmHashString64(dmModelDDF::ResetFragmentConstant::m_DDFDescriptor->m_Name))
        {
            dmModelDDF::ResetFragmentConstant* ddf = (dmModelDDF::ResetFragmentConstant*)params.m_Message->m_Data;
            dmRender::DisableRenderObjectFragmentConstant(ro, ddf->m_Register);
        }
        else if (params.m_Message->m_Id == dmHashString64(dmModelDDF::SetTexture::m_DDFDescriptor->m_Name))
        {
            dmModelDDF::SetTexture* ddf = (dmModelDDF::SetTexture*)params.m_Message->m_Data;
            uint32_t unit = ddf->m_TextureUnit;
            dmRender::HRenderContext rendercontext = (dmRender::HRenderContext)params.m_Context;
            dmGraphics::HRenderTarget rendertarget = dmRender::GetRenderTarget(rendercontext, ddf->m_TextureHash);
            if (rendertarget)
            {
                ro->m_Textures[unit] = dmGraphics::GetRenderTargetTexture(rendertarget, dmGraphics::BUFFER_TYPE_COLOR_BIT);
            }
            else
                dmLogWarning("No such render target: 0x%llx (%llu)", ddf->m_TextureHash, ddf->m_TextureHash);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }
}
