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


#include "comp_anim.h"
#include "gameobject_transform_private.h"

#include <dlib/easing_private.h>
#include <dlib/index_pool.h>
#include <dlib/inline.h>
#include <dlib/object_pool.h>
#include <dlib/profile.h>
#include <dlib/static_assert.h>

#include <stddef.h>

#include <script/script.h>

#include "component.h"
#include "gameobject_private.h"
#include "gameobject_script.h"
#include "gameobject_props.h"
#include "gameobject_props_lua.h"

extern "C"
{
#include <lua/lauxlib.h>
#include <lua/lualib.h>
}

DM_PROPERTY_EXTERN(rmtp_GameObject);
DM_PROPERTY_U32(rmtp_ComponentsAnim, 0, PROFILE_PROPERTY_FRAME_RESET, "#", &rmtp_GameObject);

namespace dmGameObject
{
#define INVALID_INDEX 0xffffffffu
#define MIN_CAPACITY_GROWTH 2048u

    static const dmhash_t PROP_EULER_X = dmHashString64("euler.x");
    static const dmhash_t PROP_EULER_Y = dmHashString64("euler.y");
    static const dmhash_t PROP_EULER_Z = dmHashString64("euler.z");

    // Optional payload owned by one Animation through m_ExtrasIndex. The pool
    // allocates this only for custom curves, release callbacks or stop callbacks;
    // ordinary built-in curves need only m_EasingType in the animation record.
    struct AnimationExtras
    {
        dmEasing::Curve  m_Easing;
        AnimationStopped m_AnimationStopped;
        void*            m_Userdata1;
        void*            m_Userdata2;
        uint32_t         m_PreviousListener;
        uint32_t         m_NextListener;
    };

    // All records in a collection live in one dense AoS array, m_Animations.
    // Animate() appends a scalar property track, or a composite parent followed
    // by its scalar component tracks. The parent advances time and owns the
    // shared callback/curve lifetime; only the component tracks write properties.
    // For a Vector3 this initially gives [parent, x, y, z], which can fill one
    // four-lane batch. SIMD batching never reorders records.
    //
    // Array growth can relocate records; swap-erasure changes their positions.
    // Lifecycle lists use stable IDs: m_AnimMap[m_Index] resolves an ID to its
    // current array position. m_Next links tracks on the same instance; listener
    // lists in AnimationExtras use the same IDs. SIMD runs use physical positions.
    struct Animation
    {
        HInstance           m_Instance;
        dmhash_t            m_ComponentId;
        dmhash_t            m_PropertyId;
        // Euler tracks use stable pool slots; other tracks retain property pointers.
        union
        {
            float*          m_Value;
            uint32_t        m_EulerSlot;
        };
        float               m_From;
        float               m_To;
        float               m_Cursor;
        float               m_Duration;
        float               m_InvDuration;
        float               m_Delay;
        uint32_t            m_Index;
        uint32_t            m_Next;
        // Independent pool slot, or INVALID_INDEX when no optional data is owned.
        uint32_t            m_ExtrasIndex;
        uint16_t            m_Playback : 3;
        uint16_t            m_Playing : 1;
        uint16_t            m_Finished : 1;
        uint16_t            m_Composite : 1;
        uint16_t            m_Backwards : 1;
        uint16_t            m_FirstUpdate : 1;
        uint16_t            m_IsGameObjectTransformProperty : 1;
        uint16_t            m_IsGameObjectEulerProperty : 1;
        uint16_t            m_EulerChannel : 2;
        // Eligible tracks have built-in easing, looping playback, positive
        // duration and direct property writes (or are Vector3 parents). Euler
        // tracks are excluded. Eligibility is fixed at creation; readiness also
        // requires a playing, activated track with no delay. RebuildAnimationRuns
        // sets readiness; StopAnimation clears it immediately, even from a setter.
        uint16_t            m_SimdEligible : 1;
        uint16_t            m_SimdReady : 1;
        uint8_t             m_EasingType;
    };

    // UpdateAnimationSimd4 loads these four fields as one register before transposing.
    DM_STATIC_ASSERT(offsetof(Animation, m_To) == offsetof(Animation, m_From) + sizeof(float), Animation_To_Layout);
    DM_STATIC_ASSERT(offsetof(Animation, m_Cursor) == offsetof(Animation, m_From) + 2 * sizeof(float), Animation_Cursor_Layout);
    DM_STATIC_ASSERT(offsetof(Animation, m_Duration) == offsetof(Animation, m_From) + 3 * sizeof(float), Animation_Duration_Layout);

    // A half-open [m_Begin, m_End) range of physical indices in m_Animations.
    // Each run contains a multiple of four consecutive ready tracks. Batches
    // may mix instances, targets, durations, playback and built-in easing curves.
    // Runs are a cached view of the array, not separate animation storage.
    //
    // Example: [6 ready tracks][1 scalar track][4 ready tracks] produces runs
    // [0, 4) and [7, 11). Indices 4..6 are evaluated individually in array order.
    // Euler tracks, custom easing curves (TYPE_FLOAT_VECTOR), once playback and
    // other ineligible tracks stay in the same array and use scalar evaluation,
    // as do short tails.
    struct AnimationRun
    {
        uint32_t m_Begin;
        uint32_t m_End;
    };

    struct AnimWorld
    {
        dmArray<Animation>                   m_Animations;
        dmArray<AnimationRun>                m_SimdRuns;
        uint32_t                             m_SimdRunsDirty : 1;
        uint32_t                             m_HasAnimationSetters : 1;
        dmObjectPool<AnimationExtras>        m_AnimationExtras;
        dmArray<uint32_t>                    m_AnimMap;
        dmIndexPool<uint32_t>                m_AnimMapIndexPool;
        // Per-instance and per-listener list heads are stable animation IDs.
        dmHashTable<HGameObject, uint32_t>   m_InstanceToIndex;
        dmHashTable<uintptr_t, uint32_t>     m_ListenerInstanceToIndex;
        uint32_t                             m_InUpdate : 1;
        // Lower bounds on physical indices needing activation or pruning;
        // INVALID_INDEX means there is no pending work for that pass. These
        // bounds may include records needing no work, so each pass still checks
        // record state. Swap-erasure keeps them conservative without reordering
        // the scans or callbacks.
        uint32_t                             m_FirstPendingAnimation;
        uint32_t                             m_FirstStoppedAnimation;
    };

