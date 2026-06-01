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

enum TestCallback
{
    TEST_CALLBACK_CURRENT_ACTION   = 1,
    TEST_CALLBACK_PREVIOUS_ACTION  = 2,
    TEST_CALLBACK_PREVIOUS_HANDLER = 3,
};

static const int MAX_CALLBACK_RECORDS = 8;

static volatile sig_atomic_t g_CurrentSignalActionCalls = 0;
static volatile sig_atomic_t g_PreviousSignalActionCalls = 0;
static volatile sig_atomic_t g_PreviousSignalHandlerCalls = 0;
static volatile sig_atomic_t g_PreviousSignalActionWasRestored = 0;
static volatile sig_atomic_t g_PreviousSignalHandlerWasRestored = 0;
static volatile sig_atomic_t g_CallbackOrderCount = 0;
static volatile sig_atomic_t g_CallbackOrder[MAX_CALLBACK_RECORDS];
static volatile sig_atomic_t g_ChainFromCurrentSignalAction = 0;
static struct sigaction g_TestPreviousSignalActions[dmCrash::MAX_SIGNAL_COUNT];

static void RecordCallback(TestCallback callback)
{
    sig_atomic_t index = g_CallbackOrderCount;
    if (index < MAX_CALLBACK_RECORDS)
    {
        g_CallbackOrder[index] = callback;
    }
    g_CallbackOrderCount = index + 1;
}

static void ResetCallbackState()
{
    g_CurrentSignalActionCalls = 0;
    g_PreviousSignalActionCalls = 0;
    g_PreviousSignalHandlerCalls = 0;
    g_PreviousSignalActionWasRestored = 0;
    g_PreviousSignalHandlerWasRestored = 0;
    g_CallbackOrderCount = 0;
    g_ChainFromCurrentSignalAction = 0;

    for (int i = 0; i < MAX_CALLBACK_RECORDS; ++i)
    {
        g_CallbackOrder[i] = 0;
    }
}

static void TestCurrentSignalAction(int signum, siginfo_t* si, void* sc)
{
    g_CurrentSignalActionCalls++;
    RecordCallback(TEST_CALLBACK_CURRENT_ACTION);

    if (g_ChainFromCurrentSignalAction)
    {
        dmCrash::ResetToDefaultSignalHandler(signum);
        dmCrash::ChainSignalOrRaiseDefault(signum, si, sc, g_TestPreviousSignalActions, TestCurrentSignalAction);
    }
}

static void TestPreviousSignalAction(int signum, siginfo_t* si, void* sc)
{
    (void)si;
    (void)sc;

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    g_PreviousSignalActionCalls++;
    RecordCallback(TEST_CALLBACK_PREVIOUS_ACTION);
    g_PreviousSignalActionWasRestored = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestPreviousSignalAction;
}

static void TestPreviousSignalHandler(int signum)
{
    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    g_PreviousSignalHandlerCalls++;
    RecordCallback(TEST_CALLBACK_PREVIOUS_HANDLER);
    g_PreviousSignalHandlerWasRestored = current.sa_handler == TestPreviousSignalHandler;
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

    ClearSignalActions(g_TestPreviousSignalActions);
    ResetCallbackState();

    bool first_install = dmCrash::InstallSignalHandler(signum, TestCurrentSignalAction, g_TestPreviousSignalActions);
    bool second_install = dmCrash::InstallSignalHandler(signum, TestCurrentSignalAction, g_TestPreviousSignalActions);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool installed_current = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestCurrentSignalAction;
    bool preserved_previous = g_TestPreviousSignalActions[signum].sa_handler == TestPreviousSignalHandler;

    g_ChainFromCurrentSignalAction = 1;
    raise(signum);
    g_ChainFromCurrentSignalAction = 0;

    sigaction(signum, &original, 0);

    ASSERT_TRUE(first_install);
    ASSERT_TRUE(second_install);
    ASSERT_TRUE(installed_current);
    ASSERT_TRUE(preserved_previous);
    ASSERT_EQ(1, (int)g_CurrentSignalActionCalls);
    ASSERT_EQ(0, (int)g_PreviousSignalActionCalls);
    ASSERT_EQ(1, (int)g_PreviousSignalHandlerCalls);
    ASSERT_TRUE(g_PreviousSignalHandlerWasRestored);
    ASSERT_EQ(2, (int)g_CallbackOrderCount);
    ASSERT_EQ((int)TEST_CALLBACK_CURRENT_ACTION, (int)g_CallbackOrder[0]);
    ASSERT_EQ((int)TEST_CALLBACK_PREVIOUS_HANDLER, (int)g_CallbackOrder[1]);
}

