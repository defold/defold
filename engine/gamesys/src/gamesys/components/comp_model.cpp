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
        dmIndexPool32           m_ComponentIndices;
    };

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelWorld* model_world = new ModelWorld();
        // TODO: Expose model count in game.project
        // https://defold.fogbugz.com/default.asp?2115
        const uint32_t max_component_count = 128;
        model_world->m_Components.SetCapacity(max_component_count);
        model_world->m_Components.SetSize(max_component_count);
        memset(&model_world->m_Components[0], 0, sizeof(ModelComponent) * max_component_count);
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
        if (model_world->m_ComponentIndices.Remaining() == 0)
        {
            dmLogError("Model could not be created since the model buffer is full (%d).", model_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = model_world->m_ComponentIndices.Pop();
        ModelComponent* component = &model_world->m_Components[index];

        Model* model = (Model*)params.m_Resource;
        component->m_Instance = params.m_Instance;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Model = model;
        component->m_ModelWorld = model_world;
        component->m_RenderObject = dmRender::RenderObject();
        component->m_RenderObject.m_Material = model->m_Material;
        *params.m_UserData = (uintptr_t)component;

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        ModelComponent* component = (ModelComponent*)*params.m_UserData;
        uint32_t index = component - &model_world->m_Components[0];
        memset(component, 0, sizeof(ModelComponent));
        model_world->m_ComponentIndices.Push(index);
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
            if (component.m_Instance == 0)
                continue;
            model = component.m_Model;
            mesh = model->m_Mesh;
            dmRender::RenderObject& ro = component.m_RenderObject;

            dmTransform::TransformS1 world = dmGameObject::GetWorldTransform(component.m_Instance);
            dmTransform::TransformS1 local(Vector3(component.m_Position), component.m_Rotation, 1.0f);
            if (dmGameObject::ScaleAlongZ(component.m_Instance))
            {
                world = dmTransform::Mul(world, local);
            }
            else
            {
                world = dmTransform::MulNoScaleZ(world, local);
            }

            ro.m_Material = model->m_Material;
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                ro.m_Textures[i] = model->m_Textures[i];
            ro.m_VertexBuffer = mesh->m_VertexBuffer;
            ro.m_VertexDeclaration = mesh->m_VertexDeclaration;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = mesh->m_VertexCount;
            ro.m_TextureTransform = Matrix4::identity();
            ro.m_WorldTransform = dmTransform::ToMatrix4(world);
            ro.m_CalculateDepthKey = 1;
            dmRender::AddToRender(render_context, &component.m_RenderObject);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ModelComponent* component = (ModelComponent*)*params.m_UserData;

        dmRender::RenderObject* ro = &component->m_RenderObject;

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

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        ModelComponent* component = (ModelComponent*)*params.m_UserData;
        dmRender::Constant constant;
        bool result = dmRender::GetMaterialProgramConstant(component->m_Model->m_Material, params.m_PropertyId, constant);
        if (result)
        {
            dmRender::RenderObject* ro = &component->m_RenderObject;
            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_CONSTANT_COUNT; ++i)
            {
                dmRender::Constant& constant = ro->m_Constants[i];
                if (constant.m_Location != -1 && constant.m_NameHash == params.m_PropertyId)
                {
                    out_value.m_Variant = dmGameObject::PropertyVar(constant.m_Value);
                    return dmGameObject::PROPERTY_RESULT_OK;
                }
            }
            out_value.m_Variant = dmGameObject::PropertyVar(constant.m_Value);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        ModelComponent* component = (ModelComponent*)*params.m_UserData;
        dmRender::Constant constant;
        bool result = dmRender::GetMaterialProgramConstant(component->m_Model->m_Material, params.m_PropertyId, constant);
        if (result)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_VECTOR4)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            dmRender::RenderObject* ro = &component->m_RenderObject;
            for (uint32_t j = 0; j < dmRender::RenderObject::MAX_CONSTANT_COUNT; ++j)
            {
                const float* v = params.m_Value.m_V4;
                dmRender::EnableRenderObjectConstant(ro, params.m_PropertyId, Vector4(v[0], v[1], v[2] ,v[3]));
            }
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }
}
