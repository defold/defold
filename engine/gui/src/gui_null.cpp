// Copyright 2020-2026 The Defold Foundation
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

#include "gui.h"

#include "gui_private.h"
#include "gui_script.h"
#include <rig/rig.h>
#include <dmsdk/dlib/vmath.h>

namespace dmGui
{
    using namespace dmVMath;

    void SetDefaultNewContextParams(NewContextParams* params)
    {
    }

    HScene GetSceneFromLua(lua_State* L)
    {
        return 0;
    }

    lua_State* InitializeScript(dmScript::HContext script_context)
    {
        return 0;
    }

    void FinalizeScript(lua_State* L, dmScript::HContext script_context)
    {
    }


    // gui_null.cpp
    const dmhash_t DEFAULT_LAYER = dmHashString64("");
    const dmhash_t DEFAULT_LAYOUT = DEFAULT_LAYER;
    const uint16_t INVALID_INDEX = 0xffff;

    InputAction::InputAction()
    {
    }

    HContext NewContext(const NewContextParams* params)
    {
        return 0;
    }

    void DeleteContext(HContext context, dmScript::HContext script_context)
    {
    }

    void GetPhysicalResolution(HContext context, uint32_t& width, uint32_t& height)
    {
        width = 0;
        height = 0;
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        return 0;
    }

    void GetSceneResolution(HScene scene, uint32_t& width, uint32_t& height)
    {
        width = 0;
        height = 0;
    }

    void SetSceneResolution(HScene scene, uint32_t width, uint32_t height)
    {
    }

    void GetPhysicalResolution(HScene scene, uint32_t& width, uint32_t& height)
    {
        width = 0;
        height = 0;
    }

    uint32_t GetDisplayDpi(HScene scene)
    {
        return 0;
    }

    void SetPhysicalResolution(HContext context, uint32_t width, uint32_t height)
    {
    }

    void SetSafeAreaAdjust(HContext context, bool enabled, uint32_t width, uint32_t height, float offset_x, float offset_y)
    {
    }

    void UpdateSafeAreaAdjust(HContext context, SafeAreaMode mode, uint32_t window_width, uint32_t window_height,
                              int32_t inset_left, int32_t inset_top, int32_t inset_right, int32_t inset_bottom)
    {
    }

    SafeAreaMode ParseSafeAreaMode(const char* mode)
    {
        return SAFE_AREA_NONE;
    }

    void SetSceneSafeAreaMode(HScene scene, SafeAreaMode mode)
    {
    }

    void GetDefaultResolution(HContext context, uint32_t& width, uint32_t& height)
    {
        width = 0;
        height = 0;
    }

    void SetDefaultResolution(HContext context, uint32_t width, uint32_t height)
    {
    }

    void* GetDisplayProfiles(HScene scene)
    {
        return 0;
    }

    AdjustReference GetSceneAdjustReference(HScene scene)
    {
        return ADJUST_REFERENCE_DISABLED;
    }

    void SetDisplayProfiles(HContext context, void* display_profiles)
    {
    }

    void SetDefaultFont(HContext context, void* font)
    {
    }

    void SetSceneAdjustReference(HScene scene, AdjustReference adjust_reference)
    {
    }

    void SetDefaultNewSceneParams(NewSceneParams* params)
    {
    }

    HScene NewScene(HContext context, const NewSceneParams* params)
    {
        return 0;
    }

    void DeleteScene(HScene scene)
    {
    }

    void SetSceneUserData(HScene scene, void* userdata)
    {
    }

    void* GetSceneUserData(HScene scene)
    {
        return 0;
    }

    Result AddTexture(HScene scene, dmhash_t texture_name_hash, dmGui::HTextureSource texture_source, NodeTextureType texture_type, uint32_t original_width, uint32_t original_height)
    {
        return RESULT_OK;
    }

    void RemoveTexture(HScene scene, dmhash_t texture_name_hash)
    {
    }

    void ClearTextures(HScene scene)
    {
    }

    HTextureSource GetTexture(HScene scene, dmhash_t texture_name_hash)
    {
        return 0;
    }

    Result NewDynamicTexture(HScene scene, const dmhash_t texture_hash, uint32_t width, uint32_t height, dmImage::Type type, dmImage::CompressionType compression_type, bool flip, const void* buffer, uint32_t buffer_size)
    {
        return RESULT_OK;
    }

    Result DeleteDynamicTexture(HScene scene, const dmhash_t texture_hash)
    {
        return RESULT_OK;
    }

