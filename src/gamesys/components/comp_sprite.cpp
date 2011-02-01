#include "comp_sprite.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>

#include <graphics/graphics.h>

#include <render/render.h>
#include <render/material.h>

#include "../resources/res_sprite.h"
#include "../gamesys.h"

#include "sprite_ddf.h"

extern char SPRITE_VPC[];
extern uint32_t SPRITE_VPC_SIZE;

extern char SPRITE_FPC[];
extern uint32_t SPRITE_FPC_SIZE;

namespace dmGameSystem
{
    struct Component
    {
        dmGameObject::HInstance     m_Instance;
        SpriteResource*             m_Resource;
        dmGameSystemDDF::Playback   m_Playback;
        float                       m_FrameTime;
        float                       m_FrameTimer;
        uint16_t                    m_StartFrame;
        uint16_t                    m_EndFrame;
        uint16_t                    m_CurrentFrame;
        uint16_t                    m_Enabled : 1;
        uint16_t                    m_PlayBackwards : 1;
    };

    struct SpriteWorld
    {
        dmArray<Component*>             m_Components;
        dmRender::HMaterial             m_Material;
        dmGraphics::HVertexProgram      m_VertexProgram;
        dmGraphics::HFragmentProgram    m_FragmentProgram;
        dmGraphics::HVertexDeclaration  m_VertexDeclaration;
        dmGraphics::HVertexBuffer       m_VertexBuffer;
        dmArray<dmRender::RenderObject> m_RenderObjects;
    };

    dmGameObject::CreateResult CompSpriteNewWorld(void* context, void** world)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        SpriteWorld* sprite_world = new SpriteWorld();

        sprite_world->m_Components.SetCapacity(16);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        sprite_world->m_VertexProgram = dmGraphics::NewVertexProgram(dmRender::GetGraphicsContext(render_context), SPRITE_VPC, SPRITE_VPC_SIZE);
        sprite_world->m_FragmentProgram = dmGraphics::NewFragmentProgram(dmRender::GetGraphicsContext(render_context), SPRITE_FPC, SPRITE_FPC_SIZE);

        sprite_world->m_Material = dmRender::NewMaterial();
        SetMaterialVertexProgramConstantType(sprite_world->m_Material, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);

        dmRender::AddMaterialTag(sprite_world->m_Material, dmHashString32("sprite"));

        dmRender::SetMaterialVertexProgram(sprite_world->m_Material, sprite_world->m_VertexProgram);
        dmRender::SetMaterialFragmentProgram(sprite_world->m_Material, sprite_world->m_FragmentProgram);

        dmGraphics::VertexElement ve[] =
        {
                {0, 3, dmGraphics::TYPE_FLOAT, 0, 0},
                {1, 2, dmGraphics::TYPE_FLOAT, 0, 0},
        };

        sprite_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(render_context), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        sprite_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(render_context), sizeof(float) * 5 * 4 * sprite_world->m_Components.Capacity(), 0x0, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        // TODO: Configurable
        sprite_world->m_RenderObjects.SetCapacity(sprite_world->m_Components.Capacity());

