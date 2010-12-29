#include "comp_gui.h"

#include <string.h>
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <gui/gui.h>
#include <graphics/graphics_device.h>
#include <render/material.h>
#include <render/font_renderer.h>

#include "../resources/res_gui.h"

extern char GUI_ARBVP[];
extern uint32_t GUI_ARBVP_SIZE;

extern char GUI_ARBFP[];
extern uint32_t GUI_ARBFP_SIZE;

namespace dmGameSystem
{
    dmRender::HRenderType g_GuiRenderType = dmRender::INVALID_RENDER_TYPE_HANDLE;

    struct Component
    {
        dmGui::HScene                    m_Scene;
        uint16_t m_Enabled : 1;
    };

    struct GuiWorld;
    struct GuiRenderNode
    {
        dmGui::Node m_GuiNode;
        GuiWorld*   m_GuiWorld;
        GuiRenderNode(const dmGui::Node& node, GuiWorld* gui_world) : m_GuiNode(node), m_GuiWorld(gui_world) {}
    };

    struct GuiWorld
    {
        dmGui::HGui                      m_Gui;
        dmArray<Component*>              m_Components;
        dmMessage::HSocket               m_Socket;
        dmRender::HMaterial              m_Material;
        dmGraphics::HVertexProgram       m_VertexProgram;
        dmGraphics::HFragmentProgram     m_FragmentProgram;
        dmGraphics::HVertexDeclaration   m_VertexDeclaration;
        dmGraphics::HVertexBuffer        m_QuadVertexBuffer;
        dmGraphics::HTexture             m_WhiteTexture;
        dmArray<dmRender::RenderObject>  m_GuiRenderObjects;
    };

