#include <gtest/gtest.h>

#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/easing.h>
#include <dlib/time.h>

#include "../gameobject.h"
#include "../gameobject_private.h"

using namespace Vectormath::Aos;

class AnimTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test/anim");
        dmScript::NewContextParams sc_params;
        m_ScriptContext = dmScript::NewContext(&sc_params);
        dmScript::Initialize(m_ScriptContext);
        dmGameObject::Initialize(m_ScriptContext);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register, m_ScriptContext, &m_ModuleContext);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register, m_ScriptContext);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, dmGameObject::GetCollectionDefaultCapacity(m_Register), dmGameObject::GetCollectionDefaultRigCapacity(m_Register));
        m_FinishCount = 0;
        m_CancelCount = 0;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmScript::Finalize(m_ScriptContext);
        dmScript::DeleteContext(m_ScriptContext);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
    }

public:
    dmScript::HContext m_ScriptContext;
    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
    dmGameObject::ModuleContext m_ModuleContext;
    uint32_t m_FinishCount;
    uint32_t m_CancelCount;
};

#define EPSILON 0.000001f

void AnimationStopped(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                    bool finished, void* userdata1, void* userdata2)
{
    AnimTest* test = (AnimTest*)userdata1;
    if (finished)
        ++test->m_FinishCount;
    else
        ++test->m_CancelCount;
}

static float X(dmGameObject::HInstance instance)
{
    return dmGameObject::GetPosition(instance).getX();
}

static dmhash_t hash(const char* s)
{
    return dmHashString64(s);
}

TEST_F(AnimTest, AnimateAndStop)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vectormath::Aos::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;
    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id,
            dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR),
            duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(2.5f, X(go), EPSILON);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(5.0f, X(go), EPSILON);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(7.5f, X(go), EPSILON);
    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(0u, this->m_CancelCount);
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    ASSERT_NEAR(10.0f, X(go), EPSILON);
    ASSERT_EQ(1u, this->m_FinishCount);
    ASSERT_EQ(0u, this->m_CancelCount);
    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Playback)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vectormath::Aos::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;

    Point3 pos;

#define ANIM(playback)\
    Animate(m_Collection, go, 0, id, playback, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);

#define ASSERT_FRAME(expected)\
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));\
    ASSERT_NEAR(expected, X(go), EPSILON);

    ANIM(dmGameObject::PLAYBACK_ONCE_FORWARD);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(10.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_ONCE_BACKWARD);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_ONCE_PINGPONG);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_NONE);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_FORWARD);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(2.5f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_BACKWARD);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(7.5f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_PINGPONG);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(0.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(0.0f);

    ASSERT_EQ(3u, this->m_FinishCount);
    ASSERT_EQ(3u, this->m_CancelCount);

#undef ANIM
#undef ASSERT_FRAME

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Cancel)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vectormath::Aos::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;

    Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, go, 0, id));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(1u, this->m_CancelCount);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, AnimateEuler)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("euler.z");
    dmGameObject::PropertyVar var(360.0f);
    float duration = 1.0f;
    float delay = 0.f;
    // Higher epsilon because of low precision euler conversions
    const float epsilon = 0.0001f;
    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id,
            dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR),
            duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    Quat r;

#define ASSERT_FRAME(q_z, q_w)\
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));\
    r = dmGameObject::GetRotation(go);\
    ASSERT_NEAR(q_z, r.getZ(), epsilon);\
    ASSERT_NEAR(q_w, r.getW(), epsilon);

    ASSERT_FRAME(M_SQRT1_2, M_SQRT1_2);

    ASSERT_FRAME(1, 0.0f);

    ASSERT_FRAME(M_SQRT1_2, -M_SQRT1_2);

    ASSERT_FRAME(0.0f, -1);

#undef ASSERT_FRAME
}

void AnimationStoppedToDelete(dmGameObject::HInstance instance, dmhash_t component_id, dmhash_t property_id,
                                    bool finished, void* userdata1, void* userdata2)
{
    AnimTest* test = (AnimTest*)userdata1;
    if (finished)
        ++test->m_FinishCount;
    else
        ++test->m_CancelCount;
    *((dmhash_t*)userdata2) = instance->m_Identifier;
}

static dmGameObject::HInstance Spawn(dmResource::HFactory factory, dmGameObject::HCollection collection, const char* prototype_name, dmhash_t id, uint8_t* property_buffer, uint32_t property_buffer_size, const Point3& position, const Quat& rotation, const Vector3& scale)
{
    dmGameObject::HPrototype prototype = 0x0;
    if (dmResource::Get(factory, prototype_name, (void**)&prototype) == dmResource::RESULT_OK) {
        dmGameObject::HInstance result = dmGameObject::Spawn(collection, prototype, prototype_name, id, property_buffer, property_buffer_size, position, rotation, scale);
        dmResource::Release(factory, prototype);
        return result;
    }
    return 0x0;
}

