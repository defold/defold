#include <gtest/gtest.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include "../liveupdate.h"
#include "../liveupdate_private.h"


static volatile bool g_TestAsyncCallbackComplete = false;
static dmResource::HFactory g_ResourceFactory = 0x0;

class LiveUpdate : public ::testing::Test
{
public:
    virtual void SetUp()
    {
        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_RELOAD_SUPPORT;
        g_ResourceFactory = dmResource::NewFactory(&params, ".");
        ASSERT_NE((void*) 0, g_ResourceFactory);
    }
    virtual void TearDown()
    {
        if (g_ResourceFactory != NULL)
        {
            dmResource::DeleteFactory(g_ResourceFactory);
        }
    }
};


namespace dmLiveUpdate
{
    dmLiveUpdate::Result NewArchiveIndexWithResource(dmResource::Manifest* manifest, const char* expected_digest, const uint32_t expected_digest_length, const dmResourceArchive::LiveUpdateResource* resource, dmResourceArchive::HArchiveIndex& out_new_index)
    {
        out_new_index = (dmResourceArchive::HArchiveIndex) 0x5678;
        assert(manifest->m_ArchiveIndex == (dmResourceArchive::HArchiveIndexContainer) 0x1234);
        assert(strcmp("DUMMY2", expected_digest)==0);
        assert(expected_digest_length == 6);
        assert(*((uint32_t*)resource->m_Data) == 0xdeadbeef);
        return dmLiveUpdate::RESULT_OK;
    }

    void SetNewArchiveIndex(dmResourceArchive::HArchiveIndexContainer archive_container, dmResourceArchive::HArchiveIndex new_index, bool mem_mapped)
    {
        ASSERT_EQ((dmResourceArchive::HArchiveIndexContainer) 0x1234, archive_container);
        ASSERT_EQ((dmResourceArchive::HArchiveIndex) 0x5678, new_index);
        ASSERT_TRUE(mem_mapped);
    }
}

static void Callback_StoreResource(dmLiveUpdate::StoreResourceCallbackData* callback_data)
{
    g_TestAsyncCallbackComplete = true;
    ASSERT_EQ(1, callback_data->m_Callback);
    ASSERT_EQ(2, callback_data->m_ResourceRef);
    ASSERT_EQ(3, callback_data->m_HexDigestRef);
    ASSERT_EQ(4, callback_data->m_Self);
    ASSERT_STREQ("DUMMY1", callback_data->m_HexDigest);
    ASSERT_TRUE(callback_data->m_Status);
}

static void Callback_StoreResourceInvalidHeader(dmLiveUpdate::StoreResourceCallbackData* callback_data)
{
    g_TestAsyncCallbackComplete = true;
    ASSERT_EQ(1, callback_data->m_Callback);
    ASSERT_EQ(2, callback_data->m_ResourceRef);
    ASSERT_EQ(3, callback_data->m_HexDigestRef);
    ASSERT_EQ(4, callback_data->m_Self);
    ASSERT_STREQ("DUMMY1", callback_data->m_HexDigest);
    ASSERT_FALSE(callback_data->m_Status);
}

TEST_F(LiveUpdate, TestAsync)
{
    dmLiveUpdate::AsyncInitialize(g_ResourceFactory);

    uint8_t buf[sizeof(dmResourceArchive::LiveUpdateResourceHeader)+sizeof(uint32_t)];
    const size_t buf_len = sizeof(buf);
    *((uint32_t*)&buf[sizeof(dmResourceArchive::LiveUpdateResourceHeader)]) = 0xdeadbeef;
    dmResourceArchive::LiveUpdateResource resource((const uint8_t*) buf, buf_len);

    dmLiveUpdate::StoreResourceCallbackData cb;
    cb.m_Callback = 1;
    cb.m_ResourceRef = 2;
    cb.m_HexDigestRef = 3;
    cb.m_Self = 4;
    cb.m_HexDigest = "DUMMY1";;

    dmResource::Manifest manifest;
    manifest.m_ArchiveIndex = (dmResourceArchive::HArchiveIndexContainer) 0x1234;

    dmLiveUpdate::AsyncResourceRequest request;
    request.m_Manifest = &manifest;
    request.m_ExpectedResourceDigestLength = 6;
    request.m_ExpectedResourceDigest = "DUMMY2";
    request.m_Resource.Set(resource);
    request.m_CallbackData = cb;
    request.m_Callback = Callback_StoreResource;

    ASSERT_TRUE(dmLiveUpdate::AddAsyncResourceRequest(request));
    while(!g_TestAsyncCallbackComplete)
        dmLiveUpdate::AsyncUpdate();

    dmLiveUpdate::AsyncFinalize();
}

TEST_F(LiveUpdate, TestAsyncInvalidResource)
{
    dmLiveUpdate::AsyncInitialize(g_ResourceFactory);

    uint8_t buf[sizeof(dmResourceArchive::LiveUpdateResourceHeader)+sizeof(uint32_t)];
    const size_t buf_len = sizeof(buf);
    *((uint32_t*)&buf[sizeof(dmResourceArchive::LiveUpdateResourceHeader)]) = 0xdeadbeef;
    dmResourceArchive::LiveUpdateResource resource((const uint8_t*) buf, buf_len);

    resource.m_Header = 0x0;

    dmLiveUpdate::StoreResourceCallbackData cb;
    cb.m_Callback = 1;
    cb.m_ResourceRef = 2;
    cb.m_HexDigestRef = 3;
    cb.m_Self = 4;
    cb.m_HexDigest = "DUMMY1";;

    dmResource::Manifest manifest;
    manifest.m_ArchiveIndex = (dmResourceArchive::HArchiveIndexContainer) 0x1234;

    dmLiveUpdate::AsyncResourceRequest request;
    request.m_Manifest = &manifest;
    request.m_ExpectedResourceDigestLength = 6;
    request.m_ExpectedResourceDigest = "DUMMY2";
    request.m_Resource.Set(resource);
    request.m_CallbackData = cb;
    request.m_Callback = Callback_StoreResourceInvalidHeader;

    ASSERT_TRUE(dmLiveUpdate::AddAsyncResourceRequest(request));
    while(!g_TestAsyncCallbackComplete)
        dmLiveUpdate::AsyncUpdate();

    dmLiveUpdate::AsyncFinalize();
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}
