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

#ifndef DM_GAMEOBJECT_TRANSFORM_PRIVATE_H
#define DM_GAMEOBJECT_TRANSFORM_PRIVATE_H

#include <dmsdk/gameobject/gameobject.h>

namespace dmGameObject
{
    struct Collection;
    struct Instance;

    const uint32_t EULER_PENDING = 1;
    const uint32_t EULER_WRITTEN = 2;

    struct EulerRotation
    {
        dmVMath::Vector3 m_Value;
        uint32_t m_InstanceIndex;
        uint32_t m_State;
        uint32_t m_NextFree;
    };

    // Sparse property state owned by the collection. Animation bindings use slots
    // so growing the pool does not invalidate them.
    struct EulerRotationPool
    {
        dmArray<EulerRotation> m_Rotations;
        dmHashTable<HGameObject, uint32_t> m_InstanceToIndex;
        uint32_t m_FirstFreeIndex;
        uint32_t m_PendingCount;
    };

    inline void MarkEulerRotationPending(EulerRotationPool* pool, EulerRotation* euler)
    {
        if (!(euler->m_State & EULER_PENDING))
            ++pool->m_PendingCount;
        euler->m_State |= EULER_PENDING | EULER_WRITTEN;
    }

    // Borrowed until the next Euler pool growth. Records retain axis values until
    // instance deletion, including after animations stop, for partial Euler setters.
    EulerRotation* GetEulerRotation(Collection* collection, Instance* instance, bool create);
    void ReleaseEulerRotation(Collection* collection, HGameObject instance);
    void RestoreEulerRotation(Collection* collection, Instance* instance, const dmVMath::Vector3& value, uint32_t state);
    dmVMath::Vector3 GetEulerProperty(Collection* collection, Instance* instance);
    void UpdateEulerToRotation(Collection* collection, EulerRotation* euler);
    void CommitPendingEulerRotation(Collection* collection, Instance* instance);

    // Synchronize buffered property writes before a collection-wide transform refresh.
    void CommitPendingEulerRotations(Collection* collection);
    // Commit the batch before callbacks, retaining precedence over later quaternion writes.
    void CommitEulerRotations(Collection* collection);
}

#endif // DM_GAMEOBJECT_TRANSFORM_PRIVATE_H
