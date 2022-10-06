// Copyright 2020-2022 The Defold Foundation
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

DM_PROPERTY_EXTERN(rmtp_Components);
DM_PROPERTY_U32(rmtp_Sprite, 0, FrameReset, "# components", &rmtp_Components);
DM_PROPERTY_U32(rmtp_SpriteVertexCount, 0, FrameReset, "# vertices", &rmtp_Sprite);
DM_PROPERTY_U32(rmtp_SpriteVertexSize, 0, FrameReset, "size of vertices in bytes", &rmtp_Sprite);
DM_PROPERTY_U32(rmtp_SpriteIndexSize, 0, FrameReset, "size of indices in bytes", &rmtp_Sprite);

namespace dmGameSystem
{
    using namespace dmVMath;

    struct SpriteComponent
    {
        dmGameObject::HInstance     m_Instance;
        Vector3                     m_Position;
        Quat                        m_Rotation;
        Vector3                     m_Scale;
        Vector3                     m_Size;     // The current size of the animation frame (in texels)
        Matrix4                     m_World;
        // Hash of the m_Resource-pointer. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        // See GenerateKeys
        uint32_t                    m_MixedHash;
        int                         m_FunctionRef; // Animation callback function
        dmMessage::URL              m_Listener;
        uint32_t                    m_AnimationID;
        SpriteResource*             m_Resource;
        HComponentRenderConstants   m_RenderConstants;
        TextureSetResource*         m_TextureSet;
        dmRender::HMaterial         m_Material;
        /// Currently playing animation
        dmhash_t                    m_CurrentAnimation;
        uint32_t                    m_CurrentAnimationFrame;
        /// Used to scale the time step when updating the timer
        float                       m_AnimInvDuration;
        /// Timer in local space: [0,1]
        float                       m_AnimTimer;
        float                       m_PlaybackRate;
        uint16_t                    m_ComponentIndex;
        uint16_t                    m_AnimPingPong : 1;
        uint16_t                    m_AnimBackwards : 1;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_Playing : 1;
        uint16_t                    m_DoTick : 1;
        uint16_t                    m_FlipHorizontal : 1;
        uint16_t                    m_FlipVertical : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_ReHash : 1;
        uint16_t                    m_UseSlice9 : 1;
        uint16_t                    m_Padding : 6;
    };