    Result SetDynamicTextureData(HScene scene, const dmhash_t texture_hash, uint32_t width, uint32_t height, dmImage::Type type, dmImage::CompressionType compression_type, bool flip, const void* buffer, uint32_t buffer_size)
    {
        return RESULT_OK;
    }

    Result GetDynamicTextureData(HScene scene, const dmhash_t texture_hash, uint32_t* out_width, uint32_t* out_height, dmImage::Type* out_type, const void** out_buffer)
    {
        return RESULT_OK;
    }

    Result AddFont(HScene scene, dmhash_t font_name_hash, void* font, dmhash_t path_hash)
    {
        return RESULT_OK;
    }

    void RemoveFont(HScene scene, dmhash_t font_name_hash)
    {
    }

    dmhash_t GetFontPath(HScene scene, dmhash_t font_hash)
    {
        return 0;
    }

    void* GetFont(HScene scene, dmhash_t font_hash)
    {
        return 0;
    }

    Result AddParticlefx(HScene scene, const char* particlefx_name, void* particlefx_prototype)
    {
        return RESULT_OK;
    }

    void RemoveParticlefx(HScene scene, const char* particlefx_name)
    {
    }

    void ClearFonts(HScene scene)
    {
    }

    Result AddLayer(HScene scene, const char* layer_name)
    {
        return RESULT_OK;
    }

    void AllocateLayouts(HScene scene, size_t node_count, size_t layouts_count)
    {
    }

    void ClearLayouts(HScene scene)
    {
    }

    Result AddLayout(HScene scene, const char* layout_id)
    {
        return RESULT_OK;
    }

    dmhash_t GetLayout(const HScene scene)
    {
        return 0;
    }

    uint16_t GetLayoutCount(const HScene scene)
    {
        return 0;
    }

    Result GetLayoutId(const HScene scene, uint16_t layout_index, dmhash_t& layout_id_out)
    {
        return RESULT_OK;
    }

    uint16_t GetLayoutIndex(const HScene scene, dmhash_t layout_id)
    {
        return 0;
    }

    Result SetNodeLayoutDesc(const HScene scene, HNode node, const void *desc, uint16_t layout_index_start, uint16_t layout_index_end)
    {
        return RESULT_OK;
    }

    Result SetLayout(const HScene scene, dmhash_t layout_id, SetNodeCallback set_node_callback)
    {
        return RESULT_OK;
    }

    HNode GetNodeHandle(InternalNode* node)
    {
        return 0;
    }

    TextureSetAnimDesc* GetNodeTextureSet(HScene scene, HNode node)
    {
        return 0;
    }

    int32_t GetNodeAnimationFrame(HScene scene, HNode node)
    {
        return 0;
    }

    int32_t GetNodeAnimationFrameCount(HScene scene, HNode node)
    {
        return 0;
    }

    const float* GetNodeFlipbookAnimUV(HScene scene, HNode node)
    {
        return 0;
    }

    Vector4 CalculateReferenceScale(HScene scene, InternalNode* node)
    {
        return Vector4(0, 0, 0, 0);
    }

    void RenderScene(HScene scene, const RenderSceneParams& params, void* context)
    {
    }

    void RenderScene(HScene scene, RenderNodes render_nodes, void* context)
    {
    }

    void UpdateAnimations(HScene scene, float dt)
    {
    }

    Result RunScript(HScene scene, ScriptFunction script_function, int custom_ref, void* args)
    {
        return RESULT_OK;
    }

    Result InitScene(HScene scene)
    {
        return RESULT_OK;
    }

    Result FinalScene(HScene scene)
    {
        return RESULT_OK;
    }

    Result UpdateScene(HScene scene, float dt)
    {
        return RESULT_OK;
    }

    HNode GetFirstChildNode(HScene scene, HNode node)
    {
        return 0;
    }

    HNode GetNextNode(HScene scene, HNode node)
    {
        return 0;
    }

