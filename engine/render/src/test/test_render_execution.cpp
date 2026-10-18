// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <assert.h>

#include <dlib/condition_variable.h>
#include <dlib/mutex.h>
#include <dlib/thread.h>
#include <testmain/testmain.h>

#include "render/render_execution.h"

using namespace dmRender;

struct RenderExecution
{
    ExecutionReason       m_InlineReason;
    ExecutionMode         m_ThreadedMode;
    ExecutionReason       m_ThreadedReason;
    ExecutionCapabilities m_Capabilities;
};

// Verify every capability combination resolves explicit modes and rejects unavailable threading.
TEST(RenderExecution, ModeMatrix)
{
    const RenderExecution cases[] = {
        { EXECUTION_REASON_BUILD_DISABLED, EXECUTION_MODE_INLINE, EXECUTION_REASON_BUILD_DISABLED, { false, false, false } },
        { EXECUTION_REASON_BUILD_DISABLED, EXECUTION_MODE_INLINE, EXECUTION_REASON_BUILD_DISABLED, { false, false, true  } },
        { EXECUTION_REASON_BUILD_DISABLED, EXECUTION_MODE_INLINE, EXECUTION_REASON_BUILD_DISABLED, { false, true,  false } },
        { EXECUTION_REASON_BUILD_DISABLED, EXECUTION_MODE_INLINE, EXECUTION_REASON_BUILD_DISABLED, { false, true,  true  } },
        { EXECUTION_REASON_INLINE, EXECUTION_MODE_INVALID, EXECUTION_REASON_THREADS_UNSUPPORTED, { true,  false, false } },
        { EXECUTION_REASON_INLINE, EXECUTION_MODE_INVALID, EXECUTION_REASON_THREADS_UNSUPPORTED, { true,  false, true  } },
        { EXECUTION_REASON_INLINE, EXECUTION_MODE_INVALID, EXECUTION_REASON_EXECUTOR_UNAVAILABLE, { true,  true,  false } },
        { EXECUTION_REASON_INLINE, EXECUTION_MODE_THREADED, EXECUTION_REASON_THREADED, { true,  true,  true  } },
    };

    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        ExecutionSelection selection = ResolveExecutionMode(EXECUTION_MODE_INLINE, cases[i].m_Capabilities);
        ASSERT_EQ(EXECUTION_MODE_INLINE, selection.m_ExecutionMode);
        ASSERT_EQ(cases[i].m_InlineReason, selection.m_ExecutionReason);
        selection = ResolveExecutionMode(EXECUTION_MODE_THREADED, cases[i].m_Capabilities);
        ASSERT_EQ(cases[i].m_ThreadedMode, selection.m_ExecutionMode);
        ASSERT_EQ(cases[i].m_ThreadedReason, selection.m_ExecutionReason);

        const ExecutionMode invalid_modes[] = { EXECUTION_MODE_INVALID, (ExecutionMode)99 };
        for (unsigned int j = 0; j < sizeof(invalid_modes) / sizeof(invalid_modes[0]); ++j)
        {
            selection = ResolveExecutionMode(invalid_modes[j], cases[i].m_Capabilities);
            ASSERT_EQ(EXECUTION_MODE_INVALID, selection.m_ExecutionMode);
            ASSERT_EQ(EXECUTION_REASON_INVALID_MODE, selection.m_ExecutionReason);
        }
    }
}

// Verify startup diagnostics distinguish build exclusions, unsupported targets, missing executors and invalid modes.
TEST(RenderExecution, Diagnostics)
{
    ASSERT_STREQ("inline", GetExecutionModeName(EXECUTION_MODE_INLINE));
    ASSERT_STREQ("threaded", GetExecutionModeName(EXECUTION_MODE_THREADED));
    ASSERT_STREQ("invalid", GetExecutionModeName(EXECUTION_MODE_INVALID));
    ASSERT_STREQ("inline mode", GetExecutionReasonString(EXECUTION_REASON_INLINE));
    ASSERT_STREQ("threaded rendering excluded by build", GetExecutionReasonString(EXECUTION_REASON_BUILD_DISABLED));
    ASSERT_STREQ("threaded mode", GetExecutionReasonString(EXECUTION_REASON_THREADED));
    ASSERT_STREQ("platform thread support unavailable", GetExecutionReasonString(EXECUTION_REASON_THREADS_UNSUPPORTED));
    ASSERT_STREQ("threaded executor unavailable", GetExecutionReasonString(EXECUTION_REASON_EXECUTOR_UNAVAILABLE));
    ASSERT_STREQ("invalid execution mode", GetExecutionReasonString(EXECUTION_REASON_INVALID_MODE));
}

