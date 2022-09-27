// Copyright 2020-2022 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
// 
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
// 
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <string.h>

#include <dlib/dlib.h>
#include <dlib/array.h>
#include <dlib/buffer.h>
#include <dlib/hash.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/message.h>
#include <dlib/profile.h>
#include <dlib/dstrings.h>
#include <dlib/trig_lookup.h>
#include <dmsdk/dlib/vmath.h>
#include <graphics/graphics.h>
#include <render/render.h>
#include <render/display_profiles.h>
#include <render/font_renderer.h>
#include <gameobject/component.h>
#include <gameobject/gameobject_ddf.h> // dmGameObjectDDF enable/disable

#include "comp_gui.h"
#include "comp_gui_private.h"
#include "comp_private.h"

#include "../resources/res_gui.h"
#include "../resources/res_skeleton.h"
#include "../resources/res_meshset.h"
#include "../resources/res_animationset.h"
#include "../gamesys.h"
#include "../gamesys_private.h"
#include <particle/particle.h>
#include <gui/gui_script.h>

#include <dmsdk/gamesys/gui.h>

DM_PROPERTY_EXTERN(rmtp_Gui);
DM_PROPERTY_U32(rmtp_GuiVertexCount, 0, FrameReset, "#", &rmtp_Gui);

namespace dmGameSystem
{
    using namespace dmVMath;

    static CompGuiNodeTypeDescriptor g_CompGuiNodeTypeSentinel = {0};
    static bool g_CompGuiNodeTypesInitialized = false;

    static dmGui::FetchTextureSetAnimResult FetchTextureSetAnimCallback(void*, dmhash_t, dmGui::TextureSetAnimDesc*);

    // implemention in comp_particlefx.cpp
    extern dmParticle::FetchAnimationResult FetchAnimationCallback(void* texture_set_ptr, dmhash_t animation, dmParticle::AnimationData* out_data);

