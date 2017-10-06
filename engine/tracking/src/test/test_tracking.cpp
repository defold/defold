#include <stdlib.h>
#include <gtest/gtest.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <script/script.h>

#include "../tracking.h"
#include "../tracking_private.h"

#include "../proto/tracking/tracking_ddf.h"

extern "C"
{
    #include <lua/lauxlib.h>
    #include <lua/lualib.h>
}

extern unsigned char TEST_TRACKING_LUA[];
extern uint32_t TEST_TRACKING_LUA_SIZE;

/*
    Tracking system tests. These tests make use of test_tracking.lua which hooks functions
    for http requests, save and load, and provides assert functions to assert things about
    if the tracking system has made requests, what they contain etc. It also has functions to
    respond to http requests made.

    That way the whole outside world is faked, including disk saves and loads that do not
    successfully complete.

    With no errors, this is the normal flow.

    0. The tracking system is started in a new installation
    1. A meta data file is saved to disk
    2. A http request is made to the url provided in the config (or default)
        - This request returns a json with urls for where to get STID:s and
          where to post events.
    3. A http request is made to the STID getting url from the configuration
    4. The returned STID is saved to disk in the new meta file
    5. An @Install event is posted to the event url with the same STID

    When a session resumes from a previous state;

    1. Meta data is loaded from disk
    2. Event tables (if any are used) are also loaded
    3. Request configuration
    4. Start posting old events again (and with same message-id as before)

    Some important rules:
        STID should never change in a session
        The same event must never be posted with a different message id again
        @Install event shall only be generated when starting with a fresh start
*/

class dmTrackingTest : public ::testing::Test
{
    public:
        virtual void SetUp()
        {
            const char *config = "[tracking]\nurl=http://example.org/config\n[tracking]\napp_id=app123\n";
            dmConfigFile::Result r = dmConfigFile::LoadFromBuffer(config, strlen(config), 0, 0, &m_Config);
            ASSERT_EQ(dmConfigFile::RESULT_OK, r);

            m_Tracking = dmTracking::New(m_Config);
            m_ScriptContext = dmTracking::GetScriptContext(m_Tracking);
            m_LuaState = dmScript::GetLuaState(m_ScriptContext);

            // hook in the the test lua too
            // this overrides http.request and sys.save and save.load so we can
            // mess around with files in memory and examine requests here in the test.
            luaL_loadbuffer(m_LuaState, (const char*)TEST_TRACKING_LUA, TEST_TRACKING_LUA_SIZE, "test");
            ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, LUA_MULTRET));

            lua_getglobal(m_LuaState, "test_setup_hooks");
            ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
        }

        // Finalizing tracking forces a save, which is not good
        // if we want to emulate abrupt shutdown, so make it optional.
        virtual void DoTearDown(bool finalize_tracking)
        {
            if (finalize_tracking)
            {
                dmTracking::Finalize(m_Tracking);
            }
            dmTracking::Delete(m_Tracking);
            dmConfigFile::Delete(m_Config);
        }

        virtual void TearDown()
        {
            DoTearDown(true);
        }

        // Helper that starts tracking and responds to the first HTTP request that asks
        // for all the URLs.
        void StartAndProvideConfig()
        {
            dmTracking::Start(m_Tracking, "Defold", "unit-test");
            dmTracking::Update(m_Tracking, 0.0f);
            // there will be a request made to the config_url
            lua_getglobal(m_LuaState, "test_respond_with_config");
            lua_pushnumber(m_LuaState, 200.0f);
            lua_pushstring(m_LuaState, "{ \"stid_url\": \"http://get-stid/\", \"event_url\": \"http://event-url/\" }");
            ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));
        }

        // Helper to do the startup and respond with a STID too
        void StartAndProvideConfigAndStid(const char *stid)
        {
            StartAndProvideConfig();
            dmTracking::Update(m_Tracking, 0.0f);

            lua_getglobal(m_LuaState, "test_respond_with_stid");
            lua_pushnumber(m_LuaState, 200.0f);
            lua_pushstring(m_LuaState, stid);
            ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));

            // it will now send install
            dmTracking::Update(m_Tracking, 0.0f);
            lua_getglobal(m_LuaState, "test_respond_with_blank");
            lua_pushnumber(m_LuaState, 200.0f);
            ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

            lua_getglobal(m_LuaState, "test_assert_has_no_request");
            ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
        }

        // keeps the files generated, starts the tracking system again with supplied params.
        void DoNewSession()
        {
            // The test lua helper stores all the saved files in __saves
            char buf[65536];
            lua_getglobal(m_LuaState, "__saves");
            ASSERT_NE(0, dmScript::CheckTable(m_LuaState, buf, 65536, 1));

            DoTearDown(false);
            SetUp();

            dmScript::PushTable(m_LuaState, buf);
            lua_setglobal(m_LuaState, "__saves");

            StartAndProvideConfig();
            dmTracking::Update(m_Tracking, 1.0f);
        }

        dmConfigFile::HConfig m_Config;
        dmTracking::HContext m_Tracking;
        dmScript::HContext m_ScriptContext;
        lua_State* m_LuaState;
};

