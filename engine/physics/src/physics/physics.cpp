#include "physics.h"
#include "physics_private.h"

#include <stdint.h>
#include <string.h>

#include <dlib/array.h>
#include <dlib/log.h>
#include <dlib/profile.h>
#include <dlib/socket.h>

namespace dmPhysics
{
    using namespace Vectormath::Aos;

    const char* PHYSICS_SOCKET_NAME = "@physics";

    static ExtensionDesc* g_FirstPhysicsExtension = 0;
    ExtensionDesc* g_PhysicsExtension = 0;
    dmMessage::HSocket g_PhysicsSocket = 0;

    void Register(struct ExtensionDesc* desc, uint32_t desc_size, const char *name, bool is3D)
    {
        desc->m_3D = is3D;
        desc->m_Name = name;
        desc->m_Next = g_FirstPhysicsExtension;
        g_FirstPhysicsExtension = desc;
    }

    static ExtensionDesc* GetFirstMatchingExtension(bool is3D)
    {
        for( ExtensionDesc* desc = g_FirstPhysicsExtension; desc != 0; desc = desc->m_Next )
        {
            if( desc->m_3D == is3D )
                return desc;
        }
        return 0;
    }


    static void DeleteExtensions()
    {
        ExtensionDesc* desc = g_FirstPhysicsExtension;
        while( desc )
        {
            ExtensionDesc* next = desc->m_Next;
            free(desc);
            desc = next;
        }
    }


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
    : m_TriggerOverlapCapacity(trigger_overlap_capacity)
    , m_OverlapCache()
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


    HContext NewContext(const NewContextParams& params)
    {
        ExtensionDesc* desc = GetFirstMatchingExtension(params.m_3D);
        if( !desc )
        {
            dmLogWarning("Could not find a %d physics extension", params.m_3D ? "3d" : "2d");
            return 0;
        }
        g_PhysicsExtension = desc;
        
        HContext context = g_PhysicsExtension->NewContext(params);

        dmMessage::Result result = dmMessage::NewSocket(PHYSICS_SOCKET_NAME, &g_PhysicsSocket);
        if (result != dmMessage::RESULT_OK)
        {
            dmLogFatal("Could not create socket '%s'.", PHYSICS_SOCKET_NAME);
            DeleteContext(context);
            return 0x0;
        }
        return context;
    }

    void DeleteContext(HContext context)
    {
        g_PhysicsExtension->DeleteContext(context);
        
        if (g_PhysicsSocket != 0)
            dmMessage::DeleteSocket(g_PhysicsSocket);
    }


    dmMessage::HSocket GetSocket(HContext _context)
    {
        return g_PhysicsSocket;
    }


    HWorld NewWorld(HContext context, const NewWorldParams& params)
    {
        g_PhysicsExtension->NewWorld(context, params);
    }

    void DeleteWorld(HContext context, HWorld world)
    {
        g_PhysicsExtension->DeleteWorld(context, world);
    }

    void StepWorld(HWorld world, const StepWorldContext& context)
    {
        g_PhysicsExtension->StepWorld(world, context);
    }

    void SetDrawDebug(HWorld world, bool draw_debug)
    {
        g_PhysicsExtension->SetDrawDebug(world, draw_debug);
    }

    HCollisionShape NewSphereShape(HContext context, float radius)
    {
        if(g_PhysicsExtension->NewSphereShape)
            g_PhysicsExtension->NewSphereShape(context, radius);
    }

    HCollisionShape NewBoxShape(HContext context, const Vectormath::Aos::Vector3& half_extents)
    {
        if(g_PhysicsExtension->NewBoxShape)
            g_PhysicsExtension->NewBoxShape(context, half_extents);
    }

    HCollisionShape NewCapsuleShape(HContext context, float radius, float height)
    {
        if(g_PhysicsExtension->NewCapsuleShape)
            g_PhysicsExtension->NewCapsuleShape(context, radius, height);
    }

    HCollisionShape NewConvexHullShape(HContext context, const float* vertices, uint32_t vertex_count)
    {
        if(g_PhysicsExtension->NewConvexHullShape)
            g_PhysicsExtension->NewConvexHullShape(context, vertices, vertex_count);
    }

    HHullSet2D NewHullSet2D(HContext context, const float* vertices, uint32_t vertex_count,
                            const HullDesc* hulls, uint32_t hull_count)
    {
        if(g_PhysicsExtension->NewHullSet2D)
            g_PhysicsExtension->NewHullSet2D(context, vertices, vertex_count, hulls, hull_count);
    }

    void DeleteHullSet2D(HHullSet2D hull_set)
    {
        if(g_PhysicsExtension->DeleteHullSet2D)
            g_PhysicsExtension->DeleteHullSet2D(hull_set);
    }

    HCollisionShape NewGridShape2D(HContext context, HHullSet2D hull_set,
                                     const Vectormath::Aos::Point3& position,
                                     uint32_t cell_width, uint32_t cell_height,
                                     uint32_t row_count, uint32_t column_count)
    {
        if(g_PhysicsExtension->NewGridShape2D)
            g_PhysicsExtension->NewGridShape2D(context, hull_set, position, cell_width, cell_height, row_count, column_count);
    }