    static uint32_t GetAnimationCapacity(uint32_t capacity, uint32_t min_growth)
    {
        // dmArray uses 32-bit byte sizes; both animation pools must fit that limit.
        const uint32_t element_size = dmMath::Max((uint32_t)sizeof(Animation), (uint32_t)sizeof(AnimationExtras));
        const uint32_t max_capacity = 0xffffffffu / element_size;
        const uint32_t growth = dmMath::Max(min_growth, capacity / 2);
        return capacity + dmMath::Min(growth, max_capacity - capacity);
    }

    static void SetAnimationCapacity(AnimWorld* world, uint32_t capacity)
    {
        world->m_Animations.SetCapacity(capacity);
        world->m_AnimMap.EnsureSize(capacity);
        world->m_AnimMapIndexPool.SetCapacity(capacity);
    }

    static bool IsAnimationSetter(const Animation* anim)
    {
        return anim->m_Playing && !anim->m_Composite && !anim->m_IsGameObjectEulerProperty && anim->m_Value == 0;
    }

    // Cache execution spans after membership changes. Records and callback order
    // stay unchanged, and steady-state SIMD loops need no per-track state checks.
    static void RebuildAnimationRuns(AnimWorld* world)
    {
        world->m_SimdRuns.SetSize(0);
        world->m_HasAnimationSetters = 0;
        const uint32_t size = world->m_Animations.Size();
        uint32_t begin = 0;
        bool pending_delays = false;
        for (uint32_t i = 0; i <= size; ++i)
        {
            if (i < size)
            {
                Animation& anim = world->m_Animations[i];
                const bool active = anim.m_SimdEligible && anim.m_Playing && !anim.m_FirstUpdate;
                anim.m_SimdReady = active && anim.m_Delay == 0.0f;
                pending_delays |= active && !anim.m_SimdReady;
                // Setters can cancel later tracks while evaluating. Such collections
                // validate each four-track batch instead of trusting cached spans.
                world->m_HasAnimationSetters |= IsAnimationSetter(&anim);
                if (anim.m_SimdReady)
                {
                    continue;
                }
            }
            // Keep complete batches; the remaining zero to three tracks stay scalar.
            const uint32_t count = (i - begin) & ~3u;
            if (count != 0)
            {
                if (world->m_SimdRuns.Full())
                {
                    world->m_SimdRuns.OffsetCapacity(dmMath::Max(16u, world->m_SimdRuns.Capacity() / 2));
                }
                AnimationRun run = { begin, begin + count };
                world->m_SimdRuns.Push(run);
            }
            begin = i + 1;
        }
        // A newly activated track may consume a partial-frame delay in evaluation.
        // Revisit it next frame, without maintaining readiness in the arithmetic loop.
        world->m_SimdRunsDirty = pending_delays;
    }

    // Remove a stopped record. A scalar tail preserves all spans; a swap that moves
    // a ready track changes its physical index and invalidates the execution cache.
    static void EraseAnimation(AnimWorld* world, uint32_t index)
    {
        world->m_SimdRunsDirty |= world->m_Animations.Back().m_SimdReady;
        Animation& moved = world->m_Animations.EraseSwap(index);
        if (index < world->m_Animations.Size())
        {
            world->m_AnimMap[moved.m_Index] = index;
            if (moved.m_Playing && moved.m_FirstUpdate)
            {
                world->m_FirstPendingAnimation = dmMath::Min(world->m_FirstPendingAnimation, index);
            }
            if (!moved.m_Playing)
            {
                world->m_FirstStoppedAnimation = dmMath::Min(world->m_FirstStoppedAnimation, index);
            }
        }
    }

    static AnimationExtras* GetAnimationExtras(AnimWorld* world, const Animation* anim)
    {
        return anim->m_ExtrasIndex == INVALID_INDEX ? 0 : &world->m_AnimationExtras.Get(anim->m_ExtrasIndex);
    }

    static Animation* ReleaseAnimationExtras(AnimWorld* world, Animation* anim)
    {
        if (anim->m_ExtrasIndex == INVALID_INDEX)
        {
            return anim;
        }

        uint32_t index = anim->m_Index;
        uint32_t extras_index = anim->m_ExtrasIndex;
        // The release callback may grow or compact either pool. Keep its argument
        // on the stack, and resolve the animation again after the callback.
        dmEasing::Curve easing = world->m_AnimationExtras.Get(extras_index).m_Easing;
        if (easing.release_callback)
        {
            easing.release_callback(&easing);
        }
        world->m_AnimationExtras.Free(extras_index, false);
        anim = &world->m_Animations[world->m_AnimMap[index]];
        anim->m_ExtrasIndex = INVALID_INDEX;
        return anim;
    }

