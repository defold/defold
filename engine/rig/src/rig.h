#ifndef DM_RIG_H
#define DM_RIG_H

#include <stdint.h>

#include <dlib/object_pool.h>
#include <dlib/hash.h>
#include <dlib/vmath.h>
#include <dlib/align.h>
#include <dlib/transform.h>

#include <render/render.h>

#include <ddf/ddf.h>
#include "rig/rig_ddf.h"

using namespace Vectormath::Aos;

namespace dmRig
{
    /// Config key to use for tweaking the total maximum number of rig instances in a context.
    extern const char* RIG_MAX_INSTANCES_KEY;

    /// Default max rig instances in each context
    const uint32_t DEFAULT_MAX_RIG_CAPACITY = 128;

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

    enum RigPlayback
    {
        PLAYBACK_NONE          = 0,
        PLAYBACK_ONCE_FORWARD  = 1,
        PLAYBACK_ONCE_BACKWARD = 2,
        PLAYBACK_ONCE_PINGPONG = 3,
        PLAYBACK_LOOP_FORWARD  = 4,
        PLAYBACK_LOOP_BACKWARD = 5,
        PLAYBACK_LOOP_PINGPONG = 6,
        PLAYBACK_COUNT = 7,
    };

    struct RigPlayer
    {
        RigPlayer() : m_Animation(0x0),
                      m_AnimationId(0x0),
                      m_Cursor(0.0f),
                      m_Playback(dmRig::PLAYBACK_ONCE_FORWARD),
                      m_Playing(0x0),
                      m_Backwards(0x0) {};
        /// Currently playing animation
        const dmRigDDF::RigAnimation* m_Animation;
        dmhash_t                      m_AnimationId;
        /// Playback cursor in the interval [0,duration]
        float                         m_Cursor;
        /// Rate of playback, multiplied with dt when stepping. Always >= 0.0f
        float                         m_PlaybackRate;
        /// Playback mode
        RigPlayback                   m_Playback;
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
        Matrix4 m_ModelToLocal;
        /// Index of parent bone, NOTE root bone has itself as parent
        uint32_t m_ParentIndex;
        /// Length of the bone
        float m_Length;
    };

    struct MeshProperties
    {
        float m_Color[4];
        uint32_t m_Order;
        int32_t m_OrderOffset;
        int32_t m_Slot;
        int32_t m_MeshId;
        bool m_Visible : 1;
        bool m_ColorFromTrack : 1;
        bool m_VisibleFromTrack : 1;
        bool m_OffsetFromTrack : 1;
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

    // NOTE: We expose two different vertex format that GenerateVertexData can output.
    // This is a temporary fix until we have better support for custom vertex formats.
    enum RigVertexFormat
    {
        RIG_VERTEX_FORMAT_SPINE = 0,
        RIG_VERTEX_FORMAT_MODEL = 1
    };

    struct RigSpineModelVertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
        uint32_t rgba;
    };

    struct RigModelVertex
    {
        float x;
        float y;
        float z;
        float u;
        float v;
        float nx;
        float ny;
        float nz;
    };

    struct RigContext
    {
        dmObjectPool<HRigInstance>      m_Instances;
        dmArray<int32_t>                m_ScratchSlotsBuffer;
        // Temporary scratch buffers used for store pose as transform and matrices
        // (avoids modifying the real pose transform data during rendering).
        dmArray<dmTransform::Transform> m_ScratchPoseTransformBuffer;
        dmArray<Matrix4>                m_ScratchInfluenceMatrixBuffer;
        dmArray<Matrix4>                m_ScratchPoseMatrixBuffer;
        // Temporary scratch buffers used when transforming the vertex buffer,
        // used to creating primitives from indices.
        dmArray<Vector3>                m_ScratchPositionBuffer;
        dmArray<Vector3>                m_ScratchNormalBuffer;
    };

    struct NewContextParams {
        HRigContext* m_Context;
        uint32_t     m_MaxRigInstanceCount;
    };

    typedef void (*RigEventCallback)(RigEventType, void*, void*, void*);
    typedef void (*RigPoseCallback)(void*, void*);