        *world = sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDeleteWorld(void* context, void* world)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)world;
        if (0 < sprite_world->m_Components.Size())
        {
            dmLogWarning("%d gui component(s) were not destroyed at sprite world destruction.", sprite_world->m_Components.Size());
            for (uint32_t i = 0; i < sprite_world->m_Components.Size(); ++i)
            {
                delete sprite_world->m_Components[i];
            }
        }
        dmRender::DeleteMaterial(sprite_world->m_Material);
        dmGraphics::DeleteVertexProgram(sprite_world->m_VertexProgram);
        dmGraphics::DeleteFragmentProgram(sprite_world->m_FragmentProgram);
        dmGraphics::DeleteVertexDeclaration(sprite_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(sprite_world->m_VertexBuffer);

        delete sprite_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteCreate(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* resource,
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)world;

        if (sprite_world->m_Components.Full())
        {
            dmLogError("Sprite could not be created since the sprite buffer is full (%d).", sprite_world->m_Components.Capacity());
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        Component* component = new Component();
        memset(component, 0, sizeof(Component));
        component->m_Instance = instance;
        component->m_Resource = (SpriteResource*)resource;
        component->m_Enabled = 1;

        *user_data = (uintptr_t)component;
        sprite_world->m_Components.Push(component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompSpriteDestroy(dmGameObject::HCollection collection,
                                              dmGameObject::HInstance instance,
                                              void* world,
                                              void* context,
                                              uintptr_t* user_data)
    {
        SpriteWorld* sprite_world = (SpriteWorld*)world;
        Component* component = (Component*)*user_data;
        for (uint32_t i = 0; i < sprite_world->m_Components.Size(); ++i)
        {
            if (sprite_world->m_Components[i] == component)
            {
                delete component;
                sprite_world->m_Components.EraseSwap(i);
                break;
            }
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompSpriteUpdate(dmGameObject::HCollection collection,
                                             const dmGameObject::UpdateContext* update_context,
                                             void* world,
                                             void* context)
    {
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;
        SpriteWorld* sprite_world = (SpriteWorld*)world;

        Point3 positions[] =
        {
            Point3(0.0f, 0.0f, 0.0f),
            Point3(0.5f, 0.0f, 0.0f),
            Point3(0.5f, 0.5f, 0.0f),
            Point3(0.0f, 0.5f, 0.0f)
        };
        float uvs[][2] =
        {
            {0.0f, 0.0f},
            {1.0f, 0.0f},
            {1.0f, 1.0f},
            {0.0f, 1.0f}
        };
        struct Vertex
        {
            Point3 position;
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
            Component* component = sprite_world->m_Components[i];
            if (component->m_Enabled)
            {
                dmGameSystemDDF::SpriteDesc* ddf = component->m_Resource->m_DDF;
                dmGraphics::HTexture texture = component->m_Resource->m_Texture;

                // Render object
                dmRender::RenderObject ro;
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                ro.m_VertexDeclaration = sprite_world->m_VertexDeclaration;
                ro.m_VertexBuffer = sprite_world->m_VertexBuffer;
                ro.m_PrimitiveType = dmGraphics::PRIMITIVE_QUADS;
                ro.m_VertexStart = vertex_index;
                ro.m_VertexCount = 4;
                ro.m_Material = sprite_world->m_Material;
                ro.m_Textures[0] = texture;
                sprite_world->m_RenderObjects.Push(ro);
                dmRender::AddToRender(render_context, &sprite_world->m_RenderObjects[sprite_world->m_RenderObjects.Size() - 1]);

                // Generate vertex data
                Matrix4 world = Matrix4::scale(Vector3(ddf->m_TileWidth, ddf->m_TileHeight, 1.0f));
                world *= Matrix4::rotation(dmGameObject::GetWorldRotation(component->m_Instance));
                world.setCol3(Vector4(dmGameObject::GetWorldPosition(component->m_Instance)));
                float tile_uv_width = ddf->m_TileWidth / (float)dmGraphics::GetTextureWidth(texture);
                float tile_uv_height = ddf->m_TileWidth / (float)dmGraphics::GetTextureWidth(texture);
                uint16_t tile_x = component->m_CurrentFrame % ddf->m_FramesPerRow;
                uint16_t tile_y = component->m_CurrentFrame / ddf->m_FramesPerRow;
                Vertex* v = (Vertex*)vertex_buffer + vertex_index;
                for (uint32_t j = 0; j < 4; ++j)
                {
                    v[j].position = Point3((world * positions[j]).getXYZ());
                    v[j].u = (uvs[j][0] + tile_x) * tile_uv_width;
                    v[j].v = (uvs[j][1] + tile_y) * tile_uv_height;
                }
                vertex_index += 4;

                // Animate
                component->m_FrameTimer += update_context->m_DT;
                if (component->m_FrameTimer >= component->m_FrameTime)
                {
                    component->m_FrameTimer -= component->m_FrameTime;
                    int16_t current_frame = (int16_t)component->m_CurrentFrame;
                    switch (component->m_Playback)
                    {
                        case dmGameSystemDDF::PLAYBACK_ONCE_FORWARD:
                            if (current_frame != component->m_EndFrame)
                                ++current_frame;
                            break;
                        case dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD:
                            if (current_frame != component->m_EndFrame)
                                --current_frame;
                            break;
                        case dmGameSystemDDF::PLAYBACK_LOOP_FORWARD:
                            if (current_frame == component->m_EndFrame)
                                current_frame = component->m_StartFrame;
                            else
                                ++current_frame;
                            break;
                        case dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD:
                            if (current_frame == component->m_EndFrame)
                                current_frame = component->m_StartFrame;
                            else
                                --current_frame;
                            break;
                        case dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG:
                            if (current_frame == component->m_StartFrame || current_frame == component->m_EndFrame)
                                component->m_PlayBackwards = ~component->m_PlayBackwards;
                            if (component->m_PlayBackwards)
                                --current_frame;
                            else
                                ++current_frame;
                            break;
                    }
                    if (current_frame < 0)
                        current_frame = ddf->m_FrameCount - 1;
                    else if ((uint16_t)current_frame >= ddf->m_FrameCount)
                        current_frame = 0;
                    component->m_CurrentFrame = (uint16_t)current_frame;
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

    dmGameObject::UpdateResult CompSpriteOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        Component* component = (Component*)*user_data;
        if (message_data->m_MessageId == dmHashString64("enable"))
        {
            component->m_Enabled = 1;
        }
        else if (message_data->m_MessageId == dmHashString64("disable"))
        {
            component->m_Enabled = 0;
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    void CompSpriteOnReload(dmGameObject::HInstance instance,
            void* resource,
            void* world,
            void* context,
            uintptr_t* user_data)
    {
        Component* component = (Component*)*user_data;
        component->m_StartFrame = 0;
        component->m_EndFrame = 0;
        component->m_CurrentFrame = 0;
        component->m_FrameTimer = 0.0f;
        component->m_FrameTime = 0.0f;
    }
}