TEST_F(AnimTest, DeleteInAnim)
{
    const uint32_t instance_count = 3;
    dmGameObject::HInstance gos[instance_count];
    dmhash_t orig_instance_ids[instance_count];
    uint32_t order[instance_count] = {1, 0, 2};
    char id_buffer[32];
    const char* id_fmt = "test_id%d";
    for (uint32_t i = 0; i < instance_count; ++i)
    {
        gos[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        DM_SNPRINTF(id_buffer, 32, id_fmt, i);
        orig_instance_ids[i] = dmHashString64(id_buffer);
        dmGameObject::SetIdentifier(m_Collection, gos[i], id_buffer);
    }

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vectormath::Aos::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;
    dmhash_t instance_id = 0;

    for (uint32_t i = 0; i < instance_count; ++i)
    {
        Animate(m_Collection, gos[i], 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStoppedToDelete, this, &instance_id);
    }

    for (uint32_t i = 0; i < instance_count; ++i)
    {
        dmGameObject::Update(m_Collection, &m_UpdateContext);

        dmGameObject::Delete(m_Collection, gos[order[i]], false);

        dmGameObject::PostUpdate(m_Collection);

        ASSERT_EQ(0u, this->m_FinishCount);
        ASSERT_EQ(i+1, this->m_CancelCount);
        ASSERT_EQ(orig_instance_ids[order[i]], instance_id);
    }
    dmGameObject::Update(m_Collection, &m_UpdateContext);
}

// Tests that animation with duration=0 is equivalent with "set" of the target value
TEST_F(AnimTest, ZeroDuration)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 0.0f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(10.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Delay)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 1.0f;
    float delay = 1.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    Point3 pos;

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_LT(0.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, DelayAboveDuration)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 1.0f;
    float delay = 2.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    Point3 pos;

    for (uint32_t i = 0; i < 8; ++i)
    {
        dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_EQ(0.0f, X(go));
    }

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_LT(0.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

// Test that a delayed animation is not stopped when a new is started immediately
TEST_F(AnimTest, DelayedNotStopped)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var_delay(1.f);
    dmGameObject::PropertyVar var_immediate(0.5f);
    float duration = 0.75f;
    float delay = 0.25f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var_delay, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var_immediate, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, 0.0f, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    Point3 pos;

    for (uint32_t i = 0; i < 4; ++i)
    {
        dmGameObject::Update(m_Collection, &m_UpdateContext);
        ASSERT_LT(0.0f, X(go));
    }

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(1.0f, X(go));

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, LoadTest)
{
    const uint32_t count = 1024;
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vector3(10.0f, 0.0f, 0.0f));
    float duration = 1.0f;
    float delay = 0.0f;

    dmGameObject::HInstance gos[count];
    for (uint32_t i = 0; i < count; ++i)
    {
        gos[i] = dmGameObject::New(m_Collection, "/dummy.goc");
        dmGameObject::PropertyResult result = Animate(m_Collection, gos[i], 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
        ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);
    }

    uint64_t time = dmTime::GetTime();
    dmGameObject::Update(m_Collection, &m_UpdateContext);
    uint64_t delta = dmTime::GetTime() - time;

    printf("%d animations started in %.3f ms\n", count*4, delta * 0.001);

    time = dmTime::GetTime();
    dmGameObject::Update(m_Collection, &m_UpdateContext);
    delta = dmTime::GetTime() - time;

    printf("%d animations simulated in %.3f ms\n", count*3, delta * 0.001);

    for (uint32_t i = 0; i < count; ++i)
    {
        dmGameObject::Delete(m_Collection, gos[i], false);
    }
}

TEST_F(AnimTest, LinkedList)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    float duration = 1.0f;
    float delay = 0.0f;
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmhash_t ids[] = {hash("position.x"), hash("position.y"), hash("position.z")};
    Point3 p;

#define ANIM\
    dmGameObject::SetPosition(go, Point3(0, 0, 0));\
    Animate(m_Collection, go, 0, ids[0], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);\
    Animate(m_Collection, go, 0, ids[1], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);\
    Animate(m_Collection, go, 0, ids[2], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, AnimationStopped, this, 0x0);
