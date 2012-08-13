#include "comp_sprite.h"

#include <string.h>
#include <float.h>

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

extern unsigned char SPRITE_VPC[];
extern uint32_t SPRITE_VPC_SIZE;

extern unsigned char SPRITE_FPC[];
extern uint32_t SPRITE_FPC_SIZE;

using namespace Vectormath::Aos;
namespace dmGameSystem
{
    struct Component
    {
        dmGameObject::HInstance     m_Instance;
        Point3                      m_Position;
        Quat                        m_Rotation;
        dmGameObject::HInstance     m_ListenerInstance;
        dmhash_t                    m_ListenerComponent;
        SpriteResource*            m_Resource;
        float                       m_FrameTime;
        float                       m_FrameTimer;
        uint16_t                    m_CurrentAnimation;
        uint16_t                    m_CurrentTile;
        uint8_t                     m_Enabled : 1;
        uint8_t                     m_PlayBackwards : 1;
        uint8_t                     m_Playing : 1;
    };

    struct SpriteWorld
    {
        dmArray<Component>              m_Components;
        dmIndexPool32                   m_ComponentIndices;
        dmArray<dmRender::RenderObject> m_RenderObjects;
        dmRender::HMaterial             m_Material;
        dmGraphics::HVertexProgram      m_VertexProgram;
        dmGraphics::HFragmentProgram    m_FragmentProgram;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
    };

    dmGameObject::CreateResult CompSpriteNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;
        SpriteWorld* sprite_world = new SpriteWorld();

        sprite_world->m_Components.SetCapacity(sprite_context->m_MaxSpriteCount);
        sprite_world->m_Components.SetSize(sprite_context->m_MaxSpriteCount);
        memset(&sprite_world->m_Components[0], 0, sizeof(Component) * sprite_context->m_MaxSpriteCount);
        sprite_world->m_ComponentIndices.SetCapacity(sprite_context->m_MaxSpriteCount);
        sprite_world->m_RenderObjects.SetCapacity(sprite_context->m_MaxSpriteCount);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        sprite_world->m_VertexProgram = dmGraphics::NewVertexProgram(dmRender::GetGraphicsContext(render_context), SPRITE_VPC, SPRITE_VPC_SIZE);
        sprite_world->m_FragmentProgram = dmGraphics::NewFragmentProgram(dmRender::GetGraphicsContext(render_context), SPRITE_FPC, SPRITE_FPC_SIZE);

        sprite_world->m_Material = dmRender::NewMaterial(sprite_context->m_RenderContext, sprite_world->m_VertexProgram, sprite_world->m_FragmentProgram);
        SetMaterialProgramConstantType(sprite_world->m_Material, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        SetMaterialProgramConstantType(sprite_world->m_Material, dmHashString64("world"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD);

        dmRender::AddMaterialTag(sprite_world->m_Material, dmHashString32("tile"));

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT},
        };

