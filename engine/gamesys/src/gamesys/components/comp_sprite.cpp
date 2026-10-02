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

#include "comp_sprite.h"
#include <render/render_frame.h>

#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/double_linked_list.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/time.h>
#include <dlib/dstrings.h>
#include <dlib/object_pool.h>
#include <dlib/math.h>
#include <dmsdk/dlib/vmath.h>
#include <dmsdk/dlib/intersection.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../resources/res_sprite.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include <gamesys/sprite_ddf.h>
#include <gamesys/gamesys_ddf.h>

#include <dmsdk/gameobject/script.h>
#include <dmsdk/gamesys/render_constants.h>

#include <dmsdk/gamesys/resources/res_material.h>
#include <dmsdk/gamesys/resources/res_textureset.h>

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Sprite, 0, PROFILE_PROPERTY_FRAME_RESET, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_SpriteVertexCount, 0, PROFILE_PROPERTY_FRAME_RESET, "# vertices", &rmtp_Sprite);
DM_PROPERTY_U32(rmtp_SpriteVertexSize, 0, PROFILE_PROPERTY_FRAME_RESET, "size of vertices in bytes", &rmtp_Sprite);
DM_PROPERTY_U32(rmtp_SpriteIndexSize, 0, PROFILE_PROPERTY_FRAME_RESET, "size of indices in bytes", &rmtp_Sprite);

namespace dmGameSystem
{
    using namespace dmVMath;

    static const char* SPRITE_MAX_COUNT_KEY = "sprite.max_count";

    // In general, rare overrides should be kept out of the struct, to keep memory down
    struct SpriteResourceOverrides
    {
        MaterialResource*       m_Material;
        dmArray<SpriteTexture>  m_Textures; // sampler name to texture set

        SpriteResourceOverrides() : m_Material(0) {}
    };

    const uint32_t MAX_TEXTURE_COUNT = dmRender::RenderObject::MAX_TEXTURE_COUNT;
    const uint8_t CACHE_EVICTION_FRAMES = 10; // how many frames cache entry is stored before eviction
    const uint8_t MINIMUM_CACHE_CAPACITY = 100; // minimum hashtable capacity for AnimationData cache

    /* AnimationData is used for storing calculation of sprite geometry.
     * Those information is used during several parts of engine tick:
     * 1. CompSpriteRender
     * 2. RenderBatch
     * 3. Pivot points which should be taken into account during frustrum culling
     * So to avoid multiple calculation during the frame we caclulate it once and use in different places.
     * Also some static sprites (without animations or with stopped animations) have the same AnimationData during several
     * frames.
     *
     * AnimationData stores in hashtable to speedup accessing during the frame. Also AnimationData stores in double linked list
     * to implement LRU cache.
     * Every AnimationData entry stores engine tick when it was last accessed. Then every engine tick we check if AnimationData
     * entry is old enough (see CACHE_EVICTION_FRAMES) and remove it from LRU and from hashtable.
     *
     * We use cache instead of resources here because AnimationData presents intemidiate calculations which can updates frequently
     * and for different sprites it can be the same AnimationData entry (because they share the same texture set and same animation).
     * So instead of storing cached information per component it stores as cache in sprite world to be sahrable between sprites.
     */
    struct AnimationData
    {
        dmDoubleLinkedList::ListNode    m_ListNode;
        // Used after resolving info from all textures
        dmhash_t                        m_AnimationID;                      // The animation of the driving atlas
        uint32_t                        m_Frames[MAX_TEXTURE_COUNT];        // The resolved frame indices
        uint32_t                        m_LastAccessTick;
        uint32_t                        m_CreatedTick;
        uint32_t                        m_TextureGenerations[MAX_TEXTURE_COUNT];
        uint32_t                        m_CacheKey;
        uint32_t                        m_VertexCount;
        uint32_t                        m_IndicesCount;
        float                           m_PageIndices[MAX_TEXTURE_COUNT];
        // Packed column-major 3x3 (9 floats) for vertex attribute: full 2D affine from unit square to atlas quad.
        float                           m_TextureTransformsPacked[MAX_TEXTURE_COUNT][9];

        const dmGameSystemDDF::TextureSetAnimation* m_Animations[MAX_TEXTURE_COUNT];
        const dmGameSystemDDF::SpriteGeometry*      m_Geometries[MAX_TEXTURE_COUNT];
        bool                                        m_CanUseQuads;
    };

#if __cplusplus >= 201103L
    DM_STATIC_ASSERT(offsetof(AnimationData, m_ListNode) == 0, "m_ListNode must be first in struct!");
#endif

    struct AnimationDataCache
    {
        dmHashTable32<AnimationData*> m_Cache;
        dmDoubleLinkedList::List      m_LRU;
        uint32_t                      m_CurrentEngineTick;
    };

    struct SpriteComponent
    {
        Matrix4                     m_World;
        Vector3                     m_Position;
        Quat                        m_Rotation;
        Vector3                     m_Scale;
        Vector3                     m_Size;     // The current size of the animation frame (in texels)
        Vector4                     m_Slice9;

        dmGameObject::HInstance     m_Instance;
        SpriteResource*             m_Resource;
        SpriteResourceOverrides*    m_Overrides;
        HComponentRenderConstants   m_RenderConstants;

        dmMessage::URL              m_Listener;
        int32_t                     m_FunctionRef; // Animation callback function
        // Hash of the m_Resource-pointer etc. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        uint32_t                    m_MixedHash;
        uint32_t                    m_AnimationDataHash;

        int32_t                     m_AnimationFrameCount; // cached value when animation is playing
        /// Currently playing animation
        dmhash_t                    m_CurrentAnimation;
        uint32_t                    m_CurrentAnimationFrame;
        /// Used to scale the time step when updating the timer
        float                       m_AnimInvDuration;
        /// Timer in local space: [0,1]
        float                       m_AnimTimer;
        float                       m_PlaybackRate;
        float                       m_PivotX;
        float                       m_PivotY;
        uint16_t                    m_AnimationID; // index into array
        uint16_t                    m_DynamicVertexAttributeIndex;
        uint16_t                    m_ComponentIndex;
        //------------------- cached values related to vertex/index buffer allocation----------
        uint16_t                    m_VertexCount;
        uint16_t                    m_IndexCount;
        uint16_t                    m_VertexStride;
        //-------------------------------------------------------------------------------------
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_DoTick : 1;
        uint16_t                    m_FlipHorizontal : 1;
        uint16_t                    m_FlipVertical : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_ReHash : 1;
        uint16_t                    m_UseSlice9 : 1;
        uint16_t                    m_AnimationReHash : 1;
        uint16_t                    m_IsPlaying : 1;
        // currently we don't support multiple animation cursors that's why we can use Playback from the first
        // texture set
        uint16_t                    m_AnimationPlayback : 7; // narrowed enum dmGameSystemDDF::Playback
        uint8_t                     m_NumTextures; // cached value from m_Resource->m_NumTextures
    };

    struct SpriteCullingInfo
    {
        float m_Position[3];
        float m_Radius;
    };

    // One reusable slot for the inline experiment. Its lifetime ends at the next
    // capture, after the previous render list has been consumed and cleared.
    struct SpriteRenderData
    {
        Matrix4 m_World;
        Vector4 m_Slice9;
        float m_Size[2];
        float m_Pivot[2];
        uint32_t m_Binding;
        uint32_t m_Geometry;
        uint32_t m_Constants;
        uint32_t m_Attributes;
        uint32_t m_Flags;
        uint32_t m_VertexCount;
        uint32_t m_BatchKey;
        uint32_t m_TagListKey;
    };

    DM_STATIC_ASSERT(sizeof(SpriteRenderData) == 128, "Sprite snapshot record size");
    DM_STATIC_ASSERT(sizeof(SpriteCullingInfo) == 16, "Sprite snapshot bound size");

    // Only immutable resource data and resolved graphics handles are reachable
    // through these frame-owned adapters. Capture lookup maps remain producer-only.
    struct SpriteFrameResolved
    {
        MaterialResource m_Material;
        TextureResource m_MaterialTextures[MAX_TEXTURE_COUNT];
        TextureSetResource m_TextureSets[MAX_TEXTURE_COUNT];
        TextureResource m_Textures[MAX_TEXTURE_COUNT];
    };

    struct SpriteFrameBinding
    {
        SpriteResource* m_Source;
        SpriteResource m_Resolved;
        SpriteTexture m_Textures[MAX_TEXTURE_COUNT];
        uint32_t m_Next;
    };

    struct SpriteFrameBlock
    {
        uint32_t m_Begin;
        uint32_t m_Count;
    };

    struct SpriteFrameConstant
    {
        dmhash_t m_Name;
        dmRenderDDF::MaterialDesc::ConstantType m_Type;
        SpriteFrameBlock m_Values;
    };

    struct SpriteRenderFrame
    {
        SpriteRenderFrame() : m_CapacityBytes(sizeof(SpriteRenderFrame)), m_GrowthPeakBytes(sizeof(SpriteRenderFrame)), m_ResourceBytes(0), m_CapacityLimit(0), m_Overflow(false), m_Builder(0), m_CentralDependencies(false) {}
        uint64_t m_CapacityBytes;
        uint64_t m_GrowthPeakBytes;
        uint64_t m_ResourceBytes;
        uint32_t m_CapacityLimit;
        bool m_Overflow;
        dmRender::RenderFrameBuilder* m_Builder; // Non-null only during extraction.
        bool m_CentralDependencies;
        dmArray<SpriteFrameResolved*> m_ResolvedBindings;
        dmArray<SpriteRenderData> m_Sprites;
        dmArray<SpriteCullingInfo> m_Bounds; // Squared radius, matching TestFrustumSphereSq.
        dmArray<SpriteFrameBinding> m_Bindings;
        dmArray<AnimationData> m_Geometry;
        dmArray<SpriteFrameBlock> m_Constants;
        dmArray<uint32_t> m_ConstantNext;
        dmArray<SpriteFrameConstant> m_ConstantDescriptors;
        dmArray<Vector4> m_ConstantValues;
        dmArray<SpriteFrameBlock> m_Attributes;
        dmArray<DynamicAttributeInfo::Info> m_AttributeValues;
        uint32_t m_VertexCount;
        uint32_t m_IndexCount;
        uint32_t m_VertexMemorySize;
        // Capture-only indices. Consumers never look up a live component/cache key.
        dmHashTable64<uint32_t> m_ResourceSizes; // Unique direct resource descriptors.
        dmHashTable64<uint32_t> m_BindingMap;
        dmHashTable32<uint32_t> m_GeometryMap;
        dmHashTable64<uint32_t> m_ConstantMap;
    };

    struct SpriteWorld;
    struct SpriteRendererState
    {
        SpriteWorld* m_LegacyWorld; // Null in snapshot mode.
        SpriteRenderFrame* m_Frame;
        uint64_t m_CaptureCount;
        uint64_t m_CaptureTotalUs;
        uint32_t m_IndexCapacityBytes;
        dmArray<dmGraphics::VertexAttributeInfo> m_AttributeScratch;
        dmArray<dmRender::HNamedConstantBuffer> m_ConstantBuffers;
        dmArray<dmRender::RenderObject*>    m_RenderObjects;
        // We currently assume the vertex format uses 2-tuple UVs
        dmArray<float>                      m_ScratchUVs[MAX_TEXTURE_COUNT];
        dmArray<Vector4>                    m_ScratchPositionWorld;
        dmArray<Vector4>                    m_ScratchPositionLocal;
        uint32_t                            m_RenderObjectsInUse;
        dmRender::HBufferedRenderBuffer     m_VertexBuffer;
        uint8_t*                            m_VertexBufferData;
        uint8_t*                            m_VertexBufferWritePtr;
        dmRender::HBufferedRenderBuffer     m_IndexBuffer;
        uint32_t                            m_VerticesWritten;
        uint32_t                            m_VertexMemorySize;
        uint32_t                            m_VertexCount;
        uint32_t                            m_IndexCount;
        uint32_t                            m_DispatchCount;
        uint8_t*                            m_IndexBufferData;
        uint8_t*                            m_IndexBufferWritePtr;
        uint8_t                             m_Is16BitIndex : 1;
        uint8_t                             m_ReallocBuffers : 1;
    };

