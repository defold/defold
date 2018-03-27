#include <gtest/gtest.h>
#include "../script.h"


class ScriptTimerTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmScript::NewContext(0x0, 0, true);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);
    }

    virtual void TearDown()
    {
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
    }

    dmScript::HContext m_Context;
    lua_State* L;
};

TEST(ScriptTimerTest, TestCreateDeleteContext)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);
    dmScript::DeleteTimerContext(timer_context);
}

TEST(ScriptTimerTest, TestCreateDeleteTimer)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    bool cancelled = dmScript::CancelTimer(timer_context, 0);
    ASSERT_EQ(cancelled, false);
    uint32_t id = dmScript::AddTimer(timer_context, 0.016f, 0x0, 0x0, 0x0, false);
    ASSERT_NE(id, INVALID_TIMER_ID);
    cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(cancelled, true);
    cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(cancelled, false);

    dmScript::DeleteTimerContext(timer_context);
}

TEST(ScriptTimerTest, TestIdReuse)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    uint32_t id1 = dmScript::AddTimer(timer_context, 0.016f, 0x0, 0x0, 0x0, false);
    uint32_t id2 = dmScript::AddTimer(timer_context, 0.016f, 0x0, 0x0, 0x0, false);
    ASSERT_NE(id1, id2);
    bool cancelled = dmScript::CancelTimer(timer_context, id1);
    ASSERT_EQ(cancelled, true);
    uint32_t id3 = dmScript::AddTimer(timer_context, 0.016f, 0x0, 0x0, 0x0, false);
    ASSERT_NE(id1, id2);
    ASSERT_NE(id1, id3);
    cancelled = dmScript::CancelTimer(timer_context, id2);
    ASSERT_EQ(cancelled, true);
    cancelled = dmScript::CancelTimer(timer_context, id3);
    ASSERT_EQ(cancelled, true);
    uint32_t id4 = dmScript::AddTimer(timer_context, 0.016f, 0x0, 0x0, 0x0, false);
    ASSERT_NE(id1, id4);
    cancelled = dmScript::CancelTimer(timer_context, id4);
    ASSERT_EQ(cancelled, true);

    dmScript::DeleteTimerContext(timer_context);
}

TEST(ScriptTimerTest, TestSameUserData1Timer)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(1);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    uint32_t userdata1[] =
    {
        1u
    };

    uint32_t userdata2[] =
    {
        10u,
        20u,
        30u,
        40u,
        50u
    };

    uint32_t ids[] = 
    {
        dmScript::AddTimer(timer_context, 0.016f, 0x0, &userdata1[0], &userdata2[0], false),
        dmScript::AddTimer(timer_context, 0.017f, 0x0, &userdata1[0], &userdata2[1], false),
        dmScript::AddTimer(timer_context, 0.018f, 0x0, &userdata1[0], &userdata2[2], false),
        dmScript::AddTimer(timer_context, 0.019f, 0x0, &userdata1[0], &userdata2[3], false),
        dmScript::AddTimer(timer_context, 0.020f, 0x0, &userdata1[0], &userdata2[4], false)
    };
    ASSERT_NE(ids[0], INVALID_TIMER_ID);
    ASSERT_NE(ids[1], INVALID_TIMER_ID);
    ASSERT_NE(ids[2], INVALID_TIMER_ID);
    ASSERT_NE(ids[3], INVALID_TIMER_ID);
    ASSERT_NE(ids[4], INVALID_TIMER_ID);

    bool cancelled = dmScript::CancelTimer(timer_context, ids[2]);
    ASSERT_EQ(cancelled, true);

    uint32_t cancelCount = dmScript::CancelTimers(timer_context, &userdata1[0]);
    ASSERT_EQ(cancelCount, 4u);

    dmScript::DeleteTimerContext(timer_context);
}

TEST(ScriptTimerTest, TestMixedUserData1Timer)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(2);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    uint32_t userdata1[] =
    {
        1u,
        2u
    };

    uint32_t userdata2[] =
    {
        10u,
        20u,
        30u,
        40u,
        50u
    };

    uint32_t ids[] = 
    {
        dmScript::AddTimer(timer_context, 0.016f, 0x0, &userdata1[0], &userdata2[0], false),
        dmScript::AddTimer(timer_context, 0.017f, 0x0, &userdata1[1], &userdata2[1], false),
        dmScript::AddTimer(timer_context, 0.018f, 0x0, &userdata1[0], &userdata2[2], false),
        dmScript::AddTimer(timer_context, 0.019f, 0x0, &userdata1[0], &userdata2[3], false),
        dmScript::AddTimer(timer_context, 0.020f, 0x0, &userdata1[1], &userdata2[4], false)
    };
    ASSERT_NE(ids[0], INVALID_TIMER_ID);
    ASSERT_NE(ids[1], INVALID_TIMER_ID);
    ASSERT_NE(ids[2], INVALID_TIMER_ID);
    ASSERT_NE(ids[3], INVALID_TIMER_ID);
    ASSERT_NE(ids[4], INVALID_TIMER_ID);

    bool cancelled = dmScript::CancelTimer(timer_context, ids[2]);
    ASSERT_EQ(cancelled, true);

    uint32_t cancelCount = dmScript::CancelTimers(timer_context, &userdata1[0]);
    ASSERT_EQ(cancelCount, 2u);

    cancelled = dmScript::CancelTimer(timer_context, ids[4]);
    ASSERT_EQ(cancelled, true);

    cancelCount = dmScript::CancelTimers(timer_context, &userdata1[0]);
    ASSERT_EQ(cancelCount, 0u);

    cancelCount = dmScript::CancelTimers(timer_context, &userdata1[1]);
    ASSERT_EQ(cancelCount, 1u);

    dmScript::DeleteTimerContext(timer_context);
}

