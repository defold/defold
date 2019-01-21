#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>
#include "testutil.h"
#include "../dlib/array.h"
#include "../dlib/hash.h"
#include "../dlib/log.h"
#include "../dlib/dstrings.h"
#include "../dlib/socket.h"
#include "../dlib/thread.h"
#include "../dlib/time.h"
#include "../dlib/path.h"
#include "../dlib/sys.h"
#include "../dlib/windefines.h"

TEST(dmLog, Init)
{
    dmLogParams params;
    dmLogInitialize(&params);
    dmLogFinalize();
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

#if !defined(DM_NO_PYTHON)
TEST(dmLog, Client)
{
    char buf[256];
    dmLogParams params;
    dmLogInitialize(&params);
    uint16_t port = dmLogGetPort();
    ASSERT_GT(port, 0);
    DM_SNPRINTF(buf, sizeof(buf), "python src/test/test_log.py %d", port);
#ifdef _WIN32
    FILE* f = popen(buf, "rb");
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

    pclose(f);
    dmThread::Join(log_thread);
    dmLogFinalize();
}
#endif

TEST(dmLog, LogFile)
{
    char path[DMPATH_MAX_PATH];
    dmSys::GetLogPath(path, sizeof(path));
    dmStrlCat(path, "/log.txt", sizeof(path));

    dmLogParams params;
    dmLogInitialize(&params);
    dmSetLogFile(path);
    dmLogInfo("TESTING_LOG");
    dmLogFinalize();

    char tmp[1024];
    FILE* f = fopen(path, "rb");
    ASSERT_NE((FILE*) 0, f);
    fread(tmp, 1, sizeof(tmp), f);
    ASSERT_TRUE(strstr(tmp, "TESTING_LOG") != 0);
    dmSys::Unlink(path);
    fclose(f);
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
    dmSetCustomLogCallback(TestLogCaptureCallback, &log_output);
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
    dmSetCustomLogCallback(0x0, 0x0);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}

