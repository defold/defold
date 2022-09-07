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
#include "../dlib/time.h"
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
    int             m_NumWrittenHook; // full message with log domain, severity + newline
    int             m_NumWrittenListener; // just the message part
    dmMutex::HMutex m_Mutex;
} g_CustomLogContext;

static void CustomLogHook(void* user_data, const char* s)
{
    CustomLogContext* ctx = (CustomLogContext*)user_data;

    bool result = dmMutex::TryLock(ctx->m_Mutex);
    assert(result && "The lock was already taken for log hook");
    if (!result)
        return;
    ctx->m_NumWrittenHook += strlen(s);
    dmMutex::Unlock(ctx->m_Mutex);
}

static void CustomLogListener(LogSeverity severity, const char* domain, const char* formatted_string)
{
    CustomLogContext* ctx = &g_CustomLogContext;

    bool result = dmMutex::TryLock(ctx->m_Mutex);
    assert(result && "The lock was already taken for log listener");
    if (!result)
        return;
    ctx->m_NumWrittenListener += strlen(formatted_string);
    dmMutex::Unlock(ctx->m_Mutex);
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
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);

    CustomLogContext& log_ctx = g_CustomLogContext;
    memset(&log_ctx, 0, sizeof(log_ctx));
    log_ctx.m_Mutex = dmMutex::New();
    dmLog::SetCustomLogCallback(CustomLogHook, &log_ctx);

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

    dmLogUnregisterListener(CustomLogListener);

    dmLog::SetCustomLogCallback(0, 0);
    dmMutex::Delete(log_ctx.m_Mutex);

    dmLogInfo("Regular output reenabled.");
    dmLogInfo("  Custom hook printed %d characters", log_ctx.m_NumWrittenHook);
    dmLogInfo("  Custom log listener printed %d characters", log_ctx.m_NumWrittenListener);

    // Size of each message should be 13: 'INFO:DLIB: %d\n'
    int expected_count_hook = 13 * num_threads * loop_count;
    ASSERT_EQ(expected_count_hook, log_ctx.m_NumWrittenHook);

    int expected_count_listener = 13 * num_threads * loop_count;
    ASSERT_EQ(expected_count_listener, log_ctx.m_NumWrittenListener);

    dmLog::LogFinalize();
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

static void TestLogCaptureCallback(void* user_data, const char* log)
{
    dmArray<char>* log_output = (dmArray<char>*)user_data;
    uint32_t len = (uint32_t)strlen(log);
    log_output->SetCapacity(log_output->Size() + len + 1);
    log_output->PushArray(log, len);
}

TEST(dmLog, TestCapture)
{
    dmArray<char> log_output;
    dmLog::SetCustomLogCallback(TestLogCaptureCallback, &log_output);
    dmLogDebug("This is a debug message");
    dmLogInfo("This is a info message");
    dmLogWarning("This is a warning message");
    dmLogError("This is a error message");
    dmLogFatal("This is a fata message");

    log_output.Push(0);

    const char* ExpectedOutput =
                "INFO:DLIB: This is a info message\n"
                "WARNING:DLIB: This is a warning message\n"
                "ERROR:DLIB: This is a error message\n"
                "FATAL:DLIB: This is a fata message\n";

    ASSERT_STREQ(ExpectedOutput,
                log_output.Begin());
    dmLog::SetCustomLogCallback(0x0, 0x0);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}