TEST_F(dmTrackingTest, Initialize)
{

}

TEST_F(dmTrackingTest, TestGetConfigUrl)
{
    dmTracking::Start(m_Tracking, "Defold", "unit-test");
    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_assert_has_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
    lua_getglobal(m_LuaState, "test_assert_request_url");
    lua_pushstring(m_LuaState, "http://example.org/config");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

TEST_F(dmTrackingTest, TestGetConfigFailing)
{
    // Test that the code patiently wait with requests (and only makes one new)
    // when making a tracking request that fails, both as in total failure and when
    // the server responds with 500
    dmTracking::Start(m_Tracking, "Defold", "unit-test");

    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_respond_with_failure");
    lua_pushnumber(m_LuaState, 0);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    dmTracking::Update(m_Tracking, 10.0f);

    lua_getglobal(m_LuaState, "test_respond_with_failure");
    lua_pushnumber(m_LuaState, 500.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    dmTracking::Update(m_Tracking, 9.0f);

    lua_getglobal(m_LuaState, "test_respond_with_config");
    lua_pushnumber(m_LuaState, 200.0f);
    lua_pushstring(m_LuaState, "{}");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));
    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
}

TEST_F(dmTrackingTest, TestInstallEvent)
{
    const char *TEST_STID = "abcdefghijklmnopqrstuvwxyz123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ==1234-this-is-test";
    StartAndProvideConfig();
    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_respond_with_stid");
    lua_pushnumber(m_LuaState, 200.0f);
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));

    dmTracking::Update(m_Tracking, 0.0f);

    // correct stid
    lua_getglobal(m_LuaState, "test_assert_request_stid");
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // correct event
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "@Install");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    lua_getglobal(m_LuaState, "test_respond_with_blank");
    lua_pushnumber(m_LuaState, 200.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    for (uint32_t i=0;i<20;i++)
    {
        dmTracking::Update(m_Tracking, 10.0f);
        // correct event
        lua_getglobal(m_LuaState, "test_assert_request_has_no_event");
        lua_pushstring(m_LuaState, "@Install");
        ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    }
}