        sprite_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), sizeof(float) * 5 * 4 * sprite_world->m_Components.Capacity(), 0x0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        *params.m_World = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        dmRender::DeleteMaterial(sprite_context->m_RenderContext, sprite_world->m_Material);
        dmGraphics::DeleteVertexProgram(sprite_world->m_VertexProgram);
        dmGraphics::DeleteFragmentProgram(sprite_world->m_FragmentProgram);
        dmGraphics::DeleteVertexDeclaration(sprite_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);

        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    bool PlayAnimation(Component* component, dmhash_t animation_id)
    {
        bool anim_found = false;
        TileSetResource* tile_set = component->m_Resource->m_TileSet;
        uint32_t n = tile_set->m_AnimationIds.Size();
        for (uint32_t i = 0; i < n; ++i)
        {
            if (animation_id == tile_set->m_AnimationIds[i])
            {
                component->m_CurrentAnimation = i;
                anim_found = true;
                break;
            }
        }
        if (anim_found)
        {
            dmGameSystemDDF::Animation* animation = &tile_set->m_TileSet->m_Animations[component->m_CurrentAnimation];
            component->m_CurrentTile = animation->m_StartTile - 1;
            component->m_PlayBackwards = 0;
            component->m_FrameTime = 1.0f / animation->m_Fps;
            component->m_FrameTimer = 0.0f;
            component->m_Playing = animation->m_Playback != dmGameSystemDDF::PLAYBACK_NONE;
        }
        return anim_found;
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
        Component* component = &sprite_world->m_Components[index];
        component->m_Instance = params.m_Instance;
        component->m_Position = params.m_Position;
        component->m_Rotation = params.m_Rotation;
        component->m_Resource = (SpriteResource*)params.m_Resource;
        component->m_ListenerInstance = 0x0;
        component->m_ListenerComponent = 0xff;
        component->m_Enabled = 1;
        PlayAnimation(component, component->m_Resource->m_DefaultAnimation);

        *params.m_UserData = (uintptr_t)component;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;
        Component* component = (Component*)*params.m_UserData;
        uint32_t index = component - &sprite_world->m_Components[0];
        memset(component, 0, sizeof(Component));
        sprite_world->m_ComponentIndices.Push(index);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static uint32_t CalculateTileCount(uint32_t tile_size, uint32_t image_size, uint32_t tile_margin, uint32_t tile_spacing)
    {
        uint32_t actual_tile_size = (2 * tile_margin + tile_spacing + tile_size);
        if (actual_tile_size > 0) {
            return (image_size + tile_spacing)/actual_tile_size;
        } else {
            return 0;
        }
    }

    dmGameObject::UpdateResult CompSpriteUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        SpriteContext* sprite_context = (SpriteContext*)params.m_Context;
        dmRender::HRenderContext render_context = sprite_context->m_RenderContext;
        SpriteWorld* sprite_world = (SpriteWorld*)params.m_World;

        struct Vertex
        {
            float x;
            float y;
            float z;
            float u;
            float v;
        };

        dmGraphics::SetVertexBufferData(sprite_world->m_VertexBuffer, 4 * sizeof(Vertex) * sprite_world->m_Components.Size(), 0x0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);
        void* vertex_buffer = dmGraphics::MapVertexBuffer(sprite_world->m_VertexBuffer, dmGraphics::BUFFER_ACCESS_WRITE_ONLY);
        if (vertex_buffer == 0x0)
        {
            dmLogError("%s", "Could not map vertex buffer when drawing sprites.");
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        uint32_t vertex_index = 0;
        sprite_world->m_RenderObjects.SetSize(0);
        for (uint32_t i = 0; i < sprite_world->m_Components.Size(); ++i)
        {
            Component* component = &sprite_world->m_Components[i];
            if (component->m_Enabled)
            {
                TileSetResource* tile_set = component->m_Resource->m_TileSet;
                dmGameSystemDDF::TileSet* tile_set_ddf = tile_set->m_TileSet;
                dmGraphics::HTexture texture = tile_set->m_Texture;
                dmGameSystemDDF::Animation* animation_ddf = &tile_set_ddf->m_Animations[component->m_CurrentAnimation];

                // Generate vertex data
                Point3 world_pos = dmGameObject::GetWorldPosition(component->m_Instance);
                Quat world_rot = dmGameObject::GetWorldRotation(component->m_Instance);
                const Quat& local_rot = component->m_Rotation;
                const Point3& local_pos = component->m_Position;
                Quat rotation = world_rot * local_rot;
                Point3 position = rotate(world_rot, Vector3(local_pos)) + world_pos;
                Matrix4 world = Matrix4::rotation(rotation);
                world *= Matrix4::scale(Vector3(tile_set_ddf->m_TileWidth, tile_set_ddf->m_TileHeight, 1.0f));

                if (!sprite_context->m_Subpixels)
                {
                    position.setX((int) position.getX());
                    position.setY((int) position.getY());
                }

                world.setCol3(Vector4(position));

                // Render object
                dmRender::RenderObject ro;
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
                ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
                ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLE_STRIP;
                ro.m_VertexStart = vertex_index;
                ro.m_VertexCount = 4;
                ro.m_Material = sprite_world->m_Material;
                ro.m_Textures[0] = texture;
                ro.m_WorldTransform = world;
                ro.m_CalculateDepthKey = 1;
                sprite_world->m_RenderObjects.Push(ro);
                dmRender::AddToRender(render_context, &sprite_world->m_RenderObjects[sprite_world->m_RenderObjects.Size() - 1]);

                uint16_t texture_width = dmGraphics::GetTextureWidth(texture);
                uint16_t texture_height = dmGraphics::GetTextureHeight(texture);
                float texture_width_recip = 1.0f / texture_width;
                float texture_height_recip = 1.0f / texture_height;
                uint32_t tiles_per_row = CalculateTileCount(tile_set_ddf->m_TileWidth, texture_width, tile_set_ddf->m_TileMargin, tile_set_ddf->m_TileSpacing);
                uint32_t tiles_per_column = CalculateTileCount(tile_set_ddf->m_TileHeight, texture_height, tile_set_ddf->m_TileMargin, tile_set_ddf->m_TileSpacing);
                uint32_t tile_count = tiles_per_row * tiles_per_column;
                uint16_t tile_x = component->m_CurrentTile % tiles_per_row;
                uint16_t tile_y = component->m_CurrentTile / tiles_per_row;
                Vertex *v = (Vertex*)((vertex_buffer)) + vertex_index;

                float tile_width = tile_set_ddf->m_TileWidth;
                float tile_height = tile_set_ddf->m_TileHeight;

                float u0 = (tile_x * tile_width) * texture_width_recip;
                float u1 = ((tile_x + 1) * tile_width) * texture_width_recip;
                if (animation_ddf->m_FlipHorizontal != 0)
                {
                    float u = u0;
                    u0 = u1;
                    u1 = u;
                }
                float v0 = (tile_y * tile_height) * texture_height_recip;
                float v1 = ((tile_y + 1) * tile_height) * texture_height_recip;
                if (animation_ddf->m_FlipVertical != 0)
                {
                    float v = v0;
                    v0 = v1;
                    v1 = v;
                }

                v[0].x = -0.5f;
                v[0].y = -0.5f;
                v[0].z = 0.0f;
                v[0].u = u0;
                v[0].v = v1;

                v[1].x = -0.5f;
                v[1].y = 0.5f;
                v[1].z = 0.0f;
                v[1].u = u0;
                v[1].v = v0;

                v[2].x = 0.5f;
                v[2].y = -0.5f;
                v[2].z = 0.0f;
                v[2].u = u1;
                v[2].v = v1;

                v[3].x = 0.5f;
                v[3].y = 0.5f;
                v[3].z = 0.0f;
                v[3].u = u1;
                v[3].v = v0;

                vertex_index += 4;

                int16_t start_tile = (int16_t)animation_ddf->m_StartTile - 1;
                int16_t end_tile = (int16_t)animation_ddf->m_EndTile - 1;
                // Stop once-animation and broadcast animation_done
                if (animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_FORWARD
                    || animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD)
                {
                    if (component->m_CurrentTile == end_tile)
                    {
                        component->m_Playing = 0;
                        if (component->m_ListenerInstance != 0x0)
                        {
                            dmhash_t message_id = dmHashString64(dmGameSystemDDF::AnimationDone::m_DDFDescriptor->m_Name);
                            dmGameSystemDDF::AnimationDone message;
                            message.m_CurrentTile = component->m_CurrentTile;
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
                                    return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
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
                // Animate
                if (component->m_Playing)
                {
                    component->m_FrameTimer += params.m_UpdateContext->m_DT;
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
                        if (current_tile < 0)
                            current_tile = tile_count - 1;
                        else if ((uint16_t)current_tile >= tile_count)
                            current_tile = 0;
                        component->m_CurrentTile = (uint16_t)current_tile;
                        if (animation_ddf->m_Playback == dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG)
                            if (current_tile == start_tile || current_tile == end_tile)
                                component->m_PlayBackwards = ~component->m_PlayBackwards;
                    }
                }
            }
        }

        if (!dmGraphics::UnmapVertexBuffer(sprite_world->m_VertexBuffer))
        {
            dmLogError("%s", "Could not unmap vertex buffer when drawing sprites.");
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        Component* component = (Component*)*params.m_UserData;
        if (params.m_Message->m_Id == dmHashString64(dmGameObjectDDF::Enable::m_DDFDescriptor->m_Name))
        {
            component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmHashString64(dmGameObjectDDF::Disable::m_DDFDescriptor->m_Name))
        {
            component->m_Enabled = 0;
        }
        else if (params.m_Message->m_Descriptor != 0x0)
        {
            if (params.m_Message->m_Id == dmHashString64(dmGameSystemDDF::PlayAnimation::m_DDFDescriptor->m_Name))
            {
                dmGameSystemDDF::PlayAnimation* ddf = (dmGameSystemDDF::PlayAnimation*)params.m_Message->m_Data;
                if (PlayAnimation(component, ddf->m_Id))
                {
                    component->m_ListenerInstance = dmGameObject::GetInstanceFromIdentifier(dmGameObject::GetCollection(component->m_Instance), params.m_Message->m_Sender.m_Path);
                    component->m_ListenerComponent = params.m_Message->m_Sender.m_Fragment;
                }
            }
        }
        else
        {
            const char* id_str = (const char*) dmHashReverse64(params.m_Message->m_Id, 0);
            LogMessageError(params.m_Message, "Unsupported sprite message '%s'.", id_str);
            return dmGameObject::UPDATE_RESULT_UNKNOWN_ERROR;
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompSpriteOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        Component* component = (Component*)*params.m_UserData;
        component->m_CurrentTile = 0;
        component->m_FrameTimer = 0.0f;
        component->m_FrameTime = 0.0f;
    }
}
