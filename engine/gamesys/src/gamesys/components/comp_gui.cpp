#include <string.h>

#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/trig_lookup.h>
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

        dmGraphics::TextureCreationParams tex_create_params;
        dmGraphics::TextureParams tex_params;

        tex_create_params.m_Width = 2;
        tex_create_params.m_Height = 2;
        tex_create_params.m_OriginalWidth = 2;
        tex_create_params.m_OriginalHeight = 2;

        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_RGBA;
        tex_params.m_Data = white_texture;
        tex_params.m_DataSize = sizeof(white_texture);
        tex_params.m_Width = 2;
        tex_params.m_Height = 2;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;

        gui_world->m_WhiteTexture = dmGraphics::NewTexture(dmRender::GetGraphicsContext(gui_context->m_RenderContext), tex_create_params);
        dmGraphics::SetTexture(gui_world->m_WhiteTexture, tex_params);

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

        dmGui::SetMaterial(scene, scene_resource->m_Material);

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
                dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SLICE9, node_desc->m_Slice9);
                dmGui::SetNodeBlendMode(scene, n, blend_mode);
                dmGui::SetNodePivot(scene, n, (dmGui::Pivot) node_desc->m_Pivot);
                dmGui::SetNodeXAnchor(scene, n, (dmGui::XAnchor) node_desc->m_Xanchor);
                dmGui::SetNodeYAnchor(scene, n, (dmGui::YAnchor) node_desc->m_Yanchor);
                dmGui::SetNodeAdjustMode(scene, n, adjust_mode);
                dmGui::SetNodeResetPoint(scene, n);
                dmGui::SetNodeInheritAlpha(scene, n, node_desc->m_InheritAlpha);

                dmGui::SetNodePerimeterVertices(scene, n, node_desc->m_Perimetervertices);
                dmGui::SetNodeInnerRadius(scene, n, node_desc->m_Innerradius);
                dmGui::SetNodeOuterBounds(scene, n, (dmGui::PieBounds) node_desc->m_Outerbounds);
                dmGui::SetNodePieFillAngle(scene, n, node_desc->m_Piefillangle);
                dmGui::SetNodeClippingMode(scene, n, (dmGui::ClippingMode) node_desc->m_ClippingMode);
                dmGui::SetNodeClippingVisible(scene, n, node_desc->m_ClippingVisible);
                dmGui::SetNodeClippingInverted(scene, n, node_desc->m_ClippingInverted);
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
        gui_component->m_AddedToUpdate = 0;

        dmGui::NewSceneParams scene_params;
        scene_params.m_MaxNodes = 512;
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

    static dmRender::RenderObject& GetRenderObject(dmGui::HScene scene, RenderGuiContext* gui_context)
    {
        GuiWorld* gui_world = gui_context->m_GuiWorld;
        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);
        dmRender::RenderObject& ro = gui_world->m_GuiRenderObjects[ro_count];
        ro.Init();
        ro.m_RenderKey.m_Order = dmGui::GetRenderOrder(scene);
        ro.m_RenderKey.m_Depth = gui_context->m_NextZ++;
        return ro;
    }

    enum ScissorClippingStatus
    {
        SCISSOR_CLIPPING_STATUS_DISABLED,
        SCISSOR_CLIPPING_STATUS_NOP,
        SCISSOR_CLIPPING_STATUS_BEGIN,
        SCISSOR_CLIPPING_STATUS_BEGIN_TEST,
        SCISSOR_CLIPPING_STATUS_END
    };

    struct ScissorClippingState
    {
        ScissorClippingStatus m_Status;
        dmGui::HScene m_Scene;
        RenderGuiContext* m_GuiContext;

        uint16_t m_NextRenderStatesIndex;
        dmGui::ScissorClippingRenderState* m_RenderStates;
        dmGui::ScissorClippingRenderState m_RenderState;
        dmGui::HNode m_NextNode;
        Vectormath::Aos::Matrix4 m_ScissorTransform;
    };

    static void InitializeScissorClipping(ScissorClippingState& state, const dmGui::HScene scene, RenderGuiContext* gui_state, const dmGui::ScissorClippingRenderState* clipping_render_states)
    {
        state.m_Status = SCISSOR_CLIPPING_STATUS_DISABLED;
        state.m_Scene = scene;
        state.m_GuiContext = gui_state;
        state.m_RenderStates = (dmGui::ScissorClippingRenderState*) clipping_render_states;
        state.m_NextRenderStatesIndex = 0;
        state.m_NextNode = clipping_render_states->m_StartNode;
        state.m_RenderState.m_EndNode = dmGui::INVALID_HANDLE;
    }

    static inline bool UpdateScissorClipping(ScissorClippingState& state, const dmGui::HNode node)
    {
        if(state.m_NextNode == node)
        {
            // new scissor node
            state.m_RenderState = state.m_RenderStates[state.m_NextRenderStatesIndex];
            state.m_NextNode = state.m_RenderStates[++state.m_NextRenderStatesIndex].m_StartNode;
            state.m_Status = SCISSOR_CLIPPING_STATUS_BEGIN;
            return true;
        }
        if(state.m_RenderState.m_EndNode == node)
        {
            if(state.m_RenderState.m_NextStateNodeArrayIndex == 0xffff)
            {
                // disable scissor (no parent scissor)
                state.m_RenderState.m_EndNode = dmGui::INVALID_HANDLE;
                state.m_Status = SCISSOR_CLIPPING_STATUS_END;
            }
            else
            {
                // change to next scissor scope (parent scissor changed)
                dmGui::ScissorClippingRenderState& current_state = state.m_RenderState;
                const dmGui::ScissorClippingRenderState& next_state = state.m_RenderStates[current_state.m_NextStateNodeArrayIndex];
                current_state.m_EndNode = next_state.m_EndNode;
                current_state.m_NextStateNodeArrayIndex = next_state.m_NextStateNodeArrayIndex;
                for(uint32_t i = 0; i < 4; i++)
                    current_state.m_Rect[i] = next_state.m_Rect[i];
                state.m_Status = SCISSOR_CLIPPING_STATUS_BEGIN_TEST;
            }
            return true;
        }

        return false;
    }

    static void ApplyScissorClipping(ScissorClippingState& state, const Matrix4* transform)
    {
        if(state.m_Status <= SCISSOR_CLIPPING_STATUS_NOP)
            return;

        dmRender::RenderObject& ro = GetRenderObject(state.m_Scene, state.m_GuiContext);
        ro.m_SetScissorTest = 1;
        dmGraphics::ScissorTestParams& stp = ro.m_ScissorTestParams;

        if(state.m_Status == SCISSOR_CLIPPING_STATUS_BEGIN)
        {
            stp.m_TestEnable = 1;

            Vector3 scale(length(transform->getCol0()), length(transform->getCol1()), length(transform->getCol3()));
            Vector4 p0 = (*transform * Point3(0.0, 0.0, 0));
            Vector4 p1 = (*transform * Point3(1.0, 1.0, 0));
            Vector3 translation(((p0[0] + p1[0]) * 0.5f) - (scale[0] * 0.5), ((p0[1] + p1[1]) * 0.5f) - (scale[1] * 0.5), 0.0f);
            Matrix4 mat(Matrix4::identity());
            mat.setUpper3x3(Matrix3::scale(scale));
            mat.setTranslation(translation);
            state.m_ScissorTransform = mat;
            p0 = (mat * Vector4(0.0f, 0.0f, 0.0f, 1.0f));
            p1 = (mat * Vector4(1.0f, 1.0f, 0.0f, 1.0f));

            // NOTE: Using GetViewProjectionMatrix at this point is expecting the last view and projection matrices set a renderscript
            // before rendering the Gui to be intended for use by the Gui.
            const Matrix4& view_proj = GetViewProjectionMatrix(state.m_GuiContext->m_RenderContext);
            float screen_width = (1.0f/view_proj.getElem(0, 0))*2.0f;
            float screen_height = (1.0f/view_proj.getElem(1, 1))*2.0f;
            int16_t sc_rect[4];
            p0 = view_proj * p0;
            p0[0] /= p0[3];
            sc_rect[0] = (int16_t) ((1.0f + p0.getX()) * screen_width / 2);
            p0[1] /= p0[3];
            sc_rect[1] = (int16_t) ((1.0f + p0.getY()) * screen_height / 2);
            p1 = view_proj * p1;
            p1[0] /= p1[3];
            sc_rect[2] = (int16_t) ((1.0f + p1.getX()) * screen_width / 2);
            p1[1] /= p1[3];
            sc_rect[3] = (int16_t) ((1.0f + p1.getY()) * screen_height / 2);

            int16_t* rect = state.m_RenderStates[state.m_NextRenderStatesIndex-1].m_Rect;
            dmGui::ScissorClippingRenderState& render_state = state.m_RenderState;
            if(render_state.m_IsAncestor)
            {
                for(uint32_t i = 0; i < 4; i++)
                    rect[i] = sc_rect[i];
            }
            else
            {
                // child scissor
                int16_t* parent_rect = state.m_RenderStates[render_state.m_ParentStateNodeArrayIndex].m_Rect;
                rect[0] = dmMath::Max(parent_rect[0], sc_rect[0]);
                rect[1] = dmMath::Max(parent_rect[1], sc_rect[1]);
                rect[2] = dmMath::Min(parent_rect[2], sc_rect[2]);
                rect[3] = dmMath::Min(parent_rect[3], sc_rect[3]);
            }

            stp.m_X = rect[0];
            stp.m_Y = rect[1];
            stp.m_Width = rect[2]-rect[0];
            stp.m_Height = rect[3]-rect[1];
            state.m_Status = SCISSOR_CLIPPING_STATUS_NOP;
        }
        else
        {
            if(state.m_Status == SCISSOR_CLIPPING_STATUS_BEGIN_TEST)
            {
                stp.m_TestEnable = 1;
                int16_t* rect = state.m_RenderState.m_Rect;
                stp.m_X = rect[0];
                stp.m_Y = rect[1];
                stp.m_Width = rect[2]-rect[0];
                stp.m_Height = rect[3]-rect[1];
                state.m_Status = SCISSOR_CLIPPING_STATUS_NOP;
            }
            else
            {
                stp.m_TestEnable = 0;
                assert(state.m_Status == SCISSOR_CLIPPING_STATUS_END);
                state.m_Status = SCISSOR_CLIPPING_STATUS_DISABLED;
            }
        }
        dmRender::AddToRender(state.m_GuiContext->m_RenderContext, &ro);
    }

    static void ResetScissorClipping(ScissorClippingState& state)
    {
        if(state.m_Status == SCISSOR_CLIPPING_STATUS_DISABLED)
            return;
        dmRender::RenderObject& ro = GetRenderObject(state.m_Scene, state.m_GuiContext);
        ro.m_SetScissorTest = 1;
        ro.m_ScissorTestParams.m_TestEnable = 0;
        dmRender::AddToRender(state.m_GuiContext->m_RenderContext, &ro);
        state.m_Status = SCISSOR_CLIPPING_STATUS_DISABLED;
    }

    static inline Matrix4* GetScissorTransform(ScissorClippingState& state)
    {
        return &state.m_ScissorTransform;
    }

    enum StencilClippingStatus
    {
        STENCIL_CLIPPING_STATUS_DISABLED,
        STENCIL_CLIPPING_STATUS_NOP,
        STENCIL_CLIPPING_STATUS_BEGIN,
        STENCIL_CLIPPING_STATUS_BEGIN_TEST,
        STENCIL_CLIPPING_STATUS_END
    };

    struct StencilClippingState
    {
        StencilClippingStatus m_Status;
        dmGui::HScene m_Scene;
        RenderGuiContext* m_GuiContext;

        uint16_t m_NextRenderStatesIndex;
        const dmGui::StencilClippingRenderState* m_RenderStates;
        dmGui::StencilClippingRenderState m_RenderState;
        dmGui::HNode m_NextNode;
        bool m_Visible;
    };

    static void InitializeStencilClipping(StencilClippingState& state, const dmGui::HScene scene, RenderGuiContext* gui_state, const dmGui::StencilClippingRenderState* clipping_render_states)
    {
        state.m_Status = STENCIL_CLIPPING_STATUS_DISABLED;
        state.m_Scene = scene;
        state.m_GuiContext = gui_state;
        state.m_RenderStates = clipping_render_states;
        state.m_NextRenderStatesIndex = 0;
        state.m_NextNode = clipping_render_states->m_StartNode;
        state.m_RenderState.m_EndNode = dmGui::INVALID_HANDLE;
    }

    static inline bool UpdateStencilClipping(StencilClippingState& state, const dmGui::HNode node)
    {
        if(state.m_NextNode == node)
        {
            // new stencil node
            state.m_RenderState = state.m_RenderStates[state.m_NextRenderStatesIndex];
            state.m_Visible = dmGui::GetNodeClippingVisible(state.m_Scene, node);
            state.m_NextNode = state.m_RenderStates[++state.m_NextRenderStatesIndex].m_StartNode;
            state.m_Status = STENCIL_CLIPPING_STATUS_BEGIN;
            return true;
        }
        if(state.m_RenderState.m_EndNode == node)
        {
            if(state.m_RenderState.m_NextStateNodeArrayIndex == 0xffff)
            {
                // disable stencil (no parent stencil)
                state.m_RenderState.m_EndNode = dmGui::INVALID_HANDLE;
                state.m_Status = STENCIL_CLIPPING_STATUS_END;
            }
            else
            {
                // change to next stencil scope (parent stencil changed)
                dmGui::StencilClippingRenderState& current_state = state.m_RenderState;
                const dmGui::StencilClippingRenderState& next_state = state.m_RenderStates[current_state.m_NextStateNodeArrayIndex];
                current_state.m_EndNode = next_state.m_EndNode;
                current_state.m_NextStateNodeArrayIndex = next_state.m_NextStateNodeArrayIndex;
                current_state.m_RefVal = next_state.m_RefVal;
                current_state.m_Mask = next_state.m_Mask;
                current_state.m_ParentMask = next_state.m_ParentMask;
                current_state.m_Inverted = next_state.m_Inverted;
                current_state.m_ParentInverted = next_state.m_ParentInverted;
                state.m_Status = STENCIL_CLIPPING_STATUS_BEGIN_TEST;
            }
            return true;
        }
        // begin test, triggered by new stencil (child tests)
        if(state.m_Status == STENCIL_CLIPPING_STATUS_BEGIN_TEST)
            return true;

        return false;
    }

    static void ApplyStencilClipping(StencilClippingState& state, ScissorClippingState& scissor_state)
    {
        if(state.m_Status <= STENCIL_CLIPPING_STATUS_NOP)
            return;

        if(state.m_RenderState.m_NewBatch)
        {
            state.m_RenderState.m_NewBatch = false;
            assert(state.m_RenderState.m_IsAncestor);
            dmRender::RenderObject& ro = GetRenderObject(state.m_Scene, state.m_GuiContext);
            ro.m_SetClearBuffer = 1;
            dmGraphics::ClearBufferParams& cbp = ro.m_ClearBufferParams;
            cbp.m_Stencil = 1;
            cbp.m_StencilValue = 0;
            cbp.m_StencilMask = 0xff;
            cbp.m_DisableScissor = scissor_state.m_Status != SCISSOR_CLIPPING_STATUS_DISABLED;
            dmRender::AddToRender(state.m_GuiContext->m_RenderContext, &ro);
        }

        dmRender::RenderObject& ro = GetRenderObject(state.m_Scene, state.m_GuiContext);
        ro.m_SetStencilTest = 1;
        dmGraphics::StencilTestParams& stp = ro.m_StencilTestParams;
        dmGui::StencilClippingRenderState& render_state = state.m_RenderState;

        if(state.m_Status == STENCIL_CLIPPING_STATUS_BEGIN)
        {
            stp.m_TestEnable = 1;
            stp.m_BufferMask = 0xff;
            stp.m_ColorBufferMask = state.m_Visible ? 0xf : 0;

            if(render_state.m_IsAncestor)
            {
                stp.m_Ref = render_state.m_RefVal;
                stp.m_RefMask = 0xff;
                stp.m_Func = dmGraphics::COMPARE_FUNC_ALWAYS;
                stp.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
                stp.m_OpDPFail = dmGraphics::STENCIL_OP_REPLACE;
                stp.m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE;
            }
            else
            {
                stp.m_Ref = render_state.m_RefVal;
                stp.m_RefMask = render_state.m_ParentMask;
                stp.m_Func = render_state.m_ParentInverted ? dmGraphics::COMPARE_FUNC_NOTEQUAL : dmGraphics::COMPARE_FUNC_EQUAL;
                stp.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
                stp.m_OpDPFail = dmGraphics::STENCIL_OP_REPLACE;
                stp.m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE;
            }
            state.m_Status = STENCIL_CLIPPING_STATUS_BEGIN_TEST;
        }
        else
        {
            if(state.m_Status == STENCIL_CLIPPING_STATUS_END)
            {
                stp.m_ColorBufferMask = 0xf;
                stp.m_TestEnable = 0;
                state.m_Status = STENCIL_CLIPPING_STATUS_DISABLED;
            }
            else
            {
                assert(state.m_Status == STENCIL_CLIPPING_STATUS_BEGIN_TEST);
                stp.m_Ref = render_state.m_RefVal;
                stp.m_RefMask = render_state.m_Mask;
                stp.m_Func = render_state.m_Inverted ? dmGraphics::COMPARE_FUNC_NOTEQUAL : dmGraphics::COMPARE_FUNC_EQUAL;
                stp.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
                stp.m_OpDPFail = dmGraphics::STENCIL_OP_KEEP;
                stp.m_OpDPPass = dmGraphics::STENCIL_OP_KEEP;
                stp.m_BufferMask = 0x00;
                stp.m_ColorBufferMask = 0xf;
                stp.m_TestEnable = 1;
                state.m_Status = STENCIL_CLIPPING_STATUS_NOP;
            }
        }
        dmRender::AddToRender(state.m_GuiContext->m_RenderContext, &ro);
    }

    static void ResetStencilClipping(StencilClippingState& state)
    {
        if(state.m_Status == STENCIL_CLIPPING_STATUS_DISABLED)
            return;
        dmRender::RenderObject& ro = GetRenderObject(state.m_Scene, state.m_GuiContext);
        ro.m_SetStencilTest = 1;
        ro.m_StencilTestParams.m_ColorBufferMask = 0xf;
        ro.m_StencilTestParams.m_TestEnable = 0;
        dmRender::AddToRender(state.m_GuiContext->m_RenderContext, &ro);
        state.m_Status = STENCIL_CLIPPING_STATUS_DISABLED;
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
                        ScissorClippingState& scissor_clipping_state,
                        void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::ClippingMode clipping_mode = dmGui::GetNodeClippingMode(scene, nodes[0]);
        Matrix4* scissor_transform = 0x0;
        if(clipping_mode == dmGui::CLIPPING_MODE_SCISSOR)
        {
            if(dmGui::GetNodeClippingVisible(scene, nodes[0]))
            {
                scissor_transform = GetScissorTransform(scissor_clipping_state);
            }
            else
            {
                if(node_count <= 1)
                    return;
                --node_count;
                ++nodes;
                ++node_transforms;
                ++node_colors;
            }
        }

        dmGui::HNode first_node = nodes[0];
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_BOX);

        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);
        dmRender::RenderObject& ro = gui_world->m_GuiRenderObjects[ro_count];
        // NOTE: ro might be uninitialized and we don't want to create a stack allocated temporary
        // See case 2264
        ro.Init();

        const int vertex_count = 6*9;

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_VertexCount = vertex_count * node_count;
        ro.m_Material = (dmRender::HMaterial) dmGui::GetMaterial(scene);
        ro.m_RenderKey.m_Order = dmGui::GetRenderOrder(scene);

        // Set default texture
        void* texture = dmGui::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = (dmGraphics::HTexture) texture;
        else
            ro.m_Textures[0] = gui_world->m_WhiteTexture;


        if (gui_world->m_ClientVertexBuffer.Remaining() < (vertex_count * node_count)) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, vertex_count * node_count));
        }

        // 9-slice values are specified with reference to the original graphics and not by
        // the possibly stretched texture.
        float org_width = (float)dmGraphics::GetOriginalTextureWidth(ro.m_Textures[0]);
        float org_height = (float)dmGraphics::GetOriginalTextureHeight(ro.m_Textures[0]);
        for (uint32_t i = 0; i < node_count; ++i)
        {
            const Vector4& color = node_colors[i];
            const dmGui::HNode node = nodes[i];

            ro.m_RenderKey.m_Depth = gui_context->m_NextZ;
            // Pre-multiplied alpha
            Vector4 pm_color(color);
            pm_color.setX(color.getX() * color.getW());
            pm_color.setY(color.getY() * color.getW());
            pm_color.setZ(color.getZ() * color.getW());
            uint32_t bcolor = dmGraphics::PackRGBA(pm_color);

            Vector4 slice9 = dmGui::GetNodeSlice9(scene, node);
            Point3 size = dmGui::GetNodeSize(scene, node);

            const float su = 1.0f / org_width;
            const float sv = 1.0f / org_height;
            const float sx = 1.0f / size.getX();
            const float sy = 1.0f / size.getY();

            float us[4], vs[4], xs[4], ys[4];

            //   0  1      2  3
            // 0 *-*-----*-*
            //   | |  y  | |
            // 1 *--*----*-*
            //   | |     | |
            //   |x|     |z|
            //   | |     | |
            // 2 *-*-----*-*
            //   | |  w  | |
            // 3 *-*-----*-*

            // v are '1-v'
            xs[0] = ys[0] = us[0] = vs[3] = 0;
            xs[3] = ys[3] = us[3] = vs[0] = 1;

            xs[1] = sx * slice9.getX();
            xs[2] = 1 - sx * slice9.getZ();

            ys[1] = sy * slice9.getW();
            ys[2] = 1 - sy * slice9.getY();

            us[1] = su * slice9.getX();
            us[2] = 1 - su * slice9.getZ();

            vs[1] = 1 - sv * slice9.getW();
            vs[2] = sv * slice9.getY();

            const Matrix4* transform = &node_transforms[i];
            if(scissor_transform != 0x0)
            {
                transform = scissor_transform;
                scissor_transform = 0x0;
            }
            Vectormath::Aos::Vector4 pts[4][4];
            for (int y=0;y<4;y++)
            {
                for (int x=0;x<4;x++)
                {
                    pts[y][x] = (*transform * Vectormath::Aos::Point3(xs[x], ys[y], 0));
                }
            }

            for (int y=0;y<3;y++)
            {
                for (int x=0;x<3;x++)
                {
                    const int x0 = x;
                    const int x1 = x+1;
                    const int y0 = y;
                    const int y1 = y+1;

                    // v<x><y>
                    BoxVertex v00(pts[y0][x0], us[x0], vs[y0], bcolor);
                    BoxVertex v10(pts[y0][x1], us[x1], vs[y0], bcolor);
                    BoxVertex v01(pts[y1][x0], us[x0], vs[y1], bcolor);
                    BoxVertex v11(pts[y1][x1], us[x1], vs[y1], bcolor);

                    gui_world->m_ClientVertexBuffer.Push(v00);
                    gui_world->m_ClientVertexBuffer.Push(v10);
                    gui_world->m_ClientVertexBuffer.Push(v11);
                    gui_world->m_ClientVertexBuffer.Push(v00);
                    gui_world->m_ClientVertexBuffer.Push(v11);
                    gui_world->m_ClientVertexBuffer.Push(v01);
                }
            }
        }

        dmRender::AddToRender(gui_context->m_RenderContext, &ro);
    }

    void RenderPieNodes(dmGui::HScene scene,
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
        assert(node_type == dmGui::NODE_TYPE_PIE);

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
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLE_STRIP;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_VertexCount = 0;
        ro.m_Material = (dmRender::HMaterial) dmGui::GetMaterial(scene);
        ro.m_RenderKey.m_Order = dmGui::GetRenderOrder(scene);

        // Set default texture
        void* texture = dmGui::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = (dmGraphics::HTexture) texture;
        else
            ro.m_Textures[0] = gui_world->m_WhiteTexture;

        uint32_t max_total_vertices = 0;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            // Computation for required number of vertices:
            // 1. four extra corner vertices per node (if rect bounds)
            // 2. above times 2 for inner and outer vertices
            // 3. one extra step for where we close the loop with exact overlapping start/stop
            const uint32_t perimeterVertices = dmMath::Max<uint32_t>(4, dmGui::GetNodePerimeterVertices(scene, nodes[i]));
            max_total_vertices += (perimeterVertices + 4) * 2 + 2;
        }

        if (gui_world->m_ClientVertexBuffer.Remaining() < max_total_vertices) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, max_total_vertices));
        }

        for (uint32_t i = 0; i < node_count; ++i)
        {
            const Vector4& color = node_colors[i];
            const dmGui::HNode node = nodes[i];
            const Point3 size = dmGui::GetNodeSize(scene, node);

            if (dmMath::Abs(size.getX()) < 0.001f)
                continue;

            ro.m_RenderKey.m_Depth = gui_context->m_NextZ;

            // Pre-multiplied alpha
            Vector4 pm_color(color);
            pm_color.setX(color.getX() * color.getW());
            pm_color.setY(color.getY() * color.getW());
            pm_color.setZ(color.getZ() * color.getW());
            uint32_t bcolor = dmGraphics::PackRGBA(pm_color);

            const uint32_t perimeterVertices = dmMath::Max<uint32_t>(4, dmGui::GetNodePerimeterVertices(scene, nodes[i]));
            const float innerMultiplier = dmGui::GetNodeInnerRadius(scene, nodes[i]) / size.getX();
            const dmGui::PieBounds outerBounds = dmGui::GetNodeOuterBounds(scene, nodes[i]);

            const float PI = 3.1415926535f;
            const float ad = PI * 2.0f / (float)perimeterVertices;

            float stopAngle = dmGui::GetNodePieFillAngle(scene, nodes[i]);
            bool backwards = false;
            if (stopAngle < 0)
            {
                stopAngle = -stopAngle;
                backwards = true;
            }

            stopAngle = dmMath::Min(360.0f, stopAngle) * PI / 180.0f;
            const uint32_t generate = ceilf(stopAngle / ad) + 1;

            float lastAngle = 0;
            float nextCorner = 0.25f * PI; // upper right rectangle corner at 45 deg
            bool first = true;
            for (int j=0;j!=generate;j++)
            {
                float a;
                if (j == (generate-1))
                    a = stopAngle;
                else
                    a = ad * j;

                if (outerBounds == dmGui::PIEBOUNDS_RECTANGLE)
                {
                    // insert extra vertex (and ignore == case)
                    if (lastAngle < nextCorner && a >= nextCorner)
                    {
                        a = nextCorner;
                        nextCorner += 0.50f * PI;
                        --j;
                    }

                    lastAngle = a;
                }

                const float s = dmTrigLookup::Sin(backwards ? -a : a);
                const float c = dmTrigLookup::Cos(backwards ? -a : a);

                // make inner vertex
                float u = 0.5f + innerMultiplier * c;
                float v = 0.5f + innerMultiplier * s;
                BoxVertex vInner(node_transforms[i] * Vectormath::Aos::Point3(u,v,0), u, 1-v, bcolor);

                // make outer vertex
                float d;
                if (outerBounds == dmGui::PIEBOUNDS_RECTANGLE)
                    d = 0.5f / dmMath::Max(dmMath::Abs(s), dmMath::Abs(c));
                else
                    d = 0.5f;

                u = 0.5f + d * c;
                v = 0.5f + d * s;
                BoxVertex vOuter(node_transforms[i] * Vectormath::Aos::Point3(u,v,0), u, 1-v, bcolor);

                // both inner & outer are doubled at first / last entry to generate degenerate triangles
                // for the triangle strip, allowing more than one pie to be chained together in the same
                // drawcall.
                if (first)
                {
                    gui_world->m_ClientVertexBuffer.Push(vInner);
                    first = false;
                }

                gui_world->m_ClientVertexBuffer.Push(vInner);
                gui_world->m_ClientVertexBuffer.Push(vOuter);

                if (j == generate-1)
                    gui_world->m_ClientVertexBuffer.Push(vOuter);
            }
        }

        ro.m_VertexCount = gui_world->m_ClientVertexBuffer.Size() - ro.m_VertexStart;

        dmRender::AddToRender(gui_context->m_RenderContext, &ro);
    }

    void RenderNodes(dmGui::HScene scene,
                    dmGui::HNode* nodes,
                    const Matrix4* node_transforms,
                    const Vector4* node_colors,
                    uint32_t node_count,
                    const dmGui::ScissorClippingRenderState* scissor_clipping_render_states,
                    const dmGui::StencilClippingRenderState* stencil_clipping_render_states,
                    void* context)
    {
        if (node_count == 0)
            return;

        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;
        ScissorClippingState scissor_clipping_state;
        InitializeScissorClipping(scissor_clipping_state, scene, gui_context, scissor_clipping_render_states);
        StencilClippingState stencil_clipping_state;
        InitializeStencilClipping(stencil_clipping_state, scene, gui_context, stencil_clipping_render_states);

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
            bool apply_clipping = UpdateScissorClipping(scissor_clipping_state, node);
            apply_clipping |= UpdateStencilClipping(stencil_clipping_state, node);

            bool batch_change = (node_type != prev_node_type || blend_mode != prev_blend_mode || texture != prev_texture || font != prev_font);
            bool flush = ((i > 0) && batch_change) || apply_clipping;

            if (flush) {
                uint32_t n = i - start;

                switch (prev_node_type)
                {
                    case dmGui::NODE_TYPE_TEXT:
                        RenderTextNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
                        break;
                    case dmGui::NODE_TYPE_BOX:
                        RenderBoxNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, scissor_clipping_state, context);
                        break;
                    case dmGui::NODE_TYPE_PIE:
                        RenderPieNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
                        break;
                    default:
                        break;
                }

                start = i;

                /*
                 * Consecutive nodes of the same type will get the some
                 * pseudo-z for batching. This is an approximation of the correct
                 * layer-rendering.
                 */
                gui_context->m_NextZ++;

                ApplyScissorClipping(scissor_clipping_state, node_transforms + start);
                ApplyStencilClipping(stencil_clipping_state, scissor_clipping_state);
            }
            prev_node_type = node_type;
            prev_blend_mode = blend_mode;
            prev_texture = texture;
            prev_font = font;

            ++i;
        }

        uint32_t n = i - start;
        if (n > 0) {
            switch (prev_node_type)
            {
                case dmGui::NODE_TYPE_TEXT:
                    RenderTextNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
                    break;
                case dmGui::NODE_TYPE_PIE:
                    RenderPieNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, context);
                    break;
                case dmGui::NODE_TYPE_BOX:
                    RenderBoxNodes(scene, nodes + start, node_transforms + start, node_colors + start, n, scissor_clipping_state, context);
                    break;
                default:
                    break;
            }
            gui_context->m_NextZ++;
        }

        dmGraphics::SetVertexBufferData(gui_world->m_VertexBuffer,
                                        gui_world->m_ClientVertexBuffer.Size() * sizeof(BoxVertex),
                                        gui_world->m_ClientVertexBuffer.Begin(),
                                        dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        ResetScissorClipping(scissor_clipping_state);
        ResetStencilClipping(stencil_clipping_state);
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

        dmGraphics::TextureCreationParams tcparams;
        dmGraphics::TextureParams tparams;

        tcparams.m_Width = width;
        tcparams.m_Height = height;
        tcparams.m_OriginalWidth = width;
        tcparams.m_OriginalHeight = height;

        tparams.m_Width = width;
        tparams.m_Height = height;
        tparams.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tparams.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tparams.m_Data = buffer;
        tparams.m_DataSize = dmImage::BytesPerPixel(type) * width * height;
        tparams.m_Format = ToGraphicsFormat(type);

        dmGraphics::HTexture t =  dmGraphics::NewTexture(gcontext, tcparams);
        dmGraphics::SetTexture(t, tparams);
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

    dmGameObject::CreateResult CompGuiAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        gui_component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompGuiUpdate(const dmGameObject::ComponentsUpdateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiContext* gui_context = (GuiContext*)params.m_Context;

        // update
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* gui_component = gui_world->m_Components[i];
            if (gui_component->m_Enabled && gui_component->m_AddedToUpdate) {
                dmGui::UpdateScene(gui_component->m_Scene, params.m_UpdateContext->m_DT);
            }
        }

        RenderGuiContext render_gui_context;
        render_gui_context.m_RenderContext = gui_context->m_RenderContext;
        render_gui_context.m_GuiWorld = gui_world;
        render_gui_context.m_NextZ = 0;


        uint32_t total_node_count = 0;
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* c = gui_world->m_Components[i];
            if (c->m_Enabled && c->m_AddedToUpdate)
            {
                total_node_count += dmGui::GetNodeCount(c->m_Scene);
            }
        }

        uint32_t total_gui_render_objects_count = (total_node_count<<1) + (total_node_count>>3);
        if (gui_world->m_GuiRenderObjects.Capacity() < total_gui_render_objects_count)
        {
            // NOTE: m_GuiRenderObjects *before* rendering as pointers
            // to render-objects are passed to the render-system
            // Given batching the capacity is perhaps a bit over the top
            // We also need to include one possible state per node + worst case batching every 8th
            gui_world->m_GuiRenderObjects.SetCapacity(total_gui_render_objects_count);
        }
        gui_world->m_GuiRenderObjects.SetSize(0);
        gui_world->m_ClientVertexBuffer.SetSize(0);
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* c = gui_world->m_Components[i];
            if (c->m_Enabled && c->m_AddedToUpdate) {
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