TEST_F(dmTrackingTest, TestInstallEventSaveFail)
{
    const char *TEST_STID = "testttest";
    StartAndProvideConfig();
    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_respond_with_stid");
    lua_pushnumber(m_LuaState, 200.0f);
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));

    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "@Install");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // @Install has been sent, but now fail the save.
    lua_getglobal(m_LuaState, "test_setup_save_fail");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    lua_getglobal(m_LuaState, "test_respond_with_failure");
    lua_pushnumber(m_LuaState, 0);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    dmTracking::Update(m_Tracking, 10.0f);
    dmTracking::Update(m_Tracking, 10.0f);

    DoNewSession();

    // In the next session it should not generate any new install event
    // and it should not send any new one
    dmTracking::Update(m_Tracking, 10.0f);
    dmTracking::Update(m_Tracking, 10.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_no_event");
    lua_pushstring(m_LuaState, "@Install");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

TEST_F(dmTrackingTest, TestSafeSaveFail)
{
    // Test save fail when there is nothing that needs to be saved; should still
    // send events
    const char *TEST_STID = "install";
    StartAndProvideConfig();
    dmTracking::Update(m_Tracking, 0.0f);

    // send stid
    lua_getglobal(m_LuaState, "test_respond_with_stid");
    lua_pushnumber(m_LuaState, 200.0f);
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));

    dmTracking::Update(m_Tracking, 0.0f);

    // send response to install
    lua_getglobal(m_LuaState, "test_respond_with_blank");
    lua_pushnumber(m_LuaState, 200.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    dmTracking::Update(m_Tracking, 10.0f);

    DoNewSession();

    dmTracking::Update(m_Tracking, 10.0f);

    // now disk is full
    lua_getglobal(m_LuaState, "test_setup_save_fail");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    dmTracking::Update(m_Tracking, 30.0f);

    // event test
    const char *type = "evt-1";
    dmTracking::PostSimpleEvent(m_Tracking, type);
    dmTracking::Update(m_Tracking, 10.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, type);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

TEST_F(dmTrackingTest, TestLoadFail)
{
    // Any load failing should also mean the tracking shuts down
    lua_getglobal(m_LuaState, "test_setup_load_fail");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    dmTracking::Start(m_Tracking, "Defold", "unit-test");

    dmTracking::Update(m_Tracking, 10.0f);
    dmTracking::Update(m_Tracking, 10.0f);

    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
}

TEST_F(dmTrackingTest, TestPersist)
{
    const char *TEST_STID = "remember-me-STID";

    StartAndProvideConfig();
    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_respond_with_stid");
    lua_pushnumber(m_LuaState, 200.0f);
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));

    dmTracking::Update(m_Tracking, 0.0f);

    // correct stid
    lua_getglobal(m_LuaState, "test_assert_request_stid");
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // now fail on install
    lua_getglobal(m_LuaState, "test_respond_with_failure");
    lua_pushnumber(m_LuaState, 0);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    dmTracking::Update(m_Tracking, 0.33f);

    // New session
    DoNewSession();

    // Now we should see the install event being sent again with the same
    // stid provided from before
    lua_getglobal(m_LuaState, "test_assert_request_stid");
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "@Install");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

TEST_F(dmTrackingTest, TestPersistStid)
{
    const char *TEST_STID = "remember-me-STID";

    StartAndProvideConfig();
    dmTracking::Update(m_Tracking, 0.0f);

    lua_getglobal(m_LuaState, "test_respond_with_stid");
    lua_pushnumber(m_LuaState, 200.0f);
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));

    dmTracking::Update(m_Tracking, 0.0f);

    // correct stid
    lua_getglobal(m_LuaState, "test_assert_request_stid");
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // now fail on install
    lua_getglobal(m_LuaState, "test_respond_with_blank");
    lua_pushnumber(m_LuaState, 200.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    dmTracking::Update(m_Tracking, 0.33f);

    // New session
    DoNewSession();

    dmTracking::PostSimpleEvent(m_Tracking, "@Invoke");

    dmTracking::Update(m_Tracking, 0.33f);

    // Now we should see the install event being sent again with the same
    // stid provided from before
    lua_getglobal(m_LuaState, "test_assert_request_stid");
    lua_pushstring(m_LuaState, TEST_STID);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // correct event
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "@Invoke");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

TEST_F(dmTrackingTest, TestDDFMessage)
{
    StartAndProvideConfigAndStid("stid-1234");

    const char *type = "evt-1";
    uint32_t sz = sizeof(dmTrackingDDF::TrackingEvent) + strlen(type) + 1;
    dmTrackingDDF::TrackingEvent* evt = (dmTrackingDDF::TrackingEvent*) malloc(sz);
    memset(evt, 0x00, sizeof(dmTrackingDDF::TrackingEvent));
    evt->m_Type = (const char*) sizeof(dmTrackingDDF::TrackingEvent);
    strcpy((char*)&evt[1], type);

    dmMessage::URL url;
    url.m_Socket = dmTracking::GetSocket(m_Tracking);
    dmMessage::Post(0, &url, dmTrackingDDF::TrackingEvent::m_DDFHash, 0, (uintptr_t) dmTrackingDDF::TrackingEvent::m_DDFDescriptor, evt, sz, 0);
    dmTracking::Update(m_Tracking, 1.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, type);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    free(evt);
}