// Verify reported capabilities match this actual binary, independently of the pure mode matrix.
TEST(RenderExecution, BuildCapabilities)
{
    ExecutionCapabilities unavailable = GetExecutionCapabilities(false);
    ExecutionCapabilities available = GetExecutionCapabilities(true);
#if defined(DM_THREADED_RENDERING) && DM_THREADED_RENDERING
    ASSERT_TRUE(unavailable.m_BuildEnabled);
#else
    ASSERT_FALSE(unavailable.m_BuildEnabled);
#endif
#if !defined(DM_USE_SINGLE_THREAD) && (defined(DM_HAS_THREADS) || defined(__EMSCRIPTEN_PTHREADS__))
    ASSERT_TRUE(unavailable.m_ThreadsSupported);
#else
    ASSERT_FALSE(unavailable.m_ThreadsSupported);
#endif
    ASSERT_FALSE(unavailable.m_ExecutorAvailable);
    ASSERT_TRUE(available.m_ExecutorAvailable);
    ASSERT_EQ(unavailable.m_BuildEnabled, available.m_BuildEnabled);
    ASSERT_EQ(unavailable.m_ThreadsSupported, available.m_ThreadsSupported);
}

static unsigned int OwnerMask(HRenderExecution execution)
{
    unsigned int mask = 0;
    if (IsRenderExecutionOwner(execution, EXECUTION_OWNER_GAME))
        mask |= 1;
    if (IsRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER))
        mask |= 2;
    return mask;
}

// Verify real creation cannot activate a missing executor, and a disabled build cannot be overridden by a requested mode.
TEST(RenderExecution, CreationWithoutExecutor)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(false);
    ExecutionSelection selection;
    HRenderExecution execution = NewRenderExecution(EXECUTION_MODE_THREADED, false, &selection);
    if (!capabilities.m_BuildEnabled)
    {
        ASSERT_NE((HRenderExecution)0, execution);
        ASSERT_EQ(EXECUTION_MODE_INLINE, selection.m_ExecutionMode);
        ASSERT_EQ(EXECUTION_REASON_BUILD_DISABLED, selection.m_ExecutionReason);
        ASSERT_EQ(3u, OwnerMask(execution));
        ASSERT_FALSE(BindRenderExecutionOwner(execution));
        DeleteRenderExecution(execution);
    }
    else
    {
        ASSERT_EQ((HRenderExecution)0, execution);
        ASSERT_EQ(EXECUTION_MODE_INVALID, selection.m_ExecutionMode);
        ASSERT_EQ(capabilities.m_ThreadsSupported ? EXECUTION_REASON_EXECUTOR_UNAVAILABLE : EXECUTION_REASON_THREADS_UNSUPPORTED, selection.m_ExecutionReason);
    }
}

// Verify invalid startup modes do not create a context, even when an executor is reported available.
TEST(RenderExecution, CreationInvalidMode)
{
    ExecutionSelection selection;
    ASSERT_EQ((HRenderExecution)0, NewRenderExecution(EXECUTION_MODE_INVALID, true, &selection));
    ASSERT_EQ(EXECUTION_MODE_INVALID, selection.m_ExecutionMode);
    ASSERT_EQ(EXECUTION_REASON_INVALID_MODE, selection.m_ExecutionReason);
}

// Verify explicit inline creation binds both owners to the caller without a worker or subsequent binding.
TEST(RenderExecution, InlineOwners)
{
    ExecutionSelection selection;
    HRenderExecution execution = NewRenderExecution(EXECUTION_MODE_INLINE, true, &selection);
    ASSERT_NE((HRenderExecution)0, execution);
    ASSERT_EQ(EXECUTION_MODE_INLINE, selection.m_ExecutionMode);
    ASSERT_EQ(3u, OwnerMask(execution));
    ASSERT_FALSE(BindRenderExecutionOwner(execution));
    assert(IsRenderExecutionOwner(execution, EXECUTION_OWNER_GAME));
    assert(IsRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER));
    DeleteRenderExecution(execution);
}