    dmGameObject::CreateResult CompGuiNewWorld(void* context, void** world)
    {
        char socket_name[32];

        GuiWorld* gui_world = new GuiWorld();
        DM_SNPRINTF(socket_name, sizeof(socket_name), "dmgui_from_%X", (unsigned int) gui_world);
        dmMessage::Result mr = dmMessage::NewSocket(socket_name, &gui_world->m_Socket);
        if (mr != dmMessage::RESULT_OK)
        {
            dmLogFatal("Unable to create gui socket: %s (%d)", socket_name, mr);
            delete gui_world;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        dmGui::NewGuiParams gui_params;
        gui_params.m_Socket = gui_world->m_Socket;
        gui_world->m_Gui = dmGui::New(&gui_params);
        gui_world->m_Components.SetCapacity(16);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        gui_world->m_VertexProgram = dmGraphics::NewVertexProgram(GUI_ARBVP, GUI_ARBVP_SIZE);
        gui_world->m_FragmentProgram = dmGraphics::NewFragmentProgram(GUI_ARBFP, GUI_ARBFP_SIZE);

        gui_world->m_Material = dmRender::NewMaterial();
        SetMaterialVertexProgramConstantType(gui_world->m_Material, 0, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        SetMaterialVertexProgramConstantType(gui_world->m_Material, 4, dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD);

        dmRender::AddMaterialTag(gui_world->m_Material, dmHashString32("gui"));

        dmRender::SetMaterialVertexProgram(gui_world->m_Material, gui_world->m_VertexProgram);
        dmRender::SetMaterialFragmentProgram(gui_world->m_Material, gui_world->m_FragmentProgram);

        dmGraphics::VertexElement ve[] =
        {
                {0, 3, dmGraphics::TYPE_FLOAT, 0, 0},
                {1, 2, dmGraphics::TYPE_FLOAT, 0, 0},
        };

        gui_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        float ext = 0.5f;
        float quad[] = { -ext,-ext, 0, 0, 0,
                         ext, ext, 0, 0, 1,
                         -ext, ext, 0, 1, 1,

                         -ext, -ext, 0, 0, 0,
                          ext, -ext, 0, 1, 1,
                          ext, ext, 0, 1, 0 };

        gui_world->m_QuadVertexBuffer = dmGraphics::NewVertexBuffer(sizeof(float) * 5 * 6, (void*) quad, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        uint8_t white_texture[] = { 0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff };

        dmGraphics::TextureParams params;
        params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        params.m_Data = white_texture;
        params.m_DataSize = sizeof(white_texture);
        params.m_Width = 2;
        params.m_Height = 2;
        gui_world->m_WhiteTexture = dmGraphics::NewTexture(params);

        *world = gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiDeleteWorld(void* context, void* world)
    {
        GuiWorld* gui_world = (GuiWorld*)world;
        if (0 < gui_world->m_Components.Size())
        {
            dmLogWarning("%d gui component(s) were not destroyed at gui context destruction.", gui_world->m_Components.Size());
            for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
            {
                delete gui_world->m_Components[i];
            }
        }
        dmRender::DeleteMaterial(gui_world->m_Material);
        dmGraphics::DeleteVertexProgram(gui_world->m_VertexProgram);
        dmGraphics::DeleteFragmentProgram(gui_world->m_FragmentProgram);
        dmGraphics::DeleteVertexDeclaration(gui_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(gui_world->m_QuadVertexBuffer);
        dmGraphics::DeleteTexture(gui_world->m_WhiteTexture);

        dmGui::Delete(gui_world->m_Gui);
        dmMessage::DeleteSocket(gui_world->m_Socket);
        delete gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiCreate(dmGameObject::HCollection collection,
                                             dmGameObject::HInstance instance,
                                             void* resource,
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data)
    {
        GuiWorld* gui_world = (GuiWorld*)world;

        GuiScenePrototype* scene_prototype = (GuiScenePrototype*) resource;

        dmGui::NewSceneParams params;
        params.m_MaxNodes = 256;
        params.m_MaxAnimations = 1024;
        params.m_UserData = instance;
        dmGui::HScene scene = dmGui::NewScene(gui_world->m_Gui, &params);

        // NOTE: We ignore errors here in order to be able to reload invalid scripts
        dmGui::SetSceneScript(scene, scene_prototype->m_Script, strlen(scene_prototype->m_Script), scene_prototype->m_Path);

        Component* gui_component = new Component();
        gui_component->m_Scene = scene;
        gui_component->m_Enabled = 1;

        for (uint32_t i = 0; i < scene_prototype->m_Fonts.Size(); ++i)
        {
            dmGui::AddFont(scene, scene_prototype->m_SceneDesc->m_Fonts[i].m_Name, (void*)scene_prototype->m_Fonts[i]);
        }

        for (uint32_t i = 0; i < scene_prototype->m_Textures.Size(); ++i)
        {
            dmGui::AddTexture(scene, scene_prototype->m_SceneDesc->m_Textures[i], (void*)scene_prototype->m_Textures[i]);
        }

        *user_data = (uintptr_t)gui_component;
        gui_world->m_Components.Push(gui_component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiInit(dmGameObject::HCollection collection,
                                           dmGameObject::HInstance instance,
                                           void* context,
                                           uintptr_t* user_data)
    {
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiDestroy(dmGameObject::HCollection collection,
                                              dmGameObject::HInstance instance,
                                              void* world,
                                              void* context,
                                              uintptr_t* user_data)
    {
        GuiWorld* gui_world = (GuiWorld*)world;
        Component* gui_component = (Component*)*user_data;
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            if (gui_world->m_Components[i] == gui_component)
            {
                dmGui::DeleteScene(gui_component->m_Scene);
                delete gui_component;
                gui_world->m_Components.EraseSwap(i);
                break;
            }
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    void DispatchGui(dmMessage::Message *message_object, void* user_ptr)
    {
        DM_PROFILE(Game, "DispatchGui");

        dmGameObject::HRegister regist = (dmGameObject::HRegister) user_ptr;
        dmGui::MessageData* gui_message = (dmGui::MessageData*) message_object->m_Data;
        dmGameObject::HInstance instance = (dmGameObject::HInstance) dmGui::GetSceneUserData(gui_message->m_Scene);
        assert(instance);

        dmGameObject::InstanceMessageData data;
        data.m_Component = 0xff;
        data.m_DDFDescriptor = 0x0;
        data.m_MessageId = gui_message->m_MessageId;
        data.m_Instance = instance;
        dmMessage::HSocket socket = dmGameObject::GetReplyMessageSocket(regist);
        uint32_t message = dmGameObject::GetMessageId(regist);
        dmMessage::Post(socket, message, &data, sizeof(dmGameObject::InstanceMessageData));
    }

    struct RenderGuiContext
    {
        dmRender::HRenderContext m_RenderContext;
        GuiWorld*                m_GuiWorld;
    };

    void RenderNode(dmGui::HScene scene,
                    const dmGui::Node* nodes,
                    uint32_t node_count,
                    void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        if (gui_world->m_GuiRenderObjects.Remaining() < node_count)
        {
            uint32_t extra = node_count - gui_world->m_GuiRenderObjects.Remaining() + 64;
            gui_world->m_GuiRenderObjects.OffsetCapacity(extra);
        }

        for (uint32_t i = 0; i < node_count; ++i)
        {
            const dmGui::Node* n = &nodes[i];
            const Vector4& position = n->m_Properties[dmGui::PROPERTY_POSITION];
            const Vector4& rotation = n->m_Properties[dmGui::PROPERTY_ROTATION];
            const Vector4& extents = n->m_Properties[dmGui::PROPERTY_EXTENTS];
            const Vector4& color = n->m_Properties[dmGui::PROPERTY_COLOR];

            dmRender::RenderObject ro;

            dmGui::BlendMode blend_mode = (dmGui::BlendMode) n->m_BlendMode;
            switch (blend_mode)
            {
                case dmGui::BLEND_MODE_ALPHA:
                    ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                    ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                break;

                case dmGui::BLEND_MODE_ADD:
                    ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                    ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                break;

                case dmGui::BLEND_MODE_ADD_ALPHA:
                    ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                    ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_SRC_ALPHA;
                break;

                case dmGui::BLEND_MODE_MULT:
                    ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ZERO;
                    ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_SRC_COLOR;
                break;

                default:
                    dmLogError("Unknown blend mode: %d\n", blend_mode);
                    assert(0);
                break;
            }

            ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
            ro.m_VertexBuffer = gui_world->m_QuadVertexBuffer;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = 2 * 3;
            ro.m_Material = gui_world->m_Material;

            // Set default texture
            ro.m_Texture = gui_world->m_WhiteTexture;
            ro.m_Texture = (dmGraphics::HTexture) n->m_Texture;

            if (n->m_NodeType == dmGui::NODE_TYPE_BOX)
            {
                const float deg_to_rad = 3.1415926f / 180.0f;
                Matrix4 m = Matrix4::translation(position.getXYZ()) *
                            Matrix4::rotationZ(rotation.getZ() * deg_to_rad) *
                            Matrix4::rotationY(rotation.getY() * deg_to_rad) *
                            Matrix4::rotationX(rotation.getX() * deg_to_rad) *
                            Matrix4::scale(Vector3(extents.getX(), extents.getY(), 1));

                ro.m_WorldTransform = m;
                dmRender::SetFragmentConstant(&ro, 0, color);
                gui_world->m_GuiRenderObjects.Push(ro);
                dmRender::AddToRender(gui_context->m_RenderContext, &gui_world->m_GuiRenderObjects[gui_world->m_GuiRenderObjects.Size()-1]);
            }
            else if (n->m_NodeType == dmGui::NODE_TYPE_TEXT)
            {
                dmRender::DrawTextParams params;
                params.m_FaceColor = color;
                params.m_Text = n->m_Text;
                params.m_X = position.getX();
                params.m_Y = position.getY();
                dmRender::DrawText(gui_context->m_RenderContext, (dmRender::HFont) n->m_Font, params);
            }
        }
    }

    dmGameObject::UpdateResult CompGuiUpdate(dmGameObject::HCollection collection,
                                             const dmGameObject::UpdateContext* update_context,
                                             void* world,
                                             void* context)
    {
        GuiWorld* gui_world = (GuiWorld*)world;
        dmRender::HRenderContext render_context = (dmRender::HRenderContext)context;

        dmGameObject::HRegister regist = dmGameObject::GetRegister(collection);
        dmMessage::Dispatch(gui_world->m_Socket, &DispatchGui, regist);

        // update
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            if (gui_world->m_Components[i]->m_Enabled)
                dmGui::UpdateScene(gui_world->m_Components[i]->m_Scene, update_context->m_DT);
        }

        RenderGuiContext render_gui_context;
        render_gui_context.m_RenderContext = render_context;
        render_gui_context.m_GuiWorld = gui_world;

        gui_world->m_GuiRenderObjects.SetSize(0);
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            Component* c = gui_world->m_Components[i];
            if (c->m_Enabled)
                dmGui::RenderScene(c->m_Scene, &RenderNode, &render_gui_context);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompGuiOnMessage(dmGameObject::HInstance instance,
            const dmGameObject::InstanceMessageData* message_data,
            void* context,
            uintptr_t* user_data)
    {
        Component* gui_component = (Component*)*user_data;
        if (message_data->m_MessageId == dmHashString32("enable"))
        {
            gui_component->m_Enabled = 1;
        }
        else if (message_data->m_MessageId == dmHashString32("disable"))
        {
            gui_component->m_Enabled = 0;
        }
        else if (message_data->m_DDFDescriptor)
        {
            dmGui::DispatchMessage(gui_component->m_Scene, message_data->m_MessageId, (const void*) message_data->m_Buffer, message_data->m_DDFDescriptor);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::InputResult CompGuiOnInput(dmGameObject::HInstance instance,
            const dmGameObject::InputAction* input_action,
            void* context,
            uintptr_t* user_data)
    {
        Component* gui_component = (Component*)*user_data;

        dmGui::InputAction gui_input_action;
        gui_input_action.m_ActionId = input_action->m_ActionId;
        gui_input_action.m_Value = input_action->m_Value;
        gui_input_action.m_Pressed = input_action->m_Pressed;
        gui_input_action.m_Released = input_action->m_Released;
        gui_input_action.m_Repeated = input_action->m_Repeated;

        dmGui::DispatchInput(gui_component->m_Scene, &gui_input_action, 1);
        return dmGameObject::INPUT_RESULT_IGNORED;
    }
}
