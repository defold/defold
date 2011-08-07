#include <string.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>

#include <graphics/graphics.h>

#include "comp_gui.h"

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

    struct GuiWorld;
    struct GuiRenderNode
    {
        dmGui::Node m_GuiNode;
        GuiWorld*   m_GuiWorld;
        GuiRenderNode(const dmGui::Node& node, GuiWorld* gui_world) : m_GuiNode(node), m_GuiWorld(gui_world) {}
    };

    dmGameObject::CreateResult CompGuiNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        GuiContext* gui_context = (GuiContext*)params.m_Context;
        GuiWorld* gui_world = new GuiWorld();
        if (!gui_context->m_Worlds.Full())
        {
            gui_context->m_Worlds.Push(gui_world);
        }
        else
        {
            dmLogWarning("The gui world could not be stored since the buffer is full (%d). Reload will not work for the scenes in this world.", gui_context->m_Worlds.Size());
        }

        gui_world->m_Components.SetCapacity(16);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        gui_world->m_VertexProgram = dmGraphics::NewVertexProgram(dmRender::GetGraphicsContext(gui_context->m_RenderContext), GUI_VPC, GUI_VPC_SIZE);
        gui_world->m_FragmentProgram = dmGraphics::NewFragmentProgram(dmRender::GetGraphicsContext(gui_context->m_RenderContext), GUI_FPC, GUI_FPC_SIZE);

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

        gui_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(gui_context->m_RenderContext), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));

        float quad[] = { 0, 0, 0, 0, 1,
                         0, 1, 0, 0, 0,
                         1, 0, 0, 1, 1,
                         1, 1, 0, 1, 0 };

        gui_world->m_QuadVertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(gui_context->m_RenderContext), sizeof(quad), (void*) quad, dmGraphics::BUFFER_USAGE_STATIC_DRAW);

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
        gui_world->m_WhiteTexture = dmGraphics::NewTexture(dmRender::GetGraphicsContext(gui_context->m_RenderContext), tex_params);

        // TODO: Configurable
        gui_world->m_GuiRenderObjects.SetCapacity(32);

        *params.m_World = gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::CreateResult CompGuiDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiContext* gui_context = (GuiContext*)params.m_Context;
        for (uint32_t i = 0; i < gui_context->m_Worlds.Size(); ++i)
        {
            if (gui_world == gui_context->m_Worlds[i])
            {
                gui_context->m_Worlds.EraseSwap(i);
            }
        }
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

    bool SetupGuiScene(dmGui::HScene scene, GuiSceneResource* scene_resource)
    {
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;
        dmGui::SetSceneScript(scene, scene_resource->m_Script);
        dmGui::SetReferenceResolution(scene, scene_desc->m_ReferenceWidth, scene_desc->m_ReferenceHeight);

        bool result = true;

        for (uint32_t i = 0; i < scene_resource->m_FontMaps.Size(); ++i)
        {
            dmGui::AddFont(scene, scene_desc->m_Fonts[i].m_Name, (void*)scene_resource->m_FontMaps[i]);
        }

        for (uint32_t i = 0; i < scene_resource->m_Textures.Size(); ++i)
        {
            dmGui::AddTexture(scene, scene_desc->m_Textures[i].m_Name, (void*)scene_resource->m_Textures[i]);
        }

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
                if (node_desc->m_Id)
                {
                    dmGui::SetNodeId(scene, n, node_desc->m_Id);
                }
                if (node_desc->m_Texture != 0x0 && *node_desc->m_Texture != '\0')
                {
                    dmGui::Result gui_result = dmGui::SetNodeTexture(scene, n, node_desc->m_Texture);
                    if (gui_result != dmGui::RESULT_OK)
                    {
                        dmLogError("The texture '%s' could not be set for the '%s', result: %d.", node_desc->m_Texture, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", gui_result);
                        result = false;
                    }
                }

                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_ROTATION, node_desc->m_Rotation);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SCALE, node_desc->m_Scale);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_COLOR, node_desc->m_Color);
                dmGui::SetNodeBlendMode(scene, n, blend_mode);
                dmGui::SetNodePivot(scene, n, (dmGui::Pivot) node_desc->m_Pivot);
                dmGui::SetNodeXAnchor(scene, n, (dmGui::XAnchor) node_desc->m_Xanchor);
                dmGui::SetNodeYAnchor(scene, n, (dmGui::YAnchor) node_desc->m_Yanchor);
            }
            else
            {
                result = false;
            }
        }
        return result;
    }

    dmGameObject::CreateResult CompGuiCreate(const dmGameObject::ComponentCreateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiContext* gui_context = (GuiContext*)params.m_Context;
        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(gui_context->m_RenderContext);

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

        uint32_t physical_width = dmGraphics::GetWindowWidth(graphics_context);
        uint32_t physical_height = dmGraphics::GetWindowHeight(graphics_context);
        dmGui::SetPhysicalResolution(scene, physical_width, physical_height);

        if (!SetupGuiScene(scene, scene_resource))
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
        uint32_t                 m_NextZ;
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
            Vector4 position = n->m_Properties[dmGui::PROPERTY_POSITION];
            const Vector4& rotation = n->m_Properties[dmGui::PROPERTY_ROTATION];
            const Vector4& extents = n->m_Properties[dmGui::PROPERTY_EXTENTS];
            const Vector4& scale = n->m_Properties[dmGui::PROPERTY_SCALE];
            const Vector4& color = n->m_Properties[dmGui::PROPERTY_COLOR];
            const dmGui::Pivot pivot = (dmGui::Pivot) n->m_Pivot;

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
            ro.m_SetBlendFactors = 1;

            ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
            ro.m_VertexBuffer = gui_world->m_QuadVertexBuffer;
            ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLE_STRIP;
            ro.m_VertexStart = 0;
            ro.m_VertexCount = 4;
            ro.m_Material = gui_world->m_Material;

            // Set default texture
            if (n->m_Texture)
                ro.m_Textures[0] = (dmGraphics::HTexture) n->m_Texture;
            else
                ro.m_Textures[0] = gui_world->m_WhiteTexture;

            float dx = 0;
            float dy = 0;
            if (n->m_NodeType == dmGui::NODE_TYPE_BOX)
            {
                switch (pivot)
                {
                    case dmGui::PIVOT_CENTER:
                    case dmGui::PIVOT_S:
                    case dmGui::PIVOT_N:
                        dx = extents.getX() * 0.5;
                        break;

                    case dmGui::PIVOT_NE:
                    case dmGui::PIVOT_E:
                    case dmGui::PIVOT_SE:
                        dx = extents.getX();
                        break;

                    case dmGui::PIVOT_SW:
                    case dmGui::PIVOT_W:
                    case dmGui::PIVOT_NW:
                        break;
                }

                switch (pivot) {
                    case dmGui::PIVOT_CENTER:
                    case dmGui::PIVOT_E:
                    case dmGui::PIVOT_W:
                        dy = extents.getY() * 0.5;
                        break;

                    case dmGui::PIVOT_N:
                    case dmGui::PIVOT_NE:
                    case dmGui::PIVOT_NW:
                        dy = extents.getY();
                        break;

                    case dmGui::PIVOT_S:
                    case dmGui::PIVOT_SW:
                    case dmGui::PIVOT_SE:
                        break;
                }
            }
            else if (n->m_NodeType == dmGui::NODE_TYPE_TEXT)
            {
                dmRender::TextMetrics metrics;
                dmRender::GetTextMetrics((dmRender::HFontMap) n->m_Font, n->m_Text, &metrics);

                switch (pivot)
                {
                    case dmGui::PIVOT_CENTER:
                    case dmGui::PIVOT_S:
                    case dmGui::PIVOT_N:
                        dx = metrics.m_Width * 0.5;
                        break;

                    case dmGui::PIVOT_NE:
                    case dmGui::PIVOT_E:
                    case dmGui::PIVOT_SE:
                        dx = metrics.m_Width;
                        break;

                    case dmGui::PIVOT_SW:
                    case dmGui::PIVOT_W:
                    case dmGui::PIVOT_NW:
                        break;
                }

                switch (pivot) {
                    case dmGui::PIVOT_CENTER:
                    case dmGui::PIVOT_E:
                    case dmGui::PIVOT_W:
                        dy = -metrics.m_MaxDescent * 0.5 + metrics.m_MaxAscent * 0.5;
                        break;

                    case dmGui::PIVOT_N:
                    case dmGui::PIVOT_NE:
                    case dmGui::PIVOT_NW:
                        dy = metrics.m_MaxAscent;
                        break;

                    case dmGui::PIVOT_S:
                    case dmGui::PIVOT_SW:
                    case dmGui::PIVOT_SE:
                        dy = -metrics.m_MaxDescent;
                        break;
                }

            }
            Vector4 delta_pivot = Vector4(dx, dy, 0, 0);
            position -= delta_pivot;

            const float deg_to_rad = 3.1415926f / 180.0f;
            Matrix4 m = Matrix4::translation(delta_pivot.getXYZ()) *
                        Matrix4::translation(position.getXYZ()) *
                        Matrix4::rotationZ(rotation.getZ() * deg_to_rad) *
                        Matrix4::rotationY(rotation.getY() * deg_to_rad) *
                        Matrix4::rotationX(rotation.getX() * deg_to_rad) *
                        Matrix4::scale(scale.getXYZ()) *
                        Matrix4::translation(-delta_pivot.getXYZ());
            if (n->m_NodeType == dmGui::NODE_TYPE_BOX)
            {
                m *= Matrix4::scale(Vector3(extents.getX(), extents.getY(), 1));
                ro.m_WorldTransform = m;
                ro.m_RenderKey.m_Depth = gui_context->m_NextZ;
                dmRender::EnableRenderObjectFragmentConstant(&ro, 0, color);
                gui_world->m_GuiRenderObjects.Push(ro);

                dmRender::AddToRender(gui_context->m_RenderContext, &gui_world->m_GuiRenderObjects[gui_world->m_GuiRenderObjects.Size()-1]);
            }
            else if (n->m_NodeType == dmGui::NODE_TYPE_TEXT)
            {
                dmRender::DrawTextParams params;
                params.m_FaceColor = color;
                params.m_Text = n->m_Text;
                params.m_WorldTransform = m;
                params.m_Depth = gui_context->m_NextZ;
                dmRender::DrawText(gui_context->m_RenderContext, (dmRender::HFontMap) n->m_Font, params);
            }
            gui_context->m_NextZ++;
        }
    }

    dmGameObject::UpdateResult CompGuiUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiContext* gui_context = (GuiContext*)params.m_Context;

        // update
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            if (gui_world->m_Components[i]->m_Enabled)
                dmGui::UpdateScene(gui_world->m_Components[i]->m_Scene, params.m_UpdateContext->m_DT);
        }

        RenderGuiContext render_gui_context;
        render_gui_context.m_RenderContext = gui_context->m_RenderContext;
        render_gui_context.m_GuiWorld = gui_world;
        render_gui_context.m_NextZ = 0;

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
        dmGui::Result result = dmGui::DispatchMessage(gui_component->m_Scene, params.m_Message);
        if (result != dmGui::RESULT_OK)
        {
            // TODO: Proper error message
            dmLogError("Error when dispatching message to gui scene: %d", result);
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
            bool consumed;
            dmGui::Result gui_result = dmGui::DispatchInput(gui_component->m_Scene, &gui_input_action, 1, &consumed);
            if (gui_result != dmGui::RESULT_OK)
            {
                return dmGameObject::INPUT_RESULT_UNKNOWN_ERROR;
            }
            else if (consumed)
            {
                return dmGameObject::INPUT_RESULT_CONSUMED;
            }
        }
        return dmGameObject::INPUT_RESULT_IGNORED;
    }

    void CompGuiOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource;
        Component* gui_component = (Component*)*params.m_UserData;
        dmGui::Result result = dmGui::FinalScene(gui_component->m_Scene);
        if (result != dmGui::RESULT_OK)
        {
            // TODO: Translate result
            dmLogError("Error when finalizing gui component: %d.", result);
        }
        dmGui::ClearTextures(gui_component->m_Scene);
        dmGui::ClearFonts(gui_component->m_Scene);
        dmGui::ClearNodes(gui_component->m_Scene);
        if (SetupGuiScene(gui_component->m_Scene, scene_resource))
        {
            result = dmGui::InitScene(gui_component->m_Scene);
            if (result != dmGui::RESULT_OK)
            {
                // TODO: Translate result
                dmLogError("Error when initializing gui component: %d.", result);
            }
        }
        else
        {
            dmLogError("Could not reload scene '%s' because of errors in the resource.", scene_resource->m_Path);
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
