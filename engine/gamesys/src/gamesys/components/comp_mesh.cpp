#include "comp_mesh.h"

#include <string.h>
#include <float.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/object_pool.h>
#include <dlib/math.h>
#include <dlib/vmath.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include "gamesys_ddf.h"
#include "mesh_ddf.h"

using namespace Vectormath::Aos;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    struct MeshComponent
    {
        dmGameObject::HInstance     m_Instance;
        dmTransform::Transform      m_Transform;
        Matrix4                     m_World;
        uint32_t                    m_MixedHash;
        CompRenderConstants         m_RenderConstants;
        MeshResource*               m_Resource;

        uint16_t                    m_ComponentIndex;
        /// Component enablement
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_DoRender : 1;
        /// Added to update or not
        uint8_t                     m_AddedToUpdate : 1;
    };

    struct MeshWorld
    {
        dmResource::HFactory            m_ResourceFactory;
        dmObjectPool<MeshComponent*>    m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
    };

    static const uint32_t VERTEX_BUFFER_MAX_BATCHES = 16;     // Max dmRender::RenderListEntry.m_MinorOrder (4 bits)

    static const dmhash_t PROP_VERTICES = dmHashString64("vertices");
    static const dmhash_t PROP_TEXTURE[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {
        dmHashString64("texture0"), dmHashString64("texture1"), dmHashString64("texture2"), dmHashString64("texture3"), dmHashString64("texture4"), dmHashString64("texture5"), dmHashString64("texture6"), dmHashString64("texture7"), dmHashString64("texture8"), dmHashString64("texture9"), dmHashString64("texture10"), dmHashString64("texture11"), dmHashString64("texture12"), dmHashString64("texture13"), dmHashString64("texture14"), dmHashString64("texture15"), dmHashString64("texture16"), dmHashString64("texture17"), dmHashString64("texture18"), dmHashString64("texture19"), dmHashString64("texture20"), dmHashString64("texture21"), dmHashString64("texture22"), dmHashString64("texture23"), dmHashString64("texture24"), dmHashString64("texture25"), dmHashString64("texture26"), dmHashString64("texture27"), dmHashString64("texture28"), dmHashString64("texture29"), dmHashString64("texture30"), dmHashString64("texture31")
    };

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    // static void DestroyComponent(ModelWorld* world, uint32_t index);

    dmGameObject::CreateResult CompMeshNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        MeshContext* context = (MeshContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        MeshWorld* world = new MeshWorld();

        world->m_ResourceFactory = context->m_Factory;
        world->m_Components.SetCapacity(context->m_MaxMeshCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxMeshCount);

        // dmGraphics::VertexElement ve[] =
        // {
        //         {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
        //         {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
        //         {"normal", 2, 3, dmGraphics::TYPE_FLOAT, false},
        // };
        // dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        // world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(graphics_context, ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        // world->m_MaxElementsVertices = dmGraphics::GetMaxElementsVertices(graphics_context);
        // world->m_VertexBuffers = new dmGraphics::HVertexBuffer[VERTEX_BUFFER_MAX_BATCHES];
        // world->m_VertexBufferData = new dmArray<dmRig::RigModelVertex>[VERTEX_BUFFER_MAX_BATCHES];
        // for(uint32_t i = 0; i < VERTEX_BUFFER_MAX_BATCHES; ++i)
        // {
        //     world->m_VertexBuffers[i] = dmGraphics::NewVertexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        // }

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompMeshDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        // dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        // for(uint32_t i = 0; i < VERTEX_BUFFER_MAX_BATCHES; ++i)
        // {
        //     dmGraphics::DeleteVertexBuffer(world->m_VertexBuffers[i]);
        // }

        dmResource::UnregisterResourceReloadedCallback(((MeshContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        // delete [] world->m_VertexBufferData;
        // delete [] world->m_VertexBuffers;

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

    // static void ReHash(ModelComponent* component)
    // {
    //     // Hash resource-ptr, material-handle, blend mode and render constants
    //     HashState32 state;
    //     bool reverse = false;
    //     ModelResource* resource = component->m_Resource;
    //     dmHashInit32(&state, reverse);
    //     dmHashUpdateBuffer32(&state, &resource->m_Textures[0], sizeof(resource->m_Textures[0])); // only one texture for now. Should we really support up to 32 textures per model?
    //     dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
    //     ReHashRenderConstants(&component->m_RenderConstants, &state);
    //     component->m_MixedHash = dmHashFinal32(&state);
    // }

    dmGameObject::CreateResult CompMeshCreate(const dmGameObject::ComponentCreateParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            dmLogError("Mesh could not be created since the buffer is full (%d).", world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = world->m_Components.Alloc();
        MeshComponent* component = new MeshComponent;
        memset(component, 0, sizeof(MeshComponent));
        world->m_Components.Set(index, component);
        component->m_Instance = params.m_Instance;
        component->m_Transform = dmTransform::Transform(Vector3(params.m_Position), params.m_Rotation, 1.0f);
        component->m_Resource = (MeshResource*)params.m_Resource;

        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_DoRender = 0;

        // ReHash(component);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompMeshDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        uint32_t index = (uint32_t)*params.m_UserData;
        MeshComponent* component = world->m_Components.Get(index);
        delete component;
        world->m_Components.Free(index, true);

        return dmGameObject::CREATE_RESULT_OK;
    }

    void UpdateTransforms(MeshWorld* world)
    {
        DM_PROFILE(Mesh, "UpdateTransforms");

        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            MeshComponent* c = components[i];

            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            const Matrix4& go_world = dmGameObject::GetWorldMatrix(c->m_Instance);
            const Matrix4 local = dmTransform::ToMatrix4(c->m_Transform);
            if (dmGameObject::ScaleAlongZ(c->m_Instance))
            {
                c->m_World = go_world * local;
            }
            else
            {
                c->m_World = dmTransform::MulNoScaleZ(go_world, local);
            }
        }
    }

    dmGameObject::CreateResult CompMeshAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        MeshComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompMeshUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        for (uint32_t i = 0; i < count; ++i)
        {
            MeshComponent& component = *components[i];
            component.m_DoRender = 0;

            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            if (dmGameSystem::AreRenderConstantsUpdated(&component.m_RenderConstants))
            {
                // FIXME(andsve): figure out what we need to do to rehash a mesh component.
                // ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        // FIXME(andsve): figure out if the transform has been updated or not
        // update_result.m_TransformsUpdated = rig_res == dmRig::RESULT_UPDATED_POSE;
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static inline void RenderBatchLocalVS(MeshWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatchLocal");

        for (uint32_t *i=begin;i!=end;i++)
        {
            // dmLogError("RenderBatchLocalVS %p", i);
            dmRender::RenderObject& ro = *world->m_RenderObjects.End();
            world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

            const MeshComponent* component = (MeshComponent*) buf[*i].m_UserData;
            const MeshResource* mr = component->m_Resource;
            const BufferResource* br = mr->m_BufferResource;
            assert(mr->m_VertexBuffer);

            uint8_t* bytes = 0x0;
            uint32_t size = 0;
            dmBuffer::Result r = dmBuffer::GetBytes(br->m_Buffer, (void**)&bytes, &size);
            assert(r == dmBuffer::RESULT_OK);
            dmGraphics::SetVertexBufferData(mr->m_VertexBuffer, mr->m_VertSize * br->m_ElementCount, bytes, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

            ro.Init();
            // ro.m_VertexDeclaration = world->m_VertexDeclaration;
            ro.m_VertexDeclaration = mr->m_VertexDeclaration;
            ro.m_VertexBuffer = mr->m_VertexBuffer;
            ro.m_Material = material;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = mr->m_ElementCount;
            ro.m_WorldTransform = component->m_World;

            // if(mr->m_IndexBuffer)
            // {
            //     ro.m_IndexBuffer = mr->m_IndexBuffer;
            //     ro.m_IndexType = mr->m_IndexBufferElementType;
            // }

            for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                ro.m_Textures[i] = mr->m_Textures[i];

            const CompRenderConstants& constants = component->m_RenderConstants;
            for (uint32_t i = 0; i < constants.m_ConstantCount; ++i)
            {
                const dmRender::Constant& c = constants.m_RenderConstants[i];
                dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
            }

            dmRender::AddToRender(render_context, &ro);
        }
    }

    static void RenderBatch(MeshWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatch");

        const MeshComponent* first = (MeshComponent*) buf[*begin].m_UserData;
        dmRender::HMaterial material = first->m_Resource->m_Material;
        switch(dmRender::GetMaterialVertexSpace(material))
        {
            // case dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD:
            //     RenderBatchWorldVS(world, material, render_context, buf, begin, end);
            // break;

            case dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL:
                RenderBatchLocalVS(world, material, render_context, buf, begin, end);
            break;

            default:
                assert(false);
            break;
        }
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        MeshWorld *world = (MeshWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                // dmLogError("RenderListDispatch - RENDER_LIST_OPERATION_BEGIN");
                world->m_RenderObjects.SetSize(0);
                // for (uint32_t batch_index = 0; batch_index < VERTEX_BUFFER_MAX_BATCHES; ++batch_index)
                // {
                //     world->m_VertexBufferData[batch_index].SetSize(0);
                // }
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                // dmLogError("RenderListDispatch - RENDER_LIST_OPERATION_BATCH");
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                // dmLogError("RenderListDispatch - RENDER_LIST_OPERATION_END");
    //             uint32_t total_size = 0;
    //             for (uint32_t batch_index = 0; batch_index < VERTEX_BUFFER_MAX_BATCHES; ++batch_index)
    //             {
    // //                 dmArray<dmRig::RigModelVertex>& vertex_buffer_data = world->m_VertexBufferData[batch_index];
    // //                 if (vertex_buffer_data.Empty())
    // //                 {
    // //                     continue;
    // //                 }
    // //                 uint32_t vb_size = sizeof(dmRig::RigModelVertex) * vertex_buffer_data.Size();
    // //                 dmGraphics::HVertexBuffer& gfx_vertex_buffer = world->m_VertexBuffers[batch_index];
    // //                 dmGraphics::SetVertexBufferData(gfx_vertex_buffer, vb_size, vertex_buffer_data.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    // //                 total_size += vb_size;
    //             }
    //             DM_COUNTER("MeshVertexBuffer", total_size);

                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompMeshRender(const dmGameObject::ComponentsRenderParams& params)
    {
        ModelContext* context = (ModelContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        MeshWorld* world = (MeshWorld*)params.m_World;

        UpdateTransforms(world);

        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        uint32_t minor_order = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            MeshComponent& component = *components[i];
            if (!component.m_DoRender)
                continue;

            uint64_t vertex_count = component.m_Resource->m_BufferResource->m_ElementCount;

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component.m_Resource->m_Material);
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MinorOrder = minor_order;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;


            // dmLogError("CompMeshRender - render %d", i);
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompMeshGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        MeshComponent* component = (MeshComponent*)user_data;
        return GetRenderConstant(&component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompMeshSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        MeshComponent* component = (MeshComponent*)user_data;
        // SetRenderConstant(&component->m_RenderConstants, component->m_Resource->m_Material, name_hash, element_index, var);
        // FIXME(andsve): rehash again
        // ReHash(component);
    }

    dmGameObject::UpdateResult CompMeshOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);
        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                // dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                // dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
                //         dmGameObject::PropertyVar(ddf->m_Value), CompMeshSetConstantCallback, component);
                // if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
                // {
                //     dmMessage::URL& receiver = params.m_Message->m_Receiver;
                //     dmLogError("'%s:%s#%s' has no constant named '%s'",
                //             dmMessage::GetSocketName(receiver.m_Socket),
                //             dmHashReverseSafe64(receiver.m_Path),
                //             dmHashReverseSafe64(receiver.m_Fragment),
                //             dmHashReverseSafe64(ddf->m_NameHash));
                // }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                if (dmGameSystem::ClearRenderConstant(&component->m_RenderConstants, ddf->m_NameHash))
                {
                    // ReHash(component);
                }
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompMeshOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        // int index = *params.m_UserData;
        // ModelComponent* component = world->m_Components.Get(index);
        // component->m_Resource = (ModelResource*)params.m_Resource;
        // (void)OnResourceReloaded(world, component, index);
    }

    dmGameObject::PropertyResult CompMeshGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        if (params.m_PropertyId == PROP_VERTICES) {
            dmhash_t mesh_path = 0x0;
            dmResource::Result r = dmResource::GetPath(world->m_ResourceFactory, component->m_Resource->m_BufferResource, &mesh_path);
            assert(r == dmResource::RESULT_OK);
            out_value.m_Variant = dmGameObject::PropertyVar(mesh_path);
            return dmGameObject::PROPERTY_RESULT_OK;
        }

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i]) {
                // FIXME(andsve)
                // out_value.m_Variant = dmGameObject::PropertyVar(component->m_Resource->m_TexturePaths[i]);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }

        // return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, true, CompMeshGetConstantCallback, component);
        return dmGameObject::PropertyResult::PROPERTY_RESULT_NOT_FOUND;
    }

    dmGameObject::PropertyResult CompMeshSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        // return SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompMeshSetConstantCallback, component);
        return dmGameObject::PropertyResult::PROPERTY_RESULT_NOT_FOUND;
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        MeshWorld* world = (MeshWorld*) params.m_UserData;
        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        // uint32_t n = components.Size();
        // for (uint32_t i = 0; i < n; ++i)
        // {
        //     MeshComponent* component = components[i];
        //     if (component->m_Resource)
        //     {
        //         if(component->m_Resource == params.m_Resource->m_Resource)
        //         {
        //             // Model resource reload
        //             OnResourceReloaded(world, component, i);
        //             continue;
        //         }
        //         RigSceneResource *rig_scene_res = component->m_Resource->m_RigScene;
        //         if((rig_scene_res) && (rig_scene_res->m_AnimationSetRes == params.m_Resource->m_Resource))
        //         {
        //             // Model resource reload because animset used in rig was reloaded
        //             OnResourceReloaded(world, component, i);
        //         }
        //     }
        // }
    }

}