TEST_F(dmTrackingTest, TestAttribute)
{
    StartAndProvideConfigAndStid("stid-1234");

    const char *type = "evt-1";
    const char *attr_key = "test-attr";
    const char *attr_value = "testvalue";

    uintptr_t ofs[6];
    ofs[0] = 0;
    ofs[1] = sizeof(dmTrackingDDF::TrackingEvent);
    ofs[2] = ofs[1] + sizeof(dmTrackingDDF::TrackingAttribute);
    ofs[3] = ofs[2] + strlen(type) + 1;
    ofs[4] = ofs[3] + strlen(attr_key) + 1;
    ofs[5] = ofs[4] + strlen(attr_value) + 1; // size

    char *msg = (char*) malloc(ofs[5]);
    memset(msg, 0x00, ofs[5]);

    dmTrackingDDF::TrackingEvent* evt = (dmTrackingDDF::TrackingEvent*) msg;
    evt->m_Attributes.m_Count = 1;
    evt->m_Attributes.m_Data = (dmTrackingDDF::TrackingAttribute*) ofs[1];
    evt->m_Type = (const char*) ofs[2];

    dmTrackingDDF::TrackingAttribute* attr = (dmTrackingDDF::TrackingAttribute*) (msg + ofs[1]);
    attr->m_Key = (const char*) ofs[3];
    attr->m_Value = (const char*) ofs[4];

    strcpy(msg + ofs[2], type);
    strcpy(msg + ofs[3], attr_key);
    strcpy(msg + ofs[4], attr_value);

    dmMessage::URL url;
    url.m_Socket = dmTracking::GetSocket(m_Tracking);
    dmMessage::Post(0, &url, dmTrackingDDF::TrackingEvent::m_DDFHash, 0, (uintptr_t) dmTrackingDDF::TrackingEvent::m_DDFDescriptor, evt, ofs[5], 0);
    dmTracking::Update(m_Tracking, 1.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_attribute");
    lua_pushstring(m_LuaState, attr_key);
    lua_pushstring(m_LuaState, attr_value);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));
    free(msg);
}

TEST_F(dmTrackingTest, TestMetric)
{
    StartAndProvideConfigAndStid("stid-1234");

    const char *type = "evt-1";
    const char *metric_key = "test-metric";
    float metric_value = 1234.56f;

    uintptr_t ofs[5];
    ofs[0] = 0;
    ofs[1] = sizeof(dmTrackingDDF::TrackingEvent);
    ofs[2] = ofs[1] + sizeof(dmTrackingDDF::TrackingMetric);
    ofs[3] = ofs[2] + strlen(type) + 1;
    ofs[4] = ofs[3] + strlen(metric_key) + 1;

    char *msg = (char*) malloc(ofs[4]);
    memset(msg, 0x00, ofs[4]);

    dmTrackingDDF::TrackingEvent* evt = (dmTrackingDDF::TrackingEvent*) msg;
    evt->m_Metrics.m_Count = 1;
    evt->m_Metrics.m_Data = (dmTrackingDDF::TrackingMetric*) ofs[1];
    evt->m_Type = (const char*) ofs[2];

    dmTrackingDDF::TrackingMetric* metric = (dmTrackingDDF::TrackingMetric*) (msg + ofs[1]);
    metric->m_Key = (const char*) ofs[3];
    metric->m_Value = metric_value;

    strcpy(msg + ofs[2], type);
    strcpy(msg + ofs[3], metric_key);

    dmMessage::URL url;
    url.m_Socket = dmTracking::GetSocket(m_Tracking);
    dmMessage::Post(0, &url, dmTrackingDDF::TrackingEvent::m_DDFHash, 0, (uintptr_t) dmTrackingDDF::TrackingEvent::m_DDFDescriptor, evt, ofs[4], 0);
    dmTracking::Update(m_Tracking, 1.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_metric");
    lua_pushstring(m_LuaState, metric_key);
    lua_pushnumber(m_LuaState, metric_value);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 2, 0));
    free(msg);
}

