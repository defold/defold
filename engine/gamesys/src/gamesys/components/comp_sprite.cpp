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
#include <dlib/index_pool.h>
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

    union SortKey
    {
        struct
        {
            uint64_t m_Index : 16;  // Index is used to ensure stable sort
            uint64_t m_Z : 16; // Quantified relative z
            uint64_t m_MixedHash : 32;
        };
        uint64_t     m_Key;
    };

    struct SpriteComponent
    {
        dmGameObject::HInstance     m_Instance;
        Point3                      m_Position;
        Quat                        m_Rotation;
        Vector3                     m_Scale;
        Matrix4                     m_World;
        SortKey                     m_SortKey;
        // Hash of the m_Resource-pointer. Hash is used to be compatible with 64-bit arch as a 32-bit value is used for sorting
        // See GenerateKeys
        uint32_t                    m_MixedHash;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
        SpriteResource*             m_Resource;
        dmArray<dmGameSystemDDF::SetConstant> m_RenderConstants;
        float                       m_FrameTime;
        float                       m_FrameTimer;
        uint16_t                    m_CurrentAnimation;
        uint16_t                    m_CurrentTile;
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_PlayBackwards : 1;
        uint8_t                     m_Playing : 1;
        uint8_t                     m_FlipHorizontal : 1;
        uint8_t                     m_FlipVertical : 1;
    };

    struct SpriteWorld
    {
        dmArray<SpriteComponent>        m_Components;
        dmIndexPool32                   m_ComponentIndices;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;

        dmArray<uint32_t>               m_RenderSortBuffer;
        float                           m_MinZ;
        float                           m_MaxZ;
    };

    struct SpriteVertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
    };

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;
        SpriteWorld* sprite_world = new SpriteWorld();

        sprite_world->m_Components.SetCapacity(sprite_context->m_MaxSpriteCount);
        sprite_world->m_Components.SetSize(sprite_context->m_MaxSpriteCount);
        memset(&sprite_world->m_Components[0], 0, sizeof(SpriteComponent) * sprite_context->m_MaxSpriteCount);
        sprite_world->m_ComponentIndices.SetCapacity(sprite_context->m_MaxSpriteCount);
        sprite_world->m_RenderObjects.SetCapacity(sprite_context->m_MaxSpriteCount);

        sprite_world->m_RenderSortBuffer.SetCapacity(sprite_context->m_MaxSpriteCount);
        sprite_world->m_RenderSortBuffer.SetSize(sprite_context->m_MaxSpriteCount);
        for (uint32_t i = 0; i < sprite_context->m_MaxSpriteCount; ++i)
        {
            sprite_world->m_RenderSortBuffer[i] = i;
        }
        sprite_world->m_MinZ = 0;
        sprite_world->m_MaxZ = 0;

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
        };

        sprite_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), sizeof(float) * sizeof(SpriteVertex) * 6 * sprite_world->m_Components.Capacity(), 0x0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        *params.m_World = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        dmGraphics::DeleteVertexDeclaration(sprite_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);

        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    bool PlayAnimation(SpriteComponent* component, dmhash_t animation_id)
    {
        bool anim_found = false;
        TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
        uint32_t n = texture_set->m_AnimationIds.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (animation_id == texture_set->m_AnimationIds[i])
            {
                component->m_CurrentAnimation = i;
                anim_found = true;
                break;
            }
        }
        if (anim_found)
        {
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_TextureSet->m_Animations[component->m_CurrentAnimation];
            component->m_CurrentTile = animation->m_Start;
            component->m_PlayBackwards = 0;
            component->m_FrameTime = 1.0f / animation->m_Fps;
            component->m_FrameTimer = 0.0f;
            component->m_Playing = animation->m_Playback != dmGameSystemDDF::PLAYBACK_NONE;
        }
        return anim_found;
    }

    void ReHash(SpriteComponent* component)
    {
        // Hash resource-ptr, material-handle, blend mode and render constants
        HashState32 state;
        bool reverse = false;
        SpriteResource* resource = component->m_Resource;
        dmGameSystemDDF::SpriteDesc* ddf = resource->m_DDF;
        dmHashInit32(&state, reverse);
        dmHashUpdateBuffer32(&state, &resource, sizeof(resource));
        dmHashUpdateBuffer32(&state, &resource->m_Material, sizeof(resource->m_Material));
        dmHashUpdateBuffer32(&state, &ddf->m_BlendMode, sizeof(ddf->m_BlendMode));
        dmArray<dmGameSystemDDF::SetConstant>& constants = component->m_RenderConstants;
        uint32_t size = constants.Size();
        // Padding in the SetConstant-struct forces us to copy the components by hand
        for (uint32_t i = 0; i < size; ++i)
        {
            dmGameSystemDDF::SetConstant& c = constants[i];
            dmHashUpdateBuffer32(&state, &c.m_NameHash, sizeof(uint64_t));
            dmHashUpdateBuffer32(&state, &c.m_Value, sizeof(Vector4));
        }
        component->m_MixedHash = dmHashFinal32(&state);
    }

    dmGameObject::CreateResult CompSpriteCreate(const dmGameObject::ComponentCreateParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        if (sprite_world->m_ComponentIndices.Remaining() == 0)
        {
            dmLogError("Sprite could not be created since the sprite buffer is full (%d).", sprite_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        uint32_t index = sprite_world->m_ComponentIndices.Pop();
        SpriteComponent* component = &sprite_world->m_Components[index];
        component->m_Instance = params.m_Instance;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Resource = (SpriteResource*)params.m_Resource;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_Enabled = 1;
        component->m_Scale = Vector3(1.0f);
        ReHash(component);
        PlayAnimation(component, component->m_Resource->m_DefaultAnimation);

        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        SpriteComponent* component = (SpriteComponent*)*params.m_UserData;
        uint32_t index = component - &sprite_world->m_Components[0];
        memset(component, 0, sizeof(SpriteComponent));
        sprite_world->m_ComponentIndices.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    struct SortPred
    {
        SortPred(SpriteWorld* sprite_world) : m_SpriteWorld(sprite_world) {}

        SpriteWorld* m_SpriteWorld;

        inline bool operator () (const uint32_t x, const uint32_t y)
        {
            SpriteComponent* c1 = &m_SpriteWorld->m_Components[x];
            SpriteComponent* c2 = &m_SpriteWorld->m_Components[y];
            return c1->m_SortKey.m_Key < c2->m_SortKey.m_Key;
        }

    };

    void GenerateKeys(SpriteWorld* sprite_world)
    {
        dmArray<SpriteComponent>& components = sprite_world->m_Components;
        uint32_t n = components.Size();

        float min_z = sprite_world->m_MinZ;
        float range = 1.0f / (sprite_world->m_MaxZ - sprite_world->m_MinZ);

        SpriteComponent* first = sprite_world->m_Components.Begin();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* c = &components[i];
            uint32_t index = c - first;
            if (c->m_Resource && c->m_Enabled)
            {
                float z = (c->m_World.getElem(3, 2) - min_z) * range * 65535;
                z = dmMath::Clamp(z, 0.0f, 65535.0f);
                uint16_t zf = (uint16_t) z;
                c->m_SortKey.m_Z = zf;
                c->m_SortKey.m_Index = index;
                c->m_SortKey.m_MixedHash = c->m_MixedHash;
            }
            else
            {
                c->m_SortKey.m_Key = 0xffffffffffffffffULL;
            }
        }
    }

    void SortSprites(SpriteWorld* sprite_world)
    {
        DM_PROFILE(Sprite, "Sort");
        dmArray<uint32_t>* buffer = &sprite_world->m_RenderSortBuffer;
        SortPred pred(sprite_world);
        std::sort(buffer->Begin(), buffer->End(), pred);
    }

    void CreateVertexData(SpriteWorld* sprite_world, void* vertex_buffer, TextureSetResource* texture_set, uint32_t start_index, uint32_t end_index)
    {
        DM_PROFILE(Sprite, "CreateVertexData");

        const dmArray<SpriteComponent>& components = sprite_world->m_Components;
        const dmArray<uint32_t>& sort_buffer = sprite_world->m_RenderSortBuffer;

        dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;

        const float* tex_coords = (const float*) texture_set->m_TextureSet->m_TexCoords.m_Data;

        for (uint32_t i = start_index; i < end_index; ++i)
        {
            const SpriteComponent* component = &components[sort_buffer[i]];

            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_CurrentAnimation];

            SpriteVertex *v = (SpriteVertex*)((vertex_buffer)) + i * 6;

            const float* tc = &tex_coords[component->m_CurrentTile * 4];
            float u0 = tc[0];
            float v0 = tc[1];
            float u1 = tc[2];
            float v1 = tc[3];
            // ddf values are guaranteed to be 0 or 1 when saved by the editor
            // component values are guaranteed to be 0 or 1
            if (animation_ddf->m_FlipHorizontal ^ component->m_FlipHorizontal)
            {
                float u = u0;
                u0 = u1;
                u1 = u;
            }
            if (animation_ddf->m_FlipVertical ^ component->m_FlipVertical)
            {
                float v = v0;
                v0 = v1;
                v1 = v;
            }

            const Matrix4& w = component->m_World;

            Vector4 p0 = w * Point3(-0.5f, -0.5f, 0.0f);
            v[0].x = p0.getX();
            v[0].y = p0.getY();
            v[0].z = p0.getZ();
            v[0].u = u0;
            v[0].v = v1;

            Vector4 p1 = w * Point3(-0.5f, 0.5f, 0.0f);
            v[1].x = p1.getX();
            v[1].y = p1.getY();
            v[1].z = p1.getZ();
            v[1].u = u0;
            v[1].v = v0;

            Vector4 p2 = w * Point3(0.5f, 0.5f, 0.0f);
            v[2].x = p2.getX();
            v[2].y = p2.getY();
            v[2].z = p2.getZ();
            v[2].u = u1;
            v[2].v = v0;

            v[3].x = p2.getX();
            v[3].y = p2.getY();
            v[3].z = p2.getZ();
            v[3].u = u1;
            v[3].v = v0;

            Vector4 p3 = w * Point3(0.5f, -0.5f, 0.0f);
            v[4].x = p3.getX();
            v[4].y = p3.getY();
            v[4].z = p3.getZ();
            v[4].u = u1;
            v[4].v = v1;

            v[5].x = v[0].x;
            v[5].y = v[0].y;
            v[5].z = v[0].z;
            v[5].u = u0;
            v[5].v = v1;
        }
    }

    static uint32_t RenderBatch(SpriteWorld* sprite_world, dmRender::HRenderContext render_context, void* vertex_buffer, uint32_t start_index)
    {
        DM_PROFILE(Sprite, "RenderBatch");
        uint32_t n = sprite_world->m_Components.Size();

        const dmArray<SpriteComponent>& components = sprite_world->m_Components;
        const dmArray<uint32_t>& sort_buffer = sprite_world->m_RenderSortBuffer;

        const SpriteComponent* first = &components[sort_buffer[start_index]];
        assert(first->m_Enabled);
        TextureSetResource* texture_set = first->m_Resource->m_TextureSet;
        uint64_t z = first->m_SortKey.m_Z;
        uint32_t hash = first->m_MixedHash;

        uint32_t end_index = n;
        for (uint32_t i = start_index; i < n; ++i)
        {
            const SpriteComponent* c = &components[sort_buffer[i]];
            if (!c->m_Enabled || c->m_MixedHash != hash || c->m_SortKey.m_Z != z)
            {
                end_index = i;
                break;
            }
        }

        // Render object
        dmRender::RenderObject ro;
        ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
        ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = start_index * 6;
        ro.m_VertexCount = (end_index - start_index) * 6;
        ro.m_Material = first->m_Resource->m_Material;
        ro.m_Textures[0] = texture_set->m_Texture;
        // The first transform is used for the batch. Mean-value might be better?
        // NOTE: The position is already transformed, see CreateVertexData, but set for sorting.
        // See also sprite.vp
        ro.m_WorldTransform = first->m_World;
        ro.m_CalculateDepthKey = 1;

        const dmArray<dmGameSystemDDF::SetConstant>& constants = first->m_RenderConstants;
        uint32_t size = constants.Size();
        for (uint32_t i = 0; i < size; ++i)
        {
            const dmGameSystemDDF::SetConstant& c = constants[i];
            dmRender::EnableRenderObjectConstant(&ro, c.m_NameHash, c.m_Value);
        }

        dmGameSystemDDF::SpriteDesc::BlendMode blend_mode = first->m_Resource->m_DDF->m_BlendMode;
        switch (blend_mode)
        {
            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_ADD_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGameSystemDDF::SpriteDesc::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ZERO;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_SRC_COLOR;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }
        ro.m_SetBlendFactors = 1;

        sprite_world->m_RenderObjects.Push(ro);
        dmRender::AddToRender(render_context, &sprite_world->m_RenderObjects[sprite_world->m_RenderObjects.Size() - 1]);

        CreateVertexData(sprite_world, vertex_buffer, texture_set, start_index, end_index);
        return end_index;
    }

    void UpdateTransforms(SpriteWorld* sprite_world, bool sub_pixels)
    {
        DM_PROFILE(Sprite, "UpdateTransforms");

        dmArray<SpriteComponent>& components = sprite_world->m_Components;
        uint32_t n = components.Size();
        float min_z = FLT_MAX;
        float max_z = -FLT_MAX;
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* c = &components[i];
            if (c->m_Enabled)
            {
                TextureSetResource* texture_set = c->m_Resource->m_TextureSet;
                dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
                dmGameSystemDDF::TextureSetAnimation* animation = &texture_set_ddf->m_Animations[c->m_CurrentAnimation];

                dmTransform::TransformS1 world = dmGameObject::GetWorldTransform(c->m_Instance);
                dmTransform::TransformS1 local(Vector3(c->m_Position), c->m_Rotation, 1.0f);
                if (dmGameObject::ScaleAlongZ(c->m_Instance))
                {
                    world = dmTransform::Mul(world, local);
                }
                else
                {
                    world = dmTransform::MulNoScaleZ(world, local);
                }
                Matrix4 w = dmTransform::ToMatrix4(world);
                w = appendScale(w, Vector3(animation->m_Width * c->m_Scale.getX(), animation->m_Height * c->m_Scale.getY(), 1.0f));
                Vector4 position = w.getCol3();
                if (!sub_pixels)
                {
                    position.setX((int) position.getX());
                    position.setY((int) position.getY());
                }
                float z = position.getZ();
                min_z = dmMath::Min(min_z, z);
                max_z = dmMath::Max(max_z, z);
                w.setCol3(position);
                c->m_World = w;
            }
        }

        if (n == 0)
        {
            // NOTE: Avoid large numbers and risk of de-normalized etc.
            // if n == 0 the actual values of min/max-z doens't matter
            min_z = 0;
            max_z = 1;
        }

        sprite_world->m_MinZ = min_z;
        sprite_world->m_MaxZ = max_z;
    }

    static void PostMessages(SpriteWorld* sprite_world)
    {
        DM_PROFILE(Sprite, "PostMessages");

        dmArray<SpriteComponent>& components = sprite_world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            if (!component->m_Enabled)
                continue;

            TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_CurrentAnimation];

            int16_t end_tile = (int16_t)animation_ddf->m_End;
            // Stop once-animation and broadcast animation_done
            if (animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_FORWARD
                || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD)
            {
                if (component->m_CurrentTile == end_tile)
                {
                    component->m_Playing = 0;
                    if (component->m_ListenerInstance != 0x0)
                    {
                        dmhash_t message_id = dmGameSystemDDF::AnimationDone::m_DDFDescriptor->m_NameHash;
                        dmGameSystemDDF::AnimationDone message;
                        // Engine has 0-based indices, scripts use 1-based
                        message.m_CurrentTile = component->m_CurrentTile + 1;
                        dmMessage::URL receiver;
                        receiver.m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_ListenerInstance));
                        if (dmMessage::IsSocketValid(receiver.m_Socket))
                        {
                            receiver.m_Path = dmGameObject::GetIdentifier(component->m_ListenerInstance);
                            receiver.m_Fragment = component->m_ListenerComponent;
                            uintptr_t descriptor = (uintptr_t)dmGameSystemDDF::AnimationDone::m_DDFDescriptor;
                            uint32_t data_size = sizeof(dmGameSystemDDF::AnimationDone);
                            dmMessage::Result result = dmMessage::Post(0x0, &receiver, message_id, 0, descriptor, &message, data_size);
                            component->m_ListenerInstance = 0x0;
                            component->m_ListenerComponent = 0xff;
                            if (result != dmMessage::RESULT_OK)
                            {
                                dmLogError("Could not send animation_done to listener.");
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
    }

    static void Animate(SpriteWorld* sprite_world, float dt)
    {
        DM_PROFILE(Sprite, "Animate");

        dmArray<SpriteComponent>& components = sprite_world->m_Components;
        uint32_t n = components.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            SpriteComponent* component = &components[i];
            if (!component->m_Enabled)
                continue;

            TextureSetResource* texture_set = component->m_Resource->m_TextureSet;
            dmGameSystemDDF::TextureSet* texture_set_ddf = texture_set->m_TextureSet;
            dmGameSystemDDF::TextureSetAnimation* animation_ddf = &texture_set_ddf->m_Animations[component->m_CurrentAnimation];
            int16_t start_tile = (int16_t)animation_ddf->m_Start;
            int16_t end_tile = (int16_t)animation_ddf->m_End;

            // Animate
            if (component->m_Playing)
            {
                component->m_FrameTimer += dt;
                if (component->m_FrameTimer >= component->m_FrameTime)
                {
                    component->m_FrameTimer -= component->m_FrameTime;
                    int16_t current_tile = (int16_t)component->m_CurrentTile;
                    switch (animation_ddf->m_Playback)
                    {
                        case dmGameSystemDDF::PLAYBACK_ONCE_FORWARD:
                            if (current_tile != end_tile)
                                ++current_tile;
                            break;
                        case dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD:
                            if (current_tile != end_tile)
                                --current_tile;
                            break;
                        case dmGameSystemDDF::PLAYBACK_LOOP_FORWARD:
                            if (current_tile == end_tile)
                                current_tile = start_tile;
                            else
                                ++current_tile;
                            break;
                        case dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD:
                            if (current_tile == end_tile)
                                current_tile = start_tile;
                            else
                                --current_tile;
                            break;
                        case dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG:
                            if (component->m_PlayBackwards)
                                --current_tile;
                            else
                                ++current_tile;
                            break;
                        default:
                            break;
                    }
                    component->m_CurrentTile = (uint16_t)current_tile;
                    if (animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
                        if (current_tile == start_tile || current_tile == end_tile)
                            component->m_PlayBackwards = ~component->m_PlayBackwards;
                }
            }
        }
    }

    dmGameObject::UpdateResult CompSpriteUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        /*
         * All sprites are sorted, using the m_RenderSortBuffer, with respect to the:
         *
         *     - hash value of m_Resource, i.e. equal iff the sprite is rendering with identical tile-source
         *     - z-value
         *     - component index
         *  or
         *     - 0xffffffff (or corresponding 64-bit value) if not enabled
         * such that all non-enabled sprites ends up last in the array
         * and sprites with equal tileset and depth consecutively
         *
         * The z-sorting is considered a hack as we assume a camera pointing along the z-axis. We currently
         * have no access, by design as render-data currently should be invariant to camera parameters,
         * to the transformation matrices when generating render-data. The render-system and go-system should probably
         * be changed such that unique render-objects are created when necessary and on-demand instead of up-front as
         * currently. Another option could be a call-back when the actual rendering occur.
         *
         * The sorted array of indices are grouped into batches, using z and resource-hash as predicates, and every
         * batch is rendered using a single draw-call. Note that the world transform
         * is set to first sprite transform for correct batch sorting. The actual vertex transformation is performed in code
         * and standard world-transformation is removed from vertex-program.
         *
         * NOTES:
         * * When/if transparency the batching predicates must be updated in order to
         *   support per sprite correct sorting.
         */

        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        dmGraphics::SetVertexBufferData(sprite_world->m_VertexBuffer, 6 * sizeof(SpriteVertex) * sprite_world->m_Components.Size(), 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        void* vertex_buffer = dmGraphics::MapVertexBuffer(sprite_world->m_VertexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
        if (vertex_buffer == 0x0)
        {
            dmLogError("%s", "Could not map vertex buffer when drawing sprites.");
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        UpdateTransforms(sprite_world, sprite_context->m_Subpixels);
        GenerateKeys(sprite_world);
        SortSprites(sprite_world);

        sprite_world->m_RenderObjects.SetSize(0);

        const dmArray<SpriteComponent>& components = sprite_world->m_Components;
        const dmArray<uint32_t>& sort_buffer = sprite_world->m_RenderSortBuffer;

        uint32_t start_index = 0;
        uint32_t n = components.Size();
        while (start_index < n && components[sort_buffer[start_index]].m_Enabled)
        {
            start_index = RenderBatch(sprite_world, render_context, vertex_buffer, start_index);
        }

        PostMessages(sprite_world);
        Animate(sprite_world, params.m_UpdateContext->m_DT);

        if (!dmGraphics::UnmapVertexBuffer(sprite_world->m_VertexBuffer))
        {
            dmLogError("%s", "Could not unmap vertex buffer when drawing sprites.");
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        SpriteComponent* component = (SpriteComponent*)*params.m_UserData;
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
                dmArray<dmGameSystemDDF::SetConstant>& constants = component->m_RenderConstants;
                uint32_t size = constants.Size();
                bool found = false;
                for (uint32_t i = 0; i < size; ++i)
                {
                    if (constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants[i] = *ddf;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    if (constants.Full())
                    {
                        constants.SetCapacity(constants.Capacity() + 4);
                    }
                    constants.Push(*ddf);
                }
                ReHash(component);
            }
            else if (params.m_Message->m_Id == dmGameSystemDDF::ResetConstant::m_DDFDescriptor->m_NameHash)
            {
                dmGameSystemDDF::ResetConstant* ddf = (dmGameSystemDDF::ResetConstant*)params.m_Message->m_Data;
                dmArray<dmGameSystemDDF::SetConstant>& constants = component->m_RenderConstants;
                uint32_t size = constants.Size();
                for (uint32_t i = 0; i < size; ++i)
                {
                    if (constants[i].m_NameHash == ddf->m_NameHash)
                    {
                        constants.EraseSwap(i);
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
        SpriteComponent* component = (SpriteComponent*)*params.m_UserData;
        component->m_CurrentTile = 0;
        component->m_FrameTimer = 0.0f;
        component->m_FrameTime = 0.0f;
    }
}
