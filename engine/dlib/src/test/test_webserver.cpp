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

#include <stdint.h>
#include <stdlib.h>
#include <map>
#include <string>
#include "../dlib/time.h"
#include "../dlib/socket.h"
#include "../dlib/math.h"
#include "../dlib/thread.h"
#include "../dlib/dstrings.h"
#include "../dlib/webserver.h"
#include "../dlib/hash.h"
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

class dmWebServerTest: public jc_test_base_class
{
public:
    dmWebServer::HServer m_Server;

    volatile bool m_Quit;
    volatile bool m_ServerStarted;

    static void ServerThread(void* user_data)
    {
        dmWebServerTest* self = (dmWebServerTest*) user_data;
        self->m_ServerStarted = true;
        int iter = 0;
        while (!self->m_Quit && iter < 10000)
        {
            dmWebServer::Update(self->m_Server);
            dmTime::Sleep(1000 * 10);
            ++iter;
        }
        ASSERT_LE(iter, 10000);
    }

    static void MulHandler(void* user_data, dmWebServer::Request* request)
    {
        //dmWebServerTest* self = (dmWebServerTest*) user_data;

        int a,b;
        sscanf(request->m_Resource, "/mul/%d/%d", &a,&b);
        int c = a + b;
        char buf[16];
        dmSnPrintf(buf, sizeof(buf), "%d", c);
        dmWebServer::Send(request, buf, strlen(buf));
    }

    static void MulHeaderHandler(void* user_data, dmWebServer::Request* request)
    {
        int a, b;

        const char* as = dmWebServer::GetHeader(request, "X-a");
        const char* bs = dmWebServer::GetHeader(request, "X-b");
        a = atoi(as);
        b = atoi(bs);
        int c = a + b;
        char buf[16];
        dmSnPrintf(buf, sizeof(buf), "%d", c);
        dmWebServer::Send(request, buf, strlen(buf));
    }

    static void QuitHandler(void* user_data, dmWebServer::Request* request)
    {
        dmWebServerTest* self = (dmWebServerTest*) user_data;
        dmWebServer::SetStatusCode(request, 200);
        self->m_Quit = true;
    }

    void SetUp() override
    {
        m_Quit = false;
        m_ServerStarted = false;
        dmWebServer::NewParams params;
        params.m_ConnectionTimeout = 20;
        params.m_Port = 8501;
        dmWebServer::Result r = dmWebServer::New(&params, &m_Server);
        ASSERT_EQ(dmWebServer::RESULT_OK, r);
        dmWebServer::HandlerParams handler_params;
        handler_params.m_Userdata = this;

        handler_params.m_Handler = QuitHandler;
        dmWebServer::AddHandler(m_Server, "/quit", &handler_params);

        handler_params.m_Handler = MulHandler;
        dmWebServer::AddHandler(m_Server, "/mul", &handler_params);

        handler_params.m_Handler = MulHeaderHandler;
        dmWebServer::AddHandler(m_Server, "/header_mul", &handler_params);
    }

    void TearDown() override
    {
        if (m_Server)
        {
            dmWebServer::Result r;
            r = dmWebServer::RemoveHandler(m_Server, "/quit");
            ASSERT_EQ(dmWebServer::RESULT_OK, r);

            r = dmWebServer::RemoveHandler(m_Server, "/mul");
            ASSERT_EQ(dmWebServer::RESULT_OK, r);

            r = dmWebServer::RemoveHandler(m_Server, "/header_mul");
            ASSERT_EQ(dmWebServer::RESULT_OK, r);
            dmWebServer::Delete(m_Server);
        }
    }
};

int g_PythonTestResult;
void RunPythonThread(void*)
{
#if !defined(DM_NO_SYSTEM_FUNCTION)
    g_PythonTestResult = system("python src/test/test_webserver.py");
#else
    g_PythonTestResult = -1;
#endif
}

#if !defined(DM_SANITIZE_ADDRESS) // until we can load the dylibs properly

TEST_F(dmWebServerTest, TestServer)
{
    dmThread::Thread thread = dmThread::New(RunPythonThread, 0x8000, 0, "test");
    int iter = 0;
    while (!m_Quit && iter < 1000)
    {
        dmWebServer::Update(m_Server);
        dmTime::Sleep(1000 * 10);
        ++iter;
    }
    ASSERT_LE(iter, 1000);
    dmThread::Join(thread);
    ASSERT_EQ(0, g_PythonTestResult);
}

#endif

int main(int argc, char **argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int ret = jc_test_run_all();
    dmSocket::Finalize();
    return ret;
}
