#include <test_script.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>

#include "components/bullet3d/comp_collision_object_bullet3d.h"
#include "gamesys_private.h"
#include "scripts/bullet3d/script_bullet3d.h"
#include "test_gamesys.h"

using namespace dmVMath;

static void RunPhysicsScriptTest(dmResource::HFactory factory, dmGameObject::HCollection collection, dmGameObject::UpdateContext* update_context,
                                 dmScript::HContext script_context, const char* prototype_path, const char* instance_path)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(script_context);

    dmGameObject::HInstance go = Spawn(factory, collection, prototype_path, dmHashString64(instance_path), 0,
                                       Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    bool tests_done = false;
    uint32_t update_count = 0;
    // Bound script-driven tests so a missing tests_done signal fails instead of hanging.
    const uint32_t max_update_count = 60;
    while (!tests_done && update_count++ < max_update_count)
    {
        ASSERT_TRUE(dmGameObject::Update(collection, update_context));
        ASSERT_TRUE(dmGameObject::PostUpdate(collection));

        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    ASSERT_TRUE(tests_done);

    ASSERT_TRUE(dmGameObject::Final(collection));
}

/* Physics joints */
TEST_F(ComponentTest, JointTest)
{
    /* Setup:
    ** joint_test_a
    ** - [collisionobject] collision_object/joint_test_sphere.collisionobject
    ** - [script] collision_object/joint_test.script
    ** joint_test_b
    ** - [collisionobject] collision_object/joint_test_sphere.collisionobject
    ** joint_test_c
    ** - [collisionobject] collision_object/joint_test_static_floor.collisionobject
    */

    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    const char* path_joint_test_a = "/collision_object/joint_test_a.goc";
    const char* path_joint_test_b = "/collision_object/joint_test_b.goc";
    const char* path_joint_test_c = "/collision_object/joint_test_c.goc";

    dmhash_t hash_go_joint_test_a = dmHashString64("/joint_test_a");
    dmhash_t hash_go_joint_test_b = dmHashString64("/joint_test_b");
    dmhash_t hash_go_joint_test_c = dmHashString64("/joint_test_c");

    dmGameObject::HInstance go_c = Spawn(m_Factory, m_Collection, path_joint_test_c, hash_go_joint_test_c, 0, Point3(0, -100, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_c);

    dmGameObject::HInstance go_b = Spawn(m_Factory, m_Collection, path_joint_test_b, hash_go_joint_test_b, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_b);

    dmGameObject::HInstance go_a = Spawn(m_Factory, m_Collection, path_joint_test_a, hash_go_joint_test_a, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_a);

    // Iteration 1: Handle proxy enable and input acquire messages from input_consume_no.script
    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

}

TEST_F(ComponentTest, Box2DWorldApiTest)
{
    /* Intent: verify b2d.world query and cast wrappers.
    ** Setup: one scripted dynamic collision object creates an isolated shape.
    ** Expected: b2d.world overlap and cast wrappers identify that shape/fixture
    ** at the expected approximate hit position and fraction.
    */
    RunPhysicsScriptTest(m_Factory, m_Collection, &m_UpdateContext, m_ScriptContext,
                         "/collision_object/box2d_world_test.goc", "/box2d_world_test");
}

TEST_F(ComponentTest, Box2DShapeApiTest)
{
    /* Intent: verify shape-facing Box2D Lua wrappers.
    ** Setup: one scripted dynamic collision object owns the tested shape/fixture.
    ** Expected: v2 fixture shape helpers and v3 body/shape-id helpers round-trip
    ** properties, ownership and derived data, then report invalid handles after destroy.
    */
    RunPhysicsScriptTest(m_Factory, m_Collection, &m_UpdateContext, m_ScriptContext,
                         "/collision_object/box2d_shape_test.goc", "/box2d_shape_test");
}

TEST_F(ComponentTest, Box2DChainApiTest)
{
    /* Intent: verify v3 chain userdata and segment-shape integration.
    ** Setup: one scripted static collision object creates v3 chains far from base geometry.
    ** Expected: v3 chain userdata, segment shapes, world casts and destroy invalidation
    ** behave consistently; v2 exposes no native chain userdata API.
    */
    RunPhysicsScriptTest(m_Factory, m_Collection, &m_UpdateContext, m_ScriptContext,
                         "/collision_object/box2d_chain_test.goc", "/box2d_chain_test");
}

TEST_F(Bullet3DComponentTest, Bullet3DApiTest)
{
    /* Intent: verify the native Bullet3D Lua wrappers against both rigid-body
    ** and trigger collision objects.
    ** Setup: one scripted dynamic body plus auxiliary dynamic, trigger, static,
    ** and kinematic collision objects.
    ** Expected: namespace/version/world queries, cross-API gravity, handle identity,
    ** rigid/trigger separation, transforms, material/CCD/activation state, velocities,
    ** damping, factors, gravity, sleep thresholds, forces, impulses and AABBs behave
    ** in Defold units at scale 0.1, and game-object deletion invalidates retained handles.
    */
    dmGameObject::HInstance rigid_body = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_rigid_body.goc", dmHashString64("/bullet3d_rigid_body"), 0, Point3(10, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, rigid_body);

    dmGameObject::HInstance trigger = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_trigger.goc", dmHashString64("/bullet3d_trigger"), 0, Point3(20, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, trigger);

    dmGameObject::HInstance static_body = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_static.goc", dmHashString64("/bullet3d_static"), 0, Point3(200, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, static_body);

    dmGameObject::HInstance kinematic_body = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_kinematic.goc", dmHashString64("/bullet3d_kinematic"), 0, Point3(100, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, kinematic_body);

    RunPhysicsScriptTest(m_Factory, m_Collection, &m_UpdateContext, m_ScriptContext, "/collision_object/bullet3d_test.goc", "/bullet3d_test");
}

TEST_F(Bullet3DComponentTest, Bullet3DWorldQueryApiTest)
{
    /* Intent: verify every Bullet3D synchronous world query, collision-filter
    ** accessor, result schema and linked inside-ray/convex-sweep use case.
    ** Setup: an isolated separated-child compound and convex hull, static unit
    ** boxes in two reciprocal filter groups, a trigger, a penetrating
    ** kinematic-box/static-box pair plus an incompatible overlapping B box,
    ** a movable box and a rotated AABB case.
    ** Expected: overlaps, sorted/capped/closest casts, primitive and hull sweeps,
    ** normalized contacts, enumeration, filtering, borrowed identity, scale-0.1
    ** geometry and immediate transform queries match the exact Lua API contract.
    */
    dmGameObject::HInstance compound = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_compound.goc", dmHashString64("/bullet3d_query_compound"), 0, Point3(-50, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, compound);

    dmGameObject::HInstance hull = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_hull.goc", dmHashString64("/bullet3d_query_hull"), 0, Point3(-30, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, hull);

    dmGameObject::HInstance near_box = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_static.goc", dmHashString64("/bullet3d_query_near"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, near_box);

    dmGameObject::HInstance filtered_box = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_filtered.goc", dmHashString64("/bullet3d_query_filtered"), 0, Point3(5, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, filtered_box);

    dmGameObject::HInstance far_box = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_static.goc", dmHashString64("/bullet3d_query_far"), 0, Point3(10, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, far_box);

    dmGameObject::HInstance trigger = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_trigger.goc", dmHashString64("/bullet3d_query_trigger"), 0, Point3(15, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, trigger);

    dmGameObject::HInstance contact_filtered = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_filtered.goc", dmHashString64("/bullet3d_query_contact_filtered"), 0, Point3(28.5f, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, contact_filtered);

    dmGameObject::HInstance contact_a = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_kinematic.goc", dmHashString64("/bullet3d_query_contact_a"), 0, Point3(30, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, contact_a);

    dmGameObject::HInstance contact_b = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_static.goc", dmHashString64("/bullet3d_query_contact_b"), 0, Point3(31.5f, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, contact_b);

    dmGameObject::HInstance movable = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_static.goc", dmHashString64("/bullet3d_query_movable"), 0, Point3(50, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, movable);

    dmGameObject::HInstance rotated = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_static.goc", dmHashString64("/bullet3d_query_rotated"), 0, Point3(70, 0, 0), Quat(0, 0, 0.38268343f, 0.92387953f), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, rotated);

    RunPhysicsScriptTest(m_Factory, m_Collection, &m_UpdateContext, m_ScriptContext, "/collision_object/bullet3d_query_test.goc", "/bullet3d_query_test");
}

TEST_F(Bullet3DComponentTest, Bullet3DConstraintApiTest)
{
    /* Intent: verify every owned constraint type and its body/world lifetime.
    ** Setup: two awake dynamic bodies in zero gravity are linked one constraint
    ** at a time, then retained through body disable, re-enable and deletion.
    ** Expected: all creators expose stable identity/enumeration and common state,
    ** a point constraint changes separation, enabled state controls world membership,
    ** and explicit or cascaded destruction invalidates stale handles.
    */
    dmGameObject::HInstance body_a = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_rigid_body.goc", dmHashString64("/bullet3d_constraint_a"), 0, Point3(-5, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body_a);
    dmGameObject::HInstance body_b = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_rigid_body.goc", dmHashString64("/bullet3d_constraint_b"), 0, Point3(5, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, body_b);

    RunPhysicsScriptTest(m_Factory, m_Collection, &m_UpdateContext, m_ScriptContext, "/collision_object/bullet3d_constraint_test.goc", "/bullet3d_constraint_test");
}

TEST_F(Bullet3DComponentTest, Bullet3DNativeScaleConversionTest)
{
    /* Intent: verify scale-sensitive Lua inputs at the native Bullet boundary.
    ** Setup: one mass-one dynamic unit box is exposed to Lua as borrowed userdata
    ** while the fixture uses physics scale 0.1.
    ** Expected: position, contact/CCD distances, velocity, gravity, sleeping threshold,
    ** force and torque are stored in Bullet units using the required scale or scale squared.
    */
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_rigid_body.goc", dmHashString64("/bullet3d_native_scale"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    uint32_t                      component_type = 0;
    dmGameObject::HComponent      component = 0;
    dmGameObject::HComponentWorld component_world = 0;
    ASSERT_EQ(dmGameObject::RESULT_OK,
              dmGameObject::GetComponent(go, dmHashString64("collisionobject"), &component_type, &component, &component_world));

    btCollisionObject* collision_object = (btCollisionObject*)dmGameSystem::CompCollisionObjectGetBullet3DCollisionObject(component);
    ASSERT_NE((void*)0, collision_object);
    btRigidBody* rigid_body = btRigidBody::upcast(collision_object);
    ASSERT_NE((void*)0, rigid_body);

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    dmGameSystem::PushBullet3DCollisionObject(L, collision_object, m_Collection, dmGameObject::GetIdentifier(go));
    lua_setglobal(L, "bullet3d_native_scale_body");

    ASSERT_TRUE(dmScriptTest::RunString(L,
                                        "local body = bullet3d_native_scale_body\n"
                                        "bullet3d.collision_object.set_position(body, vmath.vector3(7, 8, 9))\n"
                                        "bullet3d.collision_object.set_contact_processing_threshold(body, 1.25)\n"
                                        "bullet3d.collision_object.set_ccd_motion_threshold(body, 0.75)\n"
                                        "bullet3d.collision_object.set_ccd_swept_sphere_radius(body, 0.5)\n"
                                        "bullet3d.rigid_body.set_linear_velocity(body, vmath.vector3(1, 2, 3))\n"
                                        "bullet3d.rigid_body.set_gravity(body, vmath.vector3(3, -4, 5))\n"
                                        "bullet3d.rigid_body.set_sleeping_thresholds(body, 2.5, 0.75)\n"
                                        "bullet3d.rigid_body.set_mass_properties(body, 2, vmath.vector3(2, 4, 8))"));

    const btVector3& origin = collision_object->getWorldTransform().getOrigin();
    ASSERT_NEAR(0.7f, origin.x(), 0.0001f);
    ASSERT_NEAR(0.8f, origin.y(), 0.0001f);
    ASSERT_NEAR(0.9f, origin.z(), 0.0001f);
    ASSERT_NEAR(0.125f, collision_object->getContactProcessingThreshold(), 0.0001f);
    ASSERT_NEAR(0.075f, collision_object->getCcdMotionThreshold(), 0.0001f);
    ASSERT_NEAR(0.05f, collision_object->getCcdSweptSphereRadius(), 0.0001f);
    ASSERT_NEAR(0.1f, rigid_body->getLinearVelocity().x(), 0.0001f);
    ASSERT_NEAR(0.2f, rigid_body->getLinearVelocity().y(), 0.0001f);
    ASSERT_NEAR(0.3f, rigid_body->getLinearVelocity().z(), 0.0001f);
    ASSERT_NEAR(0.3f, rigid_body->getGravity().x(), 0.0001f);
    ASSERT_NEAR(-0.4f, rigid_body->getGravity().y(), 0.0001f);
    ASSERT_NEAR(0.5f, rigid_body->getGravity().z(), 0.0001f);
    ASSERT_NEAR(0.25f, rigid_body->getLinearSleepingThreshold(), 0.0001f);
    ASSERT_NEAR(0.75f, rigid_body->getAngularSleepingThreshold(), 0.0001f);
    ASSERT_NEAR(0.02f, rigid_body->getInvInertiaDiagLocal().x() == 0.0f ? 0.0f : 1.0f / rigid_body->getInvInertiaDiagLocal().x(), 0.0001f);
    ASSERT_NEAR(0.04f, rigid_body->getInvInertiaDiagLocal().y() == 0.0f ? 0.0f : 1.0f / rigid_body->getInvInertiaDiagLocal().y(), 0.0001f);
    ASSERT_NEAR(0.08f, rigid_body->getInvInertiaDiagLocal().z() == 0.0f ? 0.0f : 1.0f / rigid_body->getInvInertiaDiagLocal().z(), 0.0001f);

    ASSERT_TRUE(dmScriptTest::RunString(L,
                                        "local body = bullet3d_native_scale_body\n"
                                        "bullet3d.rigid_body.set_linear_velocity(body, vmath.vector3())\n"
                                        "bullet3d.rigid_body.clear_forces(body)\n"
                                        "bullet3d.rigid_body.apply_force(body, vmath.vector3(0, 2, 0), vmath.vector3(3, 0, 0))\n"
                                        "bullet3d.rigid_body.apply_torque(body, vmath.vector3(0, 0, 4))\n"
                                        "bullet3d.rigid_body.apply_central_impulse(body, vmath.vector3(2, 4, 6))"));

    ASSERT_NEAR(0.0f, rigid_body->getTotalForce().x(), 0.0001f);
    ASSERT_NEAR(0.2f, rigid_body->getTotalForce().y(), 0.0001f);
    ASSERT_NEAR(0.0f, rigid_body->getTotalForce().z(), 0.0001f);
    ASSERT_NEAR(0.0f, rigid_body->getTotalTorque().x(), 0.0001f);
    ASSERT_NEAR(0.0f, rigid_body->getTotalTorque().y(), 0.0001f);
    ASSERT_NEAR(0.1f, rigid_body->getTotalTorque().z(), 0.0001f);
    ASSERT_NEAR(0.1f, rigid_body->getLinearVelocity().x(), 0.0001f);
    ASSERT_NEAR(0.2f, rigid_body->getLinearVelocity().y(), 0.0001f);
    ASSERT_NEAR(0.3f, rigid_body->getLinearVelocity().z(), 0.0001f);
}

TEST_F(Bullet3DComponentTest, Bullet3DStaleIdentitiesDoNotRecycle)
{
    /* Intent: keep invalid Lua userdata invalid after more allocations than the
    ** previous 16-bit opaque-handle generation could represent.
    ** Expected: neither collision-object nor world userdata can revive while a
    ** later registration of the same native pointer is live.
    */
    const uint32_t    previous_generation_count = 0xFFFE;
    lua_State*        L = dmScript::GetLuaState(m_ScriptContext);

    btCollisionObject collision_object;
    dmGameSystem::PushBullet3DCollisionObject(L, &collision_object, 0, 0);
    lua_setglobal(L, "bullet3d_stale_collision_object");
    dmGameSystem::ScriptBullet3DInvalidateCollisionObject(&collision_object);

    bool collision_object_revived = false;
    for (uint32_t i = 0; i < previous_generation_count; ++i)
    {
        dmGameSystem::PushBullet3DCollisionObject(L, &collision_object, 0, 0);
        lua_getglobal(L, "bullet3d_stale_collision_object");
        collision_object_revived |= dmGameSystem::IsBullet3DCollisionObjectValid(L, -1);
        lua_pop(L, 1);
        dmGameSystem::ScriptBullet3DInvalidateCollisionObject(&collision_object);
        lua_pop(L, 1);
    }
    ASSERT_FALSE(collision_object_revived);

    int world_storage = 0;
    dmGameSystem::PushBullet3DWorld(L, &world_storage, 0);
    lua_setglobal(L, "bullet3d_stale_world");
    dmGameSystem::ScriptBullet3DInvalidateWorld(&world_storage);

    bool world_revived = false;
    for (uint32_t i = 0; i < previous_generation_count; ++i)
    {
        dmGameSystem::PushBullet3DWorld(L, &world_storage, 0);
        lua_getglobal(L, "bullet3d_stale_world");
        world_revived |= dmGameSystem::IsBullet3DWorldValid(L, -1);
        lua_pop(L, 1);
        dmGameSystem::ScriptBullet3DInvalidateWorld(&world_storage);
        lua_pop(L, 1);
    }
    ASSERT_FALSE(world_revived);

    lua_pushnil(L);
    lua_setglobal(L, "bullet3d_stale_collision_object");
    lua_pushnil(L);
    lua_setglobal(L, "bullet3d_stale_world");
}

TEST_F(Bullet3DComponentTest, Bullet3DCollisionObjectHandleInvalidatedOnResourceReload)
{
    /* Intent: exercise the native collision-object invalidation callback during
    ** same-generation collision resource recreation.
    ** Setup: a script retains its world and rigid-body handles while C++ reloads
    ** only the collision-object resource and verifies that the owning GO generation
    ** does not change.
    ** Expected: the old body handle becomes invalid and rejects access, the world
    ** remains valid, and a fresh body lookup returns a distinct valid handle whose
    ** post-reload disable/re-enable callbacks remove and restore world membership.
    */
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_lifetime_test.goc", dmHashString64("/bullet3d_lifetime_reload"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_getglobal(L, "bullet3d_lifetime_ready");
    ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);

    uint32_t generation = dmGameObject::GetGeneration(go);
    ASSERT_EQ(dmResource::RESULT_OK,
              dmResource::ReloadResource(m_Factory, "/collision_object/bullet3d_body.collisionobjectc", 0));
    ASSERT_EQ(generation, dmGameObject::GetGeneration(go));

    lua_pushboolean(L, 1);
    lua_setglobal(L, "bullet3d_lifetime_reload_complete");
    bool tests_done = false;
    uint32_t update_count = 0;
    while (!tests_done && update_count++ < 10)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }
    ASSERT_TRUE(tests_done);

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

TEST_F(Bullet3DComponentTest, Bullet3DHandlesInvalidatedOnCollectionTeardown)
{
    /* Intent: exercise cross-world query ownership guards and native collision-object
    ** and world invalidation callbacks while the Bullet3D script library is active.
    ** Setup: one primary-collection body and a secondary-collection script retain
    ** their body/world handles while C++ explicitly destroys only the secondary.
    ** Expected: primary-world contact queries reject the foreign body in either pair
    ** position; destruction selectively invalidates both secondary handles, world
    ** queries and filter accessors, while equivalent primary handles remain usable.
    */
    dmGameObject::HCollection collection = dmGameObject::NewCollection("bullet3d_lifetime_collection",
                                                                       m_Factory,
                                                                       m_Register,
                                                                       32,
                                                                       0x0);
    ASSERT_NE((void*)0, collection);

    dmGameObject::HInstance go = Spawn(m_Factory, collection, "/collision_object/bullet3d_lifetime_test.goc", dmHashString64("/bullet3d_lifetime_teardown"), 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    ASSERT_TRUE(dmGameObject::Update(collection, &m_UpdateContext));
    ASSERT_TRUE(dmGameObject::PostUpdate(collection));

    dmGameObject::HInstance primary_go = Spawn(m_Factory, m_Collection, "/collision_object/bullet3d_query_static.goc", dmHashString64("/bullet3d_lifetime_primary"), 0, Point3(100, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, primary_go);

    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    lua_getglobal(L, "bullet3d_lifetime_ready");
    ASSERT_TRUE(lua_toboolean(L, -1));
    lua_pop(L, 1);

    uint32_t                      primary_component_type_index = dmGameObject::GetComponentTypeIndex(m_Collection, dmGameSystem::COLLISION_OBJECT_EXT_HASH);
    dmGameObject::HComponentWorld primary_component_world = dmGameObject::GetWorld(m_Collection, primary_component_type_index);
    ASSERT_NE((void*)0, primary_component_world);
    void* primary_world = dmGameSystem::CompCollisionObjectGetBullet3DWorld(primary_component_world);
    ASSERT_NE((void*)0, primary_world);
    dmGameSystem::PushBullet3DWorld(L, primary_world, primary_component_world);
    lua_setglobal(L, "bullet3d_primary_world");

    uint32_t                      primary_body_component_type = 0;
    dmGameObject::HComponent      primary_body_component = 0;
    dmGameObject::HComponentWorld primary_body_component_world = 0;
    ASSERT_EQ(dmGameObject::RESULT_OK,
              dmGameObject::GetComponent(primary_go, dmHashString64("collisionobject"), &primary_body_component_type, &primary_body_component, &primary_body_component_world));
    btCollisionObject* primary_body = (btCollisionObject*)dmGameSystem::CompCollisionObjectGetBullet3DCollisionObject(primary_body_component);
    ASSERT_NE((void*)0, primary_body);
    dmGameSystem::PushBullet3DCollisionObject(L, primary_body, m_Collection, dmGameObject::GetIdentifier(primary_go));
    lua_setglobal(L, "bullet3d_primary_body");

    ASSERT_TRUE(dmScriptTest::RunString(L,
                                        "assert(bullet3d.world.is_valid(bullet3d_primary_world))\n"
                                        "assert(bullet3d.world.is_valid(bullet3d_lifetime_world))\n"
                                        "assert(bullet3d_primary_world ~= bullet3d_lifetime_world)\n"
                                        "assert(bullet3d.collision_object.is_valid(bullet3d_lifetime_body))\n"
                                        "assert(bullet3d.rigid_body.is_valid(bullet3d_lifetime_body))\n"
                                        "assert(bullet3d.collision_object.is_valid(bullet3d_primary_body))\n"
                                        "assert(not pcall(bullet3d.world.contact_test, bullet3d_primary_world, bullet3d_lifetime_body))\n"
                                        "assert(not pcall(bullet3d.world.contact_pair_test, bullet3d_primary_world, bullet3d_lifetime_body, bullet3d_primary_body))\n"
                                        "assert(not pcall(bullet3d.world.contact_pair_test, bullet3d_primary_world, bullet3d_primary_body, bullet3d_lifetime_body))"));

    dmGameObject::DeleteCollection(collection);
    ASSERT_TRUE(dmGameObject::PostUpdate(m_Register));

    ASSERT_TRUE(dmScriptTest::RunString(L,
                                        "assert(bullet3d.world.is_valid(bullet3d_primary_world))\n"
                                        "assert(pcall(bullet3d.world.get_gravity, bullet3d_primary_world))\n"
                                        "assert(pcall(bullet3d.world.overlap_point, bullet3d_primary_world, vmath.vector3()))\n"
                                        "assert(not bullet3d.world.is_valid(bullet3d_lifetime_world))\n"
                                        "assert(not bullet3d.collision_object.is_valid(bullet3d_lifetime_body))\n"
                                        "assert(not bullet3d.rigid_body.is_valid(bullet3d_lifetime_body))\n"
                                        "assert(not pcall(bullet3d.world.get_gravity, bullet3d_lifetime_world))\n"
                                        "assert(not pcall(bullet3d.world.overlap_point, bullet3d_lifetime_world, vmath.vector3()))\n"
                                        "assert(not pcall(bullet3d.collision_object.get_collision_filter_group, bullet3d_lifetime_body))\n"
                                        "assert(not pcall(bullet3d.collision_object.get_collision_filter_mask, bullet3d_lifetime_body))\n"
                                        "assert(not pcall(bullet3d.rigid_body.get_mass, bullet3d_lifetime_body))"));
}

/* Physics listener */
TEST_F(ComponentTest, PhysicsListenerTest)
{
    /* Setup:
    ** callback_object
    ** - [collisionobject] collision_object/callback_object.collisionobject
    ** - [script] collision_object/callback_object.script
    ** callback_trigger
    ** - [collisionobject] collision_object/callback_trigger.collisionobject
    */

    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    const char* path_test_object = "/collision_object/callback_object.goc";
    const char* path_test_trigger = "/collision_object/callback_trigger.goc";

    dmhash_t hash_go_object = dmHashString64("/test_object");
    dmhash_t hash_go_trigger = dmHashString64("/test_trigger");

    dmGameObject::HInstance go_b = Spawn(m_Factory, m_Collection, path_test_object, hash_go_object, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_b);

    dmGameObject::HInstance go_a = Spawn(m_Factory, m_Collection, path_test_trigger, hash_go_trigger, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_a);

    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));

}

/* Update mass for physics collision object */
TEST_F(ComponentTest, PhysicsUpdateMassTest)
{
    /* Setup:
    ** mass_object
    ** - [collisionobject] collision_object/mass_object.collisionobject
    ** - [script] collision_object/mass_object.script
    */

    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    const char* path_test_object = "/collision_object/mass_object.goc";

    dmhash_t hash_go_object = dmHashString64("/test_object");

    dmGameObject::HInstance go_b = Spawn(m_Factory, m_Collection, path_test_object, hash_go_object, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go_b);

    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
        ASSERT_TRUE(dmGameObject::PostUpdate(m_Collection));

        // check if tests are done
        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

    ASSERT_TRUE(dmGameObject::Final(m_Collection));
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();

    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    dmHashEnableReverseHash(true);
    // Enable message descriptor translation when sending messages
    dmDDF::RegisterAllTypes();

    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmLog::LogFinalize();
    return result;
}
