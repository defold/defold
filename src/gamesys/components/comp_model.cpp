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
    struct ModelWorld
    {
        dmArray<dmRender::HRenderObject> m_RenderObjects;
        dmIndexPool32 m_ROIndices;
    };

    struct ModelUserData
    {
        ModelWorld* m_ModelWorld;
        uint32_t m_Index;
    };

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
        ModelWorld* model_world = new ModelWorld();
        // TODO: How to configure?
        const uint32_t max_render_object_count = 1024;
        model_world->m_RenderObjects.SetCapacity(max_render_object_count);
        model_world->m_RenderObjects.SetSize(max_render_object_count);
        memset((void*)&model_world->m_RenderObjects[0], 0, max_render_object_count * sizeof(dmRender::HRenderObject));
        model_world->m_ROIndices.SetCapacity(max_render_object_count);
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
        if (model_world->m_ROIndices.Remaining() > 0)
        {
            dmModel::HModel prototype = (dmModel::HModel)resource;
            dmRender::HRenderObject ro = dmRender::NewRenderObject(g_ModelRenderType, dmModel::GetMaterial(prototype), (void*)instance);
            dmRender::SetUserData(ro, prototype);
            uint32_t index = model_world->m_ROIndices.Pop();
            model_world->m_RenderObjects[index] = ro;
            ModelUserData* model_user_data = new ModelUserData();
            model_user_data->m_ModelWorld = model_world;
            model_user_data->m_Index = index;
            *user_data = (uintptr_t)model_user_data;

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
        ModelUserData* model_user_data = (ModelUserData*)*user_data;
        model_world->m_ROIndices.Push(model_user_data->m_Index);
        dmRender::DeleteRenderObject(model_world->m_RenderObjects[model_user_data->m_Index]);
        model_world->m_RenderObjects[model_user_data->m_Index] = dmRender::INVALID_RENDER_OBJECT_HANDLE;

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelUpdate(dmGameObject::HCollection collection,
                         const dmGameObject::UpdateContext* update_context,
                         void* world,
                         void* context)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        ModelWorld* model_world = (ModelWorld*)world;
        for (uint32_t i = 0; i < model_world->m_RenderObjects.Size(); ++i)
        {
            if (model_world->m_RenderObjects[i] != dmRender::INVALID_RENDER_OBJECT_HANDLE)
                dmRender::AddToRender(render_context, model_world->m_RenderObjects[i]);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompModelOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        if (message_data->m_MessageId == dmHashString32(dmRender::SetRenderColor::m_DDFDescriptor->m_ScriptName))
        {
            ModelUserData* model_user_data = (ModelUserData*)*user_data;
            dmRender::HRenderObject ro = model_user_data->m_ModelWorld->m_RenderObjects[model_user_data->m_Index];
            dmRender::SetRenderColor* ddf = (dmRender::SetRenderColor*)message_data->m_Buffer;
            dmRender::ColorType color_type = (dmRender::ColorType)ddf->m_ColorType;
            dmRender::SetColor(ro, ddf->m_Color, color_type);
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
        dmModel::HModel model = (dmModel::HModel)dmRender::GetUserData(ro);
        Quat rotation = dmRender::GetRotation(ro);
        Point3 position = dmRender::GetPosition(ro);

        dmGraphics::HMaterial material = dmModel::GetMaterial(model);

        dmModel::HMesh mesh = dmModel::GetMesh(model);
        if (dmModel::GetPrimitiveCount(mesh) == 0)
            return;

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);

        // TODO: replace this dynamic texture thingy with proper indexing next
        if (dmModel::GetDynamicTexture0(model))
            dmGraphics::SetTexture(graphics_context, dmModel::GetDynamicTexture0(model));
        else
            dmGraphics::SetTexture(graphics_context, dmModel::GetTexture0(model));

        for (uint32_t i=0; i<dmGraphics::MAX_MATERIAL_CONSTANTS; i++)
        {
            uint32_t mask = dmGraphics::GetMaterialFragmentConstantMask(material);
            if (mask & (1 << i) )
            {
                Vector4 v = dmGraphics::GetMaterialFragmentProgramConstant(material, i);
                dmGraphics::SetFragmentConstant(graphics_context, &v, i);
            }
        }

        Vector4 diffuse_color = dmRender::GetColor(ro, dmRender::DIFFUSE_COLOR);
        dmGraphics::SetFragmentConstant(graphics_context, &diffuse_color, 0);

        Matrix4 m(rotation, Vector3(position));

        dmGraphics::SetVertexConstantBlock(graphics_context, (const Vector4*)dmRender::GetViewProjectionMatrix(render_context), 0, 4);
        dmGraphics::SetVertexConstantBlock(graphics_context, (const Vector4*)&m, 4, 4);

        Matrix4 texturmatrix;
        texturmatrix = Matrix4::identity();
        dmGraphics::SetVertexConstantBlock(graphics_context, (const Vector4*)&texturmatrix, 8, 4);


        for (uint32_t i=0; i<dmGraphics::MAX_MATERIAL_CONSTANTS; i++)
        {
            uint32_t mask = dmGraphics::GetMaterialVertexConstantMask(material);
            if (mask & (1 << i) )
            {
                Vector4 v = dmGraphics::GetMaterialVertexProgramConstant(material, i);
                dmGraphics::SetVertexConstantBlock(graphics_context, &v, i, 1);
            }
        }

        dmGraphics::EnableVertexDeclaration(graphics_context, dmModel::GetVertexDeclarationBuffer(mesh), dmModel::GetVertexBuffer(mesh));
        dmGraphics::DrawRangeElements(graphics_context, dmGraphics::PRIMITIVE_TRIANGLES, 0, dmModel::GetPrimitiveCount(mesh), dmGraphics::TYPE_UNSIGNED_INT, dmModel::GetIndexBuffer(mesh));
        dmGraphics::DisableVertexDeclaration(graphics_context, dmModel::GetVertexDeclarationBuffer(mesh));
    }

    void RenderTypeModelEnd(const dmRender::HRenderContext render_context, void* user_context)
    {
    }
}
