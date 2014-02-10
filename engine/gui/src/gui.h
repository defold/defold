#ifndef DM_GUI_H
#define DM_GUI_H

#include <stdint.h>
#include <dlib/message.h>
#include <ddf/ddf.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/easing.h>
#include <dlib/image.h>
#include <hid/hid.h>

#include <script/script.h>

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

/**
 * Defold GUI system
 *
 * About properties:
 * Previously properties where referred to using the enum Property.
 * When support for animating individual tracks was added a more generic access scheme
 * was introduced (hash of a string).
 * With the hash version of the property scalar components
 * can be referred to with "PROPERTY.COMPONENT", e.g. "position.x".
 * Note that when vector values are animated multiple tracks are created internally - one
 * for each component.
 */
namespace dmGui
{
    typedef struct Context* HContext;
    typedef struct Scene* HScene;
    typedef struct Script* HScript;
    typedef uint32_t HNode;

    /**
     * Invalid node handle
     */
    const HNode INVALID_HANDLE = 0;

    /**
     * Default layer id
     */
    const dmhash_t DEFAULT_LAYER = dmHashString64("");

    struct NewSceneParams;
    void SetDefaultNewSceneParams(NewSceneParams* params);

    struct NewSceneParams
    {
        uint32_t m_MaxNodes;
        uint32_t m_MaxAnimations;
        uint32_t m_MaxTextures;
        uint32_t m_MaxFonts;
        uint32_t m_MaxLayers;
        void*    m_UserData;

        NewSceneParams()
        {
            SetDefaultNewSceneParams(this);
        }
    };

    typedef void (*GetURLCallback)(HScene scene, dmMessage::URL* url);
    typedef uintptr_t (*GetUserDataCallback)(HScene scene);
    typedef dmhash_t (*ResolvePathCallback)(HScene scene, const char* path, uint32_t path_size);

    /**
     * Font metrics of a text string
     */
    struct TextMetrics
    {
        /// Default constructor, initializes everything to 0
        TextMetrics();

        /// Total string width
        float m_Width;
        /// Max ascent of font
        float m_MaxAscent;
        /// Max descent of font, positive value
        float m_MaxDescent;
    };
    /** callback for retrieving text metrics
     * The function is expected to fill out the supplied metrics struct with metrics of the supplied font and text.
     *
     * @param font font to measure
     * @param text text to measure
     * @param width max width. used only when line_break is true
     * @param line_break line break characters
     * @param out_metrics the metrics of the supplied text
     */
    typedef void (*GetTextMetricsCallback)(const void* font, const char* text, float width, bool line_break, TextMetrics* out_metrics);

    struct NewContextParams;
    void SetDefaultNewContextParams(NewContextParams* params);

    struct NewContextParams
    {
        dmScript::HContext      m_ScriptContext;
        GetURLCallback          m_GetURLCallback;
        GetUserDataCallback     m_GetUserDataCallback;
        ResolvePathCallback     m_ResolvePathCallback;
        GetTextMetricsCallback  m_GetTextMetricsCallback;
        uint32_t                m_Width;
        uint32_t                m_Height;
        uint32_t                m_PhysicalWidth;
        uint32_t                m_PhysicalHeight;
        dmHID::HContext         m_HidContext;

        NewContextParams()
        {
            SetDefaultNewContextParams(this);
        }
    };

    enum Result
    {
        RESULT_OK = 0,
        RESULT_SYNTAX_ERROR = -1,
        RESULT_SCRIPT_ERROR = -2,
        RESULT_OUT_OF_RESOURCES = -4,
        RESULT_RESOURCE_NOT_FOUND = -5,
        RESULT_TEXTURE_ALREADY_EXISTS = -6,
        RESULT_INVAL_ERROR = -7,
        RESULT_INF_RECURSION = -8,
    };

    enum Property
    {
        PROPERTY_POSITION   = 0,
        PROPERTY_ROTATION   = 1,
        PROPERTY_SCALE      = 2,
        PROPERTY_COLOR      = 3,
        PROPERTY_SIZE       = 4,
        PROPERTY_OUTLINE    = 5,
        PROPERTY_SHADOW     = 6,

        PROPERTY_RESERVED   = 7,

        PROPERTY_COUNT      = 8,
    };

