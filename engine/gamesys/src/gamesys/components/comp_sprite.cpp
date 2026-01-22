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

#include <string.h>
#include <float.h>
#include <algorithm>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/double_linked_list.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
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
        uint32_t                        m_CacheKey;
        uint32_t                        m_VertexCount;
        uint32_t                        m_IndicesCount;
        float                           m_PageIndices[MAX_TEXTURE_COUNT];

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
        uint16_t                    m_AnimationID; // index into array
        uint16_t                    m_DynamicVertexAttributeIndex;
        uint16_t                    m_ComponentIndex;
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

    struct SpriteWorld
    {
        AnimationDataCache                  m_AnimationDataCache;
        dmObjectPool<SpriteComponent>       m_Components;
        DynamicAttributePool                m_DynamicVertexAttributePool;
        dmArray<dmRender::RenderObject*>    m_RenderObjects;
        dmArray<float>                      m_BoundingVolumes;
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

    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SIZE, size, false);
    DM_GAMESYS_PROP_VECTOR4(SPRITE_PROP_SLICE, slice, false);

    static const dmhash_t SPRITE_PROP_CURSOR        = dmHashString64("cursor");
    static const dmhash_t SPRITE_PROP_PLAYBACK_RATE = dmHashString64("playback_rate");
    static const dmhash_t SPRITE_PROP_ANIMATION     = dmHashString64("animation");
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

    static float GetCursor(SpriteComponent* component);
    static void SetCursor(SpriteComponent* component, float cursor);
    static float GetPlaybackRate(SpriteComponent* component);
    static void SetPlaybackRate(SpriteComponent* component, float playback_rate);

    static void ReAllocateBuffers(SpriteWorld* sprite_world, dmRender::HRenderContext render_context)
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
        sprite_world->m_BoundingVolumes.SetCapacity(comp_count);
        sprite_world->m_BoundingVolumes.SetSize(comp_count);
        sprite_world->m_AnimationDataCache.m_Cache.SetCapacity(MINIMUM_CACHE_CAPACITY);
        dmDoubleLinkedList::ListInit(&sprite_world->m_AnimationDataCache.m_LRU);
        memset(sprite_world->m_Components.GetRawObjects().Begin(), 0, sizeof(SpriteComponent) * comp_count);
        sprite_world->m_RenderObjectsInUse = 0;
        sprite_world->m_VertexBuffer     = 0;
        sprite_world->m_VertexBufferData = 0;
        sprite_world->m_IndexBuffer      = 0;
        sprite_world->m_IndexBufferData  = 0;

        InitializeMaterialAttributeInfos(sprite_world->m_DynamicVertexAttributePool, 8);

        *params.m_World = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        DestroyMaterialAttributeInfos(sprite_world->m_DynamicVertexAttributePool);

        for (uint32_t i = 0; i < sprite_world->m_RenderObjects.Size(); ++i)
        {
            delete sprite_world->m_RenderObjects[i];
        }

        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::DeleteBufferedRenderBuffer(sprite_context->m_RenderContext, sprite_world->m_VertexBuffer);
        free(sprite_world->m_VertexBufferData);
        dmRender::DeleteBufferedRenderBuffer(sprite_context->m_RenderContext, sprite_world->m_IndexBuffer);
        free(sprite_world->m_IndexBufferData);

        dmHashTable32<AnimationData*>::Iterator iter = sprite_world->m_AnimationDataCache.m_Cache.GetIterator();
        while(iter.Next())
        {
            AnimationData* data = iter.GetValue();
            free(data);
        }

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
        if (!texture || !texture->m_TextureSet)
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

    dmGraphics::HTexture GetMaterialTexture(const SpriteComponent* component, uint32_t texture_unit)
    {
        TextureResource* texture = GetTextureResource(component, texture_unit);
        return texture ? texture->m_Texture : 0;
    }

    static void UpdateCurrentAnimationFrame(SpriteComponent* component)
    {
        TextureSetResource* texture_set = GetFirstTextureSet(component);
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

    static bool PlayAnimation(SpriteComponent* component, dmhash_t animation, float offset, float playback_rate)
    {
        TextureSetResource* texture_set = GetFirstTextureSet(component);
        uint32_t* anim_id = texture_set ? texture_set->m_AnimationIds.Get(animation) : 0;
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
            component->m_AnimationReHash |= component->m_CurrentAnimation != 0x0 && component->m_CurrentAnimationFrame != 0;
            // TODO: Why stop the current animation? Shouldn't it continue playing the current animation?
            component->m_IsPlaying = 0;
            component->m_CurrentAnimation = 0x0;
            component->m_CurrentAnimationFrame = 0;
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
            ShowFullBufferError("Sprite", "sprite.max_count", sprite_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
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

    template <typename T>
    static void EnsureSize(dmArray<T>& array, uint32_t size)
    {
        if (array.Capacity() < size) {
            array.OffsetCapacity(size - array.Capacity());
        }
        array.SetSize(size);
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
        const float** pi_channels, uint8_t pi_channels_count)
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

    static void CreateVertexDataSlice9(
        const SpriteComponent* component,
        uint8_t* vertices,
        uint8_t* indices,
        bool is_indices_16_bit,
        bool has_local_position_attribute,
        const Matrix4& world_matrix,
        uint32_t vertex_offset,
        uint32_t vertex_stride,
        AnimationData* anim_data,
        dmArray<float>* scratch_uvs,
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT],
        float* scratch_pi_ptrs[MAX_TEXTURE_COUNT],
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
            EnsureSize(uvs, SPRITE_VERTEX_COUNT_SLICE9*2);

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
            scratch_pi_ptrs[i] = (float*) &anim_data->m_PageIndices[i];
            uv_channels_count++;
        }

        // disable slice9 computation below a certain threshold
        // (avoid div by zero)
        const float s9_min_dim = 0.001f;

        if (texture_num == 0)
        {
            dmArray<float>& uvs = scratch_uvs[0];
            EnsureSize(uvs, SPRITE_VERTEX_COUNT_SLICE9*2);

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
            scratch_pi_ptrs[0] = (float*) &anim_data->m_PageIndices[0];
            uv_channels_count  = 1;
        }

        const float sx = sprite_size.getX() > s9_min_dim ? 1.0f / sprite_size.getX() : 0;
        const float sy = sprite_size.getY() > s9_min_dim ? 1.0f / sprite_size.getY() : 0;

        float xs[4], ys[4];
        // v are '1-v'
        xs[0] = ys[0] = 0;
        xs[3] = ys[3] = 1;

        xs[1] = sx * slice9.getX();
        xs[2] = 1 - sx * slice9.getZ();
        ys[1] = sy * slice9.getW();
        ys[2] = 1 - sy * slice9.getY();

        EnsureSize(*scratch_positions_world, SPRITE_VERTEX_COUNT_SLICE9);
        if (has_local_position_attribute)
        {
            EnsureSize(*scratch_positions_local, SPRITE_VERTEX_COUNT_SLICE9);
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
            uv_channels_count);

        // We always use the first geometry for the vertices
        float pivot_x = 0.f;
        float pivot_y = 0.f;
        GetPivot(anim_data, &pivot_x, &pivot_y);

        uint32_t sp_width = sprite_size.getX();
        uint32_t sp_height = sprite_size.getY();
        uint32_t vertex_index = 0;
        for (int y=0; y<4; y++)
        {
            for (int x=0; x<4; x++)
            {
                // convert from [0,1] to [-0.5, 0.5]
                float px = xs[x] - 0.5f;
                float py = ys[y] - 0.5f;
                Point3 p = Point3(px - pivot_x, py - pivot_y, 0);

                if (has_local_position_attribute)
                {
                    (*scratch_positions_local)[vertex_index] = Vector4(
                        p.getX() * sp_width,
                        p.getY() * sp_height,
                        0.0f, 1.0f);
                }

                (*scratch_positions_world)[vertex_index] = world_matrix * p;

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

    static void ResolveUVDataFromQuads(const SpriteComponent* component, AnimationData* data, dmArray<float>* scratch_uvs, float* scratch_uv_ptrs[MAX_TEXTURE_COUNT], float* scratch_pi_ptrs[MAX_TEXTURE_COUNT])
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
            EnsureSize(uvs, 4*2);

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
        }

        if (texture_num == 0)
        {
            dmArray<float>& uvs = scratch_uvs[0];
            EnsureSize(uvs, 4*2);

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
        AnimationData* anim_data,
        dmArray<Vector4>& scratch_pos,
        dmArray<float>* scratch_uvs,
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT],
        float* scratch_pi_ptrs[MAX_TEXTURE_COUNT],
        float scale_x, float scale_y, int reverse)
    {
        uint32_t num_vertices = anim_data->m_Geometries[0]->m_Vertices.m_Count / 2;
        float* orig_vertices = anim_data->m_Geometries[0]->m_Vertices.m_Data;
        int step = reverse ? -2 : 2;

        EnsureSize(scratch_pos, num_vertices);

        uint8_t textures_num = component->m_NumTextures;
        for (uint8_t i = 0; i < textures_num; ++i)
        {
            dmArray<float>& uvs = scratch_uvs[i];
            EnsureSize(uvs, num_vertices * 2);

            scratch_uv_ptrs[i] = uvs.Begin();
            scratch_pi_ptrs[i] = &anim_data->m_PageIndices[i];

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

            float pivot_x = 0.f;
            float pivot_y = 0.f;
            GetPivot(anim_data, &pivot_x, &pivot_y);

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
                    float vx = px - pivot_x;
                    float vy = py - pivot_y;
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

    static void CreateVertexData(SpriteWorld* sprite_world, dmGraphics::VertexAttributeInfos* material_attribute_info, 
        bool has_local_position_attribute, uint8_t** vb_where, uint8_t** ib_where, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("CreateVertexData");

        uint8_t* vertices        = *vb_where;
        uint8_t* indices         = *ib_where;
        uint32_t index_type_size = sprite_world->m_Is16BitIndex ? sizeof(uint16_t) : sizeof(uint32_t);

        const dmArray<SpriteComponent>& components = sprite_world->m_Components.GetRawObjects();

        // The offset for the indices
        uint32_t vertex_offset = sprite_world->m_VerticesWritten;
        uint32_t vertex_stride = material_attribute_info->m_VertexStride;

        // The list of pointers to the scratch uvs and page indices
        float* scratch_uv_ptrs[MAX_TEXTURE_COUNT] = {};
        float* scratch_pi_ptrs[MAX_TEXTURE_COUNT] = {};

        dmGraphics::VertexAttributeInfos sprite_attribute_info = {};
        dmGraphics::WriteAttributeParams write_params = {};

        for (uint32_t* i = begin; i != end; ++i)
        {
            uint32_t component_index         = (uint32_t)buf[*i].m_UserData;
            const SpriteComponent* component = (const SpriteComponent*) &components[component_index];
            const Matrix4& world_matrix      = component->m_World;

            float sp_width  = component->m_Size.getX();
            float sp_height = component->m_Size.getY();
            uint8_t textures_num = component->m_NumTextures;
            AnimationData* animations = GetOrCreateAnimationData(sprite_world, component);
    
            // Fill in the custom sprite attributes (if specified), otherwise fallback to use the material attributes
            dmGraphics::VertexAttributeInfos* sprite_attribute_info_ptr = material_attribute_info;
            if (component->m_Resource->m_DDF->m_Attributes.m_Count > 0 || component->m_DynamicVertexAttributeIndex != INVALID_DYNAMIC_ATTRIBUTE_INDEX)
            {
                FillAttributeInfos(&sprite_world->m_DynamicVertexAttributePool,
                    component->m_DynamicVertexAttributeIndex,
                    component->m_Resource->m_DDF->m_Attributes.m_Data,
                    component->m_Resource->m_DDF->m_Attributes.m_Count,
                    material_attribute_info,
                    &sprite_attribute_info);

                sprite_attribute_info_ptr = &sprite_attribute_info;
            }

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

                ResolvePositionAndUVDataFromGeometry(component, animations, sprite_world->m_ScratchPositionWorld, sprite_world->m_ScratchUVs, scratch_uv_ptrs, scratch_pi_ptrs, scaleX, scaleY, reverse);

                if (has_local_position_attribute)
                {
                    EnsureSize(sprite_world->m_ScratchPositionLocal, sprite_world->m_ScratchPositionWorld.Size());
                }

                const float* world_matrix_channel[]    = { (float*) &world_matrix };
                const float* world_position_channels[] = { (float*) sprite_world->m_ScratchPositionWorld.Begin() };
                const float* local_position_channels[] = { (float*) sprite_world->m_ScratchPositionLocal.Begin() };

                FillWriteVertexAttributeParams(&write_params, sprite_attribute_info_ptr,
                    world_matrix_channel,
                    world_position_channels,
                    local_position_channels,
                    (const float**) scratch_uv_ptrs,
                    textures_num,
                    (const float**) scratch_pi_ptrs,
                    textures_num);

                uint32_t num_vertices = sprite_world->m_ScratchPositionWorld.Size();
                for (uint32_t vertex_index = 0; vertex_index < num_vertices; ++vertex_index)
                {
                    if (has_local_position_attribute)
                    {
                        sprite_world->m_ScratchPositionLocal[vertex_index] = Vector4(
                            sprite_world->m_ScratchPositionWorld[vertex_index].getX() * sp_width,
                            sprite_world->m_ScratchPositionWorld[vertex_index].getY() * sp_height,
                            0.0f, 1.0f);
                    }

                    sprite_world->m_ScratchPositionWorld[vertex_index] = world_matrix * sprite_world->m_ScratchPositionWorld[vertex_index];
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
                    CreateVertexDataSlice9(component, vertices, indices, sprite_world->m_Is16BitIndex, has_local_position_attribute,
                        world_matrix, vertex_offset, vertex_stride,
                        animations, sprite_world->m_ScratchUVs, scratch_uv_ptrs, scratch_pi_ptrs,
                        &sprite_world->m_ScratchPositionWorld, &sprite_world->m_ScratchPositionLocal,
                        sprite_attribute_info_ptr);

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
                    ResolveUVDataFromQuads(component, animations, sprite_world->m_ScratchUVs, scratch_uv_ptrs, scratch_pi_ptrs);

                    // We always use the first geometry for the vertices
                    float pivot_x = 0.f;
                    float pivot_y = 0.f;
                    GetPivot(animations, &pivot_x, &pivot_y);

                    float x0 = -0.5f - pivot_x;
                    float x1 =  0.5f - pivot_x;
                    float y0 = -0.5f - pivot_y;
                    float y1 =  0.5f - pivot_y;

                    Vector4 positions_world[] = {
                        world_matrix * Point3(x0, y0, 0.0f),
                        world_matrix * Point3(x0, y1, 0.0f),
                        world_matrix * Point3(x1, y1, 0.0f),
                        world_matrix * Point3(x1, y0, 0.0f)};

                    Vector4 positions_local[4];
                    if (has_local_position_attribute)
                    {
                        positions_local[0] = Vector4(x0 * sp_width, y0 * sp_height, 0.0f, 1.0f);
                        positions_local[1] = Vector4(x0 * sp_width, y1 * sp_height, 0.0f, 1.0f);
                        positions_local[2] = Vector4(x1 * sp_width, y1 * sp_height, 0.0f, 1.0f);
                        positions_local[3] = Vector4(x1 * sp_width, y0 * sp_height, 0.0f, 1.0f);
                    }

                    const float* world_matrix_channel[]    = { (float*) &world_matrix };
                    const float* local_position_channels[] = { (float*) &positions_local };
                    const float* world_position_channels[] = { (float*) &positions_world };

                    const uint8_t uv_channels_count = textures_num != 0 ? textures_num : 1;
                    const uint8_t pi_channels_count = textures_num != 0 ? textures_num : 1;

                    FillWriteVertexAttributeParams(&write_params,
                        sprite_attribute_info_ptr,
                        world_matrix_channel,
                        world_position_channels,
                        local_position_channels,
                        (const float**) scratch_uv_ptrs,
                        uv_channels_count,
                        (const float**) scratch_pi_ptrs,
                        pi_channels_count);

                    vertices = dmGraphics::WriteAttributes(vertices, 0, 4, write_params);

                #if 0
                    for (int f = 0; f < 4; ++f)
                        printf("  %u: %.2f, %.2f\t%.2f, %.2f\n", f, vertices[f].x, vertices[f].y, vertices[f].u, vertices[f].v );
                #endif

                    if (sprite_world->m_Is16BitIndex)
                    {
                        uint16_t* indices_16 = (uint16_t*) indices;
                        indices_16[0] = vertex_offset + 0;
                        indices_16[1] = vertex_offset + 1;
                        indices_16[2] = vertex_offset + 2;
                        indices_16[3] = vertex_offset + 2;
                        indices_16[4] = vertex_offset + 3;
                        indices_16[5] = vertex_offset + 0;
                    }
                    else
                    {
                        uint32_t* indices_32 = (uint32_t*) indices;
                        indices_32[0] = vertex_offset + 0;
                        indices_32[1] = vertex_offset + 1;
                        indices_32[2] = vertex_offset + 2;
                        indices_32[3] = vertex_offset + 2;
                        indices_32[4] = vertex_offset + 3;
                        indices_32[5] = vertex_offset + 0;
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

    static void RenderBatch(SpriteWorld* sprite_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("SpriteRenderBatch");

        uint32_t component_index = (uint32_t)buf[*begin].m_UserData;
        const SpriteComponent* first = (const SpriteComponent*) &sprite_world->m_Components.GetRawObjects()[component_index];
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
        FillMaterialAttributeInfos(material, vx_decl, &material_attribute_info, dmGraphics::COORDINATE_SPACE_WORLD);

        // Fill in vertex buffer
        uint8_t* vb_begin = sprite_world->m_VertexBufferWritePtr;
        uint8_t* ib_begin = (uint8_t*)sprite_world->m_IndexBufferWritePtr;
        uint8_t* vb_iter  = vb_begin;
        uint8_t* ib_iter  = ib_begin;

        dmGraphics::VertexAttributeInfoMetadata material_attribute_info_meta = dmGraphics::GetVertexAttributeInfosMetaData(material_attribute_info);
        CreateVertexData(sprite_world, &material_attribute_info, material_attribute_info_meta.m_HasAttributeLocalPosition, &vb_iter, &ib_iter, buf, begin, end);

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
        if (constants) {
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
        Matrix4 local = dmTransform::ToMatrix4(dmTransform::Transform(component->m_Position, component->m_Rotation, 1.0f));
        Matrix4 world = dmGameObject::GetWorldMatrix(component->m_Instance);
        Vector3 size = dmVMath::MulPerElem(component->m_Size, component->m_Scale);
        size.setZ(1.f);
        Matrix4 w = dmVMath::AppendScale(world * local, size);
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

    static void UpdateVertexAndIndexCount(SpriteWorld* sprite_world, const SpriteComponent* component, dmRender::HRenderContext render_context, uint32_t& num_vertices,
        uint32_t& num_indices, uint32_t& vertex_memsize)
    {
        dmRender::HMaterial material           = GetRenderMaterial(render_context, component);
        dmGraphics::HVertexDeclaration vx_decl = dmRender::GetVertexDeclaration(material);
        uint32_t vertex_stride                 = dmGraphics::GetVertexDeclarationStride(vx_decl);
        uint8_t textures_num                   = component->m_NumTextures;

        // We need to pad the buffer if the vertex stride doesn't start at an even byte offset from the start
        vertex_memsize += vertex_stride - vertex_memsize % vertex_stride;

        // Get the correct animation frames, and other meta data
        AnimationData* anim_data = GetOrCreateAnimationData(sprite_world, component);

        if (textures_num == 0 || anim_data->m_CanUseQuads)
        {
            if (component->m_UseSlice9)
            {
                num_vertices   += SPRITE_VERTEX_COUNT_SLICE9;
                num_indices    += SPRITE_INDEX_COUNT_SLICE9;
                vertex_memsize += SPRITE_VERTEX_COUNT_SLICE9 * vertex_stride;
            }
            else
            {
                num_vertices   += SPRITE_VERTEX_COUNT_LEGACY;
                num_indices    += SPRITE_INDEX_COUNT_LEGACY;
                vertex_memsize += SPRITE_VERTEX_COUNT_LEGACY * vertex_stride;
            }
        }
        else
        {
            num_vertices += anim_data->m_VertexCount;
            num_indices += anim_data->m_IndicesCount;
            vertex_memsize += anim_data->m_VertexCount * vertex_stride;
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

        uint32_t num_vertices    = 0;
        uint32_t num_indices     = 0;
        uint32_t vertex_memsize  = 0;
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
            if (component->m_ReHash || (constants && dmGameSystem::AreRenderConstantsUpdated(constants)))
            {
                ReHash(component);
            }
            if (component->m_AnimationReHash)
            {
                AnimationReHash(component);
            }

            UpdateVertexAndIndexCount(world, component, render_context, num_vertices, num_indices, vertex_memsize);

            // TODO: check when we need send messages
            if (!component->m_IsPlaying)
                continue;
            PostMessages(component);
        }

        // In case if CompSpriteUpdate will be called several times before CompSpriteRender - assign flag with "bit or"
        // to save flag state from previous update until ReallocBuffers will be called
        world->m_ReallocBuffers   |= vertex_memsize > world->m_VertexMemorySize || num_indices > world->m_IndexCount;
        world->m_VertexCount      = num_vertices;
        world->m_IndexCount       = num_indices;
        world->m_VertexMemorySize = vertex_memsize;

        dmRender::TrimBuffer(render_context, world->m_VertexBuffer);
        dmRender::RewindBuffer(render_context, world->m_VertexBuffer);

        dmRender::TrimBuffer(render_context, world->m_IndexBuffer);
        dmRender::RewindBuffer(render_context, world->m_IndexBuffer);

        world->m_DispatchCount = 0;

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

        bool sub_pixels = sprite_context->m_Subpixels;

        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate)
                continue;
            UpdateTransform(component, sub_pixels);
            // we need to consider the full scale here
            // I.e. we want the length of the diagonal C, where C = X + Y
            float radius_sq = dmVMath::LengthSqr((component->m_World.getCol(0).getXYZ() + component->m_World.getCol(1).getXYZ()) * 0.5f);
            world->m_BoundingVolumes[i] = radius_sq;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("Sprite");

        SpriteWorld* sprite_world = (SpriteWorld*)params.m_UserData;
        const float* radiuses = sprite_world->m_BoundingVolumes.Begin();

        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        dmArray<SpriteComponent>& components = sprite_world->m_Components.GetRawObjects();
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];

            float radius_sq = radiuses[entry->m_UserData];

            SpriteComponent* component = &components[entry->m_UserData];
            const AnimationData* anim_data = GetOrCreateAnimationData(sprite_world, component);
            float pivot_x = 0.f, pivot_y = 0.f;
            GetPivot(anim_data, &pivot_x, &pivot_y);
            Point3 pivot(-pivot_x, -pivot_y, 0.f);
            Vector3 world_pos = (component->m_World * pivot).getXYZ();

            bool intersect = dmIntersection::TestFrustumSphereSq(frustum, Point3(world_pos), radius_sq);
            entry->m_Visibility = intersect ? dmRender::VISIBILITY_FULL : dmRender::VISIBILITY_NONE;
        }
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        SpriteWorld* world = (SpriteWorld*) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                world->m_VertexBufferWritePtr = world->m_VertexBufferData;
                world->m_IndexBufferWritePtr = world->m_IndexBufferData;
                world->m_RenderObjectsInUse = 0;
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

    dmGameObject::UpdateResult CompSpriteRender(const dmGameObject::ComponentsRenderParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;

        sprite_world->m_VerticesWritten = 0;

        dmArray<SpriteComponent>& components = sprite_world->m_Components.GetRawObjects();
        uint32_t sprite_count = components.Size();

        if (!sprite_count)
            return dmGameObject::UPDATE_RESULT_OK;

        if (sprite_world->m_ReallocBuffers)
        {
            ReAllocateBuffers(sprite_world, render_context);
        }

        // Submit all sprites as entries in the render list for sorting.
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, sprite_count);
        dmRender::HRenderListDispatch sprite_dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, &RenderListFrustumCulling, sprite_world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < sprite_count; ++i)
        {
            SpriteComponent& component = components[i];
            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            const Vector3 trans = component.m_World.getCol(3).getXYZ();
            write_ptr->m_WorldPosition = Point3(trans);
            write_ptr->m_UserData = i; // Assuming the object pool stays intact
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetComponentMaterial(&component));
            write_ptr->m_Dispatch = sprite_dispatch;
            write_ptr->m_MinorOrder = 0;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;

            DM_PROPERTY_ADD_U32(rmtp_Sprite, 1);
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
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
            else if (message_id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(GetComponentMaterial(component), ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), ddf->m_Index, CompSpriteSetConstantCallback, component);
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

            if (params.m_Options.m_HasKey)
            {
                out_value.m_ValueType = dmGameObject::PROP_VALUE_HASHTABLE;
                texture_set = GetTextureSetByHash(component, params.m_Options.m_Key);
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
        else if (get_property == SPRITE_PROP_ANIMATION)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_CurrentAnimation);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        else if (get_property == SPRITE_PROP_FRAME_COUNT)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(GetAnimationFrameCount(component));
            return dmGameObject::PROPERTY_RESULT_OK;
        }

        dmRender::HMaterial material = GetComponentMaterial(component);
        if (GetMaterialConstant(material, get_property, params.m_Options.m_Index, out_value, false, CompSpriteGetConstantCallback, component) == dmGameObject::PROPERTY_RESULT_OK)
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
            dmhash_t sampler_name_hash = params.m_Options.m_HasKey ? params.m_Options.m_Key : 0;
            dmGameObject::PropertyResult res = AddOverrideTextureSet(dmGameObject::GetFactory(params.m_Instance), component, sampler_name_hash, params.m_Value.m_Hash);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;

            // Since the animation referred to the old texture, we need to update it
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                TextureSetResource* texture_set = GetFirstTextureSet(component);
                dmhash_t current_animation = component->m_CurrentAnimation;
                uint32_t* anim_id = texture_set ? texture_set->m_AnimationIds.Get(current_animation) : 0;
                if (!anim_id)
                {
                    DM_HASH_REVERSE_MEM(hash_ctx, 1024);
                    // it means that new atlas doesn't contain animation with the same name as it played before
                    const char* error_message = dmHashReverseSafe64Alloc(&hash_ctx, current_animation);
                    dmHashTable64<uint32_t>::Iterator animation_iterator = texture_set->m_AnimationIds.GetIterator();
                    if (animation_iterator.Next())
                    {
                        current_animation = animation_iterator.GetKey();
                        anim_id = const_cast<uint32_t*>(&animation_iterator.GetValue());

                        const char* new_animation_name = dmHashReverseSafe64Alloc(&hash_ctx, current_animation);
                        dmLogError("Atlas doesn't contains animation '%s'. Animation '%s' will be used", error_message, new_animation_name);
                    }
                    else
                    {
                        dmLogError("Atlas doesn't contains animation '%s'. No animation will be used", error_message);
                    }
                    // else anim_id still == 0
                }
                if (anim_id)
                {
                    PlayAnimation(component, current_animation, GetCursor(component), component->m_PlaybackRate);
                }
                else
                {
                    component->m_AnimationReHash |= current_animation != 0x0 && component->m_CurrentAnimationFrame != 0;
                    component->m_AnimationPlayback = dmGameSystemDDF::PLAYBACK_NONE;
                    component->m_IsPlaying = 0;
                    component->m_CurrentAnimation = 0x0;
                    component->m_CurrentAnimationFrame = 0;
                    dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
                    if ((uint16_t)texture_set_ddf->m_Animations.m_Count <= component->m_AnimationID)
                    {
                        component->m_AnimationID = 0;
                    }
                    component->m_AnimationReHash = 1;
                }
            }
            return res;
        }
        else if ((set_property == SPRITE_PROP_FRAME_COUNT) || (set_property == SPRITE_PROP_ANIMATION))
        {
            return dmGameObject::PROPERTY_RESULT_READ_ONLY;
        }

        dmRender::HMaterial material = GetComponentMaterial(component);
        dmGameObject::PropertyResult res = SetMaterialConstant(material, params.m_PropertyId, params.m_Value, params.m_Options.m_Index, CompSpriteSetConstantCallback, component);

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
                    // the size is baked into this matrix as the scale
                    value = Vector4(transform.GetScale());
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

    // For tests
    void GetSpriteWorldRenderBuffers(void* sprite_world, dmRender::HBufferedRenderBuffer* vx_buffer, dmRender::HBufferedRenderBuffer* ix_buffer)
    {
        SpriteWorld* world = (SpriteWorld*) sprite_world;
        *vx_buffer = world->m_VertexBuffer;
        *ix_buffer = world->m_IndexBuffer;
    }

    void GetSpriteWorldDynamicAttributePool(void* sprite_world, DynamicAttributePool** pool_out)
    {
        *pool_out = &((SpriteWorld*) sprite_world)->m_DynamicVertexAttributePool;
    }
}
