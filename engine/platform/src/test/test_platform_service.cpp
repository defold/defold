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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <test/http_service_test.h>
#include "../platform_service.h"
#include "../window.hpp"
#include <dlib/json.h>
#include <dlib/math.h>
#include <dlib/webserver.h>
#include <locale.h>
#include <stdlib.h>

// A deliberately small backend: the service must link without engine or graphics.
struct dmWindow
{
    uint32_t              m_Width;
    uint32_t              m_Height;
    uint32_t              m_DrawableScale;
    float                 m_DisplayScale;
    bool                  m_Opened;
    bool                  m_ClientAvailable;
    bool                  m_ResizeSupported;
    FWindowResizeCallback m_ResizeCallback;
    void*                 m_Userdata;
};

namespace dmPlatform
{
    uint32_t GetWindowWidth(HWindow window)
    {
        return window->m_Width * window->m_DrawableScale;
    }
    uint32_t GetWindowHeight(HWindow window)
    {
        return window->m_Height * window->m_DrawableScale;
    }
    uint32_t GetWindowStateParam(HWindow window, WindowState state)
    {
        return state == WINDOW_STATE_OPENED && window->m_Opened;
    }
    float GetDisplayScaleFactor(HWindow window)
    {
        return window->m_DisplayScale;
    }
    bool IsWindowResizeSupported(HWindow window)
    {
        return window->m_ResizeSupported;
    }
    bool GetWindowClientSize(HWindow window, uint32_t* width, uint32_t* height)
    {
        *width = window->m_Width;
        *height = window->m_Height;
        return window->m_ClientAvailable;
    }
    void SetWindowSize(HWindow window, uint32_t width, uint32_t height)
    {
        window->m_Width = width;
        window->m_Height = height;
        window->m_ResizeCallback(window->m_Userdata, width, height);
    }
}

static uint64_t g_ServiceTime;
static uint64_t ServiceTime()
{
    return g_ServiceTime;
}

struct NumericLocale
{
    char* m_Previous;

    NumericLocale()
    : m_Previous(strdup(setlocale(LC_NUMERIC, 0)))
    {
    }

    ~NumericLocale()
    {
        setlocale(LC_NUMERIC, m_Previous);
        free(m_Previous);
    }

    bool UseCommaDecimalLocale()
    {
        const char* names[] = { "sv_SE.UTF-8", "sv_SE.utf8", "Swedish_Sweden.1252", "de_DE.UTF-8" };
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(names); ++i)
        {
            if (setlocale(LC_NUMERIC, names[i]) && !strcmp(localeconv()->decimal_point, ","))
            {
                return true;
            }
        }
        return false;
    }
};

class PlatformServiceTest : public HttpServiceTest
{
    public:
    dmWebServer::HServer         m_Server;
    dmPlatformService::HService  m_Service;
    HWindow                      m_Window;
    char                         m_Receipt[192];
    uint32_t                     m_ResizeCount;
    bool                         m_ClampResize;
    bool                         m_InResizeCallback;

