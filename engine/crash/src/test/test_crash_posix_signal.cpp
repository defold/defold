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

#include <signal.h>
#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include "../backtrace_signal_posix.h"

static volatile sig_atomic_t g_PreviousSignalActionCalls = 0;
static volatile sig_atomic_t g_PreviousSignalActionWasRestored = 0;

static void TestCurrentSignalAction(int signum, siginfo_t* si, void* sc)
{
    (void)signum;
    (void)si;
    (void)sc;
}

static void TestPreviousSignalAction(int signum, siginfo_t* si, void* sc)
{
    (void)si;
    (void)sc;

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    g_PreviousSignalActionCalls++;
    g_PreviousSignalActionWasRestored = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestPreviousSignalAction;
}

static void TestPreviousSignalHandler(int signum)
{
    (void)signum;
}

static void ClearSignalActions(struct sigaction* signal_actions)
{
    memset(signal_actions, 0, sizeof(struct sigaction) * dmCrash::MAX_SIGNAL_COUNT);
}

TEST(dmCrashPosixSignalTest, TestInstallSignalHandlerPreservesPreviousHandler)
{
    const int signum = SIGUSR1;

    struct sigaction original;
    memset(&original, 0, sizeof(original));
    sigaction(signum, 0, &original);

    struct sigaction previous;
    memset(&previous, 0, sizeof(previous));
    sigemptyset(&previous.sa_mask);
    previous.sa_handler = TestPreviousSignalHandler;
    previous.sa_flags = 0;
    sigaction(signum, &previous, 0);

    struct sigaction previous_signal_actions[dmCrash::MAX_SIGNAL_COUNT];
    ClearSignalActions(previous_signal_actions);

    bool first_install = dmCrash::InstallSignalHandler(signum, TestCurrentSignalAction, previous_signal_actions);
    bool second_install = dmCrash::InstallSignalHandler(signum, TestCurrentSignalAction, previous_signal_actions);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool installed_current = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestCurrentSignalAction;
    bool preserved_previous = previous_signal_actions[signum].sa_handler == TestPreviousSignalHandler;

    sigaction(signum, &original, 0);

    ASSERT_TRUE(first_install);
    ASSERT_TRUE(second_install);
    ASSERT_TRUE(installed_current);
    ASSERT_TRUE(preserved_previous);
}

TEST(dmCrashPosixSignalTest, TestChainSignalRestoresPreviousActionBeforeCalling)
{
    const int signum = SIGUSR1;

    struct sigaction original;
    memset(&original, 0, sizeof(original));
    sigaction(signum, 0, &original);

    struct sigaction previous_signal_actions[dmCrash::MAX_SIGNAL_COUNT];
    ClearSignalActions(previous_signal_actions);
    sigemptyset(&previous_signal_actions[signum].sa_mask);
    previous_signal_actions[signum].sa_sigaction = TestPreviousSignalAction;
    previous_signal_actions[signum].sa_flags = SA_SIGINFO;

    siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.si_signo = signum;

    g_PreviousSignalActionCalls = 0;
    g_PreviousSignalActionWasRestored = 0;

    dmCrash::ChainSignalOrRaiseDefault(signum, &si, 0, previous_signal_actions, TestCurrentSignalAction);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool restored_previous = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestPreviousSignalAction;

    sigaction(signum, &original, 0);

    ASSERT_EQ(1, (int)g_PreviousSignalActionCalls);
    ASSERT_TRUE(g_PreviousSignalActionWasRestored);
    ASSERT_TRUE(restored_previous);
}

TEST(dmCrashPosixSignalTest, TestChainSignalRestoresIgnoredPreviousAction)
{
    const int signum = SIGUSR1;

    struct sigaction original;
    memset(&original, 0, sizeof(original));
    sigaction(signum, 0, &original);

    struct sigaction previous_signal_actions[dmCrash::MAX_SIGNAL_COUNT];
    ClearSignalActions(previous_signal_actions);
    sigemptyset(&previous_signal_actions[signum].sa_mask);
    previous_signal_actions[signum].sa_handler = SIG_IGN;
    previous_signal_actions[signum].sa_flags = 0;

    siginfo_t si;
    memset(&si, 0, sizeof(si));
    si.si_signo = signum;

    dmCrash::ChainSignalOrRaiseDefault(signum, &si, 0, previous_signal_actions, TestCurrentSignalAction);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool restored_ignored = current.sa_handler == SIG_IGN;

    sigaction(signum, &original, 0);

    ASSERT_TRUE(restored_ignored);
}
