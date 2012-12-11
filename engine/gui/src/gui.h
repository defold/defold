#ifndef DM_GUI_H
#define DM_GUI_H

#include <stdint.h>
#include <dlib/message.h>
#include <ddf/ddf.h>
#include <dlib/hash.h>
#include <dlib/message.h>

#include <script/script.h>

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

namespace dmGui
{
    typedef struct Context* HContext;
    typedef struct Scene* HScene;
    typedef struct Script* HScript;
    typedef uint32_t HNode;

    struct NewSceneParams;
    void SetDefaultNewSceneParams(NewSceneParams* params);

    struct NewSceneParams
    {
        uint32_t m_MaxNodes;
        uint32_t m_MaxAnimations;
        uint32_t m_MaxTextures;
        uint32_t m_MaxFonts;
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
     * @param out_metrics the metrics of the supplied text
     */
    typedef void (*GetTextMetricsCallback)(const void* font, const char* text, TextMetrics* out_metrics);

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

    enum Easing
    {
        EASING_NONE = 0,
        EASING_IN = 1,
        EASING_OUT = 2,
        EASING_INOUT = 3,

        EASING_COUNT = 4,
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
        /// If the input was 0 last update
        uint16_t m_Pressed : 1;
        /// If the input turned from above 0 to 0 this update
        uint16_t m_Released : 1;
        /// If the input was held enough for the value to be repeated this update
        uint16_t m_Repeated : 1;
        /// If the position fields (m_X, m_Y, m_DX, m_DY) are set and valid to read
        uint16_t m_PositionSet : 1;
    };

    typedef void (*RenderNodes)(HScene scene,
                               HNode* nodes,
                               const Vectormath::Aos::Matrix4* node_transforms,
                               uint32_t node_count,
                               void* context);

    typedef void (*AnimationComplete)(HScene scene,
                                      HNode node,
                                      void* userdata1,
                                      void* userdata2);

    HContext NewContext(const NewContextParams* params);

    void DeleteContext(HContext context);

    void SetResolution(HContext context, uint32_t width, uint32_t height);

    void SetPhysicalResolution(HContext context, uint32_t width, uint32_t height);

    HScene NewScene(HContext context, const NewSceneParams* params);

    void DeleteScene(HScene scene);

    void SetSceneUserData(HScene scene, void* user_data);

    void* GetSceneUserData(HScene scene);

    /**
     * Adds a texture with the specified name to the scene.
     * @note Any nodes connected to the same texture_name will also be connected to the new texture. This makes this function O(n), where n is #nodes.
     * @param scene Scene to add texture to
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
     * Remove all textures from the scene.
     * @note Every node will also be disconnected from the texture they are already connected to, if any. This makes this function O(n), where n is #nodes.
     * @param scene Scene to clear from textures
     */
    void ClearTextures(HScene scene);

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

    /** Renders a gui scene
     * Renders a gui scene by calling the callback function render_nodes and supplying an array of nodes.
     *
     * @param scene Scene to render
     * @param render_nodes Callback function to perform the actual rendering
     * @param context User-defined context that will be passed to the callback function
     */
    void RenderScene(HScene scene, RenderNodes render_nodes, void* context);

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

    void SetNodeId(HScene scene, HNode node, const char* name);

    HNode GetNodeById(HScene scene, const char* id);

    uint32_t GetNodeCount(HScene scene);

    void DeleteNode(HScene scene, HNode node);

    void ClearNodes(HScene scene);

    NodeType GetNodeType(HScene scene, HNode node);

    Point3 GetNodePosition(HScene scene, HNode node);

    void SetNodePosition(HScene scene, HNode node, const Point3& position);

    Vector4 GetNodeProperty(HScene scene, HNode node, Property property);

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value);

    const char* GetNodeText(HScene scene, HNode node);
    void SetNodeText(HScene scene, HNode node, const char* text);

    void* GetNodeTexture(HScene scene, HNode node);
    Result SetNodeTexture(HScene scene, HNode node, const char* texture);

    void* GetNodeFont(HScene scene, HNode node);
    Result SetNodeFont(HScene scene, HNode node, const char* font);

    BlendMode GetNodeBlendMode(HScene scene, HNode node);
    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode);

    void SetNodeXAnchor(HScene scene, HNode node, XAnchor x_anchor);
    void SetNodeYAnchor(HScene scene, HNode node, YAnchor y_anchor);
    Pivot GetNodePivot(HScene scene, HNode node);
    void SetNodePivot(HScene scene, HNode node, Pivot pivot);

    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode);

    void AnimateNode(HScene scene, HNode node,
                     Property property,
                     const Vector4& to,
                     Easing easing,
                     float duration,
                     float delay,
                     AnimationComplete animation_complete,
                     void* userdata1,
                     void* userdata2);

    void CancelAnimation(HScene scene, HNode node, Property property);

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

    /** enables/disables a node
     * Set if a node should be enabled or not. Only enabled nodes are animated and rendered.
     *
     * @note This function updates every animation acting on the node and has a complexity of O(n) over the total animation count in the scene.
     * @param scene the scene the node exists in
     * @param node the node to be enabled/disabled
     * @param enabled whether the node should be enabled
     */
    void SetNodeEnabled(HScene scene, HNode node, bool enabled);

    HScript NewScript(HContext context);
    void DeleteScript(HScript script);
    Result SetScript(HScript script, const char* source, uint32_t source_length, const char* filename);

    /** Gets the lua state used for gui scripts.
     * @return lua state
     */
    lua_State* GetLuaState(HContext context);
}

#endif
