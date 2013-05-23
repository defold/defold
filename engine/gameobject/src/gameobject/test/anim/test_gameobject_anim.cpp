#include <gtest/gtest.h>

#include <stdio.h>

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
        m_ScriptContext = dmScript::NewContext(0);
        dmGameObject::Initialize(m_ScriptContext, m_Factory);
        m_Register = dmGameObject::NewRegister();
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection("collection", m_Factory, m_Register, 1024);
        m_FinishCount = 0;
        m_CancelCount = 0;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmGameObject::PostUpdate(m_Register);
        dmGameObject::Finalize(m_Factory);
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
            dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR,
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
    dmGameObject::Delete(m_Collection, go);
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
    Animate(m_Collection, go, 0, id, playback, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);

#define ASSERT_FRAME(expected)\
    ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));\
    ASSERT_NEAR(expected, X(go), EPSILON);

    ANIM(dmGameObject::PLAYBACK_ONCE_FORWARD);
    ASSERT_FRAME(2.5f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_ONCE_BACKWARD);
    ASSERT_FRAME(7.5f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_NONE);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_FORWARD);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(0.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_BACKWARD);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(10.0f);

    dmGameObject::SetPosition(go, Point3(0.0f, 0.0f, 0.0f));
    ANIM(dmGameObject::PLAYBACK_LOOP_PINGPONG);
    ASSERT_FRAME(2.5f);
    ASSERT_FRAME(5.0f);
    ASSERT_FRAME(7.5f);
    ASSERT_FRAME(10.0f);
    ASSERT_FRAME(7.5f);

    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(5u, this->m_CancelCount);

#undef ANIM
#undef ASSERT_FRAME

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(AnimTest, Cancel)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vectormath::Aos::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;

    Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);
    ASSERT_EQ(dmGameObject::PROPERTY_RESULT_OK, CancelAnimations(m_Collection, go, 0, id));

    dmGameObject::Update(m_Collection, &m_UpdateContext);
    ASSERT_EQ(0.0f, X(go));

    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(1u, this->m_CancelCount);

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(AnimTest, DeleteInAnim)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position");
    dmGameObject::PropertyVar var(Vectormath::Aos::Vector3(10.f, 0.f, 0.f));
    float duration = 1.0f;
    float delay = 0.f;

    Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    dmGameObject::Delete(m_Collection, go);

    dmGameObject::PostUpdate(m_Collection);

    dmGameObject::Update(m_Collection, &m_UpdateContext);

    ASSERT_EQ(0u, this->m_FinishCount);
    ASSERT_EQ(1u, this->m_CancelCount);
}

TEST_F(AnimTest, Delay)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "/dummy.goc");

    m_UpdateContext.m_DT = 0.25f;
    dmhash_t id = hash("position.x");
    dmGameObject::PropertyVar var(10.f);
    float duration = 1.0f;
    float delay = 1.0f;

    dmGameObject::PropertyResult result = Animate(m_Collection, go, 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);
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

    dmGameObject::Delete(m_Collection, go);
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
        dmGameObject::PropertyResult result = Animate(m_Collection, gos[i], 0, id, dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);
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
        dmGameObject::Delete(m_Collection, gos[i]);
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
    Animate(m_Collection, go, 0, ids[0], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);\
    Animate(m_Collection, go, 0, ids[1], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);\
    Animate(m_Collection, go, 0, ids[2], dmGameObject::PLAYBACK_ONCE_FORWARD, var, dmEasing::TYPE_LINEAR, duration, delay, AnimationStopped, this, 0x0);
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
    float duration = 1.0f;
    float delay = 0.0f;
    dmGameObject::HInstance go = dmGameObject::Spawn(m_Collection, "/restart.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), 1);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

TEST_F(AnimTest, ScriptedCancel)
{
    m_UpdateContext.m_DT = 0.25f;
    dmGameObject::PropertyVar var(1.0f);
    float duration = 1.0f;
    float delay = 0.0f;
    dmGameObject::HInstance go = dmGameObject::Spawn(m_Collection, "/cancel.goc", hash("test"), 0, 0, Point3(0, 0, 0), Quat(0, 0, 0, 1), 1);

    for (uint32_t i = 0; i < 10; ++i)
    {
        ASSERT_TRUE(dmGameObject::Update(m_Collection, &m_UpdateContext));
    }
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
