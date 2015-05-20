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

#include "sprite_ddf.h"

using namespace Vectormath::Aos;
namespace dmGameSystem
{
    const uint32_t MAX_CONSTANTS = 4;

    struct SpriteComponent
    {
        dmGameObject::HInstance     m_Instance;
        Point3                      m_Position;
        Quat                        m_Rotation;
        Vector3                     m_Scale;
        Matrix4                     m_World;
        // Hash of the m_Resource-pointer. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        // See GenerateKeys
        uint32_t                    m_MixedHash;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
        SpriteResource*             m_Resource;
        dmRender::Constant          m_RenderConstants[MAX_CONSTANTS];
        Vector4                     m_PrevRenderConstants[MAX_CONSTANTS];
        uint32_t                    m_ConstantCount;
        /// Currently playing animation
        dmhash_t                    m_CurrentAnimation;
        /// Used to scale the time step when updating the timer
        float                       m_AnimInvDuration;
        /// Timer in local space: [0,1]
        float                       m_AnimTimer;
        uint16_t                    m_ComponentIndex : 8;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_Playing : 1;
        uint16_t                    m_FlipHorizontal : 1;
        uint16_t                    m_FlipVertical : 1;
        uint16_t                    m_AddedToUpdate : 1;
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
    };

    struct PropVector3
    {
        dmhash_t m_Vector;
        dmhash_t m_X;
        dmhash_t m_Y;
        dmhash_t m_Z;
        bool m_ReadOnly;

        PropVector3(dmhash_t v, dmhash_t x, dmhash_t y, dmhash_t z, bool readOnly)
        {
            m_Vector = v;
            m_X = x;
            m_Y = y;
            m_Z = z;
            m_ReadOnly = readOnly;
        }
    };

    static bool IsReferencingProperty(const PropVector3& property, dmhash_t query);
    static dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, Vector3& ref_value, const PropVector3& property);
    static dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, Vector3& set_value, const PropVector3& property);

