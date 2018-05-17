#include <gtest/gtest.h>
#include "../script.h"
#include "../script_timer.h"


struct TimerTestCallback
{
    static uint32_t callback_count;
    static uint32_t cancel_count;
    static float elapsed_time;
};

static void TestCallback(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
{
    switch (event_type)
    {
        case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
        case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
            ++TimerTestCallback::callback_count;
            TimerTestCallback::elapsed_time += time_elapsed;
            break;
        case dmScript::TIMER_EVENT_CANCELLED:
            ++TimerTestCallback::cancel_count;
            break;
        default:
            ASSERT_TRUE(false);
            break;
    }
}

static void ResetTestCallback()
{
    TimerTestCallback::callback_count = 0;
    TimerTestCallback::cancel_count = 0;
    TimerTestCallback::elapsed_time = 0.f;
}

uint32_t TimerTestCallback::callback_count = 0;
uint32_t TimerTestCallback::cancel_count = 0;
float TimerTestCallback::elapsed_time = 0.f;

class ScriptTimerTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmConfigFile::Result r = dmConfigFile::Load("src/test/test.config", 0, 0, &m_ConfigFile);
        ASSERT_EQ(dmConfigFile::RESULT_OK, r);
        m_Context = dmScript::NewContext(m_ConfigFile, 0, true);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);
        ResetTestCallback();
    }

    virtual void TearDown()
    {
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
        dmConfigFile::Delete(m_ConfigFile);
    }

    dmConfigFile::HConfig m_ConfigFile;
    dmScript::HContext m_Context;
    lua_State* L;
};

