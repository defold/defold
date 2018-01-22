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
#include <render/display_profiles.h>
#include <render/font_renderer.h>
#include <gameobject/gameobject_ddf.h>
#include <render/render_ddf.h>

#include "comp_gui.h"
#include "comp_spine_model.h"

#include "../resources/res_gui.h"
#include "../gamesys.h"
#include "../gamesys_private.h"

extern unsigned char GUI_VPC[];
extern uint32_t GUI_VPC_SIZE;

extern unsigned char GUI_FPC[];
extern uint32_t GUI_FPC_SIZE;

namespace dmGameSystem
{
    dmGui::FetchTextureSetAnimResult FetchTextureSetAnimCallback(void*, dmhash_t, dmGui::TextureSetAnimDesc*);
    bool FetchRigSceneDataCallback(void* spine_scene, dmhash_t rig_scene_id, dmGui::RigSceneDataDesc* out_data);
    dmParticle::FetchAnimationResult FetchAnimationCallback(void* texture_set_ptr, dmhash_t animation, dmParticle::AnimationData* out_data); // implemention in comp_particlefx.cpp

    // Translation table to translate from dmGameSystemDDF playback mode into dmGui playback mode.
    static struct PlaybackGuiToRig
    {
        dmGui::Playback m_Table[dmGui::PLAYBACK_COUNT];
        PlaybackGuiToRig()
        {
            m_Table[dmGameSystemDDF::PLAYBACK_NONE]            = dmGui::PLAYBACK_NONE;
            m_Table[dmGameSystemDDF::PLAYBACK_ONCE_FORWARD]    = dmGui::PLAYBACK_ONCE_FORWARD;
            m_Table[dmGameSystemDDF::PLAYBACK_ONCE_BACKWARD]   = dmGui::PLAYBACK_ONCE_BACKWARD;
            m_Table[dmGameSystemDDF::PLAYBACK_LOOP_FORWARD]    = dmGui::PLAYBACK_LOOP_FORWARD;
            m_Table[dmGameSystemDDF::PLAYBACK_LOOP_BACKWARD]   = dmGui::PLAYBACK_LOOP_BACKWARD;
            m_Table[dmGameSystemDDF::PLAYBACK_LOOP_PINGPONG]   = dmGui::PLAYBACK_LOOP_PINGPONG;
            m_Table[dmGameSystemDDF::PLAYBACK_ONCE_PINGPONG]   = dmGui::PLAYBACK_ONCE_PINGPONG;
        }
    } ddf_playback_map;

    static struct BlendModeParticleToGui
    {
    	dmGui::BlendMode m_Table[4];
    	BlendModeParticleToGui()
    	{
    		m_Table[dmParticleDDF::BLEND_MODE_ALPHA]		= dmGui::BLEND_MODE_ALPHA;
    		m_Table[dmParticleDDF::BLEND_MODE_MULT]			= dmGui::BLEND_MODE_MULT;
    		m_Table[dmParticleDDF::BLEND_MODE_ADD]			= dmGui::BLEND_MODE_ADD;
    		m_Table[dmParticleDDF::BLEND_MODE_ADD_ALPHA]	= dmGui::BLEND_MODE_ADD_ALPHA;
    	}
    } ddf_blendmode_map;

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

