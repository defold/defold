#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <gtest/gtest.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"
#include "../dlib/dstrings.h"
#include "../dlib/socket.h"
#include "../dlib/thread.h"
#include "../dlib/time.h"
#include "../dlib/path.h"
#include "../dlib/sys.h"

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

TEST(dmLog, Client)
{
    char buf[256];
    dmLogParams params;
    dmLogInitialize(&params);
    uint16_t port = dmLogGetPort();
    ASSERT_GT(port, 0);
    DM_SNPRINTF(buf, sizeof(buf), "python src/test/test_log.py %d", port);
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
    dmLogFinalize();
}

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

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    dmSocket::Finalize();
    return ret;
}

