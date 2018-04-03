#include <gtest/gtest.h>
#include "../script.h"
#include "../script_private.h"


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

TEST_F(ScriptTimerTest, TestCreateDeleteContext)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);
    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestCreateDeleteTimer)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    bool cancelled = dmScript::CancelTimer(timer_context, 0);
    ASSERT_EQ(false, cancelled);
    dmScript::HTimer id = dmScript::AddTimer(timer_context, 0.016f, 0x0, m_Context, 0, false);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);
    cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(true, cancelled);
    cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(false, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestIdReuse)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    dmScript::HTimer id1 = dmScript::AddTimer(timer_context, 0.016f, 0x0, m_Context, 0, false);
    dmScript::HTimer id2 = dmScript::AddTimer(timer_context, 0.016f, 0x0, m_Context, 0, false);
    ASSERT_NE(id1, id2);
    bool cancelled = dmScript::CancelTimer(timer_context, id1);
    ASSERT_EQ(true, cancelled);
    dmScript::HTimer id3 = dmScript::AddTimer(timer_context, 0.016f, 0x0, m_Context, 0, false);
    ASSERT_NE(id1, id2);
    ASSERT_NE(id1, id3);
    cancelled = dmScript::CancelTimer(timer_context, id2);
    ASSERT_EQ(true, cancelled);
    cancelled = dmScript::CancelTimer(timer_context, id3);
    ASSERT_EQ(true, cancelled);
    dmScript::HTimer id4 = dmScript::AddTimer(timer_context, 0.016f, 0x0, 0x0, 0, false);
    ASSERT_NE(id1, id4);
    cancelled = dmScript::CancelTimer(timer_context, id4);
    ASSERT_EQ(true, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestSameScriptContextTimer)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(1);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    dmScript::HContext script_contexts[] =
    {
        (dmScript::HContext)1u
    };

    int refs[] =
    {
        10u,
        20u,
        30u,
        40u,
        50u
    };

    dmScript::HTimer ids[] = 
    {
        dmScript::AddTimer(timer_context, 0.016f, 0x0, script_contexts[0], refs[0], false),
        dmScript::AddTimer(timer_context, 0.017f, 0x0, script_contexts[0], refs[1], false),
        dmScript::AddTimer(timer_context, 0.018f, 0x0, script_contexts[0], refs[2], false),
        dmScript::AddTimer(timer_context, 0.019f, 0x0, script_contexts[0], refs[3], false),
        dmScript::AddTimer(timer_context, 0.020f, 0x0, script_contexts[0], refs[4], false)
    };
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);

    bool cancelled = dmScript::CancelTimer(timer_context, ids[2]);
    ASSERT_EQ(true, cancelled);

    uint32_t cancelCount = dmScript::CancelTimers(timer_context, script_contexts[0]);
    ASSERT_EQ(4u, cancelCount);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestMixedScriptContextsTimer)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(2);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    dmScript::HContext script_contexts[] =
    {
        (dmScript::HContext)1u,
        (dmScript::HContext)2u
    };

    int refs[] =
    {
        10u,
        20u,
        30u,
        40u,
        50u
    };

    dmScript::HTimer ids[] = 
    {
        dmScript::AddTimer(timer_context, 0.016f, 0x0, script_contexts[0], refs[0], false),
        dmScript::AddTimer(timer_context, 0.017f, 0x0, script_contexts[1], refs[1], false),
        dmScript::AddTimer(timer_context, 0.018f, 0x0, script_contexts[0], refs[2], false),
        dmScript::AddTimer(timer_context, 0.019f, 0x0, script_contexts[0], refs[3], false),
        dmScript::AddTimer(timer_context, 0.020f, 0x0, script_contexts[1], refs[4], false)
    };
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);

    bool cancelled = dmScript::CancelTimer(timer_context, ids[2]);
    ASSERT_EQ(true, cancelled);

    uint32_t cancelCount = dmScript::CancelTimers(timer_context, script_contexts[0]);
    ASSERT_EQ(2u, cancelCount);

    cancelled = dmScript::CancelTimer(timer_context, ids[4]);
    ASSERT_EQ(true, cancelled);

    cancelCount = dmScript::CancelTimers(timer_context, script_contexts[0]);
    ASSERT_EQ(0u, cancelCount);

    cancelCount = dmScript::CancelTimers(timer_context, script_contexts[1]);
    ASSERT_EQ(1u, cancelCount);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestTimerInstanceCountLimit)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(8);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    dmScript::HContext script_contexts[8] = {
        (dmScript::HContext)1u,
        (dmScript::HContext)2u,
        (dmScript::HContext)3u,
        (dmScript::HContext)4u,
        (dmScript::HContext)5u,
        (dmScript::HContext)6u,
        (dmScript::HContext)7u,
        (dmScript::HContext)8u
    };

    dmScript::HTimer ids[8] = 
    {
        dmScript::AddTimer(timer_context, 0.016f, 0x0, script_contexts[0], 0, false),
        dmScript::AddTimer(timer_context, 0.017f, 0x0, script_contexts[1], 0, false),
        dmScript::AddTimer(timer_context, 0.018f, 0x0, script_contexts[2], 0, false),
        dmScript::AddTimer(timer_context, 0.019f, 0x0, script_contexts[3], 0, false),
        dmScript::AddTimer(timer_context, 0.020f, 0x0, script_contexts[4], 0, false),
        dmScript::AddTimer(timer_context, 0.021f, 0x0, script_contexts[5], 0, false),
        dmScript::AddTimer(timer_context, 0.022f, 0x0, script_contexts[6], 0, false),
        dmScript::AddTimer(timer_context, 0.023f, 0x0, script_contexts[7], 0, false)
    };
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[5]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[6]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[7]);

    // Can't add a timer with yet another script context
    dmScript::HTimer id1 = dmScript::AddTimer(timer_context, 0.010f, 0x0, 0x0, 0, false);
    ASSERT_EQ(dmScript::INVALID_TIMER_ID, id1);

    // Using the same script context should be fine
    id1 = dmScript::AddTimer(timer_context, 0.010f, 0x0, script_contexts[1], 0, false);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id1);

    bool cancelled = dmScript::CancelTimer(timer_context, ids[0]);
    ASSERT_EQ(true, cancelled);

    // Should be room for one more script context
    dmScript::HTimer id2 = dmScript::AddTimer(timer_context, 0.010f, 0x0, 0x0, 0, false);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id2);

    // Now we should not have space for this
    dmScript::HTimer id3 = dmScript::AddTimer(timer_context, 0.010f, 0x0, script_contexts[0], 0, false);
    ASSERT_EQ(dmScript::INVALID_TIMER_ID, id3);
    
    cancelled = dmScript::CancelTimer(timer_context, ids[4]);
    ASSERT_EQ(true, cancelled);

    // Space should be available   
    id3 = dmScript::AddTimer(timer_context, 0.010f, 0x0, script_contexts[0], 0, false);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id3);
    
    for (uint32_t i = 0; i < 8; ++i)
    {
        dmScript::CancelTimers(timer_context, script_contexts[i]);
    }
    dmScript::CancelTimers(timer_context, 0x0);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestTimerTriggerCountLimit)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    dmScript::HContext script_contexts[8] = {
        (dmScript::HContext)1u,
        (dmScript::HContext)2u,
        (dmScript::HContext)3u,
        (dmScript::HContext)4u,
        (dmScript::HContext)5u,
        (dmScript::HContext)6u,
        (dmScript::HContext)7u,
        (dmScript::HContext)8u
    };

    dmArray<dmScript::HTimer> ids;
    ids.SetCapacity(65535u);
    ids.SetSize(65535u);

    uint32_t timer_count = 0;

    for (; timer_count < ids.Size(); ++timer_count)
    {
        ids[timer_count] = dmScript::AddTimer(timer_context, 0.10f + timer_count, 0x0, script_contexts[timer_count % 8], 0, false);
        if (ids[timer_count] == dmScript::INVALID_TIMER_ID)
        {
            break;
        }
    }

    ASSERT_LT(16u, timer_count);

    for (uint32_t i = 0; i < 8; ++i)
    {
        dmScript::CancelTimers(timer_context, script_contexts[i]);
    }

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestOneshotTimerCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(timer_context, 2.f, Callback::cb, m_Context, 1, false);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);

    dmScript::CancelTimer(timer_context, id);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestRepeatTimerCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(timer_context, 2.f, Callback::cb, m_Context, 1, true);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);

    dmScript::CancelTimer(timer_context, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestUnevenRepeatTimerCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(timer_context, 1.5f, Callback::cb, m_Context, 1, true);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 4.f);   // We only trigger once per Update!
    ASSERT_EQ(3u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(4u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 2.0f);
    ASSERT_EQ(5u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 2.0f);
    ASSERT_EQ(6u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 2.0f);
    ASSERT_EQ(7u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 5.5f);
    ASSERT_EQ(8u, callback_count);

    dmScript::CancelTimer(timer_context, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(8u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(8u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(8u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestUnevenShortRepeatTimerCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0u;
    static float total_elapsed_time = 0.0f;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int )
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
            total_elapsed_time += elapsed_time;
        }
    };
    id = dmScript::AddTimer(timer_context, 0.5f, Callback::cb, m_Context, 1, true);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    ASSERT_EQ(1.f, total_elapsed_time);

    dmScript::UpdateTimerContext(timer_context, 0.5f);
    ASSERT_EQ(2u, callback_count);
    ASSERT_EQ(1.5f, total_elapsed_time);

    bool cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(true, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestRepeatTimerCancelInCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ASSERT_EQ(id, timer_id);
            ASSERT_NE(2u, callback_count);
            ++callback_count;
            if (callback_count == 2u)
            {
                bool cancelled = dmScript::CancelTimer(timer_context, timer_id);
                ASSERT_EQ(true, cancelled);
            }
            else
            {
                ASSERT_GT(2u, callback_count);
            }
        }
    };

    id = dmScript::AddTimer(timer_context, 2.f, Callback::cb, m_Context, 1, true);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    bool cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(false, cancelled);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestOneshotTimerCancelInCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE(timer_context, (dmScript::HTimerContext)0x0);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ASSERT_EQ(id, timer_id);
            ASSERT_EQ(0u, callback_count);
            ++callback_count;
            bool cancelled = dmScript::CancelTimer(timer_context, timer_id);
            ASSERT_EQ(false, cancelled);
        }
    };

    id = dmScript::AddTimer(timer_context, 2.f, Callback::cb, m_Context, 1, false);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    bool cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(false, cancelled);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(callback_count, 1u);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(callback_count, 1u);

    ASSERT_EQ(GetAliveTimers(timer_context), 0u);
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestTriggerTimerInCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer outer_id = dmScript::INVALID_TIMER_ID;
    static dmScript::HTimer inner_id = dmScript::INVALID_TIMER_ID;
    static dmScript::HTimer inner2_id = dmScript::INVALID_TIMER_ID;
    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ++callback_count;
            if (callback_count < 2u)
            {
                ASSERT_EQ(outer_id, timer_id);
                outer_id = dmScript::AddTimer(timer_context, 2.f, Callback::cb, scriptContext, 1, false);
                ASSERT_NE(dmScript::INVALID_TIMER_ID, outer_id);
            }
            else if (callback_count == 2u)
            {
                ASSERT_EQ(outer_id, timer_id);
                inner_id = dmScript::AddTimer(timer_context, 0.f, Callback::cb, scriptContext, 1, false);
                ASSERT_NE(dmScript::INVALID_TIMER_ID, inner_id);
            }
            else if (callback_count == 3u)
            {
                ASSERT_EQ(inner_id, timer_id);
                inner2_id = dmScript::AddTimer(timer_context, 1.f, Callback::cb, scriptContext, 1, false);
                ASSERT_NE(dmScript::INVALID_TIMER_ID, inner2_id);
            }
        }
    };

    outer_id = dmScript::AddTimer(timer_context, 2.f, Callback::cb, m_Context, 1, false);
    ASSERT_NE(outer_id, dmScript::INVALID_TIMER_ID);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);

    bool cancelled = dmScript::CancelTimer(timer_context, outer_id);
    ASSERT_EQ(false, cancelled);

    dmScript::UpdateTimerContext(timer_context, 0.00001f);
    ASSERT_EQ(3u, callback_count);

    cancelled = dmScript::CancelTimer(timer_context, inner_id);
    ASSERT_EQ(false, cancelled);
    
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(4u, callback_count);

    cancelled = dmScript::CancelTimer(timer_context, inner2_id);
    ASSERT_EQ(false, cancelled);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(4u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

TEST_F(ScriptTimerTest, TestShortRepeatTimerCallback)
{
    dmScript::HTimerContext timer_context = dmScript::NewTimerContext(16);
    ASSERT_NE((dmScript::HTimerContext)0x0, timer_context);

    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HTimerContext timer_context, dmScript::HTimer timer_id, float elapsed_time, dmScript::HContext scriptContext, int ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(timer_context, 0.1f, Callback::cb, m_Context, 1, true);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(3u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(4u, callback_count);
    bool cancelled = dmScript::CancelTimer(timer_context, id);
    ASSERT_EQ(true, cancelled);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(4u, callback_count);
    dmScript::UpdateTimerContext(timer_context, 1.f);
    ASSERT_EQ(4u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(timer_context));
    dmScript::DeleteTimerContext(timer_context);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