    static dmGameObject::Result CreateRegisteredCompGuiNodeTypes(const CompGuiNodeTypeCtx* ctx, struct CompGuiContext* comp_gui_context);
    static dmGameObject::Result DestroyRegisteredCompGuiNodeTypes(const CompGuiNodeTypeCtx* ctx, struct CompGuiContext* comp_gui_context);
    static void* CreateCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type);
    static void* CloneCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type, void* node_data);
    static void DestroyCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type, void* node_data);
    static void UpdateCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type, void* node_data, float dt);
    static const CompGuiNodeType* GetCompGuiCustomType(const CompGuiContext* gui_context, uint32_t custom_type);

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
        dmGui::BlendMode m_Table[5];
        BlendModeParticleToGui()
        {
            m_Table[dmParticleDDF::BLEND_MODE_ALPHA]        = dmGui::BLEND_MODE_ALPHA;
            m_Table[dmParticleDDF::BLEND_MODE_MULT]         = dmGui::BLEND_MODE_MULT;
            m_Table[dmParticleDDF::BLEND_MODE_ADD]          = dmGui::BLEND_MODE_ADD;
            m_Table[dmParticleDDF::BLEND_MODE_ADD_ALPHA]    = dmGui::BLEND_MODE_ADD_ALPHA;
            m_Table[dmParticleDDF::BLEND_MODE_SCREEN]       = dmGui::BLEND_MODE_SCREEN;
        }
    } ddf_blendmode_map;

    struct CompGuiContext
    {
        dmArray<GuiWorld*>              m_Worlds;
        dmHashTable32<CompGuiNodeType*> m_CustomNodeTypes;

        dmResource::HFactory        m_Factory;
        dmRender::HRenderContext    m_RenderContext;
        dmGui::HContext             m_GuiContext;
        dmScript::HContext          m_ScriptContext;

        uint32_t                    m_MaxGuiComponents;
        uint32_t                    m_MaxParticleFXCount;
        uint32_t                    m_MaxParticleCount;
    };

    static void GuiResourceReloadedCallback(const dmResource::ResourceReloadedParams& params)
    {
        GuiWorld* world = (GuiWorld*) params.m_UserData;
        void* resource = params.m_Resource->m_Resource;

        for (uint32_t j = 0; j < world->m_Components.Size(); ++j)
        {
            GuiComponent* component = world->m_Components[j];
            if (resource == (void*)dmGui::GetSceneScript(component->m_Scene))
            {
                dmGui::ReloadScene(component->m_Scene);
            }
        }
    }

    static dmGameObject::CreateResult CompGuiNewWorld(const dmGameObject::ComponentNewWorldParams& params)
    {
        CompGuiContext* gui_context = (CompGuiContext*)params.m_Context;
        GuiWorld* gui_world = new GuiWorld();
        if (!gui_context->m_Worlds.Full())
        {
            gui_context->m_Worlds.Push(gui_world);
        }
        else
        {
            dmLogWarning("The gui world could not be stored since the buffer is full (%d). Increase number in gui.max_instance_count", gui_context->m_Worlds.Size());
        }

        gui_world->m_CompGuiContext = gui_context;

        uint32_t comp_count = dmMath::Min(params.m_MaxComponentInstances, gui_context->m_MaxGuiComponents);
        gui_world->m_Components.SetCapacity(comp_count);

        dmGraphics::VertexElement ve[] =
        {
                {"position", 0, 3, dmGraphics::TYPE_FLOAT, false},
                {"texcoord0", 1, 2, dmGraphics::TYPE_FLOAT, false},
                {"color", 2, 4, dmGraphics::TYPE_FLOAT, true},
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
        gui_world->m_RenderConstants.SetCapacity(128);
        gui_world->m_RenderConstants.SetSize(128);
        memset(gui_world->m_RenderConstants.Begin(), 0, gui_world->m_RenderConstants.Capacity() * sizeof(HComponentRenderConstants));

        gui_world->m_MaxParticleFXCount = gui_context->m_MaxParticleFXCount;
        gui_world->m_MaxParticleCount = gui_context->m_MaxParticleCount;
        gui_world->m_ParticleContext = dmParticle::CreateContext(gui_world->m_MaxParticleFXCount, gui_world->m_MaxParticleCount);

        gui_world->m_ScriptWorld = dmScript::NewScriptWorld(gui_context->m_ScriptContext);

        if (dLib::IsDebugMode())
        {
            dmResource::RegisterResourceReloadedCallback(gui_context->m_Factory, GuiResourceReloadedCallback, gui_world);
        }

        *params.m_World = gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompGuiDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        CompGuiContext* gui_context = (CompGuiContext*)params.m_Context;

        if (dLib::IsDebugMode())
        {
            dmResource::UnregisterResourceReloadedCallback(gui_context->m_Factory, GuiResourceReloadedCallback, gui_world);
        }

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

        for (uint32_t i = 0; i < gui_world->m_RenderConstants.Size(); ++i)
        {
            if (gui_world->m_RenderConstants[i])
            {
                dmGameSystem::DestroyRenderConstants(gui_world->m_RenderConstants[i]);
            }
        }

        dmGraphics::DeleteVertexDeclaration(gui_world->m_VertexDeclaration);
        dmGraphics::DeleteVertexBuffer(gui_world->m_VertexBuffer);
        dmGraphics::DeleteTexture(gui_world->m_WhiteTexture);

        dmScript::DeleteScriptWorld(gui_world->m_ScriptWorld);

        delete gui_world;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static bool SetNode(const dmGui::HScene scene, dmGui::HNode n, const dmGuiDDF::NodeDesc* node_desc)
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
                    gui_result = dmGui::PlayNodeFlipbookAnim(scene, n, texture_anim_name, 0.0f, 1.0f);
                    if (gui_result != dmGui::RESULT_OK)
                    {
                        dmLogError("The texture animation '%s' in texture '%s' could not be set for '%s', result: %d.", texture_anim_name, texture_str, node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", gui_result);
                        result = false;
                    }
                    // Fix for https://github.com/defold/defold/issues/6384
                    // If the animation is a single frame there's no point in actually playing the
                    // animation. Instead we can immediately cancel the animation and the node will
                    // still have the correct image.
                    // By doing this we'll not take up an animation slot (there's a max animation cap).
                    // Even though we cancel animation we still need flipbook hash:
                    // https://github.com/defold/defold/issues/6551
                    // We can't move this logic into dmGui::PlayNodeFlipbookAnim()
                    // because if we do so we will never play all the one-frame animations which is a breaking
                    // change. In this case, we cancel one-frame animation ONLY when we initially setup node.
                    if (dmGui::GetNodeAnimationFrameCount(scene, n) == 1)
                    {
                        dmGui::CancelNodeFlipbookAnim(scene, n, true);
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

        // TODO: Left a reminder that we have some bone related tasks left to fix
        if (node_desc->m_SpineNodeChild) {
            dmGui::SetNodeIsBone(scene, n, true);
        }

        dmGui::SetNodeEnabled(scene, n, node_desc->m_Enabled);
        dmGui::SetNodeVisible(scene, n, node_desc->m_Visible);

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

            case dmGuiDDF::NodeDesc::TYPE_PARTICLEFX:
                dmGui::SetNodeParticlefx(scene, n, dmHashString64(node_desc->m_Particlefx));
            break;

            case dmGuiDDF::NodeDesc::TYPE_TEMPLATE:
                dmLogError("Template nodes are not supported in run-time '%s', result: %d.", node_desc->m_Id != 0x0 ? node_desc->m_Id : "unnamed", dmGui::RESULT_INVAL_ERROR);
                result = false;
            break;


            case dmGui::NODE_TYPE_CUSTOM:
            {
                GuiComponent* component = (GuiComponent*)dmGui::GetSceneUserData(scene);
                uint32_t custom_type = dmGui::GetNodeCustomType(scene, n);
                void* custom_node_data = dmGui::GetNodeCustomData(scene, n);
                const CompGuiNodeType* node_type = GetCompGuiCustomType(component->m_World->m_CompGuiContext, custom_type);

                if (node_type->m_SetNodeDesc)
                {
                    CompGuiNodeContext ctx;

                    CustomNodeCtx nodectx;
                    nodectx.m_Scene = scene;
                    nodectx.m_Node = n;
                    nodectx.m_TypeContext = node_type->m_Context;
                    nodectx.m_NodeData = custom_node_data;
                    nodectx.m_Type = custom_type;

                    node_type->m_SetNodeDesc(&ctx, &nodectx, node_desc);
                }
            }
            break;

            default:
            break;
        }

        dmGui::SetNodeResetPoint(scene, n);
        return result;
    }

    static void SetNodeCallback(const dmGui::HScene scene, dmGui::HNode n, const void* node_desc)
    {
        SetNode(scene, n, (const dmGuiDDF::NodeDesc*) node_desc);
    }

    static void OnWindowResizeCallback(const dmGui::HScene scene, uint32_t width, uint32_t height)
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
        dmhash_t current_layout_id = GetLayout(scene);
        dmhash_t layout_id = dmRender::GetOptimalDisplayProfile(display_profiles, width, height, dmGui::GetDisplayDpi(scene), &scene_layouts);
        if(layout_id != current_layout_id)
        {
            dmRender::DisplayProfileDesc profile_desc;
            GetDisplayProfileDesc(display_profiles, layout_id, profile_desc);
            dmGui::SetSceneResolution(scene, profile_desc.m_Width, profile_desc.m_Height);
            dmGui::SetLayout(scene, layout_id, SetNodeCallback);

            // Notify the scene script. The callback originates from the dmGraphics::SetWindowSize firing the callback.
            char buf[sizeof(dmMessage::Message) + sizeof(dmGuiDDF::LayoutChanged)];
            dmMessage::Message* message = (dmMessage::Message*)buf;
            memset(message, 0, sizeof(dmMessage::Message));
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

    static void* GetSceneResourceByHash(void* ctx, dmGui::HScene scene, dmhash_t name_hash, dmhash_t suffix_with_dot)
    {
        (void)scene;
        GuiComponent* gui_component = (GuiComponent*)ctx;
        GuiSceneResource* resource = gui_component->m_Resource;

        dmhash_t* suffix = resource->m_ResourceTypes.Get(name_hash);
        if (!suffix)
        {
            dmLogError("Failed to find resource %s with suffix %s", dmHashReverseSafe64(name_hash), dmHashReverseSafe64(suffix_with_dot));
            return 0;
        }
        if (*suffix != suffix_with_dot)
        {
            dmLogError("The resource %s was of type %s, but you requested type %s", dmHashReverseSafe64(name_hash), dmHashReverseSafe64(*suffix), dmHashReverseSafe64(suffix_with_dot));
            return 0;
        }

        void** outresource = resource->m_Resources.Get(name_hash);
        if (outresource)
            return *outresource;

        dmLogError("Failed to find resource matching name: %s", dmHashReverseSafe64(name_hash));
        return 0;
    }

    static bool SetupGuiScene(GuiWorld* gui_world, GuiComponent* gui_component, dmGui::HScene scene, GuiSceneResource* scene_resource)
    {
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;
        dmGui::SetSceneScript(scene, scene_resource->m_Script);

        bool result = true;

        dmGui::SetSceneAdjustReference(scene, (dmGui::AdjustReference)scene_desc->m_AdjustReference);

        for (uint32_t i = 0; i < scene_resource->m_FontMaps.Size(); ++i)
        {
            const char* name = scene_desc->m_Fonts[i].m_Name;
            dmGui::Result r = dmGui::AddFont(scene, dmHashString64(name), (void*) scene_resource->m_FontMaps[i], scene_resource->m_FontMapPaths[i]);
            if (r != dmGui::RESULT_OK) {
                dmLogError("Unable to add font '%s' to scene (%d)", name,  r);
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

            void* texture_source;
            dmGraphics::HTexture texture = scene_resource->m_GuiTextureSets[i].m_Texture;
            dmGui::NodeTextureType texture_source_type;

            if (scene_resource->m_GuiTextureSets[i].m_TextureSet)
            {
                texture_source_type = dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET;
                texture_source      = (void*)scene_resource->m_GuiTextureSets[i].m_TextureSet;
            }
            else
            {
                texture_source_type = dmGui::NODE_TEXTURE_TYPE_TEXTURE;
                texture_source      = (void*)texture;
            }

            dmGui::Result r = dmGui::AddTexture(scene, dmHashString64(name), texture_source, texture_source_type, dmGraphics::GetOriginalTextureWidth(texture), dmGraphics::GetOriginalTextureHeight(texture));
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
            uint32_t custom_type = node_desc->m_CustomType;


            Vector4 position = node_desc->m_Position;
            Vector4 size = node_desc->m_Size;
            dmGui::HNode n = dmGui::NewNode(scene, Point3(position.getXYZ()), Vector3(size.getXYZ()), type, custom_type);
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
                dmGui::SetNodeParent(scene, n, p, false);
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

    static dmGameObject::CreateResult CompGuiCreate(const dmGameObject::ComponentCreateParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;

        GuiSceneResource* scene_resource = (GuiSceneResource*) params.m_Resource;
        dmGuiDDF::SceneDesc* scene_desc = scene_resource->m_SceneDesc;

        GuiComponent* gui_component = new GuiComponent();
        gui_component->m_World = gui_world;
        gui_component->m_Resource = scene_resource;
        gui_component->m_Instance = params.m_Instance;
        gui_component->m_Material = 0;
        gui_component->m_ComponentIndex = params.m_ComponentIndex;
        gui_component->m_Enabled = 1;
        gui_component->m_AddedToUpdate = 0;

        dmGui::NewSceneParams scene_params;
        // This is a hard cap since the render key has 13 bits for node index (see gui.cpp)
        assert(scene_desc->m_MaxNodes <= 8192);
        scene_params.m_MaxNodes = scene_desc->m_MaxNodes;
        scene_params.m_MaxAnimations = 1024;
        scene_params.m_UserData = gui_component;
        scene_params.m_MaxFonts = 64;
        scene_params.m_MaxTextures = 128;
        scene_params.m_MaxParticlefx = gui_world->m_MaxParticleFXCount;
        scene_params.m_ParticlefxContext = gui_world->m_ParticleContext;
        scene_params.m_FetchTextureSetAnimCallback = &FetchTextureSetAnimCallback;
        scene_params.m_CreateCustomNodeCallback = &CreateCustomNodeCallback;
        scene_params.m_DestroyCustomNodeCallback = &DestroyCustomNodeCallback;
        scene_params.m_CloneCustomNodeCallback = &CloneCustomNodeCallback;
        scene_params.m_UpdateCustomNodeCallback = &UpdateCustomNodeCallback;
        scene_params.m_CreateCustomNodeCallbackContext = gui_component;
        scene_params.m_GetResourceCallback = GetSceneResourceByHash;
        scene_params.m_GetResourceCallbackContext = gui_component;
        scene_params.m_OnWindowResizeCallback = &OnWindowResizeCallback;
        scene_params.m_ScriptWorld = gui_world->m_ScriptWorld;
        gui_component->m_Scene = dmGui::NewScene(scene_resource->m_GuiContext, &scene_params);
        dmGui::HScene scene = gui_component->m_Scene;

        if (!SetupGuiScene(gui_world, gui_component, scene, scene_resource))
        {
            dmGui::DeleteScene(gui_component->m_Scene);
            delete gui_component;
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }

        *params.m_UserData = (uintptr_t)gui_component;
        gui_world->m_Components.Push(gui_component);
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompGuiDestroy(const dmGameObject::ComponentDestroyParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        for (uint32_t i = 0; i < gui_world->m_Components.Size(); ++i)
        {
            if (gui_world->m_Components[i] == gui_component)
            {
                dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);
                if (gui_component->m_Material) {
                    dmResource::Release(factory, gui_component->m_Material);
                }
                for (uint32_t i = 0; i < gui_component->m_ResourcePropertyPointers.Size(); ++i) {
                    if (gui_component->m_ResourcePropertyPointers[i]) {
                        dmResource::Release(factory, gui_component->m_ResourcePropertyPointers[i]);
                    }
                }
                gui_component->m_ResourcePropertyPointers.SetSize(0);
                dmGui::DeleteScene(gui_component->m_Scene);
                delete gui_component;
                gui_world->m_Components.EraseSwap(i);
                break;
            }
        }
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::CreateResult CompGuiInit(const dmGameObject::ComponentInitParams& params)
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

    static dmGameObject::CreateResult CompGuiFinal(const dmGameObject::ComponentFinalParams& params)
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

    static void* CompGuiGetComponent(const dmGameObject::ComponentGetParams& params)
    {
        return (GuiComponent*)*params.m_UserData;
    }

    struct RenderGuiContext
    {
        dmRender::HRenderContext    m_RenderContext;
        dmRender::HMaterial         m_Material;
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

            case dmGui::BLEND_MODE_SCREEN:
                ro.m_SourceBlendFactor = dmGraphics::BLEND_FACTOR_ONE_MINUS_DST_COLOR;
                ro.m_DestinationBlendFactor = dmGraphics::BLEND_FACTOR_ONE;
            break;

            default:
                dmLogError("Unknown blend mode: %d\n", blend_mode);
                assert(0);
            break;
        }
    }

    static void ApplyStencilClipping(RenderGuiContext* gui_context, const dmGui::StencilScope* state, dmRender::StencilTestParams& stp) {
        if (state != 0x0) {
            stp.m_Front.m_Func = dmGraphics::COMPARE_FUNC_EQUAL;
            stp.m_Front.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
            stp.m_Front.m_OpDPFail = dmGraphics::STENCIL_OP_REPLACE;
            stp.m_Front.m_OpDPPass = dmGraphics::STENCIL_OP_REPLACE;
            stp.m_Ref = state->m_RefVal;
            stp.m_RefMask = state->m_TestMask;
            stp.m_BufferMask = state->m_WriteMask;
            stp.m_ColorBufferMask = state->m_ColorMask;
            stp.m_SeparateFaceStates = 0;
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
            stp.m_Front.m_Func = dmGraphics::COMPARE_FUNC_ALWAYS;
            stp.m_Front.m_OpSFail = dmGraphics::STENCIL_OP_KEEP;
            stp.m_Front.m_OpDPFail = dmGraphics::STENCIL_OP_KEEP;
            stp.m_Front.m_OpDPPass = dmGraphics::STENCIL_OP_KEEP;
            stp.m_Ref = 0;
            stp.m_RefMask = 0xff;
            stp.m_BufferMask = 0xff;
            stp.m_ColorBufferMask = 0xf;
            stp.m_SeparateFaceStates = 0;
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

    static dmGraphics::HTexture GetNodeTexture(dmGui::HScene scene, dmGui::HNode node)
    {
        dmGui::NodeTextureType texture_type;
        void* result = dmGui::GetNodeTexture(scene, node, &texture_type);

        if (texture_type == dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET)
        {
            TextureSetResource* texture_set_res = (TextureSetResource*) result;
            assert(texture_set_res);

            return texture_set_res->m_Texture;
        }

        return (dmGraphics::HTexture) result;
    }

    static void RenderTextNodes(dmGui::HScene scene,
                         const dmGui::RenderEntry* entries,
                         const Matrix4* node_transforms,
                         const float* node_opacities,
                         const dmGui::StencilScope** stencil_scopes,
                         uint32_t node_count,
                         RenderGuiContext* gui_context)
    {
        for (uint32_t i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = entries[i].m_Node;

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

    static void RenderParticlefxNodes(dmGui::HScene scene,
                          const dmGui::RenderEntry* entries,
                          const Matrix4* node_transforms,
                          const float* node_opacities,
                          const dmGui::StencilScope** stencil_scopes,
                          uint32_t node_count,
                          RenderGuiContext* gui_context)
    {
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
        ro.m_Material = gui_context->m_Material;
        ro.m_Textures[0] = (dmGraphics::HTexture)first_emitter_render_data->m_Texture;

        // Offset capacity to fit vertices for all emitters we are about to render
        uint32_t vertex_count = 0;
        for (uint32_t i = 0; i < node_count; ++i)
        {
            dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*)entries[i].m_RenderData;
            vertex_count += dmParticle::GetEmitterVertexCount(gui_world->m_ParticleContext, emitter_render_data->m_Instance, emitter_render_data->m_EmitterIndex);

            dmTransform::Transform transform = dmTransform::ToTransform(node_transforms[i]);
            // Particlefx nodes have uniformly scaled x/y values from adjust mode, we use x here but y would be fine too.
            float scale = transform.GetScalePtr()[0];
            dmParticle::SetPosition(gui_world->m_ParticleContext, emitter_render_data->m_Instance, Point3(transform.GetTranslation()));
            dmParticle::SetRotation(gui_world->m_ParticleContext, emitter_render_data->m_Instance, transform.GetRotation());
            dmParticle::SetScale(gui_world->m_ParticleContext, emitter_render_data->m_Instance, scale);
        }

        vertex_count = dmMath::Min(vertex_count, vb_max_size / (uint32_t)sizeof(ParticleGuiVertex));

        if (gui_world->m_ClientVertexBuffer.Remaining() < vertex_count) {
            gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, vertex_count));
        }

        ParticleGuiVertex *vb_begin = gui_world->m_ClientVertexBuffer.End();
        ParticleGuiVertex *vb_end = vb_begin;
        // One RO, but generate vertex data for each entry (emitter)
        for (uint32_t i = 0; i < node_count; ++i)
        {
            dmGui::HNode node = entries[i].m_Node;
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

        // If we need new render constants
        HComponentRenderConstants render_constants = gui_world->m_RenderConstants[ro_count];
        if (first_emitter_render_data->m_RenderConstantsSize > 0)
        {
            if (!render_constants)
            {
                render_constants = dmGameSystem::CreateRenderConstants();
                gui_world->m_RenderConstants[ro_count] = render_constants;
            }
        }

        for (uint32_t i = 0; i < first_emitter_render_data->m_RenderConstantsSize; ++i)
        {
            dmParticle::RenderConstant* c = &first_emitter_render_data->m_RenderConstants[i];
            dmGameSystem::SetRenderConstant(render_constants, c->m_NameHash, (dmVMath::Vector4*) &c->m_Value, c->m_IsMatrix4 ? 4 : 1);
        }

        if (render_constants) {
            dmGameSystem::EnableRenderObjectConstants(&ro, render_constants);
        }

        ApplyStencilClipping(gui_context, stencil_scopes[0], ro);
        gui_world->m_ClientVertexBuffer.SetSize(vb_end - gui_world->m_ClientVertexBuffer.Begin());
    }

    static GuiRenderObject* GetRenderObject(RenderGuiContext* gui_context)
    {
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        uint32_t ro_count = gui_world->m_GuiRenderObjects.Size();
        gui_world->m_GuiRenderObjects.SetSize(ro_count + 1);

        return &gui_world->m_GuiRenderObjects[ro_count];
    }

    static void RenderCustomNodes(dmGui::HScene scene,
                                uint32_t custom_type,
                                const CompGuiNodeType* type,
                                const dmGui::RenderEntry* entries,
                                const Matrix4* node_transforms,
                                const float* node_opacities,
                                const dmGui::StencilScope** stencil_scopes,
                                uint32_t node_count,
                                RenderGuiContext* gui_context)
    {
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        dmGui::HNode first_node = entries[0].m_Node;
        dmGui::NodeType node_type = dmGui::GetNodeType(scene, first_node);
        assert(node_type == dmGui::NODE_TYPE_CUSTOM);
        assert(custom_type != 0);

        GuiRenderObject* gro = GetRenderObject(gui_context);
        gro->m_SortOrder = gui_context->m_NextSortOrder++;

        dmRender::RenderObject& ro = gro->m_RenderObject;

        uint32_t vertex_start = gui_world->m_ClientVertexBuffer.Size();
        uint32_t vertex_count = 0;

        // Another way would be to use the vertex declaration, but that currently doesn't have an api
        // and the buffer is well suited for this.
        dmBuffer::StreamDeclaration boxvertex_stream_decl[] = {
            {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
            {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 2},
            {dmHashString64("color"), dmBuffer::VALUE_TYPE_FLOAT32, 4}
        };

        uint32_t struct_size = 0;
        dmBuffer::CalcStructSize(DM_ARRAY_SIZE(boxvertex_stream_decl), boxvertex_stream_decl, &struct_size, 0);

        for (uint32_t i = 0; i < node_count; ++i)
        {
            const dmGui::HNode node = entries[i].m_Node;

            void* custom_node_data = dmGui::GetNodeCustomData(scene, node);

            CustomNodeCtx nodectx;
            nodectx.m_Scene = scene;
            nodectx.m_Node = node;
            nodectx.m_TypeContext = type->m_Context;
            nodectx.m_NodeData = custom_node_data;
            nodectx.m_Type = custom_type;

            // Ideally, dmBuffer would support dynamic arrays, but for now this is what we do
            dmArray<uint8_t> node_vertices;
            type->m_GetVertices(&nodectx, DM_ARRAY_SIZE(boxvertex_stream_decl), boxvertex_stream_decl, struct_size, node_vertices);

            uint32_t node_vertex_count = node_vertices.Size() / struct_size;
            vertex_count += node_vertex_count;

            // Transform the vertices and modify the colors
            const Matrix4& transform = node_transforms[i];
            float opacity = node_opacities[i];
            Vector4 color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
            color = Vector4(color.getXYZ(), opacity);

            BoxVertex* vertices = (BoxVertex*)node_vertices.Begin();
            for (uint32_t v = 0; v < node_vertex_count; ++v)
            {
                BoxVertex& vertex = vertices[v];
                Point3 p(vertex.m_Position[0], vertex.m_Position[1], vertex.m_Position[2]);
                Vector4 wp = transform * p;
                vertex.m_Position[0] = wp.getX();
                vertex.m_Position[1] = wp.getY();
                vertex.m_Position[2] = wp.getZ();

                vertex.m_Color[0] = color.getX() * vertex.m_Color[0];
                vertex.m_Color[1] = color.getY() * vertex.m_Color[1];
                vertex.m_Color[2] = color.getZ() * vertex.m_Color[2];
                vertex.m_Color[3] = color.getW() * vertex.m_Color[3];
            }

            if (gui_world->m_ClientVertexBuffer.Remaining() < node_vertex_count) {
                gui_world->m_ClientVertexBuffer.OffsetCapacity(dmMath::Max(128U, node_vertex_count));
            }

            uint32_t node_vertex_start = gui_world->m_ClientVertexBuffer.Size();
            gui_world->m_ClientVertexBuffer.SetSize(node_vertex_start + node_vertex_count);
            memcpy(gui_world->m_ClientVertexBuffer.Begin() + node_vertex_start, node_vertices.Begin(), node_vertex_count * sizeof(BoxVertex));
        }

        ro.Init();
        ro.m_VertexDeclaration = gui_world->m_VertexDeclaration;
        ro.m_VertexBuffer = gui_world->m_VertexBuffer;
        ro.m_PrimitiveType = dmGraphics::PRIMITIVE_TRIANGLES;
        ro.m_VertexStart = vertex_start;
        ro.m_VertexCount = vertex_count;
        ro.m_Material = gui_context->m_Material;

        dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        SetBlendMode(ro, blend_mode);
        ro.m_SetBlendFactors = 1;

        ApplyStencilClipping(gui_context, stencil_scopes[0], ro);

        // Set default texture
        dmGraphics::HTexture texture = dmGameSystem::GetNodeTexture(scene, first_node);
        if (texture) {
            ro.m_Textures[0] = texture;
        } else {
            ro.m_Textures[0] = gui_world->m_WhiteTexture;
        }
    }

    static void RenderBoxNodes(dmGui::HScene scene,
                        const dmGui::RenderEntry* entries,
                        const Matrix4* node_transforms,
                        const float* node_opacities,
                        const dmGui::StencilScope** stencil_scopes,
                        uint32_t node_count,
                        RenderGuiContext* gui_context)
    {
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
        ro.m_Material = gui_context->m_Material;

        // Set default texture
        dmGraphics::HTexture texture = dmGameSystem::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = texture;
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

            // pre-multiplied alpha
            const Vector4& color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);
            Vector4 pm_color(color.getXYZ(), node_opacities[i]);

            // default not uv_rotated texture coords
            const float default_tc[6] = {0, 0, 0, 1, 1, 1};
            const float* tc = dmGui::GetNodeFlipbookAnimUV(scene, node);

            // tc equals 0 when texture is set from lua script directly with gui.set_texture(...) method
            bool manually_set_texture = tc == 0;
            if (manually_set_texture) {
                tc = default_tc;
            }

            Vector4 slice9 = dmGui::GetNodeSlice9(scene, node);
            bool use_slice_nine = sum(slice9) != 0;

            // render simple quad ignoring 9-slicing
            if ((!use_slice_nine && manually_set_texture) || !texture)
            {
                BoxVertex v00;
                v00.SetColor(pm_color);
                v00.SetPosition(node_transforms[i] * Point3(0, 0, 0));
                v00.SetUV(0, 0);

                BoxVertex v10;
                v10.SetColor(pm_color);
                v10.SetPosition(node_transforms[i] * Point3(1, 0, 0));
                v10.SetUV(1, 0);

                BoxVertex v01;
                v01.SetColor(pm_color);
                v01.SetPosition(node_transforms[i] * Point3(0, 1, 0));
                v01.SetUV(0, 1);

                BoxVertex v11;
                v11.SetColor(pm_color);
                v11.SetPosition(node_transforms[i] * Point3(1, 1, 0));
                v11.SetUV(1, 1);

                gui_world->m_ClientVertexBuffer.Push(v00);
                gui_world->m_ClientVertexBuffer.Push(v10);
                gui_world->m_ClientVertexBuffer.Push(v11);
                gui_world->m_ClientVertexBuffer.Push(v00);
                gui_world->m_ClientVertexBuffer.Push(v11);
                gui_world->m_ClientVertexBuffer.Push(v01);

                rendered_vert_count += 6;
                continue;
            }

            dmGui::TextureSetAnimDesc* anim_desc = dmGui::GetNodeTextureSet(scene, node);
            dmGameSystemDDF::TextureSet* texture_set_ddf = anim_desc ? (dmGameSystemDDF::TextureSet*)anim_desc->m_TextureSet : 0;
            bool use_geometries = texture_set_ddf && texture_set_ddf->m_Geometries.m_Count > 0;

            bool flip_u = false;
            bool flip_v = false;
            if (!manually_set_texture)
                GetNodeFlipbookAnimUVFlip(scene, node, flip_u, flip_v);

            // render using geometries without 9-slicing
            if (!use_slice_nine && use_geometries)
            {
                int32_t frame_index = dmGui::GetNodeAnimationFrame(scene, node);
                frame_index = texture_set_ddf->m_FrameIndices[frame_index];

                const dmGameSystemDDF::SpriteGeometry* geometry = &texture_set_ddf->m_Geometries.m_Data[frame_index];

                const Matrix4& w = node_transforms[i];

                // NOTE: The original rendering code is from the comp_sprite.cpp.
                // Compare with that one if you do any changes to either.
                uint32_t num_points = geometry->m_Vertices.m_Count / 2;

                const float* points = geometry->m_Vertices.m_Data;
                const float* uvs = geometry->m_Uvs.m_Data;

                // Depending on the sprite is flipped or not, we loop the vertices forward or backward
                // to respect face winding (and backface culling)
                int reverse = (int)flip_u ^ (int)flip_v;

                float scaleX = flip_u ? -1 : 1;
                float scaleY = flip_v ? -1 : 1;

                // Since we don't use an index buffer, we duplicate the vertices manually
                uint32_t index_count = geometry->m_Indices.m_Count;
                for (uint32_t index = 0; index < index_count; ++index)
                {
                    uint32_t i = geometry->m_Indices.m_Data[index];
                    i = reverse ? (num_points - i - 1) : i;

                    const float* point = &points[i * 2];
                    const float* uv = &uvs[i * 2];
                    // COnvert from range [-0.5,+0.5] to [0.0, 1.0]
                    float x = point[0] * scaleX + 0.5f;
                    float y = point[1] * scaleY + 0.5f;

                    Vector4 p = w * Point3(x, y, 0.0f);
                    BoxVertex v(p, uv[0], uv[1], pm_color);
                    gui_world->m_ClientVertexBuffer.Push(v);
                }

                rendered_vert_count += index_count;
                continue;
            }

            // render 9-sliced node

            //   0 1     2 3
            // 0 *-*-----*-*
            //   | |  y  | |
            // 1 *-*-----*-*
            //   | |     | |
            //   |x|     |z|
            //   | |     | |
            // 2 *-*-----*-*
            //   | |  w  | |
            // 3 *-*-----*-*
            float us[4], vs[4], xs[4], ys[4];

            // v are '1-v'
            xs[0] = ys[0] = 0;
            xs[3] = ys[3] = 1;

            // disable slice9 computation below a certain dimension
            // (avoid div by zero)
            const float s9_min_dim = 0.001f;

            const float su = 1.0f / org_width;
            const float sv = 1.0f / org_height;

            Point3 size = dmGui::GetNodeSize(scene, node);
            const float sx = size.getX() > s9_min_dim ? 1.0f / size.getX() : 0;
            const float sy = size.getY() > s9_min_dim ? 1.0f / size.getY() : 0;

            static const uint32_t uvIndex[2][4] = {{0,1,2,3}, {3,2,1,0}};
            bool uv_rotated = tc[0] != tc[2] && tc[3] != tc[5];
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

            xs[1] = sx * slice9.getX();
            xs[2] = 1 - sx * slice9.getZ();
            ys[1] = sy * slice9.getW();
            ys[2] = 1 - sy * slice9.getY();

            const Matrix4* transform = &node_transforms[i];
            Vector4 pts[4][4];
            for (int y=0;y<4;y++)
            {
                for (int x=0;x<4;x++)
                {
                    pts[y][x] = (*transform * Point3(xs[x], ys[y], 0));
                }
            }

            BoxVertex v00, v10, v01, v11;
            v00.SetColor(pm_color);
            v10.SetColor(pm_color);
            v01.SetColor(pm_color);
            v11.SetColor(pm_color);
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
            rendered_vert_count += verts_per_node;
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

    static void RenderPieNodes(dmGui::HScene scene,
                        const dmGui::RenderEntry* entries,
                        const Matrix4* node_transforms,
                        const float* node_opacities,
                        const dmGui::StencilScope** stencil_scopes,
                        uint32_t node_count,
                        RenderGuiContext* gui_context)
    {
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
        ro.m_Material = gui_context->m_Material;

        // Set default texture
        dmGraphics::HTexture texture = dmGameSystem::GetNodeTexture(scene, first_node);
        if (texture)
            ro.m_Textures[0] = texture;
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

            if (dmMath::Abs(size.getX()) < 0.001f)
                continue;

            const Vector4& color = dmGui::GetNodeProperty(scene, node, dmGui::PROPERTY_COLOR);

            // Pre-multiplied alpha
            Vector4 pm_color(color.getXYZ(), node_opacities[i]);

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
            for (uint32_t j = 0; j != generate; j++)
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
                BoxVertex vInner(node_transforms[i] * Point3(u,v,0), u0 + ((uv_rotated ? v : u) * su), v0 + ((uv_rotated ? u : 1-v) * sv), pm_color);

                // make outer vertex
                float d;
                if (outerBounds == dmGui::PIEBOUNDS_RECTANGLE)
                    d = 0.5f / dmMath::Max(dmMath::Abs(s), dmMath::Abs(c));
                else
                    d = 0.5f;

                u = 0.5f + d * c;
                v = 0.5f + d * s;
                BoxVertex vOuter(node_transforms[i] * Point3(u,v,0), u0 + ((uv_rotated ? v : u) * su), v0 + ((uv_rotated ? u : 1-v) * sv), pm_color);

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

    static uint64_t GetCombinedNodeType(uint32_t node_type, uint32_t custom_type)
    {
        uint64_t combined_type = node_type;
        if (node_type == (uint32_t)dmGui::NODE_TYPE_CUSTOM)
        {
            combined_type |= ((uint64_t)node_type) << 32;
        }
        return combined_type;
    }

    static const CompGuiNodeType* GetCompGuiCustomType(const CompGuiContext* gui_context, uint32_t custom_type)
    {
        CompGuiNodeType* const * type = gui_context->m_CustomNodeTypes.Get(custom_type);
        if (!type)
        {
            dmLogOnceError("Couldn't find gui node type: %u", custom_type);
            return 0;
        }
        return *type;
    }

    // Called from gui.cpp
    static void RenderNodes(dmGui::HScene scene,
                    const dmGui::RenderEntry* entries,
                    const Matrix4* node_transforms,
                    const float* node_opacities,
                    const dmGui::StencilScope** stencil_scopes,
                    uint32_t node_count,
                    void* context)
    {
        DM_PROFILE("RenderNodes");

        if (node_count == 0)
            return;

        RenderGuiContext* gui_context = (RenderGuiContext*) context;
        GuiWorld* gui_world = gui_context->m_GuiWorld;

        gui_world->m_RenderedParticlesSize = 0;
        gui_context->m_FirstStencil = true;

        dmGui::HNode first_node = entries[0].m_Node;
        dmGui::BlendMode prev_blend_mode = dmGui::GetNodeBlendMode(scene, first_node);
        dmGui::NodeType prev_node_type = dmGui::GetNodeType(scene, first_node);
        uint32_t prev_custom_type = dmGui::GetNodeCustomType(scene, first_node);
        uint64_t prev_combined_type = GetCombinedNodeType(prev_node_type, prev_custom_type);
        dmGraphics::HTexture prev_texture = dmGameSystem::GetNodeTexture(scene, first_node);
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

            dmGui::BlendMode blend_mode = dmGui::GetNodeBlendMode(scene, node);
            dmGui::NodeType node_type = dmGui::GetNodeType(scene, node);
            uint32_t custom_type = dmGui::GetNodeCustomType(scene, node);
            uint64_t combined_type = GetCombinedNodeType(node_type, custom_type);
            dmGraphics::HTexture texture = dmGameSystem::GetNodeTexture(scene, node);
            void* font = dmGui::GetNodeFont(scene, node);
            const dmGui::StencilScope* stencil_scope = stencil_scopes[i];
            uint32_t emitter_batch_key = 0;

            if (node_type == dmGui::NODE_TYPE_PARTICLEFX)
            {
                dmParticle::EmitterRenderData* emitter_render_data = (dmParticle::EmitterRenderData*)entries[i].m_RenderData;
                emitter_batch_key = emitter_render_data->m_MixedHashNoMaterial;
            }

            bool batch_change = combined_type != prev_combined_type ||
                                blend_mode != prev_blend_mode ||
                                texture != prev_texture ||
                                font != prev_font ||
                                prev_stencil_scope != stencil_scope ||
                                prev_emitter_batch_key != emitter_batch_key;

            bool flush = (i > 0 && batch_change);

            if (flush) {
                uint32_t n = i - start;

                switch (prev_node_type)
                {
                    case dmGui::NODE_TYPE_TEXT:
                        RenderTextNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                        break;
                    case dmGui::NODE_TYPE_BOX:
                        RenderBoxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                        break;
                    case dmGui::NODE_TYPE_PIE:
                        RenderPieNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                        break;
                    case dmGui::NODE_TYPE_PARTICLEFX:
                        RenderParticlefxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                        break;
                    case dmGui::NODE_TYPE_CUSTOM:
                        RenderCustomNodes(scene, prev_custom_type, GetCompGuiCustomType(gui_world->m_CompGuiContext, prev_custom_type), entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                        break;
                    default:
                        break;
                }

                start = i;
            }
            prev_node_type = node_type;
            prev_custom_type = custom_type;
            prev_combined_type = combined_type;
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
                    RenderTextNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                    break;
                case dmGui::NODE_TYPE_BOX:
                    RenderBoxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                    break;
                case dmGui::NODE_TYPE_PIE:
                    RenderPieNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                    break;
                case dmGui::NODE_TYPE_PARTICLEFX:
                    RenderParticlefxNodes(scene, entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                    break;
                case dmGui::NODE_TYPE_CUSTOM:
                    RenderCustomNodes(scene, prev_custom_type, GetCompGuiCustomType(gui_world->m_CompGuiContext, prev_custom_type), entries + start, node_transforms + start, node_opacities + start, stencil_scopes + start, n, gui_context);
                    break;
                default:
                    break;
            }
        }

        dmGraphics::SetVertexBufferData(gui_world->m_VertexBuffer,
                                        gui_world->m_ClientVertexBuffer.Size() * sizeof(BoxVertex),
                                        gui_world->m_ClientVertexBuffer.Begin(),
                                        dmGraphics::BUFFER_USAGE_STREAM_DRAW);

        DM_PROPERTY_ADD_U32(rmtp_GuiVertexCount, gui_world->m_ClientVertexBuffer.Size());
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

    static dmGui::FetchTextureSetAnimResult FetchTextureSetAnimCallback(void* texture_set_ptr, dmhash_t animation, dmGui::TextureSetAnimDesc* out_data)
    {
        TextureSetResource* texture_set_res = (TextureSetResource*)texture_set_ptr;
        dmGameSystemDDF::TextureSet* texture_set = texture_set_res->m_TextureSet;
        uint32_t* anim_index = texture_set_res->m_AnimationIds.Get(animation);

        if (anim_index)
        {
            if (texture_set->m_TexCoords.m_Count == 0)
            {
                return dmGui::FETCH_ANIMATION_UNKNOWN_ERROR;
            }
            dmGameSystemDDF::TextureSetAnimation* animation = &texture_set->m_Animations[*anim_index];
            uint32_t playback_index = animation->m_Playback;
            if(playback_index >= dmGui::PLAYBACK_COUNT)
                return dmGui::FETCH_ANIMATION_INVALID_PLAYBACK;

            out_data->m_TexCoords = (const float*) texture_set->m_TexCoords.m_Data;
            out_data->m_State.m_Start = animation->m_Start;
            out_data->m_State.m_End = animation->m_End;
            out_data->m_State.m_OriginalTextureWidth = dmGraphics::GetOriginalTextureWidth(texture_set_res->m_Texture);
            out_data->m_State.m_OriginalTextureHeight = dmGraphics::GetOriginalTextureHeight(texture_set_res->m_Texture);
            out_data->m_State.m_Playback = ddf_playback_map.m_Table[playback_index];
            out_data->m_State.m_FPS = animation->m_Fps;
            out_data->m_FlipHorizontal = animation->m_FlipHorizontal;
            out_data->m_FlipVertical = animation->m_FlipVertical;
            out_data->m_TextureSet = (void*)texture_set;
            return dmGui::FETCH_ANIMATION_OK;
        }
        else
        {
            return dmGui::FETCH_ANIMATION_NOT_FOUND;
        }
    }

    static void* CreateCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type)
    {
        GuiComponent* gui_component = (GuiComponent*)context;
        CompGuiContext* gui_context = gui_component->m_World->m_CompGuiContext;

        CompGuiNodeContext ctx;

        const CompGuiNodeType* type = GetCompGuiCustomType(gui_context, custom_type);
        return type->m_Create(&ctx, type->m_Context, scene, node, custom_type);
    }

    static void* CloneCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type, void* node_data)
    {
        GuiComponent* gui_component = (GuiComponent*)context;
        CompGuiContext* gui_context = gui_component->m_World->m_CompGuiContext;

        CompGuiNodeContext ctx;

        const CompGuiNodeType* type = GetCompGuiCustomType(gui_context, custom_type);

        CustomNodeCtx nodectx;
        nodectx.m_Scene = scene;
        nodectx.m_Node = node;
        nodectx.m_TypeContext = type->m_Context;
        nodectx.m_NodeData = node_data;
        nodectx.m_Type = custom_type;
        return type->m_Clone(&ctx, &nodectx);
    }

    static void DestroyCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type, void* node_data)
    {
        GuiComponent* gui_component = (GuiComponent*)context;
        CompGuiContext* gui_context = gui_component->m_World->m_CompGuiContext;

        const CompGuiNodeType* type = GetCompGuiCustomType(gui_context, custom_type);
        if (!type->m_Destroy)
            return;

        CompGuiNodeContext ctx;

        CustomNodeCtx nodectx;
        nodectx.m_Scene = scene;
        nodectx.m_Node = node;
        nodectx.m_TypeContext = type->m_Context;
        nodectx.m_NodeData = node_data;
        nodectx.m_Type = custom_type;

        type->m_Destroy(&ctx, &nodectx);
    }

    static void UpdateCustomNodeCallback(void* context, dmGui::HScene scene, dmGui::HNode node, uint32_t custom_type, void* node_data, float dt)
    {
        GuiComponent* gui_component = (GuiComponent*)context;
        CompGuiContext* gui_context = gui_component->m_World->m_CompGuiContext;

        const CompGuiNodeType* type = GetCompGuiCustomType(gui_context, custom_type);
        if (!type->m_Update)
            return;

        CustomNodeCtx nodectx;
        nodectx.m_Scene = scene;
        nodectx.m_Node = node;
        nodectx.m_TypeContext = type->m_Context;
        nodectx.m_NodeData = node_data;
        nodectx.m_Type = custom_type;

        type->m_Update(&nodectx, dt);
    }

    static dmGameObject::CreateResult CompGuiAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params) {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        gui_component->m_AddedToUpdate = true;
        return dmGameObject::CREATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult CompGuiUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result)
    {
        DM_PROFILE("Update");

        GuiWorld* gui_world = (GuiWorld*)params.m_World;

        dmScript::UpdateScriptWorld(gui_world->m_ScriptWorld, params.m_UpdateContext->m_DT);

        gui_world->m_DT = params.m_UpdateContext->m_DT;
        dmParticle::Update(gui_world->m_ParticleContext, params.m_UpdateContext->m_DT, &FetchAnimationCallback);
        const uint32_t count = gui_world->m_Components.Size();
        DM_PROPERTY_ADD_U32(rmtp_Gui, count);
        for (uint32_t i = 0; i < count; ++i)
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

    static inline dmRender::HMaterial GetMaterial(GuiComponent* component, GuiSceneResource* resource) {
        return component->m_Material ? component->m_Material : resource->m_Material;
    }

    static dmGameObject::UpdateResult CompGuiRender(const dmGameObject::ComponentsRenderParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
        CompGuiContext* gui_context = (CompGuiContext*)params.m_Context;

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
        uint32_t old_capacity = gui_world->m_GuiRenderObjects.Capacity();
        if (old_capacity < total_gui_render_objects_count) {
            gui_world->m_GuiRenderObjects.SetCapacity(total_gui_render_objects_count);

            uint32_t grow = total_gui_render_objects_count - old_capacity;
            gui_world->m_RenderConstants.SetCapacity(total_gui_render_objects_count);
            gui_world->m_RenderConstants.SetSize(total_gui_render_objects_count);
            memset(&gui_world->m_RenderConstants[old_capacity], 0, grow * sizeof(HComponentRenderConstants));
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
            render_gui_context.m_Material = GetMaterial(c, c->m_Resource);
            dmGui::RenderScene(c->m_Scene, rp, &render_gui_context);
            const uint32_t count = gui_world->m_GuiRenderObjects.Size() - lastEnd;

            dmRender::RenderListEntry* render_list = dmRender::RenderListAlloc(gui_context->m_RenderContext, count);
            dmRender::HRenderListDispatch dispatch = dmRender::RenderListMakeDispatch(gui_context->m_RenderContext, &RenderListDispatch, gui_world);
            dmRender::RenderListEntry* write_ptr = render_list;

            uint32_t render_order = dmGui::GetRenderOrder(c->m_Scene);
            while (lastEnd < gui_world->m_GuiRenderObjects.Size())
            {
                const GuiRenderObject& gro = gui_world->m_GuiRenderObjects[lastEnd];
                write_ptr->m_MinorOrder = 0;
                write_ptr->m_MajorOrder = dmRender::RENDER_ORDER_AFTER_WORLD;
                write_ptr->m_Order = MakeFinalRenderOrder(render_order, gro.m_SortOrder);
                write_ptr->m_UserData = (uintptr_t) &gro.m_RenderObject;
                write_ptr->m_BatchKey = lastEnd;
                write_ptr->m_TagListKey = dmRender::GetMaterialTagListKey(gro.m_RenderObject.m_Material);
                write_ptr->m_Dispatch = dispatch;
                write_ptr++;
                lastEnd++;
            }

            dmRender::RenderListSubmit(gui_context->m_RenderContext, render_list, write_ptr);
        }

        return dmGameObject::UPDATE_RESULT_OK;
    }

    static dmGameObject::UpdateResult CompGuiOnMessage(const dmGameObject::ComponentOnMessageParams& params)
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

    static dmGameObject::InputResult CompGuiOnInput(const dmGameObject::ComponentOnInputParams& params)
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
            gui_input_action.m_GamepadDisconnected = params.m_InputAction->m_GamepadDisconnected;
            gui_input_action.m_GamepadConnected = params.m_InputAction->m_GamepadConnected;
            gui_input_action.m_GamepadPacket = params.m_InputAction->m_GamepadPacket;
            gui_input_action.m_HasGamepadPacket = params.m_InputAction->m_HasGamepadPacket;
            gui_input_action.m_AccX = params.m_InputAction->m_AccX;
            gui_input_action.m_AccY = params.m_InputAction->m_AccY;
            gui_input_action.m_AccZ = params.m_InputAction->m_AccZ;
            gui_input_action.m_AccelerationSet = params.m_InputAction->m_AccelerationSet;

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

    static void CompGuiOnReload(const dmGameObject::ComponentOnReloadParams& params)
    {
        GuiWorld* gui_world = (GuiWorld*)params.m_World;
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
        if (SetupGuiScene(gui_world, gui_component, gui_component->m_Scene, scene_resource))
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

    // Public function used by engine (as callback from gui system)
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

    // Public function used by engine (as callback from gui system)
    uintptr_t GuiGetUserDataCallback(dmGui::HScene scene)
    {
        GuiComponent* component = (GuiComponent*)dmGui::GetSceneUserData(scene);
        return (uintptr_t)component->m_Instance;
    }

    // Public function used by engine (as callback from gui system)
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

    // Public function used by engine (as callback from gui system)
    void GuiGetTextMetricsCallback(const void* font, const char* text, float width, bool line_break, float leading, float tracking, dmGui::TextMetrics* out_metrics)
    {
        dmRender::TextMetrics metrics;
        dmRender::GetTextMetrics((dmRender::HFontMap)font, text, width, line_break, leading, tracking, &metrics);
        out_metrics->m_Width = metrics.m_Width;
        out_metrics->m_Height = metrics.m_Height;
        out_metrics->m_MaxAscent = metrics.m_MaxAscent;
        out_metrics->m_MaxDescent = metrics.m_MaxDescent;
    }

    static dmGameObject::PropertyResult CompGuiGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value) {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        dmhash_t set_property = params.m_PropertyId;
        if (set_property == PROP_MATERIAL) {
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), GetMaterial(gui_component, gui_component->m_Resource), out_value);
        }
        else if (set_property == PROP_FONTS) {
            if (!params.m_Options.m_HasKey) {
                return dmGameObject::PROPERTY_RESULT_INVALID_KEY;
            }
            out_value.m_ValueType = dmGameObject::PROP_VALUE_HASHTABLE;
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), dmGui::GetFont(gui_component->m_Scene, params.m_Options.m_Key), out_value);
        }
        else if (set_property == PROP_TEXTURES) {
            if (!params.m_Options.m_HasKey) {
                return dmGameObject::PROPERTY_RESULT_INVALID_KEY;
            }
            out_value.m_ValueType = dmGameObject::PROP_VALUE_HASHTABLE;
            return GetResourceProperty(dmGameObject::GetFactory(params.m_Instance), dmGui::GetTexture(gui_component->m_Scene, params.m_Options.m_Key), out_value);
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    static dmGameObject::PropertyResult CompGuiSetProperty(const dmGameObject::ComponentSetPropertyParams& params) {
        GuiComponent* gui_component = (GuiComponent*)*params.m_UserData;
        dmhash_t set_property = params.m_PropertyId;
        if (set_property == PROP_MATERIAL) {
            return SetResourceProperty(dmGameObject::GetFactory(params.m_Instance), params.m_Value, MATERIAL_EXT_HASH, (void**)&gui_component->m_Material);
        }
        else if (set_property == PROP_FONTS) {
            if (!params.m_Options.m_HasKey) {
                return dmGameObject::PROPERTY_RESULT_INVALID_KEY;
            }
            dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);
            dmRender::HFontMap font = 0x0;
            dmGameObject::PropertyResult res = SetResourceProperty(factory, params.m_Value, FONT_EXT_HASH, (void**)&font);
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                dmGui::Result r = dmGui::AddFont(gui_component->m_Scene, params.m_Options.m_Key, (void*) font, params.m_Value.m_Hash);
                if (r != dmGui::RESULT_OK) {
                    dmLogError("Unable to set font `%s` property in component `%s`", dmHashReverseSafe64(params.m_Options.m_Key), gui_component->m_Resource->m_Path);
                    dmResource::Release(factory, font);
                    return dmGameObject::PROPERTY_RESULT_BUFFER_OVERFLOW;
                }
                if(gui_component->m_ResourcePropertyPointers.Full()) {
                    gui_component->m_ResourcePropertyPointers.OffsetCapacity(1);
                }
                gui_component->m_ResourcePropertyPointers.Push(font);
            }
            return res;
        }
        else if (set_property == PROP_TEXTURES) {
            if (!params.m_Options.m_HasKey) {
                return dmGameObject::PROPERTY_RESULT_INVALID_KEY;
            }
            dmResource::HFactory factory = dmGameObject::GetFactory(params.m_Instance);
            TextureSetResource* texture_source = 0x0;
            dmGameObject::PropertyResult res = SetResourceProperty(factory, params.m_Value, TEXTURE_SET_EXT_HASH, (void**)&texture_source);
            if (res == dmGameObject::PROPERTY_RESULT_OK)
            {
                dmGraphics::HTexture texture = texture_source->m_Texture;
                dmGui::Result r = dmGui::AddTexture(gui_component->m_Scene, params.m_Options.m_Key, texture_source, dmGui::NODE_TEXTURE_TYPE_TEXTURE_SET, dmGraphics::GetOriginalTextureWidth(texture), dmGraphics::GetOriginalTextureHeight(texture));
                if (r != dmGui::RESULT_OK) {
                    dmLogError("Unable to add texture '%s' to scene (%d)", dmHashReverseSafe64(params.m_Options.m_Key),  r);
                    return dmGameObject::PROPERTY_RESULT_BUFFER_OVERFLOW;
                }
                if(gui_component->m_ResourcePropertyPointers.Full()) {
                    gui_component->m_ResourcePropertyPointers.OffsetCapacity(1);
                }
                gui_component->m_ResourcePropertyPointers.Push(texture_source);
            }
            return res;
        }
        return dmGameObject::PROPERTY_RESULT_NOT_FOUND;
    }

    static bool CompGuiIterGetNext(dmGameObject::SceneNodeIterator* it)
    {
        GuiComponent* component = (GuiComponent*)it->m_NextChild.m_Component;
        dmGui::HNode next = (dmGui::HNode)it->m_NextChild.m_Node;
        if (next == 0)
            return false;

        it->m_Node = it->m_NextChild; // copy data fields

        it->m_NextChild.m_Node = (uint64_t)dmGui::GetNextNode(component->m_Scene, next);
        return it->m_Node.m_Node != 0;
    }

    static void CompGuiIterChildren(dmGameObject::SceneNodeIterator* it, dmGameObject::SceneNode* node)
    {
        GuiComponent* component = (GuiComponent*)node->m_Component;

        it->m_Parent = *node;
        it->m_NextChild = *node; // copy data fields
        it->m_NextChild.m_Type = dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT;

        dmGui::HNode parent = 0;
        if (dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT == node->m_Type)
            parent = (dmGui::HNode)node->m_Node;

        it->m_NextChild.m_Node = (uint64_t)dmGui::GetFirstChildNode(component->m_Scene, parent);
        it->m_FnIterateNext = CompGuiIterGetNext;
    }

    static dmhash_t PivotToHash(dmGui::HScene scene, dmGui::HNode node)
    {
        dmGui::Pivot pivot = dmGui::GetNodePivot(scene, node);
        const char* pivot_name = "";
#define CASE_PIVOT(_NAME) case dmGui:: _NAME: pivot_name = # _NAME; break

        switch (pivot)
        {
        CASE_PIVOT(PIVOT_NW);
        CASE_PIVOT(PIVOT_N);
        CASE_PIVOT(PIVOT_NE);
        CASE_PIVOT(PIVOT_W);
        CASE_PIVOT(PIVOT_CENTER);
        CASE_PIVOT(PIVOT_E);
        CASE_PIVOT(PIVOT_SW);
        CASE_PIVOT(PIVOT_S);
        CASE_PIVOT(PIVOT_SE);
        }

#undef CASE_PIVOT

        return dmHashString64(pivot_name);
    }

    static bool CompGuiIterPropertiesGetNext(dmGameObject::SceneNodePropertyIterator* pit)
    {
        if (pit->m_Node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT)
        {
            return false; // currently not implemented
        }

        GuiComponent* component = (GuiComponent*)pit->m_Node->m_Component;
        dmGui::HNode node = (dmGui::HNode)pit->m_Node->m_Node;
        dmGui::NodeType type = dmGui::GetNodeType(component->m_Scene, node);

        uint64_t index = pit->m_Next++;

        const char* properties_common[] = { "type", "custom_type", "id", "pivot" };
        uint32_t num_properties_common = DM_ARRAY_SIZE(properties_common);

        if (index < num_properties_common)
        {
            const char* type_names[] = {"gui_node_box", "gui_node_text", "gui_node_pie", "gui_node_template", "gui_node_spine", "gui_node_particlefx", "gui_node_custom"};
            // TODO: Get the correct custom type
            const uint32_t num_gui_types = DM_ARRAY_SIZE(type_names);
            DM_STATIC_ASSERT(num_gui_types == dmGui::NODE_TYPE_COUNT, _size_mismatch);

            pit->m_Property.m_NameHash = dmHashString64(properties_common[index]);
            if (index == 0) // type
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH;
                pit->m_Property.m_Value.m_Hash = dmHashString64(type_names[type]);
            } else if (index == 1) // custom_type
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH;
                uint32_t length = 0;
                uint32_t custom_type = dmGui::GetNodeCustomType(component->m_Scene, node);
                const char* custom_type_name = (const char*)dmHashReverse32(custom_type, &length);
                if (custom_type_name == 0)
                    custom_type_name = "";
                pit->m_Property.m_Value.m_Hash = dmHashString64(custom_type_name);
            } else if (index == 2) // id
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH;
                pit->m_Property.m_Value.m_Hash = dmGui::GetNodeId(component->m_Scene, node);
            } else if (index == 3) // pivot
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_HASH;
                pit->m_Property.m_Value.m_Hash = PivotToHash(component->m_Scene, node);
            }

            return true;
        }

        index -= num_properties_common;

        const char* property_names[] = {
            "position",
            "rotation",
            "scale",
            "color",
            "size",
            "outline",
            "shadow",
            "slice9",
            "pie_params",
            "text_params",
        };

        const dmGui::Property* properties = 0;
        uint32_t num_properties = 0;


        const dmGui::Property properties_all[] = {
            dmGui::PROPERTY_POSITION, dmGui::PROPERTY_ROTATION, dmGui::PROPERTY_SCALE, dmGui::PROPERTY_COLOR,
            dmGui::PROPERTY_SIZE, dmGui::PROPERTY_OUTLINE, dmGui::PROPERTY_SHADOW, dmGui::PROPERTY_SLICE9,
            dmGui::PROPERTY_PIE_PARAMS, dmGui::PROPERTY_TEXT_PARAMS,
        };
        properties = properties_all;
        num_properties = DM_ARRAY_SIZE(properties_all);

        if (index < num_properties)
        {
            dmGui::Property property = properties[index];
            Vector4 value = dmGui::GetNodeProperty(component->m_Scene, node, property);

            pit->m_Property.m_NameHash = dmHashString64(property_names[property]);
            pit->m_Property.m_Value.m_V4[0] = value.getX();
            pit->m_Property.m_Value.m_V4[1] = value.getY();
            pit->m_Property.m_Value.m_V4[2] = value.getZ();
            pit->m_Property.m_Value.m_V4[3] = value.getW();
            pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4;
            return true;
        }

        index -= num_properties;

        const char* world_property_names[] = {
            "world_position",
            "world_rotation",
            "world_scale",
            "world_size",
        };

        uint32_t num_world_properties = DM_ARRAY_SIZE(world_property_names);
        if (index < num_world_properties)
        {
            Matrix4 world = dmGui::GetNodeWorldTransform(component->m_Scene, node);
            dmTransform::Transform transform = dmTransform::ToTransform(world);

            dmGameObject::SceneNodePropertyType type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR3;
            Vector4 value;
            switch(index)
            {
                case 0: value = Vector4(transform.GetTranslation()); break;
                case 1: value = Vector4(transform.GetRotation()); type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_VECTOR4; break;
                case 2: value = Vector4(transform.GetScale()); break;
                case 3: value = mulPerElem(dmGui::GetNodeProperty(component->m_Scene, node, dmGui::PROPERTY_SIZE), Vector4(transform.GetScale())); break;
                default:
                    return false;
            }

            pit->m_Property.m_Type = type;
            pit->m_Property.m_NameHash = dmHashString64(world_property_names[index]);
            pit->m_Property.m_Value.m_V4[0] = value.getX();
            pit->m_Property.m_Value.m_V4[1] = value.getY();
            pit->m_Property.m_Value.m_V4[2] = value.getZ();
            pit->m_Property.m_Value.m_V4[3] = value.getW();
            return true;
        }
        index -= num_world_properties;

        uint32_t num_bool_properties = 1;
        if (index < num_bool_properties)
        {
            if (index == 0)
            {
                pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_BOOLEAN;
                pit->m_Property.m_Value.m_Bool = dmGui::IsNodeEnabled(component->m_Scene, node, false);
                pit->m_Property.m_NameHash = dmHashString64("enabled");
            }
            return true;
        }
        index -= num_bool_properties;


        if (type == dmGui::NODE_TYPE_TEXT)
        {
            uint64_t num_text_properties = 1;
            if (index < num_text_properties)
            {
                if (index == 0)
                {
                    pit->m_Property.m_Type = dmGameObject::SCENE_NODE_PROPERTY_TYPE_TEXT;
                    pit->m_Property.m_Value.m_Text = dmGui::GetNodeText(component->m_Scene, node);
                    pit->m_Property.m_NameHash = dmHashString64("text");
                }
                return true;
            }
        }

        return false;
    }

    static void CompGuiIterProperties(dmGameObject::SceneNodePropertyIterator* pit, dmGameObject::SceneNode* node)
    {
        assert(node->m_Type == dmGameObject::SCENE_NODE_TYPE_COMPONENT || node->m_Type == dmGameObject::SCENE_NODE_TYPE_SUBCOMPONENT);
        assert(node->m_ComponentType != 0);
        pit->m_Node = node;
        pit->m_Next = 0;
        pit->m_FnIterateNext = CompGuiIterPropertiesGetNext;
    }

    template <typename T2>
    static void HTCopyCallback(dmHashTable64<T2> *ht, const uint64_t* key, T2* value)
    {
        ht->Put(*key, *value);
    }

    static dmGameObject::Result CompGuiTypeCreate(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        CompGuiContext* gui_context = new CompGuiContext;
        gui_context->m_Factory = ctx->m_Factory;
        gui_context->m_RenderContext = *(dmRender::HRenderContext*)ctx->m_Contexts.Get(dmHashString64("render"));
        gui_context->m_GuiContext = *(dmGui::HContext*)ctx->m_Contexts.Get(dmHashString64("guic"));
        gui_context->m_ScriptContext = *(dmScript::HContext*)ctx->m_Contexts.Get(dmHashString64("gui_scriptc"));

        gui_context->m_MaxGuiComponents = dmConfigFile::GetInt(ctx->m_Config, "gui.max_count", 64);
        gui_context->m_MaxParticleFXCount = dmConfigFile::GetInt(ctx->m_Config, "gui.max_particlefx_count", 64);
        gui_context->m_MaxParticleCount = dmConfigFile::GetInt(ctx->m_Config, "gui.max_particle_count", 1024);

        int32_t max_gui_count = dmConfigFile::GetInt(ctx->m_Config, "gui.max_instance_count", 128);
        gui_context->m_Worlds.SetCapacity(max_gui_count);

        dmGui::InitializeScript(gui_context->m_ScriptContext);

        ComponentTypeSetPrio(type, 300);

        ComponentTypeSetContext(type, gui_context);
        ComponentTypeSetHasUserData(type, true);
        ComponentTypeSetReadsTransforms(type, false);

        ComponentTypeSetNewWorldFn(type, CompGuiNewWorld);
        ComponentTypeSetDeleteWorldFn(type, CompGuiDeleteWorld);
        ComponentTypeSetCreateFn(type, CompGuiCreate);
        ComponentTypeSetDestroyFn(type, CompGuiDestroy);
        ComponentTypeSetInitFn(type, CompGuiInit);
        ComponentTypeSetFinalFn(type, CompGuiFinal);
        ComponentTypeSetAddToUpdateFn(type, CompGuiAddToUpdate);
        ComponentTypeSetUpdateFn(type, CompGuiUpdate);
        ComponentTypeSetRenderFn(type, CompGuiRender);
        ComponentTypeSetOnMessageFn(type, CompGuiOnMessage);
        ComponentTypeSetOnInputFn(type, CompGuiOnInput);
        ComponentTypeSetOnReloadFn(type, CompGuiOnReload);
        ComponentTypeSetGetPropertyFn(type, CompGuiGetProperty);
        ComponentTypeSetSetPropertyFn(type, CompGuiSetProperty);

        ComponentTypeSetChildIteratorFn(type, CompGuiIterChildren);
        ComponentTypeSetPropertyIteratorFn(type, CompGuiIterProperties);
        ComponentTypeSetGetFn(type, CompGuiGetComponent);


        // Now register the node types with the gui
        CompGuiNodeTypeCtx gui_node_ctx;
        gui_node_ctx.m_Config = ctx->m_Config;
        gui_node_ctx.m_Render = gui_context->m_RenderContext;
        gui_node_ctx.m_Factory = gui_context->m_Factory;
        gui_node_ctx.m_GuiContext = gui_context->m_GuiContext;
        gui_node_ctx.m_Script = gui_context->m_ScriptContext;
        gui_node_ctx.m_Contexts.SetCapacity(7, ((dmGameObject::ComponentTypeCreateCtx*)ctx)->m_Contexts.Capacity());
        ctx->m_Contexts.Iterate(HTCopyCallback, &gui_node_ctx.m_Contexts);
        dmGameObject::Result r = dmGameSystem::CreateRegisteredCompGuiNodeTypes(&gui_node_ctx, gui_context);


        if (dmGameObject::RESULT_OK != r)
        {
            dmLogError("Failed to initialize gui component custom node types: %d", r);//dmGameObject::ResultToString(r));
        }

        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::Result CompGuiTypeDestroy(const dmGameObject::ComponentTypeCreateCtx* ctx, dmGameObject::ComponentType* type)
    {
        CompGuiContext* gui_context = (CompGuiContext*)dmGameObject::ComponentTypeGetContext(type);
        if (!gui_context)
        {
            // if the initialization process failed (e.g. unit tests)
            return dmGameObject::RESULT_OK;
        }

        CompGuiNodeTypeCtx gui_node_ctx;
        gui_node_ctx.m_Config = ctx->m_Config;
        gui_node_ctx.m_Render = gui_context->m_RenderContext;
        gui_node_ctx.m_Factory = gui_context->m_Factory;
        gui_node_ctx.m_GuiContext = gui_context->m_GuiContext;
        gui_node_ctx.m_Script = gui_context->m_ScriptContext;
        gui_node_ctx.m_Contexts.SetCapacity(7, ((dmGameObject::ComponentTypeCreateCtx*)ctx)->m_Contexts.Capacity());
        ctx->m_Contexts.Iterate(HTCopyCallback, &gui_node_ctx.m_Contexts);

        dmGameObject::Result r = dmGameSystem::DestroyRegisteredCompGuiNodeTypes(&gui_node_ctx, gui_context);
        if (dmGameObject::RESULT_OK != r)
        {
            dmLogError("Failed to initialize gui component custom node types: %d", r);//dmGameObject::ResultToString(r));
        }

        delete gui_context;
        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result RegisterCompGuiNodeTypeDescriptor(CompGuiNodeTypeDescriptor* desc, const char* name, GuiNodeTypeCreateFunction create_fn, GuiNodeTypeDestroyFunction destroy_fn)
    {
        DM_STATIC_ASSERT(dmGameSystem::s_CompGuiNodeTypeDescBufferSize >= sizeof(CompGuiNodeTypeDescriptor), Invalid_Struct_Size);

        dmLogDebug("Registered comp_gui node type descriptor %s", name);
        desc->m_Name = name;
        desc->m_NameHash = dmHashString32(name);
        desc->m_CreateFn = create_fn;
        desc->m_DestroyFn = destroy_fn;
        desc->m_Next = g_CompGuiNodeTypeSentinel.m_Next;
        g_CompGuiNodeTypeSentinel.m_Next = desc;

        return dmGameObject::RESULT_OK;
    }

    dmGameObject::Result CreateRegisteredCompGuiNodeTypes(const CompGuiNodeTypeCtx* ctx, CompGuiContext* comp_gui_context)
    {
        if (g_CompGuiNodeTypesInitialized)
            return dmGameObject::RESULT_OK;

        CompGuiNodeTypeDescriptor* type_desc = g_CompGuiNodeTypeSentinel.m_Next;
        while (type_desc != 0)
        {
            CompGuiNodeType* node_type = new CompGuiNodeType;
            node_type->m_TypeDesc = type_desc;
            type_desc->m_NodeType = node_type;

            dmGameObject::Result r = type_desc->m_CreateFn(ctx, node_type);
            if (dmGameObject::RESULT_OK != r)
            {
                dmLogError("Failed to register custom gui node type: %s", type_desc->m_Name);
                return r;
            }

            if (comp_gui_context->m_CustomNodeTypes.Full())
            {
                uint32_t capacity = comp_gui_context->m_CustomNodeTypes.Capacity() + 4;
                comp_gui_context->m_CustomNodeTypes.SetCapacity(dmMath::Max(1U, capacity/3), capacity);
            }
            comp_gui_context->m_CustomNodeTypes.Put(type_desc->m_NameHash, node_type);

            type_desc = type_desc->m_Next;
        }
        g_CompGuiNodeTypesInitialized = true;

        return dmGameObject::RESULT_OK;
    }

    static dmGameObject::Result DestroyRegisteredCompGuiNodeTypes(const CompGuiNodeTypeCtx* ctx, CompGuiContext* comp_gui_context)
    {
        if (!g_CompGuiNodeTypesInitialized)
            return dmGameObject::RESULT_OK;

        CompGuiNodeTypeDescriptor* type_desc = g_CompGuiNodeTypeSentinel.m_Next;
        while (type_desc != 0)
        {
            if (type_desc->m_DestroyFn)
            {
                dmGameObject::Result r = type_desc->m_DestroyFn(ctx, type_desc->m_NodeType);
                if (dmGameObject::RESULT_OK != r)
                {
                    dmLogError("Failed to unregister custom gui node type: %s", type_desc->m_Name);
                }
            }

            delete type_desc->m_NodeType;
            type_desc = type_desc->m_Next;
        }

        comp_gui_context->m_CustomNodeTypes.Clear();
        g_CompGuiNodeTypesInitialized = false;
        return dmGameObject::RESULT_OK;
    }

    void    CompGuiNodeTypeSetContext(CompGuiNodeType* type, void* typectx)                       { type->m_Context = typectx; }
    void*   CompGuiNodeTypeGetContext(CompGuiNodeType* type)                                      { return type->m_Context; }
    void    CompGuiNodeTypeSetCreateFn(CompGuiNodeType* type, CompGuiNodeCreateFn fn)             { type->m_Create = fn; }
    void    CompGuiNodeTypeSetDestroyFn(CompGuiNodeType* type, CompGuiNodeDestroyFn fn)           { type->m_Destroy = fn; }
    void    CompGuiNodeTypeSetCloneFn(CompGuiNodeType* type, CompGuiNodeCloneFn fn)               { type->m_Clone = fn; }
    void    CompGuiNodeTypeSetUpdateFn(CompGuiNodeType* type, CompGuiNodeUpdateFn fn)             { type->m_Update = fn; }
    void    CompGuiNodeTypeSetGetVerticesFn(CompGuiNodeType* type, CompGuiNodeGetVerticesFn fn)   { type->m_GetVertices = fn; }
    void    CompGuiNodeTypeSetNodeDescFn(CompGuiNodeType* type, CompGuiNodeSetNodeDescFn fn)      { type->m_SetNodeDesc = fn; }

    void*                   GetContext(const struct CompGuiNodeTypeCtx* ctx, dmhash_t name)     { void* const * p = ctx->m_Contexts.Get(name); if (p) return *p; else return 0; }
    lua_State*              GetLuaState(const struct CompGuiNodeTypeCtx* ctx)                   { return dmScript::GetLuaState(ctx->m_Script); }
    dmScript::HContext      GetScript(const struct CompGuiNodeTypeCtx* ctx)                     { return ctx->m_Script; }
    dmConfigFile::HConfig   GetConfigFile(const struct CompGuiNodeTypeCtx* ctx)                 { return ctx->m_Config; }
    dmResource::HFactory    GetFactory(const struct CompGuiNodeTypeCtx* ctx)                    { return ctx->m_Factory; }
}


DM_DECLARE_COMPONENT_TYPE(ComponentTypeGui, "guic", dmGameSystem::CompGuiTypeCreate, dmGameSystem::CompGuiTypeDestroy);