TEST(dmCrashPosixSignalTest, TestInstallSignalHandlerChainsPreviousActionAfterCurrent)
{
    const int signum = SIGUSR1;

    struct sigaction original;
    memset(&original, 0, sizeof(original));
    sigaction(signum, 0, &original);

    struct sigaction previous;
    memset(&previous, 0, sizeof(previous));
    sigemptyset(&previous.sa_mask);
    previous.sa_sigaction = TestPreviousSignalAction;
    previous.sa_flags = SA_SIGINFO;
    sigaction(signum, &previous, 0);

    ClearSignalActions(g_TestPreviousSignalActions);
    ResetCallbackState();

    bool installed = dmCrash::InstallSignalHandler(signum, TestCurrentSignalAction, g_TestPreviousSignalActions);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool installed_current = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestCurrentSignalAction;
    bool preserved_previous = (g_TestPreviousSignalActions[signum].sa_flags & SA_SIGINFO) && g_TestPreviousSignalActions[signum].sa_sigaction == TestPreviousSignalAction;

    g_ChainFromCurrentSignalAction = 1;
    raise(signum);
    g_ChainFromCurrentSignalAction = 0;

    sigaction(signum, &original, 0);

    ASSERT_TRUE(installed);
    ASSERT_TRUE(installed_current);
    ASSERT_TRUE(preserved_previous);
    ASSERT_EQ(1, (int)g_CurrentSignalActionCalls);
    ASSERT_EQ(1, (int)g_PreviousSignalActionCalls);
    ASSERT_EQ(0, (int)g_PreviousSignalHandlerCalls);
    ASSERT_TRUE(g_PreviousSignalActionWasRestored);
    ASSERT_EQ(2, (int)g_CallbackOrderCount);
    ASSERT_EQ((int)TEST_CALLBACK_CURRENT_ACTION, (int)g_CallbackOrder[0]);
    ASSERT_EQ((int)TEST_CALLBACK_PREVIOUS_ACTION, (int)g_CallbackOrder[1]);
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

    ResetCallbackState();

    dmCrash::ChainSignalOrRaiseDefault(signum, &si, 0, previous_signal_actions, TestCurrentSignalAction);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool restored_previous = (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == TestPreviousSignalAction;

    sigaction(signum, &original, 0);

    ASSERT_EQ(0, (int)g_CurrentSignalActionCalls);
    ASSERT_EQ(1, (int)g_PreviousSignalActionCalls);
    ASSERT_EQ(0, (int)g_PreviousSignalHandlerCalls);
    ASSERT_TRUE(g_PreviousSignalActionWasRestored);
    ASSERT_TRUE(restored_previous);
    ASSERT_EQ(1, (int)g_CallbackOrderCount);
    ASSERT_EQ((int)TEST_CALLBACK_PREVIOUS_ACTION, (int)g_CallbackOrder[0]);
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

    ResetCallbackState();

    dmCrash::ChainSignalOrRaiseDefault(signum, &si, 0, previous_signal_actions, TestCurrentSignalAction);

    struct sigaction current;
    memset(&current, 0, sizeof(current));
    sigaction(signum, 0, &current);

    bool restored_ignored = current.sa_handler == SIG_IGN;

    sigaction(signum, &original, 0);

    ASSERT_EQ(0, (int)g_CurrentSignalActionCalls);
    ASSERT_EQ(0, (int)g_PreviousSignalActionCalls);
    ASSERT_EQ(0, (int)g_PreviousSignalHandlerCalls);
    ASSERT_TRUE(restored_ignored);
    ASSERT_EQ(0, (int)g_CallbackOrderCount);
}

int main(int argc, char **argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
