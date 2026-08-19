#include <test_script.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
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