    void SetUp()
    {
        HttpServiceTest::SetUp();
        dmWebServer::NewParams params;
        params.m_Port = 0;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::New(&params, &m_Server));
        g_ServiceTime = 1;
        m_Service = dmPlatformService::New(m_Server, ServiceTime);
        ASSERT_NE((dmPlatformService::HService)0, m_Service);
        dmWebServer::HandlerParams handler;
        handler.m_Handler = Ping;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/ping", &handler));
        m_RequestInstance = "platform-test-instance";
        m_RequestContentType = "application/json";
        m_ResizeCount = 0;
        m_ClampResize = m_InResizeCallback = false;
        m_Window = new dmWindow();
        OpenWindow();
        dmPlatformService::Attach(m_Service, m_Window);
    }

    void OpenWindow()
    {
        m_Window->m_Opened = true;
        m_Window->m_Width = 960;
        m_Window->m_Height = 640;
        m_Window->m_DrawableScale = 1;
        m_Window->m_DisplayScale = 1.0f;
        m_Window->m_ClientAvailable = true;
        m_Window->m_ResizeSupported = true;
        m_Window->m_ResizeCallback = OnResize;
        m_Window->m_Userdata = this;
    }

    void CloseWindow()
    {
        m_Window->m_Opened = false;
    }

    void TearDown()
    {
        dmPlatformService::Detach(m_Service);
        delete m_Window;
        dmPlatformService::Delete(m_Service);
        dmWebServer::Delete(m_Server);
        HttpServiceTest::TearDown();
    }

    uint16_t GetPort()
    {
        dmSocket::Address address;
        uint16_t port;
        dmWebServer::GetName(m_Server, &address, &port);
        return port;
    }

    void Poll()
    {
        dmWebServer::Update(m_Server);
    }
    static void Ping(void*, dmWebServer::Request* request)
    {
        dmWebServer::Send(request, "ok", 2);
    }

    static void OnResize(void* context, uint32_t, uint32_t)
    {
        PlatformServiceTest* test = (PlatformServiceTest*)context;
        if (test->m_InResizeCallback)
        {
            return;
        }
        ++test->m_ResizeCount;
        if (test->m_ClampResize)
        {
            // Emulate an OS refusing the requested geometry through the window API.
            test->m_InResizeCallback = true;
            dmPlatform::SetWindowSize(test->m_Window, 960, 640);
            test->m_InResizeCallback = false;
        }
    }

    void Queue(const char* body)
    {
        Fetch("POST", "/window/resize", body);
        ASSERT_EQ(202, m_Status);
        ASSERT_STREQ("no-store", m_CacheControl);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"queued\""));
        ASSERT_NE((const char*)0, strstr(m_Location, "/window/resize/"));
        dmStrlCpy(m_Receipt, m_Location, sizeof(m_Receipt));
    }
};

// Unpinned resizes complete without HTTP polling, and idle updates never issue a setter.
TEST_F(PlatformServiceTest, ResizeCompletesWithoutPolling)
{
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(0U, m_ResizeCount);
    m_RequestInstance = 0;
    Queue("{\"width\":640,\"height\":360}");
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(1U, m_ResizeCount);
    ASSERT_EQ(640U, dmPlatform::GetWindowWidth(m_Window));
    ASSERT_EQ(360U, dmPlatform::GetWindowHeight(m_Window));
    dmPlatform::SetWindowSize(m_Window, 800, 450);
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(2U, m_ResizeCount);
    m_RequestInstance = "platform-test-instance";
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"complete\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":{\"width\":640,\"height\":360}"));
}

// Window requests read live platform metrics and advance the revision when geometry changes.
TEST_F(PlatformServiceTest, WindowGeometry)
{
    Fetch("GET", "/window");
    ASSERT_EQ(200, m_Status);
    ASSERT_STREQ("no-store", m_CacheControl);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w1\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":{\"width\":960,\"height\":640}"));
    dmPlatform::SetWindowSize(m_Window, 800, 600);
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w2\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":{\"width\":800,\"height\":600}"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"drawable\":{\"width\":800,\"height\":600}"));
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w2\""));
    CloseWindow();
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w3\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":null,\"drawable\":null"));
    OpenWindow();
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w4\""));
    dmPlatformService::Detach(m_Service);
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":null,\"drawable\":null"));
}

// A refused resize is issued once; receipts retain the eventual observed size across later window changes.
TEST_F(PlatformServiceTest, ResizeObservation)
{
    m_ClampResize = true;
    Queue("{\"width\":640,\"height\":360,\"expected_window_revision\":\"w1\"}");
    dmPlatformService::Update(m_Service);
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(1U, m_ResizeCount);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"awaiting_window\""));
    Fetch("POST", "/window/resize", "{\"width\":360,\"height\":640}");
    ASSERT_EQ(409, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "resize_busy"));
    m_ClampResize = false;
    dmPlatform::SetWindowSize(m_Window, 640, 360);
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"complete\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window_revision\":\"w2\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"already_matches\":false"));
    dmPlatform::SetWindowSize(m_Window, 360, 640);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":{\"width\":640,\"height\":360}"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window_revision\":\"w2\""));
    Queue("{\"width\":360,\"height\":640}");
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(3U, m_ResizeCount);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"already_matches\":true"));
}