TEST(ScriptTimerTest, TestTimerInstanceCountLimit)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    uint32_t userdata1[8] = {
        0u,
        1u,
        2u,
        3u,
        4u,
        5u,
        6u,
        7u
    };

    uint32_t ids[8] = 
    {
        dmScript::AddTimer(timer_context, 0.016f, 0x0, &userdata1[0], 0x0, false),
        dmScript::AddTimer(timer_context, 0.017f, 0x0, &userdata1[1], 0x0, false),
        dmScript::AddTimer(timer_context, 0.018f, 0x0, &userdata1[2], 0x0, false),
        dmScript::AddTimer(timer_context, 0.019f, 0x0, &userdata1[3], 0x0, false),
        dmScript::AddTimer(timer_context, 0.020f, 0x0, &userdata1[4], 0x0, false),
        dmScript::AddTimer(timer_context, 0.021f, 0x0, &userdata1[5], 0x0, false),
        dmScript::AddTimer(timer_context, 0.022f, 0x0, &userdata1[6], 0x0, false),
        dmScript::AddTimer(timer_context, 0.023f, 0x0, &userdata1[7], 0x0, false)
    };
    ASSERT_NE(ids[0], INVALID_TIMER_ID);
    ASSERT_NE(ids[1], INVALID_TIMER_ID);
    ASSERT_NE(ids[2], INVALID_TIMER_ID);
    ASSERT_NE(ids[3], INVALID_TIMER_ID);
    ASSERT_NE(ids[4], INVALID_TIMER_ID);
    ASSERT_NE(ids[5], INVALID_TIMER_ID);
    ASSERT_NE(ids[6], INVALID_TIMER_ID);
    ASSERT_NE(ids[7], INVALID_TIMER_ID);

    // Can't add a timer with yet another userdata1
    uint32_t id1 = dmScript::AddTimer(timer_context, 0.010f, 0x0, 0x0, 0x0, false);
    ASSERT_EQ(id1, INVALID_TIMER_ID);

    // Using the same userdata1 should be fine
    id1 = dmScript::AddTimer(timer_context, 0.010f, 0x0, &userdata1[1], 0x0, false);
    ASSERT_NE(id1, INVALID_TIMER_ID);

    bool cancelled = dmScript::CancelTimer(timer_context, ids[0]);
    ASSERT_EQ(cancelled, true);

    // Should be room for one more userdata1
    uint32_t id2 = dmScript::AddTimer(timer_context, 0.010f, 0x0, 0x0, 0x0, false);
    ASSERT_NE(id2, INVALID_TIMER_ID);

    // Now we should not have space for this
    uint32_t id3 = dmScript::AddTimer(timer_context, 0.010f, 0x0, &userdata1[0], 0x0, false);
    ASSERT_EQ(id3, INVALID_TIMER_ID);
    
    cancelled = dmScript::CancelTimer(timer_context, ids[4]);
    ASSERT_EQ(cancelled, true);

    // Space should be available   
    id3 = dmScript::AddTimer(timer_context, 0.010f, 0x0, &userdata1[0], 0x0, false);
    ASSERT_NE(id3, INVALID_TIMER_ID);
    
    dmScript::DeleteTimerContext(timer_context);
}

TEST(ScriptTimerTest, TestTimerTriggerCountLimit)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    uint32_t userdata1[8] = {
        0u,
        1u,
        2u,
        3u,
        4u,
        5u,
        6u,
        7u
    };

    dmArray<uint32_t> ids;
    ids.SetCapacity(65535u);
    ids.SetSize(65535u);

    uint32_t timer_count = 0;

    for (; timer_count < ids.Size(); ++timer_count)
    {
        ids[timer_count] = dmScript::AddTimer(timer_context, 0.10f + timer_count, 0x0, &userdata1[timer_count % 8], 0x0, false);
        if (ids[timer_count] == INVALID_TIMER_ID)
        {
            break;
        }
    }

    ASSERT_GT(timer_count, 16);

    dmScript::DeleteTimerContext(timer_context);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