// Verify deleting one context does not clear another context's thread-local ownership.
TEST(RenderExecution, IndependentInlineContexts)
{
    ExecutionSelection selection;
    HRenderExecution first = NewRenderExecution(EXECUTION_MODE_INLINE, false, &selection);
    HRenderExecution second = NewRenderExecution(EXECUTION_MODE_INLINE, false, &selection);
    ASSERT_NE((HRenderExecution)0, first);
    ASSERT_NE((HRenderExecution)0, second);
    ASSERT_EQ(3u, OwnerMask(first));
    ASSERT_EQ(3u, OwnerMask(second));
    DeleteRenderExecution(first);
    ASSERT_EQ(3u, OwnerMask(second));
    assert(IsRenderExecutionOwner(second, EXECUTION_OWNER_GAME));
    DeleteRenderExecution(second);
}

struct InlineProbe
{
    HRenderExecution m_Execution;
    unsigned int m_Owners;
    uint8_t m_Bound : 1;
};

static void ProbeInlineWorker(void* context)
{
    InlineProbe* probe = (InlineProbe*)context;
    probe->m_Owners = OwnerMask(probe->m_Execution);
    probe->m_Bound = BindRenderExecutionOwner(probe->m_Execution);
}

// Verify inline ownership is thread-local, including on Windows where current-thread pseudo-handles compare equal.
TEST(RenderExecution, InlineRejectsOtherThread)
{
    if (!GetExecutionCapabilities(false).m_ThreadsSupported)
    {
        return;
    }

    ExecutionSelection selection;
    HRenderExecution execution = NewRenderExecution(EXECUTION_MODE_INLINE, false, &selection);
    ASSERT_NE((HRenderExecution)0, execution);
    InlineProbe probe = { execution, 3u, true };
    dmThread::Thread worker = dmThread::New(ProbeInlineWorker, 0x80000, &probe, "owner-probe");
    dmThread::Join(worker);
    ASSERT_EQ(0u, probe.m_Owners);
    ASSERT_FALSE(probe.m_Bound);
    ASSERT_EQ(3u, OwnerMask(execution));
    DeleteRenderExecution(execution);
}

struct ThreadedProbe
{
    HRenderExecution m_Execution;
    unsigned int     m_BeforeOwners;
    unsigned int     m_BoundOwners;
    unsigned int     m_ReleasedOwners;
    uint8_t          m_FirstBind : 1;
    uint8_t          m_SecondBind : 1;
    uint8_t          m_BindAfterRelease : 1;
};

static void ProbeThreadedWorker(void* context)
{
    ThreadedProbe* probe = (ThreadedProbe*)context;
    probe->m_BeforeOwners = OwnerMask(probe->m_Execution);
    probe->m_FirstBind = BindRenderExecutionOwner(probe->m_Execution);
    probe->m_BoundOwners = OwnerMask(probe->m_Execution);
    probe->m_SecondBind = BindRenderExecutionOwner(probe->m_Execution);
    if (probe->m_FirstBind)
    {
        assert(IsRenderExecutionOwner(probe->m_Execution, EXECUTION_OWNER_RENDER));
        ReleaseRenderExecutionOwner(probe->m_Execution);
    }
    probe->m_ReleasedOwners = OwnerMask(probe->m_Execution);
    probe->m_BindAfterRelease = BindRenderExecutionOwner(probe->m_Execution);
}

