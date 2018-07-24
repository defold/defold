#include "physics.h"
#include "physics_private.h"

#include <stdint.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>

namespace dmPhysics
{
    using namespace Vectormath::Aos;

    const char* PHYSICS_SOCKET_NAME = "@physics";

    NewContextParams::NewContextParams()
    : m_Gravity(0.0f, -10.0f, 0.0f)
    , m_WorldCount(4)
    , m_Scale(1.0f)
    , m_ContactImpulseLimit(0.0f)
    , m_TriggerEnterLimit(0.0f)
    , m_RayCastLimit2D(0)
    , m_RayCastLimit3D(0)
    , m_TriggerOverlapCapacity(0)
    {

    }

    static const float WORLD_EXTENT = 1000.0f;

    NewWorldParams::NewWorldParams()
    : m_WorldMin(-WORLD_EXTENT, -WORLD_EXTENT, -WORLD_EXTENT)
    , m_WorldMax(WORLD_EXTENT, WORLD_EXTENT, WORLD_EXTENT)
    , m_GetWorldTransformCallback(0x0)
    , m_SetWorldTransformCallback(0x0)
    {

    }

    CollisionObjectData::CollisionObjectData()
    : m_UserData(0x0)
    , m_Type(COLLISION_OBJECT_TYPE_DYNAMIC)
    , m_Mass(1.0f)
    , m_Friction(0.5f)
    , m_Restitution(0.0f)
    , m_LinearDamping(0.0f)
    , m_AngularDamping(0.0f)
    , m_Group(1)
    , m_Mask(1)
    , m_LockedRotation(0)
    , m_Enabled(1)
    {

    }

    StepWorldContext::StepWorldContext()
    {
        memset(this, 0, sizeof(*this));
    }

    RayCastRequest::RayCastRequest()
    : m_From(0.0f, 0.0f, 0.0f)
    , m_To(0.0f, 0.0f, 0.0f)
    , m_IgnoredUserData((void*)~0) // unlikely user data to ignore
    , m_UserData(0x0)
    , m_Mask(~0)
    , m_UserId(0)
    {

    }

    RayCastResponse::RayCastResponse()
    : m_Fraction(1.0f)
    , m_Position(0.0f, 0.0f, 0.0f)
    , m_Normal(0.0f, 0.0f, 0.0f)
    , m_CollisionObjectUserData(0x0)
    , m_CollisionObjectGroup(0)
    , m_Hit(0)
    {

    }

    DebugCallbacks::DebugCallbacks()
    : m_DrawLines(0x0)
    , m_DrawTriangles(0x0)
    , m_UserData(0x0)
    , m_Alpha(1.0f)
    , m_Scale(1.0f)
    , m_DebugScale(1.0f)
    {

    }

    OverlapCache::OverlapCache(uint32_t trigger_overlap_capacity)
    : m_OverlapCache()
    , m_TriggerOverlapCapacity(trigger_overlap_capacity)
    {

    }

    /**
     * Adds the overlap of an object to a specific entry.
     * Returns whether the overlap was added (might be out of space).
     */
    static bool AddOverlap(OverlapEntry* entry, void* object, bool* out_found, const uint32_t trigger_overlap_capacity)
    {
        bool found = false;
        for (uint32_t i = 0; i < entry->m_OverlapCount; ++i)
        {
            Overlap& overlap = entry->m_Overlaps[i];
            if (overlap.m_Object == object)
            {
                ++overlap.m_Count;
                found = true;
                break;
            }
        }
        if (out_found != 0x0)
            *out_found = found;
        if (!found)
        {
            if (entry->m_OverlapCount == trigger_overlap_capacity)
            {
                dmLogError("Trigger overlap capacity reached, overlap will not be stored for enter/exit callbacks.");
                return false;
            }
            else
            {
                Overlap& overlap = entry->m_Overlaps[entry->m_OverlapCount++];
                overlap.m_Object = object;
                overlap.m_Count = 1;
            }
        }
        return true;
    }

    /**
     * Remove the overlap of an object from an entry.
     */
    static void RemoveOverlap(OverlapEntry* entry, void* object)
    {
        uint32_t count = entry->m_OverlapCount;
        for (uint32_t i = 0; i < count; ++i)
        {
            Overlap& overlap = entry->m_Overlaps[i];
            if (overlap.m_Object == object)
            {
                overlap = entry->m_Overlaps[count-1];
                --entry->m_OverlapCount;
                return;
            }
        }
    }

    /**
     * Add the entry of two overlapping objects to the cache.
     * Automatically creates the overlap, but is not symmetric.
     */
    static void AddEntry(OverlapCache* cache, void* object_a, void* user_data_a, void* object_b, uint16_t group_a)
    {
        // Check if the cache needs to be expanded
        uint32_t size = cache->m_OverlapCache.Size();
        uint32_t capacity = cache->m_OverlapCache.Capacity();
        // Expand when 80% full
        if (size > 3 * capacity / 4)
        {
            capacity += CACHE_EXPANSION;
            cache->m_OverlapCache.SetCapacity(3 * capacity / 4, capacity);
        }
        OverlapEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.m_Overlaps = (Overlap*)malloc(cache->m_TriggerOverlapCapacity * sizeof(Overlap));
        entry.m_UserData = user_data_a;
        entry.m_Group = group_a;
        // Add overlap to the entry
        AddOverlap(&entry, object_b, 0x0, cache->m_TriggerOverlapCapacity);
        // Add the entry
        cache->m_OverlapCache.Put((uintptr_t)object_a, entry);
    }