// Admission and dispatch both reject stale geometry and unavailable surfaces before issuing a setter.
TEST_F(PlatformServiceTest, ResizeConflicts)
{
    Fetch("POST", "/window/resize", "{\"width\":640,\"height\":360,\"expected_window_revision\":\"old\"}");
    ASSERT_EQ(409, m_Status);
    Queue("{\"width\":640,\"height\":360,\"expected_window_revision\":\"w1\"}");
    dmPlatform::SetWindowSize(m_Window, 961, 640);
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(1U, m_ResizeCount);
    Fetch("GET", m_Receipt);
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "window_revision_changed"));
    Queue("{\"width\":640,\"height\":360}");
    dmPlatformService::Attach(m_Service, 0);
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "resize_unavailable"));
    Fetch("POST", "/window/resize", "{\"width\":640,\"height\":360}");
    ASSERT_EQ(501, m_Status);
}

// Simulated monotonic time exercises timeout, bounded retention and stale IDs without timing-dependent sleeps.
TEST_F(PlatformServiceTest, ResizeDeadlineAndLifetime)
{
    Queue("{\"width\":640,\"height\":360,\"timeout_ms\":1}");
    g_ServiceTime += 2000;
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(0U, m_ResizeCount);
    Fetch("GET", m_Receipt);
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "resize_not_observed"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":{\"width\":960,\"height\":640}"));
    g_ServiceTime += 61000000;
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_EQ(410, m_Status);
    Fetch("GET", "/window/resize/r9999");
    ASSERT_EQ(404, m_Status);
    dmPlatformService::Detach(m_Service);
    dmPlatformService::Attach(m_Service, m_Window);
    Fetch("GET", m_Receipt);
    ASSERT_EQ(410, m_Status);
}

// A full receipt ring evicts its oldest completed result instead of blocking new resizes.
TEST_F(PlatformServiceTest, ResizeReceiptCapacity)
{
    char first[192];
    for (uint32_t i = 0; i < 64; ++i)
    {
        Queue("{\"width\":960,\"height\":640}");
        if (i == 0)
        {
            dmStrlCpy(first, m_Receipt, sizeof(first));
        }
        dmPlatformService::Update(m_Service);
    }
    Queue("{\"width\":960,\"height\":640}");
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(0U, m_ResizeCount);
    Fetch("GET", first);
    ASSERT_EQ(410, m_Status);
    Fetch("GET", m_Receipt);
    ASSERT_EQ(200, m_Status);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"expires_in_ms\":60000"));
    g_ServiceTime += 1000;
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"expires_in_ms\":59999"));
    g_ServiceTime += 59999000;
    Fetch("GET", m_Receipt);
    ASSERT_EQ(410, m_Status);
}

// Reattaching a recreated window invalidates observations even when the dimensions are unchanged.
TEST_F(PlatformServiceTest, WindowRevisionAfterSurfaceReplacement)
{
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w1\""));
    CloseWindow();
    OpenWindow();
    dmPlatformService::Attach(m_Service, m_Window);
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w2\""));
    Fetch("POST", "/window/resize", "{\"width\":640,\"height\":360,\"expected_window_revision\":\"w1\"}");
    ASSERT_EQ(409, m_Status);
}

// Returning to the same dimensions between observations keeps the service revision valid.
TEST_F(PlatformServiceTest, WindowRevisionIgnoresUnobservedChanges)
{
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w1\""));
    dmPlatform::SetWindowSize(m_Window, 800, 600);
    dmPlatform::SetWindowSize(m_Window, 960, 640);
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w1\""));
    Queue("{\"width\":640,\"height\":360,\"expected_window_revision\":\"w1\"}");
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"complete\""));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window_revision\":\"w2\""));
}