#define PROP_VECTOR3(var_name, prop_name, readOnly)\
    static const PropVector3 PROP_##var_name(dmHashString64(#prop_name),\
            dmHashString64(#prop_name ".x"),\
            dmHashString64(#prop_name ".y"),\
            dmHashString64(#prop_name ".z"),\
            readOnly);

    PROP_VECTOR3(SCALE, scale, false);
    PROP_VECTOR3(SIZE, size, true);

#undef PROP_VECTOR3

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

        sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), 0, 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        sprite_world->m_VertexBufferData = (SpriteVertex*) malloc(sizeof(SpriteVertex) * 6 * sprite_world->m_Components.Capacity());

        *params.m_World = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(sprite_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);
        free(sprite_world->m_VertexBufferData);

        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    bool PlayAnimation(SpriteComponent* component, dmhash_t animation_id)
    {
        TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
        uint32_t* anim_id = texture_set->m_AnimationIds.Get(animation_id);
        if (anim_id)
        {
            component->m_CurrentAnimation = animation_id;
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_TextureSet->m_Animations[*anim_id];
            uint32_t frame_count = animation->m_End - animation->m_Start;
            if (animation->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG
                    || animation->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
                frame_count = dmMath::Max(1u, frame_count * 2 - 2);
            component->m_AnimInvDuration = (float)animation->m_Fps / frame_count;
            component->m_AnimTimer = 0.0f;
            component->m_Playing = animation->m_Playback != dmGameSystemDDF::PLAYBACK_NONE;
        }
        else
        {
            const char* anim_name = (const char*) dmHashReverse64(animation_id, 0);
            if (anim_name != 0x0) {
                dmLogError("Unable to play animation '%s' since it could not be found.", anim_name);
            } else {
                dmLogError("Unable to play animation '%llu' since it could not be found.", animation_id);
            }
        }
        return anim_id != 0;
    }

    void ReHash(SpriteComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        SpriteResource* resource = component->m_Resource;
        dmGameSystemDDF::SpriteDesc* ddf = resource->m_DDF;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource->m_TextureSet, sizeof(resource->m_TextureSet));
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        dmRender::Constant* constants = component->m_RenderConstants;
        uint32_t size = component->m_ConstantCount;
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
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Resource = (SpriteResource*)params.m_Resource;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_ComponentIndex = params.m_ComponentIndex;
        component->m_Enabled = 1;
        component->m_Scale = Vector3(1.0f);
        component->m_ConstantCount = 0;
        ReHash(component);
        PlayAnimation(component, component->m_Resource->m_DefaultAnimation);

        *params.m_UserData = (uintptr_t)index;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        uint32_t index = *params.m_UserData;
        sprite_world->m_Components.Free(index, true);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static uint32_t GetCurrentTile(const SpriteComponent* component, const dmGameSystemDDF::TextureSetAnimation* anim_ddf)
    {
        float t = component->m_AnimTimer;
        bool backwards = anim_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD
                || anim_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD;
        if (backwards)
            t = 1.0f - t;
        uint32_t interval = anim_ddf->m_End - anim_ddf->m_Start;
        uint32_t frame_count = interval;
        if (anim_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG
                || anim_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
        {
            frame_count = dmMath::Max(1u, frame_count * 2 - 2);
        }
        uint32_t frame = dmMath::Min(frame_count - 1, (uint32_t)(t * frame_count));
        if (frame >= interval) {
            frame = 2 * (interval - 1) - frame;
        }
        return frame + anim_ddf->m_Start;
    }

    static Vector3 GetSize(const SpriteComponent* sprite)
    {
        Vector3 result = Vector3(0.0f, 0.0f, 0.0f);
        if (0x0 != sprite->m_Resource && 0x0 != sprite->m_Resource->m_TextureSet)
        {
            TextureSetResource* texture_set = sprite->m_Resource->m_TextureSet;
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(sprite->m_CurrentAnimation);
            if (anim_id)
            {
                dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
                dmGameSystemDDF::TextureSetAnimation* animation = &texture_set_ddf->m_Animations[*anim_id];
                result = Vector3(animation->m_Width, animation->m_Height, 1.0f);
            }
        }
        return result;
    }

    static Vector3 ComputeScaledSize(const SpriteComponent* sprite)
    {
        Vector3 result = GetSize(sprite);
        result.setX(result.getX() * sprite->m_Scale.getX());
        result.setY(result.getY() * sprite->m_Scale.getY());
        return result;
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

            uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
            if (!anim_id)
            {
                continue;
            }

            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[*anim_id];

            const float* tc = &tex_coords[GetCurrentTile(component, animation_ddf) * 8];
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

            where[3].x = p2.getX();
            where[3].y = p2.getY();
            where[3].z = p2.getZ();
            where[3].u = tc[tex_lookup[3] * 2];
            where[3].v = tc[tex_lookup[3] * 2 + 1];

            Vector4 p3 = w * Point3(0.5f, -0.5f, 0.0f);
            where[4].x = p3.getX();
            where[4].y = p3.getY();
            where[4].z = p3.getZ();
            where[4].u = tc[tex_lookup[4] * 2];
            where[4].v = tc[tex_lookup[4] * 2 + 1];

            where[5].x = where[0].x;
            where[5].y = where[0].y;
            where[5].z = where[0].z;
            where[5].u = tc[tex_lookup[5] * 2];
            where[5].v = tc[tex_lookup[5] * 2 + 1];

            where += 6;
        }

        return where;
    }

    static void RenderBatch(SpriteWorld* sprite_world, dmRender::HRenderContext render_context, dmRender::RenderListEntry *buf, uint32_t* begin, uint32_t* end)
    {
        DM_PROFILE(Sprite, "RenderBatch");

        const SpriteComponent* first = (SpriteComponent*) buf[*begin].m_UserData;
        assert(first->m_Enabled);

        TextureSetResource* texture_set = first->m_Resource->m_TextureSet;

        // Ninja in-place writing of render object.
        dmRender::RenderObject& ro = *sprite_world->m_RenderObjects.End();
        sprite_world->m_RenderObjects.SetSize(sprite_world->m_RenderObjects.Size()+1);

        // Fill in vertex buffer
        SpriteVertex *vb_begin = sprite_world->m_VertexBufferWritePtr;
        sprite_world->m_VertexBufferWritePtr = CreateVertexData(sprite_world, vb_begin, texture_set, buf, begin, end);

        ro.Init();
        ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
        ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vb_begin - sprite_world->m_VertexBufferData;
        ro.m_VertexCount = (sprite_world->m_VertexBufferWritePtr - vb_begin);
        ro.m_Material = first->m_Resource->m_Material;
        ro.m_Textures[0] = texture_set->m_Texture;

        const dmRender::Constant* constants = first->m_RenderConstants;
        uint32_t size = first->m_ConstantCount;
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

    void UpdateTransforms(SpriteWorld* sprite_world, bool sub_pixels)
    {
        DM_PROFILE(Sprite, "UpdateTransforms");

        dmArray<SpriteComponent>& components = sprite_world->m_Components.m_Objects;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* c = &components[i];

            // NOTE: texture_set = c->m_Resource might be NULL so it's essential to "continue" here
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            TextureSetResource* texture_set = c->m_Resource->m_TextureSet;
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(c->m_CurrentAnimation);
            if (anim_id)
            {
                dmTransform::Transform world = dmGameObject::GetWorldTransform(c->m_Instance);
                dmTransform::Transform local(Vector3(c->m_Position), c->m_Rotation, 1.0f);
                if (dmGameObject::ScaleAlongZ(c->m_Instance))
                {
                    world = dmTransform::Mul(world, local);
                }
                else
                {
                    world = dmTransform::MulNoScaleZ(world, local);
                }
                Matrix4 w = dmTransform::ToMatrix4(world);
                w = appendScale(w, ComputeScaledSize(c));
                Vector4 position = w.getCol3();
                if (!sub_pixels)
                {
                    position.setX((int) position.getX());
                    position.setY((int) position.getY());
                }
                w.setCol3(position);
                c->m_World = w;
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
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
            if (!anim_id)
            {
                component->m_Playing = 0;
                continue;
            }

            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[*anim_id];

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
                    message.m_CurrentTile = GetCurrentTile(component, animation_ddf) - animation_ddf->m_Start + 1;
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
                            dmMessage::Result result = dmMessage::Post(&sender, &receiver, message_id, 0, descriptor, &message, data_size);
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
            uint32_t* anim_id = texture_set->m_AnimationIds.Get(component->m_CurrentAnimation);
            if (!anim_id)
            {
                continue;
            }

            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[*anim_id];

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
        }
    }

    dmGameObject::CreateResult CompSpriteAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        uint32_t index = (uint32_t)*params.m_UserData;
        SpriteComponent* component = &sprite_world->m_Components.Get(index);
        component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        /*
         * NOTES:
         * * When/if transparency the batching predicates must be updated in order to
         *   support per sprite correct sorting.
         */

        SpriteContext* context = (SpriteContext*)params.m_Context;
        SpriteWorld* world = (SpriteWorld*)params.m_World;
        UpdateTransforms(world, context->m_Subpixels);
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

            uint32_t const_count = component.m_ConstantCount;
            for (uint32_t const_i = 0; const_i < const_count; ++const_i)
            {
                if (lengthSqr(component.m_RenderConstants[const_i].m_Value - component.m_PrevRenderConstants[const_i]) > 0)
                {
                    ReHash(&component);
                    break;
                }
            }

            const Vector4 trans = component.m_World.getCol(3);
            write_ptr->m_WorldPosition = Point3(trans.getX(), trans.getY(), trans.getZ());
            write_ptr->m_UserData = (uintptr_t) &component;
            write_ptr->m_BatchKey = component.m_MixedHash;
            write_ptr->m_TagMask = dmRender::GetMaterialTagMask(component.m_Resource->m_Material);
            write_ptr->m_Dispatch = sprite_dispatch;
            write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_WORLD;
            ++write_ptr;
        }

        dmRender::RenderListSubmit(render_context, render_list, write_ptr);
        return dmGameObject::UPDATE_RESULT_OK;
    }

    static bool CompSpriteGetConstantCallback(void* user_data, dmhash_t name_hash, dmRender::Constant** out_constant)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;
        dmRender::Constant* constants = component->m_RenderConstants;
        uint32_t count = component->m_ConstantCount;
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

    static void CompSpriteSetConstantCallback(void* user_data, dmhash_t name_hash, uint32_t* element_index, const dmGameObject::PropertyVar& var)
    {
        SpriteComponent* component = (SpriteComponent*)user_data;
        dmRender::Constant* constants = component->m_RenderConstants;
        uint32_t count = component->m_ConstantCount;
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
            if (count == MAX_CONSTANTS)
            {
                dmLogWarning("Out of sprite constants (%d)", MAX_CONSTANTS);
                return;
            }
            dmRender::Constant c;
            dmRender::GetMaterialProgramConstant(component->m_Resource->m_Material, name_hash, c);
            constants[count] = c;
            component->m_PrevRenderConstants[count] = c.m_Value;
            v = &(constants[count].m_Value);
            component->m_ConstantCount++;
            assert(component->m_ConstantCount <= MAX_CONSTANTS);
        }
        if (element_index == 0x0)
            *v = Vector4(var.m_V4[0], var.m_V4[1], var.m_V4[2], var.m_V4[3]);
        else
            v->setElem(*element_index, (float)var.m_Number);
        ReHash(component);
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
                dmGameObject::PropertyResult result = dmGameSystem::SetMaterialConstant(component->m_Resource->m_Material, ddf->m_NameHash,
                        dmGameObject::PropertyVar(ddf->m_Value), CompSpriteSetConstantCallback, component);
                if (result == dmGameObject::PROPERTY_RESULT_NOT_FOUND)
                {
                    dmMessage::URL& receiver = params.m_Message->m_Receiver;
                    dmLogError("'%s:%s#%s' has no constant named '%s'",
                            dmMessage::GetSocketName(receiver.m_Socket),
                            (const char*)dmHashReverse64(receiver.m_Path, 0x0),
                            (const char*)dmHashReverse64(receiver.m_Fragment, 0x0),
                            (const char*)dmHashReverse64(ddf->m_NameHash, 0x0));
                }
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                dmRender::Constant* constants = component->m_RenderConstants;
                uint32_t size = component->m_ConstantCount;
                for (uint32_t i = 0; i < size; ++i)
                {
                    if (constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants[i] = constants[size - 1];
                        component->m_PrevRenderConstants[i] = component->m_PrevRenderConstants[size - 1];
                        component->m_ConstantCount--;
                        ReHash(component);
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

    static dmGameObject::PropertyResult GetProperty(dmGameObject::PropertyDesc& out_value, dmhash_t get_property, Vector3& ref_value, const PropVector3& property)
    {
        dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_OK;

        // We deliberately do not provide a value pointer in the case of read only variables,
        // in order to ensure that it is not used in any write optimisation (see animation system).
        out_value.m_ReadOnly = property.m_ReadOnly;
        if (get_property == property.m_Vector)
        {
            if (!property.m_ReadOnly)
            {
                out_value.m_ValuePtr = (float*)&ref_value;
            }
            out_value.m_ElementIds[0] = property.m_X;
            out_value.m_ElementIds[1] = property.m_Y;
            out_value.m_ElementIds[2] = property.m_Z;
            out_value.m_Variant = dmGameObject::PropertyVar(ref_value);
        }
        else if (get_property == property.m_X)
        {
            if (!property.m_ReadOnly)
            {
                out_value.m_ValuePtr = (float*)&ref_value;
            }
            out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getX());
        }
        else if (get_property == property.m_Y)
        {
            if (!property.m_ReadOnly)
            {
                out_value.m_ValuePtr = (float*)&ref_value + 1;
            }
            out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getY());
        }
        else if (get_property == property.m_Z)
        {
            if (!property.m_ReadOnly)
            {
                out_value.m_ValuePtr = (float*)&ref_value + 2;
            }
            out_value.m_Variant = dmGameObject::PropertyVar(ref_value.getZ());
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        return result;
    }

    static dmGameObject::PropertyResult SetProperty(dmhash_t set_property, const dmGameObject::PropertyVar& in_value, Vector3& set_value, const PropVector3& property)
    {
        dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_OK;

        if (property.m_ReadOnly)
        {
            result = dmGameObject::PROPERTY_RESULT_UNSUPPORTED_OPERATION;
        }

        if (set_property == property.m_Vector)
        {
            if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_VECTOR3)
            {
                set_value.setX(in_value.m_V4[0]);
                set_value.setY(in_value.m_V4[1]);
                set_value.setZ(in_value.m_V4[2]);
            }
            else
            {
                result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        else if (set_property == property.m_X)
        {
            if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                set_value.setX(in_value.m_Number);
            }
            else
            {
                result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        else if (set_property == property.m_Y)
        {
            if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                set_value.setY(in_value.m_Number);
            }
            else
            {
                result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        else if (set_property == property.m_Z)
        {
            if (in_value.m_Type == dmGameObject::PROPERTY_TYPE_NUMBER)
            {
                set_value.setZ(in_value.m_Number);
            }
            else
            {
                result = dmGameObject::PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        else
        {
            result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        }

        return result;
    }

    static bool IsReferencingProperty(const PropVector3& property, dmhash_t query)
    {
        return property.m_Vector == query
                || property.m_X == query || property.m_Y == query || property.m_Z == query;
    }

    dmGameObject::PropertyResult CompSpriteGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value)
    {
        dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
        dmhash_t get_property = params.m_PropertyId;

        if (IsReferencingProperty(PROP_SCALE, get_property))
        {
            result = GetProperty(out_value, get_property, component->m_Scale, PROP_SCALE);
        }
        else if (IsReferencingProperty(PROP_SIZE, get_property))
        {
            Vector3 size = GetSize(component);
            result = GetProperty(out_value, get_property, size, PROP_SIZE);
        }

        if (dmGameObject::PROPERTY_RESULT_NOT_FOUND == result)
        {
            result = GetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, out_value, CompSpriteGetConstantCallback, component);
        }

        return result;
    }

    dmGameObject::PropertyResult CompSpriteSetProperty(const dmGameObject::ComponentSetPropertyParams& params)
    {
        dmGameObject::PropertyResult result = dmGameObject::PROPERTY_RESULT_NOT_FOUND;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = &sprite_world->m_Components.Get(*params.m_UserData);
        dmhash_t set_property = params.m_PropertyId;

        if (IsReferencingProperty(PROP_SCALE, set_property))
        {
            result = SetProperty(set_property, params.m_Value, component->m_Scale, PROP_SCALE);
        }
        else if (IsReferencingProperty(PROP_SIZE, set_property))
        {
            Vector3 size = GetSize(component);
            result = SetProperty(set_property, params.m_Value, size, PROP_SIZE);
        }

        if (dmGameObject::PROPERTY_RESULT_NOT_FOUND == result)
        {
            result = SetMaterialConstant(component->m_Resource->m_Material, params.m_PropertyId, params.m_Value, CompSpriteSetConstantCallback, component);
        }

        return result;
    }
}
