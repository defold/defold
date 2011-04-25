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

// Workaround for evil windows #define
#ifdef DispatchMessage
#undef DispatchMessage
#endif

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

    struct NewContextParams;
    void SetDefaultNewContextParams(NewContextParams* params);

    struct NewContextParams
    {
        dmScript::HContext m_ScriptContext;
        GetURLCallback m_GetURLCallback;
        GetUserDataCallback m_GetUserDataCallback;
        ResolvePathCallback m_ResolvePathCallback;

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
        PROPERTY_POSITION  = 0,
        PROPERTY_ROTATION  = 1,
        PROPERTY_SCALE     = 2,
        PROPERTY_COLOR     = 3,
        PROPERTY_EXTENTS   = 4,

        PROPERTY_RESERVED1 = 5,
        PROPERTY_RESERVED2 = 6,
        PROPERTY_RESERVED3 = 7,

        PROPERTY_COUNT     = 8,
    };

    enum Easing
    {
        EASING_NONE = 0,
        EASING_IN = 1,
        EASING_OUT = 2,
        EASING_INOUT = 3,

        EASING_COUNT = 4,
    };

    enum BlendMode
    {
        BLEND_MODE_ALPHA     = 0,
        BLEND_MODE_ADD       = 1,
        BLEND_MODE_ADD_ALPHA = 2,
        BLEND_MODE_MULT      = 3,
    };

    enum NodeType
    {
        NODE_TYPE_BOX  = 0,
        NODE_TYPE_TEXT = 1,
    };

    struct Node
    {
        Vector4     m_Properties[PROPERTY_COUNT];
        uint32_t    m_BlendMode : 4;
        uint32_t    m_NodeType : 4;
        uint32_t    m_Reserved : 24;
        const char* m_Text;
        uint64_t    m_TextureHash;
        void*       m_Texture;
        uint64_t    m_FontHash;
        void*       m_Font;
    };

    /**
     * Container of input related information.
     */
    struct InputAction
    {
        /// Action id, hashed action name
        dmhash_t m_ActionId;
        /// Value of the input [0,1]
        float    m_Value;
        /// If the input was 0 last update
        uint16_t m_Pressed;
        /// If the input turned from above 0 to 0 this update
        uint16_t m_Released;
        /// If the input was held enough for the value to be repeated this update
        uint16_t m_Repeated;
    };

    typedef void (*RenderNode)(HScene scene,
                               const Node* nodes,
                               uint32_t node_count,
                               void* context);

    typedef void (*AnimationComplete)(HScene scene,
                                      HNode node,
                                      void* userdata1,
                                      void* userdata2);

    HContext NewContext(const NewContextParams* params);

    void DeleteContext(HContext context);

    HScene NewScene(HContext context, const NewSceneParams* params);

    void DeleteScene(HScene scene);

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
    Result DispatchMessage(HScene scene,
                           dmhash_t message_id,
                           const void* message,
                           const dmDDF::Descriptor* descriptor);

    void SetSceneUserData(HScene scene, void* user_data);

    void* GetSceneUserData(HScene scene);

    Result DispatchInput(HScene scene, const InputAction* input_actions, uint32_t input_action_count);

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

    void RenderScene(HScene scene, RenderNode render_node, void* context);

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
     * @return RESULT_OK on success
     */
    Result UpdateScene(HScene scene, float dt);

    Result SetSceneScript(HScene scene, HScript script);

    HNode NewNode(HScene scene, const Point3& position, const Vector3& extents, NodeType node_type);

    void SetNodeName(HScene scene, HNode node, const char* name);

    HNode GetNodeByName(HScene scene, const char* name);

    uint32_t GetNodeCount(HScene scene);

    void DeleteNode(HScene scene, HNode node);

    Point3 GetNodePosition(HScene scene, HNode node);

    void SetNodePosition(HScene scene, HNode node, const Point3& position);

    Vector4 GetNodeProperty(HScene scene, HNode node, Property property);

    void SetNodeProperty(HScene scene, HNode node, Property property, const Vector4& value);

    void SetNodeText(HScene scene, HNode node, const char* text);

    Result SetNodeTexture(HScene scene, HNode node, const char* texture);

    Result SetNodeFont(HScene scene, HNode node, const char* font);

    void SetNodeBlendMode(HScene scene, HNode node, BlendMode blend_mode);

    void AnimateNode(HScene scene, HNode node,
                     Property property,
                     const Vector4& to,
                     Easing easing,
                     float duration,
                     float delay,
                     AnimationComplete animation_complete,
                     void* userdata1,
                     void* userdata2);

    HScript NewScript(HContext context);
    void DeleteScript(HScript script);
    Result SetScript(HScript script, const char* source, uint32_t source_length, const char* filename);
}

#endif
