#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <gtest/gtest.h>
#include "../dlib/hash.h"
#include "../dlib/log.h"
#include "../dlib/dstrings.h"
#include "../dlib/thread.h"
#include "../dlib/time.h"

TEST(dmLog, Init)
{
    dmLogInitialize();
    dmLogFinalize();
}

static void LogThread(void* arg)
{
    dmLogWarning("a warning %d", 123);
    dmLogError("an error %d", 456);
}

TEST(dmLog, Client)
{
    char buf[256];
    dmLogInitialize();
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
    dmThread::Thread log_thread = dmThread::New(LogThread, 0x80000, 0);

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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

