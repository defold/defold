// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "comp_mesh.h"

#include <string.h>
#include <float.h>
#include <stdint.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/object_pool.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/transform.h>
#include <dmsdk/dlib/vmath.h>
#include <gameobject/gameobject_ddf.h>
#include <graphics/graphics.h>
#include <render/render.h>

#include "../gamesys_private.h"
#include "comp_private.h"

#include <gamesys/gamesys_ddf.h>
#include <gamesys/mesh_ddf.h>
#include <dmsdk/gamesys/render_constants.h>
#include <dmsdk/gamesys/resources/res_material.h>
#include <dmsdk/resource/resource.h>

#include "../resources/res_mesh.h"

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Mesh, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_MeshVertexCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# vertices", &rmtp_Mesh);
DM_PROPERTY_U32(rmtp_MeshVertexSize, 0, PROFILE_PROPERTY_FRAME_RESET, "size of vertices in bytes", &rmtp_Mesh);

namespace dmGameSystem
{
    using namespace dmVMath;
    using namespace dmGameSystemDDF;

    static const char* MESH_MAX_COUNT_KEY = "mesh.max_count";

    struct MeshComponent
    {
        dmGameObject::HInstance         m_Instance;
        dmRender::HRenderItem           m_RenderItem;
        Matrix4                         m_Local;
        Matrix4                         m_World;
        uint32_t                        m_MixedHash;
        HComponentRenderConstants       m_RenderConstants;
        MeshResource*                   m_Resource;
        dmGameSystem::BufferResource*   m_BufferResource;
        TextureResource*                m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        MaterialResource*               m_Material;

        // If the buffer resource property has changed (ie not same buffer resource as
        // used by the mesh resource), we need to create a specific vertex declaration
        // and vertbuffer for this buffer resource instead.
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmBuffer::HBuffer               m_VertexDeclarationBuffer;
        dmhash_t                        m_VertexBufferResource;
        uint32_t                        m_BufferVersion;

        /// Component enablement
        uint8_t                         m_Enabled : 1;
        /// Added to update or not
        uint8_t                         m_AddedToUpdate : 1;
        uint8_t                         m_ReHash : 1;
        uint8_t                         :5;
    };

    struct VertexBufferInfo
    {
        dmGraphics::HVertexBuffer m_VertexBuffer;
        uint32_t m_RefCount;
        uint32_t m_Version;
    };

    struct MeshAttributeBinding
    {
        dmArray<uint32_t>              m_AttributeIndices;
        dmArray<dmGraphics::VertexAttributeInfo> m_Attributes;
        dmGraphics::HVertexDeclaration m_BaseDeclaration;
        dmGraphics::HVertexDeclaration m_VertexDeclaration;
        dmGraphics::HVertexDeclaration m_InstanceDeclaration;
        dmhash_t                       m_Hash;
        bool                           m_Used;
        bool                           m_HasNormalMatrix;
    };

    struct MeshWorld
    {
        dmRender::HRenderContext           m_RenderContext;
        dmRender::HRenderItemDispatch      m_RenderDispatch;
        dmResource::HFactory               m_ResourceFactory;
        dmObjectPool<MeshComponent*>       m_Components;
        dmArray<dmRender::RenderObject>    m_RenderObjects;
        dmHashTable64<VertexBufferInfo>    m_ResourceToVertexBuffer;
        dmArray<dmGraphics::HVertexBuffer> m_VertexBufferPool;
        dmArray<dmGraphics::HVertexBuffer> m_VertexBufferWorld; // for meshes batched in world space
        dmGraphics::HContext               m_GraphicsContext;
        void*                              m_WorldVertexData;
        size_t                             m_WorldVertexDataSize;
        dmArray<MeshAttributeBinding*>     m_AttributeBindings;
        dmArray<uint8_t>                   m_AttributeData;
        dmRender::HBufferedRenderBuffer    m_AttributeBuffer;
        uint32_t                           m_AttributeBufferDispatchCount;
    };

    struct MeshContext
    {
        MeshContext()
        {
            memset(this, 0, sizeof(*this));
        }
        dmRender::HRenderContext    m_RenderContext;
        dmResource::HFactory        m_Factory;
        uint32_t                    m_MaxMeshCount;
    };

    static inline void DeallocVertexBuffer(MeshWorld* world, dmGraphics::HVertexBuffer vertex_buffer)
    {
        if (world->m_VertexBufferPool.Full())
            world->m_VertexBufferPool.OffsetCapacity(4);
        world->m_VertexBufferPool.Push(vertex_buffer);
    }