// Verify an available threaded mode binds exactly one distinct owner and cannot rebind after release or thread exit.
TEST(RenderExecution, ThreadedOwnerLifecycle)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    ExecutionSelection selection;
    HRenderExecution execution = NewRenderExecution(EXECUTION_MODE_THREADED, true, &selection);
    if (!capabilities.m_BuildEnabled)
    {
        ASSERT_NE((HRenderExecution)0, execution);
        ASSERT_EQ(EXECUTION_MODE_INLINE, selection.m_ExecutionMode);
        ASSERT_EQ(EXECUTION_REASON_BUILD_DISABLED, selection.m_ExecutionReason);
        ASSERT_FALSE(BindRenderExecutionOwner(execution));
        DeleteRenderExecution(execution);
        return;
    }
    if (!capabilities.m_ThreadsSupported)
    {
        ASSERT_EQ((HRenderExecution)0, execution);
        ASSERT_EQ(EXECUTION_MODE_INVALID, selection.m_ExecutionMode);
        ASSERT_EQ(EXECUTION_REASON_THREADS_UNSUPPORTED, selection.m_ExecutionReason);
        return;
    }

    ASSERT_NE((HRenderExecution)0, execution);
    ASSERT_EQ(EXECUTION_MODE_THREADED, selection.m_ExecutionMode);
    ASSERT_EQ(EXECUTION_REASON_THREADED, selection.m_ExecutionReason);
    ASSERT_EQ(1u, OwnerMask(execution));
    ASSERT_FALSE(BindRenderExecutionOwner(execution));
    ThreadedProbe probe = {};
    probe.m_Execution = execution;
    dmThread::Thread worker = dmThread::New(ProbeThreadedWorker, 0x80000, &probe, "render-owner");
    dmThread::Join(worker);
    ASSERT_EQ(0u, probe.m_BeforeOwners);
    ASSERT_TRUE(probe.m_FirstBind);
    ASSERT_FALSE(probe.m_SecondBind);
    ASSERT_EQ(2u, probe.m_BoundOwners);
    ASSERT_EQ(0u, probe.m_ReleasedOwners);
    ASSERT_FALSE(probe.m_BindAfterRelease);
    ASSERT_EQ(1u, OwnerMask(execution));

    InlineProbe later = { execution, 3u, true };
    worker = dmThread::New(ProbeInlineWorker, 0x80000, &later, "late-owner");
    dmThread::Join(worker);
    ASSERT_EQ(0u, later.m_Owners);
    ASSERT_FALSE(later.m_Bound);
    DeleteRenderExecution(execution);
}

struct IndependentProbe
{
    HRenderExecution m_First;
    HRenderExecution m_Second;
    unsigned int m_FirstAfterSecondBind;
    unsigned int m_SecondAfterFirstRelease;
    uint8_t m_FirstBound : 1;
    uint8_t m_SecondBound : 1;
};

static void ProbeIndependentWorker(void* context)
{
    IndependentProbe* probe = (IndependentProbe*)context;
    probe->m_FirstBound = BindRenderExecutionOwner(probe->m_First);
    probe->m_SecondBound = BindRenderExecutionOwner(probe->m_Second);
    probe->m_FirstAfterSecondBind = OwnerMask(probe->m_First);
    if (probe->m_FirstBound)
        ReleaseRenderExecutionOwner(probe->m_First);
    probe->m_SecondAfterFirstRelease = OwnerMask(probe->m_Second);
    if (probe->m_SecondBound)
        ReleaseRenderExecutionOwner(probe->m_Second);
}

// Verify binding and releasing renderer ownership in one context cannot overwrite another context's TLS membership.
TEST(RenderExecution, IndependentThreadedContexts)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    if (!capabilities.m_BuildEnabled || !capabilities.m_ThreadsSupported)
        return;
    ExecutionSelection selection;
    HRenderExecution first = NewRenderExecution(EXECUTION_MODE_THREADED, true, &selection);
    HRenderExecution second = NewRenderExecution(EXECUTION_MODE_THREADED, true, &selection);
    ASSERT_NE((HRenderExecution)0, first);
    ASSERT_NE((HRenderExecution)0, second);
    IndependentProbe probe = {};
    probe.m_First = first;
    probe.m_Second = second;
    dmThread::Thread worker = dmThread::New(ProbeIndependentWorker, 0x80000, &probe, "owner-pair");
    dmThread::Join(worker);
    ASSERT_TRUE(probe.m_FirstBound);
    ASSERT_TRUE(probe.m_SecondBound);
    ASSERT_EQ(2u, probe.m_FirstAfterSecondBind);
    ASSERT_EQ(2u, probe.m_SecondAfterFirstRelease);
    ASSERT_EQ(1u, OwnerMask(first));
    ASSERT_EQ(1u, OwnerMask(second));
    DeleteRenderExecution(first);
    ASSERT_EQ(1u, OwnerMask(second));
    DeleteRenderExecution(second);
}