    void SetGridShapeHull(HCollisionObject collision_object, uint32_t shape_index, uint32_t row, uint32_t column, uint32_t hull, HullFlags flags)
    {
        if(g_PhysicsExtension->SetGridShapeHull)
            g_PhysicsExtension->SetGridShapeHull(collision_object, shape_index, row, column, hull, flags);
    }

    void SetCollisionObjectFilter(HCollisionObject collision_object,
                                  uint32_t shape, uint32_t child,
                                  uint16_t group, uint16_t mask)
    {
        if(g_PhysicsExtension->SetCollisionObjectFilter)
            g_PhysicsExtension->SetCollisionObjectFilter(collision_object, shape, child, group, mask);
    }

    void DeleteCollisionShape(HCollisionShape shape)
    {
        if(g_PhysicsExtension->DeleteCollisionShape)
            g_PhysicsExtension->DeleteCollisionShape(shape);
    }

    HCollisionObject NewCollisionObject(HWorld world, const CollisionObjectData& data, HCollisionShape* shapes, uint32_t shape_count)
    {
        if(g_PhysicsExtension->NewCollisionObject)
            g_PhysicsExtension->NewCollisionObject(world, data, shapes, shape_count);
    }

    HCollisionObject NewCollisionObjectTransform(HWorld world, const CollisionObjectData& data,
                                        HCollisionShape* shapes,
                                        Vectormath::Aos::Vector3* translations,
                                        Vectormath::Aos::Quat* rotations,
                                        uint32_t shape_count)
    {
        if(g_PhysicsExtension->NewCollisionObjectTransform)
            g_PhysicsExtension->NewCollisionObjectTransform(world, data, shapes, translations, rotations, shape_count);
    }