    static inline dmGraphics::HVertexBuffer AllocVertexBuffer(MeshWorld* world, dmGraphics::HContext graphics_context)
    {
        if (!world->m_VertexBufferPool.Empty())
        {
            dmGraphics::HVertexBuffer vertex_buffer = world->m_VertexBufferPool[world->m_VertexBufferPool.Size()-1];
            world->m_VertexBufferPool.SetSize(world->m_VertexBufferPool.Size()-1);
            return vertex_buffer;
        }

        // TODO: perhaps control stream vs draw better?
        return dmGraphics::NewVertexBuffer(graphics_context, 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
    }

    static VertexBufferInfo* GetVertexBufferInfo(MeshWorld* world, dmhash_t path)
    {
        VertexBufferInfo* info = world->m_ResourceToVertexBuffer.Get(path);
        return info;
    }

    static dmGraphics::HVertexBuffer GetVertexBuffer(MeshWorld* world, dmhash_t path)
    {
        VertexBufferInfo* info = world->m_ResourceToVertexBuffer.Get(path);
        if (!info)
            return 0;
        return info->m_VertexBuffer;
    }

    static void AddVertexBufferInfo(MeshWorld* world, dmhash_t path, dmGraphics::HVertexBuffer vertex_buffer, uint32_t version)
    {
        VertexBufferInfo info;
        info.m_RefCount = 1;
        info.m_VertexBuffer = vertex_buffer;
        info.m_Version = version;
        if (world->m_ResourceToVertexBuffer.Full()) {
            uint32_t capacity = world->m_ResourceToVertexBuffer.Capacity() + 8;
            world->m_ResourceToVertexBuffer.SetCapacity(capacity/3, capacity);
        }
        world->m_ResourceToVertexBuffer.Put(path, info);
    }

    static void DecRefVertexBuffer(MeshWorld* world, dmhash_t path)
    {
        VertexBufferInfo* info = world->m_ResourceToVertexBuffer.Get(path);
        assert(info != 0);

        info->m_RefCount--;
        if (info->m_RefCount == 0) {
            dmGraphics::HVertexBuffer vertex_buffer = info->m_VertexBuffer;
            world->m_ResourceToVertexBuffer.Erase(path);
            DeallocVertexBuffer(world, vertex_buffer);
        }
    }

    static void IncRefVertexBuffer(MeshWorld* world, dmhash_t path)
    {
        VertexBufferInfo* info = world->m_ResourceToVertexBuffer.Get(path);
        assert(info != 0);
        info->m_RefCount++;
    }

    static uint32_t CalcBufferVersion(const MeshComponent* component, BufferResource* br)
    {
        HashState32 state;
        dmHashInit32(&state, false);

        uint32_t version;
        dmBuffer::GetContentVersion(br->m_Buffer, &version);
        dmHashUpdateBuffer32(&state, &br->m_Buffer, sizeof(br->m_Buffer)); // The handle is not a pointer, but a version+index
        dmHashUpdateBuffer32(&state, &version, sizeof(version));

        return dmHashFinal32(&state);
    }

    static void CopyBufferToVertexBuffer(dmBuffer::HBuffer buffer, dmGraphics::HVertexBuffer vertex_buffer, uint32_t vert_size, uint32_t elem_count, dmGraphics::BufferUsage buffer_usage)
    {
        uint8_t* bytes = 0x0;
        uint32_t size = 0;
        dmBuffer::Result r = dmBuffer::GetBytes(buffer, (void**)&bytes, &size);
        assert(r == dmBuffer::RESULT_OK);

        dmGraphics::SetVertexBufferData(vertex_buffer, vert_size * elem_count, bytes, buffer_usage);
    }

    static void CreateVertexBuffer(MeshWorld* world, dmGameSystem::BufferResource* br)
    {
        dmGraphics::HVertexBuffer vertex_buffer = GetVertexBuffer(world, br->m_NameHash);
        if (!vertex_buffer)
        {
            vertex_buffer = AllocVertexBuffer(world, world->m_GraphicsContext);
            AddVertexBufferInfo(world, br->m_NameHash, vertex_buffer, 0); // ref count == 1
        }
        else
        {
            IncRefVertexBuffer(world, br->m_NameHash);
        }
    }

    static void UpdateVertexBuffer(MeshWorld* world, dmGameSystem::BufferResource* br, uint32_t version)
    {
        VertexBufferInfo* info = GetVertexBufferInfo(world, br->m_NameHash);
        if (info->m_Version != version)
        {
            info->m_Version = version;
            CopyBufferToVertexBuffer(br->m_Buffer, info->m_VertexBuffer, br->m_Stride, br->m_ElementCount, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        }
    }

    static const uint32_t MAX_TEXTURE_COUNT = dmRender::RenderObject::MAX_TEXTURE_COUNT;

    static const dmhash_t PROP_VERTICES = dmHashString64("vertices");

    static const uint64_t AABB_HASH = dmHashString64("AABB");

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params);
    static void RenderListDispatch(const dmRender::RenderListDispatchParams& params);

    static void DeleteAttributeBinding(MeshAttributeBinding* binding)
    {
        if (binding->m_BaseDeclaration)
            dmGraphics::DeleteVertexDeclaration(binding->m_BaseDeclaration);
        if (binding->m_VertexDeclaration)
            dmGraphics::DeleteVertexDeclaration(binding->m_VertexDeclaration);
        if (binding->m_InstanceDeclaration)
            dmGraphics::DeleteVertexDeclaration(binding->m_InstanceDeclaration);
        delete binding;
    }

    dmGameObject::CreateResult CompMeshNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        MeshContext* context = (MeshContext*)params.m_Context;

        MeshWorld* world = new MeshWorld();
        world->m_RenderContext = context->m_RenderContext;
        world->m_RenderDispatch = 0;
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, context->m_MaxMeshCount);
        world->m_ResourceFactory = context->m_Factory;
        world->m_Components.SetCapacity(comp_count);
        world->m_RenderObjects.SetCapacity(comp_count);

        world->m_VertexBufferPool.SetCapacity(0);
        world->m_VertexBufferPool.SetSize(0);

        world->m_WorldVertexData = 0x0;
        world->m_WorldVertexDataSize = 0;

