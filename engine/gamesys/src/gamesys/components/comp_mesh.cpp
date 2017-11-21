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

#include "gamesys_ddf.h"
#include "mesh_ddf.h"

using namespace Vectormath::Aos;

namespace dmGameSystem
{
    using namespace Vectormath::Aos;
    using namespace dmGameSystemDDF;

    static const dmhash_t PROP_MESH = dmHashString64("mesh");
    static const dmhash_t PROP_TEXTURE[dmRender::RenderObject::MAX_TEXTURE_COUNT] = {
        dmHashString64("texture0"), dmHashString64("texture1"), dmHashString64("texture2"), dmHashString64("texture3"), dmHashString64("texture4"), dmHashString64("texture5"), dmHashString64("texture6"), dmHashString64("texture7"), dmHashString64("texture8"), dmHashString64("texture9"), dmHashString64("texture10"), dmHashString64("texture11"), dmHashString64("texture12"), dmHashString64("texture13"), dmHashString64("texture14"), dmHashString64("texture15"), dmHashString64("texture16"), dmHashString64("texture17"), dmHashString64("texture18"), dmHashString64("texture19"), dmHashString64("texture20"), dmHashString64("texture21"), dmHashString64("texture22"), dmHashString64("texture23"), dmHashString64("texture24"), dmHashString64("texture25"), dmHashString64("texture26"), dmHashString64("texture27"), dmHashString64("texture28"), dmHashString64("texture29"), dmHashString64("texture30"), dmHashString64("texture31")
    };

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);
    static void DestroyComponent(MeshWorld* world, uint32_t index);

    static void ReHash(MeshComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        MeshResource* resource = component->m_Resource;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource->m_Textures[0], sizeof(resource->m_Textures[0])); // only one texture for now. Should we really support up to 32 textures per model?
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t size = constants.Size();
        // Padding in the SetConstant-struct forces us to copy the components by hand
        for (uint32_t i = 0; i < size; ++i)
        {
            dmRender::Constant& c = constants[i];
            dmHashUpdateBuffer32(&state, &c.m_NameHash, sizeof(uint64_t));
            dmHashUpdateBuffer32(&state, &c.m_Value, sizeof(Vector4));
            component->m_PrevRenderConstants[i] = c.m_Value;
        }
        component->m_MixedHash = dmHashFinal32(&state);
    }

    dmGameObject::CreateResult CompMeshNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        MeshContext* context = (MeshContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        MeshWorld* world = new MeshWorld();

        world->m_ResourceFactory = context->m_Factory;
        world->m_Components.SetCapacity(context->m_MaxMeshCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxMeshCount);

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompMeshDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        // dmGraphics::DeleteVertexDeclaration(world->m_VertexDeclaration);
        // dmGraphics::DeleteVertexBuffer(world->m_VertexBuffer);

        dmResource::UnregisterResourceReloadedCallback(((MeshContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

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
        // component->m_DoRender = 0;

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void DestroyComponent(MeshWorld* world, uint32_t index)
    {
        MeshComponent* component = world->m_Components.Get(index);

        delete component;
        world->m_Components.Free(index, true);
    }

    dmGameObject::CreateResult CompMeshDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        DestroyComponent(world, *params.m_UserData);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void RenderBatch(MeshWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatch");

        const MeshComponent* first = (MeshComponent*) buf[*begin].m_UserData;
        // uint32_t vertex_count = 0;
        // uint32_t max_component_vertices = 0;
        // for (uint32_t *i=begin;i!=end;i++)
        // {
        //     const MeshComponent* c = (MeshComponent*) buf[*i].m_UserData;
        //     uint32_t count = dmRig::GetVertexCount(c->m_RigInstance);
        //     vertex_count += count;
        //     max_component_vertices = dmMath::Max(max_component_vertices, count);
        // }

        // // Early exit if there is nothing to render
        // if (vertex_count == 0) {
        //     return;
        // }

        // dmArray<dmRig::RigModelVertex> &vertex_buffer = world->m_VertexBufferData;
        // if (vertex_buffer.Remaining() < vertex_count)
        //     vertex_buffer.OffsetCapacity(vertex_count - vertex_buffer.Remaining());

        // // Fill in vertex buffer
        // dmRig::RigModelVertex *vb_begin = vertex_buffer.End();
        // dmRig::RigModelVertex *vb_end = vb_begin;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshComponent* c = (MeshComponent*) buf[*i].m_UserData;

            // if (c->m_Resource->m_Mesh->m_DataPtr != 0x0)
            if (c->m_Resource->m_VertexDeclaration != 0x0)
            {
                dmLogError("creating renderobject for mesh");
                dmRender::RenderObject& ro = *world->m_RenderObjects.End();
                world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

                ro.Init();
                ro.m_VertexDeclaration = c->m_Resource->m_VertexDeclaration;
                ro.m_VertexBuffer = c->m_Resource->m_VertexBuffer;
                ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
                ro.m_VertexStart = 0; //vb_begin - vertex_buffer.Begin();
                ro.m_VertexCount = c->m_Resource->m_Mesh->m_ElemCount;
                ro.m_Material = first->m_Resource->m_Material;
                ro.m_WorldTransform = first->m_World;
                for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
                    ro.m_Textures[i] = first->m_Resource->m_Textures[i];

                const dmArray<dmRender::Constant>& constants = first->m_RenderConstants;
                uint32_t size = constants.Size();
                for (uint32_t i = 0; i < size; ++i)
                {
                    const dmRender::Constant& c = constants[i];
                    dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
                }

                dmRender::AddToRender(render_context, &ro);
            }

        //     dmRig::HRigContext rig_context = dmGameObject::GetRigContext(dmGameObject::GetCollection(c->m_Instance));
        //     Matrix4 normal_matrix = inverse(c->m_World);
        //     normal_matrix = transpose(normal_matrix);
        //     vb_end = (dmRig::RigModelVertex *)dmRig::GenerateVertexData(rig_context, c->m_RigInstance, c->m_World, normal_matrix, Vector4(1.0), dmRig::RIG_VERTEX_FORMAT_MODEL, (void*)vb_end);
        }
        // vertex_buffer.SetSize(vb_end - vertex_buffer.Begin());

        // // Ninja in-place writing of render object.
        // dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        // world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        // ro.Init();
        // ro.m_VertexDeclaration = world->m_VertexDeclaration;
        // ro.m_VertexBuffer = world->m_VertexBuffer;
        // ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        // ro.m_VertexStart = vb_begin - vertex_buffer.Begin();
        // ro.m_VertexCount = vb_end - vb_begin;
        // ro.m_Material = first->m_Resource->m_Material;
        // ro.m_WorldTransform = first->m_World;
        // for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        //     ro.m_Textures[i] = first->m_Resource->m_Textures[i];

        // const dmArray<dmRender::Constant>& constants = first->m_RenderConstants;
        // uint32_t size = constants.Size();
        // for (uint32_t i = 0; i < size; ++i)
        // {
        //     const dmRender::Constant& c = constants[i];
        //     dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        // }

        // dmRender::AddToRender(render_context, &ro);
    }

    // void UpdateTransforms(MeshWorld* world)
    // {
    //     DM_PROFILE(Model, "UpdateTransforms");

    //     dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
    //     uint32_t n = components.Size();
    //     for (uint32_t i = 0; i < n; ++i)
    //     {
    //         MeshComponent* c = components[i];

    //         // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
    //         if (!c->m_Enabled || !c->m_AddedToUpdate)
    //             continue;

    //         if (dmRig::IsValid(c->m_RigInstance))
    //         {
    //             const Matrix4& go_world = dmGameObject::GetWorldMatrix(c->m_Instance);
    //             const Matrix4 local = dmTransform::ToMatrix4(c->m_Transform);
    //             if (dmGameObject::ScaleAlongZ(c->m_Instance))
    //             {
    //                 c->m_World = go_world * local;
    //             }
    //             else
    //             {
    //                 c->m_World = dmTransform::MulNoScaleZ(go_world, local);
    //             }
    //         }
    //     }
    // }

    dmGameObject::CreateResult CompMeshAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        MeshComponent* component = world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompMeshUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // for (uint32_t i = 0; i < count; ++i)
        // {
        //     MeshComponent& component = *components[i];
        //     component.m_DoRender = 0;

        //     if (!component.m_Enabled || !component.m_AddedToUpdate)
        //         continue;

        //     uint32_t const_count = component.m_RenderConstants.Size();
        //     for (uint32_t const_i = 0; const_i < const_count; ++const_i)
        //     {
        //         if (lengthSqr(component.m_RenderConstants[const_i].m_Value - component.m_PrevRenderConstants[const_i]) > 0)
        //         {
        //             ReHash(&component);
        //             break;
        //         }
        //     }

        //     component.m_DoRender = 1;
        // }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        MeshWorld *world = (MeshWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
            {
                world->m_RenderObjects.SetSize(0);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompMeshRender(const dmGameObject::ComponentsRenderParams& params)
    {
        MeshContext* context = (MeshContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        MeshWorld* world = (MeshWorld*)params.m_World;

        // UpdateTransforms(world);

        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < count; ++i)
        {
            MeshComponent& component = *components[i];
            // dmLogError("mesh comp stream count: %d", component.m_Resource->m_Mesh->m_Streams.m_Count);
        //     if (!component.m_DoRender)
        //         continue;

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component.m_Resource->m_Material);
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompMeshGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        MeshComponent* component = (MeshComponent*)user_data;
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t count = constants.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                *out_constant = &c;
                return true;
            }
        }
        return false;
    }

    static void CompMeshSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        MeshComponent* component = (MeshComponent*)user_data;
        dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
        uint32_t count = constants.Size();
        Vector4* v = 0x0;
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::Constant& c = constants[i];
            if (c.m_NameHash == name_hash)
            {
                v = &c.m_Value;
                break;
            }
        }
        if (v == 0x0)
        {
            if (constants.Full())
            {
                uint32_t capacity = constants.Capacity() + 4;
                constants.SetCapacity(capacity);
                component->m_PrevRenderConstants.SetCapacity(capacity);
            }
            dmRender::Constant c;
            dmRender::GetMaterialProgramConstant(component->m_Resource->m_Material, name_hash, c);
            constants.Push(c);
            component->m_PrevRenderConstants.Push(c.m_Value);
            v = &(constants[constants.Size()-1].m_Value);
        }
        if (element_index == 0x0)
            *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        else
            v->setElem(*element_index, (float)var.m_Number);
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
            // if (params.m_Message->m_Id == dmModelDDF::ModelPlayAnimation::m_DDFDescriptor->m_NameHash)
            // {
            //     dmModelDDF::ModelPlayAnimation* ddf = (dmModelDDF::ModelPlayAnimation*)params.m_Message->m_Data;
            //     if (dmRig::RESULT_OK == dmRig::PlayAnimation(component->m_RigInstance, ddf->m_AnimationId, (dmRig::RigPlayback)ddf->m_Playback, ddf->m_BlendDuration, ddf->m_Offset, ddf->m_PlaybackRate))
            //     {
            //         component->m_Listener = params.m_Message->m_Sender;
            //     }
            // }
            // else if (params.m_Message->m_Id == dmModelDDF::ModelCancelAnimation::m_DDFDescriptor->m_NameHash)
            // {
            //     dmRig::CancelAnimation(component->m_RigInstance);
            // }
            // else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            // {
            //     dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
            //     dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
            //             dmGameObject::PropertyVar(ddf->m_Value), CompMeshSetConstantCallback, component);
            //     if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
            //     {
            //         dmMessage::URL& receiver = params.m_Message->m_Receiver;
            //         dmLogError("'%s:%s#%s' has no constant named '%s'",
            //                 dmMessage::GetSocketName(receiver.m_Socket),
            //                 dmHashReverseSafe64(receiver.m_Path),
            //                 dmHashReverseSafe64(receiver.m_Fragment),
            //                 dmHashReverseSafe64(ddf->m_NameHash));
            //     }
            // }
            // else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            // {
            //     dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
            //     dmArray<dmRender::Constant>& constants = component->m_RenderConstants;
            //     uint32_t size = constants.Size();
            //     for (uint32_t i = 0; i < size; ++i)
            //     {
            //         if( constants[i].m_NameHash == ddf->m_NameHash)
            //         {
            //             constants[i] = constants[size - 1];
            //             component->m_PrevRenderConstants[i] = component->m_PrevRenderConstants[size - 1];
            //             constants.Pop();
            //             ReHash(component);
            //             break;
            //         }
            //     }
            // }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool OnResourceReloaded(MeshWorld* world, MeshComponent* component)
    {
        dmRig::HRigContext rig_context = dmGameObject::GetRigContext(dmGameObject::GetCollection(component->m_Instance));
        return true;
    }


    void CompMeshOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);
        component->m_Resource = (MeshResource*)params.m_Resource;
        (void)OnResourceReloaded(world, component);
    }

    dmGameObject::PropertyResult CompMeshGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i]) {
                out_value.m_Variant = dmGameObject::PropertyVar(component->m_Resource->m_TexturePaths[i]);
                return dmGameObject::PROPERTY_RESULT_OK;
            }
        }

        if (params.m_PropertyId == PROP_MESH) {
            dmhash_t mesh_path = 0x0;
            assert(dmResource::RESULT_OK == dmResource::GetPath(world->m_ResourceFactory, component->m_Resource, &mesh_path));
            out_value.m_Variant = dmGameObject::PropertyVar(mesh_path);
            return dmGameObject::PROPERTY_RESULT_OK;
        }

        return GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, true, CompMeshGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompMeshSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        return SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompMeshSetConstantCallback, component);
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        MeshWorld* world = (MeshWorld*) params.m_UserData;
        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            MeshComponent* component = components[i];
            if (component->m_Resource)
            {
                // if(component->m_Resource == params.m_Resource->m_Resource)
                // {
                //     // Model resource reload
                //     OnResourceReloaded(world, component);
                //     continue;
                // }
                // RigSceneResource *rig_scene_res = component->m_Resource->m_RigScene;
                // if((rig_scene_res) && (rig_scene_res->m_AnimationSetRes == params.m_Resource->m_Resource))
                // {
                //     // Model resource reload because animset used in rig was reloaded
                //     OnResourceReloaded(world, component);
                // }
            }
        }
    }

}