TEST_F(dmTrackingTest, TestSimpleEvent)
{
    StartAndProvideConfigAndStid("stid-1234");

    const char *type = "evt-1";

    dmTracking::PostSimpleEvent(m_Tracking, type);
    dmTracking::Update(m_Tracking, 0.1f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, type);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

#if !defined(__linux__) && !defined(__x86_64__)  // Reenable when we can run LuaJIT on Linux 64 bit again
TEST_F(dmTrackingTest, TestEscaping)
{
    StartAndProvideConfigAndStid("stid-1234");

    const char *type = "\n\t\b\rText\n\n!\"Quotes\"";

    dmTracking::PostSimpleEvent(m_Tracking, type);
    dmTracking::Update(m_Tracking, 0.1f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, type);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}
#endif

TEST_F(dmTrackingTest, TestEventBatching)
{
    StartAndProvideConfigAndStid("stid-1234");

    for (int i=0;i<10;i++)
    {
        dmTracking::PostSimpleEvent(m_Tracking, "generic");
    }

    dmTracking::Update(m_Tracking, 1.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "generic");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    lua_getglobal(m_LuaState, "test_respond_with_blank");
    lua_pushnumber(m_LuaState, 200.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    for (int j=0;j<50;j++)
    {
        dmTracking::Update(m_Tracking, 0.03f);
    }

    // should not retransmit
    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
}

TEST_F(dmTrackingTest, TestEventResend)
{
    StartAndProvideConfigAndStid("stid-9876");

    for (int i=0;i<10;i++)
    {
        dmTracking::PostSimpleEvent(m_Tracking, "generic");
    }

    dmTracking::Update(m_Tracking, 1.0f);

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "generic");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    lua_getglobal(m_LuaState, "test_respond_with_failure");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    for (int j=0;j<10;j++)
    {
        dmTracking::Update(m_Tracking, 1.0f);
    }

    // now just shut down & reload
    DoNewSession();

    dmTracking::Update(m_Tracking, 1.0f);

    // should have retransmit (once)
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "generic");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // also only one request
    lua_getglobal(m_LuaState, "test_respond_with_blank");
    lua_pushnumber(m_LuaState, 200.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
}

TEST_F(dmTrackingTest, TestEventSpam)
{
    StartAndProvideConfigAndStid("test");

    for (int k=0;k<10;k++)
    {
        for (int i=0;i<4000;i++)
        {
            dmTracking::PostSimpleEvent(m_Tracking, "generic");
        }

        dmTracking::Update(m_Tracking, 1.0f);
    }

    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "generic");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    lua_getglobal(m_LuaState, "test_respond_with_failure");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    lua_getglobal(m_LuaState, "test_assert_has_no_request");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
}

TEST_F(dmTrackingTest, TestManyEventsNoSave)
{
    StartAndProvideConfigAndStid("test");

    for (int k=0;k<50;k++)
    {
        for (int i=0;i<4;i++)
        {
            dmTracking::PostSimpleEvent(m_Tracking, "generic");
        }

        dmTracking::Update(m_Tracking, 1.0f);

        lua_getglobal(m_LuaState, "test_respond_with_blank");
        lua_pushnumber(m_LuaState, 200.0f);
        ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
        lua_getglobal(m_LuaState, "test_assert_has_no_request");
        ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));
        dmTracking::Update(m_Tracking, 1.0f);
    }

    // 2 saves this far. one for the first test write and then when
    // stid is acquired. no more since everything succeeds
    lua_getglobal(m_LuaState, "test_assert_max_saves");
    lua_pushnumber(m_LuaState, 2.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    dmTracking::Update(m_Tracking, 1.0f);
}

