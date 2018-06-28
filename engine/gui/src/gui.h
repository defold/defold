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
#include <particle/particle.h>
#include <rig/rig.h>

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

    /**
     * Default layout id
     */
    const dmhash_t DEFAULT_LAYOUT = dmHashString64("");

    /**
     * Animation
     */
    enum Playback
    {
        PLAYBACK_ONCE_FORWARD  = 0,
        PLAYBACK_ONCE_BACKWARD = 1,
        PLAYBACK_ONCE_PINGPONG = 2,
        PLAYBACK_LOOP_FORWARD  = 3,
        PLAYBACK_LOOP_BACKWARD = 4,
        PLAYBACK_LOOP_PINGPONG = 5,
        PLAYBACK_NONE = 6,
        PLAYBACK_COUNT = 7,
    };

    enum AdjustReference
    {
        ADJUST_REFERENCE_LEGACY   = 0,
        ADJUST_REFERENCE_PARENT   = 1,
        ADJUST_REFERENCE_DISABLED = 2
    };

    /**
     * Textureset animation
     */
    struct TextureSetAnimDesc
    {
        inline void Init()
        {
            m_State = 0;
            m_Playback = PLAYBACK_ONCE_FORWARD;
            m_TexCoords = 0;
            m_FlipHorizontal = 0;
            m_FlipVertical = 0;
        }

        union
        {
            struct
            {
                uint64_t m_Start : 13;
                uint64_t m_End : 13;
                uint64_t m_TextureWidth : 13;
                uint64_t m_TextureHeight : 13;
                uint64_t m_FPS : 8;
                uint64_t m_Playback : 4;
            };
            uint64_t m_State;
        };

        const float* m_TexCoords;
        uint32_t m_FlipHorizontal : 1;
        uint32_t m_FlipVertical : 1;
        uint32_t m_Padding : 14;
    };

    enum FetchTextureSetAnimResult
    {
        FETCH_ANIMATION_OK = 0,
        FETCH_ANIMATION_NOT_FOUND = -1,
        FETCH_ANIMATION_CALLBACK_ERROR = -2,
        FETCH_ANIMATION_INVALID_PLAYBACK = -3,
        FETCH_ANIMATION_UNKNOWN_ERROR = -1000
    };

    /**
     * Callback to fetch textureset animation info from a textureset source
     */
    typedef FetchTextureSetAnimResult (*FetchTextureSetAnimCallback)(void* texture_set_ptr, dmhash_t anim, TextureSetAnimDesc* out_data);

    struct RigSceneDataDesc
    {
        dmArray<dmRig::RigBone>* m_BindPose;
        dmRigDDF::Skeleton*      m_Skeleton;
        dmRigDDF::MeshSet*       m_MeshSet;
        dmRigDDF::AnimationSet*  m_AnimationSet;
        const dmArray<uint32_t>* m_PoseIdxToInfluence;
        const dmArray<uint32_t>* m_TrackIdxToPose;
        void*                    m_Texture;
        void*                    m_TextureSet;
    };

    typedef bool (*FetchRigSceneDataCallback)(void* spine_scene, dmhash_t rig_scene_id, RigSceneDataDesc* out_data);


    /**
     * Callback to set node from node descriptor
     */
    typedef void (*SetNodeCallback)(const HScene scene, HNode node, const void *node_desc);

    /**
     * Callback when display window size changes
     */
    typedef void (*OnWindowResizeCallback)(const HScene scene, uint32_t width, uint32_t height);

    /**
     * Callback for rig events
     */
    typedef void (*RigEventDataCallback)(HScene scene,
                                      void* node_ref,
                                      void* event_data);

    /**
     * Scene creation
     */
    struct NewSceneParams;
    void SetDefaultNewSceneParams(NewSceneParams* params);

    struct NewSceneParams
    {
        uint32_t m_MaxNodes;
        uint32_t m_MaxAnimations;
        uint32_t m_MaxTextures;
        uint32_t m_MaxFonts;
        uint32_t m_MaxSpineScenes;
        uint32_t m_MaxParticlefxs;
        uint32_t m_MaxParticlefx;
        uint32_t m_MaxLayers;
        dmRig::HRigContext m_RigContext;
        dmParticle::HParticleContext m_ParticlefxContext;
        void*    m_UserData;
        FetchTextureSetAnimCallback m_FetchTextureSetAnimCallback;
        FetchRigSceneDataCallback m_FetchRigSceneDataCallback;
        RigEventDataCallback m_RigEventDataCallback;
        OnWindowResizeCallback m_OnWindowResizeCallback;
        AdjustReference m_AdjustReference;
        dmScript::ScriptWorld* m_ScriptWorld;

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
        /// Total string height
        float m_Height;
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
     * @param tracking letter spacing
     * @param out_metrics the metrics of the supplied text
     */
    typedef void (*GetTextMetricsCallback)(const void* font, const char* text, float width, bool line_break, float leading, float tracking, TextMetrics* out_metrics);

    /**
     * Stencil clipping render state
     */
    struct StencilScope
    {
        /// Stencil reference value
        uint8_t     m_RefVal;
        /// Stencil test mask
        uint8_t     m_TestMask;
        /// Stencil write mask
        uint8_t     m_WriteMask;
        /// Color mask (R,G,B,A)
        uint8_t     m_ColorMask : 4;
        uint8_t     m_Padding : 4;
    };

    struct Scope {
        Scope(int layer, int index) : m_Index(1), m_RootLayer(layer), m_RootIndex(index) {}

        uint16_t m_Index;
        uint16_t m_RootLayer;
        uint16_t m_RootIndex;
    };

    struct NewContextParams;
    void SetDefaultNewContextParams(NewContextParams* params);

    struct NewContextParams
    {
        dmScript::HContext      m_ScriptContext;
        GetURLCallback          m_GetURLCallback;
        GetUserDataCallback     m_GetUserDataCallback;
        ResolvePathCallback     m_ResolvePathCallback;
        GetTextMetricsCallback  m_GetTextMetricsCallback;
        uint32_t                m_PhysicalWidth;
        uint32_t                m_PhysicalHeight;
        uint32_t                m_DefaultProjectWidth;
        uint32_t                m_DefaultProjectHeight;
        uint32_t                m_Dpi;
        dmHID::HContext         m_HidContext;
        dmResource::HFactory    m_Factory;

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
        RESULT_DATA_ERROR = -9,
        RESULT_WRONG_TYPE = -10,
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
        PROPERTY_SLICE9     = 7,
        PROPERTY_PIE_PARAMS = 8,
        PROPERTY_TEXT_PARAMS= 9,

        PROPERTY_COUNT      = 10,
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
    enum ClippingMode
    {
        CLIPPING_MODE_NONE    = 0,
        CLIPPING_MODE_STENCIL = 2,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum NodeType
    {
        NODE_TYPE_BOX  = 0,
        NODE_TYPE_TEXT = 1,
        NODE_TYPE_PIE  = 2,
        NODE_TYPE_TEMPLATE = 3,
        NODE_TYPE_SPINE = 4,
        NODE_TYPE_PARTICLEFX = 5,
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

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum SizeMode
    {
        SIZE_MODE_MANUAL    = 0,
        SIZE_MODE_AUTO      = 1,
    };

    // NOTE: These enum values are duplicated in scene desc in gamesys (gui_ddf.proto)
    // Don't forget to change gui_ddf.proto if you change here
    enum PieBounds
    {
        PIEBOUNDS_RECTANGLE = 0,
        PIEBOUNDS_ELLIPSE   = 1,
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
        uint32_t m_GamepadIndex;
        uint16_t m_IsGamepad : 1;
        uint16_t m_HasConnectivity : 1;
        uint16_t m_Connected : 1;
        uint16_t m_HasText : 1;
        /// If the input was 0 last update
        uint16_t m_Pressed : 1;
        /// If the input turned from above 0 to 0 this update
        uint16_t m_Released : 1;
        /// If the input was held enough for the value to be repeated this update
        uint16_t m_Repeated : 1;
        /// If the position fields (m_X, m_Y, m_DX, m_DY) are set and valid to read
        uint16_t m_PositionSet : 1;
    };

    struct RenderEntry {
        RenderEntry()
        {
            memset(this, 0x0, sizeof(*this));
        }
        uint64_t m_RenderKey;
        HNode m_Node;
        void* m_RenderData;
    };

    /**
     * Render nodes callback
     * @param scene
     * @param nodes
     * @param node_transforms
     * @param node_opacities
     * @param node_count
     * @param stencil_clipping_render_states
     * @param context
     */
    typedef void (*RenderNodes)(HScene scene,
                               const RenderEntry* node_entries,
                               const Vectormath::Aos::Matrix4* node_transforms,
                               const float* node_opacities,
                               const StencilScope** node_stencils,
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
                                      bool finished,
                                      void* userdata1,
                                      void* userdata2);

    HContext NewContext(const NewContextParams* params);

    void DeleteContext(HContext context, dmScript::HContext script_context);

    void SetPhysicalResolution(HContext context, uint32_t width, uint32_t height);

    void GetPhysicalResolution(HContext context, uint32_t& width, uint32_t& height);

    uint32_t GetDisplayDpi(HContext context);

    void GetDefaultResolution(HContext context, uint32_t& width, uint32_t& height);

    void SetDefaultResolution(HContext context, uint32_t width, uint32_t height);

    void SetDisplayProfiles(HContext context, void* display_profiles);

    void SetDefaultFont(HContext context, void* font);

    void SetSceneAdjustReference(HScene scene, AdjustReference adjust_reference);

    HScene NewScene(HContext context, const NewSceneParams* params);

    void DeleteScene(HScene scene);

    void SetSceneUserData(HScene scene, void* user_data);

    void* GetSceneUserData(HScene scene);

    void SetSceneResolution(HScene, uint32_t width, uint32_t height);

    void GetSceneResolution(HScene, uint32_t &width, uint32_t &height);

    void GetPhysicalResolution(HScene scene, uint32_t& width, uint32_t& height);

    uint32_t GetDisplayDpi(HScene scene);

    void* GetDisplayProfiles(HScene scene);

    AdjustReference GetSceneAdjustReference(HScene scene);

    dmRig::HRigContext GetRigContext(HScene scene);

    /**
     * Adds a texture and optional textureset with the specified name to the scene.
     * @note Any nodes connected to the same texture_name will also be connected to the new texture/textureset. This makes this function O(n), where n is #nodes.
     * @param scene Scene to add the texture/textureset to
     * @param texture_name Name of the texture that will be used in the gui scripts
     * @param texture The texture to add
     * @param textureset The textureset to add if animation is used, otherwise zero. If set, texture parameter is expected to be equal to textureset texture.
     * @param width With of the texture
     * @param height Height of the texture
     * @return Outcome of the operation
     */
    Result AddTexture(HScene scene, const char* texture_name, void* texture, void* textureset, uint32_t width, uint32_t height);

    /**
     * Removes a texture with the specified name from the scene.
     * @note Any nodes connected to the same texture will also be disconnected from the texture. This makes this function O(n), where n is #nodes.
     * @param scene Scene to remove texture from
     * @param texture_name Name of the texture as it is used by the gui scripts
     */
    void RemoveTexture(HScene scene, const char* texture_name);

    /**
     * Remove all textures from the scene.
     * @note Every node will also be disconnected from the texture they are already connected to, if any. This makes this function O(n), where n is #nodes.
     * @param scene Scene to clear from textures
     */
    void ClearTextures(HScene scene);

    /**
     * Create a new dynamic texture
     * @param scene
     * @param texture_hash
     * @param width
     * @param height
     * @param type
     * @param flip
     * @param buffer
     * @param buffer_size
     * @return
     */
    Result NewDynamicTexture(HScene scene, const dmhash_t texture_hash, uint32_t width, uint32_t height, dmImage::Type type, bool flip, const void* buffer, uint32_t buffer_size);

    /**
     * Delete dynamic texture
     * @param scene
     * @param texture_hash
     * @return
     */
    Result DeleteDynamicTexture(HScene scene, const dmhash_t texture_hash);

    /**
     * Update dynamic texture
     * @param scene
     * @param texture_hash
     * @param width
     * @param height
     * @param type
     * @param flip
     * @param buffer
     * @param buffer_size
     * @return
     */
    Result SetDynamicTextureData(HScene scene, const dmhash_t texture_hash, uint32_t width, uint32_t height, dmImage::Type type, bool flip, const void* buffer, uint32_t buffer_size);

    /**
     * Get texture data for a dynamic texture
     * @param scene
     * @param texture_hash
     * @param out_width
     * @param out_height
     * @param out_type
     * @param out_buffer
     * @return
     */
    Result GetDynamicTextureData(HScene scene, const dmhash_t texture_hash, uint32_t* out_width, uint32_t* out_height, dmImage::Type* out_type, const void** out_buffer);

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
     * Adds a particlefx with the specified name to the scene.
     * @note Any nodes connected to the same particlefx_name will also be connected to the new particlefx. This makes this function O(n), where n is #nodes.
     * @param scene Scene to add particlefx to
     * @param particlefx_name Name of the particlefx that will be used in the gui scripts
     * @param particlefx_prototype The particlefx to add
     * @return Outcome of the operation
     */
    Result AddParticlefx(HScene scene, const char* particlefx_name, void* particlefx_prototype);

    /**
     * Removes a particlefx with the specified name from the scene.
     * @note Any nodes connected to the same particlefx_name will also be disconnected from the particlefx. This makes this function O(n), where n is #nodes.
     * @param scene Scene to remove particlefx from
     * @param particlefx_name Name of the particlefx that will be used in the gui scripts
     */
    void RemoveParticlefx(HScene scene, const char* particlefx_name);
    
    /**
     * Adds a spine scene with the specified name to the scene.
     * @note Any nodes connected to the same spine_scene_name will also be connected to the new spine scene. This makes this function O(n), where n is #nodes.
     * @param scene Scene to add spine scene to
     * @param spine_scene_name Name of the spine scene that will be used in the gui scripts
     * @param spine_scene The spine scene to add
     * @return Outcome of the operation
     */
    Result AddSpineScene(HScene scene, const char* spine_scene_name, void* spine_scene);

    /**
     * Removes a spine scene with the specified name from the scene.
     * @note Any nodes connected to the same spine_scene_name will also be disconnected from the spine scene. This makes this function O(n), where n is #nodes.
     * @param scene Scene to remove spine scene from
     * @param spine_scene_name Name of the spine scene that will be used in the gui scripts
     */
    void RemoveSpineScene(HScene scene, const char* spine_scene_name);

    /**
     * Set scene material
     * @param scene
     * @param material
     */
    void SetMaterial(HScene scene, void* material);

    /**
     * Get scene material
     * @param scene
     */
    void* GetMaterial(HScene scene);

    /**
     * Adds a layer with the specified name to the scene.
     * @param scene Scene to add the layer to
     * @param layer_name Name of the layer that will be used in the gui scripts
     * @return Outcome of the operation
     */
    Result AddLayer(HScene scene, const char* layer_name);

    /**
     * Allocates memory needed to hold nr of layers with nr of nodes for the scene.
     * @param scene Scene to allocate for
     * @param node_count Number of nodes in the scene (not including dynamic nodes)
     * @param layouts_count Number of layouts in the scene
     */
    void AllocateLayouts(HScene scene, size_t node_count, size_t layouts_count);

    /**
     * Free layout resources and reset to scene contain only the system default layout
     * @param scene Scene to reset layouts for
     */
    void ClearLayouts(HScene scene);

    /**
     * Adds a layout with id to the scene.
     *
     * @note dmGui::AllocateLayouts must be called prior to calling this function
     * @param scene Scene to add the layout to
     * @param layout_id Unique id of the layout. If the id exists, it will be replaced
     * @return Outcome of the operation
     */
    Result AddLayout(HScene scene, const char* layout_id);

    /**
     * Get current layout id.
     * @param scene Scene of which to get layout
     * @return layout_id id of the layout
     */
    dmhash_t GetLayout(const HScene scene);

    /**
     * Get number of layouts.
     * @param scene Scene of which to get layout count
     * @return number of layouts
     */
    uint16_t GetLayoutCount(const HScene scene);

    /**
     * Get hashed id of layout with index.
     * @param scene Scene of which to get layout id
     * @param layout_index index of layout. Must be smaller than layout count.
     * @param layout_id reference to dmhash_t to receive the id
     * @return Outcome of the operation
     */
    Result GetLayoutId(const HScene scene, uint16_t layout_index, dmhash_t& layout_id_out);

    /**
     * Get index of layer with id in a scene.
     * @param scene Scene to get the layer id from
     * @param layout_id hashed id of layout to get index of
     * @return index, or 0 if not found refering the the default layout.
     */
    uint16_t GetLayoutIndex(const HScene scene, dmhash_t layout_id);

    /**
     * Set the desc for node using layouts with index in range start to end.
     * @note dmGui::AllocateLayouts must be called prior to calling this function
     * @param scene Scene
     * @param node Node to set desc to
     * @param desc Desc
     * @param layout_index_start first index to set in range
     * @param layout_index_end last index to set in range
     * @return Outcome of the operation
     */
    Result SetNodeLayoutDesc(const HScene scene, HNode node, const void *desc, uint16_t layout_index_start, uint16_t layout_index_end);

    /**
     * Set a layout with id.
     * @param scene Scene of which to set layout
     * @param layout_id id of the layout.
     * @param set_node_callback Callback function that will set node from node descriptor
     * @return Outcome of the operation
     */
    Result SetLayout(const HScene scene, dmhash_t layout_id, SetNodeCallback set_node_callback);

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

        RenderNodes                 m_RenderNodes;
        NewTexture                  m_NewTexture;
        DeleteTexture               m_DeleteTexture;
        SetTextureData              m_SetTextureData;
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
    uint32_t GetParticlefxCount(HScene scene);

    void DeleteNode(HScene scene, HNode node, bool delete_headless_pfx);

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

    Vector4 GetNodeSlice9(HScene scene, HNode node);
    Point3 GetNodeSize(HScene scene, HNode node);

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
    void SetNodeTextLeading(HScene scene, HNode node, float leading);
    float GetNodeTextLeading(HScene scene, HNode node);
    void SetNodeTextTracking(HScene scene, HNode node, float tracking);
    float GetNodeTextTracking(HScene scene, HNode node);

    void* GetNodeTexture(HScene scene, HNode node);
    dmhash_t GetNodeTextureId(HScene scene, HNode node);
    Result SetNodeTexture(HScene scene, HNode node, dmhash_t texture_id);
    Result SetNodeTexture(HScene scene, HNode node, const char* texture_id);
    dmhash_t GetNodeSpineSceneId(HScene scene, HNode node);
    Result SetNodeSpineScene(HScene scene, HNode node, dmhash_t spine_scene_id, dmhash_t skin_id, dmhash_t default_animation_id, bool generate_bones);
    Result SetNodeSpineScene(HScene scene, HNode node, const char* spine_scene_id, dmhash_t skin_id, dmhash_t default_animation_id, bool generate_bones);
    dmhash_t GetNodeSpineScene(HScene scene, HNode node);
    Result SetNodeSpineSkin(HScene scene, HNode node, dmhash_t skin_id);
    dmhash_t GetNodeSpineSkin(HScene scene, HNode node);
    Result SetNodeSpineSkinSlot(HScene scene, HNode node, dmhash_t skin_id, dmhash_t slot_id);
    dmRig::HRigInstance GetNodeRigInstance(HScene scene, HNode node);
    HNode GetNodeSpineBone(HScene scene, HNode node, dmhash_t bone_id);

    Result SetNodeParticlefx(HScene scene, HNode node, dmhash_t particlefx_id);
    Result GetNodeParticlefx(HScene scene, HNode node, dmhash_t& particlefx_id);

    Result PlayNodeFlipbookAnim(HScene scene, HNode node, dmhash_t anim, AnimationComplete anim_complete_callback = 0x0, void* callback_userdata1 = 0x0, void* callback_userdata2 = 0x0);
    Result PlayNodeFlipbookAnim(HScene scene, HNode node, const char* anim, AnimationComplete anim_complete_callback = 0x0, void* callback_userdata1 = 0x0, void* callback_userdata2 = 0x0);
    void CancelNodeFlipbookAnim(HScene scene, HNode node);
    dmhash_t GetNodeFlipbookAnimId(HScene scene, HNode node);
    const float* GetNodeFlipbookAnimUV(HScene scene, HNode node);
    void GetNodeFlipbookAnimUVFlip(HScene scene, HNode node, bool& flip_horizontal, bool& flip_vertical);

    void* GetNodeFont(HScene scene, HNode node);
    dmhash_t GetNodeFontId(HScene scene, HNode node);
    Result SetNodeFont(HScene scene, HNode node, dmhash_t font_id);
    Result SetNodeFont(HScene scene, HNode node, const char* font_id);

    dmhash_t GetNodeLayerId(HScene scene, HNode node);
    Result SetNodeLayer(HScene scene, HNode node, dmhash_t layer_id);
    Result SetNodeLayer(HScene scene, HNode node, const char* layer_id);

    bool GetNodeInheritAlpha(HScene scene, HNode node);
    void SetNodeInheritAlpha(HScene scene, HNode node, bool inherit_alpha);

    Result SetNodeSpineCursor(HScene scene, HNode node, float cursor);
    float GetNodeSpineCursor(HScene scene, HNode node);
    Result SetNodeSpinePlaybackRate(HScene scene, HNode node, float playback_rate);
    float GetNodeSpinePlaybackRate(HScene scene, HNode node);
    dmhash_t GetNodeSpineAnimation(HScene scene, HNode node);
    Result PlayNodeSpineAnim(HScene scene, HNode node, dmhash_t animation_id, Playback playback, float blend, float offset, float playback_rate, AnimationComplete animation_complete, void* userdata1, void* userdata2);
    Result CancelNodeSpineAnim(HScene scene, HNode node);

    Result PlayNodeParticlefx(HScene scene, HNode node, dmParticle::EmitterStateChangedData* data);
    Result StopNodeParticlefx(HScene scene, HNode node);
    Result SetNodeParticlefxConstant(HScene scene, HNode node, dmhash_t emitter_id, dmhash_t constant_id, Vector4& value);
    Result ResetNodeParticlefxConstant(HScene scene, HNode node, dmhash_t emitter_id, dmhash_t constant_id);

    /**
     * Set node clipping mode
     * @param scene scene
     * @param node node
     * @param mode ClippingMode enumerated type
     */
    void SetNodeClippingMode(HScene scene, HNode node, ClippingMode mode);

    /**
     * Get node clipping state.
     * @param scene scene
     * @param node node
     * @return mode
     */
    ClippingMode GetNodeClippingMode(HScene scene, HNode node);

    /**
     * Set node clipping visibilty
     * @param scene scene
     * @param node node
     * @param visible true to show clipping node, false for invisible clipper
     */
    void SetNodeClippingVisible(HScene scene, HNode node, bool visible);

    /**
     * Get node clipping visibilty
     * @param scene scene
     * @param node node
     * @return true if clipping node visible, false for invisible clipper
     */
    bool GetNodeClippingVisible(HScene scene, HNode node);

    /**
     * Set node clipping inverted
     * @param scene scene
     * @param node node
     * @param inverted true to invert clipping node (exlusive), false for default (inclusive) clipping
     */
    void SetNodeClippingInverted(HScene scene, HNode node, bool inverted);

    /**
     * Get node clipping inverted setting
     * @param scene scene
     * @param node node
     * @return true if clipping is inverted (exclusive), false for default (inclusive) clipping
     */
    bool GetNodeClippingInverted(HScene scene, HNode node);

    Result GetTextMetrics(HScene scene, const char* text, const char* font_id, float width, bool line_break, float leading, float tracking, TextMetrics* metrics);
    Result GetTextMetrics(HScene scene, const char* text, dmhash_t font_id, float width, bool line_break, float leading, float tracking, TextMetrics* metrics);

    BlendMode GetNodeBlendMode(HScene scene, HNode node);
    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode);

    XAnchor GetNodeXAnchor(HScene scene, HNode node);
    void SetNodeXAnchor(HScene scene, HNode node, XAnchor x_anchor);
    YAnchor GetNodeYAnchor(HScene scene, HNode node);
    void SetNodeYAnchor(HScene scene, HNode node, YAnchor y_anchor);
    Pivot GetNodePivot(HScene scene, HNode node);
    void SetNodePivot(HScene scene, HNode node, Pivot pivot);
    bool GetNodeIsBone(HScene scene, HNode node);
    void SetNodeIsBone(HScene scene, HNode node, bool is_bone);

    void SetNodeAdjustMode(HScene scene, HNode node, AdjustMode adjust_mode);

    void SetNodeSizeMode(HScene scene, HNode node, SizeMode size_mode);
    SizeMode GetNodeSizeMode(HScene scene, HNode node);

    void SetNodeInnerRadius(HScene scene, HNode node, float radius);
    void SetNodeOuterBounds(HScene scene, HNode node, PieBounds bounds);
    void SetNodePieFillAngle(HScene scene, HNode node, float fill_angle);
    void SetNodePerimeterVertices(HScene scene, HNode node, uint32_t vertices);

    float GetNodeInnerRadius(HScene scene, HNode node);
    PieBounds GetNodeOuterBounds(HScene scene, HNode node);
    float GetNodePieFillAngle(HScene scene, HNode node);
    uint32_t GetNodePerimeterVertices(HScene scene, HNode node);

    /**
     * Convenience method, converting a dmGui::Property value into a hash value suitable for use
     * in animation.
     *
     * @param property - the property value to be translated must be within the range PROPERTY_POSITION, PROPERTY_SHADOW
     * @return a hash value for valid properties or zero otherwise.
     */
    dmhash_t GetPropertyHash(Property property);

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
                         dmEasing::Curve easing,
                         Playback playback,
                         float duration,
                         float delay,
                         AnimationComplete animation_complete,
                         void* userdata1,
                         void* userdata2);

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
    Result SetScript(HScript script, dmLuaDDF::LuaSource *source);

    /** Gets the lua state used for gui scripts.
     * @return lua state
     */
    lua_State* GetLuaState(HContext context);

    /** Gets reference to the lua reference table used for gui scripts.
     * @return lua reference table reference
     */
    int GetContextTableRef(HScene scene);

    /** Gets the gui scene currently connected to the lua state.
     * A scene is connected while any of the callbacks in the associated gui script is being run.
     * @param L lua state
     * @return current scene, or 0
     */
    HScene GetSceneFromLua(lua_State* L);
}

#endif