    struct SpriteVertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
    };

    struct SpriteWorld
    {
        dmObjectPool<SpriteComponent>   m_Components;
        dmArray<dmRender::RenderObject*> m_RenderObjects;
        dmArray<float>                  m_BoundingVolumes;
        uint32_t                        m_RenderObjectsInUse;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        SpriteVertex*                   m_VertexBufferData;
        SpriteVertex*                   m_VertexBufferWritePtr;
        dmGraphics::HIndexBuffer        m_IndexBuffer;
        uint32_t                        m_VertexCount;
        uint32_t                        m_IndexCount;
        uint8_t*                        m_IndexBufferData;
        uint8_t*                        m_IndexBufferWritePtr;
        uint8_t                         m_Is16BitIndex : 1;
        uint8_t                         m_ReallocBuffers : 1;
    };

    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SIZE, size, false);

    static const dmhash_t SPRITE_PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t SPRITE_PROP_PLAYBACK_RATE = dmHashString64("playback_rate");
    static const dmhash_t SPRITE_PROP_ANIMATION = dmHashString64("animation");

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

    static void ReAllocateBuffers(SpriteWorld* sprite_world, dmRender::HRenderContext render_context) {
        if (sprite_world->m_VertexBuffer) {
            dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);
            sprite_world->m_VertexBuffer = 0;
        }

        sprite_world->m_VertexBuffer     = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        uint32_t vertex_memsize          = sizeof(SpriteVertex) * sprite_world->m_VertexCount;
        sprite_world->m_VertexBufferData = (SpriteVertex*) realloc(sprite_world->m_VertexBufferData, vertex_memsize);

        uint32_t index_data_type_size   = sprite_world->m_VertexCount <= 65536 ? sizeof(uint16_t) : sizeof(uint32_t);
        size_t indices_memsize          = sprite_world->m_IndexCount * index_data_type_size;
        sprite_world->m_Is16BitIndex    = index_data_type_size == sizeof(uint16_t) ? 1 : 0;
        sprite_world->m_IndexBufferData = (uint8_t*)realloc(sprite_world->m_IndexBufferData, indices_memsize);

        if (sprite_world->m_IndexBuffer)
        {
            dmGraphics::DeleteIndexBuffer(sprite_world->m_IndexBuffer);
            sprite_world->m_IndexBuffer = 0;
        }

        sprite_world->m_IndexBuffer    = dmGraphics::NewIndexBuffer(dmRender::GetGraphicsContext(render_context), indices_memsize, (void*)sprite_world->m_IndexBufferData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);
        sprite_world->m_ReallocBuffers = 0;
    }

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;
        SpriteWorld* sprite_world = new SpriteWorld();
        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, sprite_context->m_MaxSpriteCount);
        sprite_world->m_Components.SetCapacity(comp_count);
        sprite_world->m_BoundingVolumes.SetCapacity(comp_count);
        sprite_world->m_BoundingVolumes.SetSize(comp_count);
        memset(sprite_world->m_Components.m_Objects.Begin(), 0, sizeof(SpriteComponent) * comp_count);
        sprite_world->m_RenderObjectsInUse = 0;

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
        };

        sprite_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        sprite_world->m_VertexBuffer = 0;
        sprite_world->m_VertexBufferData = 0;
        sprite_world->m_IndexBuffer = 0;
        sprite_world->m_IndexBufferData = 0;

        *params.m_World = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        for (uint32_t i = 0; i < sprite_world->m_RenderObjects.Size(); ++i)
        {
            delete sprite_world->m_RenderObjects[i];
        }

        dmGraphics::DeleteVertexDeclaration(sprite_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);
        free(sprite_world->m_VertexBufferData);
        dmGraphics::DeleteIndexBuffer(sprite_world->m_IndexBuffer);
        free(sprite_world->m_IndexBufferData);

        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline Vector3 GetSizeFromAnimation(const SpriteComponent* sprite, dmGameSystemDDF::TextureSet* texture_set_ddf, uint32_t anim_id)
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

    static inline dmRender::HMaterial GetMaterial(const SpriteComponent* component, const SpriteResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline TextureSetResource* GetTextureSet(const SpriteComponent* component, const SpriteResource* resource) {
        return component->m_TextureSet ? component->m_TextureSet : resource->m_TextureSet;
    }

    static void UpdateCurrentAnimationFrame(SpriteComponent* component) {
        TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_AnimationID];

        // Set frame from cursor (tileindex or animframe)
        float t = component->m_AnimTimer;
        float backwards = (animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                        || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD) ? 1.0f : 0;

        // Original: t = backwards ? (1.0f - t) : t;
        // which translates to:
        t = backwards - 2 * t * backwards + t;

        uint32_t interval = animation_ddf->m_End - animation_ddf->m_Start;
        uint32_t frame_count = interval;
        if (animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG
                || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
        {
            frame_count = dmMath::Max(1u, frame_count * 2 - 2);
        }
        uint32_t frame = dmMath::Min(frame_count - 1, (uint32_t)(t * frame_count));
        if (frame >= interval) {
            frame = 2 * (interval - 1) - frame;
        }

        uint32_t frame_current = component->m_CurrentAnimationFrame;
        component->m_CurrentAnimationFrame = frame;

        if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_AUTO && frame != frame_current)
        {
            component->m_Size = GetSizeFromAnimation(component, texture_set_ddf, component->m_AnimationID);
        }
    }

    static bool PlayAnimation(SpriteComponent* component, dmhash_t animation, float offset, float playback_rate)
    {
        TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
        uint32_t* anim_id = texture_set->m_AnimationIds.Get(animation);
        if (anim_id)
        {
            component->m_AnimationID = *anim_id;
            component->m_CurrentAnimation = animation;
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_TextureSet->m_Animations[*anim_id];
            uint32_t frame_count = animation->m_End - animation->m_Start;
            if (animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG
                    || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
                frame_count = dmMath::Max(1u, frame_count * 2 - 2);
            component->m_AnimInvDuration = (float)animation->m_Fps / frame_count;
            component->m_AnimPingPong = animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG;
            component->m_AnimBackwards = animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD;
            component->m_Playing = animation->m_Playback != dmGameSystemDDF::PLAYBACK_NONE;

            if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_AUTO)
            {
                component->m_Size = GetSizeFromAnimation(component, texture_set->m_TextureSet, component->m_AnimationID);
            }

            offset = dmMath::Clamp(offset, 0.0f, 1.0f);
            if (animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD) {
                offset = 1.0f - offset;
            }

            component->m_PlaybackRate = dmMath::Max(playback_rate, 0.0f);
            SetCursor(component, offset);
            UpdateCurrentAnimationFrame(component);
        }
        else
        {
            // TODO: Why stop the current animation? Shouldn't it continue playing the current animation?
            component->m_Playing = 0;
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
        dmRender::HMaterial material = GetMaterial(component, resource);
        TextureSetResource* texture_set = GetTextureSet(component, resource);
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &material, sizeof(material));
        dmHashUpdateBuffer32(&state, &texture_set, sizeof(texture_set));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        if (component->m_RenderConstants) {
            dmGameSystem::HashRenderConstants(component->m_RenderConstants, &state);
        }
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    dmGameObject::CreateResult CompSpriteCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        if (sprite_world->m_Components.Full())
        {
            dmLogError("Sprite could not be created since the sprite buffer is full (%d). See 'sprite.max_count' in game.project", sprite_world->m_Components.Capacity());
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
        component->m_RenderConstants = 0;
        dmMessage::ResetURL(&component->m_Listener);
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_FunctionRef = 0;
        component->m_ReHash = 1;
        component->m_UseSlice9 = sum(component->m_Resource->m_DDF->m_Slice9) != 0 &&
                component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_MANUAL;

        component->m_Size = Vector3(0.0f, 0.0f, 0.0f);
        component->m_AnimationID = 0;

        if (component->m_Resource->m_DDF->m_SizeMode == dmGameSystemDDF::SpriteDesc::SIZE_MODE_MANUAL)
        {
            component->m_Size[0] = component->m_Resource->m_DDF->m_Size.getX();
            component->m_Size[1] = component->m_Resource->m_DDF->m_Size.getY();
        }

        PlayAnimation(component, resource->m_DefaultAnimation, 0.0f, 1.0f);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);
        if (component->m_Material) {
            dmResource::Release(factory, component->m_Material);
        }
        if (component->m_TextureSet) {
            dmResource::Release(factory, component->m_TextureSet);
        }
        if (component->m_RenderConstants)
        {
            dmGameSystem::DestroyRenderConstants(component->m_RenderConstants);
        }
        sprite_world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static void CreateVertexDataSlice9(SpriteVertex* vertices, uint8_t* indices, bool is_indices_16_bit,
        const Matrix4& transform, Vector3 sprite_size, Vector4 slice9, uint32_t vertex_offset,
        const float* tc, float texture_width, float texture_height, bool flip_u, bool flip_v)
    {
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
        float us[4], vs[4], xs[4], ys[4];

        // v are '1-v'
        xs[0] = ys[0] = 0;
        xs[3] = ys[3] = 1;

        // disable slice9 computation below a certain dimension
        // (avoid div by zero)
        const float s9_min_dim = 0.001f;
        const float su         = 1.0f / texture_width;
        const float sv         = 1.0f / texture_height;
        const float sx         = sprite_size.getX() > s9_min_dim ? 1.0f / sprite_size.getX() : 0;
        const float sy         = sprite_size.getY() > s9_min_dim ? 1.0f / sprite_size.getY() : 0;

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

        xs[1] = sx * slice9.getX();
        xs[2] = 1 - sx * slice9.getZ();
        ys[1] = sy * slice9.getW();
        ys[2] = 1 - sy * slice9.getY();

        for (int y=0;y<4;y++)
        {
            for (int x=0;x<4;x++)
            {
                Vector4 p   = transform * Point3(xs[x] - 0.5, ys[y] - 0.5, 0);
                vertices->x = p.getX();
                vertices->y = p.getY();
                vertices->z = p.getZ();
                vertices->u = us[x];
                vertices->v = vs[y];
                vertices++;
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


    static void CreateVertexData(SpriteWorld* sprite_world, SpriteVertex** vb_where, uint8_t** ib_where, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("CreateVertexData");

        SpriteVertex*   vertices = *vb_where;
        uint8_t*        indices  = *ib_where;
        uint32_t index_type_size = sprite_world->m_Is16BitIndex ? sizeof(uint16_t) : sizeof(uint32_t);

        const dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;

        // The offset for the indices
        uint32_t vertex_offset = *vb_where - sprite_world->m_VertexBufferData;

        static int tex_coord_order[] = {
            0,1,2,2,3,0,
            3,2,1,1,0,3,    //h
            1,0,3,3,2,1,    //v
            2,3,0,0,1,2     //hv
        };

        for (uint32_t* i = begin; i != end; ++i)
        {
            uint32_t component_index                            = (uint32_t)buf[*i].m_UserData;
            const SpriteComponent* component                    = (const SpriteComponent*) &components[component_index];
            TextureSetResource* texture_set                     = GetTextureSet(component, component->m_Resource);
            dmGameSystemDDF::TextureSet* texture_set_ddf        = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animations    = texture_set_ddf->m_Animations.m_Data;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &animations[component->m_AnimationID];

            if (texture_set_ddf->m_UseGeometries != 0)
            {
                const dmGameSystemDDF::SpriteGeometry* geometries = texture_set_ddf->m_Geometries.m_Data;
                uint32_t* frame_indices                           = texture_set_ddf->m_FrameIndices.m_Data;
                uint32_t frame_index                              = frame_indices[animation_ddf->m_Start + component->m_CurrentAnimationFrame];

                // Depending on the sprite is flipped or not, we loop the vertices forward or backward
                // to respect face winding (and backface culling)
                int flipx = animation_ddf->m_FlipHorizontal ^ component->m_FlipHorizontal;
                int flipy = animation_ddf->m_FlipVertical ^ component->m_FlipVertical;
                int reverse = flipx ^ flipy;

                const Matrix4& w = component->m_World;
                const dmGameSystemDDF::SpriteGeometry* geometry = &geometries[frame_index];
                uint32_t num_points = geometry->m_Vertices.m_Count / 2;
                const float* points = geometry->m_Vertices.m_Data;
                const float* uvs    = geometry->m_Uvs.m_Data;

                float scaleX = flipx ? -1 : 1;
                float scaleY = flipy ? -1 : 1;

                int step = reverse ? -2 : 2;
                points = reverse ? points + num_points*2 - 2 : points;
                uvs = reverse ? uvs + num_points*2 - 2 : uvs;

                for (uint32_t vert = 0; vert < num_points; ++vert, ++vertices, points += step, uvs += step)
                {
                    float x = points[0] * scaleX; // range -0.5,+0.5
                    float y = points[1] * scaleY;
                    float u = uvs[0];
                    float v = uvs[1];

                    Vector4 p0 = w * Point3(x, y, 0.0f);
                    vertices[0].x = ((float*)&p0)[0];
                    vertices[0].y = ((float*)&p0)[1];
                    vertices[0].z = ((float*)&p0)[2];
                    vertices[0].u = u;
                    vertices[0].v = v;
                }

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
                vertex_offset += num_points;
            }
            else
            {
                uint32_t frame_index    = animation_ddf->m_Start + component->m_CurrentAnimationFrame;
                const float* tex_coords = (const float*) texture_set_ddf->m_TexCoords.m_Data;
                const float* tc         = &tex_coords[frame_index * 4 * 2];
                uint32_t flip_flag      = 0;

                // ddf values are guaranteed to be 0 or 1 when saved by the editor
                // component values are guaranteed to be 0 or 1
                if (animation_ddf->m_FlipHorizontal ^ component->m_FlipHorizontal)
                {
                    flip_flag = 1;
                }
                if (animation_ddf->m_FlipVertical ^ component->m_FlipVertical)
                {
                    flip_flag |= 2;
                }

                const int* tex_lookup = &tex_coord_order[flip_flag * 6];
                const Matrix4& w      = component->m_World;

                // Output vertices in either a single quad format or slice-9 format
                // ==================================================================
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
                    int flipx = animation_ddf->m_FlipHorizontal ^ component->m_FlipHorizontal;
                    int flipy = animation_ddf->m_FlipVertical ^ component->m_FlipVertical;
                    CreateVertexDataSlice9(vertices, indices, sprite_world->m_Is16BitIndex,
                        w, component->m_Size, component->m_Resource->m_DDF->m_Slice9, vertex_offset, tc,
                        dmGraphics::GetTextureWidth(texture_set->m_Texture),
                        dmGraphics::GetTextureHeight(texture_set->m_Texture),
                        flipx, flipy);

                    indices       += index_type_size * SPRITE_INDEX_COUNT_SLICE9;
                    vertices      += SPRITE_VERTEX_COUNT_SLICE9;
                    vertex_offset += SPRITE_VERTEX_COUNT_SLICE9;
                }
                else
                {
                    #define SET_SPRITE_VERTEX(vert, p, tc_index)   \
                        vert.x = p.getX();                         \
                        vert.y = p.getY();                         \
                        vert.z = p.getZ();                         \
                        vert.u = tc[tex_lookup[tc_index] * 2 + 0]; \
                        vert.v = tc[tex_lookup[tc_index] * 2 + 1];

                    Vector4 p0 = w * Point3(-0.5f, -0.5f, 0.0f);
                    Vector4 p1 = w * Point3(-0.5f, 0.5f, 0.0f);
                    Vector4 p2 = w * Point3(0.5f, 0.5f, 0.0f);
                    Vector4 p3 = w * Point3(0.5f, -0.5f, 0.0f);

                    SET_SPRITE_VERTEX(vertices[0], p0, 0);
                    SET_SPRITE_VERTEX(vertices[1], p1, 1);
                    SET_SPRITE_VERTEX(vertices[2], p2, 2);
                    SET_SPRITE_VERTEX(vertices[3], p3, 4);

                    #undef SET_SPRITE_VERTEX

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

                    vertices      += SPRITE_VERTEX_COUNT_LEGACY;
                    vertex_offset += SPRITE_VERTEX_COUNT_LEGACY;
                    indices       += SPRITE_INDEX_COUNT_LEGACY * index_type_size;
                }
            }
        }

        *vb_where = vertices;
        *ib_where = indices;
    }

    static void RenderBatch(SpriteWorld* sprite_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE("SpriteRenderBatch");

        uint32_t component_index = (uint32_t)buf[*begin].m_UserData;
        const SpriteComponent* first = (const SpriteComponent*) &sprite_world->m_Components.m_Objects[component_index];
        assert(first->m_Enabled);

        SpriteResource* resource = first->m_Resource;
        TextureSetResource* texture_set = GetTextureSet(first, resource);

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

        // Fill in vertex buffer
        SpriteVertex* vb_begin = sprite_world->m_VertexBufferWritePtr;
        uint8_t* ib_begin = (uint8_t*)sprite_world->m_IndexBufferWritePtr;
        SpriteVertex* vb_iter = vb_begin;
        uint8_t* ib_iter = ib_begin;
        CreateVertexData(sprite_world, &vb_iter, &ib_iter, buf, begin, end);

        sprite_world->m_VertexBufferWritePtr = vb_iter;
        sprite_world->m_IndexBufferWritePtr = ib_iter;

        ro.Init();
        ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
        ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
        ro.m_IndexBuffer = sprite_world->m_IndexBuffer;
        ro.m_Material = GetMaterial(first, resource);
        ro.m_Textures[0] = texture_set->m_Texture;
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

        if (first->m_RenderConstants) {
            dmGameSystem::EnableRenderObjectConstants(&ro, first->m_RenderConstants);
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

    static void UpdateTransforms(SpriteWorld* sprite_world, bool sub_pixels)
    {
        DM_PROFILE("UpdateTransforms");

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t n = components.Size();

        bool scale_along_z = false;
        if (n > 0) {
            SpriteComponent* c = &components[0];
            scale_along_z = dmGameObject::ScaleAlongZ(dmGameObject::GetCollection(c->m_Instance));
        }

        // Note: We update all sprites, even though they might be disabled, or not added to update

        if (scale_along_z) {
            for (uint32_t i = 0; i < n; ++i)
            {
                SpriteComponent* c = &components[i];
                Matrix4 local = dmTransform::ToMatrix4(dmTransform::Transform(c->m_Position, c->m_Rotation, 1.0f));
                Matrix4 world = dmGameObject::GetWorldMatrix(c->m_Instance);
                Vector3 size( c->m_Size.getX() * c->m_Scale.getX(), c->m_Size.getY() * c->m_Scale.getY(), 1);
                c->m_World = appendScale(world * local, size);
            }
        } else
        {
            for (uint32_t i = 0; i < n; ++i)
            {
                SpriteComponent* c = &components[i];
                Matrix4 local = dmTransform::ToMatrix4(dmTransform::Transform(c->m_Position, c->m_Rotation, 1.0f));
                Matrix4 world = dmGameObject::GetWorldMatrix(c->m_Instance);
                Matrix4 w = dmTransform::MulNoScaleZ(world, local);
                Vector3 size( c->m_Size.getX() * c->m_Scale.getX(), c->m_Size.getY() * c->m_Scale.getY(), 1);
                c->m_World = appendScale(w, size);
            }
        }

        // The "sub_pixels" is set by default
        if (!sub_pixels) {
            for (uint32_t i = 0; i < n; ++i) {
                SpriteComponent* c = &components[i];
                Vector4 position = c->m_World.getCol3();
                position.setX((int) position.getX());
                position.setY((int) position.getY());
                c->m_World.setCol3(position);
            }
        }
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

    static void PostMessages(SpriteWorld* sprite_world)
    {
        DM_PROFILE("PostMessages");

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!component->m_Enabled || !component->m_Playing)
                continue;

            TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_AnimationID];

            bool once = animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_FORWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG;
            // Stop once-animation and broadcast animation_done
            if (once && component->m_AnimTimer >= 1.0f)
            {
                component->m_Playing = 0;
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

                    dmGameObject::Result go_result = dmGameObject::PostDDF(&message, &sender, &component->m_Listener, component->m_FunctionRef, false);
                    dmMessage::ResetURL(&component->m_Listener);
                    if (go_result != dmGameObject::RESULT_OK)
                    {
                        dmLogError("Could not send animation_done to listener. Has it been deleted?");
                    }
                }
            }
        }
    }


    static void Animate(SpriteWorld* sprite_world, float dt)
    {
        DM_PROFILE("Animate");

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!component->m_Enabled)
                continue;

            if (component->m_Playing && component->m_AddedToUpdate)
            {
                TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
                dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
                dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_AnimationID];

                // Animate
                component->m_AnimTimer += dt * component->m_AnimInvDuration * component->m_PlaybackRate;
                if (component->m_AnimTimer >= 1.0f)
                {
                    switch (animation_ddf->m_Playback)
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

            if (component->m_DoTick) {
                component->m_DoTick = 0;
                UpdateCurrentAnimationFrame(component);
            }
        }
    }

    static void CalcBoundingVolumes(SpriteWorld* sprite_world)
    {
        DM_PROFILE("CalcBoundingVolumes");

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t n = components.Size();

        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];

            float sx = Vectormath::Aos::length(component->m_World.getCol(0).getXYZ());
            float sy = Vectormath::Aos::length(component->m_World.getCol(1).getXYZ());
            float radius = dmMath::Max(sx, sy) * 0.5f;

            sprite_world->m_BoundingVolumes[i] = radius;
        }
    }

    static void UpdateVertexAndIndexCount(SpriteWorld* sprite_world)
    {
        DM_PROFILE("UpdateVertexAndIndexCount");

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t n               = components.Size();
        uint32_t num_vertices    = 0;
        uint32_t num_indices     = 0;

        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            if (!component->m_Enabled || !component->m_AddedToUpdate)
                continue;

            TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
            if (texture_set->m_TextureSet->m_UseGeometries != 0)
            {
                const dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
                const dmGameSystemDDF::SpriteGeometry* geometries  = texture_set_ddf->m_Geometries.m_Data;
                num_vertices += geometries->m_Vertices.m_Count / 2; // (x,y) coordinates
                num_indices  += geometries->m_Indices.m_Count;
            }
            else
            {
                if (component->m_UseSlice9)
                {
                    num_vertices += SPRITE_VERTEX_COUNT_SLICE9;
                    num_indices  += SPRITE_INDEX_COUNT_SLICE9;
                }
                else
                {
                    num_vertices += SPRITE_VERTEX_COUNT_LEGACY;
                    num_indices  += SPRITE_INDEX_COUNT_LEGACY;
                }
            }
        }

        sprite_world->m_ReallocBuffers = num_vertices > sprite_world->m_VertexCount || num_indices > sprite_world->m_IndexCount;
        sprite_world->m_VertexCount    = num_vertices;
        sprite_world->m_IndexCount     = num_indices;
    }

    dmGameObject::CreateResult CompSpriteAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        /*
         * NOTES:
         * * When/if transparency the batching predicates must be updated in order to
         *   support per sprite correct sorting.
         */

        SpriteWorld* world = (SpriteWorld*)params.m_World;
        Animate(world, params.m_UpdateContext->m_DT);

        PostMessages(world);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListFrustumCulling(dmRender::RenderListVisibilityParams const &params)
    {
        DM_PROFILE("Sprite");

        const SpriteWorld* sprite_world = (SpriteWorld*)params.m_UserData;
        const float* radiuses = sprite_world->m_BoundingVolumes.Begin();

        const dmIntersection::Frustum frustum = *params.m_Frustum;
        uint32_t num_entries = params.m_NumEntries;
        for (uint32_t i = 0; i < num_entries; ++i)
        {
            dmRender::RenderListEntry* entry = &params.m_Entries[i];

            float radius = radiuses[entry->m_UserData];

            bool intersect = dmIntersection::TestFrustumSphere(frustum, entry->m_WorldPosition, radius, true);
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
                    uint32_t vertex_count = world->m_VertexBufferWritePtr - world->m_VertexBufferData;
                    uint32_t vertex_size = sizeof(SpriteVertex) * vertex_count;
                    if (vertex_size)
                    {
                        dmGraphics::SetVertexBufferData(world->m_VertexBuffer, vertex_size,
                                                        world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

                        DM_PROPERTY_ADD_U32(rmtp_SpriteVertexCount, vertex_count);
                        DM_PROPERTY_ADD_U32(rmtp_SpriteVertexSize, vertex_size);

                    }
                    uint32_t index_size = (world->m_IndexBufferWritePtr - world->m_IndexBufferData);
                    if (index_size)
                    {
                        dmGraphics::SetIndexBufferData(world->m_IndexBuffer, index_size, world->m_IndexBufferData, dmGraphics::BUFFER_USAGE_DYNAMIC_DRAW);

                        DM_PROPERTY_ADD_U32(rmtp_SpriteIndexSize, index_size);
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

        UpdateTransforms(sprite_world, sprite_context->m_Subpixels); // TODO: Why is this not in the update function?

        UpdateVertexAndIndexCount(sprite_world);

        CalcBoundingVolumes(sprite_world); // uses the m_World to calculate the actual radius of the object

        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
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

            if (component.m_ReHash || (component.m_RenderConstants && dmGameSystem::AreRenderConstantsUpdated(component.m_RenderConstants)))
            {
                ReHash(&component);
            }

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = i;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(GetMaterial(&component, component.m_Resource));
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
        if (!component->m_RenderConstants)
            return false;
        return GetRenderConstant(component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompSpriteSetConstantCallback(void* user_data, dmhash_t name_hash, int32_t value_index, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;
        if (!component->m_RenderConstants)
            component->m_RenderConstants = dmGameSystem::CreateRenderConstants();
        dmGameSystem::SetRenderConstant(component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, value_index, element_index, var);
        component->m_ReHash = 1;
    }

    static void SetCursor(SpriteComponent* component, float cursor)
    {
        cursor = dmMath::Clamp(cursor, 0.0f, 1.0f);
        if (component->m_AnimPingPong) {
            cursor /= 2.0f;
        }

        if (component->m_AnimBackwards) {
            cursor = 1.0f - cursor;
        }

        component->m_AnimTimer = cursor;
        component->m_DoTick = 1;
    }

    static float GetCursor(SpriteComponent* component)
    {
        float cursor = component->m_AnimTimer;

        if (component->m_AnimBackwards)
            cursor = 1.0f - cursor;

        if (component->m_AnimPingPong) {
            cursor *= 2.0f;
            if (cursor > 1.0f) {
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

    dmGameObject::UpdateResult CompSpriteOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
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
            if (params.m_Message->m_Id == dmGameSystemDDF::PlayAnimation::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::PlayAnimation* ddf = (dmGameSystemDDF::PlayAnimation*)params.m_Message->m_Data;
                if (PlayAnimation(component, ddf->m_Id, ddf->m_Offset, ddf->m_PlaybackRate))
                {
                    component->m_Listener = params.m_Message->m_Sender;
                    component->m_FunctionRef = params.m_Message->m_UserData2;
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetFlipHorizontal::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetFlipHorizontal* ddf = (dmGameSystemDDF::SetFlipHorizontal*)params.m_Message->m_Data;
                component->m_FlipHorizontal = ddf->m_Flip != 0 ? 1 : 0;
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetFlipVertical::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetFlipVertical* ddf = (dmGameSystemDDF::SetFlipVertical*)params.m_Message->m_Data;
                component->m_FlipVertical = ddf->m_Flip != 0 ? 1 : 0;
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::SetConstant* ddf = (dmGameSystemDDF::SetConstant*)params.m_Message->m_Data;
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(GetMaterial(component, component->m_Resource), ddf->m_NameHash,
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
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                if (component->m_RenderConstants && dmGameSystem::ClearRenderConstant(component->m_RenderConstants, ddf->m_NameHash))
                {
                    component->m_ReHash = 1;
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::SetScale::m_DDFDescriptor->m_NameHash)
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
        if (component->m_Playing)
            PlayAnimation(component, component->m_CurrentAnimation, component->m_AnimTimer, component->m_PlaybackRate);
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
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterial(component, component->m_Resource), out_value);
        }
        else if (get_property == PROP_IMAGE)
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTextureSet(component, component->m_Resource), out_value);
        }
        else if (get_property == PROP_TEXTURE[0])
        {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetTextureSet(component, component->m_Resource)->m_Texture, out_value);
        }
        else if (get_property == SPRITE_PROP_ANIMATION)
        {
            out_value.m_Variant = dmGameObject::PropertyVar(component->m_CurrentAnimation);
            return dmGameObject::PROPERTY_RESULT_OK;
        }
        return GetMaterialConstant(GetMaterial(component, component->m_Resource), get_property, params.m_Options.m_Index, out_value, false, CompSpriteGetConstantCallback, component);
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
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&component->m_Material);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            return res;
        }
        else if (set_property == PROP_IMAGE)
        {
            dmGameObject::PropertyResult res = SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, TEXTURE_SET_EXT_HASH, (void**)&component->m_TextureSet);
            component->m_ReHash |= res == dmGameObject::PROPERTY_RESULT_OK;
            // Since the animation referred to the old texture, we need to update it
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
                uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
                if (anim_id)
                {
                    PlayAnimation(component, component->m_CurrentAnimation, GetCursor(component), component->m_PlaybackRate);
                }
                else
                {
                    component->m_Playing = 0;
                    component->m_CurrentAnimation = 0x0;
                    component->m_CurrentAnimationFrame = 0;
                }
            }
            return res;
        }
        return SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, params.m_Options.m_Index, CompSpriteSetConstantCallback, component);
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
                case 0: value = Vector4(transform.GetTranslation()); break;
                case 1: value = Vector4(transform.GetRotation()); type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4; break;
                case 2:
                    {
                        // Since the size is baked into the matrix, we divide by it here
                        Vector3 size( component->m_Size.getX() * component->m_Scale.getX(), component->m_Size.getY() * component->m_Scale.getY(), 1);
                        value = Vector4(Vectormath::Aos::divPerElem(transform.GetScale(), size));
                    }
                    break;
                case 3: value = Vector4(transform.GetScale()); break; // the size is baked into this matrix as the scale
                default:
                    return false;
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
}