// Reading metrics or receipts cannot dispatch or complete a resize; only the window update advances it.
TEST_F(PlatformServiceTest, WindowReadsDoNotAdvanceResize)
{
    m_ClampResize = true;
    Queue("{\"width\":640,\"height\":360}");
    Fetch("GET", "/window");
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"queued\""));
    ASSERT_EQ(0U, m_ResizeCount);
    dmPlatformService::Update(m_Service);
    m_ClampResize = false;
    dmPlatform::SetWindowSize(m_Window, 640, 360);
    Fetch("GET", "/window");
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"awaiting_window\""));
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"complete\""));
}

// JSON number spellings with an integral value use the same validation as integer literals.
TEST_F(PlatformServiceTest, ResizeJsonNumbers)
{
    Queue("{\"width\":640.0,\"height\":3.6e2}");
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(640U, dmPlatform::GetWindowWidth(m_Window));
    ASSERT_EQ(360U, dmPlatform::GetWindowHeight(m_Window));
}

// A game's numeric locale must not corrupt window JSON or change its fractional display scale.
TEST_F(PlatformServiceTest, WindowJsonWithNumericLocale)
{
    NumericLocale locale;
    if (!locale.UseCommaDecimalLocale())
    {
        SKIP();
    }
    m_Window->m_DisplayScale = 1.5f;
    Fetch("GET", "/window");
    ASSERT_EQ(200, m_Status);
    dmJson::Document json;
    ASSERT_TRUE(json.Parse(m_Body.Begin(), m_Body.Size() - 1));
    ASSERT_EQ(1.5, json.Get(json.Field(0, "display_scale")).m_Number);
    ASSERT_STREQ(",", localeconv()->decimal_point);
}

// Decimal and exponent dimensions retain their values, and fractional options remain invalid, in a comma locale.
TEST_F(PlatformServiceTest, ResizeJsonNumbersWithNumericLocale)
{
    NumericLocale locale;
    if (!locale.UseCommaDecimalLocale())
    {
        SKIP();
    }
    Queue("{\"width\":640.0,\"height\":3.6e2,\"timeout_ms\":1.5e3}");
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(640U, dmPlatform::GetWindowWidth(m_Window));
    ASSERT_EQ(360U, dmPlatform::GetWindowHeight(m_Window));
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"complete\""));
    Fetch("POST", "/window/resize", "{\"width\":1.2,\"height\":360}");
    ASSERT_EQ(400, m_Status);
    Fetch("POST", "/window/resize", "{\"width\":640,\"height\":360,\"timeout_ms\":1.5}");
    ASSERT_EQ(400, m_Status);
    ASSERT_STREQ(",", localeconv()->decimal_point);
}