        dmRig::NewContextParams rig_params = {0};
        rig_params.m_Context = &gui_world->m_RigContext;
        rig_params.m_MaxRigInstanceCount = gui_context->m_MaxSpineCount;
        dmRig::Result rr = dmRig::NewContext(rig_params);
        if (rr != dmRig::RESULT_OK)
        {
            dmLogFatal("Unable to create gui rig context: %d", rr);
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        gui_world->m_Components.SetCapacity(gui_context->m_MaxGuiComponents);

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

        gui_world->m_MaxParticleFXCount = gui_context->m_MaxParticleFXCount;
        gui_world->m_MaxParticleCount = gui_context->m_MaxParticleCount;
        gui_world->m_ParticleContext = dmParticle::CreateContext(gui_world->m_MaxParticleFXCount, gui_world->m_MaxParticleCount);

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
        dmParticle::DestroyContext(gui_world->m_ParticleContext);

        dmGraphics::DeleteVertexDeclaration(gui_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(gui_world->m_VertexBuffer);
        dmGraphics::DeleteTexture(gui_world->m_WhiteTexture);

        dmRig::DeleteContext(gui_world->m_RigContext);

        delete gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    bool SetNode(const dmGui::HScene scene, dmGui::HNode n, const dmGuiDDF::NodeDesc* node_desc)
    {
        bool result = true;

        // properties
        dmGui::SetNodePosition(scene, n, Point3(node_desc->m_Position.getXYZ()));
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_ROTATION, node_desc->m_Rotation);
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SCALE, node_desc->m_Scale);
        Vector4 color;
        color.setXYZ(node_desc->m_Color.getXYZ());
        color.setW(node_desc->m_Alpha);
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_COLOR, color);
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SIZE, node_desc->m_Size);
        color.setXYZ(node_desc->m_Outline.getXYZ());
        color.setW(node_desc->m_OutlineAlpha);
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_OUTLINE, color);
        color.setXYZ(node_desc->m_Shadow.getXYZ());
        color.setW(node_desc->m_ShadowAlpha);
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SHADOW, color);
        dmGui::SetNodeProperty(scene, n, dmGui::PROPERTY_SLICE9, node_desc->m_Slice9);

        // texture and texture animation setup
        dmGui::SetNodeSizeMode(scene, n, (dmGui::SizeMode) node_desc->m_SizeMode);
        if (node_desc->m_Texture != 0x0 && *node_desc->m_Texture != '\0')
        {
            const size_t path_str_size_max = 512;
            size_t path_str_size = strlen(node_desc->m_Texture)+1;
            if(path_str_size > path_str_size_max)
            {
                dmLogError("The texture/animation '%s' could not be set for '%s', name too long by %zu characters (max %zu).", node_desc->m_Texture, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", path_str_size_max-path_str_size, path_str_size_max);
                result = false;
            }
            else
            {
                char texture_str[path_str_size_max];
                dmStrlCpy(texture_str, node_desc->m_Texture, path_str_size);

                char* texture_anim_name = strstr(texture_str, "/");
                if(texture_anim_name)
                    *texture_anim_name++ = 0;

                dmGui::Result gui_result = dmGui::SetNodeTexture(scene, n, texture_str);
                if (gui_result != dmGui::RESULT_OK)
                {
                    dmLogError("The texture '%s' could not be set for '%s', result: %d.", texture_str, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", gui_result);
                    result = false;
                }

                if(texture_anim_name != NULL)
                {
                    gui_result = dmGui::PlayNodeFlipbookAnim(scene, n, texture_anim_name);
                    if (gui_result != dmGui::RESULT_OK)
                    {
                        dmLogError("The texture animation '%s' in texture '%s' could not be set for '%s', result: %d.", texture_anim_name, texture_str, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", gui_result);
                        result = false;
                    }
                }
            }
        }
        else
        {
            dmGui::SetNodeTexture(scene, n, "");
        }

        // layer setup
        if (node_desc->m_Layer != 0x0 && *node_desc->m_Layer != '\0')
        {
            dmGui::Result gui_result = dmGui::SetNodeLayer(scene, n, node_desc->m_Layer);
            if (gui_result != dmGui::RESULT_OK)
            {
                dmLogError("The layer '%s' could not be set for the '%s', result: %d.", node_desc->m_Layer, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", gui_result);
                dmGui::SetNodeLayer(scene, n, "");
            }
        }
        else
        {
            dmGui::SetNodeLayer(scene, n, "");
        }

        // attributes
        dmGui::BlendMode blend_mode = (dmGui::BlendMode) node_desc->m_BlendMode;
        // Add-alpha is deprecated because of premultiplied alpha and replaced by Add
        if (blend_mode == dmGui::BLEND_MODE_ADD_ALPHA)
            blend_mode = dmGui::BLEND_MODE_ADD;
        dmGui::SetNodeBlendMode(scene, n, blend_mode);

        dmGui::SetNodePivot(scene, n, (dmGui::Pivot) node_desc->m_Pivot);
        dmGui::SetNodeXAnchor(scene, n, (dmGui::XAnchor) node_desc->m_Xanchor);
        dmGui::SetNodeYAnchor(scene, n, (dmGui::YAnchor) node_desc->m_Yanchor);
        dmGui::SetNodeAdjustMode(scene, n, (dmGui::AdjustMode) node_desc->m_AdjustMode);
        dmGui::SetNodeInheritAlpha(scene, n, node_desc->m_InheritAlpha);

        dmGui::SetNodeClippingMode(scene, n, (dmGui::ClippingMode) node_desc->m_ClippingMode);
        dmGui::SetNodeClippingVisible(scene, n, node_desc->m_ClippingVisible);
        dmGui::SetNodeClippingInverted(scene, n, node_desc->m_ClippingInverted);

        if (node_desc->m_SpineNodeChild) {
            dmGui::SetNodeIsBone(scene, n, true);
        }

        // type specific attributes
        switch(node_desc->m_Type)
        {
            case dmGuiDDF::NodeDesc::TYPE_TEXT:
                dmGui::SetNodeText(scene, n, node_desc->m_Text);
                dmGui::SetNodeFont(scene, n, node_desc->m_Font);
                dmGui::SetNodeLineBreak(scene, n, node_desc->m_LineBreak);
                dmGui::SetNodeTextLeading(scene, n, node_desc->m_TextLeading);
                dmGui::SetNodeTextTracking(scene, n, node_desc->m_TextTracking);
            break;

            case dmGuiDDF::NodeDesc::TYPE_PIE:
                dmGui::SetNodePerimeterVertices(scene, n, node_desc->m_Perimetervertices);
                dmGui::SetNodeInnerRadius(scene, n, node_desc->m_Innerradius);
                dmGui::SetNodeOuterBounds(scene, n, (dmGui::PieBounds) node_desc->m_Outerbounds);
                dmGui::SetNodePieFillAngle(scene, n, node_desc->m_Piefillangle);
            break;

            case dmGuiDDF::NodeDesc::TYPE_SPINE:
                dmGui::SetNodeSpineScene(scene, n, node_desc->m_SpineScene, dmHashString64(node_desc->m_SpineSkin), dmHashString64(node_desc->m_SpineDefaultAnimation), false);
            break;

            case dmGuiDDF::NodeDesc::TYPE_PARTICLEFX:
                dmGui::SetNodeParticlefx(scene, n, dmHashString64(node_desc->m_Particlefx));
            break;

            case dmGuiDDF::NodeDesc::TYPE_TEMPLATE:
                dmLogError("Template nodes are not supported in run-time '%s', result: %d.", node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", dmGui::RESULT_INVAL_ERROR);
                result = false;
            break;

            default:
            break;
        }

        dmGui::SetNodeResetPoint(scene, n);
        return result;
    }

    void SetNodeCallback(const dmGui::HScene scene, dmGui::HNode n, const void* node_desc)
    {
        SetNode(scene, n, (const dmGuiDDF::NodeDesc*) node_desc);
    }

    void OnWindowResizeCallback(const dmGui::HScene scene, uint32_t width, uint32_t height)
    {
        dmArray<dmhash_t> scene_layouts;
        uint16_t layout_count = dmGui::GetLayoutCount(scene);
        scene_layouts.SetCapacity(layout_count);
        for(uint16_t i = 0; i < layout_count; ++i)
        {
            dmhash_t id;
            dmGui::Result r = dmGui::GetLayoutId(scene, i, id);
            if(r != dmGui::RESULT_OK)
            {
                dmLogError("GetLayoutId failed(%d). Index out of range", r);
                break;
            }
            scene_layouts.Push(id);
        }

        dmRender::HDisplayProfiles display_profiles = (dmRender::HDisplayProfiles) dmGui::GetDisplayProfiles(scene);
        dmhash_t layout_id = dmRender::GetOptimalDisplayProfile(display_profiles, width, height, dmGui::GetDisplayDpi(scene), &scene_layouts);
        if(layout_id != GetLayout(scene))
        {
            dmhash_t current_layout_id = GetLayout(scene);

            dmRender::DisplayProfileDesc profile_desc;
            GetDisplayProfileDesc(display_profiles, layout_id, profile_desc);
            dmGui::SetSceneResolution(scene, profile_desc.m_Width, profile_desc.m_Height);
            dmGui::SetLayout(scene, layout_id, SetNodeCallback);

            // Notify the scene script. The callback originates from the dmGraphics::SetWindowSize firing the callback.
            char buf[sizeof(dmMessage::Message) + sizeof(dmGuiDDF::LayoutChanged)];
            dmMessage::Message* message = (dmMessage::Message*)buf;
            message->m_Sender = dmMessage::URL();
            message->m_Receiver = dmMessage::URL();
            message->m_Id = dmHashString64("layout_changed");
            message->m_Descriptor = (uintptr_t)dmGuiDDF::LayoutChanged::m_DDFDescriptor;
            message->m_DataSize = sizeof(dmGuiDDF::LayoutChanged);
            dmGuiDDF::LayoutChanged* message_data = (dmGuiDDF::LayoutChanged*)message->m_Data;
            message_data->m_Id = layout_id;
            message_data->m_PreviousId = current_layout_id;
            DispatchMessage(scene, message);
        }
    }

    bool SetupGuiScene(dmGui::HScene scene, GuiSceneResource* scene_resource)
    {
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;
        dmGui::SetSceneScript(scene, scene_resource->m_Script);

        bool result = true;

        dmGui::SetMaterial(scene, scene_resource->m_Material);
        dmGui::SetSceneAdjustReference(scene, (dmGui::AdjustReference)scene_desc->m_AdjustReference);

        for (uint32_t i = 0; i < scene_resource->m_FontMaps.Size(); ++i)
        {
            const char* name = scene_desc->m_Fonts[i].m_Name;
            dmGui::Result r = dmGui::AddFont(scene, name, (void*) scene_resource->m_FontMaps[i]);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add font '%s' to scene (%d)", name,  r);
                return false;
            }
        }

        for (uint32_t i = 0; i < scene_resource->m_RigScenes.Size(); ++i)
        {
            const char* name = scene_desc->m_SpineScenes[i].m_Name;
            dmGui::Result r = dmGui::AddSpineScene(scene, name, (void*) scene_resource->m_RigScenes[i]);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add spine scene '%s' to GUI scene (%d)", name,  r);
                return false;
            }
        }

        for (uint32_t i = 0; i < scene_resource->m_ParticlePrototypes.Size(); ++i)
        {
            const char* name = scene_desc->m_Particlefxs.m_Data[i].m_Name;
            dmGui::Result r = dmGui::AddParticlefx(scene, name, (void*) scene_resource->m_ParticlePrototypes[i]);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add particlefx '%s' to GUI scene (%d)", name, r);
                return false;
            }
        }

        for (uint32_t i = 0; i < scene_resource->m_GuiTextureSets.Size(); ++i)
        {
            const char* name = scene_desc->m_Textures[i].m_Name;
            dmGraphics::HTexture texture = scene_resource->m_GuiTextureSets[i].m_Texture;
            dmGui::Result r = dmGui::AddTexture(scene, name, (void*) texture, (void*) scene_resource->m_GuiTextureSets[i].m_TextureSet, dmGraphics::GetTextureWidth(texture), dmGraphics::GetTextureHeight(texture));
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

        uint32_t layouts_count = scene_desc->m_Layouts.m_Count;
        if(layouts_count != 0)
        {
            dmGui::AllocateLayouts(scene, scene_desc->m_Nodes.m_Count, layouts_count);
            for (uint32_t i = 0; i < layouts_count; ++i)
            {
                const char* name = scene_desc->m_Layouts[i].m_Name;
                dmGui::Result r = dmGui::AddLayout(scene, name);
                if (r != dmGui::RESULT_OK) {
                    dmLogError("Unable to add layout '%s' to scene (%d)", name,  r);
                    return false;
                }
            }
        }

        for (uint32_t i = 0; i < scene_desc->m_Nodes.m_Count; ++i)
        {
            // NOTE: We assume that the enums in dmGui and dmGuiDDF have the same values
            const dmGuiDDF::NodeDesc* node_desc = &scene_desc->m_Nodes[i];
            dmGui::NodeType type = (dmGui::NodeType) node_desc->m_Type;
            Vector4 position = node_desc->m_Position;
            Vector4 size = node_desc->m_Size;
            dmGui::HNode n = dmGui::NewNode(scene, Point3(position.getXYZ()), Vector3(size.getXYZ()), type);
            if (n)
            {
                if (node_desc->m_Id)
                    dmGui::SetNodeId(scene, n, node_desc->m_Id);
                if(!SetNode(scene, n, node_desc))
                    return false;
                if(layouts_count != 0)
                    dmGui::SetNodeLayoutDesc(scene, n, node_desc, 0, layouts_count);
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

        if(layouts_count != 0)
        {
            for (uint32_t l = 0; l < layouts_count; ++l)
            {
                uint16_t layout_index = dmGui::GetLayoutIndex(scene, dmHashString64(scene_desc->m_Layouts[l].m_Name));
                for (uint32_t i = 0; i < scene_desc->m_Layouts[l].m_Nodes.m_Count; ++i)
                {
                    const dmGuiDDF::NodeDesc* node_desc = &scene_desc->m_Layouts[l].m_Nodes[i];
                    dmGui::HNode n = dmGui::GetNodeById(scene, node_desc->m_Id);
                    if (n)
                    {
                        dmGui::SetNodeLayoutDesc(scene, n, node_desc, layout_index, layout_index);
                    }
                    else
                    {
                        dmLogError("The default node for '%s' could not be found in the scene.", node_desc->m_Id);
                    }
                }
            }

            // we might have any resolution starting the scene, so let's set the best alternative layout directly
            dmArray<dmhash_t> scene_layouts;
            scene_layouts.SetCapacity(layouts_count+1);
            for(uint16_t i = 0; i < layouts_count+1; ++i)
            {
                dmhash_t id;
                dmGui::Result r = dmGui::GetLayoutId(scene, i, id);
                if(r != dmGui::RESULT_OK)
                {
                    dmLogError("GetLayoutId failed(%d). Index out of range", r);
                    break;
                }
                scene_layouts.Push(id);
            }

            uint32_t display_width, display_height;
            dmGui::GetPhysicalResolution(scene, display_width, display_height);
            dmRender::HDisplayProfiles display_profiles = (dmRender::HDisplayProfiles)dmGui::GetDisplayProfiles(scene);
            dmhash_t layout_id = dmRender::GetOptimalDisplayProfile(display_profiles, display_width, display_height, 0, &scene_layouts);
            if(layout_id != dmGui::DEFAULT_LAYOUT)
            {
                dmRender::DisplayProfileDesc profile_desc;
                GetDisplayProfileDesc(display_profiles, layout_id, profile_desc);
                dmGui::SetSceneResolution(scene, profile_desc.m_Width, profile_desc.m_Height);
                dmGui::SetLayout(scene, layout_id, SetNodeCallback);
            }
        }

        return result;
    }

    dmGameObject::CreateResult CompGuiCreate(const dmGameObject::ComponentCreateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;

        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource;
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;

        GuiComponent* gui_component = new GuiComponent();
        gui_component->m_Instance = params.m_Instance;
        gui_component->m_ComponentIndex = params.m_ComponentIndex;
        gui_component->m_Enabled = 1;
        gui_component->m_AddedToUpdate = 0;

        dmGui::NewSceneParams scene_params;
        // 1024 is a hard cap since the render key has 10 bits for node index
        assert(scene_desc->m_MaxNodes <= 1024);
        scene_params.m_MaxNodes = scene_desc->m_MaxNodes;
        scene_params.m_MaxAnimations = 1024;
        scene_params.m_UserData = gui_component;
        scene_params.m_MaxFonts = 64;
        scene_params.m_MaxTextures = 128;
        scene_params.m_MaxParticlefx = gui_world->m_MaxParticleFXCount;
        scene_params.m_MaxSpineScenes = 128;
        scene_params.m_RigContext = gui_world->m_RigContext;
        scene_params.m_ParticlefxContext = gui_world->m_ParticleContext;
        scene_params.m_FetchTextureSetAnimCallback = &FetchTextureSetAnimCallback;
        scene_params.m_FetchRigSceneDataCallback = &FetchRigSceneDataCallback;
        scene_params.m_OnWindowResizeCallback = &OnWindowResizeCallback;
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
        dmRender::HRenderContext    m_RenderContext;
        GuiWorld*                   m_GuiWorld;

        // This order value is increased during rendering for each
        // render object generated, then used to make sure the final
        // draw order will follow the order objects are generated in,
        // and to allow font rendering to be sorted into the right place.
        uint32_t                    m_NextSortOrder;

        // true if the stencil is the first rendered (per scene)
        bool                        m_FirstStencil;
    };

    inline uint32_t MakeFinalRenderOrder(uint32_t scene_order, uint32_t sub_order)
    {
        return (scene_order << 16) + sub_order;
    }

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

    static void ApplyStencilClipping(RenderGuiContext* gui_context, const dmGui::StencilScope* state, dmRender::StencilTestParams& stp) {
        if (state != 0x0) {
            stp.m_Func = dmGraphics::COMPARE_FUNC_EQUAL;
            stp.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
            stp.m_OpDPFail = dmGraphics::STENCIL_OP_REPLACE;
            stp.m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE;
            stp.m_Ref = state->m_RefVal;
            stp.m_RefMask = state->m_TestMask;
            stp.m_BufferMask = state->m_WriteMask;
            stp.m_ColorBufferMask = state->m_ColorMask;
            if (gui_context->m_FirstStencil)
            {
                // Set the m_ClearBuffer for the first stencil of each scene so that scenes (and components) can share the stencil buffer.
                // This will result in a stencil buffer clear call before the batch is drawn.
                // The render module will internally disregard/optimise this clear request if the stencil buffer
                // has been cleared by the renderscript clear command prior to this buffer clear request.
                gui_context->m_FirstStencil = false;
                stp.m_ClearBuffer = 1;
            }
        } else {
            stp.m_Func = dmGraphics::COMPARE_FUNC_ALWAYS;
            stp.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
            stp.m_OpDPFail = dmGraphics::STENCIL_OP_KEEP;
            stp.m_OpDPPass = dmGraphics::STENCIL_OP_KEEP;
            stp.m_Ref = 0;
            stp.m_RefMask = 0xff;
            stp.m_BufferMask = 0xff;
            stp.m_ColorBufferMask = 0xf;
        }
    }

    static void ApplyStencilClipping(RenderGuiContext* gui_context, const dmGui::StencilScope* state, dmRender::RenderObject& ro) {
        ro.m_SetStencilTest = 1;
        ApplyStencilClipping(gui_context, state, ro.m_StencilTestParams);
    }

    static void ApplyStencilClipping(RenderGuiContext* gui_context, const dmGui::StencilScope* state, dmRender::DrawTextParams& params) {
        params.m_StencilTestParamsSet = 1;
        ApplyStencilClipping(gui_context, state, params.m_StencilTestParams);
    }

    void RenderTextNodes(dmGui::HScene scene,
                         const dmGui::RenderEntry* entries,
                         const Matrix4* node_transforms,
                         const float* node_opacities,
                         const dmGui::StencilScope** stencil_scopes,
                         uint32_t node_count,
                         void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;

        for (uint32_t i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = entries[i].m_Node;

            if (dmGui::GetNodeIsBone(scene, node)) {
                continue;
            }

            const Vector4& color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
            const Vector4& outline = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_OUTLINE);
            const Vector4& shadow = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_SHADOW);

            dmGui::NodeType node_type = dmGui::GetNodeType(scene, node);
            assert(node_type == dmGui::NODE_TYPE_TEXT);

            dmRender::DrawTextParams params;
            float opacity = node_opacities[i];
            params.m_FaceColor = Vector4(color.getXYZ(), opacity);
            params.m_OutlineColor = Vector4(outline.getXYZ(), outline.getW() * opacity);
            params.m_ShadowColor = Vector4(shadow.getXYZ(), shadow.getW() * opacity);
            params.m_Text = dmGui::GetNodeText(scene, node);
            params.m_WorldTransform = node_transforms[i];
            params.m_RenderOrder = dmGui::GetRenderOrder(scene);
            params.m_LineBreak = dmGui::GetNodeLineBreak(scene, node);
            params.m_Leading = dmGui::GetNodeTextLeading(scene, node);
            params.m_Tracking = dmGui::GetNodeTextTracking(scene, node);

            Vector4 size = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_SIZE);
            params.m_Width = size.getX();
            params.m_Height = size.getY();
            ApplyStencilClipping(gui_context, stencil_scopes[i], params);
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
            dmRender::DrawText(gui_context->m_RenderContext, (dmRender::HFontMap) dmGui::GetNodeFont(scene, node), 0, 0, params);
        }

        dmRender::FlushTexts(gui_context->m_RenderContext, dmRender::RENDER_ORDER_AFTER_WORLD, MakeFinalRenderOrder(dmGui::GetRenderOrder(scene), gui_context->m_NextSortOrder++), false);
    }

    void RenderParticlefxNodes(dmGui::HScene scene,
                          const dmGui::RenderEntry* entries,
                          const Matrix4* node_transforms,
                          const float* node_opacities,
                          const dmGui::StencilScope** stencil_scopes,
                          uint32_t node_count,
                          void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;
        dmGui::HNode first_node = entries[0].m_Node;
        dmParticle::EmitterRenderData* first_emitter_render_data = (dmParticle::EmitterRenderData*)entries[0].m_RenderData;
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_PARTICLEFX);

        uint32_t vb_max_size = dmParticle::GetMaxVertexBufferSize(gui_world->m_ParticleContext, dmParticle::PARTICLE_GUI) - gui_world->m_RenderedParticlesSize;
        uint32_t total_vertex_count = 0;
        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);
        GuiRenderObject& gro = gui_world->m_GuiRenderObjects[ro_count];
        dmRender::RenderObject& ro = gro.m_RenderObject;
        gro.m_SortOrder = gui_context->m_NextSortOrder++;

        ro.Init();
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_Material = (dmRender::HMaterial) dmGui::GetMaterial(scene);
        ro.m_Textures[0] = (dmGraphics::HTexture)first_emitter_render_data->m_Texture;

        // Offset capacity to fit vertices for all emitters we are about to render
        uint32_t vertex_count = 0;
        for (int i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = entries[i].m_Node;
            if (dmGui::GetNodeIsBone(scene, node)) {
                continue;
            }
            dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*)entries[i].m_RenderData;
            vertex_count += dmParticle::GetEmitterVertexCount(gui_world->m_ParticleContext, emitter_render_data->m_Instance, emitter_render_data->m_EmitterIndex);

            dmTransform::Transform transform = dmTransform::ToTransform(node_transforms[i]);
            dmParticle::SetPosition(gui_world->m_ParticleContext, emitter_render_data->m_Instance, Point3(transform.GetTranslation()));
            dmParticle::SetRotation(gui_world->m_ParticleContext, emitter_render_data->m_Instance, transform.GetRotation());
            dmParticle::SetScale(gui_world->m_ParticleContext, emitter_render_data->m_Instance, transform.GetUniformScale());
        }

        vertex_count = dmMath::Min(vertex_count, vb_max_size / (uint32_t)sizeof(ParticleGuiVertex));

        if (gui_world->m_ClientVertexBuffer.Remaining() < vertex_count) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, vertex_count));
        }

        ParticleGuiVertex *vb_begin = gui_world->m_ClientVertexBuffer.End();
        ParticleGuiVertex *vb_end = vb_begin;
        // One RO, but generate vertex data for each entry (emitter)
        for (int i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = entries[i].m_Node;
            if (dmGui::GetNodeIsBone(scene, node)) {
                continue;
            }
            const Vector4& nodecolor = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
            float opacity = node_opacities[i];
            Vector4 color = Vector4(nodecolor.getXYZ(), opacity);

            dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*)entries[i].m_RenderData;
            uint32_t vb_generate_size = 0;
            dmParticle::GenerateVertexData(
                gui_world->m_ParticleContext,
                gui_world->m_DT,
                emitter_render_data->m_Instance,
                emitter_render_data->m_EmitterIndex,
                color,
                (void*)vb_end,
                vb_max_size,
                &vb_generate_size,
                dmParticle::PARTICLE_GUI);

            uint32_t emitter_vertex_count = vb_generate_size / sizeof(ParticleGuiVertex);
            total_vertex_count += emitter_vertex_count;
            vb_end += emitter_vertex_count;
            vb_max_size -= vb_generate_size;
        }

        gui_world->m_RenderedParticlesSize += total_vertex_count * sizeof(ParticleGuiVertex);

        ro.m_VertexCount = total_vertex_count;
        dmGui::BlendMode blend_mode = ddf_blendmode_map.m_Table[first_emitter_render_data->m_BlendMode];
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;

        for (uint32_t i = 0; i < first_emitter_render_data->m_RenderConstantsSize; ++i)
        {
            dmParticle::RenderConstant* c = &first_emitter_render_data->m_RenderConstants[i];
            dmRender::EnableRenderObjectConstant(&ro, c->m_NameHash, c->m_Value);
        }

        ApplyStencilClipping(gui_context, stencil_scopes[0], ro);
        gui_world->m_ClientVertexBuffer.SetSize(vb_end - gui_world->m_ClientVertexBuffer.Begin());
    }

    void RenderSpineNodes(dmGui::HScene scene,
                          const dmGui::RenderEntry* entries,
                          const Matrix4* node_transforms,
                          const float* node_opacities,
                          const dmGui::StencilScope** stencil_scopes,
                          uint32_t node_count,
                          void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::HNode first_node = entries[0].m_Node;
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_SPINE);

        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);

        GuiRenderObject& gro = gui_world->m_GuiRenderObjects[ro_count];
        dmRender::RenderObject& ro = gro.m_RenderObject;
        gro.m_SortOrder = gui_context->m_NextSortOrder++;

        uint32_t vertex_count = 0;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            const dmGui::HNode node = entries[i].m_Node;
            if (dmGui::GetNodeIsBone(scene, node)) {
                continue;
            }
            const dmRig::HRigInstance rig_instance = dmGui::GetNodeRigInstance(scene, node);
            uint32_t count = dmRig::GetVertexCount(rig_instance);
            vertex_count += count;
        }

        ro.Init();
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_VertexCount = vertex_count;
        ro.m_Material = (dmRender::HMaterial) dmGui::GetMaterial(scene);

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;

        ApplyStencilClipping(gui_context, stencil_scopes[0], ro);

        // Set default texture
        void* texture = dmGui::GetNodeTexture(scene, first_node);
        if (texture) {
            ro.m_Textures[0] = (dmGraphics::HTexture) texture;
        } else {
            ro.m_Textures[0] = gui_world->m_WhiteTexture;
        }

        if (gui_world->m_ClientVertexBuffer.Remaining() < vertex_count) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, vertex_count));
        }

        // Fill in vertex buffer
        BoxVertex *vb_begin = gui_world->m_ClientVertexBuffer.End();
        BoxVertex *vb_end = vb_begin;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            const dmGui::HNode node = entries[i].m_Node;
            if (dmGui::GetNodeIsBone(scene, node)) {
                continue;
            }
            const dmRig::HRigContext rig_context = gui_world->m_RigContext;
            const dmRig::HRigInstance rig_instance = dmGui::GetNodeRigInstance(scene, node);
            float opacity = node_opacities[i];
            Vector4 color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
            color = Vector4(color.getXYZ(), opacity);
            vb_end = (BoxVertex*)dmRig::GenerateVertexData(rig_context, rig_instance, node_transforms[i], Matrix4::identity(), color, dmRig::RIG_VERTEX_FORMAT_SPINE, (void*)vb_end);
        }
        gui_world->m_ClientVertexBuffer.SetSize(vb_end - gui_world->m_ClientVertexBuffer.Begin());
    }

    void RenderBoxNodes(dmGui::HScene scene,
                        const dmGui::RenderEntry* entries,
                        const Matrix4* node_transforms,
                        const float* node_opacities,
                        const dmGui::StencilScope** stencil_scopes,
                        uint32_t node_count,
                        void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::HNode first_node = entries[0].m_Node;
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_BOX);

        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);

        GuiRenderObject& gro = gui_world->m_GuiRenderObjects[ro_count];
        dmRender::RenderObject& ro = gro.m_RenderObject;
        gro.m_SortOrder = gui_context->m_NextSortOrder++;

        // NOTE: ro might be uninitialized and we don't want to create a stack allocated temporary
        // See case 2264
        ro.Init();

        ApplyStencilClipping(gui_context, stencil_scopes[0], ro);

        const int verts_per_node = 6*9;
        const uint32_t max_total_vertices = verts_per_node * node_count;

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_Material = (dmRender::HMaterial) dmGui::GetMaterial(scene);

        // Set default texture
        void* texture = dmGui::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = (dmGraphics::HTexture) texture;
        else
            ro.m_Textures[0] = gui_world->m_WhiteTexture;

        if (gui_world->m_ClientVertexBuffer.Remaining() < (max_total_vertices)) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, max_total_vertices));
        }

        // 9-slice values are specified with reference to the original graphics and not by
        // the possibly stretched texture.
        float org_width = (float)dmGraphics::GetOriginalTextureWidth(ro.m_Textures[0]);
        float org_height = (float)dmGraphics::GetOriginalTextureHeight(ro.m_Textures[0]);
        assert(org_width > 0 && org_height > 0);

        int rendered_vert_count = 0;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            const dmGui::HNode node = entries[i].m_Node;

            if (dmGui::GetNodeIsBone(scene, node)) {
                continue;
            }
            rendered_vert_count += verts_per_node;

            const Vector4& color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);

            // Pre-multiplied alpha
            Vector4 pm_color(color.getXYZ(), node_opacities[i]);
            uint32_t bcolor = dmGraphics::PackRGBA(pm_color);

            Vector4 slice9 = dmGui::GetNodeSlice9(scene, node);
            Point3 size = dmGui::GetNodeSize(scene, node);

            // disable slice9 computation below a certain dimension
            // (avoid div by zero)
            const float s9_min_dim = 0.001f;

            const float su = 1.0f / org_width;
            const float sv = 1.0f / org_height;
            const float sx = size.getX() > s9_min_dim ? 1.0f / size.getX() : 0;
            const float sy = size.getY() > s9_min_dim ? 1.0f / size.getY() : 0;

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
            xs[0] = ys[0] = 0;
            xs[3] = ys[3] = 1;
            bool uv_rotated;
            const float* tc = dmGui::GetNodeFlipbookAnimUV(scene, node);
            if(tc)
            {
                static const uint32_t uvIndex[2][4] = {{0,1,2,3}, {3,2,1,0}};
                uv_rotated = tc[0] != tc[2] && tc[3] != tc[5];
                bool flip_u, flip_v;
                GetNodeFlipbookAnimUVFlip(scene, node, flip_u, flip_v);
                if(uv_rotated)
                {
                    const uint32_t *uI = flip_v ? uvIndex[1] : uvIndex[0];
                    const uint32_t *vI = flip_u ? uvIndex[1] : uvIndex[0];
                    us[uI[0]] = tc[0];
                    us[uI[1]] = tc[0] + (su * slice9.getW());
                    us[uI[2]] = tc[2] - (su * slice9.getY());
                    us[uI[3]] = tc[2];
                    vs[vI[0]] = tc[1];
                    vs[vI[1]] = tc[1] - (sv * slice9.getX());
                    vs[vI[2]] = tc[5] + (sv * slice9.getZ());
                    vs[vI[3]] = tc[5];
                }
                else
                {
                    const uint32_t *uI = flip_u ? uvIndex[1] : uvIndex[0];
                    const uint32_t *vI = flip_v ? uvIndex[1] : uvIndex[0];
                    us[uI[0]] = tc[0];
                    us[uI[1]] = tc[0] + (su * slice9.getX());
                    us[uI[2]] = tc[4] - (su * slice9.getZ());
                    us[uI[3]] = tc[4];
                    vs[vI[0]] = tc[1];
                    vs[vI[1]] = tc[1] + (sv * slice9.getW());
                    vs[vI[2]] = tc[3] - (sv * slice9.getY());
                    vs[vI[3]] = tc[3];
                }
            }
            else
            {
                uv_rotated = false;
                us[0] = 0;
                us[1] = su * slice9.getX();
                us[2] = 1 - su * slice9.getZ();
                us[3] = 1;
                vs[0] = 0;
                vs[1] = sv * slice9.getW();
                vs[2] = 1 - sv * slice9.getY();
                vs[3] = 1;
            }

            xs[1] = sx * slice9.getX();
            xs[2] = 1 - sx * slice9.getZ();
            ys[1] = sy * slice9.getW();
            ys[2] = 1 - sy * slice9.getY();

            const Matrix4* transform = &node_transforms[i];
            Vectormath::Aos::Vector4 pts[4][4];
            for (int y=0;y<4;y++)
            {
                for (int x=0;x<4;x++)
                {
                    pts[y][x] = (*transform * Vectormath::Aos::Point3(xs[x], ys[y], 0));
                }
            }

            BoxVertex v00, v10, v01, v11;
            v00.SetColor(bcolor);
            v10.SetColor(bcolor);
            v01.SetColor(bcolor);
            v11.SetColor(bcolor);
            for (int y=0;y<3;y++)
            {
                for (int x=0;x<3;x++)
                {
                    const int x0 = x;
                    const int x1 = x+1;
                    const int y0 = y;
                    const int y1 = y+1;
                    v00.SetPosition(pts[y0][x0]);
                    v10.SetPosition(pts[y0][x1]);
                    v01.SetPosition(pts[y1][x0]);
                    v11.SetPosition(pts[y1][x1]);
                    if(uv_rotated)
                    {
                        v00.SetUV(us[y0], vs[x0]);
                        v10.SetUV(us[y0], vs[x1]);
                        v01.SetUV(us[y1], vs[x0]);
                        v11.SetUV(us[y1], vs[x1]);
                    }
                    else
                    {
                        v00.SetUV(us[x0], vs[y0]);
                        v10.SetUV(us[x1], vs[y0]);
                        v01.SetUV(us[x0], vs[y1]);
                        v11.SetUV(us[x1], vs[y1]);
                    }
                    gui_world->m_ClientVertexBuffer.Push(v00);
                    gui_world->m_ClientVertexBuffer.Push(v10);
                    gui_world->m_ClientVertexBuffer.Push(v11);
                    gui_world->m_ClientVertexBuffer.Push(v00);
                    gui_world->m_ClientVertexBuffer.Push(v11);
                    gui_world->m_ClientVertexBuffer.Push(v01);
                }
            }
        }
        ro.m_VertexCount = rendered_vert_count;
    }

    // Computes max vertices required in the vertex buffer to draw a pie node with a
    // given number of perimeter vertices in its configuration.
    inline uint32_t ComputeRequiredVertices(uint32_t perimeter_vertices)
    {
        // 1.  Minimum is capped to 4
        // 2a. There will always be one extra needed to complete a full fill.
        //     I.e. an 8-gon will need 9 vertices around, where the first and last
        //     overlap. (+1)
        // 2b. If the shape has rectangular bounds and pass through all four corners,
        //     there will be 4 vertices inserted around the loop. (+4)
        // 3.  Each vertex around the perimeter has its twin along the inside (*2)
        // 4.  To draw all pie nodes in one draw call as a strip, each pie adds two
        //     doubled vertices to tie it together (+2)
        return 2 * (dmMath::Max<uint32_t>(perimeter_vertices, 4) + 5) + 2;
    }

    void RenderPieNodes(dmGui::HScene scene,
                        const dmGui::RenderEntry* entries,
                        const Matrix4* node_transforms,
                        const float* node_opacities,
                        const dmGui::StencilScope** stencil_scopes,
                        uint32_t node_count,
                        void* context)
    {
        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::HNode first_node = entries[0].m_Node;
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_PIE);

        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);

        GuiRenderObject& gro = gui_world->m_GuiRenderObjects[ro_count];
        dmRender::RenderObject& ro = gro.m_RenderObject;
        gro.m_SortOrder = gui_context->m_NextSortOrder++;

        // NOTE: ro might be uninitialized and we don't want to create a stack allocated temporary
        // See case 2264
        ro.Init();

        ApplyStencilClipping(gui_context, stencil_scopes[0], ro);

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLE_STRIP;
        ro.m_VertexStart = gui_world->m_ClientVertexBuffer.Size();
        ro.m_VertexCount = 0;
        ro.m_Material = (dmRender::HMaterial) dmGui::GetMaterial(scene);

        // Set default texture
        void* texture = dmGui::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = (dmGraphics::HTexture) texture;
        else
            ro.m_Textures[0] = gui_world->m_WhiteTexture;

        uint32_t max_total_vertices = 0;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            max_total_vertices += ComputeRequiredVertices(dmGui::GetNodePerimeterVertices(scene, entries[i].m_Node));
        }

        if (gui_world->m_ClientVertexBuffer.Remaining() < max_total_vertices) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, max_total_vertices));
        }

        for (uint32_t i = 0; i < node_count; ++i)
        {
            const dmGui::HNode node = entries[i].m_Node;
            const Point3 size = dmGui::GetNodeSize(scene, node);

            if (dmGui::GetNodeIsBone(scene, node) || dmMath::Abs(size.getX()) < 0.001f)
                continue;

            const Vector4& color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);

            // Pre-multiplied alpha
            Vector4 pm_color(color.getXYZ(), node_opacities[i]);
            uint32_t bcolor = dmGraphics::PackRGBA(pm_color);

            const uint32_t perimeterVertices = dmMath::Max<uint32_t>(4, dmGui::GetNodePerimeterVertices(scene, node));
            const float innerMultiplier = dmGui::GetNodeInnerRadius(scene, node) / size.getX();
            const dmGui::PieBounds outerBounds = dmGui::GetNodeOuterBounds(scene, node);

            const float PI = 3.1415926535f;
            const float ad = PI * 2.0f / (float)perimeterVertices;

            float stopAngle = dmGui::GetNodePieFillAngle(scene, node);
            bool backwards = false;
            if (stopAngle < 0)
            {
                stopAngle = -stopAngle;
                backwards = true;
            }

            stopAngle = dmMath::Min(360.0f, stopAngle) * PI / 180.0f;

            // 1. Division computes number of cirlce segments needed, and we need 1 more
            // vertex than that (1 lone segment = 2 perimeter vertices).
            // 2. Round up because 48 deg fill drawn with 45 deg segmenst should be be rendered
            // as 45+3. (Set limit to if segment exceeds more than 1/1000 to allow for some
            // floating point imprecision)
            const uint32_t generate = floorf(stopAngle / ad + 0.999f) + 1;

            float lastAngle = 0;
            float nextCorner = 0.25f * PI; // upper right rectangle corner at 45 deg
            bool first = true;

            float u0,su,v0,sv;
            bool uv_rotated;
            const float* tc = dmGui::GetNodeFlipbookAnimUV(scene, node);
            if(tc)
            {
                bool flip_u, flip_v;
                GetNodeFlipbookAnimUVFlip(scene, node, flip_u, flip_v);
                uv_rotated = tc[0] != tc[2] && tc[3] != tc[5];
                if(uv_rotated ? flip_v : flip_u)
                {
                    su = -(tc[4] - tc[0]);
                    u0 = tc[0] - su;
                }
                else
                {
                    u0 = tc[0];
                    su = tc[4] - u0;
                }
                uint32_t v0i = uv_rotated ? 1 : 3;
                uint32_t v1i = uv_rotated ? 5 : 1;
                if(uv_rotated ? flip_u : flip_v)
                {
                    sv = -(tc[v1i] - tc[v0i]);
                    v0 = tc[v0i] - sv;
                }
                else
                {
                    v0 = tc[v0i];
                    sv = tc[v1i] - v0;
                }
            }
            else
            {
                uv_rotated = false;
                u0 = 0.0f;
                su = 1.0f;
                v0 = 1.0f;
                sv = -1.0f;
            }

            uint32_t sizeBefore = gui_world->m_ClientVertexBuffer.Size();
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
                BoxVertex vInner(node_transforms[i] * Vectormath::Aos::Point3(u,v,0), u0 + ((uv_rotated ? v : u) * su), v0 + ((uv_rotated ? u : 1-v) * sv), bcolor);

                // make outer vertex
                float d;
                if (outerBounds == dmGui::PIEBOUNDS_RECTANGLE)
                    d = 0.5f / dmMath::Max(dmMath::Abs(s), dmMath::Abs(c));
                else
                    d = 0.5f;

                u = 0.5f + d * c;
                v = 0.5f + d * s;
                BoxVertex vOuter(node_transforms[i] * Vectormath::Aos::Point3(u,v,0), u0 + ((uv_rotated ? v : u) * su), v0 + ((uv_rotated ? u : 1-v) * sv), bcolor);

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

            assert((gui_world->m_ClientVertexBuffer.Size() - sizeBefore) <= ComputeRequiredVertices(dmGui::GetNodePerimeterVertices(scene, entries[i].m_Node)));
        }

        ro.m_VertexCount = gui_world->m_ClientVertexBuffer.Size() - ro.m_VertexStart;
    }

    void RenderNodes(dmGui::HScene scene,
                    const dmGui::RenderEntry* entries,
                    const Matrix4* node_transforms,
                    const float* node_opacities,
                    const dmGui::StencilScope** stencil_scopes,
                    uint32_t node_count,
                    void* context)
    {
        if (node_count == 0)
            return;

        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        gui_world->m_RenderedParticlesSize = 0;
        gui_context->m_FirstStencil = true;

        dmGui::HNode first_node = entries[0].m_Node;
        dmGui::BlendMode prev_blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        dmGui::NodeType prev_node_type = dmGui::GetNodeType(scene, first_node);
        void* prev_texture = dmGui::GetNodeTexture(scene, first_node);
        void* prev_font = dmGui::GetNodeFont(scene, first_node);
        const dmGui::StencilScope* prev_stencil_scope = stencil_scopes[0];
        uint32_t prev_emitter_batch_key = 0;

        if (prev_node_type == dmGui::NODE_TYPE_PARTICLEFX)
        {
            dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*)entries[0].m_RenderData;
            prev_emitter_batch_key = emitter_render_data->m_MixedHashNoMaterial;
        }

        uint32_t i = 0;
        uint32_t start = 0;

        while (i < node_count) {
            dmGui::HNode node = entries[i].m_Node;
            if (dmGui::GetNodeIsBone(scene, node)) {
                ++i;
                continue;
            }

            dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, node);
            dmGui::NodeType node_type = dmGui::GetNodeType(scene, node);
            void* texture = dmGui::GetNodeTexture(scene, node);
            void* font = dmGui::GetNodeFont(scene, node);
            const dmGui::StencilScope* stencil_scope = stencil_scopes[i];
            uint32_t emitter_batch_key = 0;

            if (node_type == dmGui::NODE_TYPE_PARTICLEFX)
            {
                dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*)entries[i].m_RenderData;
                emitter_batch_key = emitter_render_data->m_MixedHashNoMaterial;
            }

            bool batch_change = (node_type != prev_node_type || blend_mode != prev_blend_mode || texture != prev_texture || font != prev_font || prev_stencil_scope != stencil_scope
                || prev_emitter_batch_key != emitter_batch_key);
            bool flush = (i > 0 && batch_change);

            if (flush) {
                uint32_t n = i - start;

                switch (prev_node_type)
                {
                    case dmGui::NODE_TYPE_TEXT:
                        RenderTextNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                        break;
                    case dmGui::NODE_TYPE_BOX:
                        RenderBoxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                        break;
                    case dmGui::NODE_TYPE_PIE:
                        RenderPieNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                        break;
                    case dmGui::NODE_TYPE_SPINE:
                        RenderSpineNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                        break;
                    case dmGui::NODE_TYPE_PARTICLEFX:
                    	RenderParticlefxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                    	break;
                    default:
                        break;
                }

                start = i;
            }
            prev_node_type = node_type;
            prev_blend_mode = blend_mode;
            prev_texture = texture;
            prev_font = font;
            prev_stencil_scope = stencil_scope;
            prev_emitter_batch_key = emitter_batch_key;

            ++i;
        }

        uint32_t n = i - start;
        if (n > 0) {
            switch (prev_node_type)
            {
                case dmGui::NODE_TYPE_TEXT:
                    RenderTextNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                    break;
                case dmGui::NODE_TYPE_BOX:
                    RenderBoxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                    break;
                case dmGui::NODE_TYPE_PIE:
                    RenderPieNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                    break;
                case dmGui::NODE_TYPE_SPINE:
                    RenderSpineNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                    break;
                case dmGui::NODE_TYPE_PARTICLEFX:
                    RenderParticlefxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, context);
                    break;
                default:
                    break;
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

    dmGui::FetchTextureSetAnimResult FetchTextureSetAnimCallback(void* texture_set_ptr, dmhash_t animation, dmGui::TextureSetAnimDesc* out_data)
    {
        TextureSetResource* texture_set_res = (TextureSetResource*)texture_set_ptr;
        dmGameSystemDDF::TextureSet* texture_set = texture_set_res->m_TextureSet;
        uint32_t* anim_index = texture_set_res->m_AnimationIds.Get(animation);
        if (anim_index)
        {
            if (texture_set_res->m_TextureSet->m_TexCoords.m_Count == 0)
            {
                return dmGui::FETCH_ANIMATION_UNKNOWN_ERROR;
            }
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_Animations[*anim_index];
            uint32_t playback_index = animation->m_Playback;
            if(playback_index >= dmGui::PLAYBACK_COUNT)
                return dmGui::FETCH_ANIMATION_INVALID_PLAYBACK;

            out_data->m_TexCoords = (const float*) texture_set_res->m_TextureSet->m_TexCoords.m_Data;
            out_data->m_Start = animation->m_Start;
            out_data->m_End = animation->m_End;
            out_data->m_TextureWidth = dmGraphics::GetTextureWidth(texture_set_res->m_Texture);
            out_data->m_TextureHeight = dmGraphics::GetTextureHeight(texture_set_res->m_Texture);
            out_data->m_FPS = animation->m_Fps;
            out_data->m_FlipHorizontal = animation->m_FlipHorizontal;
            out_data->m_FlipVertical = animation->m_FlipVertical;
            out_data->m_Playback = ddf_playback_map.m_Table[playback_index];
            return dmGui::FETCH_ANIMATION_OK;
        }
        else
        {
            return dmGui::FETCH_ANIMATION_NOT_FOUND;
        }
    }

    bool FetchRigSceneDataCallback(void* spine_scene, dmhash_t rig_scene_id, dmGui::RigSceneDataDesc* out_data)
    {
        RigSceneResource* rig_res = (RigSceneResource*)spine_scene;
        out_data->m_BindPose = &rig_res->m_BindPose;
        out_data->m_Skeleton = rig_res->m_SkeletonRes->m_Skeleton;
        out_data->m_MeshSet = rig_res->m_MeshSetRes->m_MeshSet;
        out_data->m_AnimationSet = rig_res->m_AnimationSetRes->m_AnimationSet;
        out_data->m_Texture = rig_res->m_TextureSet->m_Texture;
        out_data->m_TextureSet = rig_res->m_TextureSet->m_TextureSet;
        out_data->m_PoseIdxToInfluence = &rig_res->m_PoseIdxToInfluence;
        out_data->m_TrackIdxToPose     = &rig_res->m_TrackIdxToPose;

        return true;
    }

    dmGameObject::CreateResult CompGuiAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        gui_component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    dmGameObject::UpdateResult CompGuiUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;

        dmRig::Update(gui_world->m_RigContext, params.m_UpdateContext->m_DT);

        gui_world->m_DT = params.m_UpdateContext->m_DT;
        dmParticle::Update(gui_world->m_ParticleContext, params.m_UpdateContext->m_DT, &FetchAnimationCallback);
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* gui_component = gui_world->m_Components[i];
            if (gui_component->m_Enabled && gui_component->m_AddedToUpdate)
            {
                dmGui::UpdateScene(gui_component->m_Scene, params.m_UpdateContext->m_DT);
            }
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static void RenderListDispatch(dmRender::RenderListDispatchParams const &params)
    {
        if (params.m_Operation == dmRender::RENDER_LIST_OPERATION_BATCH)
        {
            for (uint32_t *i=params.m_Begin;i!=params.m_End;i++)
            {
                dmRender::RenderObject *ro = (dmRender::RenderObject*) params.m_Buf[*i].m_UserData;
                dmRender::AddToRender(params.m_Context, ro);
            }
        }
    }

    dmGameObject::UpdateResult CompGuiRender(const dmGameObject::ComponentsRenderParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiContext* gui_context = (GuiContext*)params.m_Context;

        dmGui::RenderSceneParams rp;
        rp.m_RenderNodes = &RenderNodes;
        rp.m_NewTexture = &NewTexture;
        rp.m_DeleteTexture = &DeleteTexture;
        rp.m_SetTextureData = &SetTextureData;

        RenderGuiContext render_gui_context;
        render_gui_context.m_RenderContext = gui_context->m_RenderContext;
        render_gui_context.m_GuiWorld = gui_world;
        render_gui_context.m_NextSortOrder = 0;

        uint32_t total_node_count = 0;
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* c = gui_world->m_Components[i];
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;
            total_node_count += dmGui::GetNodeCount(c->m_Scene);
            total_node_count += dmGui::GetParticlefxCount(c->m_Scene);
        }

        uint32_t total_gui_render_objects_count = (total_node_count<<1) + (total_node_count>>3);
        if (gui_world->m_GuiRenderObjects.Capacity() < total_gui_render_objects_count) {
            gui_world->m_GuiRenderObjects.SetCapacity(total_gui_render_objects_count);
        }

        gui_world->m_GuiRenderObjects.SetSize(0);
        gui_world->m_ClientVertexBuffer.SetSize(0);

        uint32_t lastEnd = 0;

        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            GuiComponent* c = gui_world->m_Components[i];
            if (!c->m_Enabled || !c->m_AddedToUpdate)
                continue;

            // Render scene and see how many render objects it added, then we add those individually.
            dmGui::RenderScene(c->m_Scene, rp, &render_gui_context);
            const uint32_t count = gui_world->m_GuiRenderObjects.Size() - lastEnd;

            dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(gui_context->m_RenderContext, count);
            dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(gui_context->m_RenderContext, &RenderListDispatch, gui_world);
            dmRender::RenderListEntry* write_ptr = render_list;

            uint32_t render_order = dmGui::GetRenderOrder(c->m_Scene);
            while (lastEnd < gui_world->m_GuiRenderObjects.Size())
            {
                const GuiRenderObject& gro = gui_world->m_GuiRenderObjects[lastEnd];
                write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_AFTER_WORLD;
                write_ptr->m_Order = MakeFinalRenderOrder(render_order, gro.m_SortOrder);
                write_ptr->m_UserData = (uintptr_t) &gro.m_RenderObject;
                write_ptr->m_BatchKey = lastEnd;
                write_ptr->m_TagMask = dmRender::GetMaterialTagMask(gro.m_RenderObject.m_Material);
                write_ptr->m_Dispatch = dispatch;
                write_ptr++;
                lastEnd++;
            }

            dmRender::RenderListSubmit(gui_context->m_RenderContext, render_list, write_ptr);
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
            gui_input_action.m_GamepadIndex = params.m_InputAction->m_GamepadIndex;
            gui_input_action.m_IsGamepad = params.m_InputAction->m_IsGamepad;

            gui_input_action.m_TouchCount = params.m_InputAction->m_TouchCount;
            int tc = params.m_InputAction->m_TouchCount;
            for (int i = 0; i < tc; ++i) {
                gui_input_action.m_Touch[i] = params.m_InputAction->m_Touch[i];
            }

            size_t text_count = dmStrlCpy(gui_input_action.m_Text, params.m_InputAction->m_Text, sizeof(gui_input_action.m_Text));
            gui_input_action.m_TextCount = text_count;
            gui_input_action.m_HasText = params.m_InputAction->m_HasText;

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
        dmGui::ClearLayouts(gui_component->m_Scene);
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

    void GuiGetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics)
    {
        dmRender::TextMetrics metrics;
        dmRender::GetTextMetrics((dmRender::HFontMap)font, text, width, line_break, leading, tracking, &metrics);
        out_metrics->m_Width = metrics.m_Width;
        out_metrics->m_Height = metrics.m_Height;
        out_metrics->m_MaxAscent = metrics.m_MaxAscent;
        out_metrics->m_MaxDescent = metrics.m_MaxDescent;
    }
}
