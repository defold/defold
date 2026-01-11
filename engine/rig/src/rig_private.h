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
        uint8_t                       m_Playing : 1;
        /// Whether the animation is playing backwards (e.g. ping pong)
        uint8_t                       m_Backwards : 1;
        uint8_t                       : 6;
    };

    struct IKAnimation
    {
        float m_Mix;
        bool m_Positive;
    };

    struct RigInstance
    {
        RigPlayer                     m_Players[2];
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

        const dmRigDDF::Model*        m_Model;      // Currently selected model
        uint32_t                      m_NumModels;

        dmhash_t                      m_ModelId;
        float                         m_BlendDuration;
        float                         m_BlendTimer;
        // Max bone count used by skeleton (if it is used) and meshset
        uint16_t                      m_MaxBoneCount;
        uint16_t                      m_PoseMatrixCacheIndex;
        /// Current player index
        uint8_t                       m_CurrentPlayer : 1;
        /// Whether we are currently X-fading or not
        uint8_t                       m_Blending : 1;
        uint8_t                       m_Enabled : 1;
        uint8_t                       m_DoRender : 1;
        uint8_t                       : 4;
    };

    /** Pose matrix cache
     * A pose matrix cache stores a buffer of pose matrices for any given number of rig instances.
     * The cache will keep growing as more rig instances are added, until it is explicitly reset.
     * Resetting the cache will not release memory.
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
