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

#ifndef DM_RIG_H
#define DM_RIG_H

#include <dmsdk/rig/rig.h>

namespace dmRig
{
    static const uint16_t INVALID_POSE_MATRIX_CACHE_ENTRY = 0xFFFF;

    typedef uint16_t HCachePoseMatrixEntry;

    struct RigModelSkinnedVertex
    {
        RigModelVertex m_Vertex;
        float          m_BoneWeights[4];
        float          m_BoneIndices[4];
    };

    /**
     * Gets the pose matrix cache data pointer and number of matrices in the cache
     * @param context The rig context
     * @param pose_matrices A pointer to where the pose matrix pointer will be stored
     * @param pose_matrices_count A pointer to where the pose matrix count will be stored
     */
    void GetPoseMatrixCacheData(HRigContext context, const dmVMath::Matrix4** pose_matrices, uint32_t* pose_matrices_count);

    /**
     * Gets the offset into the pose matrix cache for a given cache entry
     * @param context The rig context
     * @param instance The rig instance
     * @return The offset into the pose matrix cache
     */
    uint32_t GetPoseMatrixCacheDataOffset(HRigContext context, HRigInstance instance);

    /**
     * Resets the pose matrix cache, i.e rewinds the buffer to the beginning.
     * All instance entry handles will be reset to INVALID_POSE_MATRIX_CACHE_ENTRY
     * @param context The rig context
     */
    void ResetPoseMatrixCache(HRigContext context);

    /**
     * Acquires a pose matrix cache entry
     * @param context The rig context
     * @param instance The rig instance
     * @return The pose matrix cache entry or INVALID_POSE_MATRIX_CACHE_ENTRY if the cache is full
     */
    HCachePoseMatrixEntry AcquirePoseMatrixCacheEntry(HRigContext context, HRigInstance instance);

    /**
     * Checks if an instance is currently animating
     * @param instance The rig instance
     * @return True if the instance is currently animating
     */
    bool IsAnimating(HRigInstance instance);
}

#endif // DM_RIG_H
