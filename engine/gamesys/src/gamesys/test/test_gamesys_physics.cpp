#include <test_script.h>
#include <stddef.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <gamesys/components/comp_collision_object.h>
#include <gamesys/physics_ddf.h>

#include "test_gamesys.h"

using namespace dmVMath;

static void AssertLua(lua_State* L, const char* source)
{
    int result = luaL_dostring(L, source);
    if (result != 0)
    {
        dmLogError("Lua assertion failed: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    ASSERT_EQ(0, result);
}

struct PhysicsCallbackTestInstance
{
    int      m_ContextTableRef;
    uint32_t m_UniqueScriptId;
};

static int GetPhysicsCallbackTestContextTableRef(lua_State* L)
{
    PhysicsCallbackTestInstance* instance = (PhysicsCallbackTestInstance*) lua_touserdata(L, 1);
    lua_pushnumber(L, instance->m_ContextTableRef);
    return 1;
}

static int GetPhysicsCallbackTestUniqueScriptId(lua_State* L)
{
    PhysicsCallbackTestInstance* instance = (PhysicsCallbackTestInstance*) lua_touserdata(L, 1);
    lua_pushinteger(L, instance->m_UniqueScriptId);
    return 1;
}

static int IsPhysicsCallbackTestInstanceValid(lua_State* L)
{
    lua_pushboolean(L, lua_touserdata(L, 1) != 0);
    return 1;
}

static const luaL_reg PHYSICS_CALLBACK_TEST_INSTANCE_METHODS[] =
{
    { 0, 0 }
};

static const luaL_reg PHYSICS_CALLBACK_TEST_INSTANCE_META[] =
{
    { dmScript::META_TABLE_IS_VALID, IsPhysicsCallbackTestInstanceValid },
    { dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, GetPhysicsCallbackTestContextTableRef },
    { dmScript::META_GET_UNIQUE_SCRIPT_ID, GetPhysicsCallbackTestUniqueScriptId },
    { 0, 0 }
};

struct PhysicsTestCallback
{
    dmScript::LuaCallbackInfo* m_Callback;
    int                        m_InstanceRef;
    int                        m_ContextTableRef;
    int                        m_PreviousInstanceRef;
};

static PhysicsTestCallback CreatePhysicsTestCallback(lua_State* L, const char* function_name)
{
    dmScript::GetInstance(L);
    int previous_instance_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

    dmScript::RegisterUserType(L, "PhysicsCallbackTestInstance",
                               PHYSICS_CALLBACK_TEST_INSTANCE_METHODS,
                               PHYSICS_CALLBACK_TEST_INSTANCE_META);

    PhysicsCallbackTestInstance* instance = (PhysicsCallbackTestInstance*) lua_newuserdata(L, sizeof(PhysicsCallbackTestInstance));
    luaL_getmetatable(L, "PhysicsCallbackTestInstance");
    lua_setmetatable(L, -2);
    lua_newtable(L);
    instance->m_ContextTableRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
    instance->m_UniqueScriptId = dmScript::GenerateUniqueScriptId();
    int instance_ref = dmScript::Ref(L, LUA_REGISTRYINDEX);

    lua_rawgeti(L, LUA_REGISTRYINDEX, instance_ref);
    dmScript::SetInstance(L);

    lua_getglobal(L, function_name);
    dmScript::LuaCallbackInfo* callback = dmScript::CreateCallback(L, -1);
    lua_pop(L, 1);

    PhysicsTestCallback result = { callback, instance_ref, instance->m_ContextTableRef, previous_instance_ref };
    return result;
}

static void DestroyPhysicsTestCallback(lua_State* L, PhysicsTestCallback* callback)
{
    dmScript::DestroyCallback(callback->m_Callback);
    dmScript::Unref(L, LUA_REGISTRYINDEX, callback->m_InstanceRef);
    dmScript::Unref(L, LUA_REGISTRYINDEX, callback->m_ContextTableRef);

    lua_rawgeti(L, LUA_REGISTRYINDEX, callback->m_PreviousInstanceRef);
    dmScript::SetInstance(L);
    dmScript::Unref(L, LUA_REGISTRYINDEX, callback->m_PreviousInstanceRef);
}

static void RunPhysicsScriptTest(dmResource::HFactory factory, dmGameObject::HCollection collection, dmGameObject::UpdateContext* update_context,
                                 dmScript::HContext script_context, const char* prototype_path, const char* instance_path)
{
    dmHashEnableReverseHash(true);
    lua_State* L = dmScript::GetLuaState(script_context);

    dmGameObject::HInstance go = Spawn(factory, collection, prototype_path, dmHashString64(instance_path), 0,
                                       Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    bool tests_done = false;
    while (!tests_done)
    {
        ASSERT_TRUE(dmGameObject::Update(collection, update_context));
        ASSERT_TRUE(dmGameObject::PostUpdate(collection));

        lua_getglobal(L, "tests_done");
        tests_done = lua_toboolean(L, -1);
        lua_pop(L, 1);
    }

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

TEST_F(ComponentTest, PhysicsDDFDecoderTest)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);

    dmPhysicsDDF::CollisionResponse collision = {};
    collision.m_OtherId = dmHashString64("other");
    collision.m_Group = dmHashString64("group");
    collision.m_OtherPosition = Point3(1.0f, 2.0f, 3.0f);
    collision.m_OtherGroup = dmHashString64("other_group");
    collision.m_OwnGroup = dmHashString64("own_group");
    dmScript::PushDDF(L, dmPhysicsDDF::CollisionResponse::m_DDFDescriptor, (const char*)&collision, false);
    lua_setglobal(L, "decoded_collision");

    dmPhysicsDDF::ContactPointResponse contact = {};
    contact.m_Position = Point3(4.0f, 5.0f, 6.0f);
    contact.m_Normal = Vector3(0.0f, 1.0f, 0.0f);
    contact.m_RelativeVelocity = Vector3(7.0f, 8.0f, 9.0f);
    contact.m_Distance = 0.25f;
    contact.m_AppliedImpulse = 2.0f;
    contact.m_LifeTime = 3.0f;
    contact.m_Mass = 4.0f;
    contact.m_OtherMass = 5.0f;
    contact.m_OtherId = dmHashString64("other");
    contact.m_OtherPosition = Point3(10.0f, 11.0f, 12.0f);
    contact.m_Group = dmHashString64("group");
    contact.m_OtherGroup = dmHashString64("other_group");
    contact.m_OwnGroup = dmHashString64("own_group");
    dmScript::PushDDF(L, dmPhysicsDDF::ContactPointResponse::m_DDFDescriptor, (const char*)&contact, false);
    lua_setglobal(L, "decoded_contact");

    dmPhysicsDDF::TriggerResponse trigger = {};
    trigger.m_OtherId = dmHashString64("other");
    trigger.m_Enter = true;
    trigger.m_Group = dmHashString64("group");
    trigger.m_OtherGroup = dmHashString64("other_group");
    trigger.m_OwnGroup = dmHashString64("own_group");
    dmScript::PushDDF(L, dmPhysicsDDF::TriggerResponse::m_DDFDescriptor, (const char*)&trigger, false);
    lua_setglobal(L, "decoded_trigger");

    dmPhysicsDDF::RayCastResponse ray = {};
    ray.m_Fraction = 0.5f;
    ray.m_Position = Point3(13.0f, 14.0f, 15.0f);
    ray.m_Normal = Vector3(0.0f, 0.0f, 1.0f);
    ray.m_Id = dmHashString64("other");
    ray.m_Group = dmHashString64("group");
    ray.m_RequestId = 17;
    dmScript::PushDDF(L, dmPhysicsDDF::RayCastResponse::m_DDFDescriptor, (const char*)&ray, false);
    lua_setglobal(L, "decoded_ray");

    dmPhysicsDDF::RayCastMissed missed = {};
    missed.m_RequestId = 18;
    dmScript::PushDDF(L, dmPhysicsDDF::RayCastMissed::m_DDFDescriptor, (const char*)&missed, false);
    lua_setglobal(L, "decoded_missed");

    dmPhysicsDDF::VelocityResponse velocity = {};
    velocity.m_LinearVelocity = Vector3(1.0f, 2.0f, 3.0f);
    velocity.m_AngularVelocity = Vector3(4.0f, 5.0f, 6.0f);
    dmScript::PushDDF(L, dmPhysicsDDF::VelocityResponse::m_DDFDescriptor, (const char*)&velocity, false);
    lua_setglobal(L, "decoded_velocity");

    AssertLua(L,
        "assert(decoded_collision.other_id == hash('other'))\n"
        "assert(decoded_collision.group == hash('group'))\n"
        "assert(decoded_collision.other_position == vmath.vector3(1, 2, 3))\n"
        "assert(decoded_collision.other_group == hash('other_group'))\n"
        "assert(decoded_collision.own_group == hash('own_group'))\n"
        "assert(decoded_contact.position == vmath.vector3(4, 5, 6))\n"
        "assert(decoded_contact.normal == vmath.vector3(0, 1, 0))\n"
        "assert(decoded_contact.relative_velocity == vmath.vector3(7, 8, 9))\n"
        "assert(decoded_contact.distance == 0.25)\n"
        "assert(decoded_contact.applied_impulse == 2)\n"
        "assert(decoded_contact.life_time == 3)\n"
        "assert(decoded_contact.mass == 4)\n"
        "assert(decoded_contact.other_mass == 5)\n"
        "assert(decoded_contact.other_id == hash('other'))\n"
        "assert(decoded_contact.other_position == vmath.vector3(10, 11, 12))\n"
        "assert(decoded_contact.group == hash('group'))\n"
        "assert(decoded_contact.other_group == hash('other_group'))\n"
        "assert(decoded_contact.own_group == hash('own_group'))\n"
        "assert(decoded_trigger.other_id == hash('other') and decoded_trigger.enter)\n"
        "assert(decoded_trigger.group == hash('group'))\n"
        "assert(decoded_trigger.other_group == hash('other_group'))\n"
        "assert(decoded_trigger.own_group == hash('own_group'))\n"
        "assert(decoded_ray.fraction == 0.5)\n"
        "assert(decoded_ray.position == vmath.vector3(13, 14, 15))\n"
        "assert(decoded_ray.normal == vmath.vector3(0, 0, 1))\n"
        "assert(decoded_ray.id == hash('other') and decoded_ray.group == hash('group'))\n"
        "assert(decoded_ray.request_id == 17 and decoded_missed.request_id == 18)\n"
        "assert(decoded_velocity.linear_velocity == vmath.vector3(1, 2, 3))\n"
        "assert(decoded_velocity.angular_velocity == vmath.vector3(4, 5, 6))\n");

    lua_pushnil(L); lua_setglobal(L, "decoded_collision");
    lua_pushnil(L); lua_setglobal(L, "decoded_contact");
    lua_pushnil(L); lua_setglobal(L, "decoded_trigger");
    lua_pushnil(L); lua_setglobal(L, "decoded_ray");
    lua_pushnil(L); lua_setglobal(L, "decoded_missed");
    lua_pushnil(L); lua_setglobal(L, "decoded_velocity");
}

TEST_F(ComponentTest, PhysicsBatchedEventDecoderTest)
{
    lua_State* L = dmScript::GetLuaState(m_ScriptContext);
    AssertLua(L,
        "function capture_physics_events(self, events)\n"
        "    decoded_physics_events = events\n"
        "end\n");

    PhysicsTestCallback callback = CreatePhysicsTestCallback(L, "capture_physics_events");
    ASSERT_NE((dmScript::LuaCallbackInfo*) 0, callback.m_Callback);

    struct PhysicsEventBatchPayload
    {
        alignas(16) dmPhysicsDDF::CollisionEvent    m_Collision;
        alignas(16) dmPhysicsDDF::ContactPointEvent m_Contact;
        alignas(16) dmPhysicsDDF::TriggerEvent      m_Trigger;
        alignas(16) dmPhysicsDDF::RayCastResponse   m_RayCastResponse;
        alignas(16) dmPhysicsDDF::RayCastMissed     m_RayCastMissed;
    } payload = {};

    payload.m_Collision.m_A.m_Position = Point3(1.0f, 2.0f, 3.0f);
    payload.m_Collision.m_A.m_Id = dmHashString64("collision_a");
    payload.m_Collision.m_A.m_Group = dmHashString64("group_a");
    payload.m_Collision.m_B.m_Position = Point3(4.0f, 5.0f, 6.0f);
    payload.m_Collision.m_B.m_Id = dmHashString64("collision_b");
    payload.m_Collision.m_B.m_Group = dmHashString64("group_b");

    payload.m_Contact.m_A.m_Position = Point3(7.0f, 8.0f, 9.0f);
    payload.m_Contact.m_A.m_InstancePosition = Point3(10.0f, 11.0f, 12.0f);
    payload.m_Contact.m_A.m_Normal = Vector3(0.0f, 1.0f, 0.0f);
    payload.m_Contact.m_A.m_RelativeVelocity = Vector3(1.0f, 2.0f, 3.0f);
    payload.m_Contact.m_A.m_Mass = 1.25f;
    payload.m_Contact.m_A.m_Id = dmHashString64("contact_a");
    payload.m_Contact.m_A.m_Group = dmHashString64("group_a");
    payload.m_Contact.m_B.m_Position = Point3(13.0f, 14.0f, 15.0f);
    payload.m_Contact.m_B.m_InstancePosition = Point3(16.0f, 17.0f, 18.0f);
    payload.m_Contact.m_B.m_Normal = Vector3(0.0f, -1.0f, 0.0f);
    payload.m_Contact.m_B.m_RelativeVelocity = Vector3(-1.0f, -2.0f, -3.0f);
    payload.m_Contact.m_B.m_Mass = 2.5f;
    payload.m_Contact.m_B.m_Id = dmHashString64("contact_b");
    payload.m_Contact.m_B.m_Group = dmHashString64("group_b");
    payload.m_Contact.m_Distance = 0.25f;
    payload.m_Contact.m_AppliedImpulse = 3.5f;

    payload.m_Trigger.m_Enter = true;
    payload.m_Trigger.m_A.m_Id = dmHashString64("trigger_a");
    payload.m_Trigger.m_A.m_Group = dmHashString64("group_a");
    payload.m_Trigger.m_B.m_Id = dmHashString64("trigger_b");
    payload.m_Trigger.m_B.m_Group = dmHashString64("group_b");

    payload.m_RayCastResponse.m_Fraction = 0.75f;
    payload.m_RayCastResponse.m_Position = Point3(19.0f, 20.0f, 21.0f);
    payload.m_RayCastResponse.m_Normal = Vector3(0.0f, 0.0f, 1.0f);
    payload.m_RayCastResponse.m_Id = dmHashString64("ray_id");
    payload.m_RayCastResponse.m_Group = dmHashString64("ray_group");
    payload.m_RayCastResponse.m_RequestId = 42;
    payload.m_RayCastMissed.m_RequestId = 43;

    dmGameSystem::PhysicsMessage messages[] =
    {
        { (uint32_t) offsetof(PhysicsEventBatchPayload, m_Collision),       dmGameSystem::PHYSICS_MESSAGE_TYPE_COLLISION },
        { (uint32_t) offsetof(PhysicsEventBatchPayload, m_Contact),         dmGameSystem::PHYSICS_MESSAGE_TYPE_CONTACT_POINT },
        { (uint32_t) offsetof(PhysicsEventBatchPayload, m_Trigger),         dmGameSystem::PHYSICS_MESSAGE_TYPE_TRIGGER },
        { (uint32_t) offsetof(PhysicsEventBatchPayload, m_RayCastResponse), dmGameSystem::PHYSICS_MESSAGE_TYPE_RAY_CAST_RESPONSE },
        { (uint32_t) offsetof(PhysicsEventBatchPayload, m_RayCastMissed),   dmGameSystem::PHYSICS_MESSAGE_TYPE_RAY_CAST_MISSED },
    };

    dmGameSystem::RunBatchedEventCallback(callback.m_Callback, DM_ARRAY_SIZE(messages), messages, (const uint8_t*) &payload);

    AssertLua(L,
        "local events = decoded_physics_events\n"
        "assert(#events == 5)\n"
        "assert(events[1].type == hash('collision_event'))\n"
        "assert(events[1].a.position == vmath.vector3(1, 2, 3))\n"
        "assert(events[1].a.id == hash('collision_a') and events[1].a.group == hash('group_a'))\n"
        "assert(events[1].b.position == vmath.vector3(4, 5, 6))\n"
        "assert(events[1].b.id == hash('collision_b') and events[1].b.group == hash('group_b'))\n"
        "assert(events[2].type == hash('contact_point_event'))\n"
        "assert(events[2].a.position == vmath.vector3(7, 8, 9))\n"
        "assert(events[2].a.instance_position == vmath.vector3(10, 11, 12))\n"
        "assert(events[2].a.normal == vmath.vector3(0, 1, 0))\n"
        "assert(events[2].a.relative_velocity == vmath.vector3(1, 2, 3))\n"
        "assert(events[2].a.mass == 1.25)\n"
        "assert(events[2].a.id == hash('contact_a') and events[2].a.group == hash('group_a'))\n"
        "assert(events[2].b.position == vmath.vector3(13, 14, 15))\n"
        "assert(events[2].b.instance_position == vmath.vector3(16, 17, 18))\n"
        "assert(events[2].b.normal == vmath.vector3(0, -1, 0))\n"
        "assert(events[2].b.relative_velocity == vmath.vector3(-1, -2, -3))\n"
        "assert(events[2].b.mass == 2.5)\n"
        "assert(events[2].b.id == hash('contact_b') and events[2].b.group == hash('group_b'))\n"
        "assert(events[2].distance == 0.25 and events[2].applied_impulse == 3.5)\n"
        "assert(events[3].type == hash('trigger_event') and events[3].enter)\n"
        "assert(events[3].a.id == hash('trigger_a') and events[3].a.group == hash('group_a'))\n"
        "assert(events[3].b.id == hash('trigger_b') and events[3].b.group == hash('group_b'))\n"
        "assert(events[4].type == hash('ray_cast_response'))\n"
        "assert(events[4].fraction == 0.75)\n"
        "assert(events[4].position == vmath.vector3(19, 20, 21))\n"
        "assert(events[4].normal == vmath.vector3(0, 0, 1))\n"
        "assert(events[4].id == hash('ray_id') and events[4].group == hash('ray_group'))\n"
        "assert(events[4].request_id == 42)\n"
        "assert(events[5].type == hash('ray_cast_missed') and events[5].request_id == 43)\n");

    lua_pushnil(L); lua_setglobal(L, "decoded_physics_events");
    lua_pushnil(L); lua_setglobal(L, "capture_physics_events");
    DestroyPhysicsTestCallback(L, &callback);
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
