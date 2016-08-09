#ifndef DM_RIG_H
#define DM_RIG_H

#include <stdint.h>

#include <dlib/object_pool.h>
#include <dlib/hash.h>
#include <dlib/vmath.h>
#include <dlib/align.h>

#include <render/render.h>
#include <gameobject/gameobject.h>

#include <ddf/ddf.h>
#include "rig/rig_ddf.h"

using namespace Vectormath::Aos;

namespace dmRig
{
    using namespace dmRigDDF;

    typedef struct RigContext*  HRigContext;
    typedef struct RigInstance* HRigInstance;

    struct RigPlayer
    {
        /// Currently playing animation
        const dmRigDDF::RigAnimation*       m_Animation;
        dmhash_t                            m_AnimationId;
        /// Playback cursor in the interval [0,duration]
        float                               m_Cursor;
        /// Playback mode
        dmGameObject::Playback              m_Playback;
        /// Whether the animation is currently playing
        uint16_t                             m_Playing : 1;
        /// Whether the animation is playing backwards (e.g. ping pong)
        uint16_t                             m_Backwards : 1;
    };

    struct RigBone
    {
        /// Local space transform
        dmTransform::Transform m_LocalToParent;
        /// Model space transform
        dmTransform::Transform m_LocalToModel;
        /// Inv model space transform
        dmTransform::Transform m_ModelToLocal;
        /// Index of parent bone, NOTE root bone has itself as parent
        uint32_t m_ParentIndex;
        /// Length of the bone
        float m_Length;
    };

    // New enum
    enum RigMeshType
    {
        RIG_SPINE = 1,
        RIG_MODEL = 2
    };

    // SpineModelWorld -> RigContext changes;
    //  * m_Components changed to m_Instances and is now a pool of RigInstaces instead.
    //  * Removed render and vertex stuff, this should be handled by the component instead.
    struct RigContext
    {
        dmObjectPool<HRigInstance> m_Instances;
        dmArray<uint32_t>          m_DrawOrderToMesh;
    };

    struct MeshProperties {
        float m_Color[4];
        uint32_t m_Order;
        bool m_Visible;
    };

    // Notes on SpineModelComponent -> RigInstance changes;
    //  * m_NodeInstances has been removed, should be added to the corresponding
    //     user struct since it can be either GOs or a GUI nodes.
    //  * SpinePlayer renamed to RigPlayer
    //  * m_MeshType is new and indicates how the vertex data will be filled later,
    //  either with color data or normals, see RigVertexData.
    struct RigInstance
    {
        RigPlayer                   m_Players[2];
        // dmMessage::URL              m_Listener;

        // HRigContext                   m_Context;
        uint32_t                      m_Index;

        // DM_ALIGNED(16) const dmArray<RigBone>* m_BindPose;
        const dmArray<RigBone>*       m_BindPose;
        const dmRigDDF::Skeleton*     m_Skeleton;
        const dmRigDDF::MeshSet*      m_MeshSet;
        const dmRigDDF::AnimationSet* m_AnimationSet;

        /// Animated pose, every transform is local-to-model-space and describes the delta between bind pose and animation
        dmArray<dmTransform::Transform> m_Pose;

        /// Animated IK
        // dmArray<IKAnimation>        m_IKAnimation;
        /// Node instances corresponding to the bones
        // dmArray<dmGameObject::HInstance> m_NodeInstances;
        /// Animated mesh properties
        dmArray<MeshProperties>     m_MeshProperties;
        // User IK constraint targets
        // dmArray<IKTarget>           m_IKTargets;
        /// Currently used mesh
        const dmRigDDF::MeshEntry*  m_MeshEntry;
        dmhash_t                    m_Skin;
        float                       m_BlendDuration;
        float                       m_BlendTimer;
        /// Current player index
        uint8_t                     m_CurrentPlayer : 1;
        /// Whether we are currently X-fading or not
        uint8_t                     m_Blending : 1;

        /// Mesh type indicate how vertex data will be filled
        RigMeshType                 m_MeshType;
    };


    // RigVertexData includes position, texture coords and either color or normal.
    struct RigVertexData
    {
        float x;
        float y;
        float z;
        uint16_t u;
        uint16_t v;
        union  {
            struct {
                uint8_t r;
                uint8_t g;
                uint8_t b;
                uint8_t a;
            };
            struct {
                float nx;
                float ny;
                float nz;
            };
        };

    };


