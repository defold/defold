#include "comp_model.h"

#include <float.h>
#include <dlib/array.h>
#include <dlib/object_pool.h>
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
        , m_AddedToUpdate(0)
        , m_Padding(0)
        {}

        Model* m_Model;
        dmGameObject::HInstance m_Instance;
        Point3 m_Position;
        Quat   m_Rotation;
        dmRender::RenderObject m_RenderObject;
        ModelWorld* m_ModelWorld;
        uint16_t m_AddedToUpdate : 1;
        uint16_t m_Padding : 15;
    };

    struct ModelWorld
    {
        dmObjectPool<ModelComponent> m_Components;
    };

    dmGameObject::CreateResult CompModelNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        ModelWorld* model_world = new ModelWorld();
        // TODO: Expose model count in game.project
        // https://defold.fogbugz.com/default.asp?2115
        const uint32_t max_component_count = 128;
        model_world->m_Components.SetCapacity(max_component_count);
        memset(model_world->m_Components.m_Objects.Begin(), 0, sizeof(ModelComponent) * max_component_count);
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
        if (model_world->m_Components.Full())
        {
            dmLogError("Model could not be created since the model buffer is full (%d).", model_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = model_world->m_Components.Alloc();
        ModelComponent* component = &model_world->m_Components.Get(index);

        Model* model = (Model*)params.m_Resource;
        component->m_Instance = params.m_Instance;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Model = model;
        component->m_ModelWorld = model_world;
        component->m_RenderObject = dmRender::RenderObject();
        component->m_RenderObject.m_Material = model->m_Material;
        *params.m_UserData = (uintptr_t)index;

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        model_world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompModelAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        ModelWorld* model_world = (ModelWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        ModelComponent* component = &model_world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        for (uint32_t* i=params.m_Begin;i!=params.m_End;i++)
        {
            ModelComponent& component = *((ModelComponent*) params.m_Buf[*i].m_UserData);
            Model* model = component.m_Model;
            Mesh* mesh = model->m_Mesh;

            dmRender::RenderObject& ro = component.m_RenderObject;
            dmTransform::Transform world = dmGameObject::GetWorldTransform(component.m_Instance);
            dmTransform::Transform local(Vector3(component.m_Position), component.m_Rotation, 1.0f);
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
            dmRender::AddToRender(params.m_Context, &component.m_RenderObject);
        }
    }

    dmGameObject::UpdateResult CompModelRender(const dmGameObject::ComponentsRenderParams& params)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)params.m_Context;
        ModelWorld* world = (ModelWorld*)params.m_World;
        dmArray<ModelComponent>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();

        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, n);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < n; ++i)
        {
            ModelComponent& component = components[i];
            if (component.m_Instance == 0 || !component.m_AddedToUpdate)
                continue;

            write_ptr->m_WorldPosition = dmGameObject::GetWorldPosition(component.m_Instance);
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component.m_Model->m_Material);
            write_ptr->m_BatchKey = 0;
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        ModelWorld* world = (ModelWorld*) params.m_World;
        ModelComponent* component = &world->m_Components.Get(*params.m_UserData);

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

    static bool CompModelGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        ModelComponent* component = (ModelComponent*)user_data;
        dmRender::RenderObject* ro = &component->m_RenderObject;
        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_CONSTANT_COUNT; ++i)
        {
            dmRender::Constant& constant = ro->m_Constants[i];
            if (constant.m_Location != -1 && constant.m_NameHash == name_hash)
            {
                *out_constant = &constant;
                return true;
            }
        }
        return false;
    }

    static void CompModelSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        ModelComponent* component = (ModelComponent*)user_data;
        dmRender::RenderObject* ro = &component->m_RenderObject;
        Vector4 val;
        if (element_index == 0x0)
        {
            val = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2] ,var.m_V4[3]);
        }
        else
        {
            Vector4* v = 0x0;
            for (uint32_t j = 0; j < dmRender::RenderObject::MAX_CONSTANT_COUNT; ++j)
            {
                dmRender::Constant* c = &ro->m_Constants[j];
                if (c->m_Location != -1 && c->m_NameHash == name_hash)
                {
                    v = &c->m_Value;
                    break;
                }
            }
            if (v == 0x0)
            {
                dmRender::Constant c;
                dmRender::GetMaterialProgramConstant(component->m_Model->m_Material, name_hash, c);
                val = c.m_Value;
            }
            val.setElem(*element_index, var.m_Number);
        }
        dmRender::EnableRenderObjectConstant(ro, name_hash, val);
    }

    dmGameObject::PropertyResult CompModelGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        ModelWorld* world = (ModelWorld*) params.m_World;
        ModelComponent* component = &world->m_Components.Get(*params.m_UserData);
        return GetMaterialConstant(component->m_Model->m_Material, params.m_PropertyId, out_value, CompModelGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompModelSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        ModelWorld* world = (ModelWorld*) params.m_World;
        ModelComponent* component = &world->m_Components.Get(*params.m_UserData);
        return SetMaterialConstant(component->m_Model->m_Material, params.m_PropertyId, params.m_Value, CompModelSetConstantCallback, component);
    }
}