struct BindRace
{
    HRenderExecution m_Execution;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    unsigned int m_ReadyCount;
    unsigned int m_AttemptCount;
    uint8_t m_Start : 1;
    uint8_t m_Release : 1;
};

struct BindContender
{
    BindRace* m_Race;
    unsigned int m_BoundOwners;
    unsigned int m_ReleasedOwners;
    uint8_t m_Bound : 1;
    uint8_t m_BindAfterRelease : 1;
};

static void CompeteForRenderOwner(void* context)
{
    BindContender* contender = (BindContender*)context;
    BindRace* race = contender->m_Race;
    dmMutex::Lock(race->m_Mutex);
    ++race->m_ReadyCount;
    dmConditionVariable::Broadcast(race->m_Changed);
    while (!race->m_Start)
        dmConditionVariable::Wait(race->m_Changed, race->m_Mutex);
    dmMutex::Unlock(race->m_Mutex);

    contender->m_Bound = BindRenderExecutionOwner(race->m_Execution);
    contender->m_BoundOwners = OwnerMask(race->m_Execution);

    dmMutex::Lock(race->m_Mutex);
    ++race->m_AttemptCount;
    dmConditionVariable::Broadcast(race->m_Changed);
    // Hold the winning binding until both attempts have completed.
    while (!race->m_Release)
        dmConditionVariable::Wait(race->m_Changed, race->m_Mutex);
    dmMutex::Unlock(race->m_Mutex);

    if (contender->m_Bound)
        ReleaseRenderExecutionOwner(race->m_Execution);
    contender->m_ReleasedOwners = OwnerMask(race->m_Execution);
    contender->m_BindAfterRelease = BindRenderExecutionOwner(race->m_Execution);
}

// Verify simultaneous binding attempts choose exactly one owner, with explicit barriers instead of timing assumptions.
TEST(RenderExecution, CompetingRenderOwnerBindings)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    if (!capabilities.m_BuildEnabled || !capabilities.m_ThreadsSupported)
        return;
    ExecutionSelection selection;
    HRenderExecution execution = NewRenderExecution(EXECUTION_MODE_THREADED, true, &selection);
    ASSERT_NE((HRenderExecution)0, execution);

    BindRace race = {};
    race.m_Execution = execution;
    race.m_Mutex = dmMutex::New();
    race.m_Changed = dmConditionVariable::New();
    BindContender contenders[2] = {};
    dmThread::Thread workers[2];
    for (unsigned int i = 0; i < 2; ++i)
    {
        contenders[i].m_Race = &race;
        workers[i] = dmThread::New(CompeteForRenderOwner, 0x80000, &contenders[i], "owner-race");
    }

    dmMutex::Lock(race.m_Mutex);
    while (race.m_ReadyCount != 2)
        dmConditionVariable::Wait(race.m_Changed, race.m_Mutex);
    race.m_Start = true;
    dmConditionVariable::Broadcast(race.m_Changed);
    while (race.m_AttemptCount != 2)
        dmConditionVariable::Wait(race.m_Changed, race.m_Mutex);
    race.m_Release = true;
    dmConditionVariable::Broadcast(race.m_Changed);
    dmMutex::Unlock(race.m_Mutex);

    for (unsigned int i = 0; i < 2; ++i)
        dmThread::Join(workers[i]);
    dmConditionVariable::Delete(race.m_Changed);
    dmMutex::Delete(race.m_Mutex);

    ASSERT_EQ(1u, (unsigned int)contenders[0].m_Bound + (unsigned int)contenders[1].m_Bound);
    for (unsigned int i = 0; i < 2; ++i)
    {
        ASSERT_EQ(contenders[i].m_Bound ? 2u : 0u, contenders[i].m_BoundOwners);
        ASSERT_EQ(0u, contenders[i].m_ReleasedOwners);
        ASSERT_FALSE(contenders[i].m_BindAfterRelease);
    }
    ASSERT_EQ(1u, OwnerMask(execution));
    DeleteRenderExecution(execution);
}

struct WorkerGameOwner
{
    ExecutionSelection m_Selection;
    HRenderExecution m_Execution;
    dmMutex::HMutex m_Mutex;
    dmConditionVariable::HConditionVariable m_Changed;
    unsigned int m_InitialOwners;
    unsigned int m_FinalOwners;
    uint8_t m_Created : 1;
    uint8_t m_RenderFinished : 1;
};

