#include "comp_gui.h"

#include <string.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>

#include <gui/gui.h>

#include <graphics/graphics.h>

#include <render/material.h>
#include <render/font_renderer.h>

#include "../resources/res_gui.h"
#include "../gamesys.h"

extern char GUI_VPC[];
extern uint32_t GUI_VPC_SIZE;

extern char GUI_FPC[];
extern uint32_t GUI_FPC_SIZE;

namespace dmGameSystem
{
    dmRender::HRenderType g_GuiRenderType = dmRender::INVALID_RENDER_TYPE_HANDLE;

    struct Component
    {
        dmGui::HScene           m_Scene;
        dmGameObject::HInstance m_Instance;
        uint8_t                 m_ComponentIndex;
        uint8_t                 m_Enabled : 1;
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
        dmArray<Component*>              m_Components;
        dmRender::HMaterial              m_Material;
        dmGraphics::HVertexProgram       m_VertexProgram;
        dmGraphics::HFragmentProgram     m_FragmentProgram;
        dmGraphics::HVertexDeclaration   m_VertexDeclaration;
        dmGraphics::HVertexBuffer        m_QuadVertexBuffer;
        dmGraphics::HTexture             m_WhiteTexture;
        dmArray<dmRender::RenderObject>  m_GuiRenderObjects;
    };

    dmGameObject::CreateResult CompGuiNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        GuiRenderContext* gui_render_context = (GuiRenderContext*)params.m_Context;
        GuiWorld* gui_world = new GuiWorld();

        gui_world->m_Components.SetCapacity(16);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        gui_world->m_VertexProgram = dmGraphics::NewVertexProgram(dmRender::GetGraphicsContext(gui_render_context->m_RenderContext), GUI_VPC, GUI_VPC_SIZE);
        gui_world->m_FragmentProgram = dmGraphics::NewFragmentProgram(dmRender::GetGraphicsContext(gui_render_context->m_RenderContext), GUI_FPC, GUI_FPC_SIZE);

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

        gui_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(gui_render_context->m_RenderContext), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        float ext = 0.5f;
        float quad[] = { -ext,-ext, 0, 0, 0,
                         ext, -ext, 0, 1, 0,
                         ext, ext, 0, 1, 1,
                         -ext, ext, 0, 0, 1 };

        gui_world->m_QuadVertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(gui_render_context->m_RenderContext), sizeof(float) * 5 * 4, (void*) quad, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

        uint8_t white_texture[] = { 0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff,
                                    0xff, 0xff, 0xff, 0xff };

        dmGraphics::TextureParams tex_params;
        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        tex_params.m_Data = white_texture;
        tex_params.m_DataSize = sizeof(white_texture);
        tex_params.m_Width = 2;
        tex_params.m_Height = 2;
        gui_world->m_WhiteTexture = dmGraphics::NewTexture(dmRender::GetGraphicsContext(gui_render_context->m_RenderContext), tex_params);

        // TODO: Configurable
        gui_world->m_GuiRenderObjects.SetCapacity(32);

        *params.m_World = gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
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