// Strict flat JSON, methods and body limits are validated without consuming unexpected uploads.
TEST_F(PlatformServiceTest, WindowRequestValidation)
{
    const char* invalid[] = {
        "", "{}", "[]", "{", "{\"width\":1}", "{\"width\":1,\"height\":0}",
        "{\"width\":-1,\"height\":1}", "{\"width\":1.2,\"height\":1}",
        "{\"width\":01,\"height\":1}", "{\"width\":true,\"height\":1}",
        "{\"width\":1,\"height\":1,\"width\":2}", "{\"width\":1,\"height\":1,\"\\u0077idth\":2}",
        "{\"width\":1,\"height\":1,\"extra\":1}", "{\"width\":1,\"height\":1,}",
        "{\"width\":1,\"height\":1} {}", "{\"width\":1,\"height\":1,\"timeout_ms\":60001}",
        "{\"width\":1,\"height\":1,\"timeout_ms\":0}", "{\"width\":1,\"height\":1,\"expected_window_revision\":null}",
        "{\"width\":2147483648,\"height\":1}", "{\"width\":18446744073709551616,\"height\":1}",
        "{\"width\":1,\"height\":1,\"timeout_ms\":false}"
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
    {
        Fetch("POST", "/window/resize", invalid[i]);
        ASSERT_EQ(400, m_Status);
    }
    const char* paths[] = { "/window", "/window/resize/unknown" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(paths); ++i)
    {
        Fetch("DELETE", paths[i], "", false, 10);
        ASSERT_EQ(405, m_Status);
        ASSERT_STREQ("GET", m_Allow);
        ASSERT_STREQ("close", m_Connection);
        char request[256];
        dmSnPrintf(request, sizeof(request), "GET %s HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n", paths[i]);
        dmSocket::Socket stalled = OpenRequest(request);
        ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, stalled);
        Fetch("GET", "/ping");
        ASSERT_EQ(200, m_Status);
        ReadResponse(stalled);
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "400 Bad Request"));
        ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "Connection: close"));
    }
    Fetch("GET", "/window/resize");
    ASSERT_EQ(405, m_Status);
    ASSERT_STREQ("POST", m_Allow);
    const char* missing[] = { "/windows", "/window/", "/windowXYZ", "/window?wait=1", "/window/resize/id/other" };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(missing); ++i)
    {
        Fetch("POST", missing[i], "", false, 10);
        ASSERT_EQ(404, m_Status);
    }
    Fetch("POST", "/window/resize", "", false, 1025);
    ASSERT_EQ(413, m_Status);
    ASSERT_STREQ("close", m_Connection);
    m_RequestInstance = 0;
    Fetch("GET", "/window/resize/r1");
    ASSERT_EQ(400, m_Status);
    m_RequestInstance = "platform-test-instance";
    m_RequestContentType = "text/plain";
    Fetch("POST", "/window/resize", "", false, 10);
    ASSERT_EQ(400, m_Status);
}

// An incomplete upload yields to other clients and is admitted only after its body arrives.
TEST_F(PlatformServiceTest, ResizePartialUpload)
{
    m_RequestPrefixSize = 8;
    Queue("{\"width\":640,\"height\":360}");
    ASSERT_EQ(200, m_ProbeStatus);
    ASSERT_EQ(dmHttpClient::RESULT_OK, m_ProbeResult);
    dmPlatformService::Update(m_Service);
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(1U, m_ResizeCount);
}

// Observations copy live geometry; later changes do not rewrite an earlier observation.
TEST_F(PlatformServiceTest, CopiedObservation)
{
    dmPlatformService::WindowObservation observation;
    dmPlatformService::Observe(m_Service, &observation);
    ASSERT_STREQ("w1", observation.m_Revision);
    dmPlatform::SetWindowSize(m_Window, 800, 600);
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"revision\":\"w2\""));
    dmPlatformService::WindowObservation current;
    dmPlatformService::Observe(m_Service, &current);
    ASSERT_STREQ("w2", current.m_Revision);
    ASSERT_EQ(800U, current.m_Metrics.m_WindowWidth);
    ASSERT_STREQ("w1", observation.m_Revision);
    ASSERT_EQ(960U, observation.m_Metrics.m_WindowWidth);
}

// Native client sizes and drawable pixels stay distinct; graphics limits do not reject window requests.
TEST_F(PlatformServiceTest, HighDpiGeometry)
{
    m_Window->m_DrawableScale = 2;
    m_Window->m_DisplayScale = 1.5f;
    dmPlatformService::WindowObservation observation;
    dmPlatformService::Observe(m_Service, &observation);
    ASSERT_EQ(960U, observation.m_Metrics.m_WindowWidth);
    ASSERT_EQ(1920U, observation.m_Metrics.m_DrawableWidth);
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"display_scale\":1.5"));
    Queue("{\"width\":8193,\"height\":360}");
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":{\"width\":8193,\"height\":360}"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"drawable\":{\"width\":16386,\"height\":720}"));
}

