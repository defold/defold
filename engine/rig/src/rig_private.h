// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_RIG_PRIVATE_H
#define DM_RIG_PRIVATE_H

#include <dlib/array.h>
#include "rig.h"
#include <rig/rig_ddf.h>

namespace dmRig
{
    /*
    * Morph animation weights shared by all meshes of a MeshSet model.
    * Reset clears the weights to zero; rendering falls back to the mesh DDF's
    * m_MorphBaseWeights for glTF/rest defaults.
    */
    struct MorphWeightSlot
    {
        /// Model that owns the morph weights
        uint64_t m_ModelId;
        /// Maximum morph-target count among the model's meshes
        uint32_t m_MorphCount;
        /// Offset in floats into RigInstance::m_MorphWeightsBuffer, spanning m_MorphCount weights
        uint32_t m_BufferOffset;
    };

    /*
    * Playback of a single animation clip, including its cursor, speed and direction.
    * Two players are used by RigPlaybackState to crossfade between clips.
    */
    struct RigPlayer
    {
        RigPlayer()
        : m_Animation(0x0)
        , m_AnimationId(0x0)
        , m_Cursor(0.0f)
        , m_Playback(dmRig::PLAYBACK_ONCE_FORWARD)
        , m_Playing(0x0)
        , m_Backwards(0x0) {};

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
        uint8_t                       m_Playing : 1;
        /// Whether the animation is playing backwards (e.g. ping pong)
        uint8_t                       m_Backwards : 1;
        uint8_t                       : 6;
    };

    /*
    * Animated settings for an IK constraint: its influence and bend direction.
    * These are evaluated alongside the bone animation and combined with user IK targets.
    */
    struct IKAnimation
    {
        float m_Mix;
        bool  m_Positive;
    };

    /*
    * Immutable layer definition shared by rig instances through reference counting.
    * Defines evaluation order, blend weight and bone selection; playback state lives in RigLayer.
    * m_AllBones distinguishes an omitted bone selection from an explicitly empty one.
    */
    struct AnimationLayer
    {
        dmhash_t          m_Name;
        dmArray<dmhash_t> m_Bones;
        uint32_t          m_RefCount;
        int32_t           m_Order;
        float             m_Weight;
        bool              m_AllBones;
    };

    /*
    * Playback and crossfade state for the base animation or one additional layer.
    * Tracks incoming and outgoing clips, with a generation used to reject stale events.
    */
    struct RigPlaybackState
    {
        RigPlayer m_Players[2];
        float     m_BlendDuration;
        float     m_BlendTimer;
        uint64_t  m_Generation;
        uint8_t   m_CurrentPlayer;
        uint8_t   m_Blending;
    };

    /*
    * One rig instance's playback of a shared animation layer definition.
    * Resolves the definition's bone selection into a skeleton mask and stores the layer pose.
    */
    struct RigLayer
    {
        HAnimationLayer      m_Definition;
        RigPlaybackState     m_Playback;
        dmArray<uint8_t>     m_Mask;
        dmArray<BonePose>    m_Pose;
    };

    /*
    * Animation event queued until pose updates have finished.
    * The layer and playback generation identify its source so cancelled or replaced
    * playback cannot deliver stale events.
    */
    struct PendingRigEvent
    {
        RigEventType          m_Type;
        HAnimationLayer       m_Layer;
        uint64_t              m_Generation;
        union
        {
            RigCompletedEventData m_Completed;
            RigKeyframeEventData  m_Keyframe;
        };
    };

    /*
    * Runtime state for one rig, referencing its skeleton, meshes and animation data.
    * Owns base and layered playback, evaluated poses, IK and morph weights, and queued events.
    * Callbacks expose pose updates and animation events to the owning component.
    */
    struct RigInstance
    {
        RigPlaybackState              m_BasePlayback;
        dmArray<RigLayer*>             m_Layers;
        // Allocated only for layered playback. Never reuse the composite as a base pose.
        dmArray<BonePose>              m_BasePose;
        dmArray<PendingRigEvent>       m_PendingEvents;
        uint64_t                      m_PlayGeneration;
        uint32_t                      m_Index;
        /// Rig input data
        const dmArray<RigBone>*         m_BindPose;
        const dmHashTable64<uint32_t>*  m_BoneIndices;   // Map of bone name hash -> bone index
        const dmRigDDF::Skeleton*       m_Skeleton;
        const dmRigDDF::MeshSet*        m_MeshSet;
        const dmRigDDF::AnimationSet*   m_AnimationSet;

        RigPoseCallback               m_PoseCallback;
        void*                         m_PoseCBUserData1;
        void*                         m_PoseCBUserData2;

        /// Event handling
        RigEventCallback              m_EventCallback;
        void*                         m_EventCBUserData1;
        void*                         m_EventCBUserData2;
        /// Animated pose, every transform is local-to-model-space and describes the delta between bind pose and animation
        dmArray<BonePose>             m_Pose;

        /// Animated IK
        dmArray<IKAnimation>          m_IKAnimation;
        /// User IK constraint targets
        dmArray<IKTarget>             m_IKTargets;

        dmArray<MorphWeightSlot>      m_MorphSlots;
        dmArray<float>                m_MorphWeightsBuffer;
        dmArray<float>                m_MorphScratch;

        const dmRigDDF::Model*        m_Model;      // Currently selected model
        uint32_t                      m_NumModels;

        dmhash_t                      m_ModelId;
        // Max bone count used by skeleton (if it is used) and meshset
        uint16_t                      m_MaxBoneCount;
        uint16_t                      m_PoseMatrixCacheIndex;
        uint8_t                       m_Enabled : 1;
        uint8_t                       m_DoRender : 1;
        uint8_t                       m_HasPoseMatrixCacheAnimatedPose : 1;
        uint8_t                       m_RequiresUpdate : 1;
        uint8_t : 4;
    };

    /*
    * Pose matrices shared by the rig instances that acquire cache slots.
    * The cache grows as slots are added until it is reset; resetting retains allocated memory.
    */
    struct PoseMatrixCache
    {
        /// The array of pose matrices for all rig instances
        dmArray<dmVMath::Matrix4> m_PoseMatrices;
        /// The array of bone counts for the rig instances that have acquired a pose matrix cache slot
        dmArray<uint32_t>         m_CacheEntryOffsets;
        /// The total number of cached bone pose matrices
        uint32_t                  m_TotalPoseCount;
        /// The max bone count found in the set of pose matrices (I think this can be removed?)
        uint32_t                  m_MaxBoneCount;
    };

    PoseMatrixCache* GetPoseMatrixCache(HRigContext context);
}

#endif