    enum Playback
    {
        PLAYBACK_ONCE_FORWARD  = 0,
        PLAYBACK_ONCE_BACKWARD = 1,
        PLAYBACK_ONCE_PINGPONG = 2,
        PLAYBACK_LOOP_FORWARD  = 3,
        PLAYBACK_LOOP_BACKWARD = 4,
        PLAYBACK_LOOP_PINGPONG = 5,
        PLAYBACK_COUNT = 6,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum BlendMode
    {
        BLEND_MODE_ALPHA     = 0,
        BLEND_MODE_ADD       = 1,
        BLEND_MODE_ADD_ALPHA = 2,
        BLEND_MODE_MULT      = 3,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum NodeType
    {
        NODE_TYPE_BOX  = 0,
        NODE_TYPE_TEXT = 1,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum XAnchor
    {
        XANCHOR_NONE  = 0,
        XANCHOR_LEFT  = 1,
        XANCHOR_RIGHT = 2,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum YAnchor
    {
        YANCHOR_NONE   = 0,
        YANCHOR_TOP    = 1,
        YANCHOR_BOTTOM = 2,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum Pivot
    {
        PIVOT_CENTER = 0,
        PIVOT_N      = 1,
        PIVOT_NE     = 2,
        PIVOT_E      = 3,
        PIVOT_SE     = 4,
        PIVOT_S      = 5,
        PIVOT_SW     = 6,
        PIVOT_W      = 7,
        PIVOT_NW     = 8,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum AdjustMode
    {
        ADJUST_MODE_FIT     = 0,
        ADJUST_MODE_ZOOM    = 1,
        ADJUST_MODE_STRETCH = 2,
    };

    /**
     * Container of input related information.
     */
    struct InputAction
    {
        InputAction();

        /// Action id, hashed action name
        dmhash_t m_ActionId;
        /// Value of the input [0,1]
        float m_Value;
        /// Cursor X coordinate, in virtual screen space
        float m_X;
        /// Cursor Y coordinate, in virtual screen space
        float m_Y;
        /// Cursor dx since last frame, in virtual screen space
        float m_DX;
        /// Cursor dy since last frame, in virtual screen space
        float m_DY;
        /// Cursor X coordinate, in screen space
        float m_ScreenX;
        /// Cursor Y coordinate, in screen space
        float m_ScreenY;
        /// Cursor dx since last frame, in screen space
        float m_ScreenDX;
        /// Cursor dy since last frame, in screen space
        float m_ScreenDY;
        /// Touch data
        dmHID::Touch m_Touch[dmHID::MAX_TOUCH_COUNT];
        /// Number of m_Touch
        int32_t  m_TouchCount;
        char     m_Text[dmHID::MAX_CHAR_COUNT];
        uint32_t m_TextCount;
        /// If the input was 0 last update
        uint16_t m_Pressed : 1;
        /// If the input turned from above 0 to 0 this update
        uint16_t m_Released : 1;
        /// If the input was held enough for the value to be repeated this update
        uint16_t m_Repeated : 1;
        /// If the position fields (m_X, m_Y, m_DX, m_DY) are set and valid to read
        uint16_t m_PositionSet : 1;
    };

    /**
     * Render nodes callback
     * @param scene
     * @param nodes
     * @param node_transforms
     * @param node_count
     * @param context
     */
    typedef void (*RenderNodes)(HScene scene,
                               HNode* nodes,
                               const Vectormath::Aos::Matrix4* node_transforms,
                               uint32_t node_count,
                               void* context);

    /**
     * New texture callback
     * @param scene
     * @param width
     * @param height
     * @param type
     * @param buffer
     * @param context
     */
    typedef void* (*NewTexture)(HScene scene, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, void* context);

    /**
     * Delete texture callback
     * @param scene
     * @param texture
     * @param context
     */
    typedef void (*DeleteTexture)(HScene scene, void* texture, void* context);

    /**
     * Set texture (update) callback
     * @param scene
     * @param texture
     * @param width
     * @param height
     * @param type
     * @param buffer
     * @param context
     */
    typedef void (*SetTextureData)(HScene scene, void* texture, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, void* context);

    typedef void (*AnimationComplete)(HScene scene,
                                      HNode node,
                                      void* userdata1,
                                      void* userdata2);

    HContext NewContext(const NewContextParams* params);

    void DeleteContext(HContext context, dmScript::HContext script_context);

    void SetResolution(HContext context, uint32_t width, uint32_t height);

    void SetPhysicalResolution(HContext context, uint32_t width, uint32_t height);

    void SetDefaultFont(HContext context, void* font);

    HScene NewScene(HContext context, const NewSceneParams* params);

    void DeleteScene(HScene scene);

    void SetSceneUserData(HScene scene, void* user_data);

    void* GetSceneUserData(HScene scene);

    /**
     * Adds a texture with the specified name to the scene.
     * @note Any nodes connected to the same texture_name will also be connected to the new texture. This makes this function O(n), where n is #nodes.
     * @param scene Scene to add the texture to
     * @param texture_name Name of the texture that will be used in the gui scripts
     * @param texture The texture to add
     * @return Outcome of the operation
     */
    Result AddTexture(HScene scene, const char* texture_name, void* texture);

    /**
     * Removes a texture with the specified name from the scene.
     * @note Any nodes connected to the same texture_name will also be disconnected from the texture. This makes this function O(n), where n is #nodes.
     * @param scene Scene to remove texture from
     * @param texture_name Name of the texture that will be used in the gui scripts
     */
    void RemoveTexture(HScene scene, const char* texture_name);

    /**
     * Remove all static textures from the scene.
     * @note User created textures are not removed
     * @note Every node will also be disconnected from the texture they are already connected to, if any. This makes this function O(n), where n is #nodes.
     * @param scene Scene to clear from textures
     */
    void ClearTextures(HScene scene);

    /**
     * Create a new dynamic texture
     * @param scene
     * @param texture_name
     * @param width
     * @param height
     * @param type
     * @param buffer
     * @param buffer_size
     * @return
     */
    Result NewDynamicTexture(HScene scene, const char* texture_name, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, uint32_t buffer_size);

    /**
     * Delete dynamic texture
     * @param scene
     * @param texture_name
     * @return
     */
    Result DeleteDynamicTexture(HScene scene, const char* texture_name);

    /**
     * Update dynamic texture
     * @param scene
     * @param texture_name
     * @param width
     * @param height
     * @param type
     * @param buffer
     * @param buffer_size
     * @return
     */
    Result SetDynamicTextureData(HScene scene, const char* texture_name, uint32_t width, uint32_t height, dmImage::Type type, const void* buffer, uint32_t buffer_size);

    /**
     * Adds a font with the specified name to the scene.
     * @note Any nodes connected to the same font_name will also be connected to the new font. This makes this function O(n), where n is #nodes.
     * @param scene Scene to add texture to
     * @param font_name Name of the font that will be used in the gui scripts
     * @param font The font to add
     * @return Outcome of the operation
     */
    Result AddFont(HScene scene, const char* font_name, void* font);
    /**
     * Removes a font with the specified name from the scene.
     * @note Any nodes connected to the same font_name will also be disconnected from the font. This makes this function O(n), where n is #nodes.
     * @param scene Scene to remove font from
     * @param font_name Name of the font that will be used in the gui scripts
     */
    void RemoveFont(HScene scene, const char* font_name);

    /**
     * Remove all fonts from the scene.
     * @note Every node will also be disconnected from the font they are already connected to, if any. This makes this function O(n), where n is #nodes.
     * @param scene Scene to clear from fonts
     */
    void ClearFonts(HScene scene);

    /**
     * Adds a layer with the specified name to the scene.
     * @param scene Scene to add the layer to
     * @param layer_name Name of the layer that will be used in the gui scripts
     * @return Outcome of the operation
     */
    Result AddLayer(HScene scene, const char* layer_name);

    /** Renders a gui scene
     * Renders a gui scene by calling the callback function render_nodes and supplying an array of nodes.
     *
     * @param scene Scene to render
     * @param render_nodes Callback function to perform the actual rendering
     * @param context User-defined context that will be passed to the callback function
     */
    void RenderScene(HScene scene, RenderNodes render_nodes, void* context);

    struct RenderSceneParams
    {
        RenderSceneParams()
        {
            memset(this, 0, sizeof(*this));
        }

        RenderNodes     m_RenderNodes;
        NewTexture      m_NewTexture;
        DeleteTexture   m_DeleteTexture;
        SetTextureData  m_SetTextureData;
    };

    void RenderScene(HScene scene, const RenderSceneParams& params, void* context);

    /**
     * Run the init-function of the scene script.
     * @param scene Scene for which to run the script
     * @return RESULT_OK on success
     */
    Result InitScene(HScene scene);

    /**
     * Run the final-function of the scene script.
     * @param scene Scene for which to run the script
     * @return RESULT_OK on success
     */
    Result FinalScene(HScene scene);

    /**
     * Run the update-function of the scene script.
     * @param scene Scene for which to run the script
     * @param dt Time step during which to simulate
     * @return RESULT_OK on success
     */
    Result UpdateScene(HScene scene, float dt);

    /**
     * Dispatch DDF or lua-table message to gui script. If the descriptor is NULL
     * message should be a serialized lua-table, see dmScript::CheckTable()
     * Otherwise message should point to a valid ddf-message buffer.
     * @param scene scene
     * @param message_id message-id
     * @param message message payload
     * @param descriptor DDF-descriptor. NULL for lua-table. See comment above.
     * @return RESULT_OK on success
     */
    Result DispatchMessage(HScene scene, dmMessage::Message* message);

    /**
     * Dispatch input to gui script.
     * @param scene scene
     * @param input_actions an array of input actions
     * @param input_action_count the number of input actions in the array
     * @param input_consumed an array of out-parameters signaling if the input was consumed or not
     * @return RESULT_OK on success
     */
    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count, bool* input_consumed);

    /**
     * Run the on_reload-function of the scene script.
     * @param scene Scene for which to run the script
     * @return RESULT_OK on success
     */
    Result ReloadScene(HScene scene);

    Result SetSceneScript(HScene scene, HScript script);

    HScript GetSceneScript(HScene scene);

    HNode NewNode(HScene scene, const Point3& position, const Vector3& size, NodeType node_type);

    void SetNodeId(HScene scene, HNode node, dmhash_t id);
    void SetNodeId(HScene scene, HNode node, const char* id);

    HNode GetNodeById(HScene scene, const char* id);
    HNode GetNodeById(HScene scene, dmhash_t id);

    uint32_t GetNodeCount(HScene scene);

    void DeleteNode(HScene scene, HNode node);

    void ClearNodes(HScene scene);

    /**
     * Reset all nodes to initial state created by SetNodeResetPoint()
     * @param scene
     */
    void ResetNodes(HScene scene);

    /**
     * Get scene render-order. The value is typically used for a render-key when sorting
     * @param scene
     * @return
     */
    uint16_t GetRenderOrder(HScene scene);

    NodeType GetNodeType(HScene scene, HNode node);

    Point3 GetNodePosition(HScene scene, HNode node);

    void SetNodePosition(HScene scene, HNode node, const Point3& position);

    /**
     * Check if a property exists (by hash)
     * @param scene scene
     * @param node node
     * @param property property hash
     * @return true if the property exists
     */
    bool HasPropertyHash(HScene scene, HNode node, dmhash_t property);

    /**
     * Get property value. (vector)
     * @param scene scene
     * @param node node
     * @param property property enum
     * @return property value
     */
    Vector4 GetNodeProperty(HScene scene, HNode node, Property property);

    /**
     * Get property from hash. As opposed to GetNodeProperty() this function
     * can access individual components, e.g. hash("position.x"). For scalar
     * properties the result is returned in the first element
     * @param scene scene
     * @param node node
     * @param property property hash
     * @return property value
     */
    Vector4 GetNodePropertyHash(HScene scene, HNode node, dmhash_t property);

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value);

    /**
     * Save state to reset to. See ResetNodes
     * @param scene
     * @param node
     */
    void SetNodeResetPoint(HScene scene, HNode node);

    const char* GetNodeText(HScene scene, HNode node);
    void SetNodeText(HScene scene, HNode node, const char* text);
    void SetNodeLineBreak(HScene scene, HNode node, bool line_break);
    bool GetNodeLineBreak(HScene scene, HNode node);

    void* GetNodeTexture(HScene scene, HNode node);
    dmhash_t GetNodeTextureId(HScene scene, HNode node);
    Result SetNodeTexture(HScene scene, HNode node, dmhash_t texture_id);
    Result SetNodeTexture(HScene scene, HNode node, const char* texture_id);

    void* GetNodeFont(HScene scene, HNode node);
    dmhash_t GetNodeFontId(HScene scene, HNode node);
    Result SetNodeFont(HScene scene, HNode node, dmhash_t font_id);
    Result SetNodeFont(HScene scene, HNode node, const char* font_id);

    dmhash_t GetNodeLayerId(HScene scene, HNode node);
    Result SetNodeLayer(HScene scene, HNode node, dmhash_t layer_id);
    Result SetNodeLayer(HScene scene, HNode node, const char* layer_id);

    Result GetTextMetrics(HScene scene, const char* text, const char* font_id, float width, bool line_break, TextMetrics* metrics);
    Result GetTextMetrics(HScene scene, const char* text, dmhash_t font_id, float width, bool line_break, TextMetrics* metrics);

    BlendMode GetNodeBlendMode(HScene scene, HNode node);
    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode);

    XAnchor GetNodeXAnchor(HScene scene, HNode node);
    void SetNodeXAnchor(HScene scene, HNode node, XAnchor x_anchor);
    YAnchor GetNodeYAnchor(HScene scene, HNode node);
    void SetNodeYAnchor(HScene scene, HNode node, YAnchor y_anchor);
    Pivot GetNodePivot(HScene scene, HNode node);
    void SetNodePivot(HScene scene, HNode node, Pivot pivot);

    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode);

    /**
     * Animate node vector property. Internally multiple tracks are created.
     * This function is obsolete in favor for AnimateNodeHash
     *
     * @param scene
     * @param node
     * @param property
     * @param to
     * @param easing
     * @param playback
     * @param duration
     * @param delay
     * @param animation_complete
     * @param userdata1
     * @param userdata2
     */
    void AnimateNode(HScene scene, HNode node,
                     Property property,
                     const Vector4& to,
                     dmEasing::Type easing,
                     Playback playback,
                     float duration,
                     float delay,
                     AnimationComplete animation_complete,
                     void* userdata1,
                     void* userdata2);

    /**
     * Animate property. The property parameter is the hash value of the property to animate.
     * Could either be vector property, e.g. "position", or a scalar "position.x". For vector
     * properties multiple animation-tracks are created internally but the call-back is only
     * invoked for the first.
     *
     * @param scene
     * @param node
     * @param property
     * @param to
     * @param easing
     * @param playback
     * @param duration
     * @param delay
     * @param animation_complete
     * @param userdata1
     * @param userdata2
     */
    void AnimateNodeHash(HScene scene, HNode node,
                         dmhash_t property,
                         const Vector4& to,
                         dmEasing::Type easing,
                         Playback playback,
                         float duration,
                         float delay,
                         AnimationComplete animation_complete,
                         void* userdata1,
                         void* userdata2);

    void CancelAnimation(HScene scene, HNode node, Property property);
    void CancelAnimationHash(HScene scene, HNode node, dmhash_t property_hash);

    /** determines if a node can be picked
     * Reports if a node is picked by the supplied screen-space coordinates.
     *
     * @param scene the scene the node exists in
     * @param node node to be picked
     * @param x x-coordinate in predefined screen-space
     * @param y y-coordinate in predefined screen-space
     * @return true if the node was picked, false otherwise
     */
    bool PickNode(HScene scene, HNode node, float x, float y);

    /** retrieves if a node is enabled or not
     * Only enabled nodes are animated and rendered.
     *
     * @param scene the scene the node exists in
     * @param node the node to be enabled/disabled
     * @return whether the node is enabled or not
     */
    bool IsNodeEnabled(HScene scene, HNode node);
    /** enables/disables a node
     * Set if a node should be enabled or not. Only enabled nodes are animated and rendered.
     *
     * @note This function updates every animation acting on the node and has a complexity of O(n) over the total animation count in the scene.
     * @param scene the scene the node exists in
     * @param node the node to be enabled/disabled
     * @param enabled whether the node should be enabled
     */
    void SetNodeEnabled(HScene scene, HNode node, bool enabled);

    Result SetNodeParent(HScene scene, HNode node, HNode parent);

    Result CloneNode(HScene scene, HNode node, HNode* out_node);

    /** reorders the given node relative the reference
     * Move the given node to be positioned above the reference node.
     * If the reference node is INVALID_HANDLE, the node is moved to the top.
     *
     * @note This function might alter the indices of any number of nodes in the scene.
     * @param scene Scene the node exists in
     * @param node Node to be moved
     * @param reference Node the first node should be moved in relation to, might be INVALID_HANDLE
     */
    void MoveNodeAbove(HScene scene, HNode node, HNode reference);
    /** reorders the given node relative the reference
     * Move the given node to be positioned below the reference node.
     * If the reference node is INVALID_HANDLE, the node is moved to the bottom.
     *
     * @note This function might alter the indices of any number of nodes in the scene.
     * @param scene Scene the node exists in
     * @param node Node to be moved
     * @param reference Node the first node should be moved in relation to, might be INVALID_HANDLE
     */
    void MoveNodeBelow(HScene scene, HNode node, HNode reference);

    HScript NewScript(HContext context);
    void DeleteScript(HScript script);
    Result SetScript(HScript script, const char* source, uint32_t source_length, const char* filename);

    /** Gets the lua state used for gui scripts.
     * @return lua state
     */
    lua_State* GetLuaState(HContext context);
}

#endif
