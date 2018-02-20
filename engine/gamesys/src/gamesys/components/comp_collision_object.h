#ifndef DM_GAMESYS_COMP_COLLISION_OBJECT_H
#define DM_GAMESYS_COMP_COLLISION_OBJECT_H

#include <stdint.h>

#include <gameobject/gameobject.h>

namespace dmPhysics
{
    typedef void* HCollisionObject2D;
    typedef void* HCollisionObject3D;
    struct ContactPoint;
    typedef void (*ContactPointTestCallback)(const dmPhysics::ContactPoint& contact_point, void* user_ctx);
}

namespace dmGameSystem
{
    dmGameObject::CreateResult CompCollisionObjectNewWorld(const dmGameObject::ComponentNewWorldParams& params);

    dmGameObject::CreateResult CompCollisionObjectDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);

    dmGameObject::CreateResult CompCollisionObjectCreate(const dmGameObject::ComponentCreateParams& params);

    dmGameObject::CreateResult CompCollisionObjectDestroy(const dmGameObject::ComponentDestroyParams& params);

    dmGameObject::CreateResult CompCollisionObjectFinal(const dmGameObject::ComponentFinalParams& params);

    dmGameObject::CreateResult CompCollisionObjectAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);

    dmGameObject::UpdateResult CompCollisionObjectUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);

    dmGameObject::UpdateResult CompCollisionObjectPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);

    dmGameObject::UpdateResult CompCollisionObjectOnMessage(const dmGameObject::ComponentOnMessageParams& params);

    void* CompCollisionObjectGetComponent(const dmGameObject::ComponentGetParams& params);

    void CompCollisionObjectOnReload(const dmGameObject::ComponentOnReloadParams& params);

    dmGameObject::PropertyResult CompCollisionObjectGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);

    dmGameObject::PropertyResult CompCollisionObjectSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    // Public functions (used from script_physics.cpp)
    struct CollisionComponent;
    struct CollisionWorld;
    struct PhysicsContext;

    uint16_t CompCollisionGetGroupBitIndex(void* world, uint64_t group_hash);
    uint64_t GetLSBGroupHash(CollisionWorld* world, uint16_t mask);

    /**
     * Gets the physics context used for a world
     */
    PhysicsContext* CompCollisionGetPhysicsContext(const CollisionWorld* world);

    /**
     * Gets the 2D physics object given a component
     */
    dmPhysics::HCollisionObject2D CompCollisionGetCollisionObject2D(const CollisionComponent* component);

    /**
     * Gets the 3D physics object given a component
     */
    dmPhysics::HCollisionObject3D CompCollisionGetCollisionObject3D(const CollisionComponent* component);

    /**
     * Gets the game object from a collision component
     */
    dmGameObject::HInstance CompCollisionGetGameObject(const CollisionComponent* component);


    /**
     * Gets the contact points from
     */
    void ContactPointTest3D(PhysicsContext* context, CollisionWorld* world, CollisionComponent* component, const Vectormath::Aos::Point3& position, const Vectormath::Aos::Quat& rotation, dmPhysics::ContactPointTestCallback callback, void* user_ctx);

}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_H