#define ASSERT_FRAME(x, y, z)\
    dmGameObject::Update(m_Collection, &m_UpdateContext);\
    p = dmGameObject::GetPosition(go);\
    ASSERT_NEAR(x, p.getX(), EPSILON);\
    ASSERT_NEAR(y, p.getY(), EPSILON);\
    ASSERT_NEAR(z, p.getZ(), EPSILON);

    // Cancel head
    ANIM;
    dmGameObject::CancelAnimations(m_Collection, go, 0, ids[0]);
    ASSERT_FRAME(0, 0.25, 0.25);
    dmGameObject::CancelAnimations(m_Collection, go);
    ASSERT_FRAME(0, 0.25, 0.25);

    // Cancel middle
    ANIM;
    dmGameObject::CancelAnimations(m_Collection, go, 0, ids[1]);
    ASSERT_FRAME(0.25, 0, 0.25);
    dmGameObject::CancelAnimations(m_Collection, go);
    ASSERT_FRAME(0.25, 0, 0.25);

    // Cancel tail
    ANIM;
    dmGameObject::CancelAnimations(m_Collection, go, 0, ids[2]);
    ASSERT_FRAME(0.25, 0.25, 0);
    dmGameObject::CancelAnimations(m_Collection, go);
    ASSERT_FRAME(0.25, 0.25, 0);

#undef ANIM
#undef ASSERT_FRAME
}

TEST_F(AnimTest, ScriptedRestart)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/restart.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedCancel)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/cancel.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedAnimBadURL)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/anim_bad_url.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_EQ(0, go);
}

TEST_F(AnimTest, ScriptedCancelBadURL)
{
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/cancel_bad_url.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_EQ(0, go);
}

TEST_F(AnimTest, ScriptedChainOtherProp)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/chain_other_prop.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 12; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedChainDelayBug)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/chain_delay_bug.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 12; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

// Test that relocating anims through a SetCapacity (via callback) works
TEST_F(AnimTest, ScriptedDemo)
{
    uint32_t count = 257;
    char id[8];
    for (uint32_t i = 0; i < count; ++i)
    {
        DM_SNPRINTF(id, 8, "box%d", i + 1);
        dmGameObject::HInstance box = Spawn(m_Factory, m_Collection, "/demo_box.goc", hash(id), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
        ASSERT_NE((void*)0, box);
    }
    dmGameObject::HInstance demo = Spawn(m_Factory, m_Collection, "/demo.goc", hash("demo"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, demo);

    uint32_t frame_count = 1000;
    m_UpdateContext.m_DT = 0.25f;

    for (uint32_t i = 0; i < frame_count; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedInvalidType)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/invalid_type.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_EQ((void*)0, go);
}

TEST_F(AnimTest, ScriptedDelayedCompositeCallback)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/composite_delay.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}


TEST_F(AnimTest, ScriptedCustomEasing)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/custom_easing.goc", hash("custom_easing"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedChainedEasing)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    dmGameObject::HInstance go = Spawn(m_Factory, m_Collection, "/chained_easing.goc", hash("chained_easing"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), Vector3(1, 1, 1));
    ASSERT_NE((void*)0, go);

    for (uint32_t i = 0; i < 20; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, PositionUniformAnim)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetPosition(go, Point3(1.f, 2.f, 3.f));
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    Point3 position = dmGameObject::GetPosition(go);
    ASSERT_NEAR(2.0f, position.getX(), 0.000001f);
    ASSERT_NEAR(2.0f, position.getY(), 0.000001f);
    ASSERT_NEAR(2.0f, position.getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

// Test that the 3 component scale can be animated as a uniform scale (legacy)
TEST_F(AnimTest, UniformScale)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetScale(go, 1.0f);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(2.0f, dmGameObject::GetUniformScale(go), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleUniformAnim)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetScale(go, Vector3(1.f, 2.f, 3.f));
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    Vector3 scale = dmGameObject::GetScale(go);
    ASSERT_NEAR(2.0f, scale.getX(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getY(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, Scale)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    dmGameObject::SetScale(go, Vector3(1.f, 2.f, 3.f));
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale");
    dmGameObject::PropertyVar var(Vector3(2.f));
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    Vector3 scale = dmGameObject::GetScale(go);
    ASSERT_NEAR(2.0f, scale.getX(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getY(), 0.000001f);
    ASSERT_NEAR(2.0f, scale.getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleX)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::SetScale(go, 1.0);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale.x");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(2.0f, dmGameObject::GetScale(go).getX(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getY(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleY)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::SetScale(go, 1.0);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale.y");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getX(), 0.000001f);
    ASSERT_NEAR(2.0f, dmGameObject::GetScale(go).getY(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

TEST_F(AnimTest, ScaleZ)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");
    dmGameObject::SetScale(go, 1.0);
    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("scale.z");
    dmGameObject::PropertyVar var(2.f);
    float duration = 0.25f;
    float delay = 0.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::Curve(dmEasing::TYPE_LINEAR), duration, delay, 0x0, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, result);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getX(), 0.000001f);
    ASSERT_NEAR(1.0f, dmGameObject::GetScale(go).getY(), 0.000001f);
    ASSERT_NEAR(2.0f, dmGameObject::GetScale(go).getZ(), 0.000001f);

    dmGameObject::Delete(m_Collection, go, false);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
