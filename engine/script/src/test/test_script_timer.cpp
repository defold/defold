#include <gtest/gtest.h>
#include "../script.h"
#include "../script_private.h"


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

TEST_F(ScriptTimerTest, TestCreateDeleteContext)
{
    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestCreateDeleteTimer)
{
    bool cancelled = dmScript::CancelTimer(m_Context, 0);
    ASSERT_EQ(false, cancelled);
    dmScript::HTimer id = dmScript::AddTimer(m_Context, 0.016f, false, 0x0, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);
    cancelled = dmScript::CancelTimer(m_Context, id);
    ASSERT_EQ(true, cancelled);
    cancelled = dmScript::CancelTimer(m_Context, id);
    ASSERT_EQ(false, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestIdReuse)
{
    dmScript::HTimer id1 = dmScript::AddTimer(m_Context, 0.016f, false, 0x0, 0x10, LUA_NOREF, LUA_NOREF);
    dmScript::HTimer id2 = dmScript::AddTimer(m_Context, 0.016f, false, 0x0, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(id1, id2);
    bool cancelled = dmScript::CancelTimer(m_Context, id1);
    ASSERT_EQ(true, cancelled);
    dmScript::HTimer id3 = dmScript::AddTimer(m_Context, 0.016f, false, 0x0, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(id1, id2);
    ASSERT_NE(id1, id3);
    cancelled = dmScript::CancelTimer(m_Context, id2);
    ASSERT_EQ(true, cancelled);
    cancelled = dmScript::CancelTimer(m_Context, id3);
    ASSERT_EQ(true, cancelled);
    dmScript::HTimer id4 = dmScript::AddTimer(m_Context, 0.016f, false, 0x0, 0x0, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(id1, id4);
    cancelled = dmScript::CancelTimer(m_Context, id4);
    ASSERT_EQ(true, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestSameOwnerTimer)
{
    uintptr_t owner[] =
    {
        1u
    };

    dmScript::HTimer ids[] = 
    {
        dmScript::AddTimer(m_Context, 0.016f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.017f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.018f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.019f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.020f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF)
    };

    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);

    bool cancelled = dmScript::CancelTimer(m_Context, ids[2]);
    ASSERT_EQ(true, cancelled);

    uint32_t cancelCount = dmScript::CancelTimers(m_Context, owner[0]);
    ASSERT_EQ(4u, cancelCount);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestMixedOwnersTimer)
{
    uintptr_t owner[] =
    {
        1u,
        2u
    };

    dmScript::HTimer ids[] = 
    {
        dmScript::AddTimer(m_Context, 0.016f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.017f, false, 0x0, owner[1], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.018f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.019f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.020f, false, 0x0, owner[1], LUA_NOREF, LUA_NOREF)
    };

    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[0]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[1]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[2]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[3]);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, ids[4]);

    bool cancelled = dmScript::CancelTimer(m_Context, ids[2]);
    ASSERT_EQ(true, cancelled);

    uint32_t cancelCount = dmScript::CancelTimers(m_Context, owner[0]);
    ASSERT_EQ(2u, cancelCount);

    cancelled = dmScript::CancelTimer(m_Context, ids[4]);
    ASSERT_EQ(true, cancelled);

    cancelCount = dmScript::CancelTimers(m_Context, owner[0]);
    ASSERT_EQ(0u, cancelCount);

    cancelCount = dmScript::CancelTimers(m_Context, owner[1]);
    ASSERT_EQ(1u, cancelCount);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestTimerOwnerCountLimit)
{
    // src/test/test.config sets a owner count limit of 8 and this test relies on that
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
        dmScript::AddTimer(m_Context, 0.016f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.017f, false, 0x0, owner[1], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.018f, false, 0x0, owner[2], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.019f, false, 0x0, owner[3], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.020f, false, 0x0, owner[4], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.021f, false, 0x0, owner[5], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.022f, false, 0x0, owner[6], LUA_NOREF, LUA_NOREF),
        dmScript::AddTimer(m_Context, 0.023f, false, 0x0, owner[7], LUA_NOREF, LUA_NOREF)
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
    dmScript::HTimer id1 = dmScript::AddTimer(m_Context, 0.010f, false, 0x0, 0x0, LUA_NOREF, LUA_NOREF);
    ASSERT_EQ(dmScript::INVALID_TIMER_ID, id1);

    // Using the same owner should be fine
    id1 = dmScript::AddTimer(m_Context, 0.010f, false, 0x0, owner[1], LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id1);

    bool cancelled = dmScript::CancelTimer(m_Context, ids[0]);
    ASSERT_EQ(true, cancelled);

    // Should be room for one more owner
    dmScript::HTimer id2 = dmScript::AddTimer(m_Context, 0.010f, false, 0x0, 0x0, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id2);

    // Now we should not have space for this
    dmScript::HTimer id3 = dmScript::AddTimer(m_Context, 0.010f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF);
    ASSERT_EQ(dmScript::INVALID_TIMER_ID, id3);
    
    cancelled = dmScript::CancelTimer(m_Context, ids[4]);
    ASSERT_EQ(true, cancelled);

    // Space should be available   
    id3 = dmScript::AddTimer(m_Context, 0.010f, false, 0x0, owner[0], LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id3);
    
    for (uint32_t i = 0; i < 8; ++i)
    {
        dmScript::CancelTimers(m_Context, owner[i]);
    }
    dmScript::CancelTimers(m_Context, 0x0);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestTimerTriggerCountLimit)
{
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
        ids[timer_count] = dmScript::AddTimer(m_Context, 0.10f + timer_count, false, 0x0, owner[timer_count % 8], LUA_NOREF, LUA_NOREF);
        if (ids[timer_count] == dmScript::INVALID_TIMER_ID)
        {
            break;
        }
    }

    ASSERT_LT(16u, timer_count);

    for (uint32_t i = 0; i < 8; ++i)
    {
        dmScript::CancelTimers(m_Context, owner[i]);
    }

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestOneshotTimerCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(m_Context, 2.f, false, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);

    dmScript::CancelTimer(m_Context, id);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestRepeatTimerCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(m_Context, 2.f, true, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);

    dmScript::CancelTimer(m_Context, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestUnevenRepeatTimerCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(m_Context, 1.5f, true, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 4.f);   // We only trigger once per Update!
    ASSERT_EQ(3u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(4u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 2.0f);
    ASSERT_EQ(5u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 2.0f);
    ASSERT_EQ(6u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 2.0f);
    ASSERT_EQ(7u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 5.5f);
    ASSERT_EQ(8u, callback_count);

    dmScript::CancelTimer(m_Context, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(8u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(8u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(8u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestUnevenShortRepeatTimerCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0u;
    static float total_elapsed_time = 0.0f;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float elapsed_time, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
            total_elapsed_time += elapsed_time;
        }
    };
    id = dmScript::AddTimer(m_Context, 0.5f, true, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    ASSERT_EQ(1.f, total_elapsed_time);

    dmScript::UpdateTimerContext(m_Context, 0.5f);
    ASSERT_EQ(2u, callback_count);
    ASSERT_EQ(1.5f, total_elapsed_time);

    bool cancelled = dmScript::CancelTimer(m_Context, id);
    ASSERT_EQ(true, cancelled);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestRepeatTimerCancelInCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ASSERT_NE(2u, callback_count);
            ++callback_count;
            if (callback_count == 2u)
            {
                bool cancelled = dmScript::CancelTimer(context, timer_id);
                ASSERT_EQ(true, cancelled);
            }
            else
            {
                ASSERT_GT(2u, callback_count);
            }
        }
    };

    id = dmScript::AddTimer(m_Context, 2.f, true, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    bool cancelled = dmScript::CancelTimer(m_Context, id);
    ASSERT_EQ(false, cancelled);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestOneshotTimerCancelInCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ASSERT_EQ(0u, callback_count);
            ++callback_count;
            bool cancelled = dmScript::CancelTimer(context, timer_id);
            ASSERT_EQ(false, cancelled);
        }
    };

    id = dmScript::AddTimer(m_Context, 2.f, false, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    bool cancelled = dmScript::CancelTimer(m_Context, id);
    ASSERT_EQ(false, cancelled);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(callback_count, 1u);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(callback_count, 1u);

    ASSERT_EQ(GetAliveTimers(m_Context), 0u);
}

TEST_F(ScriptTimerTest, TestTriggerTimerInCallback)
{
    static dmScript::HTimer outer_id = dmScript::INVALID_TIMER_ID;
    static dmScript::HTimer inner_id = dmScript::INVALID_TIMER_ID;
    static dmScript::HTimer inner2_id = dmScript::INVALID_TIMER_ID;
    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ++callback_count;
            if (callback_count < 2u)
            {
                ASSERT_EQ(outer_id, timer_id);
                outer_id = dmScript::AddTimer(context, 2.f, false, Callback::cb, owner, LUA_NOREF, LUA_NOREF);
                ASSERT_NE(dmScript::INVALID_TIMER_ID, outer_id);
            }
            else if (callback_count == 2u)
            {
                ASSERT_EQ(outer_id, timer_id);
                inner_id = dmScript::AddTimer(context, 0.f, false, Callback::cb, owner, LUA_NOREF, LUA_NOREF);
                ASSERT_NE(dmScript::INVALID_TIMER_ID, inner_id);
            }
            else if (callback_count == 3u)
            {
                ASSERT_EQ(inner_id, timer_id);
                inner2_id = dmScript::AddTimer(context, 1.f, false, Callback::cb, owner, LUA_NOREF, LUA_NOREF);
                ASSERT_NE(dmScript::INVALID_TIMER_ID, inner2_id);
            }
        }
    };

    outer_id = dmScript::AddTimer(m_Context, 2.f, false, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(outer_id, dmScript::INVALID_TIMER_ID);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(0u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);

    bool cancelled = dmScript::CancelTimer(m_Context, outer_id);
    ASSERT_EQ(false, cancelled);

    dmScript::UpdateTimerContext(m_Context, 0.00001f);
    ASSERT_EQ(3u, callback_count);

    cancelled = dmScript::CancelTimer(m_Context, inner_id);
    ASSERT_EQ(false, cancelled);
    
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(4u, callback_count);

    cancelled = dmScript::CancelTimer(m_Context, inner2_id);
    ASSERT_EQ(false, cancelled);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(4u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
}

TEST_F(ScriptTimerTest, TestShortRepeatTimerCallback)
{
    static dmScript::HTimer id = dmScript::INVALID_TIMER_ID;

    static uint32_t callback_count = 0;

    struct Callback {
        static void cb(dmScript::HContext context, dmScript::HTimer timer_id, float time_elapsed, uintptr_t owner, int self_ref, int callback_ref)
        {
            ASSERT_EQ(id, timer_id);
            ++callback_count;
        }
    };

    id = dmScript::AddTimer(m_Context, 0.1f, true, Callback::cb, 0x10, LUA_NOREF, LUA_NOREF);
    ASSERT_NE(dmScript::INVALID_TIMER_ID, id);

    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(1u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(2u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(3u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(4u, callback_count);
    bool cancelled = dmScript::CancelTimer(m_Context, id);
    ASSERT_EQ(true, cancelled);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(4u, callback_count);
    dmScript::UpdateTimerContext(m_Context, 1.f);
    ASSERT_EQ(4u, callback_count);

    ASSERT_EQ(0u, GetAliveTimers(m_Context));
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

TEST_F(ScriptTimerTest, TestLuaOneshot)
{
    int top = lua_gettop(L);

    LuaInit(L);

    const char pre_script[] =
        "local function cb(self, id, elapsed_time)\n"
        "    print(\"Timer: \"..tostring(id)..\", Elapsed: \"..tostring(elapsed_time))\n"
        "    test.callback_counter(id, elapsed_time)\n"
        "end\n"
        "\n"
        "id = timer.delay(1, false, cb)\n"
        "\n";

    const char post_script[] =
        "local cancelled = timer.cancel(id)\n"
        "if cancelled == true then\n"
        "    print(\"ERROR\")"  // Would be nicer if we could force the lua execution to return an error here!
        "end";

    cb_callback_counter = 0u;
    cb_elapsed_time = 0.0f;

    ASSERT_TRUE(RunString(L, pre_script));
    ASSERT_EQ(top, lua_gettop(L));

    dmScript::UpdateTimerContext(m_Context, 1.0f);

    ASSERT_NE(dmScript::INVALID_TIMER_ID, cb_callback_id);
    ASSERT_EQ(1u, cb_callback_counter);
    ASSERT_EQ(1.0f, cb_elapsed_time);

    dmScript::UpdateTimerContext(m_Context, 1.0f);
    ASSERT_EQ(1u, cb_callback_counter);
    ASSERT_EQ(1.0f, cb_elapsed_time);

    ASSERT_TRUE(RunString(L, post_script));
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_EQ(GetAliveTimers(m_Context), 0u);
}

TEST_F(ScriptTimerTest, TestLuaRepeating)
{
    int top = lua_gettop(L);

    LuaInit(L);

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

    ASSERT_TRUE(RunString(L, pre_script));
    ASSERT_EQ(top, lua_gettop(L));

    dmScript::UpdateTimerContext(m_Context, 1.0f);

    ASSERT_NE(dmScript::INVALID_TIMER_ID, cb_callback_id);
    ASSERT_EQ(1u, cb_callback_counter);
    ASSERT_EQ(1.0f, cb_elapsed_time);

    dmScript::UpdateTimerContext(m_Context, 0.5f);
    ASSERT_EQ(2u, cb_callback_counter);
    ASSERT_EQ(1.5f, cb_elapsed_time);

    ASSERT_TRUE(RunString(L, post_script));
    ASSERT_EQ(top, lua_gettop(L));

    ASSERT_EQ(GetAliveTimers(m_Context), 0u);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
