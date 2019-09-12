#include "../dlib/thread.h"
#include "../dlib/time.h"
#include "../dlib/dns.h"
#include "../dlib/dstrings.h"
#include "../dlib/socket.h"
#include "../dlib/network_constants.h"

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

template <> char* jc_test_print_value(char* buffer, size_t buffer_len, dmDNS::Result r) {
    return buffer + DM_SNPRINTF(buffer, buffer_len, "%s", dmDNS::ResultToString(r));
}


void WaitForBool(bool* lock)
{
    const uint32_t maximum_wait = 5000; // milliseconds
    const uint32_t wait_timeout = 100;  // milliseconds
    uint32_t wait_count = 0;
    for (uint32_t i = 0; i < (maximum_wait / wait_timeout); ++i)
    {
        if (!(*lock))
        {
            wait_count += 1;
            dmTime::Sleep(wait_timeout * 1000); // nanoseconds
        }
    }

    printf("Waited for %d/%d counts (%d ms each)\n", wait_count, (maximum_wait / wait_timeout), wait_timeout);
    ASSERT_TRUE(*lock);
    dmTime::Sleep(5 * wait_timeout * 1000); // nanoseconds
}

class dmDNSTest: public jc_test_base_class
{
public:
    dmDNS::HChannel channel;
    virtual void SetUp()
    {
        dmDNS::Result result_channel = dmDNS::NewChannel(&channel);
        ASSERT_EQ(dmDNS::RESULT_OK, result_channel);
    }

    virtual void TearDown()
    {
        dmDNS::DeleteChannel(channel);
    }
};

/////////////////////////////////////////////////////////////
// These tests are exact same as test_socket.cpp : 818 - 900
TEST_F(dmDNSTest, GetHostByName_IPv4_Localhost)
{
    dmSocket::Address address;
    const char* hostname = DM_LOOPBACK_ADDRESS_IPV4;
    dmDNS::Result result = dmDNS::GetHostByName(hostname, &address, channel, true, false);
    ASSERT_EQ(dmDNS::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV4, address.m_family);
    ASSERT_EQ(0x0100007f, *dmSocket::IPv4(&address));
}

TEST_F(dmDNSTest, GetHostByName_IPv6_Localhost)
{
    dmSocket::Address address;
    const char* hostname = DM_LOOPBACK_ADDRESS_IPV6;
    dmDNS::Result result = dmDNS::GetHostByName(hostname, &address, channel, false, true);
    ASSERT_EQ(dmDNS::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV6, address.m_family);
    ASSERT_EQ(0x00000000, address.m_address[0]);
    ASSERT_EQ(0x00000000, address.m_address[1]);
    ASSERT_EQ(0x00000000, address.m_address[2]);
    ASSERT_EQ(0x01000000, address.m_address[3]);
}

TEST_F(dmDNSTest, GetHostByName_IPv6_External)
{
#if !defined(_WIN32)
    dmSocket::Address address;
    const char* hostname = "ipv6-test.com";
    dmDNS::Result result = dmDNS::GetHostByName(hostname, &address, channel, false, true);
    ASSERT_EQ(dmDNS::RESULT_OK, result);
    ASSERT_EQ(dmSocket::DOMAIN_IPV6, address.m_family);
#else
    printf("[   INFO   ] Test for DNS/GetHostByName/IPv6 is disabled on Windows.\n");
    ASSERT_TRUE(true);
#endif
}

TEST_F(dmDNSTest, GetHostByName_IPv4_Unavailable)
{
    dmSocket::Address address;
    const char* hostname = "localhost.invalid";
    dmDNS::Result result = dmDNS::GetHostByName(hostname, &address, channel, true, false);
    ASSERT_EQ(dmDNS::RESULT_HOST_NOT_FOUND, result);
}

TEST_F(dmDNSTest, GetHostByName_IPv6_Unavailable)
{
    dmSocket::Address address;
    const char* hostname = "localhost.invalid";
    dmDNS::Result result = dmDNS::GetHostByName(hostname, &address, channel, false, true);
    ASSERT_EQ(dmDNS::RESULT_HOST_NOT_FOUND, result);
}

TEST_F(dmDNSTest, GetHostByName_NoValidAddressFamily)
{
    dmSocket::Address address;
    const char* hostname = "localhost";
    dmDNS::Result result = dmDNS::GetHostByName(hostname, &address, channel, false, false);
    ASSERT_EQ(dmDNS::RESULT_HOST_NOT_FOUND, result);
}

/////////////////////////////////////////////////////////////

struct ThreadRunnerContext
{
    dmDNS::HChannel channel;
    bool started;

    struct
    {
        const char*   hostname;
        dmDNS::Result result;
    } request;
};

static void ThreadRunner(void* arg)
{
    dmSocket::Address address;
    ThreadRunnerContext* ctx = (ThreadRunnerContext*) arg;

    ctx->started = true;

    uint64_t time_start    = dmTime::GetTime();
    const uint64_t timeout = 5000 * 1000; // 5s timeout

    while(ctx->request.result != dmDNS::RESULT_CANCELLED)
    {
        if ((dmTime::GetTime() - time_start) > timeout)
        {
            ASSERT_TRUE(false);
        }

        ctx->request.result = dmDNS::GetHostByName(ctx->request.hostname, &address, ctx->channel, true, true);
    }
}

TEST(DNS, GetHostByName_Threaded_Cancel)
{
    ThreadRunnerContext ctx;
    ctx.started          = false;
    ctx.request.hostname = "localhost";

    dmDNS::Result result_channel = dmDNS::NewChannel(&ctx.channel);
    ASSERT_EQ(dmDNS::RESULT_OK, result_channel);

    dmThread::Thread thread = dmThread::New(&ThreadRunner, 0x80000, (void *) &ctx, "runner");
    WaitForBool(&ctx.started);

    dmDNS::StopChannel(ctx.channel);
    dmThread::Join(thread);

    ASSERT_EQ(dmDNS::RESULT_CANCELLED, ctx.request.result);
    dmDNS::DeleteChannel(ctx.channel);
}

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    dmDNS::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmDNS::Finalize();
    dmSocket::Finalize();
    return ret;
}