TEST_F(dmTrackingTest, TestManySessions)
{
    StartAndProvideConfigAndStid("stid-9876");

    for (int i=0;i<10;i++)
    {
        dmTracking::PostSimpleEvent(m_Tracking, "generic");
    }

    dmTracking::Update(m_Tracking, 1.0f);

    lua_getglobal(m_LuaState, "test_respond_with_failure");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

    for (int j=0;j<10;j++)
    {
       DoNewSession();

        // post one session-<i> event each session
        char buf[64];
        sprintf(buf, "session-%d", j);
        dmTracking::PostSimpleEvent(m_Tracking, buf);

        for (int k=0;k<50;k++)
        {
            if (k == 5)
            {
                // it will try to send so fail
                lua_getglobal(m_LuaState, "test_assert_has_request");
                ASSERT_EQ(0, dmScript::PCall(m_LuaState, 0, 0));

                lua_getglobal(m_LuaState, "test_respond_with_failure");
                lua_pushnumber(m_LuaState, 0);
                ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
            }
            dmTracking::Update(m_Tracking, 1.0f);
        }
    }

    // now just shut down & reload
    DoNewSession();

    dmTracking::Update(m_Tracking, 1.0f);

    // answer the request with generic.  the other ones should not be batched
    // with the first since it is locked with message id
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "generic");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    lua_getglobal(m_LuaState, "test_respond_with_blank");
    lua_pushnumber(m_LuaState, 200.0f);
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));

    // see that the rest get sent now
    dmTracking::Update(m_Tracking, 1.0f);
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "session-0");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "session-1");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
    lua_getglobal(m_LuaState, "test_assert_request_has_event");
    lua_pushstring(m_LuaState, "session-2");
    ASSERT_EQ(0, dmScript::PCall(m_LuaState, 1, 0));
}

class dmTrackingNoHookTest : public ::testing::Test
{
    public:
        virtual void SetUp()
        {
            const char *config = "\n";
            dmConfigFile::Result r = dmConfigFile::LoadFromBuffer(config, strlen(config), 0, 0, &m_Config);
            ASSERT_EQ(dmConfigFile::RESULT_OK, r);

            m_Tracking = dmTracking::New(m_Config);
            m_ScriptContext = dmTracking::GetScriptContext(m_Tracking);
            m_LuaState = dmScript::GetLuaState(m_ScriptContext);
        }

        virtual void TearDown()
        {
            dmTracking::Finalize(m_Tracking);
            dmTracking::Delete(m_Tracking);
            dmConfigFile::Delete(m_Config);
        }

        dmConfigFile::HConfig m_Config;
        dmTracking::HContext m_Tracking;
        dmScript::HContext m_ScriptContext;
        lua_State* m_LuaState;
};

TEST_F(dmTrackingNoHookTest, Test)
{
    dmTracking::Start(m_Tracking, "Defold", "unit-test");
    for (uint32_t i=0;i!=5;i++)
    {
        dmTracking::Update(m_Tracking, 0.03f);
    }
}

class dmTrackingNoHookLocalhost : public ::testing::Test
{
    public:
        virtual void SetUp()
        {
            const char *config = "[tracking]\nurl=http://localhost:9999/\napp_id=APPID\n";
            dmConfigFile::Result r = dmConfigFile::LoadFromBuffer(config, strlen(config), 0, 0, &m_Config);
            ASSERT_EQ(dmConfigFile::RESULT_OK, r);

            m_Tracking = dmTracking::New(m_Config);
            m_ScriptContext = dmTracking::GetScriptContext(m_Tracking);
            m_LuaState = dmScript::GetLuaState(m_ScriptContext);
        }

        virtual void TearDown()
        {
            dmTracking::Finalize(m_Tracking);
            dmTracking::Delete(m_Tracking);
            dmConfigFile::Delete(m_Config);
        }

        dmConfigFile::HConfig m_Config;
        dmTracking::HContext m_Tracking;
        dmScript::HContext m_ScriptContext;
        lua_State* m_LuaState;
};

TEST_F(dmTrackingNoHookLocalhost, Test)
{
    dmTracking::Start(m_Tracking, "Defold", "unit-test");
    // The first update will perform an http.request
    // Since the http.request is not mocked away in this test, the real http-service is used, which expects the recipient to deallocate some dynamic memory attached to the response
    // Let's wait for it to make valgrind happy
    dmMessage::HSocket socket = dmTracking::GetSocket(m_Tracking);
    dmTracking::Update(m_Tracking, 0.3f);
    while (!dmMessage::HasMessages(socket))
    {
        dmTime::Sleep(10000);
    }
    dmTracking::Update(m_Tracking, 0.3f);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
