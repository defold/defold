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
        uint32_t                    m_AnimationID;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
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
        uint16_t                    m_ComponentIndex;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_Playing : 1;
        uint16_t                    m_FlipHorizontal : 1;
        uint16_t                    m_FlipVertical : 1;
        uint16_t                    m_AddedToUpdate : 1;
        uint16_t                    m_ReHash : 1;
        uint16_t                    m_Padding : 3;
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
    };

    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SCALE, scale, false);
    DM_GAMESYS_PROP_VECTOR3(SPRITE_PROP_SIZE, size, true);

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        if(sprite_context->m_MaxSpriteCount > 16384)
        {
            // We use 4 vertices per sprite and use 16bit index buffers, so max sprite count is 65536/4
            dmLogError("Max sprite count (%d) exceeds max value (16384).", sprite_context->m_MaxSpriteCount);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
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

        sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        sprite_world->m_VertexBufferData = (SpriteVertex*) malloc(sizeof(SpriteVertex) * 4 * sprite_world->m_Components.Capacity());
        {
            uint32_t indices_count = 6*sprite_context->m_MaxSpriteCount;
            uint16_t* indices = new uint16_t[indices_count];
            uint16_t* index = indices;
            for(int32_t i = 0, v = 0; i < indices_count; i += 6, v += 4)
            {
                *index++ = v;
                *index++ = v+1;
                *index++ = v+2;
                *index++ = v+2;
                *index++ = v+3;
                *index++ = v;
            }

            sprite_world->m_IndexBuffer = dmGraphics::NewIndexBuffer(dmRender::GetGraphicsContext(render_context), indices_count*sizeof(uint16_t), indices, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
            delete[] indices;
        }


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

    static bool PlayAnimation(SpriteComponent* component, dmhash_t animation)
    {
        TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
        uint32_t* anim_id = texture_set->m_AnimationIds.Get(animation);
        if (anim_id)
        {
            component->m_AnimationID = *anim_id;
            component->m_CurrentAnimation = animation;
            component->m_CurrentAnimationFrame = 0;
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_TextureSet->m_Animations[*anim_id];
            uint32_t frame_count = animation->m_End - animation->m_Start;
            if (animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG
                    || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
                frame_count = dmMath::Max(1u, frame_count * 2 - 2);
            component->m_AnimInvDuration = (float)animation->m_Fps / frame_count;
            component->m_AnimTimer = 0.0f;
            component->m_Playing = animation->m_Playback != dmGameSystemDDF::PLAYBACK_NONE;
            component->m_Size = GetSize(component, texture_set->m_TextureSet, component->m_AnimationID);
        }
        else
        {
            // TODO: Why stop the current animation? Shouldn't it continue playing the current animation?
            component->m_Playing = 0;
            component->m_CurrentAnimation = 0x0;
            component->m_CurrentAnimationFrame = 0;
            dmLogError("Unable to play animation '%s' since it could not be found.", dmHashReverseSafe64(animation));
        }
        return anim_id != 0;
    }

    static inline dmRender::HMaterial GetMaterial(const SpriteComponent* component, const SpriteResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static inline TextureSetResource* GetTextureSet(const SpriteComponent* component, const SpriteResource* resource) {
        return component->m_TextureSet ? component->m_TextureSet : resource->m_TextureSet;
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
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_Scale = Vector3(1.0f);

        component->m_ReHash = 1;

        component->m_Size = Vector3(0.0f, 0.0f, 0.0f);
        component->m_AnimationID = 0;
        PlayAnimation(component, component->m_Resource->m_DefaultAnimation);

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


    SpriteVertex* CreateVertexData(SpriteWorld* sprite_world, SpriteVertex* where, TextureSetResource* texture_set, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Sprite, "CreateVertexData");
        static int tex_coord_order[] = {
            0,1,2,2,3,0,
            3,2,1,1,0,3,    //h
            1,0,3,3,2,1,    //v
            2,3,0,0,1,2     //hv
        };

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
        const float* tex_coords = (const float*) texture_set->m_TextureSet->m_TexCoords.m_Data;

        for (uint32_t *i = begin;i != end; ++i)
        {
            const SpriteComponent* component = (SpriteComponent*) buf[*i].m_UserData;

            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_AnimationID];

            const float* tc = &tex_coords[(animation_ddf->m_Start + component->m_CurrentAnimationFrame) * 8];
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
            where[0].x = p0.getX();
            where[0].y = p0.getY();
            where[0].z = p0.getZ();
            where[0].u = tc[tex_lookup[0] * 2];
            where[0].v = tc[tex_lookup[0] * 2 + 1];

            Vector4 p1 = w * Point3(-0.5f, 0.5f, 0.0f);
            where[1].x = p1.getX();
            where[1].y = p1.getY();
            where[1].z = p1.getZ();
            where[1].u = tc[tex_lookup[1] * 2];
            where[1].v = tc[tex_lookup[1] * 2 + 1];

            Vector4 p2 = w * Point3(0.5f, 0.5f, 0.0f);
            where[2].x = p2.getX();
            where[2].y = p2.getY();
            where[2].z = p2.getZ();
            where[2].u = tc[tex_lookup[2] * 2];
            where[2].v = tc[tex_lookup[2] * 2 + 1];

            Vector4 p3 = w * Point3(0.5f, -0.5f, 0.0f);
            where[3].x = p3.getX();
            where[3].y = p3.getY();
            where[3].z = p3.getZ();
            where[3].u = tc[tex_lookup[4] * 2];
            where[3].v = tc[tex_lookup[4] * 2 + 1];

            where += 4;
        }

        return where;
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
        SpriteVertex *vb_begin = sprite_world->m_VertexBufferWritePtr;
        sprite_world->m_VertexBufferWritePtr = CreateVertexData(sprite_world, vb_begin, texture_set, buf, begin, end);

        ro.Init();
        ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
        ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
        ro.m_IndexBuffer = sprite_world->m_IndexBuffer;
        ro.m_Material = GetMaterial(first, resource);
        ro.m_Textures[0] = texture_set->m_Texture;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_IndexType = dmGraphics::TYPE_UNSIGNED_SHORT;
        ro.m_VertexStart = (vb_begin - sprite_world->m_VertexBufferData)*3;             // Element arrays: offset in bytes into element buffer. Triangles is 3 elements = 6 bytes (16bit)
        ro.m_VertexCount = ((sprite_world->m_VertexBufferWritePtr - vb_begin)>>1)*3;    // Element arrays: number of elements to draw, which is triangles * 3

        const dmRender::Constant* constants = first->m_RenderConstants.m_RenderConstants;
        uint32_t size = first->m_RenderConstants.m_ConstantCount;
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmRender::Constant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmGameSystemDDF::SpriteDesc::BlendMode blend_mode = first->m_Resource->m_DDF->m_BlendMode;
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

            TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_AnimationID];

            bool once = animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_FORWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG;
            // Stop once-animation and broadcast animation_done
            if (once && component->m_AnimTimer >= 1.0f)
            {
                component->m_Playing = 0;
                if (component->m_ListenerInstance != 0x0)
                {
                    dmhash_t message_id = dmGameSystemDDF::AnimationDone::m_DDFDescriptor->m_NameHash;
                    dmGameSystemDDF::AnimationDone message;
                    // Engine has 0-based indices, scripts use 1-based
                    message.m_CurrentTile = component->m_CurrentAnimationFrame + 1;
                    message.m_Id = component->m_CurrentAnimation;
                    dmMessage::URL receiver;
                    receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_ListenerInstance));
                    dmMessage::URL sender;
                    sender.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
                    if (dmMessage::IsSocketValid(receiver.m_Socket) && dmMessage::IsSocketValid(sender.m_Socket))
                    {
                        dmGameObject::Result go_result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &sender.m_Fragment);
                        if (go_result == dmGameObject::RESULT_OK)
                        {
                            receiver.m_Path = dmGameObject::GetIdentifier(component->m_ListenerInstance);
                            receiver.m_Fragment = component->m_ListenerComponent;
                            sender.m_Path = dmGameObject::GetIdentifier(component->m_Instance);
                            uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::AnimationDone::m_DDFDescriptor;
                            uint32_t data_size = sizeof(dmGameSystemDDF::AnimationDone);
                            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size, 0);
                            component->m_ListenerInstance = 0x0;
                            component->m_ListenerComponent = 0xff;
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
                        component->m_ListenerInstance = 0x0;
                        component->m_ListenerComponent = 0xff;
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
            if (!component->m_Enabled || !component->m_Playing || !component->m_AddedToUpdate)
                continue;

            TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_AnimationID];

            // Animate
            component->m_AnimTimer += dt * component->m_AnimInvDuration;
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

            if (frame != component->m_CurrentAnimationFrame)
            {
                component->m_Size = GetSize(component, texture_set_ddf, component->m_AnimationID);
            }

            component->m_CurrentAnimationFrame = frame;
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
        SpriteWorld *world = (SpriteWorld *) params.m_UserData;

        switch (params.m_Operation)
        {
            case dmRender::RENDER_LIST_OPERATION_BEGIN:
                world->m_VertexBufferWritePtr = world->m_VertexBufferData;
                world->m_RenderObjects.SetSize(0);
                break;
            case dmRender::RENDER_LIST_OPERATION_END:
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, 0, 0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                dmGraphics::SetVertexBufferData(world->m_VertexBuffer, sizeof(SpriteVertex) * (world->m_VertexBufferWritePtr - world->m_VertexBufferData),
                                                world->m_VertexBufferData, dmGraphics::BUFFER_USAGE_STATIC_DRAW);
                DM_COUNTER("SpriteVertexBuffer", (world->m_VertexBufferWritePtr - world->m_VertexBufferData) * sizeof(SpriteVertex));
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

            bool rehash = component.m_ReHash;
            if(!rehash)
            {
                uint32_t const_count = component.m_RenderConstants.m_ConstantCount;
                for (uint32_t const_i = 0; const_i < const_count; ++const_i)
                {
                    if (lengthSqr(component.m_RenderConstants.m_RenderConstants[const_i].m_Value - component.m_RenderConstants.m_PrevRenderConstants[const_i]) > 0)
                    {
                        rehash = true;
                        break;
                    }
                }
            }
            if(rehash)
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
                if (PlayAnimation(component, ddf->m_Id))
                {
                    component->m_ListenerInstance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), params.m_Message->m_Sender.m_Path);
                    component->m_ListenerComponent = params.m_Message->m_Sender.m_Fragment;
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
                dmRender::Constant* constants = component->m_RenderConstants.m_RenderConstants;
                uint32_t size = component->m_RenderConstants.m_ConstantCount;
                for (uint32_t i = 0; i < size; ++i)
                {
                    if (constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants[i] = constants[size - 1];
                        component->m_RenderConstants.m_PrevRenderConstants[i] = component->m_RenderConstants.m_PrevRenderConstants[size - 1];
                        component->m_RenderConstants.m_ConstantCount--;
                        component->m_ReHash = 1;
                        break;
                    }
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
            PlayAnimation(component, component->m_CurrentAnimation);
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
            return res;
        }
        return SetMaterialConstant(GetMaterial(component, component->m_Resource), params.m_PropertyId, params.m_Value, CompSpriteSetConstantCallback, component);
    }
}
