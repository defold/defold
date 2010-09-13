#ifndef DM_GUI_H
#define DM_GUI_H

#include <stdint.h>

#include <vectormath/cpp/vectormath_aos.h>
using namespace Vectormath::Aos;

namespace dmGui
{
    /*
     * TODO:
     * Delete nodes from lua
     * Input/Events
     * Layers/Draw order
     * Timers in lua
     * More demos
     * Pivot point for rotation
     */

    typedef struct Gui* HGui;
    typedef struct Scene* HScene;
    typedef uint32_t HNode;

    struct NewSceneParams;
    void SetDefaultNewSceneParams(NewSceneParams* params);

    struct NewSceneParams
    {
        uint32_t m_MaxNodes;
        uint32_t m_MaxAnimations;
        uint32_t m_MaxTextures;
        uint32_t m_MaxFonts;

        NewSceneParams()
        {
            SetDefaultNewSceneParams(this);
        }
    };

    enum Result
    {
        RESULT_OK = 0,
        RESULT_SYNTAX_ERROR = -1,
        RESULT_SCRIPT_ERROR = -2,
        RESULT_MISSING_UPDATE_FUNCTION_ERROR = -3,
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
        void*       m_Texture;
        void*       m_Font;
    };

    typedef void (*RenderNode)(HScene scene,
                               const Node* nodes,
                               uint32_t node_count,
                               void* context);

    typedef void (*AnimationComplete)(HScene scene,
                                      HNode node,
                                      void* userdata1,
                                      void* userdata2);

    HGui New();

    void Delete(HGui gui);

    HScene NewScene(HGui gui, const NewSceneParams* params);

    void DeleteScene(HScene scene);

    Result AddTexture(HScene scene, const char* texture_name, void* texture);

    Result AddFont(HScene scene, const char* font_name, void* font);

    void RenderScene(HScene scene, RenderNode render_node, void* context);

    Result UpdateScene(HScene scene, float dt);

    Result SetSceneScript(HScene scene, const char* script, uint32_t script_length);

    HNode NewNode(HScene scene, const Point3& position, const Vector3& extents, NodeType node_type);

    void SetNodeName(HScene scene, HNode node, const char* name);

    HNode GetNodeByName(HScene scene, const char* name);

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
}

#endif