// Fullscreen and mobile-style metrics remain observable without advertising an unsupported resize.
TEST_F(PlatformServiceTest, UnavailableResize)
{
    m_Window->m_ResizeSupported = false;
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "window_mode_unsupported"));
    Fetch("POST", "/window/resize", "{\"width\":640,\"height\":360}");
    ASSERT_EQ(501, m_Status);
    m_Window->m_ClientAvailable = false;
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"window\":null,\"drawable\":{\"width\":960,\"height\":640}"));
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "client_metrics_unavailable"));
    m_Window->m_ClientAvailable = m_Window->m_ResizeSupported = true;
    Queue("{\"width\":640,\"height\":360}");
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "\"state\":\"complete\""));
    m_Window->m_DrawableScale = 0;
    Fetch("GET", "/window");
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "drawable_unavailable"));
    Fetch("POST", "/window/resize", "{\"width\":640,\"height\":360}");
    ASSERT_EQ(501, m_Status);
    ASSERT_EQ(1U, m_ResizeCount);
}

// Detaching retires receipts and native pointers; reattaching the same server registers no duplicate routes.
TEST_F(PlatformServiceTest, DetachAndReattach)
{
    dmArray<char> paths;
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    Queue("{\"width\":640,\"height\":360}");
    dmPlatformService::Detach(m_Service);
    delete m_Window;
    m_Window = 0;
    dmPlatformService::Update(m_Service);
    Fetch("GET", "/window");
    Fetch("GET", m_Receipt);
    ASSERT_EQ(410, m_Status);
    dmPlatformService::WindowObservation observation;
    dmPlatformService::Observe(m_Service, &observation);
    ASSERT_FALSE(observation.m_Metrics.m_WindowAvailable);
    ASSERT_FALSE(observation.m_Metrics.m_DrawableAvailable);
    m_Window = new dmWindow();
    OpenWindow();
    dmPlatformService::Attach(m_Service, m_Window);
    Queue("{\"width\":640,\"height\":360}");
    dmPlatformService::Update(m_Service);
    ASSERT_EQ(1U, m_ResizeCount);
    dmArray<char> reattached_paths;
    dmWebServer::GetOpenAPIPaths(m_Server, &reattached_paths);
    ASSERT_STREQ(paths.Begin(), reattached_paths.Begin());
}

// An explicit new attachment invalidates observations even if a native wrapper reuses its address and dimensions.
TEST_F(PlatformServiceTest, SameAddressAttachment)
{
    Queue("{\"width\":640,\"height\":360,\"expected_window_revision\":\"w1\"}");
    dmPlatformService::Attach(m_Service, m_Window);
    dmPlatformService::Update(m_Service);
    Fetch("GET", m_Receipt);
    ASSERT_NE((const char*)0, strstr(m_Body.Begin(), "window_revision_changed"));
    ASSERT_EQ(0U, m_ResizeCount);
}

// Removing a service unregisters handlers even for a stalled upload and leaves the borrowed server usable.
TEST_F(PlatformServiceTest, UnregisterBeforeDelete)
{
    char request[256];
    dmSnPrintf(request, sizeof(request), "POST /window/resize HTTP/1.1\r\nHost: localhost\r\nX-Engine-Instance: %s\r\nContent-Type: application/json\r\nContent-Length: 26\r\n\r\n{", m_RequestInstance);
    dmSocket::Socket stalled = OpenRequest(request);
    ASSERT_NE(dmSocket::INVALID_SOCKET_HANDLE, stalled);
    Fetch("GET", "/ping");
    ASSERT_EQ(200, m_Status);
    dmPlatformService::Delete(m_Service);
    Fetch("GET", "/window");
    ASSERT_EQ(404, m_Status);
    dmArray<char> paths;
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_EQ((const char*)0, strstr(paths.Begin(), "/window"));
    CloseRequest(stalled);
    Fetch("GET", "/ping");
    ASSERT_EQ(200, m_Status);
    m_Service = dmPlatformService::New(m_Server, ServiceTime);
    ASSERT_NE((dmPlatformService::HService)0, m_Service);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
