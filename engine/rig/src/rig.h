// Copyright 2020-2025 The Defold Foundation
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

#ifndef DM_RIG_H
#define DM_RIG_H

#include <dmsdk/rig/rig.h>

namespace dmRig
{
	static const uint16_t INVALID_POSE_MATRIX_CACHE_INDEX = 0xFFFF;

	struct RigModelSkinnedVertex
    {
        RigModelVertex m_Vertex;
        float          m_BoneWeights[4];
        float          m_BoneIndices[4];
    };

    struct PoseMatrixCache
    {
        dmArray<dmVMath::Matrix4> m_PoseMatrices;
        dmArray<uint32_t>         m_BoneCounts;
        uint32_t                  m_MaxBoneCount;
    };

    void                   ResetPoseMatrixCache(HRigContext context);
    const PoseMatrixCache* GetPoseMatrixCache(HRigContext context);
    uint16_t               AcquirePoseMatrixCacheIndex(HRigContext context, HRigInstance instance);
    uint32_t               GetPoseMatrixCacheIndex(HRigContext context, uint16_t cache_index);
}

#endif // DM_RIG_H
