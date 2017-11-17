#ifndef PHYSICS_PRIVATE_H
#define PHYSICS_PRIVATE_H

#include <dlib/hashtable.h>

namespace dmPhysics
{
    /**
     * Used to track the overlapping of an object.
     * Count defines how many overlap-contacts are known.
     */
    struct Overlap
    {
        void* m_Object;
        uint32_t m_Count;
    };

    /**
     * Initial capacity of the cache.
     * The cache is a hash table with 75% as many buckets as capacity.
     */
    const uint32_t CACHE_INITIAL_CAPACITY = 128;

    /**
     * Count of elements to expand the cache with when it's 75% full.
     */
    const uint32_t CACHE_EXPANSION = 16;

    /**
     * Used to track all overlaps given an object.
     */
    struct OverlapEntry
    {
        void* m_UserData;
        Overlap* m_Overlaps;
        uint32_t m_OverlapCount;
        uint16_t m_Group;
    };

    /**
     * Stores every set of overlaps for each object.
     */
    struct OverlapCache {
    	OverlapCache(uint32_t triggerOverlapCapacity);

    	dmHashTable<uintptr_t, OverlapEntry> m_OverlapCache;

        /**
         * Max count of tracked overlaps per object.
         * Exceeding this limit is safe and results in warnings.
         */
    	uint32_t m_TriggerOverlapCapacity;
    };

    /**
     * Initialize the cache with CACHE_INITIAL_CAPACITY and 75% as many buckets.
     */
    void OverlapCacheInit(OverlapCache* cache);

    /**
     * Reset the overlap cache which set the contact point counts (Overlap::m_Count) to 0,
     * so that removed contacts will be properly removed from the cache.
     */
    void OverlapCacheReset(OverlapCache* cache);

    struct OverlapCacheAddData
    {
        OverlapCacheAddData();

        /// Trigger entered callback
        TriggerEnteredCallback  m_TriggerEnteredCallback;
        /// Trigger entered callback user data
        void*                   m_TriggerEnteredUserData;
        /// First object of the pair
        void*                   m_ObjectA;
        /// First object user data of the pair
        void*                   m_UserDataA;
        /// Second object of the pair
        void*                   m_ObjectB;
        /// Second object user data of the pair
        void*                   m_UserDataB;
        /// Collision group of the first object
        uint16_t                m_GroupA;
        /// Collision group of the second object
        uint16_t                m_GroupB;
    };

    /**
     * Adds an overlap to the cache and potentially calls the trigger entered callback
     * if it was the first known occurrence of overlap.
     */
    void OverlapCacheAdd(OverlapCache* cache, const OverlapCacheAddData& data);

    /**
     * Removes an object from the cache, which cleans all occurences of the object.
     */
    void OverlapCacheRemove(OverlapCache* cache, void* object);

    struct OverlapCachePruneData
    {
        OverlapCachePruneData();

        /// Trigger exited callback
        TriggerExitedCallback  m_TriggerExitedCallback;
        /// Trigger exited callback user data
        void*                   m_TriggerExitedUserData;
    };

    /**
     * Cleans lingering overlaps and potentially calls the trigger exited callback
     * if it is the last known occurrence of overlap.
     */
    void OverlapCachePrune(OverlapCache* cache, const OverlapCachePruneData& data);
}

#endif // PHYSICS_PRIVATE_H
