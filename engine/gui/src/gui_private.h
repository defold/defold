#ifndef DM_GUI_PRIVATE_H
#define DM_GUI_PRIVATE_H

#include <dlib/index_pool.h>
#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dlib/easing.h>
#include <dlib/image.h>

#include "gui.h"

extern "C"
{
#include "lua/lua.h"
}

namespace dmGui
{
    const uint32_t MAX_MESSAGE_DATA_SIZE = 512;
    extern const uint16_t INVALID_INDEX;

    #define GUI_SCRIPT "GuiScript"
    #define GUI_SCRIPT_INSTANCE "GuiScriptInstance"

    enum ScriptFunction
    {
        SCRIPT_FUNCTION_INIT,
        SCRIPT_FUNCTION_FINAL,
        SCRIPT_FUNCTION_UPDATE,
        SCRIPT_FUNCTION_ONMESSAGE,
        SCRIPT_FUNCTION_ONINPUT,
        SCRIPT_FUNCTION_ONRELOAD,
        MAX_SCRIPT_FUNCTION_COUNT
    };

    enum CalculateNodeTransformFlags
    {
        CALCULATE_NODE_BOUNDARY     = (1<<0),   // calculates the boundary transform (if omitted, use the render transform)
        CALCULATE_NODE_INCLUDE_SIZE = (1<<1),   // include size in the resulting transform
        CALCULATE_NODE_RESET_PIVOT  = (1<<2)    // ignore pivot in the resulting transform
    };

    struct SceneTraversalCache
    {
        SceneTraversalCache() : m_NodeIndex(0), m_Version(0) {}
        struct Data
        {
            Matrix4     m_Transform;
            float       m_Alpha;
        };
        dmArray<Data>   m_Data;
        uint16_t        m_NodeIndex;
        uint16_t        m_Version;
    };

    struct InternalClippingNode
    {
        StencilScope            m_Scope;
        StencilScope            m_ChildScope;
        uint32_t                m_VisibleRenderKey;
        uint16_t                m_ParentIndex;
        uint16_t                m_NextNonInvIndex;
        uint16_t                m_NodeIndex;
    };

    struct Context
    {
        lua_State*                      m_LuaState;
        GetURLCallback                  m_GetURLCallback;
        GetUserDataCallback             m_GetUserDataCallback;
        ResolvePathCallback             m_ResolvePathCallback;
        GetTextMetricsCallback          m_GetTextMetricsCallback;
        uint32_t                        m_PhysicalWidth;
        uint32_t                        m_PhysicalHeight;
        uint32_t                        m_DefaultProjectWidth;
        uint32_t                        m_DefaultProjectHeight;
        uint32_t                        m_Dpi;
        dmArray<HScene>                 m_Scenes;
        dmArray<RenderEntry>            m_RenderNodes;
        dmArray<Matrix4>                m_RenderTransforms;
        dmArray<Vector4>                m_RenderColors;
        dmArray<InternalClippingNode>   m_StencilClippingNodes;
        dmArray<StencilScope*>          m_StencilScopes;
        dmArray<uint16_t>               m_StencilScopeIndices;
        dmHID::HContext                 m_HidContext;
        void*                           m_DefaultFont;
        void*                           m_DisplayProfiles;
        SceneTraversalCache             m_SceneTraversalCache;
    };

    struct Node
    {
        Vector4     m_Properties[PROPERTY_COUNT];
        Vector4     m_ResetPointProperties[PROPERTY_COUNT];
        Matrix4     m_LocalTransform;
        Vector4     m_LocalScale;
        uint32_t    m_ResetPointState;

        uint32_t m_PerimeterVertices;
        PieBounds m_OuterBounds;

        union
        {
            struct
            {
                uint32_t    m_BlendMode : 4;
                uint32_t    m_NodeType : 4;
                uint32_t    m_XAnchor : 2;
                uint32_t    m_YAnchor : 2;
                uint32_t    m_Pivot : 4;
                uint32_t    m_AdjustMode : 2;
                uint32_t    m_LineBreak : 1;
                uint32_t    m_Enabled : 1; // Only enabled (1) nodes are animated and rendered
                uint32_t    m_DirtyLocal : 1;
                uint32_t    m_InheritAlpha : 1;
                uint32_t    m_ClippingMode : 2;
                uint32_t    m_ClippingVisible : 1;
                uint32_t    m_ClippingInverted : 1;
                uint32_t    m_Reserved : 6;
            };

            uint32_t m_State;
        };

        bool        m_HasResetPoint;
        const char* m_Text;

        uint64_t    m_TextureHash;
        void*       m_Texture;
        void*       m_TextureSet;

        TextureSetAnimDesc m_TextureSetAnimDesc;
        uint64_t    m_FlipbookAnimHash;
        float       m_FlipbookAnimPosition;

        uint64_t    m_FontHash;
        void*       m_Font;
        dmhash_t    m_LayerHash;
        uint16_t    m_LayerIndex;

        void**      m_NodeDescTable;
    };

