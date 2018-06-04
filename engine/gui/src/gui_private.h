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
            float     	m_Opacity;
        };
        dmArray<Data>   m_Data;
        uint16_t        m_NodeIndex;
        uint16_t        m_Version;
    };

    struct InternalClippingNode
    {
        StencilScope            m_Scope;
        StencilScope            m_ChildScope;
        uint64_t                m_VisibleRenderKey;
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
        dmArray<float>                	m_RenderOpacities;
        dmArray<InternalClippingNode>   m_StencilClippingNodes;
        dmArray<StencilScope*>          m_StencilScopes;
        dmArray<uint16_t>               m_StencilScopeIndices;
        dmArray<HNode>                  m_ScratchBoneNodes;
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
        Vector4     m_LocalAdjustScale;
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
                uint32_t    m_SizeMode : 1;
                uint32_t    m_LineBreak : 1;
                uint32_t    m_Enabled : 1; // Only enabled (1) nodes are animated and rendered
                uint32_t    m_DirtyLocal : 1;
                uint32_t    m_InheritAlpha : 1;
                uint32_t    m_ClippingMode : 2;
                uint32_t    m_ClippingVisible : 1;
                uint32_t    m_ClippingInverted : 1;
                uint32_t    m_IsBone : 1;
                uint32_t    m_HasHeadlessPfx : 1;
                uint32_t    m_Reserved : 3;
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

        uint64_t            m_SpineSceneHash;
        void*               m_SpineScene;
        dmRig::HRigInstance m_RigInstance;

        uint64_t                m_ParticlefxHash;
        void*                   m_ParticlefxPrototype;
        dmParticle::HInstance   m_ParticleInstance;
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

    struct SpineAnimation
    {
        HNode    m_Node;
        AnimationComplete m_AnimationComplete;
        RigEventDataCallback m_EventDataCallback;
        void*    m_Userdata1;
        void*    m_Userdata2;
    };

    struct Script
    {
        int         m_FunctionReferences[MAX_SCRIPT_FUNCTION_COUNT];
        Context*    m_Context;
        int         m_InstanceReference;
    };

    struct TextureInfo
    {
        TextureInfo(void* texture, void *textureset, uint32_t width, uint32_t height) : m_Texture(texture), m_TextureSet(textureset), m_Width(width), m_Height(height) {}
        void*   m_Texture;
        void*   m_TextureSet;
        uint32_t m_Width : 16;
        uint32_t m_Height : 16;
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

    struct ParticlefxComponent
    {
        ParticlefxComponent()
        {
            memset(this, 0, sizeof(*this));
        }
        dmParticle::HInstance   m_Instance;
        dmParticle::HPrototype  m_Prototype;
        HNode                   m_Node;
    };

    struct Scene
    {
        int                     m_InstanceReference;
        int                     m_DataReference;
        int                     m_ContextTableReference;
        Context*                m_Context;
        Script*                 m_Script;
        dmIndexPool16           m_NodePool;
        dmArray<InternalNode>   m_Nodes;
        dmArray<Animation>      m_Animations;
        dmArray<SpineAnimation> m_SpineAnimations;
        dmHashTable64<void*>    m_Fonts;
        dmHashTable64<TextureInfo>    m_Textures;
        dmHashTable64<DynamicTexture> m_DynamicTextures;
        dmRig::HRigContext      m_RigContext;
        dmHashTable64<void*>    m_SpineScenes;
        dmParticle::HParticleContext m_ParticlefxContext;
        dmHashTable64<dmParticle::HPrototype>    m_Particlefxs;
        dmArray<ParticlefxComponent> m_AliveParticlefxs;
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
        dmScript::ScriptWorld*  m_ScriptWorld;
        FetchTextureSetAnimCallback m_FetchTextureSetAnimCallback;
        FetchRigSceneDataCallback m_FetchRigSceneDataCallback;
        RigEventDataCallback    m_RigEventDataCallback;
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

    /** Updates the local transform of the node. Requires the parent node(s) to have been updated beforehand.
     * @param scene scene of the node
     * @param node node for which to calculate the transform
     */
    void UpdateLocalTransform(HScene scene, InternalNode* node);

    /**
     */
    inline Vector4 CalcPivotDelta(uint32_t pivot, Vector4 size)
    {
        float width = size.getX();
        float height = size.getY();

        Vector4 delta_pivot = Vector4(0.0f, 0.0f, 0.0f, 1.0f);

        switch (pivot)
        {
            case dmGui::PIVOT_CENTER:
            case dmGui::PIVOT_S:
            case dmGui::PIVOT_N:
                delta_pivot.setX(-width * 0.5f);
                break;

            case dmGui::PIVOT_NE:
            case dmGui::PIVOT_E:
            case dmGui::PIVOT_SE:
                delta_pivot.setX(-width);
                break;

            case dmGui::PIVOT_SW:
            case dmGui::PIVOT_W:
            case dmGui::PIVOT_NW:
                break;
        }
        switch (pivot) {
            case dmGui::PIVOT_CENTER:
            case dmGui::PIVOT_E:
            case dmGui::PIVOT_W:
                delta_pivot.setY(-height * 0.5f);
                break;

            case dmGui::PIVOT_N:
            case dmGui::PIVOT_NE:
            case dmGui::PIVOT_NW:
                delta_pivot.setY(-height);
                break;

            case dmGui::PIVOT_S:
            case dmGui::PIVOT_SW:
            case dmGui::PIVOT_SE:
                break;
        }
        return delta_pivot;
    }

    /** Calculates a transform with the pivot and the scale set
     *
     * @param scene scene of the node
     * @param node node for which to calculate the transform
     * @param flags CalculateNodeTransformFlags enumerated behaviour flags
     * @param transform [out] out-parameter to write the calculated transform to
     */
    inline void CalculateNodeExtents(const Node& node, const CalculateNodeTransformFlags flags, Matrix4& transform)
    {
        Vector4 size(1.0f, 1.0f, 0.0f, 0.0f);
        if (flags & CALCULATE_NODE_INCLUDE_SIZE)
        {
            size = node.m_Properties[dmGui::PROPERTY_SIZE];
        }
        // Reset the pivot of the node, so that the resulting transform has the origin in the lower left, which is used for quad rendering etc.
        if (flags & CALCULATE_NODE_RESET_PIVOT)
        {
            Vector4 pivot_delta = transform * CalcPivotDelta(node.m_Pivot, size);
            transform.setCol3(pivot_delta);
        }

        bool render_text = node.m_NodeType == NODE_TYPE_TEXT && !(flags & CALCULATE_NODE_BOUNDARY);
        if ((flags & CALCULATE_NODE_INCLUDE_SIZE) && !render_text)
        {
            transform.setUpper3x3(transform.getUpper3x3() * Matrix3::scale(Vector3(size.getX(), size.getY(), 1)));
        }
    }

    /** calculates the transform of a parent node
     *
     * @param scene scene of the node
     * @param node node for which to calculate the transform
     * @param flags CalculateNodeTransformFlags enumerated behaviour flags
     * @param out_transform [out] out-parameter to write the calculated transform to
     * @param out_opacity [out] out-parameter to write the calculated opacity
     * @param traversal_cache [in, out] a helper cache to store/retrieve properties during traversal
     */
    inline void CalculateParentNodeTransformAndAlphaCached(HScene scene, InternalNode* n, Matrix4& out_transform, float& out_opacity, SceneTraversalCache& traversal_cache)
    {
        const Node& node = n->m_Node;
        uint16_t cache_index;
        bool cached;
        uint16_t cache_version = n->m_SceneTraversalCacheVersion;
        if(cache_version != traversal_cache.m_Version)
        {
            n->m_SceneTraversalCacheVersion = traversal_cache.m_Version;
            cache_index = n->m_SceneTraversalCacheIndex = traversal_cache.m_NodeIndex++;
            cached = false;
        }
        else
        {
            cache_index = n->m_SceneTraversalCacheIndex;
            cached = true;
        }
        SceneTraversalCache::Data& cache_data = traversal_cache.m_Data[cache_index];

        Matrix4 parent_trans;
        float parent_opacity;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            CalculateParentNodeTransformAndAlphaCached(scene, parent, parent_trans, parent_opacity, traversal_cache);
        }

        if (node.m_DirtyLocal || (scene->m_ResChanged && scene->m_AdjustReference != ADJUST_REFERENCE_DISABLED))
        {
            UpdateLocalTransform(scene, n);
        }
        else if(cached)
        {
            out_transform = cache_data.m_Transform;
            out_opacity = cache_data.m_Opacity;
            return;
        }
        out_transform = node.m_LocalTransform;
        out_opacity = n->m_Node.m_Properties[dmGui::PROPERTY_COLOR].getW();

        if (n->m_ParentIndex != INVALID_INDEX)
        {
            out_transform = parent_trans * out_transform;

            if (node.m_InheritAlpha)
            {
                out_opacity *= parent_opacity;
            }
        }

        cache_data.m_Transform = out_transform;
        cache_data.m_Opacity = out_opacity;
    }

    /** calculates the transform of a node
     * A boundary transform maps the local rectangle (0,1),(0,1) to screen space such that it inclusively encapsulates the node boundaries in screen space.
     * Box nodes are rendered in boundary space (quad with dimensions (0,1),(0,1)), so the same transform is calculated whether or not the CALCULATE_NODE_BOUNDARY flag is set.
     * Text nodes are rendered in a transform where the origin is located at the left edge of the base line, excluding the text size, since it's implicitly spanned by glyph quads.
     * Their boundary transform is analogue to the box boundary transform.
     * This is complicated and could be simplified by supporting different pivots when rendering text.
     *
     * @param scene scene of the node
     * @param node node for which to calculate the transform
     * @param flags CalculateNodeTransformFlags enumerated behaviour flags
     * @param out_transform [out] out-parameter to write the calculated transform to
     * @param out_opacity [out] out-parameter to write the calculated opacity
     */
    inline void CalculateNodeTransformAndAlphaCached(HScene scene, InternalNode* n, const CalculateNodeTransformFlags flags, Matrix4& out_transform, float& out_opacity)
    {
        Matrix4 parent_trans;
        float parent_opacity;
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            InternalNode* parent = &scene->m_Nodes[n->m_ParentIndex];
            CalculateParentNodeTransformAndAlphaCached(scene, parent, parent_trans, parent_opacity, scene->m_Context->m_SceneTraversalCache);
        }

        const Node& node = n->m_Node;
        if (node.m_DirtyLocal || (scene->m_ResChanged && scene->m_AdjustReference != ADJUST_REFERENCE_DISABLED))
        {
            UpdateLocalTransform(scene, n);
        }
        out_transform = node.m_LocalTransform;
        CalculateNodeExtents(node, flags, out_transform);

        out_opacity = node.m_Properties[dmGui::PROPERTY_COLOR].getW();
        if (n->m_ParentIndex != INVALID_INDEX)
        {
            out_transform = parent_trans * out_transform;
            if (node.m_InheritAlpha)
            {
                out_opacity *= parent_opacity;
            }
        }
    }


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