        world->m_GraphicsContext = dmRender::GetGraphicsContext(context->m_RenderContext);
        world->m_AttributeBuffer = dmRender::NewBufferedRenderBuffer(context->m_RenderContext, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
        world->m_AttributeBufferDispatchCount = 0;

        *params.m_World = world;

        dmResource::RegisterResourceReloadedCallback(context->m_Factory, ResourceReloadedCallback, world);

        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompMeshDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        if (world->m_RenderDispatch != 0)
            dmRender::UnregisterRenderListDispatch(world->m_RenderContext, world->m_RenderDispatch);

        if (world->m_VertexBufferPool.Capacity() < world->m_VertexBufferPool.Size()+world->m_VertexBufferWorld.Size())
            world->m_VertexBufferPool.OffsetCapacity(world->m_VertexBufferWorld.Size());
        world->m_VertexBufferPool.PushArray(world->m_VertexBufferWorld.Begin(), world->m_VertexBufferWorld.Size());
        world->m_VertexBufferWorld.SetSize(0);

        for(uint32_t i = 0; i < world->m_VertexBufferPool.Size(); ++i)
        {
            dmGraphics::DeleteVertexBuffer(world->m_VertexBufferPool[i]);
        }

        if (world->m_WorldVertexData)
        {
            free(world->m_WorldVertexData);
        }

        for (uint32_t i = 0; i < world->m_AttributeBindings.Size(); ++i)
            DeleteAttributeBinding(world->m_AttributeBindings[i]);
        dmRender::DeleteBufferedRenderBuffer(world->m_RenderContext, world->m_AttributeBuffer);

        dmResource::UnregisterResourceReloadedCallback(((MeshContext*)params.m_Context)->m_Factory, ResourceReloadedCallback, world);

        delete world;

        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline dmGameSystem::BufferResource* GetBufferResource(const MeshComponent* component)
    {
        return component->m_BufferResource ? component->m_BufferResource : component->m_Resource->m_BufferResource;
    }

    static inline MaterialResource* GetMaterialResource(const MeshComponent* component, const MeshResource* resource)
    {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline dmRender::HMaterial GetMaterial(const MeshComponent* component, const MeshResource* resource)
    {
        return GetMaterialResource(component, resource)->m_Material;
    }

    static TextureResource* GetTextureResource(const MeshComponent* component, uint32_t texture_unit)
    {
        assert(texture_unit < MAX_TEXTURE_COUNT);
        TextureResource* texture;
        // Overridden textures
        texture = component->m_Textures[texture_unit];
        if (texture)
            return texture;
        // Overridden material
        MaterialResource* material = component->m_Material;
        if (material)
            texture = material->m_Textures[texture_unit];
        if (texture)
            return texture;
        // resource textures
        texture = component->m_Resource->m_Textures[texture_unit];
        if (texture)
            return texture;
        // resource material
        material = component->m_Resource->m_Material;
        if (material)
            texture = material->m_Textures[texture_unit];
        if (texture)
            return texture;
        return 0;
    }

    static inline dmGraphics::HTexture GetTexture(const MeshComponent* component, uint32_t index)
    {
        TextureResource* texture_res = GetTextureResource(component, index);
        return texture_res ? texture_res->m_Texture : 0;
    }

    static inline dmGraphics::HVertexDeclaration GetVertexDeclaration(const MeshComponent* component) {
        return component->m_VertexDeclaration ? component->m_VertexDeclaration : component->m_Resource->m_VertexDeclaration;
    }

    static void UpdateComponentVertexDeclaration(MeshComponent* component)
    {
        BufferResource* buffer = GetBufferResource(component);
        if (component->m_VertexDeclarationBuffer != buffer->m_Buffer)
        {
            dmGraphics::HVertexDeclaration declaration;
            if (!BuildVertexDeclaration(buffer, &declaration))
                return;
            if (component->m_VertexDeclaration)
                dmGraphics::DeleteVertexDeclaration(component->m_VertexDeclaration);
            component->m_VertexDeclaration = declaration;
            component->m_VertexDeclarationBuffer = buffer->m_Buffer;
            component->m_ReHash = 1;
        }
    }

    static void UpdateComponentVertexBuffer(MeshWorld* world, MeshComponent* component)
    {
        UpdateComponentVertexDeclaration(component);
        BufferResource* buffer = GetBufferResource(component);
        // Track ownership independently of the component material. A render-script
        // material override can also require local geometry for a world-space mesh.
        if (component->m_VertexBufferResource != buffer->m_NameHash)
        {
            if (component->m_VertexBufferResource)
                DecRefVertexBuffer(world, component->m_VertexBufferResource);
            CreateVertexBuffer(world, buffer);
            component->m_VertexBufferResource = buffer->m_NameHash;
        }
        component->m_BufferVersion = CalcBufferVersion(component, buffer);
        UpdateVertexBuffer(world, buffer, component->m_BufferVersion);
    }

    static void ReHash(MeshComponent* component)
    {
        // Hash resource-ptr, material-handle, textures and render constants
        HashState32 state;
        bool reverse = false;
        MeshResource* resource = component->m_Resource;
        dmHashInit32(&state, reverse);
        dmRender::HMaterial material = GetMaterial(component, resource);
        dmHashUpdateBuffer32(&state, &resource->m_PrimitiveType, sizeof(resource->m_PrimitiveType));
        dmHashUpdateBuffer32(&state, &material, sizeof(material));

        // We have to hash individually since we don't know which textures are set as properties
        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            dmGraphics::HTexture texture = GetTexture(component, i);
            dmHashUpdateBuffer32(&state, &texture, sizeof(texture));
        }

        BufferResource* br = GetBufferResource(component);
        dmHashUpdateBuffer32(&state, &br->m_NameHash, sizeof(br->m_NameHash));

        // Make sure there is a vertex declaration
        // If the mesh uses a buffer that has zero elements we couldn't
        // create a vert declaration, so just skip hashing it here.
        dmGraphics::HVertexDeclaration vert_decl = GetVertexDeclaration(component);
        if (vert_decl) {
            dmGraphics::HashVertexDeclaration(&state, vert_decl);
        }
        if (component->m_RenderConstants)
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    dmGameObject::CreateResult CompMeshCreate(const dmGameObject::ComponentCreateParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        if (world->m_Components.Full())
        {
            ShowFullBufferError("Mesh", MESH_MAX_COUNT_KEY, world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_TOO_MANY_COMPONENTS;
        }
        uint32_t index = world->m_Components.Alloc();
        MeshComponent* component = new MeshComponent;
        memset(component, 0, sizeof(MeshComponent));
        world->m_Components.Set(index, component);
        component->m_Instance = params.m_Instance;
        component->m_Local = Matrix4(params.m_Rotation, Vector3(params.m_Position));
        component->m_Resource = (MeshResource*)params.m_Resource;

        component->m_Enabled = 1;
        component->m_World = Matrix4::identity();
        component->m_BufferVersion = 0;
        component->m_VertexDeclarationBuffer = GetBufferResource(component)->m_Buffer;

        const Matrix4& go_world = dmGameObject::GetWorldMatrix(component->m_Instance);
        component->m_World = go_world * component->m_Local;

        // Local space uses separate vertex buffers
        if (dmRender::GetMaterialVertexSpace(GetMaterial(component, component->m_Resource)) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
        {
            UpdateComponentVertexBuffer(world, component);
        }

        ReHash(component);
        if (world->m_RenderDispatch == 0)
            world->m_RenderDispatch = dmRender::RegisterRenderListDispatch(world->m_RenderContext, RenderListDispatch, 0, world);
        component->m_RenderItem = dmRender::NewRenderItem(world->m_RenderContext, world->m_RenderDispatch, (uintptr_t)component);
        dmRender::SetWorldTransform(world->m_RenderContext, component->m_RenderItem, component->m_World);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    void* CompMeshGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        return (void*)world->m_Components.Get(params.m_UserData);
    }

    dmGameObject::CreateResult CompMeshDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;

        uint32_t index = (uint32_t)*params.m_UserData;
        MeshComponent* component = world->m_Components.Get(index);
        dmRender::DeleteRenderItem(world->m_RenderContext, component->m_RenderItem);
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);

        if (component->m_VertexBufferResource)
            DecRefVertexBuffer(world, component->m_VertexBufferResource);
        if (component->m_VertexDeclaration)
            dmGraphics::DeleteVertexDeclaration(component->m_VertexDeclaration);

        if (component->m_Material)
        {
            dmResource::Release(factory, component->m_Material);
        }

        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i) {
            if (component->m_Textures[i]) {
                dmResource::Release(factory, component->m_Textures[i]);
            }
        }

        if (component->m_BufferResource) {
            dmResource::Release(factory, component->m_BufferResource);
        }
        if (component->m_RenderConstants)
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);

        delete component;
        world->m_Components.Free(index, true);

        return dmGameObject::CREATE_RESULT_OK;
    }