    struct InternalNode
    {
        Node            m_Node;
        dmhash_t        m_NameHash;
        uint16_t        m_Version;
        uint16_t        m_Index;
        uint16_t        m_PrevIndex;
        uint16_t        m_NextIndex;
        uint16_t        m_ParentIndex;
        uint16_t        m_ChildHead;
        uint16_t        m_ChildTail;
        uint16_t        m_SceneTraversalCacheIndex;
        uint16_t        m_SceneTraversalCacheVersion;
        uint16_t        m_ClipperIndex;
        uint16_t        m_Deleted : 1; // Set to true for deferred deletion
        uint16_t        m_Padding : 15;
    };

    struct NodeProxy
    {
        HScene m_Scene;
        HNode  m_Node;
    };

    struct Animation
    {
        HNode    m_Node;
        float*   m_Value;
        float    m_From;
        float    m_To;
        float    m_Delay;
        float    m_Elapsed;
        float    m_Duration;
        dmEasing::Curve m_Easing;
        Playback m_Playback;
        AnimationComplete m_AnimationComplete;
        void*    m_Userdata1;
        void*    m_Userdata2;
        uint16_t m_FirstUpdate : 1;
        uint16_t m_AnimationCompleteCalled : 1;
        uint16_t m_Cancelled : 1;
        uint16_t m_Backwards : 1;
    };

    struct Script
    {
        int         m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
        Context*    m_Context;
        int         m_InstanceReference;
    };

    struct TextureInfo
    {
        TextureInfo(void* texture, void *textureset) : m_Texture(texture), m_TextureSet(textureset) {}
        void*   m_Texture;
        void*   m_TextureSet;
    };

    struct DynamicTexture
    {
        DynamicTexture(void* handle)
        {
            memset(this, 0, sizeof(*this));
            m_Handle = handle;
            m_Type = (dmImage::Type) -1;
        }
        void*           m_Handle;
        uint32_t        m_Created : 1;
        uint32_t        m_Deleted : 1;
        uint32_t        m_Width;
        uint32_t        m_Height;
        void*           m_Buffer;
        dmImage::Type   m_Type;
    };

    struct Scene
    {
        int                     m_InstanceReference;
        int                     m_DataReference;
        Context*                m_Context;
        Script*                 m_Script;
        dmIndexPool16           m_NodePool;
        dmArray<InternalNode>   m_Nodes;
        dmArray<Animation>      m_Animations;
        dmHashTable64<void*>    m_Fonts;
        dmHashTable64<TextureInfo>    m_Textures;
        dmHashTable64<DynamicTexture> m_DynamicTextures;
        void*                   m_Material;
        dmHashTable64<uint16_t> m_Layers;
        dmArray<dmhash_t>       m_Layouts;
        dmArray<void*>          m_LayoutsNodeDescs;
        dmhash_t                m_LayoutId;
        AdjustReference         m_AdjustReference;
        dmArray<dmhash_t>       m_DeletedDynamicTextures;
        void*                   m_DefaultFont;
        void*                   m_UserData;
        uint16_t                m_RenderHead;
        uint16_t                m_RenderTail;
        uint16_t                m_NextVersionNumber;
        uint16_t                m_RenderOrder; // For the render-key
        uint16_t                m_NextLayerIndex;
        uint16_t                m_NextLayoutIndex;
        uint16_t                m_ResChanged : 1;
        uint32_t                m_Width;
        uint32_t                m_Height;
        FetchTextureSetAnimCallback m_FetchTextureSetAnimCallback;
        OnWindowResizeCallback   m_OnWindowResizeCallback;
    };

    InternalNode* GetNode(HScene scene, HNode node);

    /** calculates the transform of a node
     * A boundary transform maps the local rectangle (0,1),(0,1) to screen space such that it inclusively encapsulates the node boundaries in screen space.
     * Box nodes are rendered in boundary space (quad with dimensions (0,1),(0,1)), so the same transform is calculated whether or not the CALCULATE_NODE_BOUNDARY flag is set.
     * Text nodes are rendered in a transform where the origin is located at the left edge of the base line, excluding the text size, since it's implicitly spanned by glyph quads.
     * Their boundary transform is analogue to the box boundary transform.
     * This is complicated and could be simplified by supporting different pivots when rendering text.
     *
     * @param scene scene of the node
     * @param node node for which to calculate the transform
     * @param reference_scale the reference scale of the scene which is the ratio between physical and reference dimensions
     * @param flags CalculateNodeTransformFlags enumerated behaviour flags
     * @param out_transform out-parameter to write the calculated transform to
     */
    void CalculateNodeTransform(HScene scene, InternalNode* node, const CalculateNodeTransformFlags flags, Matrix4& out_transform);

    /** calculates the reference scale for a node
     * The reference scale is defined as scaling from the predefined screen space to the actual screen space.
     *
     * @param scene scene for which the node exists in
     * @param node node for which the reference scale should be calculated
     * @return a scaling vector (ref_scale, ref_scale, 1, 1)
     */
    Vector4 CalculateReferenceScale(HScene scene, InternalNode* node);

    HNode GetNodeHandle(InternalNode* node);
}

#endif
