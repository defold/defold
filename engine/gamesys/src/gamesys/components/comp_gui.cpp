#include <string.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <graphics/graphics.h>
#include <graphics/graphics_util.h>
#include <render/render.h>
#include <render/font_renderer.h>
#include <gameobject/gameobject_ddf.h>

#include "comp_gui.h"

#include "../resources/res_gui.h"
#include "../gamesys.h"
#include "../gamesys_private.h"

extern unsigned char GUI_VPC[];
extern uint32_t GUI_VPC_SIZE;

extern unsigned char GUI_FPC[];
extern uint32_t GUI_FPC_SIZE;

namespace dmGameSystem
{
    dmRender::HRenderType g_GuiRenderType = dmRender::INVALID_RENDER_TYPE_HANDLE;

    struct GuiWorld;
    struct GuiRenderNode
    {
        dmGui::HNode m_GuiNode;
        GuiWorld*   m_GuiWorld;
        GuiRenderNode(dmGui::HNode node, GuiWorld* gui_world) : m_GuiNode(node), m_GuiWorld(gui_world) {}
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

        gui_world->m_Components.SetCapacity(64);

        // TODO: Everything below here should be move to the "universe" when available
        // and hence shared among all the worlds
        gui_world->m_VertexProgram = dmGraphics::NewVertexProgram(dmRender::GetGraphicsContext(gui_context->m_RenderContext), GUI_VPC, GUI_VPC_SIZE);
        gui_world->m_FragmentProgram = dmGraphics::NewFragmentProgram(dmRender::GetGraphicsContext(gui_context->m_RenderContext), GUI_FPC, GUI_FPC_SIZE);

        gui_world->m_Material = dmRender::NewMaterial(gui_context->m_RenderContext, gui_world->m_VertexProgram, gui_world->m_FragmentProgram);
        SetMaterialProgramConstantType(gui_world->m_Material, dmHashString64("view_proj"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_VIEWPROJ);
        SetMaterialProgramConstantType(gui_world->m_Material, dmHashString64("world"), dmRenderDDF::MaterialDesc::CONSTANT_TYPE_WORLD);

        dmRender::AddMaterialTag(gui_world->m_Material, dmHashString32("gui"));

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
                {"color", 2, 4, dmGraphics::TYPE_UNSIGNED_BYTE, true},
        };

        gui_world->m_VertexDeclaration = dmGraphics::NewVertexDeclaration(dmRender::GetGraphicsContext(gui_context->m_RenderContext), ve, sizeof(ve) / sizeof(dmGraphics::VertexElement));
        // Grows automatically
        gui_world->m_ClientVertexBuffer.SetCapacity(512);
        gui_world->m_VertexBuffer = dmGraphics::NewVertexBuffer(dmRender::GetGraphicsContext(gui_context->m_RenderContext), 0, 0, dmGraphics::BUFFER_USAGE_STREAM_DRAW);

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
        tex_params.m_OriginalWidth = 2;
        tex_params.m_OriginalHeight = 2;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        gui_world->m_WhiteTexture = dmGraphics::NewTexture(dmRender::GetGraphicsContext(gui_context->m_RenderContext), tex_params);

        // Grows automatically
        gui_world->m_GuiRenderObjects.SetCapacity(128);

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
        dmRender::DeleteMaterial(gui_context->m_RenderContext, gui_world->m_Material);
        dmGraphics::DeleteVertexProgram(gui_world->m_VertexProgram);
        dmGraphics::DeleteFragmentProgram(gui_world->m_FragmentProgram);
        dmGraphics::DeleteVertexDeclaration(gui_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(gui_world->m_VertexBuffer);
        dmGraphics::DeleteTexture(gui_world->m_WhiteTexture);

        delete gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    bool SetupGuiScene(dmGui::HScene scene, GuiSceneResource* scene_resource)
    {
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;
        dmGui::SetSceneScript(scene, scene_resource->m_Script);

        bool result = true;

        for (uint32_t i = 0; i < scene_resource->m_FontMaps.Size(); ++i)
        {
            const char* name = scene_desc->m_Fonts[i].m_Name;
            dmGui::Result r = dmGui::AddFont(scene, name, (void*) scene_resource->m_FontMaps[i]);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add font '%s' to scene (%d)", name,  r);
                return false;
            }
        }

        for (uint32_t i = 0; i < scene_resource->m_Textures.Size(); ++i)
        {
            const char* name = scene_desc->m_Textures[i].m_Name;
            dmGui::Result r = dmGui::AddTexture(scene, name, (void*) scene_resource->m_Textures[i]);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add texture '%s' to scene (%d)", name,  r);
                return false;
            }
        }

