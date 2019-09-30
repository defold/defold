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
        dmGameSystem::BufferResource* m_BufferResource;
        dmGraphics::HTexture        m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmRender::HMaterial         m_Material;

        // If the buffer resource property has changed (ie not same buffer resource as
        // used by the mesh resource), we need to create a specific vertex declaration
        // and vertbuffer for this buffer resource instead.
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        uint32_t                        m_ElementCount;
        uint32_t                        m_VertSize;
        // dmGraphics::HVertexBuffer       m_VertexBuffer;

        uint16_t                    m_ComponentIndex;
        /// Component enablement
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_DoRender : 1;
        /// Added to update or not
        uint8_t                     m_AddedToUpdate : 1;
        uint8_t                     m_ReHash : 1;
    };

    struct MeshWorld
    {
        dmResource::HFactory               m_ResourceFactory;
        uint32_t                           m_CurrentVertexBuffer;
        dmArray<dmGraphics::HVertexBuffer> m_VertexBuffers;
        void*                              m_WorldVertexData;
        size_t                             m_WorldVertexDataSize;
        /// Keep track of how much vertex data we have rendered so it can be
        /// reported to the profiler at end of the render dispatching.
        uint32_t                           m_RenderedVertexSize;

        dmObjectPool<MeshComponent*>       m_Components;
        dmArray<dmRender::RenderObject>    m_RenderObjects;
    };

    static const uint32_t MAX_TEXTURE_COUNT = dmRender::RenderObject::MAX_TEXTURE_COUNT;

    static const dmhash_t PROP_VERTICES = dmHashString64("vertices");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams& params);

    dmGameObject::CreateResult CompMeshNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        MeshContext* context = (MeshContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;

        MeshWorld* world = new MeshWorld();
        world->m_ResourceFactory = context->m_Factory;
        world->m_Components.SetCapacity(context->m_MaxMeshCount);
        world->m_RenderObjects.SetCapacity(context->m_MaxMeshCount);

        world->m_CurrentVertexBuffer = 0;
        world->m_VertexBuffers.SetCapacity(0);
        world->m_VertexBuffers.SetSize(0);

        world->m_WorldVertexData = 0x0;
        world->m_WorldVertexDataSize = 0;

        world->m_RenderedVertexSize = 0;

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompMeshDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        for(uint32_t i = 0; i < world->m_VertexBuffers.Size(); ++i)
        {
            dmGraphics::DeleteVertexBuffer(world->m_VertexBuffers[i]);
        }

        if (world->m_WorldVertexData)
        {
            free(world->m_WorldVertexData);
        }

        dmResource::UnregisterResourceReloadedCallback(((MeshContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline dmGameSystem::BufferResource* GetVerticesBuffer(const MeshComponent* component, const MeshResource* resource) {
        return component->m_BufferResource ? component->m_BufferResource : resource->m_BufferResource;
    }

    static inline bool HasCustomVerticesBuffer(const MeshComponent* component, const MeshResource* resource) {
        return component->m_BufferResource;
    }

    static inline dmRender::HMaterial GetMaterial(const MeshComponent* component, const MeshResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline dmGraphics::HTexture GetTexture(const MeshComponent* component, const MeshResource* resource, uint32_t index) {
        assert(index < MAX_TEXTURE_COUNT);
        return component->m_Textures[index] ? component->m_Textures[index] : resource->m_Textures[index];
    }

    static void ReHash(MeshComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        MeshResource* resource = component->m_Resource;
        dmHashInit32(&state, reverse);
        dmRender::HMaterial material = GetMaterial(component, resource);
        dmHashUpdateBuffer32(&state, &resource->m_PrimitiveType, sizeof(resource->m_PrimitiveType));
        dmHashUpdateBuffer32(&state, &material, sizeof(material));

        // We have to hash individually since we don't know which textures are set as properties
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i) {
            dmGraphics::HTexture texture = GetTexture(component, resource, i);
            dmHashUpdateBuffer32(&state, &texture, sizeof(texture));
        }
        if (HasCustomVerticesBuffer(component, resource)) {
            dmGraphics::HashVertexDeclaration(&state, component->m_VertexDeclaration);
        } else {
            dmGraphics::HashVertexDeclaration(&state, resource->m_VertexDeclaration);
        }
        ReHashRenderConstants(&component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
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
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);
        if (component->m_Material) {
            dmResource::Release(factory, component->m_Material);
        }
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i) {
            if (component->m_Textures[i]) {
                dmResource::Release(factory, component->m_Textures[i]);
            }
        }

        delete component;
        world->m_Components.Free(index, true);

        return dmGameObject::CREATE_RESULT_OK;
    }

    static void UpdateTransforms(MeshWorld* world)
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

            if (component.m_ReHash || dmGameSystem::AreRenderConstantsUpdated(&component.m_RenderConstants))
            {
                ReHash(&component);
            }

            component.m_DoRender = 1;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void FillRenderObject(dmRender::RenderObject& ro,
        const dmGraphics::PrimitiveType& primitive_type,
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
        ro.m_PrimitiveType = primitive_type;
        ro.m_VertexStart = vert_start;
        ro.m_VertexCount = vert_count;
        ro.m_WorldTransform = world_transform;

        // TODO(andsve): For future reference; we might want to have a separate buffer resource for indices.
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

    template<typename T> static void FillAndApply(const Matrix4& matrix, bool is_vector, uint32_t count, uint32_t stride, T* raw_data, T* src_stream_data, T* dst_data_ptr)
    {
        // Offset dst_data_ptr if stream isn't first!
        uint32_t ptr_offset = (uint8_t*)src_stream_data - (uint8_t*)raw_data;
        dst_data_ptr = (T*)((uint8_t*)dst_data_ptr + ptr_offset);

        Vector4 v(0.0f);
        for (int pi = 0; pi < count; ++pi)
        {
            v[0] = src_stream_data[0];
            v[1] = src_stream_data[1];
            v[2] = src_stream_data[2];
            v[3] = !is_vector;
            v = matrix * v;
            dst_data_ptr[0] = v[0];
            dst_data_ptr[1] = v[1];
            dst_data_ptr[2] = v[2];

            // Update in/out-ptrs with stride (they will point to next entry in T "list").
            src_stream_data += stride;
            dst_data_ptr += stride;
        }
    }

    static inline void FillAndApplyStream(const BufferResource* buffer_resource, const Matrix4& matrix, dmhash_t stream_id, dmBufferDDF::ValueType value_type, void* raw_data, void* dst_data_ptr)
    {
        // Get position stream to figure out offset and stride etc
        void* stream_data = 0x0;
        uint32_t count = 0;
        uint32_t components = 0;
        uint32_t stride = 0;
        dmBuffer::Result r = dmBuffer::GetStream(buffer_resource->m_Buffer, stream_id, &stream_data, &count, &components, &stride);
        if (r != dmBuffer::RESULT_OK) {
            dmLogError("Could not get stream %s from buffer when rendering mesh in world space (%d).", dmHashReverseSafe64(stream_id), r);
            return;
        }

        if (components != 3) {
            dmLogError("Rendering mesh components in world space is only supported for streams with 3 components, %s has %d components.", dmHashReverseSafe64(stream_id), components);
            return;
        }

        switch (value_type)
        {
            case dmBufferDDF::VALUE_TYPE_UINT8:
            FillAndApply<uint8_t>(matrix, 0, count, stride, (uint8_t*)raw_data, (uint8_t*)stream_data, (uint8_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_UINT16:
            FillAndApply<uint16_t>(matrix, 0, count, stride, (uint16_t*)raw_data, (uint16_t*)stream_data, (uint16_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_UINT32:
            FillAndApply<uint32_t>(matrix, 0, count, stride, (uint32_t*)raw_data, (uint32_t*)stream_data, (uint32_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_INT8:
            FillAndApply<int8_t>(matrix, 0, count, stride, (int8_t*)raw_data, (int8_t*)stream_data, (int8_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_INT16:
            FillAndApply<int16_t>(matrix, 0, count, stride, (int16_t*)raw_data, (int16_t*)stream_data, (int16_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_INT32:
            FillAndApply<int32_t>(matrix, 0, count, stride, (int32_t*)raw_data, (int32_t*)stream_data, (int32_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_FLOAT32:
            FillAndApply<float>(matrix, 0, count, stride, (float*)raw_data, (float*)stream_data, (float*)dst_data_ptr);
                break;
            default:
                assert(false && "Stream type not supported.");
                break;
        }
    }

    static inline dmGraphics::HVertexBuffer GetFreeVertexBuffer(MeshWorld* world, dmRender::HRenderContext render_context)
    {
        if (world->m_VertexBuffers.Capacity() <= world->m_CurrentVertexBuffer) {
            world->m_VertexBuffers.OffsetCapacity(4);

            dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(render_context);
            for(uint32_t i = 0; i < 4; ++i)
            {
                world->m_VertexBuffers.Push(dmGraphics::NewVertexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW));
            }
        }

        return world->m_VertexBuffers[world->m_CurrentVertexBuffer++];
    }

    static inline void RenderBatchWorldVS(MeshWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatchWorld");

        dmGraphics::HVertexBuffer vert_buffer = GetFreeVertexBuffer(world, render_context);
        assert(vert_buffer);

        dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        const MeshComponent* first = (MeshComponent*) buf[*begin].m_UserData;
        const MeshResource* mr = first->m_Resource;

        // Setup vertex declaration, count and sizes etc.
        // These defaults to values in the mesh and buffer resources,
        // but will be overwritten if the component instance has a "custom"
        // vertices buffer set (ie set via resource properties).
        dmGraphics::HVertexDeclaration vert_decl = mr->m_VertexDeclaration;
        uint32_t vert_size = mr->m_VertSize;

        // Find out how many elements/vertices all instances in this batch has
        uint32_t element_count = 0;
        int non_custom = 0;
        int custom = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshComponent* c = (MeshComponent*) buf[*i].m_UserData;
            const MeshResource* mr = c->m_Resource;
            const BufferResource* br = GetVerticesBuffer(c, mr);
            if (HasCustomVerticesBuffer(c, mr))
            {
                element_count += c->m_ElementCount;
                custom += 1;
            } else {
                element_count += br->m_ElementCount;
                non_custom += 1;
            }
        }

        // Allocate a larger scratch buffer if vert count * vert size is larger than current buffer.
        if (world->m_WorldVertexDataSize < vert_size * element_count)
        {
            if (world->m_WorldVertexData) {
                free(world->m_WorldVertexData);
            }
            world->m_WorldVertexDataSize = vert_size * element_count;
            world->m_WorldVertexData = malloc(world->m_WorldVertexDataSize);
        }

        // Fill scratch buffer with data
        void* dst_data_ptr = world->m_WorldVertexData;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshComponent* component = (MeshComponent*) buf[*i].m_UserData;
            const MeshResource* mr = component->m_Resource;
            const BufferResource* br = GetVerticesBuffer(component, mr);

            void* raw_data = 0x0;
            uint32_t size = 0;
            dmBuffer::Result r = dmBuffer::GetBytes(br->m_Buffer, &raw_data, &size);
            if (r != dmBuffer::RESULT_OK) {
                dmLogError("Could not get bytes from buffer when rendering mesh in world space (%d).", r);
                continue;
            }

            // Copy all buffer data
            memcpy(dst_data_ptr, raw_data, size);

            // Modify position stream, if specified
            if (mr->m_PositionStreamId) {
                FillAndApplyStream(br, component->m_World, mr->m_PositionStreamId, mr->m_PositionStreamType, raw_data, dst_data_ptr);
            }

            // Modify normal stream, if specified
            if (mr->m_NormalStreamId) {
                Matrix4 normal_matrix = affineInverse(component->m_World);
                normal_matrix = transpose(normal_matrix);
                FillAndApplyStream(br, normal_matrix, mr->m_NormalStreamId, mr->m_NormalStreamType, raw_data, dst_data_ptr);
            }

            dst_data_ptr = (void*)((uint8_t*)dst_data_ptr + size);
        }

        world->m_RenderedVertexSize += vert_size * element_count;

        FillRenderObject(ro, mr->m_PrimitiveType, material, mr->m_Textures, vert_decl, vert_buffer, 0, element_count, Matrix4::identity(), first->m_RenderConstants);
        dmGraphics::SetVertexBufferData(vert_buffer, vert_size * element_count, world->m_WorldVertexData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
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
            dmGameSystem::BufferResource* br = GetVerticesBuffer(component, component->m_Resource);

            // Setup vertex declaration, buffer, count and sizes etc.
            // These defaults to values in the mesh and buffer resources,
            // but will be overwritten if the component instance has a "custom"
            // vertices buffer set (ie set via resource properties).
            dmGraphics::HVertexDeclaration vert_decl = mr->m_VertexDeclaration;
            uint32_t vert_size = mr->m_VertSize;
            uint32_t elem_count = br->m_ElementCount;
            dmGraphics::HVertexBuffer vert_buf = mr->m_VertexBuffer;
            if (HasCustomVerticesBuffer(component, mr)) {
                vert_decl = component->m_VertexDeclaration;
                vert_size = component->m_VertSize;
                elem_count = component->m_ElementCount;
                vert_buf = GetFreeVertexBuffer(world, render_context);
            }
            assert(vert_buf);

            uint8_t* bytes = 0x0;
            uint32_t size = 0;
            dmBuffer::Result r = dmBuffer::GetBytes(br->m_Buffer, (void**)&bytes, &size);
            assert(r == dmBuffer::RESULT_OK);
            dmGraphics::SetVertexBufferData(vert_buf, vert_size * elem_count, bytes, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

            world->m_RenderedVertexSize += vert_size * elem_count;

            FillRenderObject(ro, mr->m_PrimitiveType, material, mr->m_Textures, vert_decl, vert_buf, 0, elem_count, component->m_World, component->m_RenderConstants);
            dmRender::AddToRender(render_context, &ro);
        }
    }

    static void RenderBatch(MeshWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Mesh, "RenderBatch");

        const MeshComponent* first = (MeshComponent*) buf[*begin].m_UserData;
        dmRender::HMaterial material = GetMaterial(first, first->m_Resource);
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
                world->m_RenderedVertexSize = 0;
                world->m_CurrentVertexBuffer = 0;
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
                DM_COUNTER("MeshVertexBuffer", world->m_RenderedVertexSize);
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

        UpdateTransforms(world);

        dmArray<MeshComponent*>& components = world->m_Components.m_Objects;
        const uint32_t count = components.Size();

        // Prepare list submit
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, world);
        dmRender::RenderListEntry* write_ptr = render_list;

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
            write_ptr->m_MinorOrder = 0;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;

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
        component->m_ReHash = 1;
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
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), CompMeshSetConstantCallback, component);
                if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
                {
                    dmMessage::URL& receiver = params.m_Message->m_Receiver;
                    dmLogError("'%s:%s#%s' has no constant named '%s'",
                            dmMessage::GetSocketName(receiver.m_Socket),
                            dmHashReverseSafe64(receiver.m_Path),
                            dmHashReverseSafe64(receiver.m_Fragment),
                            dmHashReverseSafe64(ddf->m_NameHash));
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                if (dmGameSystem::ClearRenderConstant(&component->m_RenderConstants, ddf->m_NameHash))
                {
                    component->m_ReHash = 1;
                }
            }
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompMeshOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        int index = *params.m_UserData;
        MeshComponent* component = world->m_Components.Get(index);
        component->m_Resource = (MeshResource*)params.m_Resource;
        component->m_ReHash = 1;
    }

    dmGameObject::PropertyResult CompMeshGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        if (params.m_PropertyId == PROP_VERTICES) {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetVerticesBuffer(component, component->m_Resource), out_value);
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterial(component, component->m_Resource), out_value);
        }

        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i])
            {
                return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTexture(component, component->m_Resource, i), out_value);
            }
        }

        return GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, out_value, true, CompMeshGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompMeshSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        if (params.m_PropertyId == PROP_VERTICES) {
            BufferResource* prev_buffer_resource = component->m_BufferResource;
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, BUFFER_EXT_HASH, (void**)&component->m_BufferResource);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;

            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                // If the buffer resource was changed, we might need to recreate the vertex declaration.
                if (HasCustomVerticesBuffer(component, component->m_Resource) && component->m_BufferResource != prev_buffer_resource) {

                    // Delete old vertex declaration
                    if (component->m_VertexDeclaration) {
                        dmGraphics::DeleteVertexDeclaration(component->m_VertexDeclaration);
                        component->m_VertexDeclaration = 0x0;
                    }

                    bool r = dmGameSystem::BuildVertexDeclaration(component->m_BufferResource, &component->m_VertexDeclaration, &component->m_ElementCount, &component->m_VertSize);
                    if (!r) {
                        dmLogError("Error while building vertex declaration from new resource.");
                        return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                    }
                }
            }

            return res;
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }

        for(uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            if(params.m_PropertyId == PROP_TEXTURE[i])
            {
                dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, TEXTURE_EXT_HASH, (void**)&component->m_Textures[i]);
                component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
                return res;
            }
        }

        return SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, CompMeshSetConstantCallback, component);
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
                const dmRender::HMaterial material = GetMaterial(component, component->m_Resource);
                if(component->m_Resource == params.m_Resource->m_Resource ||
                   material == params.m_Resource->m_Resource)
                {
                    component->m_ReHash = 1;
                    continue;
                }
            }
        }
    }

}
