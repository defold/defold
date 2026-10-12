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

#include "gameobject_private.h"
#include "gameobject_transform_private.h"

namespace dmGameObject
{
    EulerRotation* GetEulerRotation(Collection* collection, Instance* instance, bool create)
    {
        EulerRotationPool* pool = &collection->m_EulerRotations;
        HGameObject handle = GetInstanceHandle(collection, instance);
        uint32_t* index_ptr = pool->m_InstanceToIndex.Get(handle);
        if (index_ptr)
            return &pool->m_Rotations[*index_ptr];
        if (!create)
            return 0;

        uint32_t index = pool->m_FirstFreeIndex;
        if (index != INVALID_INSTANCE_INDEX)
        {
            pool->m_FirstFreeIndex = pool->m_Rotations[index].m_NextFree;
        }
        else
        {
            if (pool->m_Rotations.Full())
            {
                uint32_t capacity = pool->m_Rotations.Capacity();
                capacity = dmMath::Min(collection->m_Instances.Capacity(), capacity + dmMath::Max(512u, capacity / 2));
                pool->m_Rotations.SetCapacity(capacity);
                pool->m_InstanceToIndex.SetCapacity(dmMath::Max(1u, capacity / 3), capacity);
            }
            index = pool->m_Rotations.Size();
            pool->m_Rotations.SetSize(index + 1);
        }
        EulerRotation& euler = pool->m_Rotations[index];
        // Reads do not allocate records. Initialize all axes when a writer first
        // needs retained Euler state, including scalar animations and setters.
        Quat rotation = collection->m_LocalTransforms[instance->m_Index].GetRotation();
        euler.m_Value = dmVMath::QuatToEuler(rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW());
        euler.m_InstanceIndex = instance->m_Index;
        euler.m_State = 0;
        euler.m_NextFree = INVALID_INSTANCE_INDEX;
        pool->m_InstanceToIndex.Put(handle, index);
        return &euler;
    }

    void ReleaseEulerRotation(Collection* collection, HGameObject instance)
    {
        EulerRotationPool* pool = &collection->m_EulerRotations;
        uint32_t* index_ptr = pool->m_InstanceToIndex.Get(instance);
        if (!index_ptr)
            return;
        uint32_t index = *index_ptr;
        EulerRotation& euler = pool->m_Rotations[index];
        if (euler.m_State & EULER_PENDING)
            --pool->m_PendingCount;
        euler.m_State = 0;
        euler.m_InstanceIndex = INVALID_INSTANCE_INDEX;
        euler.m_NextFree = pool->m_FirstFreeIndex;
        pool->m_FirstFreeIndex = index;
        pool->m_InstanceToIndex.Erase(instance);
    }

    void RestoreEulerRotation(Collection* collection, Instance* instance, const Vector3& value, uint32_t state)
    {
        EulerRotation* euler = GetEulerRotation(collection, instance, false);
        if (!euler)
            return;
        EulerRotationPool* pool = &collection->m_EulerRotations;
        if (euler->m_State & EULER_PENDING)
            --pool->m_PendingCount;
        if (state & EULER_PENDING)
            ++pool->m_PendingCount;
        euler->m_Value = value;
        euler->m_State = state;
    }

    Vector3 GetEulerProperty(Collection* collection, Instance* instance)
    {
        EulerRotation* euler = GetEulerRotation(collection, instance, false);
        if (euler && (euler->m_State & EULER_PENDING))
            return euler->m_Value;

        Quat q = collection->m_LocalTransforms[instance->m_Index].GetRotation();
        Vector3 value = dmVMath::QuatToEuler(q.getX(), q.getY(), q.getZ(), q.getW());
        if (euler)
            euler->m_Value = value;
        return value;
    }

    void UpdateEulerToRotation(Collection* collection, EulerRotation* euler)
    {
        collection->m_LocalTransforms[euler->m_InstanceIndex].SetRotation(dmVMath::EulerToQuat(euler->m_Value));
        if (euler->m_State & EULER_PENDING)
            --collection->m_EulerRotations.m_PendingCount;
        // Preserve the written marker so the final pass retains Euler precedence over quaternion animation.
        euler->m_State &= ~EULER_PENDING;
    }

    void CommitPendingEulerRotation(Collection* collection, Instance* instance)
    {
        EulerRotation* euler = GetEulerRotation(collection, instance, false);
        if (euler && (euler->m_State & EULER_PENDING))
            UpdateEulerToRotation(collection, euler);
    }

    void CommitPendingEulerRotations(Collection* collection)
    {
        EulerRotationPool* pool = &collection->m_EulerRotations;
        if (!pool->m_PendingCount)
            return;

        for (uint32_t i = 0; i < pool->m_Rotations.Size(); ++i)
        {
            EulerRotation& euler = pool->m_Rotations[i];
            if (euler.m_State & EULER_PENDING)
                UpdateEulerToRotation(collection, &euler);
        }
    }

    void CommitEulerRotations(Collection* collection)
    {
        EulerRotationPool* pool = &collection->m_EulerRotations;
        for (uint32_t i = 0; i < pool->m_Rotations.Size(); ++i)
        {
            EulerRotation& euler = pool->m_Rotations[i];
            if (euler.m_State & EULER_WRITTEN)
            {
                UpdateEulerToRotation(collection, &euler);
                euler.m_State = 0;
            }
        }
    }
}