static void CreateOnGameWorker(void* context)
{
    WorkerGameOwner* game = (WorkerGameOwner*)context;
    game->m_Execution = NewRenderExecution(EXECUTION_MODE_THREADED, true, &game->m_Selection);
    if (game->m_Execution)
        game->m_InitialOwners = OwnerMask(game->m_Execution);

    dmMutex::Lock(game->m_Mutex);
    game->m_Created = true;
    dmConditionVariable::Broadcast(game->m_Changed);
    while (!game->m_RenderFinished)
        dmConditionVariable::Wait(game->m_Changed, game->m_Mutex);
    dmMutex::Unlock(game->m_Mutex);

    if (game->m_Execution)
    {
        game->m_FinalOwners = OwnerMask(game->m_Execution);
        DeleteRenderExecution(game->m_Execution);
    }
}

// Verify web-style reversed placement: a game worker creates/deletes the context and the test main thread owns rendering.
TEST(RenderExecution, GameWorkerWithMainRenderOwner)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    if (!capabilities.m_BuildEnabled || !capabilities.m_ThreadsSupported)
        return;
    WorkerGameOwner game = {};
    game.m_Mutex = dmMutex::New();
    game.m_Changed = dmConditionVariable::New();
    dmThread::Thread worker = dmThread::New(CreateOnGameWorker, 0x80000, &game, "game-owner");

    dmMutex::Lock(game.m_Mutex);
    while (!game.m_Created)
        dmConditionVariable::Wait(game.m_Changed, game.m_Mutex);
    dmMutex::Unlock(game.m_Mutex);

    unsigned int initial_owners = 0;
    unsigned int bound_owners = 0;
    unsigned int released_owners = 0;
    bool bound = false;
    bool rebound = false;
    bool created = game.m_Execution != 0;
    if (created)
    {
        initial_owners = OwnerMask(game.m_Execution);
        bound = BindRenderExecutionOwner(game.m_Execution);
        bound_owners = OwnerMask(game.m_Execution);
        if (bound)
            ReleaseRenderExecutionOwner(game.m_Execution);
        released_owners = OwnerMask(game.m_Execution);
        rebound = BindRenderExecutionOwner(game.m_Execution);
    }

    // No context access after this signal: deletion belongs to the waiting game worker.
    dmMutex::Lock(game.m_Mutex);
    game.m_RenderFinished = true;
    dmConditionVariable::Broadcast(game.m_Changed);
    dmMutex::Unlock(game.m_Mutex);
    dmThread::Join(worker);
    dmConditionVariable::Delete(game.m_Changed);
    dmMutex::Delete(game.m_Mutex);

    ASSERT_TRUE(created);
    ASSERT_EQ(EXECUTION_MODE_THREADED, game.m_Selection.m_ExecutionMode);
    ASSERT_EQ(1u, game.m_InitialOwners);
    ASSERT_EQ(1u, game.m_FinalOwners);
    ASSERT_EQ(0u, initial_owners);
    ASSERT_TRUE(bound);
    ASSERT_EQ(2u, bound_owners);
    ASSERT_EQ(0u, released_owners);
    ASSERT_FALSE(rebound);
}

#if !defined(JC_TEST_NO_DEATH_TEST) && !defined(NDEBUG)
// Verify a threaded game's actual owner cannot perform render work, including graphics calls, before a consumer binds.
TEST(RenderExecution, ThreadedGameOwnerAssertion)
{
    ExecutionCapabilities capabilities = GetExecutionCapabilities(true);
    if (!capabilities.m_BuildEnabled || !capabilities.m_ThreadsSupported)
        return;
    ExecutionSelection selection;
    HRenderExecution execution = NewRenderExecution(EXECUTION_MODE_THREADED, true, &selection);
    ASSERT_NE((HRenderExecution)0, execution);
    ASSERT_EQ(1u, OwnerMask(execution));
    ASSERT_DEATH(assert(IsRenderExecutionOwner(execution, EXECUTION_OWNER_RENDER)), "");
    ASSERT_EQ(1u, OwnerMask(execution));
    DeleteRenderExecution(execution);
}
#endif

extern "C" void dmExportedSymbols();

int main(int argc, char** argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