        delete gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiCreate(const dmGameObject::ComponentCreateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiRenderContext* gui_render_context = (GuiRenderContext*)params.m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(gui_render_context->m_RenderContext);

        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource;

        Component* gui_component = new Component();
        gui_component->m_Instance = params.m_Instance;
        gui_component->m_ComponentIndex = params.m_ComponentIndex;
        gui_component->m_Enabled = 1;

        dmGui::NewSceneParams scene_params;
        scene_params.m_MaxNodes = 256;
        scene_params.m_MaxAnimations = 1024;
        scene_params.m_UserData = gui_component;
        gui_component->m_Scene = dmGui::NewScene(scene_resource->m_GuiContext, &scene_params);
        dmGui::HScene scene = gui_component->m_Scene;
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;

        dmGui::SetSceneScript(scene, scene_resource->m_Script);
        dmGui::SetReferenceResolution(scene, scene_desc->m_ReferenceWidth, scene_desc->m_ReferenceHeight);

        uint32_t physical_width = dmGraphics::GetWindowWidth(graphics_context);
        uint32_t physical_height = dmGraphics::GetWindowHeight(graphics_context);
        dmGui::SetPhysicalResolution(scene, physical_width, physical_height);

        for (uint32_t i = 0; i < scene_resource->m_FontMaps.Size(); ++i)
        {
            dmGui::AddFont(scene, scene_desc->m_Fonts[i].m_Name, (void*)scene_resource->m_FontMaps[i]);
        }

        for (uint32_t i = 0; i < scene_resource->m_Textures.Size(); ++i)
        {
            dmGui::AddTexture(scene, scene_desc->m_Textures[i].m_Name, (void*)scene_resource->m_Textures[i]);
        }

        bool error = false;
        for (uint32_t i = 0; i < scene_desc->m_Nodes.m_Count; ++i)
        {
            const dmGuiDDF::NodeDesc* node_desc = &scene_desc->m_Nodes[i];

            // NOTE: We assume that the enums in dmGui and dmGuiDDF have the same values
            dmGui::NodeType type = (dmGui::NodeType) node_desc->m_Type;
            dmGui::BlendMode blend_mode = (dmGui::BlendMode) node_desc->m_BlendMode;

            Vector4 position = node_desc->m_Position;
            Vector4 extents = node_desc->m_Size;
            dmGui::HNode n = dmGui::NewNode(scene, Point3(position.getXYZ()), Vector3(extents.getXYZ()), type);
            if (n)
            {
                if (node_desc->m_Type == dmGuiDDF::NodeDesc::TYPE_TEXT)
                {
                    dmGui::SetNodeText(scene, n, node_desc->m_Text);
                    dmGui::SetNodeFont(scene, n, node_desc->m_Font);
                }

                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_ROTATION, node_desc->m_Rotation);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SCALE, node_desc->m_Rotation);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_COLOR, node_desc->m_Color);
                dmGui::SetNodeBlendMode(scene, n, blend_mode);
            }
            else
            {
                error = true;
                break;
            }
        }

        if (error)
        {
            dmGui::DeleteScene(gui_component->m_Scene);
            delete gui_component;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        else
        {
            *params.m_UserData = (uintptr_t)gui_component;
            gui_world->m_Components.Push(gui_component);
            return dmGameObject::CREATE_RESULT_OK;
        }
    }

    dmGameObject::CreateResult CompGuiDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        Component* gui_component = (Component*)*params.m_UserData;
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

    dmGameObject::CreateResult CompGuiInit(const dmGameObject::ComponentInitParams& params)
    {
        Component* gui_component = (Component*)*params.m_UserData;
        dmGui::Result result = dmGui::InitScene(gui_component->m_Scene);
        if (result != dmGui::RESULT_OK)
        {
            // TODO: Translate result
            dmLogError("Error when initializing gui component: %d.", result);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiFinal(const dmGameObject::ComponentFinalParams& params)
    {
        Component* gui_component = (Component*)*params.m_UserData;
        dmGui::Result result = dmGui::FinalScene(gui_component->m_Scene);
        if (result != dmGui::RESULT_OK)
        {
            // TODO: Translate result
            dmLogError("Error when finalizing gui component: %d.", result);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
        return dmGameObject::CREATE_RESULT_OK;
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
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_QUADS;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = 2 * 3;
            ro.m_Material = gui_world->m_Material;

            // Set default texture
            if (n->m_Texture)
                ro.m_Textures[0] = (dmGraphics::HTexture) n->m_Texture;
            else
                ro.m_Textures[0] = gui_world->m_WhiteTexture;

            if (n->m_NodeType == dmGui::NODE_TYPE_BOX)
            {
                const float deg_to_rad = 3.1415926f / 180.0f;
                Matrix4 m = Matrix4::translation(position.getXYZ()) *
                            Matrix4::rotationZ(rotation.getZ() * deg_to_rad) *
                            Matrix4::rotationY(rotation.getY() * deg_to_rad) *
                            Matrix4::rotationX(rotation.getX() * deg_to_rad) *
                            Matrix4::scale(Vector3(extents.getX(), extents.getY(), 1));

                ro.m_WorldTransform = m;
                dmRender::EnableRenderObjectFragmentConstant(&ro, 0, color);
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
                dmRender::DrawText(gui_context->m_RenderContext, (dmRender::HFontMap) n->m_Font, params);
            }
        }
    }

    dmGameObject::UpdateResult CompGuiUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiRenderContext* gui_render_context = (GuiRenderContext*)params.m_Context;

        // update
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            if (gui_world->m_Components[i]->m_Enabled)
                dmGui::UpdateScene(gui_world->m_Components[i]->m_Scene, params.m_UpdateContext->m_DT);
        }

        RenderGuiContext render_gui_context;
        render_gui_context.m_RenderContext = gui_render_context->m_RenderContext;
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

    dmGameObject::UpdateResult CompGuiOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        Component* gui_component = (Component*)*params.m_UserData;
        if (params.m_Message->m_Id == dmHashString64("enable"))
        {
            gui_component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmHashString64("disable"))
        {
            gui_component->m_Enabled = 0;
        }
        else
        {
            dmGui::DispatchMessage(gui_component->m_Scene, params.m_Message);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::InputResult CompGuiOnInput(const dmGameObject::ComponentOnInputParams& params)
    {
        Component* gui_component = (Component*)*params.m_UserData;

        if (gui_component->m_Enabled)
        {
            dmGui::InputAction gui_input_action;
            gui_input_action.m_ActionId = params.m_InputAction->m_ActionId;
            gui_input_action.m_Value = params.m_InputAction->m_Value;
            gui_input_action.m_Pressed = params.m_InputAction->m_Pressed;
            gui_input_action.m_Released = params.m_InputAction->m_Released;
            gui_input_action.m_Repeated = params.m_InputAction->m_Repeated;

            dmGui::DispatchInput(gui_component->m_Scene, &gui_input_action, 1);
        }
        return dmGameObject::INPUT_RESULT_IGNORED;
    }

    void CompGuiOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource;
        Component* gui_component = (Component*)*params.m_UserData;
        dmGui::ClearTextures(gui_component->m_Scene);
        dmGui::ClearFonts(gui_component->m_Scene);
        dmGui::SetSceneScript(gui_component->m_Scene, scene_resource->m_Script);
        for (uint32_t i = 0; i < scene_resource->m_FontMaps.Size(); ++i)
        {
            dmGui::AddFont(gui_component->m_Scene, scene_resource->m_SceneDesc->m_Fonts[i].m_Name, (void*)scene_resource->m_FontMaps[i]);
        }
        for (uint32_t i = 0; i < scene_resource->m_Textures.Size(); ++i)
        {
            dmGui::AddTexture(gui_component->m_Scene, scene_resource->m_SceneDesc->m_Textures[i].m_Name, (void*)scene_resource->m_Textures[i]);
        }
    }

    void GuiGetURLCallback(dmGui::HScene scene, dmMessage::URL* url)
    {
        Component* component = (Component*)dmGui::GetSceneUserData(scene);
        url->m_Socket = dmGameObject::GetMessageSocket(dmGameObject::GetCollection(component->m_Instance));
        url->m_Path = dmGameObject::GetIdentifier(component->m_Instance);
        dmGameObject::Result result = dmGameObject::GetComponentId(component->m_Instance, component->m_ComponentIndex, &url->m_Fragment);
        if (result != dmGameObject::RESULT_OK)
        {
            dmLogError("Could not find gui component: %d", result);
        }
    }

    uintptr_t GuiGetUserDataCallback(dmGui::HScene scene)
    {
        Component* component = (Component*)dmGui::GetSceneUserData(scene);
        return (uintptr_t)component->m_Instance;
    }

    dmhash_t GuiResolvePathCallback(dmGui::HScene scene, const char* path, uint32_t path_size)
    {
        Component* component = (Component*)dmGui::GetSceneUserData(scene);
        if (path_size > 0)
        {
            return dmGameObject::GetAbsoluteIdentifier(component->m_Instance, path, path_size);
        }
        else
        {
            return dmGameObject::GetIdentifier(component->m_Instance);
        }
    }
}
