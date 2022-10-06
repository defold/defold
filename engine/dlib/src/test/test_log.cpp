// Copyright 2020-2022 The Defold Foundation
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

#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>
#include "../dlib/array.h"
#include "../dlib/dlib.h"
#include "../dlib/hash.h"
#include "../dlib/log.h"
#include "../dlib/mutex.h"
#include "../dlib/dstrings.h"
#include "../dlib/socket.h"
#include "../dlib/thread.h"
#include "../dlib/path.h"
#include "../dlib/sys.h"
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

TEST(dmLog, Init)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmLog::LogFinalize();
}

static void LogThread(void* arg)
{
    dmLogWarning("a warning %d", 123);
    dmLogError("an error %d", 456);

    int n = 1024 * 1024;
    char* s = new char[n];
    for (int i = 0; i < n; ++i) {
        s[i] = '.';
    }
    const char* msg = "Very large message";
    memcpy(s, msg, strlen(msg));
    s[n-1] = '\0';
    dmLogInfo("%s", s);
    delete[] s;
}

struct ThreadContext
{
    int m_ThreadID;
    int m_LoopCount;
};

static void LogThreadSmall(void* arg)
{
    ThreadContext ctx = *(ThreadContext*)arg;
    int threadid = ctx.m_ThreadID;
    int n = ctx.m_LoopCount;
    for (int i = 0; i < n; ++i) {
        dmLogInfo("%d", threadid);
    }
}

struct CustomLogContext
{
    int             m_NumWritten; // full message with log domain, severity + newline
    dmMutex::HMutex m_Mutex;
} g_CustomLogContext;

static void CustomLogListener(LogSeverity severity, const char* domain, const char* formatted_string)
{
    CustomLogContext* ctx = &g_CustomLogContext;

    bool result = dmMutex::TryLock(ctx->m_Mutex);
    assert(result && "The lock was already taken for log listener");
    if (!result)
        return;
    ctx->m_NumWritten += strlen(formatted_string);
    dmMutex::Unlock(ctx->m_Mutex);

    // This is not supported, and should be sent to the void
    dmLogInfo("Calling dmLog::LogInternal from the Log Listener");
}



#if !(defined(__EMSCRIPTEN__) || defined(__NX__))
TEST(dmLog, Client)
{
    char buf[256];
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    uint16_t port = dmLog::GetPort();
    ASSERT_GT(port, 0);
    dmSnPrintf(buf, sizeof(buf), "python src/test/test_log.py %d", port);
#ifdef _WIN32
    FILE* f = _popen(buf, "rb");
#else
    FILE* f = popen(buf, "r");
#endif
    ASSERT_NE((void*) 0, f);
    // Wait for test_log.py to be ready, ie connection established
    int c = fgetc(f);
    ASSERT_EQ(255, c);
    ASSERT_NE((void*) 0, f);
    dmThread::Thread log_thread = dmThread::New(LogThread, 0x80000, 0, "test");

    size_t n;

    do
    {
        n = fread(buf, 1, 1, f);
        for (size_t i = 0; i < n; ++i)
            printf("%c", buf[i]);
    } while (n > 0);

#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
    dmThread::Join(log_thread);
    dmLog::LogFinalize();
}
#endif

TEST(dmLog, Stress)
{
    dLib::SetDebugMode(false);

    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    CustomLogContext& log_ctx = g_CustomLogContext;
    memset(&log_ctx, 0, sizeof(log_ctx));
    log_ctx.m_Mutex = dmMutex::New();

    dmLogRegisterListener(CustomLogListener);

    const int loop_count = 100;
    const int num_threads = 4;
    dmThread::Thread threads[num_threads] = {0};
    ThreadContext thread_ctx[num_threads];

    for (int i = 0; i < num_threads; ++i)
    {
        ThreadContext& ctx = thread_ctx[i];
        ctx.m_ThreadID = i;
        ctx.m_LoopCount = loop_count;
        threads[i] = dmThread::New(LogThreadSmall, 0x80000, &ctx, "test");
    }

    for (int i = 0; i < num_threads; ++i)
    {
        dmThread::Join(threads[i]);
    }

    // wait for thread to join, thus making sure we get all the pending log messages
    dmLog::LogFinalize();

    dmLogInfo("Regular output reenabled.");
    dmLogInfo("  Custom log listener printed %d characters", log_ctx.m_NumWritten);

    // Size of each message should be 13: 'INFO:DLIB: %d\n'
    int expected_count_listener = 13 * num_threads * loop_count;
    ASSERT_EQ(expected_count_listener, log_ctx.m_NumWritten);

    dmMutex::Delete(log_ctx.m_Mutex);
    dLib::SetDebugMode(true);
}

TEST(dmLog, LogFile)
{
    if (!dLib::FeaturesSupported(DM_FEATURE_BIT_SOCKET_SERVER_TCP))
    {
        printf("Test disabled due to platform not supporting TCP");
        return;
    }

    char path[DMPATH_MAX_PATH];
    dmSys::GetLogPath(path, sizeof(path));
    dmStrlCat(path, "log.txt", sizeof(path));

    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmLog::SetLogFile(path);
    dmLogInfo("TESTING_LOG");
    dmLog::LogFinalize();

    char tmp[1024];
    FILE* f = fopen(path, "rb");
    ASSERT_NE((FILE*) 0, f);
    if (f) {
        fread(tmp, 1, sizeof(tmp), f);
        ASSERT_TRUE(strstr(tmp, "TESTING_LOG") != 0);
        fclose(f);
    }
    dmSys::Unlink(path);
}

dmArray<char> g_LogListenerOutput;

static void TestLogCaptureCallback(LogSeverity severity, const char* domain, const char* formatted_string)
{
    if (strstr(formatted_string, "Log server started on port") != 0)
        return;

    dmArray<char>* log_output = &g_LogListenerOutput;
    uint32_t len = (uint32_t)strlen(formatted_string);
    log_output->SetCapacity(log_output->Size() + len + 1);
    log_output->PushArray(formatted_string, len);
}

TEST(dmLog, TestCapture)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    dmLogRegisterListener(TestLogCaptureCallback);
    dmLogDebug("This is a debug message");
    dmLogInfo("This is a info message");
    dmLogWarning("This is a warning message");
    dmLogError("This is a error message");
    dmLogFatal("This is a fatal message");

    // wait for thread to join, thus making sure we get all the pending log messages
    dmLog::LogFinalize();

    // Make the string null terminated
    g_LogListenerOutput.SetCapacity(g_LogListenerOutput.Size()+1);
    g_LogListenerOutput.Push(0);

    const char* ExpectedOutput =
                "INFO:DLIB: This is a info message\n"
                "WARNING:DLIB: This is a warning message\n"
                "ERROR:DLIB: This is a error message\n"
                "FATAL:DLIB: This is a fatal message\n";

    ASSERT_STREQ(ExpectedOutput, g_LogListenerOutput.Begin());
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}