    CreateResult CompAnimNewWorld(const ComponentNewWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            AnimWorld* world = new AnimWorld();
            *params.m_World = world;
            const uint32_t anim_count = 512;
            SetAnimationCapacity(world, anim_count);
            // This is fetched from res_collection.cpp (ResCollectionCreate)
            const uint32_t instance_count = dmMath::Min((uint32_t)params.m_MaxInstances, anim_count);
            const uint32_t table_count = dmMath::Max(1u, instance_count/3);
            world->m_InstanceToIndex.SetCapacity(table_count, instance_count);
            world->m_ListenerInstanceToIndex.SetCapacity(table_count, instance_count);
            world->m_InUpdate = 0;
            world->m_FirstPendingAnimation = INVALID_INDEX;
            world->m_FirstStoppedAnimation = INVALID_INDEX;
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    CreateResult CompAnimDeleteWorld(const ComponentDeleteWorldParams& params)
    {
        if (params.m_World != 0x0)
        {
            delete (AnimWorld*)params.m_World;
            return CREATE_RESULT_OK;
        }
        else
        {
            return CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    static void StopAnimation(AnimWorld* world, Animation* anim, bool finished)
    {
        world->m_SimdRunsDirty |= anim->m_SimdReady || IsAnimationSetter(anim);
        anim->m_SimdReady = 0;
        anim->m_Finished = finished;
        anim->m_Playing = 0;
        uint32_t index = (uint32_t)(anim - world->m_Animations.Begin());
        world->m_FirstStoppedAnimation = dmMath::Min(world->m_FirstStoppedAnimation, index);
    }

    static void StopAnimations(AnimWorld* world, uint32_t* head_ptr, dmhash_t component_id, dmhash_t property_id)
    {
        if (head_ptr != 0x0)
        {
            uint32_t index = *head_ptr;
            while (index != INVALID_INDEX)
            {
                Animation* anim = &world->m_Animations[world->m_AnimMap[index]];
                if (anim->m_ComponentId == component_id && (property_id == 0 || anim->m_PropertyId == property_id))
                {
                    StopAnimation(world, anim, false);
                }
                index = anim->m_Next;
            }
        }
    }

    static void StopAllAnimations(AnimWorld* world, uint32_t* head_ptr)
    {
        if (head_ptr != 0x0)
        {
            uint32_t index = *head_ptr;
            while (index != INVALID_INDEX)
            {
                Animation* anim = &world->m_Animations[world->m_AnimMap[index]];
                StopAnimation(world, anim, false);
                index = anim->m_Next;
            }
        }
    }

    static void RemoveAnimationCallback(AnimWorld* world, Animation* anim);

    CreateResult CompAnimAddToUpdate(const ComponentAddToUpdateParams& params) {
        // Intentional pass-through
        return CREATE_RESULT_OK;
    }

    // Four consecutive active loop tracks, including Vector3 composite parents.
    // Storage stays AoS; only these temporary registers are transposed.
    static bool UpdateAnimationSimd4(Animation* anim, float dt)
    {
        // 1. Pack four AoS records into vectors with one animation per lane.
        // m_From, m_To, m_Cursor and m_Duration must stay consecutive in Animation.
        // Before transposing, each vector is one record; afterwards, it is one field.
        dsimd_float4_t track0 = SimdLoad(&anim[0].m_From);
        dsimd_float4_t track1 = SimdLoad(&anim[1].m_From);
        dsimd_float4_t track2 = SimdLoad(&anim[2].m_From);
        dsimd_float4_t track3 = SimdLoad(&anim[3].m_From);
        SimdTranspose(&track0, &track1, &track2, &track3);
        const dsimd_float4_t from = track0;
        const dsimd_float4_t to = track1;
        dsimd_float4_t cursor = track2;
        const dsimd_float4_t duration = track3;
        const dsimd_float4_t inv_duration = SimdMake(anim[0].m_InvDuration, anim[1].m_InvDuration,
                                                     anim[2].m_InvDuration, anim[3].m_InvDuration);
        const dsimd_uint4_t easing_types = SimdMakeUint(anim[0].m_EasingType, anim[1].m_EasingType,
                                                        anim[2].m_EasingType, anim[3].m_EasingType);
        dsimd_uint4_t backwards = SimdMakeUint(anim[0].m_Backwards, anim[1].m_Backwards,
                                               anim[2].m_Backwards, anim[3].m_Backwards);
        const dsimd_uint4_t playback = SimdMakeUint(anim[0].m_Playback, anim[1].m_Playback,
                                                    anim[2].m_Playback, anim[3].m_Playback);
        const dsimd_uint4_t pingpong_mask = SimdEqualUint(playback, SimdSplatUint(PLAYBACK_LOOP_PINGPONG));

        // 2. Most batches do not cross a loop boundary. Enter wrapping only when
        // at least one lane crosses; preserve scalar repeated-subtraction rounding.
        // The caller guarantees positive durations and nonnegative dt.
        cursor = SimdAdd(cursor, SimdSplat(dt));
        dsimd_uint4_t wrap_mask = SimdNotUint(SimdLess(cursor, duration));
        if (SimdAny(wrap_mask))
        {
            const dsimd_uint4_t original_backwards = backwards;
            do
            {
                cursor = SimdSelect(wrap_mask, SimdSub(cursor, duration), cursor);
                // Comparison masks have all bits set; direction flags need a 0/1 XOR toggle.
                dsimd_uint4_t direction_toggle = SimdAndUint(wrap_mask, pingpong_mask);
                direction_toggle = SimdAndUint(direction_toggle, SimdSplatUint(1));
                backwards = SimdXorUint(backwards, direction_toggle);
                wrap_mask = SimdNotUint(SimdLess(cursor, duration));
            } while (SimdAny(wrap_mask));

            // Direction changes only for an odd number of ping-pong crossings.
            // Keep unchanged flags out of the normal per-frame property writeback.
            if (SimdAny(SimdNotUint(SimdEqualUint(original_backwards, backwards))))
            {
                uint32_t directions[4];
                SimdStoreUint(directions, backwards);
                for (uint32_t lane = 0; lane < 4; ++lane)
                {
                    anim[lane].m_Backwards = directions[lane];
                }
            }
        }

        // 3. Map the cursor to playback progress: normalize, reverse, then fold
        // ping-pong lanes from 0..2 into 0..1..0. Other loop modes keep their progress.
        const dsimd_float4_t one = SimdSplat(1.0f);
        dsimd_float4_t progress = SimdMul(cursor, inv_duration);
        progress = SimdMin(one, SimdMax(SimdSplat(0.0f), progress));
        const dsimd_uint4_t backwards_mask = SimdEqualUint(backwards, SimdSplatUint(1));
        progress = SimdSelect(backwards_mask, SimdSub(one, progress), progress);

        dsimd_float4_t pingpong_progress = SimdMul(progress, SimdSplat(2.0f));
        const dsimd_uint4_t return_leg_mask = SimdLess(one, pingpong_progress);
        const dsimd_float4_t return_progress = SimdSub(SimdSplat(2.0f), pingpong_progress);
        pingpong_progress = SimdSelect(return_leg_mask, return_progress, pingpong_progress);
        progress = SimdSelect(pingpong_mask, pingpong_progress, progress);

        // 4. Each lane may use a different built-in curve. Interpolate from the
        // captured start value using the same multiply-add order as scalar evaluation.
        const dsimd_float4_t eased_progress = dmEasing::GetValue4Normalized(easing_types, progress);
        const dsimd_float4_t distance = SimdSub(to, from);
        const dsimd_float4_t values = SimdMulAdd(distance, eased_progress, from);

        // 5. Store cursors back into the AoS records and scatter results to their
        // properties. Looping tracks do not generate completion events.
        float cursors[4];
        float output[4];
        SimdStore(cursors, cursor);
        SimdStore(output, values);
        for (uint32_t lane = 0; lane < 4; ++lane)
        {
            anim[lane].m_Cursor = cursors[lane];
            // Composite parent lanes advance time but own no property to write.
            if (anim[lane].m_Value)
            {
                *anim[lane].m_Value = output[lane];
            }
        }
        return anim[0].m_IsGameObjectTransformProperty | anim[1].m_IsGameObjectTransformProperty |
               anim[2].m_IsGameObjectTransformProperty | anim[3].m_IsGameObjectTransformProperty;
    }

    // Apply one calculated value, preserving setter order and deferred Euler commits.
    static DM_FORCE_INLINE bool ApplyAnimationValue(Collection* collection, Animation* anim, float v)
    {
        if (anim->m_IsGameObjectEulerProperty)
        {
            // Unchanged Euler tracks must not overwrite quaternion animations.
            EulerRotation& euler = collection->m_EulerRotations.m_Rotations[anim->m_EulerSlot];
            float* value = (float*)&euler.m_Value + anim->m_EulerChannel;
            bool changed = memcmp(value, &v, sizeof(v)) != 0;
            *value = v;
            if (changed)
            {
                MarkEulerRotationPending(&collection->m_EulerRotations, &euler);
            }
            return changed;
        }
        if (anim->m_Value != 0x0)
        {
            *anim->m_Value = v;
        }
        else
        {
            PropertyOptions property_opt;
            property_opt.m_IsAnimation = true;
            AddPropertyOptionsIndex(&property_opt, 0);
            SetProperty(anim->m_Instance, anim->m_ComponentId, anim->m_PropertyId, property_opt, PropertyVar(v));
        }
        return false;
    }

    struct AnimationUpdateChanges
    {
        bool m_Transforms;
        bool m_Euler;
    };

    // Update one track that is outside a SIMD run, including delayed, custom-curve,
    // Euler and once-playback tracks. Completion callbacks remain in finalization.
    // Keep this in the traversal loop to avoid a function call for every scalar track.
    static DM_FORCE_INLINE void UpdateAnimationScalar(AnimWorld* world, Collection* collection, Animation& anim, float dt, AnimationUpdateChanges& changes)
    {
        // Ignore canceled or delayed animations
        if (!anim.m_Playing)
        {
            return;
        }
        if (anim.m_Delay > dt)
        {
            anim.m_Delay -= dt;
            return;
        }
        // Consume the delay without scheduling work in the arithmetic loop.
        const float delay = anim.m_Delay;
        dt -= delay;
        anim.m_Delay = 0.0f;
        // Advance cursor
        if (anim.m_Playback != PLAYBACK_NONE)
        {
            anim.m_Cursor += dt;
        }
        // Adjust cursor
        bool completed = false;
        switch (anim.m_Playback)
        {
            case PLAYBACK_ONCE_FORWARD:
            case PLAYBACK_ONCE_BACKWARD:
            case PLAYBACK_ONCE_PINGPONG:
            {
                if (anim.m_Cursor >= anim.m_Duration)
                {
                    anim.m_Cursor = anim.m_Duration;
                    completed = true;
                }
                break;
            }
            case PLAYBACK_LOOP_FORWARD:
            case PLAYBACK_LOOP_BACKWARD:
            {
                if (anim.m_Duration > 0)
                {
                    while (anim.m_Cursor >= anim.m_Duration)
                    {
                        anim.m_Cursor -= anim.m_Duration;
                    }
                }
                break;
            }
            case PLAYBACK_LOOP_PINGPONG:
            {
                if (anim.m_Duration > 0)
                {
                    while (anim.m_Cursor >= anim.m_Duration)
                    {
                        anim.m_Cursor -= anim.m_Duration;
                        anim.m_Backwards = ~anim.m_Backwards;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }

        // Evaluate animation
        if (!anim.m_Composite)
        {
            float t = 1.0f;
            if (anim.m_Cursor < anim.m_Duration)
            {
                t = dmMath::Clamp(anim.m_Cursor * anim.m_InvDuration, 0.0f, 1.0f);
            }
            if (anim.m_Backwards)
            {
                t = 1.0f - t;
            }
            if (anim.m_Playback == PLAYBACK_ONCE_PINGPONG || anim.m_Playback == PLAYBACK_LOOP_PINGPONG)
            {
                t *= 2.0f;
                if (t > 1.0f)
                {
                    t = 2.0f - t;
                }
            }
            t = anim.m_EasingType == dmEasing::TYPE_FLOAT_VECTOR
                ? dmEasing::GetValue(world->m_AnimationExtras.Get(anim.m_ExtrasIndex).m_Easing, t)
                : dmEasing::GetValue((dmEasing::Type)anim.m_EasingType, t);
            float v = anim.m_From + (anim.m_To - anim.m_From) * t;
            changes.m_Euler |= ApplyAnimationValue(collection, &anim, v);
            changes.m_Transforms |= anim.m_IsGameObjectTransformProperty;
        }
        if (completed)
        {
            StopAnimation(world, &anim, true);
        }
    }

    // Setters can stop later tracks, so inspect all four just before evaluating them.
    static bool CanUpdateAnimationSimd4(const Animation* animations, uint32_t count)
    {
        return count >= 4 && (animations[0].m_SimdReady & animations[1].m_SimdReady &
                              animations[2].m_SimdReady & animations[3].m_SimdReady);
    }

    // Walk the array in physical order, interleaving scalar work and SIMD runs.
    // UseBatches is selected once per collection/frame and requires an executable
    // four-track batch and nonnegative dt. Without one, compile out batch dispatch.
    // This does not select a hardware backend: UpdateAnimationSimd4 uses the same
    // Simd* operations with native vectors or scalar primitives. Individual-track
    // evaluation handles unsupported tracks and tails in either case.
    template <bool UseBatches>
    static AnimationUpdateChanges EvaluateAnimations(AnimWorld* world, Collection* collection, uint32_t size, float frame_dt)
    {
        AnimationUpdateChanges changes = { false, false };
        // A setter may cancel a later track during this walk. In that case check
        // all four records just before each batch, rather than trusting a run.
        const bool cached_runs = !world->m_HasAnimationSetters;
        uint32_t run_index = 0;
        uint32_t next_run = cached_runs && world->m_SimdRuns.Size() != 0 ? world->m_SimdRuns[0].m_Begin : size;
        for (uint32_t i = 0; i < size; ++i)
        {
            Animation& anim = world->m_Animations[i];
            const bool use_batch = UseBatches &&
                                   (cached_runs ? i == next_run : CanUpdateAnimationSimd4(&anim, size - i));
            if (use_batch)
            {
                const uint32_t end = cached_runs ? world->m_SimdRuns[run_index].m_End : i + 4;
                Animation* animations = world->m_Animations.Begin();
                for (uint32_t j = i; j < end; j += 4)
                {
                    changes.m_Transforms |= UpdateAnimationSimd4(&animations[j], frame_dt);
                }
                i = end - 1;
                if (cached_runs)
                {
                    ++run_index;
                    next_run = run_index < world->m_SimdRuns.Size() ? world->m_SimdRuns[run_index].m_Begin : size;
                }
            }
            else
            {
                UpdateAnimationScalar(world, collection, anim, frame_dt, changes);
            }
        }
        return changes;
    }

    UpdateResult CompAnimUpdate(const ComponentsUpdateParams& params, ComponentsUpdateResult& update_result)
    {
        const float frame_dt = params.m_UpdateContext->m_DT;
        DM_PROFILE("Update");

        /*
         * The update has three stages: activation, evaluation and finalization.
         *
         * The first pass activates tracks whose delay expires this frame and
         * captures their from-values. Delay is consumed in evaluation, so a track
         * with a partial-frame delay stays scalar until the next run rebuild.
         * Newly started animations should automatically cancel others of the
         * same property, which is why this is done in a separate pass. Otherwise
         * an ongoing animation could already have changed the property's value
         * before the newly started animation captures it as its from-value.
         *
         * After activation, rebuild dirty SIMD runs before the second pass
         * advances and evaluates the array. Unchanged runs are reused across
         * frames. Activation, stopping and swap-erasure invalidate the cache when
         * readiness or physical positions change. The arithmetic loop does not
         * reschedule tracks.
         *
         * Euler writes are committed after evaluation, once per touched instance,
         * before finalization prunes stopped animations and calls callbacks.
         *
         * The reason for this is to give consistent animation evaluation, independent of ordering.
         */
        UpdateResult result = UPDATE_RESULT_OK;
        AnimWorld* world = (AnimWorld*)params.m_World;
        Collection* collection = GetCollectionFromHandle(params.m_Collection);
        world->m_InUpdate = 1;
        uint32_t size = world->m_Animations.Size();
        bool transforms_updated = false;
        bool euler_updated = false;

        DM_PROPERTY_ADD_U32(rmtp_ComponentsAnim, size);

        if (world->m_FirstPendingAnimation != INVALID_INDEX)
        {
            const uint32_t begin = world->m_FirstPendingAnimation;
            world->m_FirstPendingAnimation = INVALID_INDEX;
            for (uint32_t i = begin; i < size; ++i)
            {
                Animation& anim = world->m_Animations[i];
                if (!anim.m_Playing || !anim.m_FirstUpdate)
                {
                    continue;
                }
                // Check delay
                if (anim.m_Delay > frame_dt)
                {
                    world->m_FirstPendingAnimation = dmMath::Min(world->m_FirstPendingAnimation, i);
                    continue;
                }
                anim.m_FirstUpdate = 0;
                world->m_SimdRunsDirty |= anim.m_SimdEligible;
                // Update from-value
                if (!anim.m_Composite)
                {
                    if (anim.m_IsGameObjectEulerProperty)
                    {
                        anim.m_From = collection->m_EulerRotations.m_Rotations[anim.m_EulerSlot].m_Value.getElem(anim.m_EulerChannel);
                    }
                    else if (anim.m_Value != 0x0)
                    {
                        anim.m_From = *anim.m_Value;
                    }
                    else
                    {
                        PropertyDesc desc;
                        PropertyOptions property_opt;
                        AddPropertyOptionsIndex(&property_opt, 0);

                        GetProperty(anim.m_Instance, anim.m_ComponentId, anim.m_PropertyId, property_opt, desc);
                        anim.m_From = (float)desc.m_Variant.m_Number;
                    }
                }
                // Cancel other currently playing animations
                uint32_t* head_ptr = world->m_InstanceToIndex.Get(anim.m_Instance);
                if (head_ptr != 0x0)
                {
                    uint32_t index = *head_ptr;
                    while (index != INVALID_INDEX)
                    {
                        uint32_t anim_index = world->m_AnimMap[index];
                        Animation* a2 = &world->m_Animations[anim_index];
                        if (anim_index != i && !a2->m_FirstUpdate && a2->m_ComponentId == anim.m_ComponentId
                            && a2->m_PropertyId == anim.m_PropertyId && a2->m_Delay <= 0.0f)
                        {
                            StopAnimation(world, a2, false);
                        }
                        index = a2->m_Next;
                    }
                }
            }
        }
        AnimationUpdateChanges changes;
        if (world->m_SimdRunsDirty && frame_dt >= 0.0f)
        {
            RebuildAnimationRuns(world);
        }
        if (world->m_SimdRuns.Size() != 0 && frame_dt >= 0.0f)
        {
            changes = EvaluateAnimations<true>(world, collection, size, frame_dt);
        }
        else
        {
            changes = EvaluateAnimations<false>(world, collection, size, frame_dt);
        }
        // Negative steps can restore delays. Refresh readiness before batching again.
        world->m_SimdRunsDirty |= frame_dt < 0.0f;
        transforms_updated = changes.m_Transforms;
        euler_updated = changes.m_Euler;
        if (transforms_updated)
        {
            collection->m_DirtyTransforms = 1;
        }
        if (euler_updated)
        {
            DM_PROFILE("EulerWriteback");
            CommitEulerRotations(collection);
        }
        // Prune canceled animations and call callbacks only when something stopped.
        if (world->m_FirstStoppedAnimation != INVALID_INDEX)
        {
            const uint32_t begin = world->m_FirstStoppedAnimation;
            world->m_FirstStoppedAnimation = INVALID_INDEX;
            for (uint32_t i = begin; i < size;)
            {
                Animation* anim = &world->m_Animations[i];
                if (!anim->m_Playing)
                {
                    AnimationExtras* extras = GetAnimationExtras(world, anim);
                    if (extras && extras->m_AnimationStopped != 0x0)
                    {
                        uint32_t orig_size = size;
                        extras->m_AnimationStopped(anim->m_Instance, anim->m_ComponentId, anim->m_PropertyId, anim->m_Finished,
                                                   extras->m_Userdata1, extras->m_Userdata2);
                        // Check if the callback added animations, in which case we need to update the pointer (possible relocation)
                        size = world->m_Animations.Size();
                        if (size != orig_size)
                        {
                            anim = &world->m_Animations[i];
                        }
                        RemoveAnimationCallback(world, anim);
                    }
                    anim = ReleaseAnimationExtras(world, anim);
                    size = world->m_Animations.Size();
                    uint32_t* head_ptr = world->m_InstanceToIndex.Get(anim->m_Instance);
                    uint32_t* index_ptr = head_ptr;
                    while (*index_ptr != INVALID_INDEX)
                    {
                        if (*index_ptr == anim->m_Index)
                        {
                            *index_ptr = anim->m_Next;
                            world->m_AnimMapIndexPool.Push(anim->m_Index);
                            break;
                        }
                        else
                        {
                            index_ptr = &world->m_Animations[world->m_AnimMap[*index_ptr]].m_Next;
                        }
                    }
                    // Remove instance when the list is empty
                    if (*head_ptr == INVALID_INDEX)
                    {
                        world->m_InstanceToIndex.Erase(anim->m_Instance);
                    }
                    // delete the instance from the list
                    EraseAnimation(world, i);
                    --size;
                }
                else
                {
                    ++i;
                }
            }
        }
        world->m_InUpdate = 0;
        update_result.m_TransformsUpdated = transforms_updated;
        return result;
    }

    static AnimWorld* GetWorld(Collection* collection)
    {
        if (!collection)
            return 0;
        HResourceType resource_type;
        dmResource::Result result = dmResource::GetTypeFromExtension(collection->m_Factory, "animc", &resource_type);
        assert(result == dmResource::RESULT_OK);
        uint32_t component_index;
        ComponentType* type = FindComponentType(collection->m_Register, resource_type, &component_index);
        assert(type != 0x0);
        return (AnimWorld*)collection->m_ComponentWorlds[component_index];
    }

    static AnimWorld* GetWorld(HCollection collection)
    {
        return GetWorld(GetCollectionFromHandle(collection));
    }

    static bool PlayAnimation(AnimWorld* world, HGameObject instance, dmhash_t component_id,
                              dmhash_t property_id,
                              Playback playback,
                              float* value,
                              float from,
                              float to,
                              dmEasing::Curve easing,
                              float duration,
                              float delay,
                              AnimationStopped animation_stopped,
                              void* userdata1, void* userdata2,
                              uint32_t composite_count)
    {
        const bool composite = composite_count != 0;
        uint32_t top = world->m_Animations.Size();
        if (world->m_Animations.Full())
        {
            uint32_t capacity = GetAnimationCapacity(world->m_Animations.Capacity(), MIN_CAPACITY_GROWTH);
            if (capacity == world->m_Animations.Capacity())
            {
                dmLogError("Animation could not be stored since the buffer reached its byte-size limit (%u).", capacity);
                return false;
            }
            SetAnimationCapacity(world, capacity);
        }
        bool needs_extras = easing.type == dmEasing::TYPE_FLOAT_VECTOR || easing.release_callback || animation_stopped;
        if (needs_extras && world->m_AnimationExtras.Full())
        {
            uint32_t capacity = GetAnimationCapacity(world->m_AnimationExtras.Capacity(), 16u);
            if (capacity == world->m_AnimationExtras.Capacity())
            {
                dmLogError("Animation extras could not be stored since the buffer reached its byte-size limit (%u).", capacity);
                return false;
            }
            world->m_AnimationExtras.SetCapacity(capacity);
        }
        uint32_t index = world->m_AnimMapIndexPool.Pop();
        uint32_t* index_ptr = world->m_InstanceToIndex.Get(instance);
        if (index_ptr == 0x0)
        {
            if (world->m_InstanceToIndex.Full())
            {
                uint32_t capacity = GetAnimationCapacity(world->m_InstanceToIndex.Capacity(), MIN_CAPACITY_GROWTH);
                world->m_InstanceToIndex.SetCapacity(dmMath::Max(1u, capacity / 3), capacity);
            }
            world->m_InstanceToIndex.Put(instance, index);
        }
        else
        {
            Animation* last_anim = &world->m_Animations[world->m_AnimMap[*index_ptr]];
            while (last_anim->m_Next != INVALID_INDEX)
            {
                last_anim = &world->m_Animations[world->m_AnimMap[last_anim->m_Next]];
            }
            last_anim->m_Next = index;
        }

        uint32_t anim_count = top + 1;
        world->m_Animations.SetSize(anim_count);

        Animation& animation = world->m_Animations[top];
        memset(&animation, 0, sizeof(Animation));

        world->m_AnimMap[index] = top;
        animation.m_Index = index;

        animation.m_Instance = instance;
        animation.m_ComponentId = component_id;
        animation.m_PropertyId = property_id;
        animation.m_Playback = playback;
        animation.m_EasingType = easing.type;
        animation.m_ExtrasIndex = INVALID_INDEX;
        if (needs_extras)
        {
            animation.m_ExtrasIndex = world->m_AnimationExtras.Alloc();
            AnimationExtras& extras = world->m_AnimationExtras.Get(animation.m_ExtrasIndex);
            extras.m_Easing = easing;
            extras.m_AnimationStopped = animation_stopped;
            extras.m_Userdata1 = userdata1;
            extras.m_Userdata2 = userdata2;
            extras.m_PreviousListener = INVALID_INDEX;
            extras.m_NextListener = INVALID_INDEX;
        }
        animation.m_Value = value;
        animation.m_From = from;
        animation.m_To = to;
        animation.m_Delay = dmMath::Max(delay, 0.0f);
        animation.m_Duration = dmMath::Max(duration, 0.0f);
        animation.m_InvDuration = 0.0f;
        if (animation.m_Duration > 0.0f)
        {
            animation.m_InvDuration = 1.0f / animation.m_Duration;
        }
        animation.m_Next = INVALID_INDEX;
        animation.m_Playing = 1;
        animation.m_Composite = composite ? 1 : 0;
        animation.m_IsGameObjectTransformProperty = !composite && component_id == 0 && IsGameObjectTransformProperty(property_id) ? 1 : 0;
        animation.m_IsGameObjectEulerProperty = component_id == 0 && !composite &&
                                                (property_id == PROP_EULER_X || property_id == PROP_EULER_Y || property_id == PROP_EULER_Z) ? 1 : 0;
        if (animation.m_IsGameObjectEulerProperty)
        {
            Collection* collection;
            Instance* target = GetInstanceFromHandle(instance, &collection);
            EulerRotation* euler = GetEulerRotation(collection, target, true);
            // Indices survive pool growth; no animation retains a pointer into the pool.
            animation.m_EulerSlot = (uint32_t)(euler - collection->m_EulerRotations.m_Rotations.Begin());
            animation.m_EulerChannel = property_id == PROP_EULER_X ? 0 : property_id == PROP_EULER_Y ? 1 : 2;
        }
        if (animation.m_Playback == PLAYBACK_ONCE_BACKWARD || animation.m_Playback == PLAYBACK_LOOP_BACKWARD)
        {
            animation.m_Backwards = 1;
        }
        // Custom vector curves have arbitrary sample counts and no padded endpoint.
        // Keep them on the scalar path without changing track or callback order.
        // A Vector3 parent fills the fourth lane in [parent, x, y, z]. It
        // advances time without writing a property. Other parents stay scalar.
        animation.m_SimdEligible = !animation.m_IsGameObjectEulerProperty && (value != 0 || composite_count == 3) &&
                                   easing.type < dmEasing::TYPE_FLOAT_VECTOR && animation.m_Duration > 0.0f &&
                                   playback >= PLAYBACK_LOOP_FORWARD && playback <= PLAYBACK_LOOP_PINGPONG;
        // Appending an unready track leaves existing spans intact. Setters change
        // whether evaluation may trust them, even before their delay expires.
        world->m_SimdRunsDirty |= IsAnimationSetter(&animation);
        animation.m_FirstUpdate = 1;
        world->m_FirstPendingAnimation = dmMath::Min(world->m_FirstPendingAnimation, top);

        if (0x0 != animation_stopped)
        {
            index_ptr = world->m_ListenerInstanceToIndex.Get((uintptr_t)userdata1);
            if (0x0 == index_ptr)
            {
                if (world->m_ListenerInstanceToIndex.Full())
                {
                    uint32_t capacity = GetAnimationCapacity(world->m_ListenerInstanceToIndex.Capacity(), MIN_CAPACITY_GROWTH);
                    world->m_ListenerInstanceToIndex.SetCapacity(dmMath::Max(1u, capacity / 3), capacity);
                }
            }
            else
            {
                Animation* last_anim = &world->m_Animations[world->m_AnimMap[*index_ptr]];
                world->m_AnimationExtras.Get(animation.m_ExtrasIndex).m_NextListener = last_anim->m_Index;
                world->m_AnimationExtras.Get(last_anim->m_ExtrasIndex).m_PreviousListener = index;
            }
            world->m_ListenerInstanceToIndex.Put((uintptr_t)userdata1, index);
        }

        return true;
    }

    static bool PlayCompositeAnimation(AnimWorld* world, HGameObject instance, dmhash_t component_id,
                                       dmhash_t property_id, Playback playback, float duration, float delay, dmEasing::Curve easing, AnimationStopped animation_stopped,
                                       void* userdata1, void* userdata2, uint32_t element_count)
    {
        return PlayAnimation(world, instance, component_id, property_id, playback, 0x0, 0, 0, easing,
                             duration, delay, animation_stopped, userdata1, userdata2, element_count);
    }

    static uint32_t GetElementCount(PropertyType type, dmhash_t* element_ids)
    {
        switch (type)
        {
        case PROPERTY_TYPE_NUMBER:
            return 1;
        case PROPERTY_TYPE_VECTOR3:
        {
            if (element_ids[2] == 0)
                return 2;
            return 3;
        }
        case PROPERTY_TYPE_VECTOR4:
        case PROPERTY_TYPE_QUAT:
            return 4;
        default:
            return 0;
        }
    }

    PropertyResult Animate(HCollection collection, HGameObject instance, dmhash_t component_id,
                     dmhash_t property_id,
                     Playback playback,
                     PropertyVar& to,
                     dmEasing::Curve easing,
                     float duration,
                     float delay,
                     AnimationStopped animation_stopped,
                     void* userdata1, void* userdata2)
    {
        PropertyDesc prop_desc;
        PropertyOptions property_opt;
        AddPropertyOptionsIndex(&property_opt, 0);

        if (collection == 0 || GetCollection(instance) != collection)
            return PROPERTY_RESULT_INVALID_INSTANCE;

        PropertyResult prop_result = GetProperty(instance, component_id, property_id, property_opt, prop_desc);
        if (prop_result != PROPERTY_RESULT_OK)
        {
            return prop_result;
        }
        if (prop_desc.m_ReadOnly)
        {
            return PROPERTY_RESULT_UNSUPPORTED_OPERATION;
        }
        if (to.m_Type != prop_desc.m_Variant.m_Type)
        {
            // If user wants to animate a vector property with scalar "to" value, create "to" vector from scalar value
            if (to.m_Type == PROPERTY_TYPE_NUMBER && ((prop_desc.m_Variant.m_Type == PROPERTY_TYPE_VECTOR3) || prop_desc.m_Variant.m_Type == PROPERTY_TYPE_VECTOR4))
            {
                float val = to.m_Number;
                PropertyVar prop_var = (prop_desc.m_Variant.m_Type == PROPERTY_TYPE_VECTOR3) ? PropertyVar(Vector3(val)) : PropertyVar(Vector4(val));
                to = PropertyVar(prop_var);
            }
            else
            {
                return PROPERTY_RESULT_TYPE_MISMATCH;
            }
        }
        uint32_t element_count = GetElementCount(prop_desc.m_Variant.m_Type, prop_desc.m_ElementIds);
        if (element_count == 0)
        {
            return PROPERTY_RESULT_UNSUPPORTED_TYPE;
        }
        AnimWorld* world = GetWorld(collection);

        if (element_count > 1)
        {
            if (!PlayCompositeAnimation(world, instance, component_id, property_id, playback,
                                        duration, delay, easing, animation_stopped, userdata1, userdata2, element_count))
            {
                return PROPERTY_RESULT_BUFFER_OVERFLOW;
            }

            // Clear the release_callback for element animation to make sure we only call it once in the composite animation
            easing.release_callback = 0x0;
            float* v = prop_desc.m_Variant.m_V4;
            for (uint32_t i = 0; i < element_count; ++i)
            {
                float* val_ptr = 0x0;
                if (prop_desc.m_ValuePtr != 0x0)
                    val_ptr = prop_desc.m_ValuePtr + i;
                if (!PlayAnimation(world, instance, component_id, prop_desc.m_ElementIds[i], playback, val_ptr,
                                   *(v + i), to.m_V4[i], easing, duration, delay, 0x0, 0x0, 0x0, 0))
                {
                    return PROPERTY_RESULT_BUFFER_OVERFLOW;
                }
            }
        }
        else
        {
            if (!PlayAnimation(world, instance, component_id, property_id, playback, prop_desc.m_ValuePtr,
                               (float)prop_desc.m_Variant.m_Number, (float)to.m_Number, easing, duration, delay, animation_stopped,
                               userdata1, userdata2, 0))
            {
                return PROPERTY_RESULT_BUFFER_OVERFLOW;
            }
        }
        return PROPERTY_RESULT_OK;
    }

    PropertyResult CancelAnimations(HCollection collection, HGameObject instance, dmhash_t component_id,
                     dmhash_t property_id)
    {
        if (property_id == 0)
        {
            if (collection == 0 || GetCollection(instance) != collection)
                return PROPERTY_RESULT_INVALID_INSTANCE;
            AnimWorld* world = GetWorld(collection);
            StopAnimations(world, world->m_InstanceToIndex.Get(instance), component_id, 0);
            return PROPERTY_RESULT_OK;
        }

        PropertyDesc prop_desc;
        PropertyOptions property_opt;
        AddPropertyOptionsIndex(&property_opt, 0);
        if (collection == 0 || GetCollection(instance) != collection)
            return PROPERTY_RESULT_INVALID_INSTANCE;
        PropertyResult prop_result = GetProperty(instance, component_id, property_id, property_opt, prop_desc);
        if (prop_result != PROPERTY_RESULT_OK)
        {
            return prop_result;
        }
        uint32_t element_count = GetElementCount(prop_desc.m_Variant.m_Type, prop_desc.m_ElementIds);
        if (element_count == 0)
        {
            return PROPERTY_RESULT_UNSUPPORTED_TYPE;
        }

        AnimWorld* world = GetWorld(collection);
        uint32_t* head_ptr = world->m_InstanceToIndex.Get(instance);
        StopAnimations(world, head_ptr, component_id, property_id);
        if (element_count > 1)
        {
            for (uint32_t i = 0; i < element_count; ++i)
            {
                StopAnimations(world, head_ptr, component_id, prop_desc.m_ElementIds[i]);
            }
        }
        return PROPERTY_RESULT_OK;
    }

    static void CancelAnimations(AnimWorld* world, HGameObject instance)
    {
        // Deferred cancel while in update
        if (world->m_InUpdate)
        {
            StopAllAnimations(world, world->m_InstanceToIndex.Get(instance));
        }
        else
        {
            uint32_t* head_ptr = world->m_InstanceToIndex.Get(instance);
            if (head_ptr != 0x0)
            {
                uint32_t index = *head_ptr;
                while (index != INVALID_INDEX)
                {
                    uint32_t anim_index = world->m_AnimMap[index];
                    Animation* anim = &world->m_Animations[anim_index];
                    StopAnimation(world, anim, false);
                    AnimationExtras* extras = GetAnimationExtras(world, anim);
                    if (extras && extras->m_AnimationStopped != 0x0)
                    {
                        extras->m_AnimationStopped(anim->m_Instance, anim->m_ComponentId, anim->m_PropertyId, anim->m_Finished,
                                                   extras->m_Userdata1, extras->m_Userdata2);
                        anim = &world->m_Animations[world->m_AnimMap[index]];
                        RemoveAnimationCallback(world, anim);
                    }
                    anim = ReleaseAnimationExtras(world, anim);
                    world->m_AnimMapIndexPool.Push(index);
                    index = anim->m_Next;
                    // delete the instance from the list
                    anim_index = (uint32_t)(anim - world->m_Animations.Begin());
                    EraseAnimation(world, anim_index);
                }
                world->m_InstanceToIndex.Erase(instance);
            }
        }
    }

    void CancelAnimations(Collection* collection, HGameObject instance)
    {
        CancelAnimations(GetWorld(collection), instance);
    }

    void CancelAnimations(HCollection collection, HGameObject instance)
    {
        Collection* collection_ptr = GetCollectionFromHandle(collection);
        if (collection_ptr && GetCollection(instance) == collection)
            CancelAnimations(collection_ptr, instance);
    }

    static void RemoveAnimationCallback(AnimWorld* world, Animation* anim)
    {
        AnimationExtras& extras = world->m_AnimationExtras.Get(anim->m_ExtrasIndex);
        uint32_t previous = extras.m_PreviousListener;
        uint32_t next = extras.m_NextListener;

        if (INVALID_INDEX != previous)
        {
            uint32_t anim_index_prev = world->m_AnimMap[previous];
            world->m_AnimationExtras.Get(world->m_Animations[anim_index_prev].m_ExtrasIndex).m_NextListener = next;
        }
        if (INVALID_INDEX != next)
        {
            uint32_t anim_index_next = world->m_AnimMap[next];
            world->m_AnimationExtras.Get(world->m_Animations[anim_index_next].m_ExtrasIndex).m_PreviousListener = previous;
        }
        if (INVALID_INDEX == previous)
        {
            if (INVALID_INDEX == next)
            {
                uint32_t* p = world->m_ListenerInstanceToIndex.Get((uintptr_t)extras.m_Userdata1);
                if (p != 0)
                {
                    world->m_ListenerInstanceToIndex.Erase((uintptr_t)extras.m_Userdata1);
                }
            }
            else
            {
                world->m_ListenerInstanceToIndex.Put((uintptr_t)extras.m_Userdata1, next);
            }
        }

        extras.m_PreviousListener = INVALID_INDEX;
        extras.m_NextListener = INVALID_INDEX;
        extras.m_AnimationStopped = 0x0;
        extras.m_Userdata1 = 0x0;
        extras.m_Userdata2 = 0x0;

    }

    void CancelAnimationCallbacks(HCollection collection, void* userdata1)
    {
        AnimWorld* const world = GetWorld(collection);
        if (!world)
            return;
        uint32_t* head_ptr = world->m_ListenerInstanceToIndex.Get((uintptr_t)userdata1);
        if (0x0 != head_ptr)
        {
            uint32_t index = *head_ptr;
            while (INVALID_INDEX != index)
            {
                uint32_t anim_index = world->m_AnimMap[index];
                Animation* const anim = &world->m_Animations[anim_index];
                AnimationExtras& extras = world->m_AnimationExtras.Get(anim->m_ExtrasIndex);

                index = extras.m_NextListener;

                extras.m_PreviousListener = INVALID_INDEX;
                extras.m_NextListener = INVALID_INDEX;
                extras.m_AnimationStopped = 0x0;
                extras.m_Userdata1 = 0x0;
                extras.m_Userdata2 = 0x0;
            }
            world->m_ListenerInstanceToIndex.Erase((uintptr_t)userdata1);
        }
    }
}
