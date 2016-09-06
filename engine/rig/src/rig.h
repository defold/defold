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
    /// Config key to use for tweaking the total maximum number of rig instances in a context.
    extern const char* MAX_RIG_INSTANCE_COUNT_KEY;

    using namespace dmRigDDF;

    typedef struct RigContext*  HRigContext;
    typedef struct RigInstance* HRigInstance;

    enum Result
    {
        RESULT_OK             = 0,
        RESULT_ERROR          = 1,
        RESULT_ANIM_NOT_FOUND = 2
    };

    enum RigMeshType
    {
        RIG_SPINE = 1,
        RIG_MODEL = 2
    };

    struct RigPlayer
    {
        RigPlayer() : m_Animation(0x0),
                      m_AnimationId(0x0),
                      m_Cursor(0.0f),
                      m_Playback(dmGameObject::PLAYBACK_ONCE_FORWARD),
                      m_Playing(0x0),
                      m_Backwards(0x0) {};
        /// Currently playing animation
        const dmRigDDF::RigAnimation* m_Animation;
        dmhash_t                      m_AnimationId;
        /// Playback cursor in the interval [0,duration]
        float                         m_Cursor;
        /// Playback mode
        dmGameObject::Playback        m_Playback;
        /// Whether the animation is currently playing
        uint16_t                      m_Playing : 1;
        /// Whether the animation is playing backwards (e.g. ping pong)
        uint16_t                      m_Backwards : 1;
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

    struct MeshProperties
    {
        float m_Color[4];
        uint32_t m_Order;
        bool m_Visible;
    };

    struct IKAnimation
    {
        float m_Mix;
        bool m_Positive;
    };

    typedef struct IKTarget IKTarget;
    typedef Vector3 (*RigIKTargetCallback)(IKTarget*);

    // IK targets can either use a static position or a callback (that is
    // called during the context update). A pointer to the IKTarget struct
    // is passed to the callback as the only argument. If the IK target
    // becomes invalid (for example the GO is removed in the collection,
    // or a GUI node in the GUI scene) it is up the callback to reset the
    // struct fields.
    struct IKTarget {
        float               m_Mix;
        /// Static IK target position
        Vector3             m_Position;
        /// Callback to dynamically set the IK target position.
        RigIKTargetCallback m_Callback;
        void*               m_UserPtr;
        dmhash_t            m_UserHash;
    };

    struct RigIKTargetParams
    {
        HRigInstance        m_RigInstance;
        dmhash_t            m_ConstraintId;
        float               m_Mix;
        RigIKTargetCallback m_Callback;
        void*               m_UserData1;
        void*               m_UserData2;
    };

    enum RigEventType
    {
        RIG_EVENT_TYPE_COMPLETED = 0,
        RIG_EVENT_TYPE_KEYFRAME  = 1
    };

    struct RigCompletedEventData
    {
        uint64_t  m_AnimationId;
        uint32_t  m_Playback;
    };

    struct RigKeyframeEventData
    {
        uint64_t  m_EventId;
        uint64_t  m_AnimationId;
        float     m_T;
        float     m_BlendWeight;
        int32_t   m_Integer;
        float     m_Float;
        uint64_t  m_String;
    };

    struct RigContext
    {
        dmObjectPool<HRigInstance> m_Instances;
        dmArray<uint32_t>          m_DrawOrderToMesh;
    };

    struct NewContextParams {
        HRigContext* m_Context;
        uint32_t     m_MaxRigInstanceCount;
    };

    typedef void (*RigEventCallback)(void*, RigEventType, const void*);
    typedef void (*RigPoseCallback)(void*);

    struct RigInstance
    {
        RigPlayer                     m_Players[2];
        uint32_t                      m_Index;
        /// Rig input data
        const dmArray<RigBone>*       m_BindPose;
        const dmRigDDF::Skeleton*     m_Skeleton;
        const dmRigDDF::MeshSet*      m_MeshSet;
        const dmRigDDF::AnimationSet* m_AnimationSet;
        RigPoseCallback               m_PoseCallback;
        void*                         m_PoseCBUserData;
        /// Event handling
        RigEventCallback              m_EventCallback;
        void*                         m_EventCBUserData;
        /// Animated pose, every transform is local-to-model-space and describes the delta between bind pose and animation
        dmArray<dmTransform::Transform> m_Pose;
        /// Animated IK
        dmArray<IKAnimation>          m_IKAnimation;
        /// User IK constraint targets
        dmArray<IKTarget>             m_IKTargets;
        /// Animated mesh properties
        dmArray<MeshProperties>       m_MeshProperties;
        /// Currently used mesh
        const dmRigDDF::MeshEntry*    m_MeshEntry;
        dmhash_t                      m_MeshId;
        float                         m_BlendDuration;
        float                         m_BlendTimer;
        /// Mesh type indicate how vertex data will be filled
        RigMeshType                   m_MeshType;
        /// Current player index
        uint8_t                       m_CurrentPlayer : 1;
        /// Whether we are currently X-fading or not
        uint8_t                       m_Blending : 1;
        uint8_t                       m_Enabled : 1;
        uint8_t                       m_DoRender : 1;
    };

    struct InstanceCreateParams
    {
        HRigContext                   m_Context;
        HRigInstance*                 m_Instance;
        RigMeshType                   m_MeshType;

        dmhash_t                      m_MeshId;
        dmhash_t                      m_DefaultAnimation;

        const dmArray<RigBone>*       m_BindPose;
        const dmRigDDF::Skeleton*     m_Skeleton;
        const dmRigDDF::MeshSet*      m_MeshSet;
        const dmRigDDF::AnimationSet* m_AnimationSet;
        RigPoseCallback               m_PoseCallback;
        void*                         m_PoseCBUserData;
        RigEventCallback              m_EventCallback;
        void*                         m_EventCBUserData;
    };

    struct InstanceDestroyParams
    {
        HRigContext  m_Context;
        HRigInstance m_Instance;
    };

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

    struct RigGenVertexDataParams
    {
        Matrix4 m_ModelMatrix;
        void**  m_VertexData;
        int32_t m_VertexStride;
    };

    Result NewContext(const NewContextParams& params);
    void DeleteContext(HRigContext context);
    Result Update(HRigContext context, float dt);

    Result InstanceCreate(const InstanceCreateParams& params);
    Result InstanceDestroy(const InstanceDestroyParams& params);

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, dmGameObject::Playback playback, float blend_duration);
    Result CancelAnimation(HRigInstance instance);
    dmhash_t GetAnimation(HRigInstance instance);
    uint32_t GetVertexCount(HRigInstance instance);
    RigVertexData* GenerateVertexData(HRigContext context, HRigInstance instance, const RigGenVertexDataParams& params);
    Result SetMesh(HRigInstance instance, dmhash_t mesh_id);
    dmhash_t GetMesh(HRigInstance instance);
    float GetCursor(HRigInstance instance, bool normalized);
    Result SetCursor(HRigInstance instance, float cursor, bool normalized);
    dmArray<dmTransform::Transform>* GetPose(HRigInstance instance);
    IKTarget* GetIKTarget(HRigInstance instance, dmhash_t constraint_id);
    void SetEnabled(HRigInstance instance, bool enabled);
    bool GetEnabled(HRigInstance instance);
    bool IsValid(HRigInstance instance);

}

#endif // DM_RIG_H