    struct RigInstance
    {
        RigPlayer                     m_Players[2];
        uint32_t                      m_Index;
        /// Rig input data
        const dmArray<RigBone>*       m_BindPose;
        const dmRigDDF::Skeleton*     m_Skeleton;
        const dmRigDDF::MeshSet*      m_MeshSet;
        const dmRigDDF::AnimationSet* m_AnimationSet;
        const dmArray<uint32_t>*      m_PoseIdxToInfluence;
        const dmArray<uint32_t>*      m_TrackIdxToPose;
        RigPoseCallback               m_PoseCallback;
        void*                         m_PoseCBUserData1;
        void*                         m_PoseCBUserData2;
        dmArray<uint32_t>             m_DrawOrderToMesh;
        /// Event handling
        RigEventCallback              m_EventCallback;
        void*                         m_EventCBUserData1;
        void*                         m_EventCBUserData2;
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
        // Max bone count used by skeleton (if it is used) and meshset
        uint32_t                      m_MaxBoneCount;
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

        const dmArray<uint32_t>*      m_PoseIdxToInfluence;
        const dmArray<uint32_t>*      m_TrackIdxToPose;

        RigPoseCallback               m_PoseCallback;
        void*                         m_PoseCBUserData1;
        void*                         m_PoseCBUserData2;
        RigEventCallback              m_EventCallback;
        void*                         m_EventCBUserData1;
        void*                         m_EventCBUserData2;
    };

    struct InstanceDestroyParams
    {
        HRigContext  m_Context;
        HRigInstance m_Instance;
    };

#define DM_RIG_PROTO(ret, name,  ...) \
    \
    ret name(__VA_ARGS__);\
    extern "C" DM_DLLEXPORT ret Rig_##name(__VA_ARGS__)


    // Result NewContext(const NewContextParams& params);
    DM_RIG_PROTO(Result, NewContext, const NewContextParams& params);
    // void DeleteContext(HRigContext context);
    DM_RIG_PROTO(void, DeleteContext, HRigContext context);
    // Result Update(HRigContext context, float dt);
    DM_RIG_PROTO(Result, Update, HRigContext context, float dt);

    Result InstanceCreate(const InstanceCreateParams& params);
    Result InstanceDestroy(const InstanceDestroyParams& params);

    Result PlayAnimation(HRigInstance instance, dmhash_t animation_id, RigPlayback playback, float blend_duration, float offset, float playback_rate);
    Result CancelAnimation(HRigInstance instance);
    dmhash_t GetAnimation(HRigInstance instance);

    //void* GenerateVertexData(HRigContext context, HRigInstance instance, const Matrix4& model_matrix, const Matrix4& normal_matrix, const Vector4 color, RigVertexFormat vertex_format, void* vertex_data_out);
    DM_RIG_PROTO(void*, GenerateVertexData, HRigContext context, HRigInstance instance, const Matrix4& model_matrix, const Matrix4& normal_matrix, const Vector4 color, RigVertexFormat vertex_format, void* vertex_data_out);
    //uint32_t GetVertexCount(HRigInstance instance);
    DM_RIG_PROTO(uint32_t, GetVertexCount, HRigInstance instance);

    Result SetMesh(HRigInstance instance, dmhash_t mesh_id);
    dmhash_t GetMesh(HRigInstance instance);
    float GetCursor(HRigInstance instance, bool normalized);
    Result SetCursor(HRigInstance instance, float cursor, bool normalized);
    float GetPlaybackRate(HRigInstance instance);
    Result SetPlaybackRate(HRigInstance instance, float playback_rate);
    dmArray<dmTransform::Transform>* GetPose(HRigInstance instance);
    IKTarget* GetIKTarget(HRigInstance instance, dmhash_t constraint_id);
    void SetEnabled(HRigInstance instance, bool enabled);
    bool GetEnabled(HRigInstance instance);
    bool IsValid(HRigInstance instance);
    uint32_t GetBoneCount(HRigInstance instance);
    uint32_t GetMaxBoneCount(HRigInstance instance);
    void SetEventCallback(HRigInstance instance, RigEventCallback event_callback, void* user_data1, void* user_data2);

    // Util function used to fill a bind pose array from skeleton data
    // used in rig tests and loading rig resources.
    void CreateBindPose(dmRigDDF::Skeleton& skeleton, dmArray<RigBone>& bind_pose);
    void FillBoneListArrays(const dmRigDDF::MeshSet& meshset, const dmRigDDF::AnimationSet& animationset, const dmRigDDF::Skeleton& skeleton, dmArray<uint32_t>& track_idx_to_pose, dmArray<uint32_t>& pose_idx_to_influence);
}

#endif // DM_RIG_H