    void DeleteCollisionObject(HWorld world, HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->DeleteCollisionObject)
            g_PhysicsExtension->DeleteCollisionObject(world, collision_object);
    }

    uint32_t GetCollisionShapes(HCollisionObject collision_object, HCollisionShape* out_buffer, uint32_t buffer_size)
    {
        if(g_PhysicsExtension->GetCollisionShapes)
            return g_PhysicsExtension->GetCollisionShapes(collision_object, out_buffer, buffer_size);
        return 0;
    }


    void SetCollisionObjectUserData(HCollisionObject collision_object, void* user_data)
    {
        if(g_PhysicsExtension->SetCollisionObjectUserData)
            g_PhysicsExtension->SetCollisionObjectUserData(collision_object, user_data);
    }

    /**
     * Set 3D collision object user data
     *
     * @param collision_object Collision object
     * @return User data
     */
    void* GetCollisionObjectUserData(HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetCollisionObjectUserData)
            return g_PhysicsExtension->GetCollisionObjectUserData(collision_object);
        return 0;
    }


    /**
     * Apply a force to the specified 3D collision object at the specified position
     *
     * @param context Physics context
     * @param collision_object Collision object receiving the force, must be of type COLLISION_OBJECT_TYPE_DYNAMIC
     * @param force Force to be applied (world space)
     * @param position Position of where the force will be applied (world space)
     */
    void ApplyForce(HContext context, HCollisionObject collision_object, const Vectormath::Aos::Vector3& force, const Vectormath::Aos::Point3& position)
    {
        if(g_PhysicsExtension->ApplyForce)
            g_PhysicsExtension->ApplyForce(context, collision_object, force, position);
    }

    /**
     * Return the total force currently applied to the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Which collision object to inspect. For objects with another type than COLLISION_OBJECT_TYPE_DYNAMIC, the force will always be of zero size.
     * @return The total force (world space).
     */
    Vectormath::Aos::Vector3 GetTotalForce(HContext context, HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetTotalForce)
            return g_PhysicsExtension->GetTotalForce(context, collision_object);
        return Vectormath::Aos::Vector3(0,0,0);
    }

    /**
     * Return the world position of the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space position
     */
    Vectormath::Aos::Point3 GetWorldPosition(HContext context, HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetWorldPosition)
            return g_PhysicsExtension->GetWorldPosition(context, collision_object);
        return Vectormath::Aos::Point3(0,0,0);
    }

    /**
     * Return the world rotation of the specified 3D collision object.
     *
     * @param context Physics context
     * @param collision_object Collision object handle
     * @return The world space rotation
     */
    Vectormath::Aos::Quat GetWorldRotation(HContext context, HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetWorldRotation)
            return g_PhysicsExtension->GetWorldRotation(context, collision_object);
        return Vectormath::Aos::Quat();
    }

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The linear velocity.
     */
    Vectormath::Aos::Vector3 GetLinearVelocity(HContext context, HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetLinearVelocity)
            return g_PhysicsExtension->GetLinearVelocity(context, collision_object);
        return Vectormath::Aos::Vector3(0,0,0);
    }

    /**
     * Return the linear velocity of the 3D collision object.
     *
     * @param context Physics context
     * @param collision_object
     * @return The angular velocity. The direction of the vector coincides with the axis of rotation, the magnitude is the angle of rotation.
     */
    Vectormath::Aos::Vector3 GetAngularVelocity(HContext context, HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetAngularVelocity)
            return g_PhysicsExtension->GetAngularVelocity(context, collision_object);
        return Vectormath::Aos::Vector3(0,0,0);
    }

    /**
     * Return whether the 3D collision object is enabled or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is enabled
     */
    bool IsEnabled(HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->IsEnabled)
            return g_PhysicsExtension->IsEnabled(collision_object);
        return false;
    }

    /**
     * Set whether the 3D collision object is enabled or not.
     *
     * If a disabled dynamic collision object is enabled, it will have its internal state reset and assume the transform
     * from the transform callback before being being enabled.
     *
     * @param world World of the collision object
     * @param collision_object Collision object to enable/disable
     * @param enabled true if the object should be enabled, false otherwise
     */
    void SetEnabled(HWorld world, HCollisionObject collision_object, bool enabled)
    {
        if(g_PhysicsExtension->SetEnabled)
            g_PhysicsExtension->SetEnabled(world, collision_object, enabled);
    }

    /**
     * Return whether the 3D collision object is sleeping or not.
     *
     * @param collision_object Collision object
     * @return true if the collision object is sleeping
     */
    bool IsSleeping(HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->IsSleeping)
            return g_PhysicsExtension->IsSleeping(collision_object);
        return false;
    }

    /**
     * Set whether the 3D collision object has locked rotation or not, which means that the angular velocity will always be 0.
     *
     * @param collision_object Collision object to lock
     * @param locked_rotation true to lock the rotation
     */
    void SetLockedRotation(HCollisionObject collision_object, bool locked_rotation)
    {
        if(g_PhysicsExtension->SetLockedRotation)
            g_PhysicsExtension->SetLockedRotation(collision_object, locked_rotation);
    }

    /**
     * Return the linear damping of the 3D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetLinearDamping(HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetLinearDamping)
            return g_PhysicsExtension->GetLinearDamping(collision_object);
        return 0.0f;
    }

    /**
     * Set the linear damping of the 3D collision object, which decreases the linear velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param linear_damping Damping in the interval [0, 1]
     */
    void SetLinearDamping(HCollisionObject collision_object, float linear_damping)
    {
        if(g_PhysicsExtension->SetLinearDamping)
            g_PhysicsExtension->SetLinearDamping(collision_object, linear_damping);
    }

    /**
     * Return the angular damping of the 3D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @return the linear damping
     */
    float GetAngularDamping(HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetAngularDamping)
            return g_PhysicsExtension->GetAngularDamping(collision_object);
        return 0.0f;
    }

    /**
     * Set the angular damping of the 3D collision object, which decreases the angular velocity over time.
     *
     * The damping is applied as:
     * velocity *= pow(1.0 - damping, dt)
     *
     * @param collision_object Collision object
     * @param angular_damping Damping in the interval [0, 1]
     */
    void SetAngularDamping(HCollisionObject collision_object, float angular_damping)
    {
        if(g_PhysicsExtension->SetAngularDamping)
            g_PhysicsExtension->SetAngularDamping(collision_object, angular_damping);
    }

    /**
     * Return the mass of the 3D collision object.
     *
     * @param collision_object Collision object
     * @return the total mass
     */
    float GetMass(HCollisionObject collision_object)
    {
        if(g_PhysicsExtension->GetMass)
            return g_PhysicsExtension->GetMass(collision_object);
        return 0.0f;
    }

    /**
     * Request a ray cast that will be performed the next time the 3D world is updated
     *
     * @param world Physics world in which to perform the ray cast
     * @param request Struct containing data for the query
     */
    void RequestRayCast(HWorld world, const RayCastRequest& request)
    {
        if(g_PhysicsExtension->RequestRayCast)
            g_PhysicsExtension->RequestRayCast(world, request);
    }

    /**
     * Set functions to use when drawing debug data.
     *
     * @param context Context for which to set callbacks
     * @param callbacks New callback functions
     */
    void SetDebugCallbacks(HContext context, const DebugCallbacks& callbacks)
    {
        if(g_PhysicsExtension->SetDebugCallbacks)
            g_PhysicsExtension->SetDebugCallbacks(context, callbacks);
    }

    /**
     * Replace a shape with another shape for all 3D collision objects connected to that shape.
     *
     * @param context Context in which to replace shapes
     * @param old_shape The shape to disconnect
     * @param new_shape The shape to connect
     */
    void ReplaceShape(HContext context, HCollisionShape old_shape, HCollisionShape new_shape)
    {
        if(g_PhysicsExtension->ReplaceShape)
            g_PhysicsExtension->ReplaceShape(context, old_shape, new_shape);
    }

}