    // RigGenVertexDataParams points to a buffer with RigVertexData structs. Spine will
    // pass a slightly smaller struct since it has fewer fields (byte colors vs
    // float normals), but cast it to RigVertexData*).
    // Instead of iterating over the data pointer as RigVertexData we need to
    // move with m_VertexStride due to this very reason...
    struct RigGenVertexDataParams
    {
        Matrix4 m_ModelMatrix;
        void**  m_VertexData;
        int     m_VertexDataSize;
        int     m_VertexStride;
    };

    // Both RigIKTargetParams and RigIKTargetPositionParams are self explanatory,
    // they hold information that was previously set directly in script_spine_model.cpp
    struct RigIKTargetParams
    {
        HRigInstance            m_RigInstance;
        dmhash_t                m_ConstraintId;
        dmGameObject::HInstance m_GOInstance;
    };

    struct RigIKTargetPositionParams
    {
        HRigInstance             m_RigInstance;
        dmhash_t                 m_ConstraintId;
        Vectormath::Aos::Vector3 m_Position;
    };

    enum CreateResult
    {
        CREATE_RESULT_OK    = 0,
        CREATE_RESULT_ERROR = 1
    };

    enum UpdateResult
    {
        UPDATE_RESULT_OK    = 0,
        UPDATE_RESULT_ERROR = 1
    };

    enum PlayResult
    {
        PLAY_RESULT_OK             = 0,
        PLAY_RESULT_ANIM_NOT_FOUND = 1,
        PLAY_RESULT_UNKNOWN_ERROR  = 2
    };

    enum IKUpdate
    {
        IKUPDATE_RESULT_OK    = 0,
        IKUPDATE_RESULT_ERROR = 1
    };

    struct NewContextParams {
        HRigContext* m_Context;
        uint32_t     m_MaxRigInstanceCount;
    };

    struct DeleteContextParams {
        HRigContext m_Context;
    };

    struct InstanceCreateParams
    {
        HRigContext             m_Context;
        HRigInstance*           m_Instance;
        RigMeshType             m_MeshType;

        const char*             m_SkinId;
        const char*             m_DefaultAnimation;

        const dmArray<RigBone>*       m_BindPose;
        const dmRigDDF::Skeleton*     m_Skeleton;
        const dmRigDDF::MeshSet*      m_MeshSet;
        const dmRigDDF::AnimationSet* m_AnimationSet;
    };

    struct InstanceDestroyParams
    {
        HRigContext  m_Context;
        HRigInstance m_Instance;
    };

    /// Config key to use for tweaking the total maximum number of particles in a context.
    extern const char* MAX_RIG_INSTANCE_COUNT_KEY;

    //////////////////////////////////////////////////////////////////
    // External API:
    //////////////////////////////////////////////////////////////////

    /* CompSpineModelNewWorld */
    CreateResult NewContext(const NewContextParams& params);

    /* CompSpineModelDeleteWorld */
    CreateResult DeleteContext(const DeleteContextParams& params);

    /* CompSpineModelUpdate */
    UpdateResult Update(HRigContext context, float dt);

    /* CompSpineModelCreate */
    CreateResult InstanceCreate(const InstanceCreateParams& params);

    /* CompSpineModelDestroy */
    CreateResult InstanceDestroy(const InstanceDestroyParams& params);

    /* CompSpineModelOnMessage */
    PlayResult PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmGameObject::Playback playback, float blend_duration);

    /* CompSpineModelOnMessage */
    PlayResult CancelAnimation(HRigInstance instance);

    /* CompSpineModelRender */
    uint32_t GetVertexCount(HRigInstance instance);
    RigVertexData* GenerateVertexData(HRigContext context, HRigInstance instance, const RigGenVertexDataParams& params);

    // The following functions correspond to properties that previously
    // would be returned/set in CompSpineModelGetProperty and CompSpineModelSetProperty;
    dmhash_t GetSkin(HRigInstance instance);
    void SetSkin(HRigInstance instance, dmhash_t skin_id);
    dmhash_t GetAnimation(HRigInstance instance);
    void SetAnimation(HRigInstance instance, dmhash_t animation_id);

    // New getters/setters:
    const dmArray<dmTransform::Transform>& GetPose(HRigInstance instance);
    float GetCursor(HRigInstance instance);
    PlayResult SetCursor(HRigInstance instance, float cursor);

    // New IK related setters
    IKUpdate SetIKTarget(const RigIKTargetParams& params);
    IKUpdate SetIKTargetPosition(const RigIKTargetPositionParams& params);

}




#endif // DM_RIG_H
