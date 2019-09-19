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
#include <dlib/transform.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include "gamesys_ddf.h"
#include "mesh_ddf.h"

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
        // dmArray<dmGraphics::HVertexBuffer> m_WorldVertexBuffers;
        void*                           m_WorldVertexData;
        size_t                          m_WorldVertexDataSize;
        /// Vertex buffer used for meshes that should render vertices in world space.
        dmGraphics::HVertexBuffer       m_WorldVertexBuffer;
        // void*                           m_WorldVertexBufferData;
        // size_t                          m_WorldVertexBufferDataSize;
        dmObjectPool<MeshComponent*>    m_Components;
        dmArray<dmRender::RenderObject> m_RenderObjects;
    };

    static const uint32_t VERTEX_BUFFER_MAX_BATCHES = 16;     // Max dmRender::RenderListEntry.m_MinorOrder (4 bits)

    static const dmhash_t PROP_VERTICES = dmHashString64("vertices");

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

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
        // world->m_WorldVertexBuffers.SetCapacity(4);
        // world->m_WorldVertexBuffers.SetSize(0);
        world->m_WorldVertexData = 0x0;
        world->m_WorldVertexDataSize = 0;
        world->m_WorldVertexBuffer = dmGraphics::NewVertexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        // world->m_WorldVertexBufferData = 0x0;

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
        // dmGraphics::DeleteVertexBuffer(world->m_WorldVertexBuffer);
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

    static void ReHash(MeshComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        MeshResource* resource = component->m_Resource;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource->m_Textures[0], sizeof(resource->m_Textures[0])); // only one texture for now. Should we really support up to 32 textures per model?
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &resource->m_BufferResource, sizeof(resource->m_BufferResource));
        dmHashUpdateBuffer32(&state, &resource->m_VertexDeclaration, sizeof(resource->m_VertexDeclaration)); // TODO(andsve): does this make sense?
        ReHashRenderConstants(&component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
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
        component->m_DoRender = 0;

        ReHash(component);

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
                // FIXME(andsve): figure out if we need to do this if we change any of the resource properties?
                ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        // FIXME(andsve): figure out if the transform has been updated or not
        // update_result.m_TransformsUpdated = rig_res == dmRig::RESULT_UPDATED_POSE;
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void FillRenderObject(dmRender::RenderObject& ro,
        const dmRender::HMaterial& material,
        const dmGraphics::HTexture* textures,
        const dmGraphics::HVertexDeclaration& vert_decl,
        const dmGraphics::HVertexBuffer& vert_buffer,
        uint32_t vert_start, uint32_t vert_count,
        const Matrix4& world_transform,
        const CompRenderConstants& constants)
    {
        ro.Init();
        ro.m_VertexDeclaration = vert_decl;
        ro.m_VertexBuffer = vert_buffer;
        ro.m_Material = material;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vert_start;
        ro.m_VertexCount = vert_count;
        ro.m_WorldTransform = world_transform;

        // if(mr->m_IndexBuffer)
        // {
        //     ro.m_IndexBuffer = mr->m_IndexBuffer;
        //     ro.m_IndexType = mr->m_IndexBufferElementType;
        // }

        for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
            ro.m_Textures[i] = textures[i];

        for (uint32_t i = 0; i < constants.m_ConstantCount; ++i)
        {
            const dmRender::Constant& c = constants.m_RenderConstants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }
    }

    static inline void RenderBatchWorldVS(MeshWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatchWorld");

        // if (world->m_WorldVertexBuffers.Remaining() == 0) {
        //     world->m_WorldVertexBuffers.OffsetCapacity(4);
        // }

        // dmGraphics::HVertexBuffer vert_buffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        // world->m_WorldVertexBuffers.Push(vert_buffer);

        dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        const MeshComponent* first_component = (MeshComponent*) buf[*begin].m_UserData;
        const MeshResource* mr = first_component->m_Resource;
        const BufferResource* br = mr->m_BufferResource;

        uint32_t batch_size = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            batch_size++;
        }

        // TODO(andsve): prealloc the data buffer to avoid alloc during this loop??
        if (world->m_WorldVertexDataSize < mr->m_VertSize * br->m_ElementCount * batch_size)
        {
            if (world->m_WorldVertexData) {
                free(world->m_WorldVertexData);
            }
            world->m_WorldVertexData = malloc(mr->m_VertSize * br->m_ElementCount * batch_size);
            world->m_WorldVertexDataSize = mr->m_VertSize * br->m_ElementCount * batch_size;
        }

        float* dst_data_ptr = (float*)world->m_WorldVertexData;
        for (uint32_t *i=begin;i!=end;i++)
        {

            // dmLogError("RenderBatchLocalVS %p", i);

            const MeshComponent* component = (MeshComponent*) buf[*i].m_UserData;

            float* raw_data = 0x0;
            uint32_t size = 0;
            dmBuffer::Result r = dmBuffer::GetBytes(br->m_Buffer, (void**)&raw_data, &size);
            if (r != dmBuffer::RESULT_OK) {
                assert(false && "Megafail 1.");
            }


            // Get position stream to figure out offset and stride etc
            float* position_data = 0x0;
            uint32_t count = 0;
            uint32_t components = 0;
            uint32_t stride = 0;
            r = dmBuffer::GetStream(br->m_Buffer, mr->m_PositionStreamId, (void**)&position_data, &count, &components, &stride);
            if (r != dmBuffer::RESULT_OK) {
                assert(false && "Megafail 2.");
            }

            uint8_t* last_p = (uint8_t*)raw_data;
            uint8_t* last_dst_p = (uint8_t*)dst_data_ptr;

            Point3 in_p;
            Vector4 v;
            for (int pi = 0; pi < count; ++pi)
            {
                // Copy mem from last entry position up to this one
                uint32_t diff_size = (uint8_t*)position_data - last_p;
                if (diff_size > 0) {
                    memcpy(last_dst_p, last_p, diff_size);
                }

                // Apply world transform to all positions
                if (components == 3) {

                    in_p[0] = position_data[0];
                    in_p[1] = position_data[1];
                    in_p[2] = position_data[2];
                    v = component->m_World * in_p;
                    dst_data_ptr[0] = v[0];
                    dst_data_ptr[1] = v[1];
                    dst_data_ptr[2] = v[2];
                } else if (components == 2) {

                    in_p[0] = position_data[0];
                    in_p[1] = position_data[1];
                    in_p[2] = 0.0f;
                    v = component->m_World * in_p;
                    dst_data_ptr[0] = v[0];
                    dst_data_ptr[1] = v[1];
                } else {
                    assert(false && "Cannot apply world transform on stream with only one component.");
                }

                // Update memcpy-ptrs so they point to end of last component element.
                last_dst_p = (uint8_t*)(&dst_data_ptr[components]);
                last_p = (uint8_t*)(&position_data[components]);

                // Update in/out-ptrs with stride (they will point to next entry in float "list").
                position_data += stride;
                dst_data_ptr += stride;
            }

            // Copy any remaining data
            uint8_t* raw_data_end = (uint8_t*)raw_data + size;
            uint32_t diff_size = raw_data_end - last_p;
            if (diff_size > 0) {
                memcpy(last_dst_p, last_p, diff_size);
            }

        }

        FillRenderObject(ro, material, mr->m_Textures, mr->m_VertexDeclaration, world->m_WorldVertexBuffer, 0, mr->m_ElementCount * batch_size, Matrix4::identity(), first_component->m_RenderConstants);
        dmGraphics::SetVertexBufferData(world->m_WorldVertexBuffer, mr->m_VertSize * br->m_ElementCount * batch_size, world->m_WorldVertexData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        dmRender::AddToRender(render_context, &ro);
    }


    static inline void RenderBatchLocalVS(MeshWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatchLocal");

        for (uint32_t *i=begin;i!=end;i++)
        {
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

            FillRenderObject(ro, material, mr->m_Textures, mr->m_VertexDeclaration, mr->m_VertexBuffer, 0, mr->m_ElementCount, component->m_World, component->m_RenderConstants);
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
            case dmRenderDDF::MaterialDesc::VERTEX_SPACE_WORLD:
                RenderBatchWorldVS(world, material, render_context, buf, begin, end);
            break;

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
        SetRenderConstant(&component->m_RenderConstants, component->m_Resource->m_Material, name_hash, element_index, var);
        ReHash(component);
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
                    ReHash(component);
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

    // static inline dmGraphics::HTexture GetTexture(const MeshComponent* component, const MeshResource* resource, uint32_t index) {
    //     assert(index < MAX_TEXTURE_COUNT);
    //     return component->m_Textures[index] ? component->m_Textures[index] : resource->m_Textures[index];
    // }

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

        // for (uint32_t i = 0; i < dmRender::RenderObject::MAX_TEXTURE_COUNT; ++i)
        // {
        //     if (params.m_PropertyId == PROP_TEXTURE[i])
        //     {
        //         return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTexture(component, component->m_Resource, i), out_value);
        //     }
        // }

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