    static void UpdateTransforms(MeshWorld* world)
    {
        DM_PROFILE("UpdateTransforms");

        const dmArray<MeshComponent*>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            MeshComponent* c = components[i];

            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            const Matrix4& go_world = dmGameObject::GetWorldMatrix(c->m_Instance);
            Matrix4        transform = go_world * c->m_Local;
            if (memcmp(&transform, &c->m_World, sizeof(transform)) != 0)
            {
                c->m_World = transform;
                dmRender::SetWorldTransform(world->m_RenderContext, c->m_RenderItem, transform);
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
        DM_PROFILE("Update");

        MeshWorld* world = (MeshWorld*)params.m_World;

        const dmArray<MeshComponent*>& components = world->m_Components.GetRawObjects();
        const uint32_t count = components.Size();

        dmRender::TrimBuffer(world->m_RenderContext, world->m_AttributeBuffer);
        dmRender::RewindBuffer(world->m_RenderContext, world->m_AttributeBuffer);
        world->m_AttributeBufferDispatchCount = 0;
        for (uint32_t i = 0; i < world->m_AttributeBindings.Size();)
        {
            MeshAttributeBinding* binding = world->m_AttributeBindings[i];
            if (!binding->m_Used)
            {
                DeleteAttributeBinding(binding);
                world->m_AttributeBindings.EraseSwap(i);
            }
            else
            {
                binding->m_Used = false;
                ++i;
            }
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            MeshComponent& component = *components[i];

            if (!component.m_Enabled || !component.m_AddedToUpdate)
            {
                continue;
            }

            dmRender::HMaterial material = GetMaterial(&component, component.m_Resource);
            if (component.m_VertexBufferResource || dmRender::GetMaterialVertexSpace(material) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
            {
                // Needs to be calculated here, since the buffer resource might have been changed since the last update
                UpdateComponentVertexBuffer(world, &component);
            }
            else
            {
                UpdateComponentVertexDeclaration(&component);
            }

            if (component.m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component.m_RenderConstants))
                component.m_ReHash = 1;

            if (component.m_ReHash)
            {
                ReHash(&component);
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompMeshLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE("LateUpdate");
        MeshWorld* world = (MeshWorld*)params.m_World;

        UpdateTransforms(world);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void FillRenderObject(dmRender::RenderObject& ro,
        const dmGraphics::PrimitiveType& primitive_type,
        const dmRender::HMaterial& material,
        const TextureResource** textures_resource,
        const TextureResource** textures_component,
        const dmGraphics::HVertexDeclaration& vert_decl,
        const dmGraphics::HVertexBuffer& vert_buffer,
        uint32_t vert_start, uint32_t vert_count,
        const Matrix4& world_transform,
        HComponentRenderConstants constants)
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
        {
            if (textures_component[i])
            {
                ro.m_Textures[i] = textures_component[i]->m_Texture;;
            }
            else if (textures_resource[i])
            {
                ro.m_Textures[i] = textures_resource[i]->m_Texture;
            }
        }

        if (constants)
        {
            dmGameSystem::EnableRenderObjectConstants(&ro, constants);
        }
    }

    template<typename T> static void FillAndApply(const Matrix4& matrix, bool is_point, uint8_t component_count, uint32_t count, uint32_t stride, T* raw_data, T* src_stream_data, T* dst_data_ptr)
    {
        // Offset dst_data_ptr if stream isn't first!
        uint32_t ptr_offset = (uint8_t*)src_stream_data - (uint8_t*)raw_data;
        dst_data_ptr = (T*)((uint8_t*)dst_data_ptr + ptr_offset);

        Vector4 v(0.0f);
        float w = is_point ? 1.0f : 0.0f;

        if (component_count == 2) {
            for (int pi = 0; pi < count; ++pi)
            {
                v[0] = (float)src_stream_data[0];
                v[1] = (float)src_stream_data[1];
                v[2] = 0.0f;
                v[3] = w;
                v = matrix * v;
                dst_data_ptr[0] = (T)v[0];
                dst_data_ptr[1] = (T)v[1];
                dst_data_ptr[2] = (T)v[2];

                // Update in/out-ptrs with stride (they will point to next entry in T "list").
                src_stream_data += stride;
                dst_data_ptr += stride;
            }
        } else {
            for (int pi = 0; pi < count; ++pi)
            {
                v[0] = (float)src_stream_data[0];
                v[1] = (float)src_stream_data[1];
                v[2] = (float)src_stream_data[2];
                v[3] = w;
                v = matrix * v;
                dst_data_ptr[0] = (T)v[0];
                dst_data_ptr[1] = (T)v[1];
                dst_data_ptr[2] = (T)v[2];

                // Update in/out-ptrs with stride (they will point to next entry in T "list").
                src_stream_data += stride;
                dst_data_ptr += stride;
            }
        }
    }

    static inline void FillAndApplyStream(const BufferResource* buffer_resource, bool is_point, const Matrix4& matrix, dmhash_t stream_id, dmBufferDDF::ValueType value_type, void* raw_data, void* dst_data_ptr)
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

        if (!(components == 3 || components == 2)) {
            dmLogError("Rendering mesh components in world space is only supported for streams with 3 or 2 components, %s has %d components.", dmHashReverseSafe64(stream_id), components);
            return;
        }

        switch (value_type)
        {
            case dmBufferDDF::VALUE_TYPE_UINT8:
            FillAndApply<uint8_t>(matrix, is_point, components, count, stride, (uint8_t*)raw_data, (uint8_t*)stream_data, (uint8_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_UINT16:
            FillAndApply<uint16_t>(matrix, is_point, components, count, stride, (uint16_t*)raw_data, (uint16_t*)stream_data, (uint16_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_UINT32:
            FillAndApply<uint32_t>(matrix, is_point, components, count, stride, (uint32_t*)raw_data, (uint32_t*)stream_data, (uint32_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_INT8:
            FillAndApply<int8_t>(matrix, is_point, components, count, stride, (int8_t*)raw_data, (int8_t*)stream_data, (int8_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_INT16:
            FillAndApply<int16_t>(matrix, is_point, components, count, stride, (int16_t*)raw_data, (int16_t*)stream_data, (int16_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_INT32:
            FillAndApply<int32_t>(matrix, is_point, components, count, stride, (int32_t*)raw_data, (int32_t*)stream_data, (int32_t*)dst_data_ptr);
                break;
            case dmBufferDDF::VALUE_TYPE_FLOAT32:
            FillAndApply<float>(matrix, is_point, components, count, stride, (float*)raw_data, (float*)stream_data, (float*)dst_data_ptr);
                break;
            default:
                dmLogError("Stream type (%d) for %s is not supported.", value_type, dmHashReverseSafe64(stream_id));
                break;
        }
    }

    static inline void RenderBatchWorldVS(MeshWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("RenderBatchWorld");

        dmGraphics::HVertexBuffer vert_buffer = AllocVertexBuffer(world, world->m_GraphicsContext);
        assert(vert_buffer);

        if (world->m_VertexBufferWorld.Full())
            world->m_VertexBufferWorld.OffsetCapacity(2);
        world->m_VertexBufferWorld.Push(vert_buffer);

        dmRender::RenderObject& ro = *world->m_RenderObjects.End();
        world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);

        const MeshComponent* first = (MeshComponent*) buf[*begin].m_UserData;
        const MeshResource* mr = first->m_Resource;
        const BufferResource* br = GetBufferResource(first);

        // Setup vertex declaration, count and sizes etc.
        // These defaults to values in the mesh and buffer resources,
        // but will be overwritten if the component instance has a "custom"
        // vertices buffer set (ie set via resource properties).
        dmGraphics::HVertexDeclaration vert_decl = GetVertexDeclaration(first);
        uint32_t vert_size = br->m_Stride;

        // Find out how many elements/vertices all instances in this batch has
        // TODO: We might want to calculate this only when a mesh/buffer changes.
        uint32_t element_count = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshComponent* c = (MeshComponent*) buf[*i].m_UserData;
            const BufferResource* br = GetBufferResource(c);

            element_count += br->m_ElementCount;
        }

        // Allocate a larger scratch buffer if vert count * vert size is larger than current buffer.
        if (world->m_WorldVertexDataSize < vert_size * element_count)
        {
            world->m_WorldVertexDataSize = vert_size * element_count;
            world->m_WorldVertexData = realloc(world->m_WorldVertexData, world->m_WorldVertexDataSize);
        }

        // Fill scratch buffer with data
        void* dst_data_ptr = world->m_WorldVertexData;
        for (uint32_t *i=begin;i!=end;i++)
        {
            const MeshComponent* component = (MeshComponent*) buf[*i].m_UserData;
            const MeshResource* mr = component->m_Resource;
            const BufferResource* br = GetBufferResource(component);

            // No idea of rendering with zero element count.
            if (br->m_ElementCount == 0) {
                continue;
            }

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
                FillAndApplyStream(br, true, component->m_World, mr->m_PositionStreamId, mr->m_PositionStreamType, raw_data, dst_data_ptr);
            }

            // Modify normal stream, if specified
            if (mr->m_NormalStreamId) {
                Matrix4 normal_matrix = affineInverse(component->m_World);
                normal_matrix = transpose(normal_matrix);
                FillAndApplyStream(br, false, normal_matrix, mr->m_NormalStreamId, mr->m_NormalStreamType, raw_data, dst_data_ptr);
            }

            dst_data_ptr = (void*)((uint8_t*)dst_data_ptr + size);
        }

        DM_PROPERTY_ADD_U32(rmtp_MeshVertexCount, element_count);
        DM_PROPERTY_ADD_U32(rmtp_MeshVertexSize, vert_size * element_count);

        // since they are batched, they have the same settings as the first mesh
        const MeshComponent* component                 = (MeshComponent*) buf[*begin].m_UserData;
        const TextureResource** mesh_resource_textures = (const TextureResource**) mr->m_Textures;
        const TextureResource** component_textures     = (const TextureResource**) component->m_Textures;

        FillRenderObject(ro, mr->m_PrimitiveType, material, mesh_resource_textures, component_textures, vert_decl, vert_buffer, 0, element_count, Matrix4::identity(), first->m_RenderConstants);
        dmGraphics::SetVertexBufferData(vert_buffer, vert_size * element_count, world->m_WorldVertexData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        dmRender::AddToRender(render_context, &ro);
    }


    static MeshAttributeBinding* GetAttributeBinding(MeshWorld* world, const MeshComponent* component, dmRender::HMaterial material)
    {
        const dmGraphics::VertexAttributeInfo* attributes;
        uint32_t attribute_count;
        dmRender::GetMaterialProgramVertexAttributeInfos(material, &attributes, &attribute_count);

        HashState32 geometry_state;
        dmHashInit32(&geometry_state, false);
        dmGraphics::HashVertexDeclaration(&geometry_state, GetVertexDeclaration(component));
        uint32_t geometry_hash = dmHashFinal32(&geometry_state);
        HashState64 state;
        dmHashInit64(&state, false);
        dmHashUpdateBuffer64(&state, &geometry_hash, sizeof(geometry_hash));
        for (uint32_t i = 0; i < attribute_count; ++i)
        {
            const dmGraphics::VertexAttributeInfo& attribute = attributes[i];
            dmHashUpdateBuffer64(&state, &attribute.m_NameHash, sizeof(attribute.m_NameHash));
            dmHashUpdateBuffer64(&state, &attribute.m_SemanticType, sizeof(attribute.m_SemanticType));
            dmHashUpdateBuffer64(&state, &attribute.m_DataType, sizeof(attribute.m_DataType));
            dmHashUpdateBuffer64(&state, &attribute.m_VectorType, sizeof(attribute.m_VectorType));
            dmHashUpdateBuffer64(&state, &attribute.m_StepFunction, sizeof(attribute.m_StepFunction));
            dmHashUpdateBuffer64(&state, &attribute.m_RequestedStepFunction, sizeof(attribute.m_RequestedStepFunction));
            dmHashUpdateBuffer64(&state, &attribute.m_Normalize, sizeof(attribute.m_Normalize));
        }
        uint32_t geometry_stride = dmGraphics::GetVertexDeclarationStride(GetVertexDeclaration(component));
        dmHashUpdateBuffer64(&state, &geometry_stride, sizeof(geometry_stride));
        dmhash_t hash = dmHashFinal64(&state);
        MeshAttributeBinding* binding = 0;
        for (uint32_t i = 0; i < world->m_AttributeBindings.Size(); ++i)
        {
            if (world->m_AttributeBindings[i]->m_Hash == hash)
            {
                binding = world->m_AttributeBindings[i];
                break;
            }
        }

        if (!binding)
        {
            binding = new MeshAttributeBinding();
            binding->m_Hash = hash;
            binding->m_AttributeIndices.SetCapacity(attribute_count);
            dmArray<dmhash_t> excluded_streams;
            excluded_streams.SetCapacity(attribute_count);
            dmGraphics::HVertexStreamDeclaration vertex_streams = dmGraphics::NewVertexStreamDeclaration(world->m_GraphicsContext);
            dmGraphics::HVertexStreamDeclaration instance_streams = dmGraphics::NewVertexStreamDeclaration(world->m_GraphicsContext, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE);
            uint32_t vertex_count = 0;
            uint32_t instance_count = 0;
            uint32_t vertex_stride = 0;
            uint32_t instance_stride = 0;
            BufferResource* buffer = GetBufferResource(component);
            for (uint32_t i = 0; i < attribute_count; ++i)
            {
                const dmGraphics::VertexAttributeInfo& attribute = attributes[i];
                dmBuffer::ValueType stream_type;
                uint32_t stream_count;
                bool matrix = attribute.m_SemanticType == dmGraphics::VertexAttribute::SEMANTIC_TYPE_WORLD_MATRIX ||
                              attribute.m_SemanticType == dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX;
                bool generated = attribute.m_RequestedStepFunction == dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE ||
                                 (matrix && dmBuffer::GetStreamType(buffer->m_Buffer, attribute.m_NameHash, &stream_type, &stream_count) != dmBuffer::RESULT_OK);
                if (!generated)
                    continue;

                binding->m_HasNormalMatrix |= attribute.m_SemanticType == dmGraphics::VertexAttribute::SEMANTIC_TYPE_NORMAL_MATRIX;

                // Instance attributes belong to the component, even when a buffer
                // happens to contain a stream with the same name. Keep this rule
                // on devices that turn instance attributes into vertex attributes.
                binding->m_AttributeIndices.Push(i);
                excluded_streams.Push(attribute.m_NameHash);
                bool instance = attribute.m_StepFunction == dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
                dmGraphics::AddVertexStream(instance ? instance_streams : vertex_streams,
                    attribute.m_NameHash, attribute.m_ElementCount, dmGraphics::GetGraphicsType(attribute.m_DataType), attribute.m_Normalize);
                uint32_t size = attribute.m_ElementCount * dmGraphics::DataTypeToByteWidth(attribute.m_DataType);
                uint32_t aligned_size = (size + 3) & ~3u;
                if (instance)
                {
                    ++instance_count;
                    instance_stride += aligned_size;
                }
                else
                {
                    ++vertex_count;
                    vertex_stride += aligned_size;
                }
            }
            if (!excluded_streams.Empty())
                BuildVertexDeclaration(buffer, excluded_streams.Begin(), excluded_streams.Size(), &binding->m_BaseDeclaration);
            if (vertex_count)
                binding->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(world->m_GraphicsContext, vertex_streams, vertex_stride);
            if (instance_count)
                binding->m_InstanceDeclaration = dmGraphics::NewVertexDeclaration(world->m_GraphicsContext, instance_streams, instance_stride);
            dmGraphics::DeleteVertexStreamDeclaration(vertex_streams);
            dmGraphics::DeleteVertexStreamDeclaration(instance_streams);
            uint32_t offsets[2] = {};
            uint32_t stream_indices[2] = {};
            for (uint32_t i = 0; i < binding->m_AttributeIndices.Size(); ++i)
            {
                const dmGraphics::VertexAttributeInfo& attribute = attributes[binding->m_AttributeIndices[i]];
                bool instance = attribute.m_StepFunction == dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE;
                dmGraphics::HVertexDeclaration declaration = instance ? binding->m_InstanceDeclaration : binding->m_VertexDeclaration;
                // Keep float attributes aligned even after byte/short values.
                // Unlike source geometry, generated layouts are ours to choose.
                bool result = dmGraphics::SetStreamOffset(declaration, stream_indices[instance]++, offsets[instance]);
                assert(result);
                uint32_t size = attribute.m_ElementCount * dmGraphics::DataTypeToByteWidth(attribute.m_DataType);
                offsets[instance] += (size + 3) & ~3u;
            }
            uint32_t count = binding->m_AttributeIndices.Size();
            binding->m_Attributes.SetCapacity(count);
            binding->m_Attributes.SetSize(count);
            if (world->m_AttributeBindings.Full())
                world->m_AttributeBindings.OffsetCapacity(8);
            world->m_AttributeBindings.Push(binding);
        }

        // Values may change without changing the layout, including through
        // render-script material overrides. Refresh the material-owned pointers.
        for (uint32_t i = 0; i < binding->m_AttributeIndices.Size(); ++i)
            binding->m_Attributes[i] = attributes[binding->m_AttributeIndices[i]];
        binding->m_Used = true;
        return binding;
    }

    static uint32_t WriteGeneratedAttributes(MeshWorld* world, MeshAttributeBinding* binding, const MeshComponent* component,
                                            dmGraphics::VertexStepFunction step, uint32_t count, bool align_start)
    {
        dmGraphics::HVertexDeclaration declaration = step == dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE ? binding->m_InstanceDeclaration : binding->m_VertexDeclaration;
        uint32_t stride = dmGraphics::GetVertexDeclarationStride(declaration);
        uint32_t offset = world->m_AttributeData.Size();
        if (align_start)
            offset = (offset + 3) & ~3u;
        uint32_t size = offset + stride * count;
        if (world->m_AttributeData.Capacity() < size)
            world->m_AttributeData.SetCapacity(dmMath::Max(size, world->m_AttributeData.Capacity() + 4096));
        world->m_AttributeData.SetSize(size);

        Matrix4 normal_matrix = binding->m_HasNormalMatrix ? dmRender::GetNormalMatrix(world->m_RenderContext, component->m_World) : Matrix4::identity();
        const float* world_matrix_channels[] = { (const float*)&component->m_World };
        const float* normal_matrix_channels[] = { (const float*)&normal_matrix };
        dmGraphics::VertexAttributeInfos infos;
        infos.m_NumInfos = 1;
        dmGraphics::WriteAttributeParams params = {};
        params.m_VertexAttributeInfos = &infos;
        params.m_StepFunction = step;
        params.m_WorldMatrix.m_Data = world_matrix_channels;
        params.m_WorldMatrix.m_VectorType = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4;
        params.m_WorldMatrix.m_StreamCount = 1;
        params.m_WorldMatrix.m_IsGlobalData = 1;
        params.m_NormalMatrix.m_Data = normal_matrix_channels;
        params.m_NormalMatrix.m_VectorType = dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4;
        params.m_NormalMatrix.m_StreamCount = 1;
        params.m_NormalMatrix.m_IsGlobalData = 1;
        for (uint32_t i = 0; i < binding->m_Attributes.Size(); ++i)
        {
            const dmGraphics::VertexAttributeInfo& attribute = binding->m_Attributes[i];
            if (attribute.m_StepFunction != step)
                continue;
            infos.m_Infos = &attribute;
            float value[16];
            uint8_t* end = dmGraphics::WriteAttributes((uint8_t*)value, 0, 1, params);
            uint32_t value_size = end - (uint8_t*)value;
            uint32_t stream_offset = dmGraphics::GetVertexStreamOffset(declaration, attribute.m_NameHash);
            // Respect backend padding between attributes and at the end of each
            // record. The generic writer itself produces tightly packed values.
            for (uint32_t j = 0; j < count; ++j)
                memcpy(world->m_AttributeData.Begin() + offset + j * stride + stream_offset, value, value_size);
        }
        return offset;
    }

    static inline void RenderBatchLocalVS(MeshWorld* world, dmRender::HMaterial material, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("RenderBatchLocal");

        MeshComponent* first = (MeshComponent*)buf[*begin].m_UserData;
        BufferResource* buffer = GetBufferResource(first);
        if (buffer->m_ElementCount == 0)
            return;
        UpdateComponentVertexBuffer(world, first);
        MeshAttributeBinding* binding = GetAttributeBinding(world, first, material);
        bool instanced = binding->m_InstanceDeclaration != 0 && binding->m_VertexDeclaration == 0;
        if (!binding->m_Attributes.Empty() && world->m_AttributeData.Empty() &&
            dmRender::GetBufferIndex(render_context, world->m_AttributeBuffer) < world->m_AttributeBufferDispatchCount)
        {
            dmRender::AddRenderBuffer(render_context, world->m_AttributeBuffer);
        }

        // Vertex-step generated matrices vary per component and cannot share
        // geometry within one draw. They also provide the non-instanced fallback.
        uint32_t vertex_count = 0;
        uint32_t vertex_data_size = 0;
        for (uint32_t *i=begin;i!=end;i++)
        {
            MeshComponent* component = (MeshComponent*) buf[*i].m_UserData;
            const MeshResource* mr = component->m_Resource;
            dmGameSystem::BufferResource* br = GetBufferResource(component);
            UpdateComponentVertexBuffer(world, component);
            VertexBufferInfo* info = world->m_ResourceToVertexBuffer.Get(br->m_NameHash);
            assert(info != 0);

            vertex_count += br->m_ElementCount;
            vertex_data_size += br->m_Stride * br->m_ElementCount;
            uint32_t instance_offset = 0;
            if (binding->m_InstanceDeclaration)
                instance_offset = WriteGeneratedAttributes(world, binding, component, dmGraphics::VERTEX_STEP_FUNCTION_INSTANCE, 1, !instanced || i == begin);
            if (instanced && i != begin)
                continue;

            dmRender::RenderObject& ro = *world->m_RenderObjects.End();
            world->m_RenderObjects.SetSize(world->m_RenderObjects.Size()+1);
            dmGraphics::HVertexDeclaration vert_decl = binding->m_BaseDeclaration ? binding->m_BaseDeclaration : GetVertexDeclaration(component);
            const TextureResource** mesh_resource_textures = (const TextureResource**) mr->m_Textures;
            const TextureResource** component_textures     = (const TextureResource**) component->m_Textures;
            FillRenderObject(ro, mr->m_PrimitiveType, GetMaterial(component, mr), mesh_resource_textures, component_textures, vert_decl, info->m_VertexBuffer, 0, br->m_ElementCount, component->m_World, component->m_RenderConstants);
            // Use the same effective textures as the batch key, including
            // textures supplied by a component material override.
            for (uint32_t texture = 0; texture < MAX_TEXTURE_COUNT; ++texture)
                ro.m_Textures[texture] = GetTexture(component, texture);
            if (binding->m_InstanceDeclaration)
            {
                ro.m_VertexDeclarations[1] = binding->m_InstanceDeclaration;
                ro.m_VertexBuffers[1] = (dmGraphics::HVertexBuffer)dmRender::GetBuffer(render_context, world->m_AttributeBuffer);
                ro.m_VertexBufferOffsets[1] = instance_offset;
                ro.m_InstanceCount = instanced ? end - begin : 1;
            }
            if (binding->m_VertexDeclaration)
            {
                ro.m_VertexDeclarations[2] = binding->m_VertexDeclaration;
                ro.m_VertexBuffers[2] = (dmGraphics::HVertexBuffer)dmRender::GetBuffer(render_context, world->m_AttributeBuffer);
                ro.m_VertexBufferOffsets[2] = WriteGeneratedAttributes(world, binding, component, dmGraphics::VERTEX_STEP_FUNCTION_VERTEX, br->m_ElementCount, true);
            }
            dmRender::AddToRender(render_context, &ro);
        }
        DM_PROPERTY_ADD_U32(rmtp_MeshVertexCount, vertex_count);
        DM_PROPERTY_ADD_U32(rmtp_MeshVertexSize, vertex_data_size);
    }

    static void RenderBatch(MeshWorld* world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("MeshRenderBatch");

        const MeshComponent* first = (MeshComponent*) buf[*begin].m_UserData;
        dmRender::HMaterial material = dmRender::GetContextMaterial(render_context);
        if (!material)
            material = GetMaterial(first, first->m_Resource);
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
                world->m_RenderObjects.SetSize(0);
                world->m_AttributeData.SetSize(0);

                if (world->m_VertexBufferPool.Capacity() < world->m_VertexBufferPool.Size()+world->m_VertexBufferWorld.Size())
                    world->m_VertexBufferPool.OffsetCapacity(world->m_VertexBufferWorld.Size());
                world->m_VertexBufferPool.PushArray(world->m_VertexBufferWorld.Begin(), world->m_VertexBufferWorld.Size());
                world->m_VertexBufferWorld.SetSize(0);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_BATCH:
            {
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
                break;
            }
            case dmRender::RENDER_LIST_OPERATION_END:
            {
                if (!world->m_AttributeData.Empty())
                {
                    dmRender::SetBufferData(params.m_Context, world->m_AttributeBuffer, world->m_AttributeData.Size(), world->m_AttributeData.Begin(), dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
                    ++world->m_AttributeBufferDispatchCount;
                    DM_PROPERTY_ADD_U32(rmtp_MeshVertexSize, world->m_AttributeData.Size());
                }
                break;
            }
            default:
                assert(false);
                break;
        }
    }

    dmGameObject::UpdateResult CompMeshRender(const dmGameObject::ComponentsRenderParams& params)
    {
        DM_PROFILE("Render");

        MeshContext* context = (MeshContext*)params.m_Context;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        MeshWorld* world = (MeshWorld*)params.m_World;

        const dmArray<MeshComponent*>& components = world->m_Components.GetRawObjects();
        const uint32_t count = components.Size();

        uint32_t num_enabled = 0;
        for (uint32_t i = 0; i < count; ++i)
        {
            MeshComponent& component = *components[i];
            bool           enabled = component.m_Enabled && component.m_AddedToUpdate;
            dmRender::Enable(render_context, component.m_RenderItem, enabled);
            if (!enabled)
                continue;
            ++num_enabled;
            dmRender::SetBatchKey(render_context, component.m_RenderItem, component.m_MixedHash);
            dmRender::SetMaterial(render_context, component.m_RenderItem, GetMaterial(&component, component.m_Resource));

            BufferResource*     buffer = GetBufferResource(&component);
            void*               bounds = 0;
            uint32_t            count = 0;
            dmBuffer::ValueType type;
            dmBuffer::Result    result = dmBuffer::GetMetaData(buffer->m_Buffer, AABB_HASH, &bounds, &count, &type);
            if (result == dmBuffer::RESULT_OK && type == dmBuffer::VALUE_TYPE_FLOAT32 && count == 6)
            {
                float* values = (float*)bounds;
                dmRender::SetBoundingOBB(render_context, component.m_RenderItem, Vector3(values[0], values[1], values[2]), Vector3(values[3], values[4], values[5]));
            }
            else
            {
                if (result == dmBuffer::RESULT_OK)
                {
                    dmLogError("Invalid AABB metadata. Expected array of 6 floats.");
                }
                else if (result != dmBuffer::RESULT_METADATA_MISSING)
                {
                    dmLogError("Error getting AABB for mesh buffer");
                }
                dmRender::SetBoundingAABB(render_context, component.m_RenderItem, Vector3(1), Vector3(-1));
            }
        }
        DM_PROPERTY_ADD_U32(rmtp_Mesh, num_enabled);

        if (num_enabled)
            dmRender::SubmitRenderListDispatch(render_context, world->m_RenderDispatch);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompMeshGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        MeshComponent* component = (MeshComponent*)user_data;
        return component->m_RenderConstants && dmGameSystem::GetRenderConstant(component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompMeshSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        MeshComponent* component = (MeshComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, value_index, element_index, var);
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
            if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                if (component->m_RenderConstants && dmGameSystem::ClearRenderConstant(component->m_RenderConstants, ddf->m_NameHash))
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
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetBufferResource(component), out_value);
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterialResource(component, component->m_Resource), out_value);
        }

        for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
        {
            if (params.m_PropertyId == PROP_TEXTURE[i])
            {
                return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTextureResource(component, i), out_value);
            }
        }

        int32_t value_index = 0;
        GetPropertyOptionsIndex(params.m_Options, 0, &value_index);

        return GetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, value_index, out_value, true, CompMeshGetConstantCallback, component);
    }

    dmGameObject::PropertyResult CompMeshSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        MeshWorld* world = (MeshWorld*)params.m_World;
        MeshComponent* component = world->m_Components.Get(*params.m_UserData);

        if (params.m_PropertyId == PROP_VERTICES)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, BUFFER_EXT_HASH, (void**)&component->m_BufferResource);
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                BufferResource* buffer = GetBufferResource(component);
                dmGraphics::HVertexDeclaration declaration;
                if (!BuildVertexDeclaration(buffer, &declaration))
                {
                    dmLogError("Error while building vertex declaration from new resource.");
                    return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_VALUE;
                }
                if (component->m_VertexDeclaration)
                    dmGraphics::DeleteVertexDeclaration(component->m_VertexDeclaration);
                component->m_VertexDeclaration = declaration;
                component->m_VertexDeclarationBuffer = buffer->m_Buffer;
                component->m_ReHash = 1;
                if (component->m_VertexBufferResource || dmRender::GetMaterialVertexSpace(GetMaterial(component, component->m_Resource)) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
                    UpdateComponentVertexBuffer(world, component);
            }
            return res;
        }
        else if (params.m_PropertyId == PROP_MATERIAL)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                component->m_ReHash = 1;
                if (dmRender::GetMaterialVertexSpace(GetMaterial(component, component->m_Resource)) == dmRenderDDF::MaterialDesc::VERTEX_SPACE_LOCAL)
                    UpdateComponentVertexBuffer(world, component);
                else if (component->m_VertexBufferResource)
                {
                    DecRefVertexBuffer(world, component->m_VertexBufferResource);
                    component->m_VertexBufferResource = 0;
                }
            }
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

        int32_t value_index = 0;
        GetPropertyOptionsIndex(params.m_Options, 0, &value_index);

        dmGameObject::PropertyResult res = SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, value_index, CompMeshSetConstantCallback, component);
        component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;

        return res;
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        MeshWorld* world = (MeshWorld*) params->m_UserData;
        const dmArray<MeshComponent*>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();

        const void* resource = dmResource::GetResource(params->m_Resource);
        for (uint32_t i = 0; i < n; ++i)
        {
            MeshComponent* component = components[i];
            if (component->m_Resource)
            {
                const dmGameSystem::BufferResource* buffer_resource = GetBufferResource(component);
                if (component->m_Resource == resource ||
                   GetMaterialResource(component, component->m_Resource) == resource ||
                   buffer_resource == resource)
                {
                    component->m_ReHash = 1;
                    continue;
                }

                for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
                {
                    TextureResource* texture = GetTextureResource(component, i);
                    if (texture == resource)
                    {
                        component->m_ReHash = 1;
                        break;
                    }
                }
            }
        }
    }

    static bool CompMeshIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        MeshWorld* world = (MeshWorld*)pit->m_Node->m_ComponentWorld;
        MeshComponent* component = world->m_Components.Get(pit->m_Node->m_Component);

        uint64_t index = pit->m_Next++;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                pit->m_Property.m_Value.m_Bool = component->m_Enabled;
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;

        return false;
    }