    Result DispatchMessage(HScene scene, dmMessage::Message* message)
    {
        return RESULT_OK;
    }

    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count, bool* input_consumed)
    {
        return RESULT_OK;
    }

    Result ReloadScene(HScene scene)
    {
        return RESULT_OK;
    }

    Result SetSceneScript(HScene scene, HScript script)
    {
        return RESULT_OK;
    }

    HScript GetSceneScript(HScene scene)
    {
        return 0;
    }

    HNode NewNode(HScene scene, const Point3& position, const Vector3& size, NodeType node_type, uint32_t custom_type)
    {
        return 0;
    }

    void DeleteNode(HScene scene, HNode node, bool delete_headless_pfx)
    {
    }

    InternalNode* GetNode(HScene scene, HNode node)
    {
        return 0;
    }

    void SetNodeId(HScene scene, HNode node, dmhash_t id)
    {
    }

    void SetNodeId(HScene scene, HNode node, const char* id)
    {
    }

    dmhash_t GetNodeId(HScene scene, HNode node)
    {
        return 0;
    }

    HNode GetNodeById(HScene scene, const char* id)
    {
        return 0;
    }

    HNode GetNodeById(HScene scene, dmhash_t id)
    {
        return 0;
    }

    uint32_t GetNodeCount(HScene scene)
    {
        return 0;
    }

    uint32_t GetParticlefxCount(HScene scene)
    {
        return 0;
    }


    void ClearNodes(HScene scene)
    {
    }

    void UpdateLocalTransform(HScene scene, InternalNode* n)
    {
    }

    void ResetNodes(HScene scene)
    {
    }

    uint16_t GetRenderOrder(HScene scene)
    {
        return 0;
    }

    NodeType GetNodeType(HScene scene, HNode node)
    {
        return NODE_TYPE_BOX;
    }

    Point3 GetNodePosition(HScene scene, HNode node)
    {
        return Point3(0,0,0);
    }

    Point3 GetNodeSize(HScene scene, HNode node)
    {
        return Point3(0,0,0);
    }

    Matrix4 GetNodeWorldTransform(HScene scene, HNode node)
    {
        Matrix4 world;
        return world;
    }

    Vector4 GetNodeSlice9(HScene scene, HNode node)
    {
        return Vector4(0, 0, 0, 0);
    }

    void SetNodePosition(HScene scene, HNode node, const Point3& position)
    {
    }

    bool HasPropertyHash(HScene scene, HNode node, dmhash_t property)
    {
        return false;
    }

    Vector4 GetNodeProperty(HScene scene, HNode node, Property property)
    {
        return Vector4(0, 0, 0, 0);
    }

    Vector4 GetNodePropertyHash(HScene scene, HNode node, dmhash_t property)
    {
        return Vector4(0, 0, 0, 0);
    }

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value)
    {
    }

    void SetNodeResetPoint(HScene scene, HNode node)
    {
    }

    const char* GetNodeText(HScene scene, HNode node)
    {
        return 0;
    }

    void SetNodeText(HScene scene, HNode node, const char* text)
    {
    }

    void SetNodeLineBreak(HScene scene, HNode node, bool line_break)
    {
    }

    bool GetNodeLineBreak(HScene scene, HNode node)
    {
        return false;
    }

    void SetNodeTextLeading(HScene scene, HNode node, float leading)
    {
    }

    float GetNodeTextLeading(HScene scene, HNode node)
    {
        return 0.0f;
    }

    void SetNodeTextTracking(HScene scene, HNode node, float tracking)
    {
    }

    float GetNodeTextTracking(HScene scene, HNode node)
    {
        return 0.0f;
    }

    HTextureSource GetNodeTexture(HScene scene, HNode node, NodeTextureType* textureTypeOut)
    {
        return 0;
    }

    dmhash_t GetNodeTextureId(HScene scene, HNode node)
    {
        return 0;
    }

    dmhash_t GetNodeFlipbookAnimId(HScene scene, HNode node)
    {
        return 0;
    }

    Result SetNodeTexture(HScene scene, HNode node, dmhash_t texture_id)
    {
        return RESULT_OK;
    }

    Result SetNodeTexture(HScene scene, HNode node, const char* texture_id)
    {
        return RESULT_OK;
    }

    Result SetNodeParticlefx(HScene scene, HNode node, dmhash_t particlefx_id)
    {
        return RESULT_OK;
    }

    Result GetNodeParticlefx(HScene scene, HNode node, dmhash_t& particlefx_id)
    {
        return RESULT_OK;
    }

    Result SetNodeParticlefxConstant(HScene scene, HNode node, dmhash_t emitter_id, dmhash_t constant_id, Vector4& value)
    {
        return RESULT_OK;
    }

    Result ResetNodeParticlefxConstant(HScene scene, HNode node, dmhash_t emitter_id, dmhash_t constant_id)
    {
        return RESULT_OK;
    }

    void* GetNodeFont(HScene scene, HNode node)
    {
        return 0;
    }

    dmhash_t GetNodeFontId(HScene scene, HNode node)
    {
        return 0;
    }

    Result SetNodeFont(HScene scene, HNode node, dmhash_t font_id)
    {
        return RESULT_OK;
    }

    Result SetNodeFont(HScene scene, HNode node, const char* font_id)
    {
        return RESULT_OK;
    }

    dmhash_t GetNodeLayerId(HScene scene, HNode node)
    {
        return 0;
    }

    Result SetNodeLayer(HScene scene, HNode node, dmhash_t layer_id)
    {
        return RESULT_OK;
    }

    Result SetNodeLayer(HScene scene, HNode node, const char* layer_id)
    {
        return RESULT_OK;
    }

    bool GetNodeInheritAlpha(HScene scene, HNode node)
    {
        return false;
    }

    void SetNodeInheritAlpha(HScene scene, HNode node, bool inherit_alpha)
    {
    }

    void SetNodeAlpha(HScene scene, HNode node, float alpha)
    {
    }

    float GetNodeAlpha(HScene scene, HNode node)
    {
        return 0.0f;
    }

    float GetNodeFlipbookCursor(HScene scene, HNode node)
    {
        return 0.0f;
    }

    void SetNodeFlipbookCursor(HScene scene, HNode node, float cursor)
    {
    }

    float GetNodeFlipbookPlaybackRate(HScene scene, HNode node)
    {
        return 0.0f;
    }

    void SetNodeFlipbookPlaybackRate(HScene scene, HNode node, float playback_rate)
    {
    }

    Result PlayNodeParticlefx(HScene scene, HNode node, dmParticle::EmitterStateChangedData* callbackdata)
    {
        return RESULT_OK;
    }

    Result StopNodeParticlefx(HScene scene, HNode node, bool clear_particles)
    {
        return RESULT_OK;
    }

    void SetNodeClippingMode(HScene scene, HNode node, ClippingMode mode)
    {
    }

    ClippingMode GetNodeClippingMode(HScene scene, HNode node)
    {
        return CLIPPING_MODE_NONE;
    }

    void SetNodeClippingVisible(HScene scene, HNode node, bool visible)
    {
    }

    bool GetNodeClippingVisible(HScene scene, HNode node)
    {
        return false;
    }

    void SetNodeClippingInverted(HScene scene, HNode node, bool inverted)
    {
    }

    bool GetNodeClippingInverted(HScene scene, HNode node)
    {
        return false;
    }

    Result GetTextMetrics(HScene scene, const char* text, const char* font_id, float width, bool line_break, float leading, float tracking, TextMetrics* metrics)
    {
        return RESULT_OK;
    }

    Result GetTextMetrics(HScene scene, const char* text, dmhash_t font_id, float width, bool line_break, float leading, float tracking, TextMetrics* metrics)
    {
        return RESULT_OK;
    }

    BlendMode GetNodeBlendMode(HScene scene, HNode node)
    {
        return BLEND_MODE_ADD;
    }

    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode)
    {
    }

    XAnchor GetNodeXAnchor(HScene scene, HNode node)
    {
        return XANCHOR_NONE;
    }

    void SetNodeXAnchor(HScene scene, HNode node, XAnchor x_anchor)
    {
    }

    YAnchor GetNodeYAnchor(HScene scene, HNode node)
    {
        return YANCHOR_NONE;
    }

    void SetNodeYAnchor(HScene scene, HNode node, YAnchor y_anchor)
    {
    }


    void SetNodeOuterBounds(HScene scene, HNode node, PieBounds bounds)
    {
    }

    void SetNodePerimeterVertices(HScene scene, HNode node, uint32_t vertices)
    {
    }

    void SetNodeInnerRadius(HScene scene, HNode node, float radius)
    {
    }

    void SetNodePieFillAngle(HScene scene, HNode node, float fill_angle)
    {
    }

    PieBounds GetNodeOuterBounds(HScene scene, HNode node)
    {
        return PIEBOUNDS_RECTANGLE;
    }

    uint32_t GetNodePerimeterVertices(HScene scene, HNode node)
    {
        return 0;
    }

    float GetNodeInnerRadius(HScene scene, HNode node)
    {
        return 0.0f;
    }

    float GetNodePieFillAngle(HScene scene, HNode node)
    {
        return 0.0f;
    }

    Pivot GetNodePivot(HScene scene, HNode node)
    {
        return PIVOT_CENTER;
    }

    void SetNodePivot(HScene scene, HNode node, Pivot pivot)
    {
    }

    bool GetNodeIsBone(HScene scene, HNode node)
    {
        return false;
    }

    void SetNodeIsBone(HScene scene, HNode node, bool is_bone)
    {
    }

    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode)
    {
    }

    void SetNodeSizeMode(HScene scene, HNode node, SizeMode size_mode)
    {
    }

    SizeMode GetNodeSizeMode(HScene scene, HNode node)
    {
        return SIZE_MODE_AUTO;
    }


    void AnimateNodeHash(HScene scene,
                         HNode node,
                         dmhash_t property,
                         const Vector4& to,
                         dmEasing::Curve easing,
                         Playback playback,
                         float duration,
                         float delay,
                         AnimationComplete animation_complete,
                         void* userdata1,
                         void* userdata2)
    {
    }

    dmhash_t GetPropertyHash(Property property)
    {
        return 0;
    }

    void CancelAnimationHash(HScene scene, HNode node, dmhash_t property_hash)
    {
    }


    inline Animation* GetComponentAnimation(HScene scene, HNode node, float* value)
    {
        uint16_t version = (uint16_t) (node >> 16);
        uint16_t index = node & 0xffff;
        InternalNode* n = &scene->m_Nodes[index];
        assert(n->m_Version == version);

        dmArray<Animation>* animations = &scene->m_Animations;
        uint32_t n_animations = animations->Size();
        for (uint32_t i = 0; i < n_animations; ++i)
        {
            Animation* anim = &(*animations)[i];
            if (anim->m_Node == node && anim->m_Value == value)
                return anim;
        }
        return 0;
    }


    Result PlayNodeFlipbookAnim(HScene scene, HNode node, dmhash_t anim, float offset, float playback_rate, AnimationComplete anim_complete_callback, void* callback_userdata1, void* callback_userdata2)
    {
        return RESULT_OK;
    }

    Result PlayNodeFlipbookAnim(HScene scene, HNode node, const char* anim, float offset, float playback_rate, AnimationComplete anim_complete_callback, void* callback_userdata1, void* callback_userdata2)
    {
        return RESULT_OK;
    }
    
    void CancelNodeFlipbookAnim(HScene scene, HNode node, bool keep_anim_hash)
    {
    }

    void CancelNodeFlipbookAnim(HScene scene, HNode node)
    {
    }

    void GetNodeFlipbookAnimUVFlip(HScene scene, HNode node, bool& flip_horizontal, bool& flip_vertical)
    {
    }

    bool PickNode(HScene scene, HNode node, float x, float y)
    {
        return false;
    }

    bool IsNodeEnabled(HScene scene, HNode node, bool recursive)
    {
        return false;
    }

    void SetNodeEnabled(HScene scene, HNode node, bool enabled)
    {
    }

    bool GetNodeVisible(HScene scene, HNode node)
    {
        return false;
    }

    void SetNodeVisible(HScene scene, HNode node, bool visible)
    {
    }

    void SetScreenPosition(HScene scene, HNode node, const dmVMath::Point3& screen_position)
    {
    }

    dmVMath::Point3 ScreenToLocalPosition(HScene scene, HNode node, const dmVMath::Point3& screen_position)
    {
        return Point3(0,0,0);
    }

    void MoveNodeBelow(HScene scene, HNode node, HNode reference)
    {
    }

    void MoveNodeAbove(HScene scene, HNode node, HNode reference)
    {
    }

    Result SetNodeParent(HScene scene, HNode node, HNode parent, bool keep_scene_transform)
    {
        return RESULT_OK;
    }

    Result CloneNode(HScene scene, HNode node, HNode* out_node)
    {
        return RESULT_OK;
    }

    void CalculateNodeTransform(HScene scene, InternalNode* n, const CalculateNodeTransformFlags flags, Matrix4& out_transform)
    {
    }

    HScript NewScript(HContext context)
    {
        return 0;
    }

    void DeleteScript(HScript script)
    {
    }

    Result SetScript(HScript script, dmLuaDDF::LuaSource *source)
    {
        return RESULT_OK;
    }

    lua_State* GetLuaState(HContext context)
    {
        return 0;
    }

    int GetContextTableRef(HScene scene)
    {
        return 0;
    }

}  // namespace dmGui