    struct SpriteWorld
    {
        AnimationDataCache m_AnimationDataCache;
        dmObjectPool<SpriteComponent> m_Components;
        DynamicAttributePool m_DynamicVertexAttributePool;
        dmArray<SpriteCullingInfo> m_CullingInfo;
        SpriteRendererState m_Renderer;
        SpriteRenderFrame* m_ThreadFrames[2];
        SpriteSnapshotStats m_ThreadStats;
        bool m_Threaded;
        uint32_t m_VertexCount;
        uint32_t m_IndexCount;
        uint32_t m_VertexMemorySize;
        uint8_t m_ReallocBuffers;
    };

    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SIZE, size, false);
    DM_GAMESYS_PROP_VECTOR4(SPRITE_PROP_SLICE, slice, false);

    static const dmhash_t SPRITE_PROP_CURSOR        = dmHashString64("cursor");
    static const dmhash_t SPRITE_PROP_PLAYBACK_RATE = dmHashString64("playback_rate");
    static const dmhash_t SPRITE_PROP_FRAME_COUNT   = dmHashString64("frame_count");

    // The 9 slice function produces 16 vertices (4 rows 4 columns)
    // and since there's 2 triangles per quad and 9 quads in total,
    // the amount of indices is 6 per quad and 9 quads = 54 indices in total
    static const uint8_t SPRITE_VERTEX_COUNT_SLICE9 = 16;
    static const uint8_t SPRITE_INDEX_COUNT_SLICE9  = 9 * 6;
    // For the legacy version, we produce a single quad with 4 vertices
    // and 6 indices, 2 triangles per quad and three points each.
    static const uint8_t SPRITE_VERTEX_COUNT_LEGACY = 4;
    static const uint8_t SPRITE_INDEX_COUNT_LEGACY  = 6;

    static void ReleaseSpriteFrame(SpriteRenderFrame* frame, dmResource::HFactory factory);

    static float GetCursor(SpriteComponent* component);
    static void SetCursor(SpriteComponent* component, float cursor);
    static float GetPlaybackRate(SpriteComponent* component);
    static void SetPlaybackRate(SpriteComponent* component, float playback_rate);
    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params);

    static void ReAllocateBuffers(SpriteRendererState* sprite_world, dmRender::HRenderContext render_context)
    {
        if (sprite_world->m_VertexBuffer)
        {
            dmRender::DeleteBufferedRenderBuffer(render_context, sprite_world->m_VertexBuffer);
            sprite_world->m_VertexBuffer = 0;
        }

        sprite_world->m_VertexBuffer     = dmRender::NewBufferedRenderBuffer(render_context, dmRender::RENDER_BUFFER_TYPE_VERTEX_BUFFER);
        uint32_t vertex_memsize          = sprite_world->m_VertexMemorySize;
        sprite_world->m_VertexBufferData = (uint8_t*) realloc(sprite_world->m_VertexBufferData, vertex_memsize);

        uint32_t index_data_type_size   = sprite_world->m_VertexCount <= 65536 ? sizeof(uint16_t) : sizeof(uint32_t);
        size_t indices_memsize          = sprite_world->m_IndexCount * index_data_type_size;
        sprite_world->m_IndexCapacityBytes = indices_memsize;
        sprite_world->m_Is16BitIndex    = index_data_type_size == sizeof(uint16_t) ? 1 : 0;
        sprite_world->m_IndexBufferData = (uint8_t*)realloc(sprite_world->m_IndexBufferData, indices_memsize);

        if (sprite_world->m_IndexBuffer)
        {
            dmRender::DeleteBufferedRenderBuffer(render_context, sprite_world->m_IndexBuffer);
            sprite_world->m_IndexBuffer = 0;
        }

        sprite_world->m_IndexBuffer    = dmRender::NewBufferedRenderBuffer(render_context, dmRender::RENDER_BUFFER_TYPE_INDEX_BUFFER);
        sprite_world->m_ReallocBuffers = 0;
    }

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        SpriteWorld* sprite_world = new SpriteWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, sprite_context->m_MaxSpriteCount);
        sprite_world->m_Components.SetCapacity(comp_count);
        sprite_world->m_CullingInfo.SetCapacity(comp_count);
        sprite_world->m_CullingInfo.SetSize(comp_count);
        sprite_world->m_AnimationDataCache.m_Cache.SetCapacity(MINIMUM_CACHE_CAPACITY);
        dmDoubleLinkedList::ListInit(&sprite_world->m_AnimationDataCache.m_LRU);
        memset(sprite_world->m_Components.GetRawObjects().Begin(), 0, sizeof(SpriteComponent) * comp_count);
        sprite_world->m_Renderer.m_RenderObjectsInUse   = 0;
        sprite_world->m_Renderer.m_VertexBuffer         = 0;
        sprite_world->m_Renderer.m_VertexBufferData     = 0;
        sprite_world->m_Renderer.m_VertexBufferWritePtr = 0;
        sprite_world->m_Renderer.m_IndexBuffer          = 0;
        sprite_world->m_Renderer.m_VerticesWritten      = 0;
        sprite_world->m_VertexMemorySize     = 0;
        sprite_world->m_VertexCount          = 0;
        sprite_world->m_IndexCount           = 0;
        sprite_world->m_Renderer.m_DispatchCount        = 0;
        sprite_world->m_Renderer.m_IndexBufferData      = 0;
        sprite_world->m_Renderer.m_IndexBufferWritePtr  = 0;
        sprite_world->m_Renderer.m_Is16BitIndex         = 0;
        sprite_world->m_ReallocBuffers       = 1;

        sprite_world->m_Renderer.m_LegacyWorld = sprite_world;
        sprite_world->m_ThreadFrames[0] = 0;
        sprite_world->m_ThreadFrames[1] = 0;
        sprite_world->m_Threaded = sprite_context->m_SnapshotThreaded;
        memset(&sprite_world->m_ThreadStats, 0, sizeof(sprite_world->m_ThreadStats));
        sprite_world->m_ThreadStats.m_Threaded = sprite_world->m_Threaded;
        sprite_world->m_Renderer.m_Frame = 0;
        sprite_world->m_Renderer.m_CaptureCount = 0;
        sprite_world->m_Renderer.m_CaptureTotalUs = 0;
        sprite_world->m_Renderer.m_IndexCapacityBytes = 0;
        sprite_world->m_Renderer.m_VertexMemorySize = 0;
        sprite_world->m_Renderer.m_VertexCount = 0;
        sprite_world->m_Renderer.m_IndexCount = 0;
        sprite_world->m_Renderer.m_ReallocBuffers = 1;
        InitializeMaterialAttributeInfos(sprite_world->m_DynamicVertexAttributePool, 8);

        *params.m_World = sprite_world;

        dmResource::RegisterResourceReloadedCallback(sprite_context->m_Factory, ResourceReloadedCallback, sprite_world);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        DestroyMaterialAttributeInfos(sprite_world->m_DynamicVertexAttributePool);

        for (uint32_t i = 0; i < sprite_world->m_Renderer.m_RenderObjects.Size(); ++i)
        {
            delete sprite_world->m_Renderer.m_RenderObjects[i];
        }

        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::DeleteBufferedRenderBuffer(sprite_context->m_RenderContext, sprite_world->m_Renderer.m_VertexBuffer);
        free(sprite_world->m_Renderer.m_VertexBufferData);
        dmRender::DeleteBufferedRenderBuffer(sprite_context->m_RenderContext, sprite_world->m_Renderer.m_IndexBuffer);
        free(sprite_world->m_Renderer.m_IndexBufferData);

        dmHashTable32<AnimationData*>::Iterator iter = sprite_world->m_AnimationDataCache.m_Cache.GetIterator();
        while(iter.Next())
        {
            AnimationData* data = iter.GetValue();
            free(data);
        }

        if (sprite_world->m_Threaded)
        {
            ReleaseSpriteThreadFrames(sprite_world, sprite_context);
            delete sprite_world->m_ThreadFrames[0];
            delete sprite_world->m_ThreadFrames[1];
            sprite_world->m_Renderer.m_Frame = 0;
        }
        if (sprite_world->m_Renderer.m_Frame)
        {
            ReleaseSpriteFrame(sprite_world->m_Renderer.m_Frame, sprite_context->m_Factory);
            delete sprite_world->m_Renderer.m_Frame;
        }
        for (uint32_t i = 0; i < sprite_world->m_Renderer.m_ConstantBuffers.Size(); ++i)
            dmRender::DeleteNamedConstantBuffer(sprite_world->m_Renderer.m_ConstantBuffers[i]);
        dmResource::UnregisterResourceReloadedCallback(sprite_context->m_Factory, ResourceReloadedCallback, sprite_world);
        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline Vector3 GetSizeFromAnimation(const SpriteComponent* sprite, dmGameSystemDDF::TextureSet* texture_set_ddf, uint16_t anim_id)
    {
        Vector3 result;
        dmGameSystemDDF::TextureSetAnimation* animation = &texture_set_ddf->m_Animations[anim_id];
        if(texture_set_ddf->m_TexDims.m_Count)
        {
            const float* td = (const float*) texture_set_ddf->m_TexDims.m_Data + ((animation->m_Start + sprite->m_CurrentAnimationFrame ) << 1);
            result[0] = td[0];
            result[1] = td[1];
        }
        else
        {
            result[0] = animation->m_Width;
            result[1] = animation->m_Height;
        }
        result[2] = 1.0f;
        return result;
    }

    void DeleteOverrides(dmResource::HFactory factory, SpriteComponent* component)
    {
        SpriteResourceOverrides* overrides = component->m_Overrides;
        if (!overrides)
            return;

        uint32_t num_textures = overrides->m_Textures.Size();
        for (uint32_t i = 0; i < num_textures; ++i)
        {
            if (overrides->m_Textures[i].m_TextureSet) // it may be sparse
                dmResource::Release(factory, overrides->m_Textures[i].m_TextureSet);
        }
        if (overrides->m_Material)
        {
            dmResource::Release(factory, overrides->m_Material);
        }
        delete component->m_Overrides;
    }

    static inline void HashResourceOverrides(HashState32* state, SpriteResourceOverrides* overrides)
    {
        if (!overrides)
            return;

        if (overrides->m_Material)
            dmHashUpdateBuffer32(state, overrides->m_Material, sizeof(MaterialResource*));
        dmHashUpdateBuffer32(state, overrides->m_Textures.Begin(), sizeof(SpriteTexture) * overrides->m_Textures.Size());
    }

    // Keep the size/ordering up-to-date for the textures in the overrides list
    // Given a material, with an array of textures+samplers, will create an corresponding array
    // where we keep overrides. The samplers are set, but the texturesets may be null:
    //  material:  [(diffuse, textureset0), (normal, textureset1), (emissive, textureset1)]
    //  overrides: [(diffuse, null),        (normal, textureset1), (emissive, null)]
    static void UpdateOverrideTexturesArray(dmResource::HFactory factory, SpriteComponent* component, MaterialResource* material)
    {
        SpriteResourceOverrides* overrides = component->m_Overrides;

        // Create a new array
        uint32_t num_textures = material->m_NumTextures;
        dmArray<SpriteTexture> textures;
        textures.SetCapacity(num_textures);
        textures.SetSize(num_textures);
        memset(textures.Begin(), 0, sizeof(SpriteTexture) * num_textures);

        for (uint32_t i = 0; i < num_textures; ++i)
        {
            textures[i].m_SamplerNameHash = material->m_SamplerNames[i];
            textures[i].m_TextureSet = 0;
        }

        // For each kept sampler, copy the texture set
        uint32_t num_old_textures = overrides->m_Textures.Size();
        for (uint32_t i = 0; i < num_old_textures; ++i)
        {
            // Copy the texture set to the new array
            const dmhash_t sampler_name_hash = overrides->m_Textures[i].m_SamplerNameHash;
            for (uint32_t j = 0; j < num_textures; ++j)
            {
                if (sampler_name_hash == textures[j].m_SamplerNameHash)
                {
                    textures[j].m_TextureSet = overrides->m_Textures[i].m_TextureSet;
                    overrides->m_Textures[i].m_TextureSet = 0;
                    break;
                }
            }

            // if the texture set wasn't copied to the new array, it should be released
            if (overrides->m_Textures[i].m_TextureSet)
            {
                dmResource::Release(factory, overrides->m_Textures[i].m_TextureSet);
            }
        }

        overrides->m_Textures.Swap(textures);
    }

    static dmGameObject::PropertyResult AddOverrideMaterial(dmResource::HFactory factory, SpriteComponent* component, dmhash_t resource)
    {
        if (!component->m_Overrides)
            component->m_Overrides = new SpriteResourceOverrides;
        SpriteResourceOverrides* overrides = component->m_Overrides;

        dmGameObject::PropertyResult res = SetResourceProperty(factory, resource, MATERIAL_EXT_HASH, (void**)&overrides->m_Material);
        if (dmGameObject::PROPERTY_RESULT_OK == res)
            UpdateOverrideTexturesArray(factory, component, overrides->m_Material);
        return res;
    }

    static dmGameObject::PropertyResult AddOverrideTextureSet(dmResource::HFactory factory, SpriteComponent* component, dmhash_t sampler_name_hash, dmhash_t resource)
    {
        // scenarios
        // * Change material, with possibly different sampler indices
        // * Update a texture / sampler
        // * Clear a texture / sampler

        if (!component->m_Overrides)
        {
            component->m_Overrides = new SpriteResourceOverrides;

            // Make sure the array is of equal length as the materials' sampler list
            MaterialResource* material = component->m_Resource->m_Material;
            UpdateOverrideTexturesArray(factory, component, material);
        }
        SpriteResourceOverrides* overrides = component->m_Overrides;

        // At this point, the array holds each available sampler name
        TextureSetResource** texture_set = 0;

        if (sampler_name_hash == 0)
        {
            if (overrides->m_Textures.Empty())
            {
                SpriteTexture empty = {};
                overrides->m_Textures.SetCapacity(1);
                overrides->m_Textures.Push(empty);
            }
            texture_set = &overrides->m_Textures[0].m_TextureSet;
        }
        else
        {
            uint32_t num_textures = overrides->m_Textures.Size();
            for (uint32_t i = 0; i < num_textures; ++i)
            {
                if (overrides->m_Textures[i].m_SamplerNameHash == sampler_name_hash)
                {
                    texture_set = &overrides->m_Textures[i].m_TextureSet;
                    break;
                }
            }
            if (!texture_set)
            {
                return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
            }
        }

        return SetResourceProperty(factory, resource, TEXTURE_SET_EXT_HASH, (void**)texture_set);
    }

    static inline HComponentRenderConstants GetRenderConstants(const SpriteComponent* component)
    {
        return component->m_RenderConstants;
    }

    static inline MaterialResource* GetMaterialResource(const SpriteComponent* component)
    {
        const SpriteResource* resource = component->m_Resource;
        const SpriteResourceOverrides* overrides = component->m_Overrides;
        return (overrides && overrides->m_Material) ? overrides->m_Material : resource->m_Material;
    }

    static inline dmRender::HMaterial GetComponentMaterial(const SpriteComponent* component)
    {
        return GetMaterialResource(component)->m_Material;
    }

    static inline dmRender::HMaterial GetRenderMaterial(dmRender::HRenderContext render_context, const SpriteComponent* component)
    {
        dmRender::HMaterial context_material = dmRender::GetContextMaterial(render_context);
        return context_material ? context_material : GetComponentMaterial(component);
    }

    static inline TextureSetResource* GetTextureSetByIndex(const SpriteComponent* component, uint32_t index)
    {
        const SpriteResourceOverrides* overrides = component->m_Overrides;
        const SpriteTexture* texture = 0;
        if (overrides && index < overrides->m_Textures.Size())
            texture = &overrides->m_Textures[index];
        if ((!texture || !texture->m_TextureSet) && index < component->m_Resource->m_NumTextures)
            texture = &component->m_Resource->m_Textures[index];
        return texture ? texture->m_TextureSet : 0;
    }

    static inline TextureSetResource* GetTextureSetByHash(const SpriteComponent* component, dmhash_t sampler_name_hash)
    {
        if (component->m_Overrides)
        {
            for (uint32_t i = 0; i < component->m_Overrides->m_Textures.Size(); ++i)
            {
                if (sampler_name_hash == component->m_Overrides->m_Textures[i].m_SamplerNameHash)
                {
                    return component->m_Overrides->m_Textures[i].m_TextureSet;
                }
            }
        }
        for (uint32_t i = 0; i < component->m_Resource->m_NumTextures; ++i)
        {
            if (sampler_name_hash == component->m_Resource->m_Textures[i].m_SamplerNameHash)
            {
                return component->m_Resource->m_Textures[i].m_TextureSet;
            }
        }
        return 0;
    }

    // Until we can set multiple play cursors, we'll use the first texture set as the driving animation
    static inline TextureSetResource* GetFirstTextureSet(const SpriteComponent* component)
    {
        return GetTextureSetByIndex(component, 0);
    }

    TextureResource* GetTextureResource(const SpriteComponent* component, uint32_t texture_unit)
    {
        if(texture_unit >= component->m_Resource->m_NumTextures)
            return 0;

        const SpriteResourceOverrides* overrides = component->m_Overrides;
        if (overrides && texture_unit < overrides->m_Textures.Size())
        {
            if (overrides->m_Textures[texture_unit].m_TextureSet)
                return overrides->m_Textures[texture_unit].m_TextureSet->m_Texture;
        }
        return component->m_Resource->m_Textures[texture_unit].m_TextureSet->m_Texture;
    }

    uint32_t GetTextureResourceGeneration(const SpriteComponent* component, uint32_t texture_unit)
    {
        if(texture_unit >= component->m_Resource->m_NumTextures)
            return 0;

        const SpriteResourceOverrides* overrides = component->m_Overrides;
        if (overrides && texture_unit < overrides->m_Textures.Size())
        {
            if (overrides->m_Textures[texture_unit].m_TextureSet)
                return overrides->m_Textures[texture_unit].m_TextureSet->m_TexturesGeneration;
        }
        return component->m_Resource->m_Textures[texture_unit].m_TextureSet->m_TexturesGeneration;
    }

    dmGraphics::HTexture GetMaterialTexture(const SpriteComponent* component, uint32_t texture_unit)
    {
        TextureResource* texture = GetTextureResource(component, texture_unit);
        return texture ? texture->m_Texture : 0;
    }

    static void UpdateCurrentAnimationFrame(SpriteComponent* component)
    {
        TextureSetResource* texture_set = GetFirstTextureSet(component);
        if (!texture_set)
            return;

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        const dmGameSystemDDF::Playback playback = (dmGameSystemDDF::Playback)component->m_AnimationPlayback;

        // Set frame from cursor (tileindex or animframe)
        float t = component->m_AnimTimer;
        float backwards = (playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                        || playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD) ? 1.0f : 0;

        // Original: t = backwards ? (1.0f - t) : t;
        // which translates to:
        t = backwards - 2 * t * backwards + t;

        uint32_t interval = component->m_AnimationFrameCount;
        uint32_t frame_count = interval;
        if (playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG ||
            playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
        {
            frame_count = dmMath::Max(1u, frame_count * 2 - 2);
        }
        uint32_t frame = dmMath::Min(frame_count - 1, (uint32_t)(t * frame_count));
        if (frame >= interval)
        {
            frame = 2 * (interval - 1) - frame;
        }

        uint32_t frame_current = component->m_CurrentAnimationFrame;
        component->m_CurrentAnimationFrame = frame;
        component->m_AnimationReHash |= frame != frame_current;

        if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_AUTO && frame != frame_current)
        {
            component->m_Size = GetSizeFromAnimation(component, texture_set_ddf, component->m_AnimationID);
        }
    }

    static void ClearCurrentAnimation(SpriteComponent* component)
    {
        component->m_AnimationPlayback = dmGameSystemDDF::PLAYBACK_NONE;
        component->m_IsPlaying = 0;
        component->m_CurrentAnimation = 0x0;
        component->m_CurrentAnimationFrame = 0;
        component->m_AnimationID = 0;

        component->m_AnimationReHash = 1;
    }

    static bool GetCurrentOrFirstAnimation(TextureSetResource* texture_set, dmhash_t current_animation, dmhash_t* animation_out)
    {
        if (!texture_set)
            return false;

        if (texture_set->m_AnimationIds.Get(current_animation) != 0)
        {
            *animation_out = current_animation;
            return true;
        }

        dmHashTable64<uint32_t>::Iterator animation_iterator = texture_set->m_AnimationIds.GetIterator();
        if (!animation_iterator.Next())
            return false;

        *animation_out = animation_iterator.GetKey();
        return true;
    }

    static bool PlayAnimation(SpriteComponent* component, dmhash_t animation, float offset, float playback_rate)
    {
        TextureSetResource* texture_set = GetFirstTextureSet(component);
        if (!texture_set)
        {
            ClearCurrentAnimation(component);
            dmLogError("Unable to play animation '%s' since the sprite has no texture.", dmHashReverseSafe64(animation));
            return false;
        }

        uint32_t* anim_id = texture_set->m_AnimationIds.Get(animation);
        if (anim_id)
        {
            component->m_AnimationID = (uint16_t)(*anim_id);
            component->m_AnimationReHash |= component->m_CurrentAnimation != animation;
            component->m_CurrentAnimation = animation;

            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_TextureSet->m_Animations[*anim_id];
            uint32_t frame_count = animation->m_End - animation->m_Start;
            const dmGameSystemDDF::Playback playback = animation->m_Playback;
            component->m_AnimationFrameCount = frame_count;
            if (playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG ||
                playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
            {
                frame_count = dmMath::Max(1u, frame_count * 2 - 2);
            }
            component->m_AnimInvDuration = (float)animation->m_Fps / frame_count;
            component->m_AnimationPlayback = playback;
            component->m_IsPlaying = playback != dmGameSystemDDF::PLAYBACK_NONE;

            if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_AUTO)
            {
                component->m_Size = GetSizeFromAnimation(component, texture_set->m_TextureSet, component->m_AnimationID);
            }

            offset = dmMath::Clamp(offset, 0.0f, 1.0f);
            if (playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD ||
                playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD)
            {
                offset = 1.0f - offset;
            }

            component->m_PlaybackRate = dmMath::Max(playback_rate, 0.0f);
            SetCursor(component, offset);
            UpdateCurrentAnimationFrame(component);
        }
        else
        {
            ClearCurrentAnimation(component);
            dmLogError("Unable to play animation '%s' from texture '%s' since it could not be found.", dmHashReverseSafe64(animation), dmHashReverseSafe64(texture_set->m_TexturePath));
        }
        return anim_id != 0;
    }

    static void ReHash(SpriteComponent* component)
    {
        // Hash material, texture set, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        SpriteResource* resource = component->m_Resource;
        dmGameSystemDDF::SpriteDesc* ddf = resource->m_DDF;

        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        HComponentRenderConstants constants = GetRenderConstants(component);
        if (constants)
        {
            dmGameSystem::HashRenderConstants(constants, &state);
        }

        dmHashUpdateBuffer32(&state, resource->m_Textures, sizeof(SpriteTexture) * resource->m_NumTextures);
        for (size_t idx = 0; idx < resource->m_NumTextures; ++idx)
        {
            uint32_t generation = GetTextureResourceGeneration(component, idx);
            dmHashUpdateBuffer32(&state, &generation, sizeof(generation));
        }
        if (resource->m_NumTextures > 0)
        {
            dmHashUpdateBuffer32(&state, resource->m_Textures->m_TextureSet, sizeof(resource->m_Textures->m_TextureSet));
        }
        dmHashUpdateBuffer32(&state, resource->m_Material, sizeof(MaterialResource*));

        HashResourceOverrides(&state, component->m_Overrides);

        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
        component->m_AnimationReHash = 1;
    }

    static void AnimationReHash(SpriteComponent* component)
    {
        // component, component->m_MixedHash, component->m_CurrentAnimation, component->m_CurrentAnimationFrame
        HashState32 state;
        dmHashInit32(&state, false);
        dmHashUpdateBuffer32(&state, &component->m_MixedHash, sizeof(component->m_MixedHash));
        dmHashUpdateBuffer32(&state, &component->m_CurrentAnimation, sizeof(component->m_CurrentAnimation));
        dmHashUpdateBuffer32(&state, &component->m_CurrentAnimationFrame, sizeof(component->m_CurrentAnimationFrame));
        component->m_AnimationDataHash = dmHashFinal32(&state);
        component->m_AnimationReHash = 0;
    }

    dmGameObject::CreateResult CompSpriteCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        if (sprite_world->m_Components.Full())
        {
            ShowFullBufferError("Sprite", SPRITE_MAX_COUNT_KEY, sprite_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_TOO_MANY_COMPONENTS;
        }
        uint32_t index = sprite_world->m_Components.Alloc();
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        memset(component, 0, sizeof(SpriteComponent));

        component->m_Instance = params.m_Instance;
        component->m_Position = Vector3(params.m_Position);
        component->m_Rotation = params.m_Rotation;
        component->m_Scale = params.m_Scale;
        SpriteResource* resource = (SpriteResource*)params.m_Resource;
        component->m_Resource = resource;
        // narrow to 8 bits because I don't believe that sprite can have more than 255 textures
        component->m_NumTextures = (uint8_t)resource->m_NumTextures;
        component->m_Overrides = 0;

        dmMessage::ResetURL(&component->m_Listener);

        component->m_VertexCount = 0;
        component->m_VertexStride = 1;
        component->m_IndexCount = 0;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_FunctionRef = 0;
        component->m_ReHash = 1;
        component->m_AnimationReHash = 1;
        component->m_Slice9 = component->m_Resource->m_DDF->m_Slice9;
        component->m_UseSlice9 = sum(component->m_Slice9) != 0 &&
                component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_MANUAL;

        component->m_DynamicVertexAttributeIndex = INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        component->m_Size = Vector3(0.0f, 0.0f, 0.0f);
        component->m_AnimationID = 0;
        component->m_AnimationPlayback = dmGameSystemDDF::PLAYBACK_NONE;
        component->m_AnimationFrameCount = 1;
        component->m_PivotX = 0.5f;
        component->m_PivotY = 0.5f;

        if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_MANUAL || component->m_NumTextures == 0)
        {
            component->m_Size[0] = component->m_Resource->m_DDF->m_Size.getX();
            component->m_Size[1] = component->m_Resource->m_DDF->m_Size.getY();
        }

        if (component->m_NumTextures > 0)
        {
            PlayAnimation(component, resource->m_DefaultAnimation,
                    component->m_Resource->m_DDF->m_Offset, component->m_Resource->m_DDF->m_PlaybackRate);
        }

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    void* CompSpriteGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        SpriteWorld* world = (SpriteWorld*)params.m_World;
        return (void*)&world->m_Components.Get(params.m_UserData);
    }

    dmGameObject::CreateResult CompSpriteDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);

        HComponentRenderConstants constants = GetRenderConstants(component);
        if (constants)
        {
            dmGameSystem::DestroyRenderConstants(constants);
        }

        DeleteOverrides(factory, component);

        FreeMaterialAttribute(sprite_world->m_DynamicVertexAttributePool, component->m_DynamicVertexAttributeIndex);

        sprite_world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void FillSlice9Uvs(const float us[4], const float vs[4], bool rotated, float uvs[SPRITE_VERTEX_COUNT_SLICE9*2]) {
        int index = 0;
        for (int y=0; y<4; ++y)
        {
            for (int x=0; x<4; ++x, ++index)
            {
                if (rotated)
                {
                    uvs[index*2+0] = us[y];
                    uvs[index*2+1] = vs[x];
                }
                else
                {
                    uvs[index*2+0] = us[x];
                    uvs[index*2+1] = vs[y];
                }
            }
        }
    }

    static inline void FillWriteVertexAttributeParams(dmGraphics::WriteAttributeParams* params,
        const dmGraphics::VertexAttributeInfos* attribute_infos,
        const float** world_matrix,
        const float** positions_world_space,
        const float** positions_local_space,
        const float** uv_channels, uint8_t uv_channels_count,
        const float** pi_channels, uint8_t pi_channels_count,
        const float** tt_channels, uint8_t tt_channels_count)
    {
        memset(params, 0, sizeof(dmGraphics::WriteAttributeParams));
        params->m_VertexAttributeInfos = attribute_infos;
        params->m_StepFunction         = dmGraphics::VERTEX_STEP_FUNCTION_VERTEX;

        // Per vertex channels
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_PositionsWorldSpace, positions_world_space, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_PositionsLocalSpace, positions_local_space, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC4, 1, false);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_TexCoords, uv_channels, dmGraphics::VertexAttribute::VECTOR_TYPE_VEC2, uv_channels_count, false);

        // Global channels (data repeated per vertex)
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_WorldMatrix, world_matrix, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT4, 1, true);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_PageIndices, pi_channels, dmGraphics::VertexAttribute::VECTOR_TYPE_SCALAR, pi_channels_count, true);
        dmGraphics::SetWriteAttributeStreamDesc(&params->m_TextureTransform2D, tt_channels, dmGraphics::VertexAttribute::VECTOR_TYPE_MAT3, tt_channels_count, true);
    }

    static inline void GetPivot(const AnimationData* animation_data, float* out_x_pivot, float* out_y_pivot)
    {
        // We always use the first geometry for the vertices
        *out_x_pivot = 0.f;
        *out_y_pivot = 0.f;
        const dmGameSystemDDF::SpriteGeometry* geometry = animation_data->m_Geometries[0]; // textures_num == 0 then all elements in the array should 0x0
        if (geometry)
        {
            *out_x_pivot = geometry->m_PivotX;
            *out_y_pivot = geometry->m_PivotY;
        }
    }

    static inline void UpdateVertexMetricsCache(SpriteComponent* component, const AnimationData* anim_data, dmRender::HRenderContext render_context)
    {
        dmRender::HMaterial material           = GetRenderMaterial(render_context, component);
        dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material);
        component->m_VertexStride              = dmGraphics::GetVertexDeclarationStride(vx_decl);
        uint8_t textures_num                   = component->m_NumTextures;

        if (textures_num == 0 || anim_data->m_CanUseQuads)
        {
            if (component->m_UseSlice9)
            {
                component->m_VertexCount   = SPRITE_VERTEX_COUNT_SLICE9;
                component->m_IndexCount    = SPRITE_INDEX_COUNT_SLICE9;
            }
            else
            {
                component->m_VertexCount   = SPRITE_VERTEX_COUNT_LEGACY;
                component->m_IndexCount    = SPRITE_INDEX_COUNT_LEGACY;
            }
        }
        else
        {
            component->m_VertexCount = anim_data->m_VertexCount;
            component->m_IndexCount = anim_data->m_IndicesCount;
        }
    }

    static void CreateVertexDataSlice9(
        const SpriteComponent* component,
        uint8_t* vertices,
        uint8_t* indices,
        bool is_indices_16_bit,
        bool has_world_position_attribute,
        bool has_local_position_attribute,
        const Matrix4& world_matrix,
        uint32_t vertex_offset,
        uint32_t vertex_stride,
        const AnimationData* anim_data,
        dmArray<float>* scratch_uvs,
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT],
        const float* scratch_pi_ptrs[MAX_TEXTURE_COUNT],
        const float* scratch_tt_ptrs[MAX_TEXTURE_COUNT],
        dmArray<dmVMath::Vector4>* scratch_positions_world,
        dmArray<dmVMath::Vector4>* scratch_positions_local,
        dmGraphics::VertexAttributeInfos* sprite_infos)
    {
        Vector3 sprite_size = component->m_Size;
        Vector4 slice9 = component->m_Slice9;
        bool flip_u = component->m_FlipHorizontal == 1;
        bool flip_v = component->m_FlipVertical == 1;
        uint8_t texture_num = component->m_NumTextures;
        // render 9-sliced node
        //   0 1     2 3
        // 0 *-*-----*-*
        //   | |  y  | |
        // 1 *-*-----*-*
        //   | |     | |
        //   |x|     |z|
        //   | |     | |
        // 2 *-*-----*-*
        //   | |  w  | |
        // 3 *-*-----*-*

        uint32_t uv_channels_count = 0;

        for (uint8_t i = 0; i < texture_num; ++i)
        {
            dmArray<float>& uvs = scratch_uvs[i];
            uvs.EnsureSize(SPRITE_VERTEX_COUNT_SLICE9*2);

            uint32_t frame_index = anim_data->m_Frames[i];
            if (frame_index == 0xFFFFFFFF)
            {
                // The animation frame wasn't found in the textureset.
                memset(uvs.Begin(), 0, uvs.Size());
                continue;
            }

            const TextureSetResource* texture_set_resource = GetTextureSetByIndex(component, i);
            const dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set_resource->m_TextureSet;
            const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;
            const float* tc         = &tex_coords[frame_index * 4 * 2];

            float us[4], vs[4];

            uint32_t texture_width = texture_set_ddf->m_Width;
            uint32_t texture_height = texture_set_ddf->m_Height;

            const float su = 1.0f / texture_width;
            const float sv = 1.0f / texture_height;

            static const uint32_t uvIndex[2][4] = {{0,1,2,3}, {3,2,1,0}};
            bool uv_rotated = tc[0] != tc[2] && tc[3] != tc[5];
            if(uv_rotated)
            {
                const uint32_t *uI = flip_v ? uvIndex[1] : uvIndex[0];
                const uint32_t *vI = flip_u ? uvIndex[1] : uvIndex[0];
                us[uI[0]] = tc[0];
                us[uI[1]] = tc[0] + (su * slice9.getW());
                us[uI[2]] = tc[2] - (su * slice9.getY());
                us[uI[3]] = tc[2];
                vs[vI[0]] = tc[1];
                vs[vI[1]] = tc[1] - (sv * slice9.getX());
                vs[vI[2]] = tc[5] + (sv * slice9.getZ());
                vs[vI[3]] = tc[5];
            }
            else
            {
                const uint32_t *uI = flip_u ? uvIndex[1] : uvIndex[0];
                const uint32_t *vI = flip_v ? uvIndex[1] : uvIndex[0];
                us[uI[0]] = tc[0];
                us[uI[1]] = tc[0] + (su * slice9.getX());
                us[uI[2]] = tc[4] - (su * slice9.getZ());
                us[uI[3]] = tc[4];
                vs[vI[0]] = tc[1];
                vs[vI[1]] = tc[1] + (sv * slice9.getW());
                vs[vI[2]] = tc[3] - (sv * slice9.getY());
                vs[vI[3]] = tc[3];
            }

            FillSlice9Uvs(us, vs, uv_rotated, uvs.Begin());

            scratch_uv_ptrs[i] = uvs.Begin();
            scratch_pi_ptrs[i] = &anim_data->m_PageIndices[i];
            scratch_tt_ptrs[i] = &anim_data->m_TextureTransformsPacked[i][0];
            uv_channels_count++;
        }

        // disable slice9 computation below a certain threshold
        // (avoid div by zero)
        const float s9_min_dim = 0.001f;

        if (texture_num == 0)
        {
            dmArray<float>& uvs = scratch_uvs[0];
            uvs.EnsureSize(SPRITE_VERTEX_COUNT_SLICE9*2);

            float us[4];
            float vs[4];
            us[0] = 0.0f;
            us[1] = sprite_size.getX() > s9_min_dim ? slice9.getX() / sprite_size.getX() : 0.0f;
            us[2] = 1.0f - (sprite_size.getX() > s9_min_dim ? slice9.getZ() / sprite_size.getX() : 0.0f);
            us[3] = 1.0f;

            vs[0] = 0.0f;
            vs[1] = sprite_size.getY() > s9_min_dim ? slice9.getY() / sprite_size.getY() : 0.0f;
            vs[2] = 1.0f - (sprite_size.getY() > s9_min_dim ? slice9.getW() / sprite_size.getY() : 0.0f);
            vs[3] = 1.0f;

            FillSlice9Uvs(us, vs, false, uvs.Begin());

            scratch_uv_ptrs[0] = uvs.Begin();
            scratch_pi_ptrs[0] = &anim_data->m_PageIndices[0];
            scratch_tt_ptrs[0] = &anim_data->m_TextureTransformsPacked[0][0];
            uv_channels_count  = 1;
        }

        const float sx = sprite_size.getX() > s9_min_dim ? 1.0f / sprite_size.getX() : 0;
        const float sy = sprite_size.getY() > s9_min_dim ? 1.0f / sprite_size.getY() : 0;

        float xs[4], ys[4];
        // v are '1-v'
        xs[0] = ys[0] = 0;
        xs[3] = ys[3] = 1;

        // Flipping the UV grid also moves the fixed-size slice borders to the
        // opposite side of the sprite. Keep the geometry subdivisions aligned
        // with the reversed UV subdivisions for asymmetric slice values.
        xs[1] = sx * (flip_u ? slice9.getZ() : slice9.getX());
        xs[2] = 1 - sx * (flip_u ? slice9.getX() : slice9.getZ());
        ys[1] = sy * (flip_v ? slice9.getY() : slice9.getW());
        ys[2] = 1 - sy * (flip_v ? slice9.getW() : slice9.getY());

        if (has_world_position_attribute)
        {
            scratch_positions_world->EnsureSize(SPRITE_VERTEX_COUNT_SLICE9);
        }

        if (has_local_position_attribute)
        {
            scratch_positions_local->EnsureSize(SPRITE_VERTEX_COUNT_SLICE9);
        }

        const float* world_matrix_channels[] = { (float*) &world_matrix };
        const float* local_position_channels[] = { (float*) scratch_positions_local->Begin() };
        const float* world_position_channels[] = { (float*) scratch_positions_world->Begin() };

        dmGraphics::WriteAttributeParams params = {};
        FillWriteVertexAttributeParams(&params,
            sprite_infos,
            world_matrix_channels, 
            world_position_channels,
            local_position_channels,
            (const float**) scratch_uv_ptrs,
            uv_channels_count,
            (const float**) scratch_pi_ptrs,
            uv_channels_count,
            (const float**) scratch_tt_ptrs,
            uv_channels_count);

        uint32_t sp_width = sprite_size.getX();
        uint32_t sp_height = sprite_size.getY();
        uint32_t vertex_index = 0;
        for (int y=0; y<4; y++)
        {
            for (int x=0; x<4; x++)
            {
                if (has_local_position_attribute || has_world_position_attribute)
                {
                    // convert from [0,1] to [-0.5, 0.5]
                    float px = xs[x] - 0.5f;
                    float py = ys[y] - 0.5f;
                    Point3 p = Point3(px - component->m_PivotX, py - component->m_PivotY, 0);

                    // Local space has size applied; world = mtx_world * local (mtx_world has no scale)
                    Vector4 local_pos(p.getX() * sp_width, p.getY() * sp_height, 0.0f, 1.0f);

                    if (has_local_position_attribute)
                    {
                        (*scratch_positions_local)[vertex_index] = local_pos;
                    }
                    if (has_world_position_attribute)
                    {
                        (*scratch_positions_world)[vertex_index] = world_matrix * local_pos;
                    }
                }

                vertices = dmGraphics::WriteAttributes(vertices, vertex_index++, 1, params);
            }
        }

        uint32_t index = 0;
        for (int y=0;y<3;y++)
        {
            for (int x=0;x<3;x++)
            {
                uint32_t p0 = vertex_offset + y * 4 + x;
                uint32_t p1 = p0 + 1;
                uint32_t p2 = p0 + 4;
                uint32_t p3 = p2 + 1;

                if (is_indices_16_bit)
                {
                    uint16_t* indices_16 = (uint16_t*) indices;
                    // Triangle 1
                    indices_16[index++] = p0;
                    indices_16[index++] = p1;
                    indices_16[index++] = p2;
                    // Triangle 2
                    indices_16[index++] = p2;
                    indices_16[index++] = p1;
                    indices_16[index++] = p3;
                }
                else
                {
                    uint32_t* indices_32 = (uint32_t*) indices;
                    // Triangle 1
                    indices_32[index++] = p0;
                    indices_32[index++] = p1;
                    indices_32[index++] = p2;
                    // Triangle 2
                    indices_32[index++] = p2;
                    indices_32[index++] = p1;
                    indices_32[index++] = p3;
                }
            }
        }
    }

    static void ResolveAnimationData(const SpriteComponent* component, AnimationData* data)
    {
        dmhash_t anim_id = component->m_CurrentAnimation;
        uint32_t current_anim_frame_index = component->m_CurrentAnimationFrame;
        data->m_AnimationID = anim_id;

        // For the first texture set, we figure out the actual frame index,
        // and from that index we figure out the name of that single frame animation.
        // We can then use that frame animation name to lookup an animation with same name in another atlas
        dmhash_t frame_anim_id = 0xFFFFFFFFFFFFFFFF;

        bool uses_geometries = false;

        uint8_t texture_num = component->m_NumTextures;
        for (uint8_t i = 0; i < texture_num; ++i)
        {
            TextureSetResource* resource = GetTextureSetByIndex(component, i);
            data->m_TextureGenerations[i] = resource->m_TexturesGeneration;

            const dmGameSystemDDF::TextureSet* texture_set_ddf = resource->m_TextureSet;
            const uint32_t* frame_indices = texture_set_ddf->m_FrameIndices.m_Data;
            const uint32_t* page_indices = texture_set_ddf->m_PageIndices.m_Data;
            const dmGameSystemDDF::SpriteGeometry* geometries = texture_set_ddf->m_Geometries.m_Data;

            uint32_t* anim_index = resource->m_AnimationIds.Get(anim_id);
            if (anim_index)
                data->m_Animations[i] = &texture_set_ddf->m_Animations[*anim_index];
            else
                data->m_Animations[i] = &texture_set_ddf->m_Animations[0]; // If the animation doesn't exist in the atlas, then fallback to the first animation (old behavior)

            uint32_t frame_index = 0xFFFFFFFF;
            if (frame_anim_id == 0xFFFFFFFFFFFFFFFF)
            {
                uint32_t anim_frame_index = data->m_Animations[i]->m_Start + current_anim_frame_index;
                frame_index = frame_indices[anim_frame_index];

                // The name hash of the current single frame animation
                if (frame_index < texture_set_ddf->m_ImageNameHashes.m_Count)
                    frame_anim_id = texture_set_ddf->m_ImageNameHashes[frame_index];
            }
            else
            {
                // Use the name hash of the current single frame animation from the driving atlas
                // to lookup the frame number in this atlas
                uint32_t* resource_frame_index = resource->m_FrameIds.Get(frame_anim_id);
                if (!resource_frame_index)
                {
                    // Missing image in this atlas, we need to skip this texture slot
                    data->m_Frames[i] = 0xFFFFFFFF;
                    continue;
                }

                frame_index = *resource_frame_index;
            }

            data->m_Frames[i]       = frame_index;
            data->m_PageIndices[i]  = (float) page_indices[frame_index];
            data->m_Geometries[i]   = &geometries[frame_index];

            const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;
            const float* tc         = &tex_coords[frame_index * 4 * 2];

            // Full 2D affine from unit square (s,t) to atlas: p = mat * vec3(s,t,1). GLSL mat3 columns are
            // [0]=∂p/∂s, [1]=∂p/∂t, [2]=translation (BL). Corner order from TextureSetGenerator.putRect:
            // unrotated: BL, TL, TR, BR; rotated 90° CW in atlas: TL, TR, BR, BL.
            // Same uv_rotated test as slice-9 in this file (geometry->m_Rotated is not needed here — particles can match).
            const bool uv_rotated = (tc[0] != tc[2]) && (tc[3] != tc[5]);
            float* tt_packed = data->m_TextureTransformsPacked[i];
            if (uv_rotated)
            {
                tt_packed[0] = tc[4] - tc[6];
                tt_packed[1] = tc[5] - tc[7];
                tt_packed[2] = 0.0f;
                tt_packed[3] = tc[0] - tc[6];
                tt_packed[4] = tc[1] - tc[7];
                tt_packed[5] = 0.0f;
                tt_packed[6] = tc[6];
                tt_packed[7] = tc[7];
                tt_packed[8] = 1.0f;
            }
            else
            {
                tt_packed[0] = tc[6] - tc[0];
                tt_packed[1] = tc[7] - tc[1];
                tt_packed[2] = 0.0f;
                tt_packed[3] = tc[2] - tc[0];
                tt_packed[4] = tc[3] - tc[1];
                tt_packed[5] = 0.0f;
                tt_packed[6] = tc[0];
                tt_packed[7] = tc[1];
                tt_packed[8] = 1.0f;
            }

            uses_geometries |= data->m_Geometries[i]->m_TrimMode != dmGameSystemDDF::SPRITE_TRIM_MODE_OFF;
        }

        data->m_CanUseQuads = !uses_geometries || data->m_Geometries[0]->m_TrimMode == dmGameSystemDDF::SPRITE_TRIM_MODE_OFF;

        if (!data->m_CanUseQuads)
        {
            TextureSetResource* texture_set                     = GetFirstTextureSet(component);
            dmGameSystemDDF::TextureSet* texture_set_ddf        = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animations    = texture_set_ddf->m_Animations.m_Data;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &animations[component->m_AnimationID];
            uint32_t* frame_indices                             = texture_set_ddf->m_FrameIndices.m_Data;
            uint32_t frame_index                                = frame_indices[animation_ddf->m_Start + component->m_CurrentAnimationFrame];
            dmGameSystemDDF::SpriteGeometry* geometries         = texture_set_ddf->m_Geometries.m_Data;
            dmGameSystemDDF::SpriteGeometry* geometry           = &geometries[frame_index];
            uint32_t geometry_vx_count                          = geometry->m_Vertices.m_Count / 2;

            data->m_VertexCount   = geometry_vx_count; // (x,y) coordinates
            data->m_IndicesCount  = geometry->m_Indices.m_Count;
        }
    }

    static void ResolveUVDataFromQuads(
        const SpriteComponent* component,
        const AnimationData* data,
        dmArray<float>* scratch_uvs,
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT],
        const float* scratch_pi_ptrs[MAX_TEXTURE_COUNT],
        const float* tex_transform_ptrs[MAX_TEXTURE_COUNT])
    {
        static int tex_coord_order[] = {
            0,1,2,2,3,0,    // no flip
            3,2,1,1,0,3,    // flip h
            1,0,3,3,2,1,    // flip v
            2,3,0,0,1,2     // flip hv
        };
        uint16_t flip_horizontal = component->m_FlipHorizontal;
        uint16_t flip_vertical = component->m_FlipVertical;
        uint8_t texture_num = component->m_NumTextures;

        for (uint8_t i = 0; i < texture_num; ++i)
        {
            dmArray<float>& uvs = scratch_uvs[i];
            uvs.EnsureSize(4*2);

            uint32_t frame_index = data->m_Frames[i];
            if (frame_index == 0xFFFFFFFF)
            {
                // The animation frame wasn't found in the textureset.
                memset(uvs.Begin(), 0, uvs.Size());
                continue;
            }

            const TextureSetResource* texture_set_resource = GetTextureSetByIndex(component, i);
            const dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set_resource->m_TextureSet;
            const dmGameSystemDDF::TextureSetAnimation* animation_ddf = data->m_Animations[i];
            if (!animation_ddf)
            {
                memset(uvs.Begin(), 0, sizeof(float)*uvs.Size());
                continue;
            }

            const float* tex_coords     = (const float*) texture_set_ddf->m_TexCoords.m_Data;
            const float* tc             = &tex_coords[frame_index * 4 * 2];
            uint32_t flip_flag          = 0;

            // ddf values are guaranteed to be 0 or 1 when saved by the editor
            // component values are guaranteed to be 0 or 1
            if (animation_ddf->m_FlipHorizontal ^ flip_horizontal)
            {
                flip_flag = 1;
            }
            if (animation_ddf->m_FlipVertical ^ flip_vertical)
            {
                flip_flag |= 2;
            }

            const int* tex_lookup = &tex_coord_order[flip_flag * 6];
            uvs[0] = tc[tex_lookup[0] * 2 + 0];
            uvs[1] = tc[tex_lookup[0] * 2 + 1];
            uvs[2] = tc[tex_lookup[1] * 2 + 0];
            uvs[3] = tc[tex_lookup[1] * 2 + 1];
            uvs[4] = tc[tex_lookup[2] * 2 + 0];
            uvs[5] = tc[tex_lookup[2] * 2 + 1];
            uvs[6] = tc[tex_lookup[4] * 2 + 0];
            uvs[7] = tc[tex_lookup[4] * 2 + 1];

            scratch_uv_ptrs[i] = uvs.Begin();
            scratch_pi_ptrs[i] = &data->m_PageIndices[i];
            tex_transform_ptrs[i] = &data->m_TextureTransformsPacked[i][0];
        }

        if (texture_num == 0)
        {
            dmArray<float>& uvs = scratch_uvs[0];
            uvs.EnsureSize(4*2);

            // top left
            uvs[0] = 0.0f;
            uvs[1] = 0.0f;

            // bottom left
            uvs[2] = 0.0f;
            uvs[3] = 1.0f;

            // bottom right
            uvs[4] = 1.0f;
            uvs[5] = 1.0f;

            // top right
            uvs[6] = 1.0f;
            uvs[7] = 0.0f;

            scratch_uv_ptrs[0] = uvs.Begin();
            scratch_pi_ptrs[0] = &data->m_PageIndices[0];
        }
    }

    // Since each texture set may have different trimming, the geometry for each image may not map 1:1.
    // We therefore use the geometry of the first texture set as vertices.
    // Then, for each texture set, we map local vertex ([-0.5,0.5]) into a final UV for each image
    // It of course has some caveats:
    //   * The geometry may not map 1:1, and for polygon packed atlases, it may result in texture bleeding
    static void ResolvePositionAndUVDataFromGeometry(const SpriteComponent *component,
        const AnimationData* anim_data,
        dmArray<Vector4>& scratch_pos,
        dmArray<float>* scratch_uvs,
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT],
        const float* scratch_pi_ptrs[MAX_TEXTURE_COUNT],
        const float* scratch_tt_ptrs[MAX_TEXTURE_COUNT],
        float scale_x, float scale_y, int reverse)
    {
        uint32_t num_vertices = anim_data->m_Geometries[0]->m_Vertices.m_Count / 2;
        float* orig_vertices = anim_data->m_Geometries[0]->m_Vertices.m_Data;
        int step = reverse ? -2 : 2;

        scratch_pos.EnsureSize(num_vertices);

        uint8_t textures_num = component->m_NumTextures;
        for (uint8_t i = 0; i < textures_num; ++i)
        {
            dmArray<float>& uvs = scratch_uvs[i];
            uvs.EnsureSize(num_vertices * 2);

            scratch_uv_ptrs[i] = uvs.Begin();
            scratch_pi_ptrs[i] = &anim_data->m_PageIndices[i];
            scratch_tt_ptrs[i] = &anim_data->m_TextureTransformsPacked[i][0];

            TextureSetResource* texture_set_resource = GetTextureSetByIndex(component, i);
            dmGameSystemDDF::TextureSet* texture_set = texture_set_resource->m_TextureSet;
            uint32_t width = texture_set->m_Width;
            uint32_t height = texture_set->m_Height;

            const dmGameSystemDDF::SpriteGeometry* geometry = anim_data->m_Geometries[i];
            bool rotated = geometry->m_Rotated; // if true, rotate 90 deg (CCW)
            // width/height are not rotated
            float image_width = geometry->m_Width;
            float image_height = geometry->m_Height;
            if (rotated)
            {
                float t = image_width;
                image_width = image_height;
                image_height = t;
            }
            // center X/Y may be rotated, if the image is stored rotated
            float center_x = geometry->m_CenterX;
            float center_y = geometry->m_CenterY;

            const float* vertices = reverse ? orig_vertices + num_vertices*2 - 2 : orig_vertices;

            for (uint32_t j = 0; j < num_vertices; ++j, vertices += step)
            {
                // local coordinates in range [-0.5, 0.5]
                // No need to rotate these, instead we transform these vertices into correct uv space for each image
                float px = vertices[0];
                float py = vertices[1];

                // local coordinates in range ([-image_width, image_width], [-image_height, image_height])
                float ix = px;
                float iy = py;

                // A rotated image is stored with a 90 deg CW rotation
                // so we need to convert the vertices into the uv space of that image
                if (rotated) // rotate 90 degrees CW
                {
                    float t = iy;
                    iy = -ix;
                    ix = t;
                }

                float u = (center_x + ix * image_width) / width;
                float v = (center_y + -iy * image_height) / height;

                uvs[j*2+0] = u;
                uvs[j*2+1] = 1.0f - v;

                // We grab the geometry as positions from the first texture
                if (i == 0)
                {
                    float vx = px - component->m_PivotX;
                    float vy = py - component->m_PivotY;
                    scratch_pos[j] = Vector4(vx * scale_x, vy * scale_y, 0.0f, 1.0f);
                }
            }
        }
    }

    static AnimationData* GetOrCreateAnimationData(SpriteWorld* sprite_world, const SpriteComponent* component)
    {
        // 1. Search in hastable
        uint32_t hash = component->m_AnimationDataHash;
        AnimationData** found = sprite_world->m_AnimationDataCache.m_Cache.Get(hash);
        if (found)
        {
            // Runtime resource.set also replaces atlas DDF in release builds,
            // where reload callbacks are disabled. Never trust cached pointers
            // solely because the component's animation hash stayed unchanged.
            bool current = true;
            for (uint32_t i = 0; current && i < component->m_NumTextures; ++i)
                current = (*found)->m_TextureGenerations[i] == GetTextureResourceGeneration(component, i);
            if (!current)
            {
                dmDoubleLinkedList::ListRemove(&sprite_world->m_AnimationDataCache.m_LRU, (dmDoubleLinkedList::ListNode*)*found);
                free(*found);
                sprite_world->m_AnimationDataCache.m_Cache.Erase(hash);
                found = 0;
            }
        }
        if (found != 0x0)
        {
            // updates only once per frame
            if ((*found)->m_LastAccessTick != sprite_world->m_AnimationDataCache.m_CurrentEngineTick)
            {
                dmDoubleLinkedList::ListNode* node = (dmDoubleLinkedList::ListNode*)(*found);
                dmDoubleLinkedList::ListRemove(&sprite_world->m_AnimationDataCache.m_LRU, node);
                dmDoubleLinkedList::ListAdd(&sprite_world->m_AnimationDataCache.m_LRU, node);
                (*found)->m_LastAccessTick = sprite_world->m_AnimationDataCache.m_CurrentEngineTick;
            }

            return *found;
        }

        // 2. Create if doesn't exist
        // Get the correct animation frames, and other meta data
        AnimationData* anim_data = (AnimationData*)malloc(sizeof(AnimationData));
        memset(anim_data, 0, sizeof(AnimationData));
        ResolveAnimationData(component, anim_data);

        anim_data->m_CreatedTick = sprite_world->m_AnimationDataCache.m_CurrentEngineTick;
        anim_data->m_LastAccessTick = sprite_world->m_AnimationDataCache.m_CurrentEngineTick;
        anim_data->m_CacheKey = hash;
        if (sprite_world->m_AnimationDataCache.m_Cache.Full())
        {
            sprite_world->m_AnimationDataCache.m_Cache.OffsetCapacity(30);
        }
        sprite_world->m_AnimationDataCache.m_Cache.Put(hash, anim_data);
        dmDoubleLinkedList::ListAdd(&sprite_world->m_AnimationDataCache.m_LRU, (dmDoubleLinkedList::ListNode*)anim_data);
        return anim_data;
    }

    static const uint32_t INVALID_FRAME_INDEX = 0xffffffff;

    template <typename T> static void FramePush(dmArray<T>& array, const T& value)
    {
        if (array.Full())
            array.SetCapacity(dmMath::Max(16U, array.Capacity() * 2));
        array.Push(value);
    }

    static bool GrowSpriteFrame(SpriteRenderFrame* frame, uint64_t bytes, uint64_t old_bytes)
    {
        if (frame->m_Overflow)
            return false;
        if (frame->m_CapacityLimit && frame->m_CapacityBytes + bytes + old_bytes > frame->m_CapacityLimit)
        {
            frame->m_Overflow = true;
            return false;
        }
        if (frame->m_Builder)
        {
            if (!dmRender::AdmitRenderFrameAllocation(frame->m_Builder, bytes, old_bytes))
            {
                frame->m_Overflow = true;
                return false;
            }
            frame->m_Builder->m_Frame->m_Payloads[0].m_Capacity += bytes;
        }
        frame->m_GrowthPeakBytes = dmMath::Max(frame->m_GrowthPeakBytes, frame->m_CapacityBytes + bytes + old_bytes);
        frame->m_CapacityBytes += bytes;
        return true;
    }

    template <typename T> static bool FramePush(SpriteRenderFrame* frame, dmArray<T>& array, const T& value)
    {
        if (frame->m_Overflow)
            return false;
        if (array.Full())
        {
            uint32_t capacity = dmMath::Max(16U, array.Capacity() * 2);
            if (!GrowSpriteFrame(frame, (uint64_t)(capacity - array.Capacity()) * sizeof(T), (uint64_t)array.Capacity() * sizeof(T)))
                return false;
            array.SetCapacity(capacity);
        }
        array.Push(value);
        return true;
    }

    template <typename K> static void FramePut(SpriteRenderFrame* frame, dmHashTable<K, uint32_t>& map, K key, uint32_t value)
    {
        if (frame->m_Overflow)
            return;
        if (map.Full())
        {
            uint32_t old_capacity = map.Capacity();
            uint32_t capacity = dmMath::Max(16U, old_capacity * 2);
            uint64_t old_bytes = old_capacity ? old_capacity * sizeof(typename dmHashTable<K, uint32_t>::Entry) + dmMath::Max(1U, old_capacity * 2 / 3) * sizeof(uint32_t) : 0;
            uint64_t new_bytes = capacity * sizeof(typename dmHashTable<K, uint32_t>::Entry) + dmMath::Max(1U, capacity * 2 / 3) * sizeof(uint32_t);
            if (!GrowSpriteFrame(frame, new_bytes - old_bytes, old_bytes))
                return;
            map.SetCapacity(capacity);
        }
        map.Put(key, value);
    }

    static void ReleaseSpriteFrame(SpriteRenderFrame* frame, dmResource::HFactory factory)
    {
        for (uint32_t i = 0; !frame->m_CentralDependencies && i < frame->m_Bindings.Size(); ++i)
        {
            SpriteFrameBinding& binding = frame->m_Bindings[i];
            for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
                dmResource::Release(factory, binding.m_Textures[t].m_TextureSet);
            dmResource::Release(factory, binding.m_Resolved.m_Material);
            dmResource::Release(factory, binding.m_Source);
        }
        frame->m_Sprites.SetSize(0);
        frame->m_Bounds.SetSize(0);
        frame->m_Bindings.SetSize(0);
        frame->m_Geometry.SetSize(0);
        frame->m_Constants.SetSize(0);
        frame->m_ConstantNext.SetSize(0);
        frame->m_ConstantDescriptors.SetSize(0);
        frame->m_ConstantValues.SetSize(0);
        frame->m_Attributes.SetSize(0);
        frame->m_AttributeValues.SetSize(0);
        frame->m_ResourceBytes = 0;
        frame->m_ResourceSizes.Clear();
        frame->m_BindingMap.Clear();
        frame->m_GeometryMap.Clear();
        frame->m_ConstantMap.Clear();
    }

    static void CaptureResourceSize(SpriteRenderFrame* frame, dmResource::HFactory factory, void* resource)
    {
        uint64_t key = (uintptr_t)resource;
        if (frame->m_ResourceSizes.Get(key))
            return;
        dmhash_t path;
        HResourceDescriptor descriptor;
        if (dmResource::GetPath(factory, resource, &path) == dmResource::RESULT_OK &&
            ResourceGetDescriptorByHash(factory, path, &descriptor) == RESOURCE_RESULT_OK)
        {
            uint32_t bytes = ResourceDescriptorGetResourceSize(descriptor);
            FramePut(frame, frame->m_ResourceSizes, key, bytes);
            if (!frame->m_Overflow)
                frame->m_ResourceBytes += bytes;
        }
    }

    static uint32_t CaptureBinding(SpriteRenderFrame* frame, const SpriteComponent* component, dmResource::HFactory factory, uint32_t previous)
    {
        // Adjacent sprites commonly share all bindings. Avoid constructing and
        // hashing a temporary descriptor, but check effective overrides too.
        // The index is local to this capture; no resource generation is cached.
        if (previous != INVALID_FRAME_INDEX)
        {
            const SpriteFrameBinding& candidate = frame->m_Bindings[previous];
            bool equal = candidate.m_Source == component->m_Resource &&
                         candidate.m_Resolved.m_Material == GetMaterialResource(component);
            for (uint32_t t = 0; equal && t < component->m_Resource->m_NumTextures; ++t)
                equal = candidate.m_Textures[t].m_TextureSet == GetTextureSetByIndex(component, t);
            if (equal)
                return previous;
        }
        SpriteFrameBinding binding = {};
        binding.m_Source = component->m_Resource;
        binding.m_Resolved = *component->m_Resource;
        binding.m_Resolved.m_Material = GetMaterialResource(component);
        binding.m_Resolved.m_Textures = 0; // Fixed up after the table stops growing.
        uintptr_t key[MAX_TEXTURE_COUNT + 2] = {};
        key[0] = (uintptr_t)binding.m_Source;
        key[1] = (uintptr_t)binding.m_Resolved.m_Material;
        for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
        {
            binding.m_Textures[t] = component->m_Resource->m_Textures[t];
            binding.m_Textures[t].m_TextureSet = GetTextureSetByIndex(component, t);
            key[t + 2] = (uintptr_t)binding.m_Textures[t].m_TextureSet;
        }
        uint64_t hash = dmHashBuffer64(key, sizeof(key[0]) * (binding.m_Resolved.m_NumTextures + 2));
        uint32_t* head = frame->m_BindingMap.Get(hash);
        binding.m_Next = head ? *head : INVALID_FRAME_INDEX;
        for (uint32_t i = binding.m_Next; i != INVALID_FRAME_INDEX; i = frame->m_Bindings[i].m_Next)
        {
            const SpriteFrameBinding& candidate = frame->m_Bindings[i];
            if (candidate.m_Source != binding.m_Source || candidate.m_Resolved.m_Material != binding.m_Resolved.m_Material)
                continue;
            bool equal = true;
            for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
                equal &= candidate.m_Textures[t].m_TextureSet == binding.m_Textures[t].m_TextureSet;
            if (equal)
                return i;
        }
        uint32_t index = frame->m_Bindings.Size();
        if (!FramePush(frame, frame->m_Bindings, binding))
            return INVALID_FRAME_INDEX;
        FramePut(frame, frame->m_BindingMap, hash, index);
        // Producer-owned references pin resource dependencies through consumption.
        // Threaded mode additionally drains CPU and GPU work before in-place mutation.
        CaptureResourceSize(frame, factory, binding.m_Source);
        CaptureResourceSize(frame, factory, binding.m_Resolved.m_Material);
        for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
            CaptureResourceSize(frame, factory, binding.m_Textures[t].m_TextureSet);
        if (frame->m_Builder)
        {
            bool ok = dmRender::RetainRenderFrameResource(frame->m_Builder, binding.m_Source);
            ok &= dmRender::RetainRenderFrameResource(frame->m_Builder, binding.m_Resolved.m_Material);
            for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
                ok &= dmRender::RetainRenderFrameResource(frame->m_Builder, binding.m_Textures[t].m_TextureSet);
            frame->m_Overflow |= !ok;
        }
        else
        {
            dmResource::IncRef(factory, binding.m_Source);
            dmResource::IncRef(factory, binding.m_Resolved.m_Material);
            for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
                dmResource::IncRef(factory, binding.m_Textures[t].m_TextureSet);
        }
        return index;
    }

    static uint32_t CaptureConstants(SpriteRenderFrame* frame, HComponentRenderConstants constants)
    {
        if (!constants || !GetRenderConstantCount(constants))
            return INVALID_FRAME_INDEX;
        uint32_t count = GetRenderConstantCount(constants);
        HashState64 state;
        dmHashInit64(&state, false);
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::HConstant source = GetRenderConstant(constants, i);
            dmhash_t name = dmRender::GetConstantName(source);
            dmRenderDDF::MaterialDesc::ConstantType type = dmRender::GetConstantType(source);
            uint32_t value_count;
            const Vector4* values = dmRender::GetConstantValues(source, &value_count);
            dmHashUpdateBuffer64(&state, &name, sizeof(name));
            dmHashUpdateBuffer64(&state, &type, sizeof(type));
            dmHashUpdateBuffer64(&state, &value_count, sizeof(value_count));
            dmHashUpdateBuffer64(&state, values, value_count * sizeof(Vector4));
        }
        uint64_t key = dmHashFinal64(&state);
        uint32_t* head = frame->m_ConstantMap.Get(key);
        uint32_t next = head ? *head : INVALID_FRAME_INDEX;
        for (uint32_t index = next; index != INVALID_FRAME_INDEX; index = frame->m_ConstantNext[index])
        {
            const SpriteFrameBlock& candidate = frame->m_Constants[index];
            if (candidate.m_Count != count)
                continue;
            bool equal = true;
            for (uint32_t i = 0; i < count && equal; ++i)
            {
                const SpriteFrameConstant& constant = frame->m_ConstantDescriptors[candidate.m_Begin + i];
                dmRender::HConstant source = GetRenderConstant(constants, i);
                uint32_t value_count;
                const Vector4* values = dmRender::GetConstantValues(source, &value_count);
                equal = constant.m_Name == dmRender::GetConstantName(source) && constant.m_Type == dmRender::GetConstantType(source) &&
                    constant.m_Values.m_Count == value_count && (value_count == 0 || memcmp(&frame->m_ConstantValues[constant.m_Values.m_Begin], values, value_count * sizeof(Vector4)) == 0);
            }
            if (equal)
                return index;
        }
        SpriteFrameBlock block = {frame->m_ConstantDescriptors.Size(), count};
        for (uint32_t i = 0; i < count; ++i)
        {
            dmRender::HConstant source = GetRenderConstant(constants, i);
            SpriteFrameConstant constant;
            constant.m_Name = dmRender::GetConstantName(source);
            constant.m_Type = dmRender::GetConstantType(source);
            constant.m_Values.m_Begin = frame->m_ConstantValues.Size();
            Vector4* values = dmRender::GetConstantValues(source, &constant.m_Values.m_Count);
            for (uint32_t v = 0; v < constant.m_Values.m_Count; ++v)
            {
                if (!FramePush(frame, frame->m_ConstantValues, values[v]))
                    return INVALID_FRAME_INDEX;
            }
            FramePush(frame, frame->m_ConstantDescriptors, constant);
        }
        uint32_t index = frame->m_Constants.Size();
        FramePush(frame, frame->m_Constants, block);
        FramePush(frame, frame->m_ConstantNext, next);
        FramePut(frame, frame->m_ConstantMap, key, index);
        return index;
    }

    static void CaptureSpriteFrame(SpriteWorld* world, dmResource::HFactory factory, SpriteRenderFrame* frame)
    {
        DM_PROFILE("SpriteCapture");
        ReleaseSpriteFrame(frame, factory);
        frame->m_Overflow = false;
        frame->m_VertexCount = world->m_VertexCount;
        frame->m_IndexCount = world->m_IndexCount;
        frame->m_VertexMemorySize = world->m_VertexMemorySize;
        const dmArray<SpriteComponent>& components = world->m_Components.GetRawObjects();
        uint32_t previous_binding = INVALID_FRAME_INDEX;
        for (uint32_t i = 0; i < components.Size(); ++i)
        {
            const SpriteComponent* component = &components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate || !component->m_VertexCount || !component->m_IndexCount)
                continue;
            SpriteRenderData data;
            data.m_World = component->m_World;
            data.m_Slice9 = component->m_Slice9;
            data.m_Size[0] = component->m_Size.getX();
            data.m_Size[1] = component->m_Size.getY();
            data.m_Pivot[0] = component->m_PivotX;
            data.m_Pivot[1] = component->m_PivotY;
            data.m_Flags = component->m_FlipHorizontal | (component->m_FlipVertical << 1) | (component->m_UseSlice9 << 2);
            data.m_VertexCount = component->m_VertexCount;
            data.m_BatchKey = component->m_MixedHash;
            data.m_TagListKey = dmRender::GetMaterialTagListKey(GetComponentMaterial(component));
            data.m_Binding = CaptureBinding(frame, component, factory, previous_binding);
            if (frame->m_Overflow)
                break;
            previous_binding = data.m_Binding;
            uint32_t* geometry = frame->m_GeometryMap.Get(component->m_AnimationDataHash);
            if (geometry)
                data.m_Geometry = *geometry;
            else
            {
                // Resource messages may replace atlas DDF after component update.
                // Its live animation cache can still point at the freed generation.
                // Resolve each unique geometry from the currently retained binding.
                AnimationData copy = {};
                ResolveAnimationData(component, &copy);
                data.m_Geometry = frame->m_Geometry.Size();
                FramePush(frame, frame->m_Geometry, copy);
                FramePut(frame, frame->m_GeometryMap, component->m_AnimationDataHash, data.m_Geometry);
            }
            data.m_Constants = CaptureConstants(frame, component->m_RenderConstants);
            data.m_Attributes = INVALID_FRAME_INDEX;
            if (component->m_DynamicVertexAttributeIndex != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
            {
                const DynamicAttributeInfo& attributes = world->m_DynamicVertexAttributePool.Get(component->m_DynamicVertexAttributeIndex);
                SpriteFrameBlock block = {frame->m_AttributeValues.Size(), attributes.m_NumInfos};
                for (uint32_t a = 0; a < attributes.m_NumInfos; ++a)
                    FramePush(frame, frame->m_AttributeValues, attributes.m_Infos[a]);
                data.m_Attributes = frame->m_Attributes.Size();
                FramePush(frame, frame->m_Attributes, block);
            }
            FramePush(frame, frame->m_Sprites, data);
            FramePush(frame, frame->m_Bounds, world->m_CullingInfo[i]);
            if (frame->m_Overflow)
                break;
        }
        for (uint32_t i = 0; i < frame->m_Bindings.Size(); ++i)
            frame->m_Bindings[i].m_Resolved.m_Textures = frame->m_Bindings[i].m_Textures;
    }

    // A stack-only adapter keeps the existing quad/slice9/trimmed geometry math in
    // one place. It contains only captured render fields, never live game pointers.
    static const SpriteComponent* GetRenderSprite(SpriteRendererState* renderer, uint32_t index, SpriteComponent* view)
    {
        if (!renderer->m_Frame)
            return &renderer->m_LegacyWorld->m_Components.GetRawObjects()[index];
        const SpriteRenderData& data = renderer->m_Frame->m_Sprites[index];
        memset(view, 0, sizeof(*view));
        view->m_World = data.m_World;
        view->m_Size = Vector3(data.m_Size[0], data.m_Size[1], 0);
        view->m_Slice9 = data.m_Slice9;
        view->m_PivotX = data.m_Pivot[0];
        view->m_PivotY = data.m_Pivot[1];
        view->m_FlipHorizontal = data.m_Flags & 1;
        view->m_FlipVertical = (data.m_Flags >> 1) & 1;
        view->m_UseSlice9 = (data.m_Flags >> 2) & 1;
        view->m_VertexCount = data.m_VertexCount;
        view->m_Resource = &renderer->m_Frame->m_Bindings[data.m_Binding].m_Resolved;
        view->m_NumTextures = view->m_Resource->m_NumTextures;
        view->m_Enabled = 1;
        view->m_DynamicVertexAttributeIndex = INVALID_DYNAMIC_ATTRIBUTE_INDEX;
        return view;
    }

    static void PrepareSpriteConstants(SpriteRendererState* renderer)
    {
        const SpriteRenderFrame* frame = renderer->m_Frame;
        if (!frame)
            return;
        for (uint32_t i = 0; i < frame->m_Constants.Size(); ++i)
        {
            if (i == renderer->m_ConstantBuffers.Size())
                FramePush(renderer->m_ConstantBuffers, dmRender::NewNamedConstantBuffer());
            dmRender::HNamedConstantBuffer buffer = renderer->m_ConstantBuffers[i];
            dmRender::ClearNamedConstantBuffer(buffer);
            const SpriteFrameBlock& block = frame->m_Constants[i];
            for (uint32_t c = 0; c < block.m_Count; ++c)
            {
                const SpriteFrameConstant& constant = frame->m_ConstantDescriptors[block.m_Begin + c];
                dmRender::SetNamedConstant(buffer, constant.m_Name,
                    constant.m_Values.m_Count ? (Vector4*)&frame->m_ConstantValues[constant.m_Values.m_Begin] : 0, constant.m_Values.m_Count, constant.m_Type);
            }
        }
    }

    static void CreateVertexData(SpriteRendererState* sprite_world, dmGraphics::VertexAttributeInfos* material_attribute_info,
        uint8_t** vb_where, uint8_t** ib_where, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("CreateVertexData");

        uint8_t* vertices        = *vb_where;
        uint8_t* indices         = *ib_where;
        uint32_t index_type_size = sprite_world->m_Is16BitIndex ? sizeof(uint16_t) : sizeof(uint32_t);


        // The offset for the indices
        uint32_t vertex_offset = sprite_world->m_VerticesWritten;
        uint32_t vertex_stride = material_attribute_info->m_VertexStride;

        // The list of pointers to the scratch uvs and page indices
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT] = {};
        const float* scratch_pi_ptrs[MAX_TEXTURE_COUNT] = {};
        const float* scratch_tt_ptrs[MAX_TEXTURE_COUNT] = {};

        if (sprite_world->m_AttributeScratch.Capacity() < material_attribute_info->m_NumInfos)
            sprite_world->m_AttributeScratch.SetCapacity(material_attribute_info->m_NumInfos);
        dmGraphics::VertexAttributeInfos scratch;
        scratch.m_Infos = sprite_world->m_AttributeScratch.Begin();
        dmGraphics::VertexAttributeInfos* scratch_attribute_infos = &scratch;
        dmGraphics::WriteAttributeParams write_params = {};

        for (uint32_t* i = begin; i != end; ++i)
        {
            uint32_t component_index         = (uint32_t)buf[*i].m_UserData;
            SpriteComponent view;
            const SpriteComponent* component = GetRenderSprite(sprite_world, component_index, &view);
            const Matrix4& world_matrix      = component->m_World;

            float sp_width  = component->m_Size.getX();
            float sp_height = component->m_Size.getY();
            uint8_t textures_num = component->m_NumTextures;
            const SpriteRenderFrame* frame = sprite_world->m_Frame;
            const SpriteRenderData* data = frame ? &frame->m_Sprites[component_index] : 0;
            const AnimationData* animations = frame ? &frame->m_Geometry[data->m_Geometry] : GetOrCreateAnimationData(sprite_world->m_LegacyWorld, component);

            // Fill in the custom sprite attributes (if specified), otherwise fallback to use the material attributes
            if (frame)
            {
                DynamicAttributeInfo attributes = {};
                if (data->m_Attributes != INVALID_FRAME_INDEX)
                {
                    const SpriteFrameBlock& block = frame->m_Attributes[data->m_Attributes];
                    attributes.m_Infos = (DynamicAttributeInfo::Info*)&frame->m_AttributeValues[block.m_Begin];
                    attributes.m_NumInfos = block.m_Count;
                }
                FillAttributeInfos(&attributes, component->m_Resource->m_DDF->m_Attributes.m_Data,
                    component->m_Resource->m_DDF->m_Attributes.m_Count, material_attribute_info,
                    scratch_attribute_infos, dmGraphics::COORDINATE_SPACE_WORLD);
            }
            else if (component->m_Resource->m_DDF->m_Attributes.m_Count > 0 || component->m_DynamicVertexAttributeIndex != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
            {
                FillAttributeInfos(&sprite_world->m_LegacyWorld->m_DynamicVertexAttributePool,
                    component->m_DynamicVertexAttributeIndex,
                    component->m_Resource->m_DDF->m_Attributes.m_Data,
                    component->m_Resource->m_DDF->m_Attributes.m_Count,
                    material_attribute_info,
                    scratch_attribute_infos,
                    dmGraphics::COORDINATE_SPACE_WORLD);
            }
            else
            {
                CopyAttributeInfos(scratch_attribute_infos, material_attribute_info, dmGraphics::COORDINATE_SPACE_WORLD);
            }

            dmGraphics::VertexAttributeInfoMetadata attribute_infos_meta = dmGraphics::GetVertexAttributeInfosMetaData(*scratch_attribute_infos);
            bool has_local_position_attribute = attribute_infos_meta.m_HasAttributeLocalPosition;
            bool has_world_position_attribute = attribute_infos_meta.m_HasAttributeWorldPosition;

            // We need to pad the buffer if the vertex stride doesn't start at an even byte offset from the start
            const uint32_t vb_buffer_offset = vertices - sprite_world->m_VertexBufferData;
            vertex_offset = vb_buffer_offset / vertex_stride;

            if (vb_buffer_offset % vertex_stride != 0)
            {
                vertices      += vertex_stride - vb_buffer_offset % vertex_stride;
                vertex_offset += 1;
            }

            // if textures_num == 0, then we don't have a texture set to get any vertex/uv coordinates from
            if (textures_num != 0 && !animations->m_CanUseQuads)
            {
                const dmGameSystemDDF::TextureSetAnimation* animation_ddf = animations->m_Animations[0];

                int flipx = animation_ddf->m_FlipHorizontal ^ component->m_FlipHorizontal;
                int flipy = animation_ddf->m_FlipVertical ^ component->m_FlipVertical;
                float scaleX = flipx ? -1 : 1;
                float scaleY = flipy ? -1 : 1;

                // Depending on the sprite is flipped or not, we loop the vertices forward or backward
                // to respect face winding (and backface culling)
                int reverse = flipx ^ flipy;

                ResolvePositionAndUVDataFromGeometry(component, animations, sprite_world->m_ScratchPositionWorld, sprite_world->m_ScratchUVs, scratch_uv_ptrs, scratch_pi_ptrs, scratch_tt_ptrs, scaleX, scaleY, reverse);

                if (has_local_position_attribute)
                {
                    sprite_world->m_ScratchPositionLocal.EnsureSize(sprite_world->m_ScratchPositionWorld.Size());
                }

                const float* world_matrix_channel[]    = { (float*) &world_matrix };
                const float* world_position_channels[] = { (float*) sprite_world->m_ScratchPositionWorld.Begin() };
                const float* local_position_channels[] = { (float*) sprite_world->m_ScratchPositionLocal.Begin() };

                FillWriteVertexAttributeParams(&write_params, scratch_attribute_infos,
                    world_matrix_channel,
                    world_position_channels,
                    local_position_channels,
                    (const float**) scratch_uv_ptrs,
                    textures_num,
                    (const float**) scratch_pi_ptrs,
                    textures_num,
                    (const float**) scratch_tt_ptrs,
                    textures_num);

                uint32_t num_vertices = sprite_world->m_ScratchPositionWorld.Size();
                for (uint32_t vertex_index = 0; vertex_index < num_vertices; ++vertex_index)
                {
                    if (has_local_position_attribute || has_world_position_attribute)
                    {
                        // Local space has size applied; world = mtx_world * local (mtx_world has no scale)
                        Vector4 local_pos(
                            sprite_world->m_ScratchPositionWorld[vertex_index].getX() * sp_width,
                            sprite_world->m_ScratchPositionWorld[vertex_index].getY() * sp_height,
                            0.0f, 1.0f);
                        if (has_local_position_attribute)
                        {
                            sprite_world->m_ScratchPositionLocal[vertex_index] = local_pos;
                        }
                        if (has_world_position_attribute)
                        {
                            sprite_world->m_ScratchPositionWorld[vertex_index] = world_matrix * local_pos;
                        }
                    }
                    vertices = dmGraphics::WriteAttributes(vertices, vertex_index, 1, write_params);
                }

                const dmGameSystemDDF::SpriteGeometry* geometry = animations->m_Geometries[0];
                uint32_t index_count = geometry->m_Indices.m_Count;
                uint32_t* geom_indices = geometry->m_Indices.m_Data;
                if (sprite_world->m_Is16BitIndex)
                {
                    for (uint32_t index = 0; index < index_count; ++index)
                    {
                        ((uint16_t*)indices)[index] = vertex_offset + geom_indices[index];
                    }
                }
                else
                {
                    for (uint32_t index = 0; index < index_count; ++index)
                    {
                        ((uint32_t*)indices)[index] = vertex_offset + geom_indices[index];
                    }
                }

                indices       += index_type_size * geometry->m_Indices.m_Count;
                vertex_offset += num_vertices;
            }
            else
            {
                // Output vertices in either a single quad format or slice-9 format
                // ****************************************************************************
                // Note regarding how we decide how the vertices should be generated:
                //      Currently in the code below, we only support generating slice-9
                //      quads when any components of the slice-9 property are set
                //      and the size mode is set to manual. The reason why we only allow
                //      slice-9 together with SIZE_MODE_MANUAL is more from a performance standpoint
                //      than a functionality standpoint. The slice-9 limits are specified in pixel
                //      coordinates and not in UV coordinates (or any other coordinate space),
                //      so a sprite with the same size as the source texture will yield a
                //      1:1 mapping between any area of the slice-9 quads and the texture.
                //      Meaning, the result of using slice-9 will look exactly the same
                //      as outputting a single quad since the density of the texture coordinates
                //      are the same everywhere across the surface, and we would just be
                //      submitting more vertices than needed.
                if (component->m_UseSlice9)
                {
                    CreateVertexDataSlice9(
                        component,
                        vertices,
                        indices,
                        sprite_world->m_Is16BitIndex,
                        has_world_position_attribute,
                        has_local_position_attribute,
                        world_matrix,
                        vertex_offset,
                        vertex_stride,
                        animations,
                        sprite_world->m_ScratchUVs,
                        scratch_uv_ptrs,
                        scratch_pi_ptrs,
                        scratch_tt_ptrs,
                        &sprite_world->m_ScratchPositionWorld,
                        &sprite_world->m_ScratchPositionLocal,
                        scratch_attribute_infos);

                    indices       += index_type_size * SPRITE_INDEX_COUNT_SLICE9;
                    vertices      += SPRITE_VERTEX_COUNT_SLICE9 * vertex_stride;
                    vertex_offset += SPRITE_VERTEX_COUNT_SLICE9;
                }
                else
                {
                    // We have two use cases:
                    // A) We know that no image is using sprite trimming
                    //    Thus we can use the corresponding quad for each image
                    // B) The first image is a quad, and any remapping
                    //    for any subsequent geometry would yield a quad anyways.
                    ResolveUVDataFromQuads(component, animations, sprite_world->m_ScratchUVs, scratch_uv_ptrs, scratch_pi_ptrs, scratch_tt_ptrs);

                    float x0 = -0.5f - component->m_PivotX;
                    float x1 =  0.5f - component->m_PivotX;
                    float y0 = -0.5f - component->m_PivotY;
                    float y1 =  0.5f - component->m_PivotY;

                    Vector4 positions_local[4];
                    Vector4 positions_world[4];

                    if (has_local_position_attribute || has_world_position_attribute)
                    {
                        positions_local[0] = Vector4(x0 * sp_width, y0 * sp_height, 0.0f, 1.0f);
                        positions_local[1] = Vector4(x0 * sp_width, y1 * sp_height, 0.0f, 1.0f);
                        positions_local[2] = Vector4(x1 * sp_width, y1 * sp_height, 0.0f, 1.0f);
                        positions_local[3] = Vector4(x1 * sp_width, y0 * sp_height, 0.0f, 1.0f);

                        if (has_world_position_attribute)
                        {
                            positions_world[0] = world_matrix * positions_local[0];
                            positions_world[1] = world_matrix * positions_local[1];
                            positions_world[2] = world_matrix * positions_local[2];
                            positions_world[3] = world_matrix * positions_local[3];
                        }
                    }

                    const float* world_matrix_channel[]    = { (float*) &world_matrix };
                    const float* local_position_channels[] = { (float*) &positions_local };
                    const float* world_position_channels[] = { (float*) &positions_world };

                    const uint8_t uv_channels_count = textures_num != 0 ? textures_num : 1;

                    FillWriteVertexAttributeParams(&write_params,
                        scratch_attribute_infos,
                        world_matrix_channel,
                        world_position_channels,
                        local_position_channels,
                        (const float**) scratch_uv_ptrs,
                        uv_channels_count,
                        (const float**) scratch_pi_ptrs,
                        uv_channels_count,
                        (const float**) scratch_tt_ptrs,
                        uv_channels_count);

                    vertices = dmGraphics::WriteAttributes(vertices, 0, 4, write_params);

                #if 0
                    for (int f = 0; f < 4; ++f)
                        printf("  %u: %.2f, %.2f\t%.2f, %.2f\n", f, vertices[f].x, vertices[f].y, vertices[f].u, vertices[f].v );
                #endif

                    // CCW winding order (OpenGL front-face default)
                    // Vertices: [0]=BL, [1]=TL, [2]=TR, [3]=BR
                    if (sprite_world->m_Is16BitIndex)
                    {
                        uint16_t* indices_16 = (uint16_t*) indices;
                        indices_16[0] = vertex_offset + 0;
                        indices_16[1] = vertex_offset + 3;
                        indices_16[2] = vertex_offset + 2;
                        indices_16[3] = vertex_offset + 0;
                        indices_16[4] = vertex_offset + 2;
                        indices_16[5] = vertex_offset + 1;
                    }
                    else
                    {
                        uint32_t* indices_32 = (uint32_t*) indices;
                        indices_32[0] = vertex_offset + 0;
                        indices_32[1] = vertex_offset + 3;
                        indices_32[2] = vertex_offset + 2;
                        indices_32[3] = vertex_offset + 0;
                        indices_32[4] = vertex_offset + 2;
                        indices_32[5] = vertex_offset + 1;
                    }
                    vertex_offset += SPRITE_VERTEX_COUNT_LEGACY;
                    indices       += SPRITE_INDEX_COUNT_LEGACY * index_type_size;
                }
            }
        }

        sprite_world->m_VerticesWritten = vertex_offset;

        *vb_where = vertices;
        *ib_where = indices;
    }

    static void EnsureVertexBufferCapacity(SpriteRendererState* sprite_world, uint32_t vertex_stride, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        uint32_t write_offset = sprite_world->m_VertexBufferWritePtr - sprite_world->m_VertexBufferData;
        uint32_t required_size = write_offset;

        for (uint32_t* i = begin; i != end; ++i)
        {
            uint32_t component_index = (uint32_t) buf[*i].m_UserData;
            uint32_t vertex_count = sprite_world->m_Frame ? sprite_world->m_Frame->m_Sprites[component_index].m_VertexCount :
                sprite_world->m_LegacyWorld->m_Components.GetRawObjects()[component_index].m_VertexCount;
            uint32_t remainder = required_size % vertex_stride;
            if (remainder != 0)
            {
                required_size += vertex_stride - remainder;
            }
            required_size += vertex_count * vertex_stride;
        }

        if (required_size <= sprite_world->m_VertexMemorySize)
        {
            return;
        }

        sprite_world->m_VertexBufferData = (uint8_t*) realloc(sprite_world->m_VertexBufferData, required_size);
        sprite_world->m_VertexBufferWritePtr = sprite_world->m_VertexBufferData + write_offset;
        sprite_world->m_VertexMemorySize = required_size;
    }

    static void RenderBatch(SpriteRendererState* sprite_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("SpriteRenderBatch");

        uint32_t component_index = (uint32_t)buf[*begin].m_UserData;
        SpriteComponent view;
        const SpriteComponent* first = GetRenderSprite(sprite_world, component_index, &view);
        assert(first->m_Enabled);

        SpriteResource* resource = first->m_Resource;

        // Although we generally like to preallocate it, we cannot since we
        // 1) don't want to preallocate max_sprite number of render objects and
        // 2) We cannot keep the render object in a (small) fixed array and then reallocate it, since we pass the pointer to the render engine
        if (sprite_world->m_RenderObjectsInUse == sprite_world->m_RenderObjects.Capacity())
        {
            sprite_world->m_RenderObjects.OffsetCapacity(1);
            dmRender::RenderObject* ro = new dmRender::RenderObject;
            sprite_world->m_RenderObjects.Push(ro);
        }

        dmRender::RenderObject& ro = *sprite_world->m_RenderObjects[sprite_world->m_RenderObjectsInUse++];
        dmRender::HMaterial material           = GetRenderMaterial(render_context, first);
        dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material);

        dmGraphics::VertexAttributeInfos material_attribute_info;
        // Same default coordinate space as the editor
        FillMaterialAttributeInfos(material, vx_decl, &material_attribute_info);

        // The context material can have a larger vertex format than the component material
        // used during update. Grow the CPU staging buffer before writing this batch.
        EnsureVertexBufferCapacity(sprite_world, material_attribute_info.m_VertexStride, buf, begin, end);

        // Fill in vertex buffer
        uint8_t* vb_begin = sprite_world->m_VertexBufferWritePtr;
        uint8_t* ib_begin = (uint8_t*)sprite_world->m_IndexBufferWritePtr;
        uint8_t* vb_iter  = vb_begin;
        uint8_t* ib_iter  = ib_begin;

        CreateVertexData(sprite_world, &material_attribute_info, &vb_iter, &ib_iter, buf, begin, end);

        sprite_world->m_VertexBufferWritePtr = vb_iter;
        sprite_world->m_IndexBufferWritePtr = ib_iter;

        if (dmRender::GetBufferIndex(render_context, sprite_world->m_VertexBuffer) < sprite_world->m_DispatchCount)
        {
            dmRender::AddRenderBuffer(render_context, sprite_world->m_VertexBuffer);
        }
        if (dmRender::GetBufferIndex(render_context, sprite_world->m_IndexBuffer) < sprite_world->m_DispatchCount)
        {
            dmRender::AddRenderBuffer(render_context, sprite_world->m_IndexBuffer);
        }

        ro.Init();
        ro.m_VertexDeclaration = vx_decl;
        ro.m_VertexBuffer = (dmGraphics::HVertexBuffer) dmRender::GetBuffer(render_context, sprite_world->m_VertexBuffer);
        ro.m_IndexBuffer = (dmGraphics::HIndexBuffer) dmRender::GetBuffer(render_context, sprite_world->m_IndexBuffer);
        ro.m_Material = GetComponentMaterial(first);
        for(uint32_t i = 0; i < resource->m_NumTextures; ++i)
        {
            ro.m_Textures[i] = GetMaterialTexture(first, i);
        }

        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_IndexType = sprite_world->m_Is16BitIndex ? dmGraphics::TYPE_UNSIGNED_SHORT : dmGraphics::TYPE_UNSIGNED_INT;

        // offset in bytes into element buffer
        uint32_t index_offset = ib_begin - sprite_world->m_IndexBufferData;

        // num elements = Number of bytes / sizeof(index_type)
        uint32_t index_type_size = sprite_world->m_Is16BitIndex ? sizeof(uint16_t) : sizeof(uint32_t);
        uint32_t num_elements = ((uint8_t*)sprite_world->m_IndexBufferWritePtr - (uint8_t*)ib_begin) / index_type_size;

        // These should be named "element" or "index" (as opposed to vertex)
        ro.m_VertexStart = index_offset;
        ro.m_VertexCount = num_elements;

        HComponentRenderConstants constants = GetRenderConstants(first);
        if (sprite_world->m_Frame)
        {
            uint32_t constant_index = sprite_world->m_Frame->m_Sprites[component_index].m_Constants;
            if (constant_index != INVALID_FRAME_INDEX)
                ro.m_ConstantBuffer = sprite_world->m_ConstantBuffers[constant_index];
        }
        else if (constants) {
            dmGameSystem::EnableRenderObjectConstants(&ro, constants);
        }

        dmGameSystemDDF::SpriteDesc::BlendMode blend_mode = resource->m_DDF->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD:
            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_SCREEN:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }

        ro.m_SetBlendFactors = 1;

        dmRender::AddToRender(render_context, &ro);
    }

    static void UpdateTransform(SpriteComponent* component, bool sub_pixels)
    {
        dmTransform::Transform transform = dmTransform::Transform(component->m_Position, component->m_Rotation, component->m_Scale);
        Matrix4 local = dmTransform::ToMatrix4(transform);
        Matrix4 world = dmGameObject::GetWorldMatrix(component->m_Instance);
        Matrix4 w = world * local;
        if (!sub_pixels)
        {
            Vector4 position = w.getCol3();
            position.setX((int) position.getX());
            position.setY((int) position.getY());
            w.setCol3(position);
        }
        component->m_World = w;
    }

    static bool GetSender(SpriteComponent* component, dmMessage::URL* out_sender)
    {
        dmMessage::URL sender;
        sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
        if (dmMessage::IsSocketValid(sender.m_Socket))
        {
            dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
            if (go_result == dmGameObject::RESULT_OK)
            {
                sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
                *out_sender = sender;
                return true;
            }
        }
        return false;
    }

    static void PostMessages(SpriteComponent* component)
    {
        const dmGameSystemDDF::Playback playback = (dmGameSystemDDF::Playback)component->m_AnimationPlayback;
        bool once = playback == dmGameSystemDDF::PLAYBACK_ONCE_FORWARD
                || playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                || playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG;
        // Stop once-animation and broadcast animation_done
        if (once && component->m_AnimTimer >= 1.0f)
        {
            component->m_IsPlaying = 0;
            if (component->m_Listener.m_Fragment != 0x0)
            {
                dmMessage::URL sender;
                if (!GetSender(component, &sender))
                {
                    dmLogError("Could not send animation_done from component. Has it been deleted?");
                    return;
                }

                // Should this check be handled by the comp_script.cpp?
                dmGameObject::HInstance listener_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), component->m_Listener.m_Path);
                if (!listener_instance)
                {
                    dmLogError("Could not send animation_done to instance: %s:%s#%s", dmHashReverseSafe64(component->m_Listener.m_Socket), dmHashReverseSafe64(component->m_Listener.m_Path), dmHashReverseSafe64(component->m_Listener.m_Fragment));
                    return;
                }

                dmGameSystemDDF::AnimationDone message;
                message.m_CurrentTile = component->m_CurrentAnimationFrame + 1; // Engine has 0-based indices, scripts use 1-based
                message.m_Id = component->m_CurrentAnimation;

                // This is a 'done' callback, so we should tell the message system to remove the callback once it's been consumed
                dmGameObject::Result go_result = dmGameObject::PostDDF(&message, &sender, &component->m_Listener, component->m_FunctionRef, true);
                component->m_FunctionRef = 0;

                dmMessage::ResetURL(&component->m_Listener);
                if (go_result != dmGameObject::RESULT_OK)
                {
                    dmLogError("Could not send animation_done to listener. Has it been deleted?");
                }
            }
        }
    }


    static void Animate(SpriteComponent* component, float dt)
    {
        if (component->m_IsPlaying && component->m_AddedToUpdate)
        {
            // Animate
            component->m_AnimTimer += dt * component->m_AnimInvDuration * component->m_PlaybackRate;
            if (component->m_AnimTimer >= 1.0f)
            {
                switch (component->m_AnimationPlayback)
                {
                    case dmGameSystemDDF::PLAYBACK_ONCE_FORWARD:
                    case dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD:
                    case dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG:
                        component->m_AnimTimer = 1.0f;
                        break;
                    default:
                        component->m_AnimTimer -= floorf(component->m_AnimTimer);
                        break;
                }
            }
            component->m_DoTick = 1;
        }

        if (component->m_DoTick)
        {
            component->m_DoTick = 0;
            UpdateCurrentAnimationFrame(component);
        }
    }

    dmGameObject::CreateResult CompSpriteAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void UpdateAnimationDataCache(SpriteWorld* sprite_world)
    {
        DM_PROFILE("UpdateAnimationDataCache");
        // update current tick
        sprite_world->m_AnimationDataCache.m_CurrentEngineTick++;
        // evict all cache entries which older than CACHE_EVICTION_FRAMES ticks
        dmDoubleLinkedList::List* list = &sprite_world->m_AnimationDataCache.m_LRU;
        dmDoubleLinkedList::ListNode* current = dmDoubleLinkedList::ListGetLast(list);
        while (current != 0x0)
        {
            // list node is used as pointer to memory block where AnimationData structure is placed
            // cast to AnimationData allows to work with memory block as structure
            AnimationData* data = (AnimationData*)current;
            uint32_t cache_eviction_frame = data->m_LastAccessTick + CACHE_EVICTION_FRAMES;
            uint32_t underflow_tick = sprite_world->m_AnimationDataCache.m_CurrentEngineTick - CACHE_EVICTION_FRAMES;

            //in normal case we evict cache entry when CACHE_EVICTION_FRAMES passed
            // but also in case if m_LastAccessTick tick or m_CurrentEngineTick is near overflow
            if (cache_eviction_frame <= sprite_world->m_AnimationDataCache.m_CurrentEngineTick
                && (data->m_LastAccessTick < cache_eviction_frame || underflow_tick > sprite_world->m_AnimationDataCache.m_CurrentEngineTick))
            {
                sprite_world->m_AnimationDataCache.m_Cache.Erase(data->m_CacheKey);
                dmDoubleLinkedList::ListRemove(list, current);
                free(data);
            }
            else
            {
                break;
            }
            current = dmDoubleLinkedList::ListGetLast(list);
        }
    }

    dmGameObject::UpdateResult CompSpriteUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        /*
         * NOTES:
         * * When/if transparency the batching predicates must be updated in order to
         *   support per sprite correct sorting.
         */

        SpriteWorld* world = (SpriteWorld*)params.m_World;
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;

        UpdateAnimationDataCache(world);

        dmArray<SpriteComponent>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();

        if (n == 0)
        {
            return dmGameObject::UPDATE_RESULT_OK;
        }

        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate)
                continue;
            Animate(component, params.m_UpdateContext->m_DT);

            HComponentRenderConstants constants = GetRenderConstants(component);
            bool is_component_changed = component->m_ReHash || component->m_AnimationReHash;
            if (component->m_ReHash || (constants && dmGameSystem::AreRenderConstantsUpdated(constants)))
            {
                ReHash(component);
            }
            if (component->m_AnimationReHash)
            {
                AnimationReHash(component);
            }

            // Get the correct animation frames, and other meta data
            AnimationData* anim_data = GetOrCreateAnimationData(world, component);
            // update cached pivot
            if (is_component_changed || anim_data->m_CreatedTick == world->m_AnimationDataCache.m_CurrentEngineTick)
            {
                GetPivot(anim_data, &component->m_PivotX, &component->m_PivotY);
                UpdateVertexMetricsCache(component, anim_data, render_context);
            }

            // TODO: check when we need send messages
            if (!component->m_IsPlaying)
                continue;
            PostMessages(component);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteLateUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        SpriteWorld* world = (SpriteWorld*)params.m_World;
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmArray<SpriteComponent>& components = world->m_Components.GetRawObjects();
        uint32_t n = components.Size();

        if (n == 0)
        {
            return dmGameObject::UPDATE_RESULT_OK;
        }

        uint32_t num_vertices    = 0;
        uint32_t num_indices     = 0;
        uint32_t vertex_memsize  = 0;

        bool sub_pixels = sprite_context->m_Subpixels;

        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate)
                continue;
            if (component->m_VertexCount == 0 || component->m_IndexCount == 0)
                continue;
            UpdateTransform(component, sub_pixels);
            // Bounding radius: world matrix already contains component scale; incorporate only sprite size
            Vector3 size = component->m_Size;
            Vector3 half_diagonal = (component->m_World.getCol(0).getXYZ() * size.getX() + component->m_World.getCol(1).getXYZ() * size.getY()) * 0.5f;
            float radius_sq = dmVMath::LengthSqr(half_diagonal);

            Point3 pivot_scaled(-component->m_PivotX * size.getX(), -component->m_PivotY * size.getY(), 0.f);
            Vector3 world_pos = (component->m_World * pivot_scaled).getXYZ();

            world->m_CullingInfo[i].m_Position[0] = world_pos.getX();
            world->m_CullingInfo[i].m_Position[1] = world_pos.getY();
            world->m_CullingInfo[i].m_Position[2] = world_pos.getZ();
            world->m_CullingInfo[i].m_Radius = radius_sq;

            // We need to pad the buffer if the vertex stride doesn't start at an even byte offset from the start
            vertex_memsize += component->m_VertexStride - vertex_memsize % component->m_VertexStride;

            num_vertices += component->m_VertexCount;
            num_indices += component->m_IndexCount;
            vertex_memsize += component->m_VertexCount * component->m_VertexStride;
        }

        // In case if CompSpriteUpdate will be called several times before CompSpriteRender - assign flag with "bit or"
        // to save flag state from previous update until ReallocBuffers will be called
        world->m_ReallocBuffers   |= vertex_memsize > world->m_VertexMemorySize || num_indices > world->m_IndexCount;
        world->m_VertexCount      = num_vertices;
        world->m_IndexCount       = num_indices;
        world->m_VertexMemorySize = dmMath::Max(world->m_VertexMemorySize, vertex_memsize);

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("Sprite");

        SpriteRendererState* sprite_world = (SpriteRendererState*)params.m_UserData;
        const SpriteCullingInfo* infos = sprite_world->m_Frame ? sprite_world->m_Frame->m_Bounds.Begin() : sprite_world->m_LegacyWorld->m_CullingInfo.Begin();

        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];
            const SpriteCullingInfo& culling_info = infos[entry->m_UserData];
            Vector4 pos(culling_info.m_Position[0], culling_info.m_Position[1], culling_info.m_Position[2], 1.0f);
            bool intersect = dmIntersection::TestFrustumSphereSq(frustum, pos, culling_info.m_Radius);
            entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        SpriteRendererState* world = (SpriteRendererState*) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                world->m_VertexBufferWritePtr = world->m_VertexBufferData;
                world->m_IndexBufferWritePtr = world->m_IndexBufferData;
                world->m_RenderObjectsInUse = 0;
                world->m_VerticesWritten = 0;
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                {
                    uint32_t vertex_data_size = world->m_VertexBufferWritePtr - world->m_VertexBufferData;
                    uint32_t index_data_size  = world->m_IndexBufferWritePtr - world->m_IndexBufferData;

                    // JG: The renderer executes the dispatch function for begin/end regardless if something is actually batched or not
                    //     This behaviour can cause side-effects on certain platforms and non-opengl graphics adapters.
                    //     We might want to change how that process is setup, but for now this is a safer change.
                    if (vertex_data_size && index_data_size)
                    {
                        dmRender::SetBufferData(params.m_Context, world->m_VertexBuffer, vertex_data_size, world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
                        dmRender::SetBufferData(params.m_Context, world->m_IndexBuffer, index_data_size, world->m_IndexBufferData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

                        DM_PROPERTY_ADD_U32(rmtp_SpriteVertexCount, world->m_VertexCount);
                        DM_PROPERTY_ADD_U32(rmtp_SpriteVertexSize, vertex_data_size);
                        DM_PROPERTY_ADD_U32(rmtp_SpriteIndexSize, index_data_size);

                        world->m_DispatchCount++;
                    }
                }
                break;
            default:
                assert(params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH);
                RenderBatch(world, params.m_Context, params.m_Buf, params.m_Begin, params.m_End);
        }
    }

    static dmGameObject::UpdateResult SubmitSpriteFrame(SpriteRendererState* renderer, dmRender::HRenderContext render_context,
                                                       uint32_t vertex_count, uint32_t index_count, uint32_t vertex_memory_size,
                                                       const dmRender::RenderFrame* global_frame = 0)
    {
        uint32_t sprite_count = renderer->m_Frame ? renderer->m_Frame->m_Sprites.Size() : renderer->m_LegacyWorld->m_Components.GetRawObjects().Size();
        // Even an empty capture must release the previous frame's retained resources.
        if (!sprite_count)
            return dmGameObject::UPDATE_RESULT_OK;

        renderer->m_ReallocBuffers |= vertex_memory_size > renderer->m_VertexMemorySize || index_count > renderer->m_IndexCount;
        renderer->m_VertexMemorySize = dmMath::Max(renderer->m_VertexMemorySize, vertex_memory_size);
        renderer->m_VertexCount = vertex_count;
        renderer->m_IndexCount = index_count;
        if (renderer->m_ReallocBuffers)
            ReAllocateBuffers(renderer, render_context);
        dmRender::TrimBuffer(render_context, renderer->m_VertexBuffer);
        dmRender::RewindBuffer(render_context, renderer->m_VertexBuffer);
        dmRender::TrimBuffer(render_context, renderer->m_IndexBuffer);
        dmRender::RewindBuffer(render_context, renderer->m_IndexBuffer);
        renderer->m_DispatchCount = 0;
        renderer->m_VerticesWritten = 0;
        PrepareSpriteConstants(renderer);

        dmRender::RenderListEntry* entries = dmRender::RenderListAlloc(render_context, sprite_count);
        dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, &RenderListFrustumCulling, renderer);
        dmRender::RenderListEntry* write_ptr = entries;
        for (uint32_t i = 0; i < sprite_count; ++i)
        {
            if (renderer->m_Frame)
            {
                const SpriteRenderData& data = renderer->m_Frame->m_Sprites[i];
                write_ptr->m_WorldPosition = Point3(data.m_World.getCol3().getXYZ());
                write_ptr->m_BatchKey = data.m_BatchKey;
                write_ptr->m_TagListKey = data.m_TagListKey;
            }
            else
            {
                const SpriteComponent& component = renderer->m_LegacyWorld->m_Components.GetRawObjects()[i];
                if (!component.m_Enabled || !component.m_AddedToUpdate || !component.m_VertexCount || !component.m_IndexCount)
                    continue;
                write_ptr->m_WorldPosition = Point3(component.m_World.getCol3().getXYZ());
                write_ptr->m_BatchKey = component.m_MixedHash;
                write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetComponentMaterial(&component));
            }
            write_ptr->m_UserData = i;
            write_ptr->m_Dispatch = dispatch;
            write_ptr->m_MinorOrder = 0;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
            DM_PROPERTY_ADD_U32(rmtp_Sprite, 1);
        }
        if (global_frame)
        {
            write_ptr = entries;
            for (uint32_t i = 0; i < global_frame->m_Entries.Size(); ++i)
            {
                const dmRender::RenderFrameEntry& entry = global_frame->m_Entries[i];
                if (entry.m_Consumer != 0) continue;
                write_ptr->m_WorldPosition = Point3(entry.m_Bounds.getXYZ());
                write_ptr->m_BatchKey = entry.m_BatchKey;
                write_ptr->m_TagListKey = entry.m_TagListKey;
                write_ptr->m_UserData = *(const uint32_t*)dmRender::GetRenderFrameData(*global_frame, entry.m_Payload, sizeof(uint32_t));
                write_ptr->m_Dispatch = dispatch;
                write_ptr->m_MajorOrder = entry.m_MajorOrder;
                write_ptr->m_Order = entry.m_Order;
                write_ptr->m_MinorOrder = 0;
                ++write_ptr;
            }
        }
        dmRender::RenderListSubmit(render_context, entries, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteRender(const dmGameObject::ComponentsRenderParams& params)
    {
        SpriteContext* context = (SpriteContext*)params.m_Context;
        SpriteWorld* world = (SpriteWorld*)params.m_World;
        SpriteRendererState* renderer = &world->m_Renderer;
        dmRender::HRenderContext render_context = context->m_RenderContext;
        if (context->m_SnapshotInline)
        {
            if (!renderer->m_Frame)
                renderer->m_Frame = new SpriteRenderFrame();
            renderer->m_LegacyWorld = 0;
            uint64_t start = dmTime::GetMonotonicTime();
            CaptureSpriteFrame(world, context->m_Factory, renderer->m_Frame);
            renderer->m_CaptureTotalUs += dmTime::GetMonotonicTime() - start;
            ++renderer->m_CaptureCount;
        }
        else
        {
            // Runtime mode changes are test-only; startup configuration is fixed.
            if (renderer->m_Frame)
            {
                ReleaseSpriteFrame(renderer->m_Frame, context->m_Factory);
                delete renderer->m_Frame;
                renderer->m_Frame = 0;
            }
            renderer->m_LegacyWorld = world;
        }
        renderer->m_ReallocBuffers |= world->m_ReallocBuffers;
        world->m_ReallocBuffers = 0;
        return SubmitSpriteFrame(renderer, render_context, world->m_VertexCount, world->m_IndexCount, world->m_VertexMemorySize);
    }

    static bool CompSpriteGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;

        HComponentRenderConstants constants = GetRenderConstants(component);
        if (!constants)
            return false;
        return GetRenderConstant(constants, name_hash, out_constant);
    }

    static void CompSpriteSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;
        if (!component->m_RenderConstants)
        {
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        }
        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetComponentMaterial(component), name_hash, value_index, element_index, var);
        component->m_ReHash = 1;
    }

    static bool CompSpriteGetMaterialAttributeCallback(void* user_data, dmhash_t name_hash, const dmGraphics::VertexAttribute** attribute)
    {
        SpriteComponent* component                                    = (SpriteComponent*) user_data;
        const dmGraphics::VertexAttribute* sprite_resource_attributes = component->m_Resource->m_DDF->m_Attributes.m_Data;
        const uint32_t sprite_resource_attribute_count                = component->m_Resource->m_DDF->m_Attributes.m_Count;

        int sprite_attribute_index = FindAttributeIndex(sprite_resource_attributes, sprite_resource_attribute_count, name_hash);
        if (sprite_attribute_index >= 0)
        {
            *attribute = &sprite_resource_attributes[sprite_attribute_index];
            return true;
        }
        return false;
    }

    static void SetCursor(SpriteComponent* component, float cursor)
    {
        const dmGameSystemDDF::Playback playback = (dmGameSystemDDF::Playback)component->m_AnimationPlayback;
        cursor = dmMath::Clamp(cursor, 0.0f, 1.0f);
        if (playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG 
            || playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
        {
            cursor /= 2.0f;
        }
        else if (playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD 
            || playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD)
        {
            cursor = 1.0f - cursor;
        }

        component->m_AnimTimer = cursor;
        component->m_DoTick = 1;
    }

    static float GetCursor(SpriteComponent* component)
    {
        float cursor = component->m_AnimTimer;
        const dmGameSystemDDF::Playback playback = (dmGameSystemDDF::Playback)component->m_AnimationPlayback;

        if (playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD 
            || playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD)
        {
            cursor = 1.0f - cursor;
        }
        else if (playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG 
            || playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
        {
            cursor *= 2.0f;
            if (cursor > 1.0f)
            {
                cursor = 2.0f - cursor;
            }
        }
        return cursor;
    }

    static void SetPlaybackRate(SpriteComponent* component, float playback_rate)
    {
        component->m_PlaybackRate = playback_rate;
    }

    static float GetPlaybackRate(SpriteComponent* component)
    {
        return component->m_PlaybackRate;
    }

    static inline float GetAnimationFrameCount(SpriteComponent* component)
    {
        return component->m_AnimationFrameCount;
    }

    dmGameObject::UpdateResult CompSpriteOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
        const dmhash_t message_id = params.m_Message->m_Id;
        if (message_id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 1;
        }
        else if (message_id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (message_id == dmGameSystemDDF::PlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::PlayAnimation* ddf = (dmGameSystemDDF::PlayAnimation*)params.m_Message->m_Data;
                if (PlayAnimation(component, ddf->m_Id, ddf->m_Offset, ddf->m_PlaybackRate))
                {
                    // Remove the currently assigned callback by sending an unref message
                    if (component->m_FunctionRef)
                    {
                        dmMessage::URL sender = {};
                        GetSender(component, &sender);
                        dmGameObject::PostScriptUnrefMessage(&sender, &component->m_Listener, component->m_FunctionRef);
                    }
                    component->m_Listener = params.m_Message->m_Sender;
                    component->m_FunctionRef = params.m_Message->m_UserData2;
                }
            }
            else if (message_id == dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetFlipHorizontal* ddf = (dmGameSystemDDF::SetFlipHorizontal*)params.m_Message->m_Data;
                component->m_FlipHorizontal = ddf->m_Flip != 0 ? 1 : 0;
            }
            else if (message_id == dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetFlipVertical* ddf = (dmGameSystemDDF::SetFlipVertical*)params.m_Message->m_Data;
                component->m_FlipVertical = ddf->m_Flip != 0 ? 1 : 0;
            }
            else if (message_id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                if (component->m_RenderConstants && dmGameSystem::ClearRenderConstant(component->m_RenderConstants, ddf->m_NameHash))
                {
                    component->m_ReHash = 1;
                }
            }
            else if (message_id == dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetScale* ddf = (dmGameSystemDDF::SetScale*)params.m_Message->m_Data;
                component->m_Scale = ddf->m_Scale;
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompSpriteOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
        if (component->m_IsPlaying)
        {
            PlayAnimation(component, component->m_CurrentAnimation, component->m_AnimTimer, component->m_PlaybackRate);
        }
    }

    dmGameObject::PropertyResult CompSpriteGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
        dmhash_t get_property = params.m_PropertyId;

        if (IsReferencingProperty(SPRITE_PROP_SCALE, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Scale, SPRITE_PROP_SCALE);
        }
        else if (IsReferencingProperty(SPRITE_PROP_SIZE, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Size, SPRITE_PROP_SIZE);
        }
        else if (IsReferencingProperty(SPRITE_PROP_SLICE, get_property))
        {
            return GetProperty(out_value, get_property, component->m_Slice9, SPRITE_PROP_SLICE);
        }
        else if (get_property == SPRITE_PROP_CURSOR)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(GetCursor(component));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (get_property == SPRITE_PROP_PLAYBACK_RATE)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(GetPlaybackRate(component));
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (get_property == PROP_MATERIAL)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterialResource(component), out_value);
        }
        else if (get_property == PROP_IMAGE)
        {
            TextureSetResource* texture_set = 0;

            dmhash_t key = 0;
            if (GetPropertyOptionsKey(params.m_Options, 0, &key) == dmGameObject::PROPERTY_RESULT_OK)
            {
                out_value.m_ValueType = dmGameObject::PROP_VALUE_HASHTABLE;
                texture_set = GetTextureSetByHash(component, key);
            }
            if (!texture_set)
            {
                texture_set = GetFirstTextureSet(component);
            }
            if (!texture_set)
            {
                return dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND;
            }
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), texture_set, out_value);
        }
        else if (get_property == PROP_TEXTURE[0])
        {
            TextureSetResource* texture_set = GetFirstTextureSet(component);
            if (!texture_set)
                return dmGameObject::PROPERTY_RESULT_RESOURCE_NOT_FOUND;
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), texture_set->m_Texture, out_value);
        }
        else if (get_property == PROP_ANIMATION)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_CurrentAnimation);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (get_property == SPRITE_PROP_FRAME_COUNT)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(GetAnimationFrameCount(component));
            return dmGameObject::PROPERTY_RESULT_OK;
        }

        int32_t value_index = 0;
        GetPropertyOptionsIndex(params.m_Options, 0, &value_index);
        dmRender::HMaterial material = GetComponentMaterial(component);
        if (GetMaterialConstant(material, get_property, value_index, out_value, false, CompSpriteGetConstantCallback, component) == dmGameObject::PROPERTY_RESULT_OK)
        {
            return dmGameObject::PROPERTY_RESULT_OK;
        }

        return GetMaterialAttribute(sprite_world->m_DynamicVertexAttributePool, component->m_DynamicVertexAttributeIndex, material, get_property, out_value, CompSpriteGetMaterialAttributeCallback, component);
    }

    dmGameObject::PropertyResult CompSpriteSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
        dmhash_t set_property = params.m_PropertyId;

        if (IsReferencingProperty(SPRITE_PROP_SCALE, set_property))
        {
            return SetProperty(set_property, params.m_Value, component->m_Scale, SPRITE_PROP_SCALE);
        }
        else if (IsReferencingProperty(SPRITE_PROP_SIZE, set_property))
        {
            if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_AUTO)
            {
                return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION;
            }
            return SetProperty(set_property, params.m_Value, component->m_Size, SPRITE_PROP_SIZE);
        }
        else if (IsReferencingProperty(SPRITE_PROP_SLICE, set_property))
        {
            if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_AUTO)
            {
                return dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION;
            }

            dmGameObject::PropertyResult result = SetProperty(set_property, params.m_Value, component->m_Slice9, SPRITE_PROP_SLICE);
            if (dmGameObject::PROPERTY_RESULT_OK == result)
            {
                component->m_UseSlice9 = sum(component->m_Slice9) != 0;
            }
            return result;
        }
        else if (params.m_PropertyId == SPRITE_PROP_CURSOR)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            SetCursor(component, params.m_Value.m_Number);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (params.m_PropertyId == SPRITE_PROP_PLAYBACK_RATE)
        {
            if (params.m_Value.m_Type != dmGameObject::PROPERTY_TYPE_NUMBER)
                return dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;

            SetPlaybackRate(component, params.m_Value.m_Number);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (set_property == PROP_MATERIAL)
        {
            dmGameObject::PropertyResult res = AddOverrideMaterial(dmGameObject::GetFactory(params.m_Instance), component, params.m_Value.m_Hash);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        else if (set_property == PROP_IMAGE)
        {
            dmhash_t sampler_name_hash = 0;
            GetPropertyOptionsKey(params.m_Options, 0, &sampler_name_hash);

            dmGameObject::PropertyResult res = AddOverrideTextureSet(dmGameObject::GetFactory(params.m_Instance), component, sampler_name_hash, params.m_Value.m_Hash);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;

            // Since the animation referred to the old texture, we need to update it
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                TextureSetResource* texture_set = GetFirstTextureSet(component);
                dmhash_t current_animation = component->m_CurrentAnimation;
                if (GetCurrentOrFirstAnimation(texture_set, current_animation, &current_animation))
                {
                    PlayAnimation(component, current_animation, GetCursor(component), component->m_PlaybackRate);
                }
                else
                {
                    ClearCurrentAnimation(component);
                }
            }
            return res;
        }
        else if ((set_property == SPRITE_PROP_FRAME_COUNT) || (set_property == PROP_ANIMATION))
        {
            return dmGameObject::PROPERTY_RESULT_READ_ONLY;
        }

        int32_t value_index = 0;
        GetPropertyOptionsIndex(params.m_Options, 0, &value_index);

        dmRender::HMaterial material = GetComponentMaterial(component);
        dmGameObject::PropertyResult res = SetMaterialConstant(material, params.m_PropertyId, params.m_Value, value_index, CompSpriteSetConstantCallback, component);

        // Only check attributes if the constant property was not found
        if (res == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
        {
            return SetMaterialAttribute(sprite_world->m_DynamicVertexAttributePool, &component->m_DynamicVertexAttributeIndex, material, set_property, params.m_Value, CompSpriteGetMaterialAttributeCallback, component, 0);
        }
        return res;
    }

    static bool CompSpriteIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)pit->m_Node->m_ComponentWorld;
        SpriteComponent* component = &sprite_world->m_Components.Get(pit->m_Node->m_Component);

        uint64_t index = pit->m_Next++;

        const char* property_names[] = {
            "position",
            "rotation",
            "scale",
            "size",
        };

        uint32_t num_properties = DM_ARRAY_SIZE(property_names);
        if (index < 4)
        {
            int num_elements = 3;
            Vector4 value;
            switch(index)
            {
            case 0: value = Vector4(component->m_Position); break;
            case 1: value = Vector4(component->m_Rotation); num_elements = 4; break;
            case 2: value = Vector4(component->m_Scale); break;
            case 3: value = Vector4(component->m_Size); break;
            }

            pit->m_Property.m_NameHash = dmHashString64(property_names[index]);
            pit->m_Property.m_Type = num_elements == 3 ? dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3 : dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4;
            pit->m_Property.m_Value.m_V4[0] = value.getX();
            pit->m_Property.m_Value.m_V4[1] = value.getY();
            pit->m_Property.m_Value.m_V4[2] = value.getZ();
            pit->m_Property.m_Value.m_V4[3] = value.getW();

            return true;
        }

        index -= num_properties;

        const char* world_property_names[] = {
            "world_position",
            "world_rotation",
            "world_scale",
            "world_size",
        };

        uint32_t num_world_properties = DM_ARRAY_SIZE(world_property_names);
        if (index < num_world_properties)
        {
            dmTransform::Transform transform = dmTransform::ToTransform(component->m_World);

            dmGameObject::SceneNodePropertyType type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3;
            Vector4 value;
            switch(index)
            {
                case 0:
                    value = Vector4(transform.GetTranslation());
                    break;
                case 1:
                    value = Vector4(transform.GetRotation());
                    type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4;
                    break;
                case 2:
                {
                    Matrix4 parent_world = dmGameObject::GetWorldMatrix(component->m_Instance);
                    Vector3 parent_scale = dmTransform::ToTransform(parent_world).GetScale();
                    Vector3 world_scale = dmVMath::MulPerElem(parent_scale, component->m_Scale);
                    value = Vector4(world_scale);
                    break;
                }
                case 3:
                    // world_size: size is not in the matrix, return component size * scale
                    value = Vector4(dmVMath::MulPerElem(component->m_Size, component->m_Scale));
                    value.setZ(1.0f);
                    break;
                default: return false;
            }

            pit->m_Property.m_Type = type;
            pit->m_Property.m_NameHash = dmHashString64(world_property_names[index]);
            pit->m_Property.m_Value.m_V4[0] = value.getX();
            pit->m_Property.m_Value.m_V4[1] = value.getY();
            pit->m_Property.m_Value.m_V4[2] = value.getZ();
            pit->m_Property.m_Value.m_V4[3] = value.getW();
            return true;
        }
        index -= num_world_properties;

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

    void CompSpriteIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompSpriteIterPropertiesGetNext;
    }

    static void ResourceReloadedCallback(const dmResource::ResourceReloadedParams* params)
    {
        dmhash_t name_hash = ResourceTypeGetNameHash(params->m_Type);
        if (name_hash != TEXTURE_SET_EXT_HASH)
        {
            return;
        }
        SpriteWorld* sprite_world = (SpriteWorld*) params->m_UserData;
        dmArray<SpriteComponent>& components = sprite_world->m_Components.GetRawObjects();
        uint32_t component_count = components.Size();

        const void* resource = dmResource::GetResource(params->m_Resource);
        for (uint32_t i = 0; i < component_count; ++i)
        {
            SpriteComponent* component = &components[i];
            if (component->m_Resource != 0x0)
            {
                uint32_t num_textures = component->m_NumTextures;
                for (uint32_t texture_idx = 0; texture_idx < num_textures; ++texture_idx)
                {
                    uintptr_t texture = (uintptr_t)GetTextureSetByIndex(component, texture_idx);
                    if (texture == (uintptr_t) resource)
                    {
                        component->m_ReHash = 1;
                        break;
                    }
                }
            }
        }
    }

    template <typename T> static void CountFrameArray(const dmArray<T>& array, SpriteSnapshotStats* stats)
    {
        stats->m_PayloadUsedBytes += (uint64_t)array.Size() * sizeof(T);
        stats->m_FrameCapacityBytes += (uint64_t)array.Capacity() * sizeof(T);
    }

    template <typename K> static uint32_t FrameMapCapacity(const dmHashTable<K, uint32_t>& map)
    {
        return map.Capacity() ? sizeof(uint32_t) * dmMath::Max(1U, map.Capacity() * 2 / 3) + sizeof(typename dmHashTable<K, uint32_t>::Entry) * map.Capacity() : 0;
    }

    static void ComputeSpriteSnapshotStats(SpriteWorld* world, const SpriteRenderFrame* frame, SpriteSnapshotStats* stats, bool renderer_idle = true)
    {
        memset(stats, 0, sizeof(*stats));
        const SpriteRendererState& renderer = world->m_Renderer;
        stats->m_RecordBytes = sizeof(SpriteRenderData);
        stats->m_BoundBytes = sizeof(SpriteCullingInfo);
        stats->m_Inline = frame != 0;
        stats->m_CaptureCount = renderer.m_CaptureCount;
        stats->m_CaptureTotalUs = renderer.m_CaptureTotalUs;
        if (renderer_idle)
        {
            stats->m_RendererCpuCapacityBytes = renderer.m_VertexMemorySize + renderer.m_IndexCapacityBytes +
                renderer.m_AttributeScratch.Capacity() * sizeof(dmGraphics::VertexAttributeInfo) +
                renderer.m_ConstantBuffers.Capacity() * sizeof(dmRender::HNamedConstantBuffer) +
                renderer.m_RenderObjects.Capacity() * sizeof(dmRender::RenderObject*) +
                renderer.m_RenderObjects.Size() * sizeof(dmRender::RenderObject) +
                (renderer.m_ScratchPositionWorld.Capacity() + renderer.m_ScratchPositionLocal.Capacity()) * sizeof(Vector4);
            for (uint32_t i = 0; i < MAX_TEXTURE_COUNT; ++i)
                stats->m_RendererCpuCapacityBytes += renderer.m_ScratchUVs[i].Capacity() * sizeof(float);
            for (uint32_t i = 0; i < renderer.m_ConstantBuffers.Size(); ++i)
                stats->m_ConstantBufferCapacityBytes += dmRender::GetNamedConstantBufferCapacity(renderer.m_ConstantBuffers[i]);
            stats->m_RendererCpuCapacityBytes += stats->m_ConstantBufferCapacityBytes;
            stats->m_RendererGpuLogicalBytes = dmRender::GetBufferedRenderBufferDataSize(renderer.m_VertexBuffer) + dmRender::GetBufferedRenderBufferDataSize(renderer.m_IndexBuffer);
        }
        if (!frame)
            return;
        stats->m_SpriteCount = frame->m_Sprites.Size();
        stats->m_BindingCount = frame->m_Bindings.Size();
        stats->m_GeometryCount = frame->m_Geometry.Size();
        stats->m_ConstantBlockCount = frame->m_Constants.Size();
        stats->m_AttributeBlockCount = frame->m_Attributes.Size();
        stats->m_FrameCapacityBytes = sizeof(*frame);
        stats->m_FrameGrowthPeakBytes = frame->m_GrowthPeakBytes;
        CountFrameArray(frame->m_Sprites, stats);
        CountFrameArray(frame->m_Bounds, stats);
        CountFrameArray(frame->m_Bindings, stats);
        CountFrameArray(frame->m_Geometry, stats);
        CountFrameArray(frame->m_Constants, stats);
        CountFrameArray(frame->m_ConstantNext, stats);
        CountFrameArray(frame->m_ConstantDescriptors, stats);
        CountFrameArray(frame->m_ConstantValues, stats);
        CountFrameArray(frame->m_Attributes, stats);
        CountFrameArray(frame->m_AttributeValues, stats);
        stats->m_FrameCapacityBytes += FrameMapCapacity(frame->m_ResourceSizes) + FrameMapCapacity(frame->m_BindingMap) + FrameMapCapacity(frame->m_GeometryMap) + FrameMapCapacity(frame->m_ConstantMap);
        stats->m_RetainedResourceReportedBytes = frame->m_ResourceBytes;
        for (uint32_t i = 0; i < frame->m_Bindings.Size(); ++i)
            stats->m_RetainedReferenceCount += 2 + frame->m_Bindings[i].m_Resolved.m_NumTextures;
    }

    void GetSpriteSnapshotStats(void* sprite_world, SpriteSnapshotStats* stats)
    {
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        if (world->m_Threaded)
            *stats = world->m_ThreadStats;
        else
            ComputeSpriteSnapshotStats(world, world->m_Renderer.m_Frame, stats);
    }

    void ReleaseSpriteThreadFrames(void* sprite_world, SpriteContext* context)
    {
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        for (uint32_t i = 0; i < 2; ++i)
            if (world->m_ThreadFrames[i])
                ReleaseSpriteFrame(world->m_ThreadFrames[i], context->m_Factory);
    }

    bool CaptureSpriteThreadFrame(void* sprite_world, SpriteContext* context, uint32_t slot, uint32_t capacity_limit)
    {
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        assert(slot < 2 && context->m_SnapshotThreaded);
        assert(world->m_Threaded || !world->m_Renderer.m_Frame);
        world->m_Threaded = true;
        // Only this reserved slot is free. Never release the other reader slot.
        // Factory references and capture counters remain producer-owned.
        if (!world->m_ThreadFrames[slot])
            world->m_ThreadFrames[slot] = new SpriteRenderFrame();
        world->m_ThreadFrames[slot]->m_CapacityLimit = dmMath::Min(capacity_limit, 32U * 1024 * 1024);
        uint64_t begin = dmTime::GetMonotonicTime();
        CaptureSpriteFrame(world, context->m_Factory, world->m_ThreadFrames[slot]);
        world->m_Renderer.m_CaptureTotalUs += dmTime::GetMonotonicTime() - begin;
        ++world->m_Renderer.m_CaptureCount;
        ComputeSpriteSnapshotStats(world, world->m_ThreadFrames[slot], &world->m_ThreadStats, false);
        SpriteSnapshotStats other;
        ComputeSpriteSnapshotStats(world, world->m_ThreadFrames[1 - slot], &other, false);
        world->m_ThreadStats.m_FrameCapacityBytes += other.m_FrameCapacityBytes;
        world->m_ThreadStats.m_FrameGrowthPeakBytes += other.m_FrameGrowthPeakBytes;
        world->m_ThreadStats.m_Threaded = 1;
        // Hard admission budget, including both slot capacities and capture maps.
        if (world->m_ThreadFrames[slot]->m_Overflow || world->m_ThreadFrames[slot]->m_CapacityBytes > capacity_limit || world->m_ThreadStats.m_FrameCapacityBytes > 64 * 1024 * 1024)
        {
            dmLogError("Sprite thread slot capacity budget exceeded; frame rejected");
            return false;
        }
        return true;
    }

    void FinishSpriteThreadFrame(void* sprite_world, SpriteContext* context, uint32_t slot)
    {
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        // Called after the previous consumer completes, before publishing this slot.
        if (world->m_ThreadFrames[1 - slot])
            ReleaseSpriteFrame(world->m_ThreadFrames[1 - slot], context->m_Factory);
        SpriteSnapshotStats renderer;
        ComputeSpriteSnapshotStats(world, 0, &renderer);
        world->m_ThreadStats.m_RendererCpuCapacityBytes = renderer.m_RendererCpuCapacityBytes;
        world->m_ThreadStats.m_ConstantBufferCapacityBytes = renderer.m_ConstantBufferCapacityBytes;
        world->m_ThreadStats.m_RendererGpuLogicalBytes = renderer.m_RendererGpuLogicalBytes;
    }

    void RenderSpriteThreadFrame(void* sprite_world, SpriteContext* context, uint32_t slot)
    {
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        SpriteRenderFrame* frame = world->m_ThreadFrames[slot];
        world->m_Renderer.m_Frame = frame;
        world->m_Renderer.m_LegacyWorld = 0;
        SubmitSpriteFrame(&world->m_Renderer, context->m_RenderContext, frame->m_VertexCount, frame->m_IndexCount, frame->m_VertexMemorySize);
    }

    static void DeleteSpriteRenderPayload(void* payload)
    {
        SpriteRenderFrame* frame = (SpriteRenderFrame*)payload;
        for (uint32_t i = 0; i < frame->m_ResolvedBindings.Size(); ++i)
            delete frame->m_ResolvedBindings[i];
        delete frame;
    }

    static void SubmitSpriteRenderPayload(void* state, dmRender::HRenderContext context, const dmRender::RenderFrame& owner)
    {
        SpriteRendererState* renderer = (SpriteRendererState*)state;
        SpriteRenderFrame* frame = (SpriteRenderFrame*)owner.m_Payloads[0].m_Data;
        renderer->m_LegacyWorld = 0;
        renderer->m_Frame = frame;
        SubmitSpriteFrame(renderer, context, frame->m_VertexCount, frame->m_IndexCount, frame->m_VertexMemorySize, &owner);
    }

    bool RegisterSpriteRenderFrame(void* sprite_world, dmRender::RenderFrameConsumers* consumers)
    {
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        // Renderer scratch is exclusively accessed by the registered consumer.
        world->m_Renderer.m_LegacyWorld = 0;
        world->m_Threaded = true;
        return dmRender::RegisterRenderFrameConsumer(consumers, 0, &world->m_Renderer, SubmitSpriteRenderPayload);
    }

    bool CaptureSpriteRenderFrame(void* sprite_world, SpriteContext* context, dmRender::RenderFrameBuilder* builder)
    {
        uint64_t capture_begin = dmTime::GetMonotonicTime();
        SpriteWorld* world = (SpriteWorld*)sprite_world;
        dmRender::RenderFramePayload& payload = builder->m_Frame->m_Payloads[0];
        if (!payload.m_Data)
        {
            if (!dmRender::AdmitRenderFrameAllocation(builder, sizeof(SpriteRenderFrame), 0)) return false;
            payload.m_Data = new SpriteRenderFrame();
            payload.m_Delete = DeleteSpriteRenderPayload;
            payload.m_Capacity = sizeof(SpriteRenderFrame);
        }
        SpriteRenderFrame* frame = (SpriteRenderFrame*)payload.m_Data;
        frame->m_CentralDependencies = true;
        frame->m_Builder = builder;
        CaptureSpriteFrame(world, context->m_Factory, frame);
        // Resolve mutable resource wrappers into slot-owned adapters only after
        // binding deduplication. The retained generation protects immutable DDF.
        for (uint32_t i = 0; !frame->m_Overflow && i < frame->m_Bindings.Size(); ++i)
        {
            if (i == frame->m_ResolvedBindings.Size())
            {
                if (!GrowSpriteFrame(frame, sizeof(SpriteFrameResolved), 0)) break;
                SpriteFrameResolved* resolved = new SpriteFrameResolved();
                if (!FramePush(frame, frame->m_ResolvedBindings, resolved)) { delete resolved; break; }
            }
            SpriteFrameBinding& binding = frame->m_Bindings[i];
            SpriteFrameResolved& resolved = *frame->m_ResolvedBindings[i];
            resolved.m_Material = *binding.m_Resolved.m_Material;
            for (uint32_t t = 0; t < resolved.m_Material.m_NumTextures; ++t)
            {
                if (!resolved.m_Material.m_Textures[t]) continue;
                resolved.m_MaterialTextures[t] = *resolved.m_Material.m_Textures[t];
                resolved.m_Material.m_Textures[t] = &resolved.m_MaterialTextures[t];
            }
            binding.m_Resolved.m_Material = &resolved.m_Material;
            for (uint32_t t = 0; t < binding.m_Resolved.m_NumTextures; ++t)
            {
                const TextureSetResource& source = *binding.m_Textures[t].m_TextureSet;
                resolved.m_Textures[t] = *source.m_Texture;
                resolved.m_TextureSets[t].m_Texture = &resolved.m_Textures[t];
                resolved.m_TextureSets[t].m_TextureSet = source.m_TextureSet;
                resolved.m_TextureSets[t].m_TexturesGeneration = source.m_TexturesGeneration;
                binding.m_Textures[t].m_TextureSet = &resolved.m_TextureSets[t];
            }
        }
        for (uint32_t i = 0; !frame->m_Overflow && i < frame->m_Sprites.Size(); ++i)
        {
            const SpriteRenderData& data = frame->m_Sprites[i];
            const SpriteCullingInfo& bound = frame->m_Bounds[i];
            uint32_t offset = dmRender::AllocateRenderFrameData(builder, sizeof(uint32_t));
            if (offset == UINT32_MAX) { frame->m_Overflow = true; break; }
            memcpy(builder->m_Frame->m_Data.Begin() + offset, &i, sizeof(i));
            dmRender::RenderFrameEntry entry = {};
            entry.m_Bounds = Vector4(bound.m_Position[0], bound.m_Position[1], bound.m_Position[2], bound.m_Radius);
            entry.m_Payload = offset;
            entry.m_PayloadBytes = sizeof(uint32_t);
            entry.m_BatchKey = data.m_BatchKey;
            entry.m_TagListKey = data.m_TagListKey;
            entry.m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            if (!dmRender::AddRenderFrameEntry(builder, entry)) { frame->m_Overflow = true; break; }
        }
        payload.m_GrowthPeak = frame->m_GrowthPeakBytes;
        ++world->m_Renderer.m_CaptureCount;
        world->m_Renderer.m_CaptureTotalUs += dmTime::GetMonotonicTime() - capture_begin;
        ComputeSpriteSnapshotStats(world, frame, &world->m_ThreadStats, false);
        world->m_ThreadStats.m_FrameCapacityBytes = payload.m_Capacity;
        frame->m_Builder = 0;
        return !frame->m_Overflow;
    }

    // For tests
    void GetSpriteWorldRenderBuffers(void* sprite_world, dmRender::HBufferedRenderBuffer* vx_buffer, dmRender::HBufferedRenderBuffer* ix_buffer)
    {
        SpriteWorld* world = (SpriteWorld*) sprite_world;
        *vx_buffer = world->m_Renderer.m_VertexBuffer;
        *ix_buffer = world->m_Renderer.m_IndexBuffer;
    }

    uint32_t GetSpriteWorldVertexBufferCapacity(void* sprite_world)
    {
        return ((SpriteWorld*) sprite_world)->m_Renderer.m_VertexMemorySize;
    }

    void GetSpriteWorldDynamicAttributePool(void* sprite_world, DynamicAttributePool** pool_out)
    {
        *pool_out = &((SpriteWorld*) sprite_world)->m_DynamicVertexAttributePool;
    }

    void GetSpriteComponentScale(void* sprite_component, dmVMath::Vector3* scale_out)
    {
        SpriteComponent* comp = (SpriteComponent*) sprite_component;
        *scale_out = comp->m_Scale;
    }

    uint16_t GetSpriteComponentAnimationIndex(void* sprite_component)
    {
        SpriteComponent* comp = (SpriteComponent*) sprite_component;
        return comp->m_AnimationID;
    }
}