        uint32_t layer_count = scene_desc->m_Layers.m_Count;
        for (uint32_t i = 0; i < layer_count; ++i)
        {
            const char* name = scene_desc->m_Layers[i].m_Name;
            dmGui::Result r = dmGui::AddLayer(scene, name);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add layer '%s' to scene (%d)", name,  r);
                return false;
            }
        }

        for (uint32_t i = 0; i < scene_desc->m_Nodes.m_Count; ++i)
        {
            const dmGuiDDF::NodeDesc* node_desc = &scene_desc->m_Nodes[i];

            // NOTE: We assume that the enums in dmGui and dmGuiDDF have the same values
            dmGui::NodeType type = (dmGui::NodeType) node_desc->m_Type;
            dmGui::BlendMode blend_mode = (dmGui::BlendMode) node_desc->m_BlendMode;
            // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
            if (blend_mode == dmGui::BLEND_MODE_ADD_ALPHA)
                blend_mode = dmGui::BLEND_MODE_ADD;
            dmGui::AdjustMode adjust_mode = (dmGui::AdjustMode) node_desc->m_AdjustMode;

            Vector4 position = node_desc->m_Position;
            Vector4 size = node_desc->m_Size;
            dmGui::HNode n = dmGui::NewNode(scene, Point3(position.getXYZ()), Vector3(size.getXYZ()), type);
            if (n)
            {
                if (node_desc->m_Type == dmGuiDDF::NodeDesc::TYPE_TEXT)
                {
                    dmGui::SetNodeText(scene, n, node_desc->m_Text);
                    dmGui::SetNodeFont(scene, n, node_desc->m_Font);
                    dmGui::SetNodeLineBreak(scene, n, node_desc->m_LineBreak);
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
                if (node_desc->m_Layer != 0x0 && *node_desc->m_Layer != '\0')
                {
                    dmGui::Result gui_result = dmGui::SetNodeLayer(scene, n, node_desc->m_Layer);
                    if (gui_result != dmGui::RESULT_OK)
                    {
                        dmLogError("The layer '%s' could not be set for the '%s', result: %d.", node_desc->m_Layer, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", gui_result);
                        result = false;
                    }
                }

                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_ROTATION, node_desc->m_Rotation);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SCALE, node_desc->m_Scale);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_COLOR, node_desc->m_Color);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_OUTLINE, node_desc->m_Outline);
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SHADOW, node_desc->m_Shadow);
                dmGui::SetNodeBlendMode(scene, n, blend_mode);
                dmGui::SetNodePivot(scene, n, (dmGui::Pivot) node_desc->m_Pivot);
                dmGui::SetNodeXAnchor(scene, n, (dmGui::XAnchor) node_desc->m_Xanchor);
                dmGui::SetNodeYAnchor(scene, n, (dmGui::YAnchor) node_desc->m_Yanchor);
                dmGui::SetNodeAdjustMode(scene, n, adjust_mode);
                dmGui::SetNodeResetPoint(scene, n);
                dmGui::SetNodeInheritColor(scene, n, node_desc->m_InheritColor);
            }
            else
            {
                result = false;
            }
        }
        if (result)
        {
            for (uint32_t i = 0; i < scene_desc->m_Nodes.m_Count; ++i)
            {
                const dmGuiDDF::NodeDesc* node_desc = &scene_desc->m_Nodes[i];
                dmGui::HNode n = dmGui::GetNodeById(scene, node_desc->m_Id);
                dmGui::HNode p = dmGui::INVALID_HANDLE;
                if (node_desc->m_Parent != 0x0 && *node_desc->m_Parent != 0)
                {
                    p = dmGui::GetNodeById(scene, node_desc->m_Parent);
                    if (p == dmGui::INVALID_HANDLE)
                    {
                        dmLogError("The parent '%s' could not be found in the scene.", node_desc->m_Parent);
                        result = false;
                    }
                }
                dmGui::SetNodeParent(scene, n, p);
            }
        }
        return result;
    }

    dmGameObject::CreateResult CompGuiCreate(const dmGameObject::ComponentCreateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;

        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource;

        GuiComponent* gui_component = new GuiComponent();
        gui_component->m_Instance = params.m_Instance;
        gui_component->m_ComponentIndex = params.m_ComponentIndex;
        gui_component->m_Enabled = 1;

        dmGui::NewSceneParams scene_params;
        scene_params.m_MaxNodes = 256;
        scene_params.m_MaxAnimations = 1024;
        scene_params.m_UserData = gui_component;
        scene_params.m_MaxFonts = 64;
        scene_params.m_MaxTextures = 128;
        gui_component->m_Scene = dmGui::NewScene(scene_resource->m_GuiContext, &scene_params);
        dmGui::HScene scene = gui_component->m_Scene;

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
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
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
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
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
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
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

    static void SetBlendMode(dmRender::RenderObject& ro, dmGui::BlendMode blend_mode)
    {
        switch (blend_mode)
        {
            case dmGui::BLEND_MODE_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            case dmGui::BLEND_MODE_ADD:
            case dmGui::BLEND_MODE_ADD_ALPHA:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            case dmGui::BLEND_MODE_MULT:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }
    }

    void RenderTextNodes(dmGui::HScene scene,
                         dmGui::HNode* nodes,
                         const Matrix4* node_transforms,
                         const Vector4* node_colors,
                         uint32_t node_count,
                         void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = nodes[i];

            const Vector4& color = node_colors[i];
            const Vector4& outline = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_OUTLINE);
            const Vector4& shadow = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_SHADOW);

            dmGui::NodeType node_type = dmGui::GetNodeType(scene, node);
            assert(node_type == dmGui::NODE_TYPE_TEXT);

            dmRender::DrawTextParams params;
            params.m_FaceColor = color;
            params.m_OutlineColor = outline;
            params.m_ShadowColor = shadow;
            params.m_Text = dmGui::GetNodeText(scene, node);
            params.m_WorldTransform = node_transforms[i];
            params.m_Depth = gui_context->m_NextZ;
            params.m_RenderOrder = dmGui::GetRenderOrder(scene);
            params.m_LineBreak = dmGui::GetNodeLineBreak(scene, node);
            Vector4 size = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_SIZE);
            params.m_Width = size.getX();
            params.m_Height = size.getY();
            dmGui::Pivot pivot = dmGui::GetNodePivot(scene, node);
            switch (pivot)
            {
            case dmGui::PIVOT_NW:
                params.m_Align = dmRender::TEXT_ALIGN_LEFT;
                params.m_VAlign = dmRender::TEXT_VALIGN_TOP;
                break;
            case dmGui::PIVOT_N:
                params.m_Align = dmRender::TEXT_ALIGN_CENTER;
                params.m_VAlign = dmRender::TEXT_VALIGN_TOP;
                break;
            case dmGui::PIVOT_NE:
                params.m_Align = dmRender::TEXT_ALIGN_RIGHT;
                params.m_VAlign = dmRender::TEXT_VALIGN_TOP;
                break;
            case dmGui::PIVOT_W:
                params.m_Align = dmRender::TEXT_ALIGN_LEFT;
                params.m_VAlign = dmRender::TEXT_VALIGN_MIDDLE;
                break;
            case dmGui::PIVOT_CENTER:
                params.m_Align = dmRender::TEXT_ALIGN_CENTER;
                params.m_VAlign = dmRender::TEXT_VALIGN_MIDDLE;
                break;
            case dmGui::PIVOT_E:
                params.m_Align = dmRender::TEXT_ALIGN_RIGHT;
                params.m_VAlign = dmRender::TEXT_VALIGN_MIDDLE;
                break;
            case dmGui::PIVOT_SW:
                params.m_Align = dmRender::TEXT_ALIGN_LEFT;
                params.m_VAlign = dmRender::TEXT_VALIGN_BOTTOM;
                break;
            case dmGui::PIVOT_S:
                params.m_Align = dmRender::TEXT_ALIGN_CENTER;
                params.m_VAlign = dmRender::TEXT_VALIGN_BOTTOM;
                break;
            case dmGui::PIVOT_SE:
                params.m_Align = dmRender::TEXT_ALIGN_RIGHT;
                params.m_VAlign = dmRender::TEXT_VALIGN_BOTTOM;
                break;
            }
            dmRender::DrawText(gui_context->m_RenderContext, (dmRender::HFontMap) dmGui::GetNodeFont(scene, node), params);
        }
    }

    void RenderBoxNodes(dmGui::HScene scene,
                        dmGui::HNode* nodes,
                        const Matrix4* node_transforms,
                        const Vector4* node_colors,
                        uint32_t node_count,
                        void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::HNode first_node = nodes[0];
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_BOX);

        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);
        dmRender::RenderObject& ro = gui_world->m_GuiRenderObjects[ro_count];
        // NOTE: ro might be uninitialized and we don't want to create a stack allocated temporary
        // See case 2264
        ro.Init();

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_VertexCount = 6 * node_count;
        ro.m_Material = gui_world->m_Material;
        ro.m_RenderKey.m_Order = dmGui::GetRenderOrder(scene);

        // Set default texture
        void* texture = dmGui::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = (dmGraphics::HTexture) texture;
        else
            ro.m_Textures[0] = gui_world->m_WhiteTexture;

        if (gui_world->m_ClientVertexBuffer.Remaining() < (6 * node_count)) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, 6 * node_count));
        }

        for (uint32_t i = 0; i < node_count; ++i)
        {
            const Vector4& color = node_colors[i];

            ro.m_RenderKey.m_Depth = gui_context->m_NextZ;
            // Pre-multiplied alpha
            Vector4 pm_color(color);
            pm_color.setX(color.getX() * color.getW());
            pm_color.setY(color.getY() * color.getW());
            pm_color.setZ(color.getZ() * color.getW());
            uint32_t bcolor = dmGraphics::PackRGBA(pm_color);

            Vectormath::Aos::Vector4 p0 = (node_transforms[i] * Vectormath::Aos::Point3(0, 0, 0));
            Vectormath::Aos::Vector4 p1 = (node_transforms[i] * Vectormath::Aos::Point3(0, 1, 0));
            Vectormath::Aos::Vector4 p2 = (node_transforms[i] * Vectormath::Aos::Point3(1, 0, 0));
            Vectormath::Aos::Vector4 p3 = (node_transforms[i] * Vectormath::Aos::Point3(0, 1, 0));
            Vectormath::Aos::Vector4 p4 = (node_transforms[i] * Vectormath::Aos::Point3(1, 1, 0));
            Vectormath::Aos::Vector4 p5 = (node_transforms[i] * Vectormath::Aos::Point3(1, 0, 0));

            BoxVertex v0(p0, 0, 1, bcolor);
            BoxVertex v1(p1, 0, 0, bcolor);
            BoxVertex v2(p2, 1, 1, bcolor);
            BoxVertex v3(p3, 0, 0, bcolor);
            BoxVertex v4(p4, 1, 0, bcolor);
            BoxVertex v5(p5, 1, 1, bcolor);

            gui_world->m_ClientVertexBuffer.Push(v0);
            gui_world->m_ClientVertexBuffer.Push(v1);
            gui_world->m_ClientVertexBuffer.Push(v2);
            gui_world->m_ClientVertexBuffer.Push(v3);
            gui_world->m_ClientVertexBuffer.Push(v4);
            gui_world->m_ClientVertexBuffer.Push(v5);

        }
        dmRender::AddToRender(gui_context->m_RenderContext, &ro);
    }

    void RenderNodes(dmGui::HScene scene,
                    dmGui::HNode* nodes,
                    const Matrix4* node_transforms,
                    const Vector4* node_colors,
                    uint32_t node_count,
                    void* context)
    {
        if (node_count == 0)
            return;

        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::HNode first_node = nodes[0];

        dmGui::BlendMode prev_blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        dmGui::NodeType prev_node_type = dmGui::GetNodeType(scene, first_node);
        void* prev_texture = dmGui::GetNodeTexture(scene, first_node);
        void* prev_font = dmGui::GetNodeFont(scene, first_node);

        uint32_t i = 0;
        uint32_t start = 0;
        while (i < node_count) {
            dmGui::HNode node = nodes[i];
            dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, node);
            dmGui::NodeType node_type = dmGui::GetNodeType(scene, node);
            void* texture = dmGui::GetNodeTexture(scene, node);
            void* font = dmGui::GetNodeFont(scene, node);

            bool batch_change = (node_type != prev_node_type || blend_mode != prev_blend_mode || texture != prev_texture || font != prev_font);
            bool flush = (i > 0 && batch_change);

            if (flush) {
                uint32_t n = i - start;
                if (prev_node_type == dmGui::NODE_TYPE_TEXT) {
                    RenderTextNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
                } else {
                    RenderBoxNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
                }

                start = i;

                /*
                 * Consecutive nodes of the same type will get the some
                 * pseudo-z for batching. This is an approximation of the correct
                 * layer-rendering.
                 */
                gui_context->m_NextZ++;
            }

            prev_node_type = node_type;
            prev_blend_mode = blend_mode;
            prev_texture = texture;
            prev_font = font;

            ++i;
        }

        uint32_t n = i - start;
        if (n > 0) {
            if (prev_node_type == dmGui::NODE_TYPE_TEXT) {
                RenderTextNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
            } else {
                RenderBoxNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
            }
        }

        dmGraphics::SetVertexBufferData(gui_world->m_VertexBuffer,
                                        gui_world->m_ClientVertexBuffer.Size() * sizeof(BoxVertex),
                                        gui_world->m_ClientVertexBuffer.Begin(),
                                        dmGraphics::BUFFER_USAGE_STREAM_DRAW);
    }

    static dmGraphics::TextureFormat ToGraphicsFormat(dmImage::Type type) {
        switch (type) {
            case dmImage::TYPE_RGB:
                return dmGraphics::TEXTURE_FORMAT_RGB;
                break;
            case dmImage::TYPE_RGBA:
                return dmGraphics::TEXTURE_FORMAT_RGBA;
                break;
            case dmImage::TYPE_LUMINANCE:
                return dmGraphics::TEXTURE_FORMAT_LUMINANCE;
                break;
            default:
                assert(false);
        }
        return (dmGraphics::TextureFormat) 0; // Never reached
    }

    static void* NewTexture(dmGui::HScene scene, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        dmGraphics::HContext gcontext = dmRender::GetGraphicsContext(gui_context->m_RenderContext);

        dmGraphics::TextureParams tparams;
        tparams.m_Width = width;
        tparams.m_Height = height;
        tparams.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tparams.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tparams.m_Data = buffer;
        tparams.m_DataSize = dmImage::BytesPerPixel(type) * width * height;
        tparams.m_Format = ToGraphicsFormat(type);

        dmGraphics::HTexture t =  dmGraphics::NewTexture(gcontext, tparams);
        return (void*) t;
    }

    static void DeleteTexture(dmGui::HScene scene, void* texture, void* context)
    {
        dmGraphics::DeleteTexture((dmGraphics::HTexture) texture);
    }

    static void SetTextureData(dmGui::HScene scene, void* texture, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, void* context)
    {
        dmGraphics::TextureParams tparams;
        tparams.m_Width = width;
        tparams.m_Height = height;
        tparams.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tparams.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tparams.m_Data = buffer;
        tparams.m_DataSize = dmImage::BytesPerPixel(type) * width * height;
        tparams.m_Format = ToGraphicsFormat(type);
        dmGraphics::SetTexture((dmGraphics::HTexture) texture, tparams);
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


        uint32_t total_node_count = 0;
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* c = gui_world->m_Components[i];
            if (c->m_Enabled)
            {
                total_node_count += dmGui::GetNodeCount(c->m_Scene);
            }
        }

        if (gui_world->m_GuiRenderObjects.Capacity() < total_node_count)
        {
            // NOTE: m_GuiRenderObjects *before* rendering as pointers
            // to render-objects are passed to the render-system
            // Given batching the capacity is perhaps a bit over the top
            gui_world->m_GuiRenderObjects.SetCapacity(total_node_count);
        }
        gui_world->m_GuiRenderObjects.SetSize(0);
        gui_world->m_ClientVertexBuffer.SetSize(0);
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* c = gui_world->m_Components[i];
            if (c->m_Enabled) {
                dmGui::RenderSceneParams rp;
                rp.m_RenderNodes = &RenderNodes;
                rp.m_NewTexture = &NewTexture;
                rp.m_DeleteTexture = &DeleteTexture;
                rp.m_SetTextureData = &SetTextureData;
                dmGui::RenderScene(c->m_Scene, rp, &render_gui_context);
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompGuiOnMessage(const dmGameObject::ComponentOnMessageParams& params)
    {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        if (params.m_Message->m_Id == dmGameObjectDDF::Enable::m_DDFDescriptor->m_NameHash)
        {
            gui_component->m_Enabled = 1;
        }
        else if (params.m_Message->m_Id == dmGameObjectDDF::Disable::m_DDFDescriptor->m_NameHash)
        {
            gui_component->m_Enabled = 0;
        }
        dmGui::Result result = dmGui::DispatchMessage(gui_component->m_Scene, params.m_Message);
        if (result != dmGui::RESULT_OK)
        {
            // TODO: Proper error message
            LogMessageError(params.m_Message, "Error when dispatching message to gui scene: %d.", result);
        }
        return dmGameObject::UPDATE_RESULT_OK;
    }

    dmGameObject::InputResult CompGuiOnInput(const dmGameObject::ComponentOnInputParams& params)
    {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;

        if (gui_component->m_Enabled)
        {
            dmGui::HScene scene = gui_component->m_Scene;
            dmGui::InputAction gui_input_action;
            gui_input_action.m_ActionId = params.m_InputAction->m_ActionId;
            gui_input_action.m_Value = params.m_InputAction->m_Value;
            gui_input_action.m_Pressed = params.m_InputAction->m_Pressed;
            gui_input_action.m_Released = params.m_InputAction->m_Released;
            gui_input_action.m_Repeated = params.m_InputAction->m_Repeated;
            gui_input_action.m_PositionSet = params.m_InputAction->m_PositionSet;
            gui_input_action.m_X = params.m_InputAction->m_X;
            gui_input_action.m_Y = params.m_InputAction->m_Y;
            gui_input_action.m_DX = params.m_InputAction->m_DX;
            gui_input_action.m_DY = params.m_InputAction->m_DY;
            gui_input_action.m_ScreenX = params.m_InputAction->m_ScreenX;
            gui_input_action.m_ScreenY = params.m_InputAction->m_ScreenY;
            gui_input_action.m_ScreenDX = params.m_InputAction->m_ScreenDX;
            gui_input_action.m_ScreenDY = params.m_InputAction->m_ScreenDY;

            gui_input_action.m_TouchCount = params.m_InputAction->m_TouchCount;
            int tc = params.m_InputAction->m_TouchCount;
            for (int i = 0; i < tc; ++i) {
                gui_input_action.m_Touch[i] = params.m_InputAction->m_Touch[i];
            }

            size_t text_count = dmStrlCpy(gui_input_action.m_Text, params.m_InputAction->m_Text, sizeof(gui_input_action.m_Text));
            gui_input_action.m_TextCount = text_count;

            bool consumed;
            dmGui::Result gui_result = dmGui::DispatchInput(scene, &gui_input_action, 1, &consumed);
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
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
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
        GuiComponent* component = (GuiComponent*)dmGui::GetSceneUserData(scene);
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
        GuiComponent* component = (GuiComponent*)dmGui::GetSceneUserData(scene);
        return (uintptr_t)component->m_Instance;
    }

    dmhash_t GuiResolvePathCallback(dmGui::HScene scene, const char* path, uint32_t path_size)
    {
        GuiComponent* component = (GuiComponent*)dmGui::GetSceneUserData(scene);
        if (path_size > 0)
        {
            return dmGameObject::GetAbsoluteIdentifier(component->m_Instance, path, path_size);
        }
        else
        {
            return dmGameObject::GetIdentifier(component->m_Instance);
        }
    }

    void GuiGetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, dmGui::TextMetrics* out_metrics)
    {
        dmRender::TextMetrics metrics;
        dmRender::GetTextMetrics((dmRender::HFontMap)font, text, width, line_break, &metrics);
        out_metrics->m_Width = metrics.m_Width;
        out_metrics->m_MaxAscent = metrics.m_MaxAscent;
        out_metrics->m_MaxDescent = metrics.m_MaxDescent;
    }
}