    void CompMeshIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompMeshIterPropertiesGetNext;
    }

    static dmGameObject::Result CompMeshTypeCreate(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        MeshContext* mesh_context = new MeshContext;
        HContextRegistry context_registry = dmGameObject::ComponentGetContextRegistry(ctx);
        mesh_context->m_Factory = ctx->m_Factory;
        mesh_context->m_RenderContext = (dmRender::HRenderContext) ContextRegistryGet(context_registry, RENDER_CONTEXT_NAME);
        mesh_context->m_MaxMeshCount = dmConfigFile::GetInt(ctx->m_Config, MESH_MAX_COUNT_KEY, 128);

        ComponentTypeSetPrio(type, 725);

        ComponentTypeSetContext(type, mesh_context);
        ComponentTypeSetNewWorldFn(type, CompMeshNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompMeshDeleteWorld);
        ComponentTypeSetCreateFn(type, CompMeshCreate);
        ComponentTypeSetDestroyFn(type, CompMeshDestroy);
        ComponentTypeSetAddToUpdateFn(type, CompMeshAddToUpdate);
        ComponentTypeSetUpdateFn(type, CompMeshUpdate);
        ComponentTypeSetLateUpdateFn(type, CompMeshLateUpdate);
        ComponentTypeSetRenderFn(type, CompMeshRender);
        ComponentTypeSetOnMessageFn(type, CompMeshOnMessage);
        ComponentTypeSetGetPropertyFn(type, CompMeshGetProperty);
        ComponentTypeSetSetPropertyFn(type, CompMeshSetProperty);
        ComponentTypeSetGetFn(type, CompMeshGetComponent);

        ComponentTypeSetPropertyIteratorFn(type, CompMeshIterProperties);

        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::Result CompMeshTypeDestroy(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        MeshContext* mesh_context = (MeshContext*)dmGameObject::ComponentTypeGetContext(type);
        if (!mesh_context)
        {
            // if the initialization process failed (e.g. unit tests)
            return dmGameObject::RESULT_OK;
        }

        delete mesh_context;

        return dmGameObject::RESULT_OK;
    }

}

DM_DECLARE_COMPONENT_TYPE(ComponentTypeMesh, "meshc", dmGameSystem::CompMeshTypeCreate, dmGameSystem::CompMeshTypeDestroy);
