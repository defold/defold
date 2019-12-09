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
#include <graphics/graphics.h>
#include <render/render.h>
#include <gameobject/gameobject_ddf.h>

#include "../resources/res_sprite.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include "comp_private.h"

#include "sprite_ddf.h"
#include "gamesys_ddf.h"

using namespace Vectormath::Aos;
namespace dmGameSystem
{
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
        dmMessage::URL              m_Listener;
        uint32_t                    m_AnimationID;
        SpriteResource*             m_Resource;
        CompRenderConstants         m_RenderConstants;
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
        uint16_t                    m_Padding : 7;
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
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        SpriteVertex*                   m_VertexBufferData;
        SpriteVertex*                   m_VertexBufferWritePtr;
        dmGraphics::HIndexBuffer        m_IndexBuffer;
        uint8_t*                        m_IndexBufferData;
        uint8_t*                        m_IndexBufferWritePtr;
        uint8_t                         m_Is16BitIndex : 1;
        uint8_t                         m_UsesGeometries : 1;
    };

    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SIZE, size, true);

    static const dmhash_t SPRITE_PROP_CURSOR = dmHashString64("cursor");
    static const dmhash_t SPRITE_PROP_PLAYBACK_RATE = dmHashString64("playback_rate");

    static float GetCursor(SpriteComponent* component);
    static void SetCursor(SpriteComponent* component, float cursor);
    static float GetPlaybackRate(SpriteComponent* component);
    static void SetPlaybackRate(SpriteComponent* component, float playback_rate);

    template<typename T>
    void fillIndices(T* index, uint32_t indices_count) {
        for(uint32_t i = 0, v = 0; i < indices_count; i += 6, v += 4)
        {
            *index++ = v+0;
            *index++ = v+1;
            *index++ = v+2;
            *index++ = v+2;
            *index++ = v+3;
            *index++ = v+0;
        }
    }

    static void AllocateBuffers(SpriteWorld* sprite_world, dmRender::HRenderContext render_context, uint32_t max_sprite_count, uint32_t num_vertices_per_sprite, uint32_t num_indices_per_sprite) {
        if (sprite_world->m_VertexBuffer == 0)
            sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        {
            uint32_t memsize = sizeof(SpriteVertex) * num_vertices_per_sprite * max_sprite_count;
            sprite_world->m_VertexBufferData = (SpriteVertex*) malloc(memsize);
        }

        {
            uint32_t vertex_count = num_vertices_per_sprite * max_sprite_count;
            uint32_t size_type = vertex_count <= 65536 ? sizeof(uint16_t) : sizeof(uint32_t);
            sprite_world->m_Is16BitIndex = size_type == sizeof(uint16_t) ? 1 : 0;

            uint32_t indices_count = num_indices_per_sprite * max_sprite_count;
            size_t indices_size = indices_count * size_type;

            sprite_world->m_IndexBufferData = (uint8_t*)realloc(sprite_world->m_IndexBufferData, indices_size);

            if (!sprite_world->m_UsesGeometries) {
                if (sprite_world->m_Is16BitIndex) {
                    fillIndices<uint16_t>((uint16_t*)sprite_world->m_IndexBufferData, indices_count);
                } else {
                    fillIndices<uint32_t>((uint32_t*)sprite_world->m_IndexBufferData, indices_count);
                }
            }

            if (sprite_world->m_IndexBuffer) {
                dmGraphics::DeleteIndexBuffer(sprite_world->m_IndexBuffer);
            }

            sprite_world->m_IndexBuffer = dmGraphics::NewIndexBuffer(dmRender::GetGraphicsContext(render_context), indices_size, (void*)sprite_world->m_IndexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
        }
    }

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;
        SpriteWorld* sprite_world = new SpriteWorld();

        sprite_world->m_Components.SetCapacity(sprite_context->m_MaxSpriteCount);
        memset(sprite_world->m_Components.m_Objects.Begin(), 0, sizeof(SpriteComponent) * sprite_context->m_MaxSpriteCount);
        sprite_world->m_RenderObjects.SetCapacity(sprite_context->m_MaxSpriteCount);

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

        sprite_world->m_UsesGeometries = 0;

        // TODO: Create new settings for vertex buffer size
        // TODO: Should we remove the index buffer altogether?
        uint32_t num_vertices_per_sprite = 8; // Current max value from the editor
        uint32_t num_indices_per_sprite = (num_vertices_per_sprite - 2) * 3;
        AllocateBuffers(sprite_world, render_context, sprite_context->m_MaxSpriteCount, num_vertices_per_sprite, num_indices_per_sprite);

        *params.m_World = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(sprite_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);
        free(sprite_world->m_VertexBufferData);
        dmGraphics::DeleteIndexBuffer(sprite_world->m_IndexBuffer);
        free(sprite_world->m_IndexBufferData);

        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static inline Vector3 GetSize(const SpriteComponent* sprite, dmGameSystemDDF::TextureSet* texture_set_ddf, uint32_t anim_id)
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

        if (frame != frame_current)
        {
            component->m_Size = GetSize(component, texture_set_ddf, component->m_AnimationID);
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
            component->m_Size = GetSize(component, texture_set->m_TextureSet, component->m_AnimationID);

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
        ReHashRenderConstants(&component->m_RenderConstants, &state);
        component->m_MixedHash = dmHashFinal32(&state);
        component->m_ReHash = 0;
    }

    dmGameObject::CreateResult CompSpriteCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        if (sprite_world->m_Components.Full())
        {
            dmLogError("Sprite could not be created since the sprite buffer is full (%d).", sprite_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = sprite_world->m_Components.Alloc();
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        memset(component, 0, sizeof(SpriteComponent));
        component->m_Instance = params.m_Instance;
        component->m_Position = Vector3(params.m_Position);
        component->m_Rotation = params.m_Rotation;
        SpriteResource* resource = (SpriteResource*)params.m_Resource;
        component->m_Resource = resource;
        dmMessage::ResetURL(component->m_Listener);
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_Scale = Vector3(1.0f);

        component->m_ReHash = 1;

        component->m_Size = Vector3(0.0f, 0.0f, 0.0f);
        component->m_AnimationID = 0;
        PlayAnimation(component, resource->m_DefaultAnimation, 0.0f, 1.0f);

        TextureSetResource* texture_set = GetTextureSet(component, resource);
        sprite_world->m_UsesGeometries |= texture_set->m_TextureSet->m_Geometries.m_Count != 0 ? 1 : 0;

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
        sprite_world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }


    static void CreateVertexData(SpriteWorld* sprite_world, SpriteVertex** vb_where, uint8_t** ib_where, TextureSetResource* texture_set, dmRender::RenderListEntry* buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Sprite, "CreateVertexData");
        static int tex_coord_order[] = {
            0,1,2,2,3,0,
            3,2,1,1,0,3,    //h
            1,0,3,3,2,1,    //v
            2,3,0,0,1,2     //hv
        };

        uint16_t texture_width = dmGraphics::GetOriginalTextureWidth(texture_set->m_Texture);
        uint16_t texture_height = dmGraphics::GetOriginalTextureHeight(texture_set->m_Texture);

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        dmGameSystemDDF::TextureSetAnimation* animations = texture_set_ddf->m_Animations.m_Data;
        uint32_t* frame_indices = texture_set_ddf->m_FrameIndices.m_Data;

        SpriteVertex*   vertices = *vb_where;
        uint8_t*        indices = *ib_where;

        uint32_t index_type_size = sprite_world->m_Is16BitIndex ? sizeof(uint16_t) : sizeof(uint32_t);

        if (sprite_world->m_UsesGeometries)
        {
            const dmGameSystemDDF::SpriteGeometry* geometries = texture_set_ddf->m_Geometries.m_Data;

            // The offset for the indices
            uint32_t vertex_offset = *vb_where - sprite_world->m_VertexBufferData;

            for (uint32_t* i = begin; i != end; ++i)
            {
                const SpriteComponent* component = (SpriteComponent*) buf[*i].m_UserData;

                const dmGameSystemDDF::TextureSetAnimation* animation_ddf = &animations[component->m_AnimationID];

                uint32_t frame_index = frame_indices[animation_ddf->m_Start + component->m_CurrentAnimationFrame];

                const dmGameSystemDDF::SpriteGeometry* geometry = &geometries[frame_index];

                const Matrix4& w = component->m_World;

                uint32_t num_points = geometry->m_Vertices.m_Count / 2;

                const float* points = geometry->m_Vertices.m_Data;
                const float* uvs = geometry->m_Uvs.m_Data;

                // Depending on the sprite is flipped or not, we loop the vertices forward or backward
                // to respect face winding (and backface culling)
                int flipx = animation_ddf->m_FlipHorizontal ^ component->m_FlipHorizontal;
                int flipy = animation_ddf->m_FlipVertical ^ component->m_FlipVertical;
                int reverse = flipx ^ flipy;

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
                indices += index_type_size * geometry->m_Indices.m_Count;
                vertex_offset += num_points;
            }
        }
        else // original path using quads
        {
            static int tex_coord_order[] = {
                0,1,2,2,3,0,
                3,2,1,1,0,3,    //h
                1,0,3,3,2,1,    //v
                2,3,0,0,1,2     //hv
            };

            const float* tex_coords = (const float*) texture_set->m_TextureSet->m_TexCoords.m_Data;

            for (uint32_t *i = begin;i != end; ++i)
            {
                const SpriteComponent* component = (SpriteComponent*) buf[*i].m_UserData;

                dmGameSystemDDF::TextureSetAnimation* animation_ddf = &animations[component->m_AnimationID];

                uint32_t frame_index = animation_ddf->m_Start + component->m_CurrentAnimationFrame;
                const float* tc = &tex_coords[frame_index * 4 * 2];
                uint32_t flip_flag = 0;

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

                const Matrix4& w = component->m_World;

                Vector4 p0 = w * Point3(-0.5f, -0.5f, 0.0f);
                vertices[0].x = p0.getX();
                vertices[0].y = p0.getY();
                vertices[0].z = p0.getZ();
                vertices[0].u = tc[tex_lookup[0] * 2];
                vertices[0].v = tc[tex_lookup[0] * 2 + 1];

                Vector4 p1 = w * Point3(-0.5f, 0.5f, 0.0f);
                vertices[1].x = p1.getX();
                vertices[1].y = p1.getY();
                vertices[1].z = p1.getZ();
                vertices[1].u = tc[tex_lookup[1] * 2];
                vertices[1].v = tc[tex_lookup[1] * 2 + 1];

                Vector4 p2 = w * Point3(0.5f, 0.5f, 0.0f);
                vertices[2].x = p2.getX();
                vertices[2].y = p2.getY();
                vertices[2].z = p2.getZ();
                vertices[2].u = tc[tex_lookup[2] * 2];
                vertices[2].v = tc[tex_lookup[2] * 2 + 1];

                Vector4 p3 = w * Point3(0.5f, -0.5f, 0.0f);
                vertices[3].x = p3.getX();
                vertices[3].y = p3.getY();
                vertices[3].z = p3.getZ();
                vertices[3].u = tc[tex_lookup[4] * 2];
                vertices[3].v = tc[tex_lookup[4] * 2 + 1];

                // for (int f = 0; f < 4; ++f)
                //     printf("  %u: %.2f, %.2f\t%.2f, %.2f\n", f, vertices[f].x, vertices[f].y, vertices[f].u, vertices[f].v );

                vertices += 4;
                indices += 6 * index_type_size;
            }
        }

        *vb_where = vertices;
        *ib_where = indices;
    }

    static void RenderBatch(SpriteWorld* sprite_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Sprite, "RenderBatch");

        const SpriteComponent* first = (SpriteComponent*) buf[*begin].m_UserData;
        assert(first->m_Enabled);

        SpriteResource* resource = first->m_Resource;
        TextureSetResource* texture_set = GetTextureSet(first, resource);

        // Ninja in-place writing of render object.
        dmRender::RenderObject& ro = *sprite_world->m_RenderObjects.End();
        sprite_world->m_RenderObjects.SetSize(sprite_world->m_RenderObjects.Size()+1);

        // Fill in vertex buffer
        SpriteVertex* vb_begin = sprite_world->m_VertexBufferWritePtr;
        uint8_t* ib_begin = (uint8_t*)sprite_world->m_IndexBufferWritePtr;
        SpriteVertex* vb_iter = vb_begin;
        uint8_t* ib_iter = ib_begin;
        CreateVertexData(sprite_world, &vb_iter, &ib_iter, texture_set, buf, begin, end);

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

        // // These should be named "element" or "index" (as opposed to vertex)
        ro.m_VertexStart = index_offset;
        ro.m_VertexCount = num_elements;

        const dmRender::Constant* constants = first->m_RenderConstants.m_RenderConstants;
        uint32_t size = first->m_RenderConstants.m_ConstantCount;
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
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
        DM_PROFILE(Sprite, "UpdateTransforms");

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
        DM_PROFILE(Sprite, "PostMessages");

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
                        dmLogError("Could not send animation_done to listener.");
                        return;
                    }

                    dmhash_t message_id = dmGameSystemDDF::AnimationDone::m_DDFDescriptor->m_NameHash;
                    dmGameSystemDDF::AnimationDone message;
                    // Engine has 0-based indices, scripts use 1-based
                    message.m_CurrentTile = component->m_CurrentAnimationFrame + 1;
                    message.m_Id = component->m_CurrentAnimation;

                    dmGameObject::HInstance listener_instance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), component->m_Listener.m_Path);
                    if (!listener_instance)
                    {
                        dmLogError("Could not send animation_done to instance: %s#%s", dmHashReverseSafe64(component->m_Listener.m_Path), dmHashReverseSafe64(component->m_Listener.m_Fragment));
                        return;
                    }

                    dmMessage::URL receiver = component->m_Listener;
                    sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
                    if (dmMessage::IsSocketValid(receiver.m_Socket) && dmMessage::IsSocketValid(sender.m_Socket))
                    {
                        dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
                        if (go_result == dmGameObject::RESULT_OK)
                        {
                            sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
                            uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::AnimationDone::m_DDFDescriptor;
                            uint32_t data_size = sizeof(dmGameSystemDDF::AnimationDone);
                            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size, 0);
                            dmMessage::ResetURL(component->m_Listener);
                            if (result != dmMessage::RESULT_OK)
                            {
                                dmLogError("Could not send animation_done to listener.");
                            }
                        }
                        else
                        {
                            dmLogError("Could not send animation_done to listener because of incomplete component.");
                        }
                    }
                    else
                    {
                        dmMessage::ResetURL(component->m_Listener);
                    }
                }
            }
        }
    }


    static void Animate(SpriteWorld* sprite_world, float dt)
    {
        DM_PROFILE(Sprite, "Animate");

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

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        SpriteWorld* world = (SpriteWorld*) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                world->m_VertexBufferWritePtr = world->m_VertexBufferData;
                world->m_IndexBufferWritePtr = world->m_IndexBufferData;
                world->m_RenderObjects.SetSize(0);
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(SpriteVertex) * (world->m_VertexBufferWritePtr - world->m_VertexBufferData),
                                                world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

                DM_COUNTER("SpriteVertexBuffer", (world->m_VertexBufferWritePtr - world->m_VertexBufferData) * sizeof(SpriteVertex));

                if (world->m_UsesGeometries)
                {
                    uint32_t index_size = (world->m_IndexBufferWritePtr - world->m_IndexBufferData);
                    dmGraphics::SetIndexBufferData(world->m_IndexBuffer, index_size, world->m_IndexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                    DM_COUNTER("SpriteIndexBuffer", index_size);
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

        UpdateTransforms(sprite_world, sprite_context->m_Subpixels);

        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t sprite_count = components.Size();

        if (!sprite_count)
            return dmGameObject::UPDATE_RESULT_OK;

        // Submit all sprites as entries in the render list for sorting.
        dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(render_context, sprite_count);
        dmRender::HRenderListDispatch sprite_dispatch = dmRender::RenderListMakeDispatch(render_context, &RenderListDispatch, sprite_world);
        dmRender::RenderListEntry* write_ptr = render_list;

        for (uint32_t i = 0; i < sprite_count; ++i)
        {
            SpriteComponent& component = components[i];
            if (!component.m_Enabled || !component.m_AddedToUpdate)
                continue;

            if (component.m_ReHash || dmGameSystem::AreRenderConstantsUpdated(&component.m_RenderConstants))
            {
                ReHash(&component);
            }

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(GetMaterial(&component, component.m_Resource));
            write_ptr->m_Dispatch = sprite_dispatch;
            write_ptr->m_MinorOrder = 0;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompSpriteGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;
        return GetRenderConstant(&component->m_RenderConstants, name_hash, out_constant);
    }

    static void CompSpriteSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;
        SetRenderConstant(&component->m_RenderConstants, GetMaterial(component, component->m_Resource), name_hash, element_index, var);
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
                        dmGameObject::PropertyVar(ddf->m_Value), CompSpriteSetConstantCallback, component);
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
        return GetMaterialConstant(GetMaterial(component, component->m_Resource), get_property, out_value, false, CompSpriteGetConstantCallback, component);
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
                // Try to maintain offset and rate
                PlayAnimation(component, component->m_CurrentAnimation, GetCursor(component), component->m_PlaybackRate);

                TextureSetResource* texture_set = GetTextureSet(component, component->m_Resource);
                sprite_world->m_UsesGeometries |= texture_set->m_TextureSet->m_Geometries.m_Count != 0 ? 1 : 0;
            }
            return res;
        }
        return SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, CompSpriteSetConstantCallback, component);
    }
}