TEST_F(ScriptTimerTest, TestCreateDeleteWorld)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);
    ASSERT_NE(0x0, (uintptr_t)timer_world);
    ASSERT_EQ(0u, GetAliveTimers(timer_world));
    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestCreateDeleteTimer)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);
    bool cancelled = dmScript::CancelTimer(timer_world, 0);
    ASSERT_EQ(false, cancelled);
    dmScript::HTimer id = dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);
    cancelled = dmScript::CancelTimer(timer_world, id);
    ASSERT_EQ(true, cancelled);
    cancelled = dmScript::CancelTimer(timer_world, id);
    ASSERT_EQ(false, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));
    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestIdReuse)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);
    dmScript::HTimer id1 = dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, 0x10, 0x0);
    dmScript::HTimer id2 = dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, 0x10, 0x0);
    ASSERT_NE(id1, id2);
    bool cancelled = dmScript::CancelTimer(timer_world, id1);
    ASSERT_EQ(true, cancelled);
    dmScript::HTimer id3 = dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, 0x10, 0x0);
    ASSERT_NE(id1, id2);
    ASSERT_NE(id1, id3);
    cancelled = dmScript::CancelTimer(timer_world, id2);
    ASSERT_EQ(true, cancelled);
    cancelled = dmScript::CancelTimer(timer_world, id3);
    ASSERT_EQ(true, cancelled);
    dmScript::HTimer id4 = dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, 0x10, 0x0);
    ASSERT_NE(id1, id4);
    cancelled = dmScript::CancelTimer(timer_world, id4);
    ASSERT_EQ(true, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));
    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestSameOwnerTimer)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    uintptr_t owner[] =
    {
        1u
    };

    dmScript::HTimer ids[] = 
    {
        dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.017f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.018f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.019f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.020f, false, TestCallback, owner[0], 0x0)
    };

    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);

    bool cancelled = dmScript::CancelTimer(timer_world, ids[2]);
    ASSERT_EQ(true, cancelled);

    uint32_t killCount = dmScript::KillTimers(timer_world, owner[0]);
    ASSERT_EQ(4u, killCount);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestMixedOwnersTimer)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    uintptr_t owner[] =
    {
        1u,
        2u
    };

    dmScript::HTimer ids[] = 
    {
        dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.017f, false, TestCallback, owner[1], 0x0),
        dmScript::AddTimer(timer_world, 0.018f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.019f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.020f, false, TestCallback, owner[1], 0x0)
    };

    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);

    bool cancelled = dmScript::CancelTimer(timer_world, ids[2]);
    ASSERT_EQ(true, cancelled);

    uint32_t killCount = dmScript::KillTimers(timer_world, owner[0]);
    ASSERT_EQ(2u, killCount);

    killCount = dmScript::CancelTimer(timer_world, ids[4]);
    ASSERT_EQ(true, killCount);

    killCount = dmScript::KillTimers(timer_world, owner[0]);
    ASSERT_EQ(0u, killCount);

    killCount = dmScript::KillTimers(timer_world, owner[1]);
    ASSERT_EQ(1u, killCount);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestTimerOwnerCountLimit)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(8);

    uintptr_t owner[8] = {
        1u,
        2u,
        3u,
        4u,
        5u,
        6u,
        7u,
        8u
    };

    dmScript::HTimer ids[8] = 
    {
        dmScript::AddTimer(timer_world, 0.016f, false, TestCallback, owner[0], 0x0),
        dmScript::AddTimer(timer_world, 0.017f, false, TestCallback, owner[1], 0x0),
        dmScript::AddTimer(timer_world, 0.018f, false, TestCallback, owner[2], 0x0),
        dmScript::AddTimer(timer_world, 0.019f, false, TestCallback, owner[3], 0x0),
        dmScript::AddTimer(timer_world, 0.020f, false, TestCallback, owner[4], 0x0),
        dmScript::AddTimer(timer_world, 0.021f, false, TestCallback, owner[5], 0x0),
        dmScript::AddTimer(timer_world, 0.022f, false, TestCallback, owner[6], 0x0),
        dmScript::AddTimer(timer_world, 0.023f, false, TestCallback, owner[7], 0x0)
    };

    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[5]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[6]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[7]);

    // Can't add a timer with yet another owner
    dmScript::HTimer id1 = dmScript::AddTimer(timer_world, 0.010f, false, TestCallback, 0x0, 0x0);
    ASSERT_EQ(dmScript::INVALID_TIMER_ID, id1);

    // Using the same owner should be fine
    id1 = dmScript::AddTimer(timer_world, 0.010f, false, TestCallback, owner[1], 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id1);

    bool cancelled = dmScript::CancelTimer(timer_world, ids[0]);
    ASSERT_EQ(true, cancelled);

    // Should be room for one more owner
    dmScript::HTimer id2 = dmScript::AddTimer(timer_world, 0.010f, false, TestCallback, 0x0, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id2);

    // Now we should not have space for this
    dmScript::HTimer id3 = dmScript::AddTimer(timer_world, 0.010f, false, TestCallback, owner[0], 0x0);
    ASSERT_EQ(dmScript::INVALID_TIMER_ID, id3);
    
    cancelled = dmScript::CancelTimer(timer_world, ids[4]);
    ASSERT_EQ(true, cancelled);

    // Space should be available   
    id3 = dmScript::AddTimer(timer_world, 0.010f, false, TestCallback, owner[0], 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id3);
    
    for (uint32_t i = 0; i < 8; ++i)
    {
        dmScript::KillTimers(timer_world, owner[i]);
    }
    dmScript::KillTimers(timer_world, 0x0);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestTimerTriggerCountLimit)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    uintptr_t owner[8] = {
        1u,
        2u,
        3u,
        4u,
        5u,
        6u,
        7u,
        8u
    };

    dmArray<dmScript::HTimer> ids;
    ids.SetCapacity(65535u);
    ids.SetSize(65535u);

    uint32_t timer_count = 0;

    for (; timer_count < ids.Size(); ++timer_count)
    {
        ids[timer_count] = dmScript::AddTimer(timer_world, 0.10f + timer_count, false, TestCallback, owner[timer_count % 8], 0x0);
        if (ids[timer_count] == dmScript::INVALID_TIMER_ID)
        {
            break;
        }
    }

    ASSERT_LT(16u, timer_count);

    for (uint32_t i = 0; i < 8; ++i)
    {
        dmScript::KillTimers(timer_world, owner[i]);
    }

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestOneshotTimerCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            ASSERT_EQ(dmScript::TIMER_EVENT_TRIGGER_WILL_DIE, event_type);
            ASSERT_EQ(id, timer_id);
            ++TimerTestCallback::callback_count;
        }
    };

    id = dmScript::AddTimer(timer_world, 2.f, false, Callback::cb, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(0u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);

    ASSERT_FALSE(dmScript::CancelTimer(timer_world, id));

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestRepeatTimerCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            ASSERT_EQ(id, timer_id);
            switch (event_type)
            {
                case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
                case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
                    ++TimerTestCallback::callback_count;
                    break;
                case dmScript::TIMER_EVENT_CANCELLED:
                    ++TimerTestCallback::cancel_count;
                    break;
            }
        }
    };

    id = dmScript::AddTimer(timer_world, 2.f, true, Callback::cb, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(0u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);

    ASSERT_TRUE(dmScript::CancelTimer(timer_world, id));
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestUnevenRepeatTimerCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            ASSERT_EQ(id, timer_id);
            switch (event_type)
            {
                case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
                case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
                    ++TimerTestCallback::callback_count;
                    break;
                case dmScript::TIMER_EVENT_CANCELLED:
                    ++TimerTestCallback::cancel_count;
                    break;
            }
        }
    };

    id = dmScript::AddTimer(timer_world, 1.5f, true, Callback::cb, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(0u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 4.f);   // We only trigger once per Update!
    ASSERT_EQ(3u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(4u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 2.0f);
    ASSERT_EQ(5u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 2.0f);
    ASSERT_EQ(6u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 2.0f);
    ASSERT_EQ(7u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 5.5f);
    ASSERT_EQ(8u, TimerTestCallback::callback_count);

    ASSERT_TRUE(dmScript::CancelTimer(timer_world, id));
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(8u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(8u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(8u, TimerTestCallback::callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestUnevenShortRepeatTimerCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            ASSERT_EQ(id, timer_id);
            switch (event_type)
            {
                case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
                case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
                    ++TimerTestCallback::callback_count;
                    break;
                case dmScript::TIMER_EVENT_CANCELLED:
                    ++TimerTestCallback::cancel_count;
                    break;
            }
            TimerTestCallback::elapsed_time += time_elapsed;
        }
    };
    id = dmScript::AddTimer(timer_world, 0.5f, true, Callback::cb, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    ASSERT_EQ(1.f, TimerTestCallback::elapsed_time);

    dmScript::UpdateTimers(timer_world, 0.5f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    ASSERT_EQ(1.5f, TimerTestCallback::elapsed_time);

    bool cancelled = dmScript::CancelTimer(timer_world, id);
    ASSERT_TRUE(cancelled);
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestRepeatTimerCancelInCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            ASSERT_EQ(id, timer_id);

            ASSERT_NE(dmScript::TIMER_EVENT_TRIGGER_WILL_DIE, event_type);

            switch (event_type)
            {
                case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
                case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
                    {
                        ASSERT_NE(2u, TimerTestCallback::callback_count);
                        ++TimerTestCallback::callback_count;
                        if (TimerTestCallback::callback_count == 2u)
                        {
                            bool cancelled = dmScript::CancelTimer(timer_world, timer_id);
                            ASSERT_EQ(true, cancelled);
                        }
                        else
                        {
                            ASSERT_GT(2u, TimerTestCallback::callback_count);
                        }
                    }
                    break;
                case dmScript::TIMER_EVENT_CANCELLED:
                    ++TimerTestCallback::cancel_count;
                    break;
            }
        }
    };

    id = dmScript::AddTimer(timer_world, 2.f, true, Callback::cb, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(0u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    bool cancelled = dmScript::CancelTimer(timer_world, id);
    ASSERT_EQ(false, cancelled);
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestOneshotTimerCancelInCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            ASSERT_EQ(id, timer_id);

            ASSERT_NE(dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT, event_type);
            switch (event_type)
            {
                case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
                case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
                    ASSERT_EQ(0u, TimerTestCallback::callback_count);
                    ++TimerTestCallback::callback_count;
                    ASSERT_TRUE(dmScript::CancelTimer(timer_world, timer_id));
                    break;
                case dmScript::TIMER_EVENT_CANCELLED:
                    ASSERT_EQ(0u, TimerTestCallback::cancel_count);
                    ++TimerTestCallback::cancel_count;
                    break;
            }
        }
    };

    id = dmScript::AddTimer(timer_world, 2.f, false, Callback::cb, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(0u, TimerTestCallback::callback_count);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);
    bool cancelled = dmScript::CancelTimer(timer_world, id);
    ASSERT_EQ(false, cancelled);
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(TimerTestCallback::callback_count, 1u);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(TimerTestCallback::callback_count, 1u);

    ASSERT_EQ(GetAliveTimers(timer_world), 0u);

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestTriggerTimerInCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer outer_id = dmScript::INVALID_TIMER_ID;
    static dmScript::HTimer inner_id = dmScript::INVALID_TIMER_ID;
    static dmScript::HTimer inner2_id = dmScript::INVALID_TIMER_ID;

    struct Callback {
        static void cb(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
        {
            switch (event_type)
            {
                case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
                case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
                    ++TimerTestCallback::callback_count;
                    {
                        if (TimerTestCallback::callback_count < 2u)
                        {
                            ASSERT_EQ(outer_id, timer_id);
                            outer_id = dmScript::AddTimer(timer_world, 2.f, false, Callback::cb, owner, 0x0);
                            ASSERT_NE(dmScript::INVALID_TIMER_ID, outer_id);
                        }
                        else if (TimerTestCallback::callback_count == 2u)
                        {
                            ASSERT_EQ(outer_id, timer_id);
                            inner_id = dmScript::AddTimer(timer_world, 0.f, false, Callback::cb, owner, 0x0);
                            ASSERT_NE(dmScript::INVALID_TIMER_ID, inner_id);
                        }
                        else if (TimerTestCallback::callback_count == 3u)
                        {
                            ASSERT_EQ(inner_id, timer_id);
                            inner2_id = dmScript::AddTimer(timer_world, 1.f, false, Callback::cb, owner, 0x0);
                            ASSERT_NE(dmScript::INVALID_TIMER_ID, inner2_id);
                        }
                    }
                    break;
                case dmScript::TIMER_EVENT_CANCELLED:
                    ++TimerTestCallback::cancel_count;
                    break;
            }
        }
    };

    outer_id = dmScript::AddTimer(timer_world, 2.f, false, Callback::cb, 0x10, 0x0);
    ASSERT_NE(outer_id, dmScript::INVALID_TIMER_ID);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(0u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);

    bool cancelled = dmScript::CancelTimer(timer_world, outer_id);
    ASSERT_EQ(false, cancelled);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 0.00001f);
    ASSERT_EQ(3u, TimerTestCallback::callback_count);

    cancelled = dmScript::CancelTimer(timer_world, inner_id);
    ASSERT_EQ(false, cancelled);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);
    
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(4u, TimerTestCallback::callback_count);

    cancelled = dmScript::CancelTimer(timer_world, inner2_id);
    ASSERT_EQ(false, cancelled);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(4u, TimerTestCallback::callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

static void ShortRepeatTimerCallback(dmScript::HTimerWorld timer_world, dmScript::TimerEventType event_type, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, uintptr_t userdata)
{
    switch (event_type)
    {
        case dmScript::TIMER_EVENT_TRIGGER_WILL_REPEAT:
        case dmScript::TIMER_EVENT_TRIGGER_WILL_DIE:
            ++TimerTestCallback::callback_count;
            break;
        case dmScript::TIMER_EVENT_CANCELLED:
            ++TimerTestCallback::cancel_count;
            break;
    }
}

TEST_F(ScriptTimerTest, TestShortRepeatTimerCallback)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    id = dmScript::AddTimer(timer_world, 0.1f, true, ShortRepeatTimerCallback, 0x10, 0x0);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(1u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(2u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(3u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(4u, TimerTestCallback::callback_count);
    bool cancelled = dmScript::CancelTimer(timer_world, id);
    ASSERT_EQ(true, cancelled);
    ASSERT_EQ(1u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(4u, TimerTestCallback::callback_count);
    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(4u, TimerTestCallback::callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

TEST_F(ScriptTimerTest, TestKillTimers)
{
    dmScript::HTimerWorld timer_world = dmScript::NewTimerWorld(16);

    dmScript::HTimer ids[5];

    for (uint32_t i = 0; i < 5; ++i)
    {
        ids[i] = dmScript::AddTimer(timer_world, 1.0f, true, TestCallback, 0x10, 0x0);
        ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[i]);
    }

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(5u, TimerTestCallback::callback_count);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(10u, TimerTestCallback::callback_count);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(15u, TimerTestCallback::callback_count);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);

    uint32_t kill_count = dmScript::KillTimers(timer_world, 0x10);
    ASSERT_EQ(5u, kill_count);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);
    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::UpdateTimers(timer_world, 1.f);
    ASSERT_EQ(15u, TimerTestCallback::callback_count);
    ASSERT_EQ(0u, TimerTestCallback::cancel_count);

    kill_count = dmScript::KillTimers(timer_world, 0x10);
    ASSERT_EQ(0u, kill_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_world));

    dmScript::DeleteTimerWorld(timer_world);
}

static bool RunString(lua_State* L, const char* script)
{
    luaL_loadstring(L, script);
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}

static dmScript::HTimer cb_callback_id = dmScript::INVALID_TIMER_ID;
static uint32_t cb_callback_counter = 0u;
static float cb_elapsed_time = 0.0f;

static int CallbackCounter(lua_State* L)
{
    int top = lua_gettop(L);
    const int id = luaL_checkint(L, 1);
    const double dt = luaL_checknumber(L, 2);
    cb_callback_id = (dmScript::HTimer)id;
    cb_elapsed_time += dt;
    ++cb_callback_counter;
    assert(top == lua_gettop(L));
    return 0;
}

static const luaL_reg Module_methods[] = {
    { "callback_counter", CallbackCounter},
    { 0, 0 }
};

static void LuaInit(lua_State* L) {
    int top = lua_gettop(L);

    luaL_register(L, "test", Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

struct ScriptInstance
{
    int m_InstanceReference;
    int m_ContextTableReference;
};

static int ScriptGetInstanceContextTableRef(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    const int self_index = 1;

    ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, self_index);
    if (i != 0x0 && i->m_ContextTableReference != LUA_NOREF)
    {
        lua_pushnumber(L, i->m_ContextTableReference);
    }
    else
    {
        lua_pushnil(L);
    }

    return 1;
}

static int ScriptInstanceIsValid(lua_State* L)
{
    ScriptInstance* i = (ScriptInstance*)lua_touserdata(L, 1);
    lua_pushboolean(L, i != 0x0 && i->m_ContextTableReference != LUA_NOREF);
    return 1;
}
    
static const luaL_reg ScriptInstance_methods[] =
{
    {0,0}
};

static const luaL_reg ScriptInstance_meta[] =
{
    {dmScript::META_TABLE_IS_VALID,                 ScriptInstanceIsValid},
    {dmScript::META_GET_INSTANCE_CONTEXT_TABLE_REF, ScriptGetInstanceContextTableRef},
    {0, 0}
};

static void CreateScriptInstance(lua_State* L, const char* type)
{
    ScriptInstance* i = (ScriptInstance *)lua_newuserdata(L, sizeof(ScriptInstance));
    i->m_InstanceReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

    lua_newtable(L);
    i->m_ContextTableReference = dmScript::Ref( L, LUA_REGISTRYINDEX );

    lua_rawgeti(L, LUA_REGISTRYINDEX, i->m_InstanceReference);
    luaL_getmetatable(L, type);
    lua_setmetatable(L, -2);
}

static void DeleteScriptInstance(lua_State* L)
{
    ScriptInstance* i = (ScriptInstance *)lua_touserdata(L, -1);
    dmScript::Unref(L, LUA_REGISTRYINDEX, i->m_InstanceReference);
    dmScript::Unref(L, LUA_REGISTRYINDEX, i->m_ContextTableReference);
}

TEST_F(ScriptTimerTest, TestLuaOneshot)
{
    int top = lua_gettop(L);
    LuaInit(L);

    dmScript::HScriptWorld script_world = dmScript::NewScriptWorld(m_Context);

    const char pre_script[] =
        "local function cb(self, id, elapsed_time)\n"
        "    print(\"Timer: \"..tostring(id)..\", Elapsed: \"..tostring(elapsed_time))\n"
        "    test.callback_counter(id, elapsed_time)\n"
        "end\n"
        "\n"
        "id = timer.delay(1, false, cb)\n"
        "assert(id ~= timer.INVALID_TIMER_ID)\n"
        "\n";

    const char post_script[] =
        "local cancelled = timer.cancel(id)\n"
        "assert(cancelled == false)\n";

    cb_callback_counter = 0u;
    cb_elapsed_time = 0.0f;

    const char* SCRIPTINSTANCE = "TestScriptInstance";

    dmScript::RegisterUserType(L, SCRIPTINSTANCE, ScriptInstance_methods, ScriptInstance_meta);

    CreateScriptInstance(L, SCRIPTINSTANCE);
    dmScript::SetInstance(L);

    ASSERT_TRUE(dmScript::IsInstanceValid(L));
    dmScript::InitializeInstance(script_world);

    ASSERT_TRUE(RunString(L, pre_script));
    ASSERT_EQ(top, lua_gettop(L));

    dmScript::UpdateScriptWorld(script_world, 1.0f);

    ASSERT_NE(dmScript::INVALID_TIMER_ID, cb_callback_id);
    ASSERT_EQ(1u, cb_callback_counter);
    ASSERT_EQ(1.0f, cb_elapsed_time);

    dmScript::UpdateScriptWorld(script_world, 1.0f);
    ASSERT_EQ(1u, cb_callback_counter);
    ASSERT_EQ(1.0f, cb_elapsed_time);

    ASSERT_TRUE(RunString(L, post_script));
    ASSERT_EQ(top, lua_gettop(L));

    FinalizeInstance(script_world);    

    dmScript::GetInstance(L);
    DeleteScriptInstance(L);

    lua_pushnil(L);
    dmScript::SetInstance(L);

    dmScript::DeleteScriptWorld(script_world);
}

TEST_F(ScriptTimerTest, TestLuaRepeating)
{
    int top = lua_gettop(L);
    LuaInit(L);

    dmScript::HScriptWorld script_world = dmScript::NewScriptWorld(m_Context);

    const char pre_script[] =
        "local function cb(self, id, elapsed_time)\n"
        "    print(\"Timer: \"..tostring(id)..\", Elapsed: \"..tostring(elapsed_time))\n"
        "    test.callback_counter(id, elapsed_time)\n"
        "end\n"
        "\n"
        "id = timer.delay(0.5, true, cb)\n"
        "\n";

    const char post_script[] =
        "local cancelled = timer.cancel(id)\n"
        "if cancelled == false then\n"
        "    print(\"ERROR\")"  // Would be nicer if we could force the lua execution to return an error here!
        "end";

    cb_callback_counter = 0u;
    cb_elapsed_time = 0.0f;

    const char* SCRIPTINSTANCE = "TestScriptInstance";
    dmScript::RegisterUserType(L, SCRIPTINSTANCE, ScriptInstance_methods, ScriptInstance_meta);

    CreateScriptInstance(L, SCRIPTINSTANCE);
    dmScript::SetInstance(L);

    ASSERT_TRUE(dmScript::IsInstanceValid(L));
    dmScript::InitializeInstance(script_world);

    ASSERT_TRUE(RunString(L, pre_script));
    ASSERT_EQ(top, lua_gettop(L));

    dmScript::UpdateScriptWorld(script_world, 1.0f);

    ASSERT_NE(dmScript::INVALID_TIMER_ID, cb_callback_id);
    ASSERT_EQ(1u, cb_callback_counter);
    ASSERT_EQ(1.0f, cb_elapsed_time);

    dmScript::UpdateScriptWorld(script_world, 0.5f);
    ASSERT_EQ(2u, cb_callback_counter);
    ASSERT_EQ(1.5f, cb_elapsed_time);

    ASSERT_TRUE(RunString(L, post_script));
    ASSERT_EQ(top, lua_gettop(L));

    FinalizeInstance(script_world);    

    dmScript::GetInstance(L);
    DeleteScriptInstance(L);

    lua_pushnil(L);
    dmScript::SetInstance(L);

    dmScript::DeleteScriptWorld(script_world);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