    void OverlapCacheInit(OverlapCache* cache)
    {
    	cache->m_OverlapCache.SetCapacity(3 * CACHE_INITIAL_CAPACITY / 4, CACHE_INITIAL_CAPACITY);
    }

    /**
     * Sets an overlap to have 0 count, used to iterate the hash table from reset.
     */
    static void ResetOverlap(void* context, const uintptr_t* key, OverlapEntry* value)
    {
        uint32_t count = value->m_OverlapCount;
        for (uint32_t i = 0; i < count; ++i)
        {
            value->m_Overlaps[i].m_Count = 0;
        }
    }

    void OverlapCacheReset(OverlapCache* cache)
    {
    	cache->m_OverlapCache.Iterate(ResetOverlap, (void*)0x0);
    }

    OverlapCacheAddData::OverlapCacheAddData()
    {
        memset(this, 0, sizeof(*this));
    }

    void OverlapCacheAdd(OverlapCache* cache, const OverlapCacheAddData& data)
    {
        bool found = false;
        OverlapEntry* entry_a = cache->m_OverlapCache.Get((uintptr_t)data.m_ObjectA);
        if (entry_a != 0x0)
        {
            if (!AddOverlap(entry_a, data.m_ObjectB, &found, cache->m_TriggerOverlapCapacity))
            {
                // The overlap could not be added to entry_a, bail out
                return;
            }
        }
        OverlapEntry* entry_b = cache->m_OverlapCache.Get((uintptr_t)data.m_ObjectB);
        if (entry_b != 0x0)
        {
            if (!AddOverlap(entry_b, data.m_ObjectA, &found, cache->m_TriggerOverlapCapacity))
            {
                // No space in entry_b, clean up entry_a
                if (entry_a != 0x0)
                    RemoveOverlap(entry_a, data.m_ObjectB);
                // Bail out
                return;
            }
        }
        // Add entries for previously unrecorded objects
        if (entry_a == 0x0)
            AddEntry(cache, data.m_ObjectA, data.m_UserDataA, data.m_ObjectB, data.m_GroupA);
        if (entry_b == 0x0)
            AddEntry(cache, data.m_ObjectB, data.m_UserDataB, data.m_ObjectA, data.m_GroupB);
        // Callback for newly added overlaps
        if (!found && data.m_TriggerEnteredCallback != 0x0)
        {
            TriggerEnter enter;
            enter.m_UserDataA = data.m_UserDataA;
            enter.m_UserDataB = data.m_UserDataB;
            enter.m_GroupA = data.m_GroupA;
            enter.m_GroupB = data.m_GroupB;
            data.m_TriggerEnteredCallback(enter, data.m_TriggerEnteredUserData);
        }
    }

    void OverlapCacheRemove(OverlapCache* cache, void* object)
    {
        OverlapEntry* entry = cache->m_OverlapCache.Get((uintptr_t)object);
        if (entry != 0x0)
        {
            // Remove back-references to the object from others
            for (uint32_t i = 0; i < entry->m_OverlapCount; ++i)
            {
                Overlap& overlap = entry->m_Overlaps[i];
                OverlapEntry* entry2 = cache->m_OverlapCache.Get((uintptr_t)overlap.m_Object);
                if (entry2 != 0x0)
                {
                    RemoveOverlap(entry2, object);
                }
            }
            // Remove the object from the cache
            cache->m_OverlapCache.Erase((uintptr_t)object);
            free(entry->m_Overlaps);
        }
    }

    /**
     * Context when iterating entries to prune them
     */
    struct PruneContext
    {
        TriggerExitedCallback m_Callback;
        void* m_UserData;
        OverlapCache* m_Cache;
    };

    /**
     * Prunes an entry, used to iterate the cache when pruning.
     */
    static void PruneOverlap(PruneContext* context, const uintptr_t* key, OverlapEntry* value)
    {
        TriggerExitedCallback callback = context->m_Callback;
        void* user_data = context->m_UserData;
        OverlapCache* cache = context->m_Cache;
        void* object_a = (void*)*key;
        uint32_t i = 0;
        // Iterate overlaps
        while (i < value->m_OverlapCount)
        {
            Overlap& overlap = value->m_Overlaps[i];
            // Condition to prune: no registered contacts
            if (overlap.m_Count == 0)
            {
                OverlapEntry* entry = cache->m_OverlapCache.Get((uintptr_t)overlap.m_Object);
                // Trigger exit callback
                if (callback != 0x0)
                {
                    TriggerExit data;
                    data.m_UserDataA = value->m_UserData;
                    data.m_UserDataB = entry->m_UserData;
                    data.m_GroupA = value->m_Group;
                    data.m_GroupB = entry->m_Group;
                    callback(data, user_data);
                }
                RemoveOverlap(entry, object_a);

                overlap = value->m_Overlaps[value->m_OverlapCount-1];
                --value->m_OverlapCount;
            }
            else
            {
                ++i;
            }
        }
    }

    OverlapCachePruneData::OverlapCachePruneData()
    {
        memset(this, 0, sizeof(*this));
    }

    void OverlapCachePrune(OverlapCache* cache, const OverlapCachePruneData& data)
    {
        PruneContext context;
        context.m_Callback = data.m_TriggerExitedCallback;
        context.m_UserData = data.m_TriggerExitedUserData;
        context.m_Cache = cache;
        cache->m_OverlapCache.Iterate(PruneOverlap, &context);
    }
}
