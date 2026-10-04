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

#include <string.h>
#include <dlib/socket.h>
#include <dlib/webserver.h>
#include <dlib/webserver_openapi.h>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#define GET_OPERATION "\"get\":{\"responses\":{\"200\":{\"description\":\"OK\"}}}"

static void EmptyHandler(void*, dmWebServer::Request*)
{
}

class WebServerOpenAPITest : public jc_test_base_class
{
public:
    dmWebServer::HServer m_Server;
    dmWebServer::HandlerParams m_Params;

    void SetUp()
    {
        dmWebServer::NewParams params;
        ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::New(&params, &m_Server));
        m_Params.m_Handler = EmptyHandler;
        m_Params.m_Userdata = 0;
    }

    void TearDown()
    {
        dmWebServer::Delete(m_Server);
    }
};

// Registration and removal must keep the generated paths synchronized with live handlers.
TEST_F(WebServerOpenAPITest, RegisterAndRemovePaths)
{
    static const char first[] = "{\"/first\":{" GET_OPERATION "}}";
    static const char second[] = "{\"/second\":{" GET_OPERATION "}}";
    dmArray<char> paths;
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ("{}", paths.Begin());
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/first", &m_Params, first));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/second", &m_Params, second));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ("{\"/first\":{" GET_OPERATION "},\"/second\":{" GET_OPERATION "}}", paths.Begin());
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/first"));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ(second, paths.Begin());
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/first", &m_Params, first));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/second"));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ(first, paths.Begin());
}

// Both APIs must allow undocumented handlers without adding empty fragments or stray commas to discovery.
TEST_F(WebServerOpenAPITest, OptionalMetadataLifecycle)
{
    static const char first[] = "{\"/first\":{" GET_OPERATION "}}";
    static const char second[] = "{\"/second\":{" GET_OPERATION "}}";
    dmArray<char> paths;
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/legacy", &m_Params));
    ASSERT_EQ(dmWebServer::RESULT_HANDLER_ALREADY_REGISTRED, dmWebServer::AddHandler(m_Server, "/legacy", &m_Params, 0));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ("{}", paths.Begin());
    ASSERT_EQ(3U, paths.Size());

    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/first", &m_Params, first));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/middle", &m_Params, 0));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/second", &m_Params, second));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/last", &m_Params));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ("{\"/first\":{" GET_OPERATION "},\"/second\":{" GET_OPERATION "}}", paths.Begin());
    ASSERT_EQ(strlen(paths.Begin()) + 1, paths.Size());
    ASSERT_EQ(dmWebServer::RESULT_ERROR_INVAL, dmWebServer::AddHandler(m_Server, "/", &m_Params, first));

    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/middle"));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/first"));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ(second, paths.Begin());
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/first", &m_Params));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/second"));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ("{}", paths.Begin());

    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/first"));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/first", &m_Params, first));
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ(first, paths.Begin());
}

// A routing prefix may describe several concrete paths and several methods on one path.
TEST_F(WebServerOpenAPITest, MultiplePathsAndMethods)
{
    static const char json[] =
        " \n { \"/extension\":{" GET_OPERATION ",\"post\":{\"responses\":{\"204\":{\"description\":\"Updated\"}}}},"
        "\"/extension/{id}\":{\"parameters\":[{\"name\":\"id\",\"in\":\"path\",\"required\":true,\"schema\":{\"type\":\"string\"}}],"
        GET_OPERATION "}} \r\n";
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/extension", &m_Params, json));
    dmArray<char> paths;
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_NE((const char*)0, strstr(paths.Begin(), "\"/extension/{id}\""));
    ASSERT_NE((const char*)0, strstr(paths.Begin(), "\"post\""));
    dmWebServer::OpenAPI parsed;
    ASSERT_TRUE(dmWebServer::ParseOpenAPI("/", paths.Begin(), &parsed));
    ASSERT_EQ(2U, parsed.m_Paths.Size());
}

// Invalid metadata must fail atomically without publishing a handler or changing existing documentation.
TEST_F(WebServerOpenAPITest, RejectInvalidMetadata)
{
    static const char valid[] = "{\"/valid\":{" GET_OPERATION "}}";
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/valid", &m_Params, valid));
    const char* invalid[] = {
        "", "{}", "[]", "null", "{", "{\"/bad\":", "{\"/bad\":{}}", "{\"/bad\":[]}",
        "{\"/other\":{" GET_OPERATION "}}",
        "{\"/bad\":{\"get\":{}}}",
        "{\"/bad\":{\"get\":{\"responses\":{}}}}",
        "{\"/bad\":{\"get\":{\"responses\":[]}}}",
        "{\"/bad\":{\"GET\":{\"responses\":{\"200\":{\"description\":\"OK\"}}}}}",
        "{\"/bad\":{" GET_OPERATION ",}}",
        "{\"/bad\":{" GET_OPERATION "},}",
        "{\"/bad\":{" GET_OPERATION "}} trailing",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":01}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":1.}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":1e+}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":NaN}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":[true,]}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":\"\\q\"}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":\"\\u12\"}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":\"\\ud800\"}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":\"\\udc00\"}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":\"\n\"}}",
        "{\"/bad\":{" GET_OPERATION ",\"x-value\":\"\xc0\x80\"}}",
        "{\"/bad\\u0000hidden\":{" GET_OPERATION "}}",
        "{\"/bad/{id\":{" GET_OPERATION "}}",
        "{\"/bad/{}\":{" GET_OPERATION "}}",
        "{\"/bad/{id/other}\":{" GET_OPERATION "}}",
        "{\"/bad?query\":{" GET_OPERATION "}}",
        "{\"/bad\":{" GET_OPERATION "," GET_OPERATION "}}",
        "{\"/bad\":{" GET_OPERATION "},\"/bad\":{" GET_OPERATION "}}"
    };
    for (uint32_t i = 0; i < DM_ARRAY_SIZE(invalid); ++i)
    {
        ASSERT_EQ(dmWebServer::RESULT_ERROR_INVAL, dmWebServer::AddHandler(m_Server, "/bad", &m_Params, invalid[i]));
        ASSERT_EQ(dmWebServer::RESULT_HANDLER_NOT_REGISTRED, dmWebServer::RemoveHandler(m_Server, "/bad"));
        dmArray<char> paths;
        dmWebServer::GetOpenAPIPaths(m_Server, &paths);
        ASSERT_STREQ(valid, paths.Begin());
    }
}

// Escaped JSON keys and renamed path parameters must not bypass duplicate-path checks.
TEST_F(WebServerOpenAPITest, RejectConflictingPaths)
{
    static const char json[] = "{\"/items/{id}\":{" GET_OPERATION "}}";
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/items/", &m_Params, json));
    ASSERT_EQ(dmWebServer::RESULT_HANDLER_ALREADY_REGISTRED, dmWebServer::AddHandler(m_Server, "/items/", &m_Params, json));
    ASSERT_EQ(dmWebServer::RESULT_ERROR_INVAL, dmWebServer::AddHandler(m_Server, "/items", &m_Params,
        "{\"\\/items\\/{name}\":{" GET_OPERATION "}}"));
    ASSERT_EQ(dmWebServer::RESULT_ERROR_INVAL, dmWebServer::AddHandler(m_Server, "/", &m_Params,
        "{\"/items/\\u007bid\\u007d\":{" GET_OPERATION "}}"));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::RemoveHandler(m_Server, "/items/"));
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/items", &m_Params, json));
}

// JSON escapes, UTF-8, nested values and numeric formats must survive document assembly unchanged.
TEST_F(WebServerOpenAPITest, PreserveJSONValues)
{
    static const char json[] = "{\"/values\":{" GET_OPERATION ",\"x-data\":{"
        "\"string\":\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u00e5\\ud83d\\ude00 å\","
        "\"array\":[null,true,false,0,-1,1.5,1e2,-1.2E-3,{},[]]}}}";
    ASSERT_EQ(dmWebServer::RESULT_OK, dmWebServer::AddHandler(m_Server, "/values", &m_Params, json));
    dmArray<char> paths;
    dmWebServer::GetOpenAPIPaths(m_Server, &paths);
    ASSERT_STREQ(json, paths.Begin());
}

// Excessively nested metadata must be rejected before recursive parsing exhausts the stack.
TEST_F(WebServerOpenAPITest, LimitNesting)
{
    char json[512] = "{\"/deep\":{" GET_OPERATION ",\"x-data\":";
    char* p = json + strlen(json);
    for (uint32_t i = 0; i < 70; ++i) *p++ = '[';
    *p++ = '0';
    for (uint32_t i = 0; i < 70; ++i) *p++ = ']';
    strcpy(p, "}}");
    ASSERT_EQ(dmWebServer::RESULT_ERROR_INVAL, dmWebServer::AddHandler(m_Server, "/deep", &m_Params, json));
}

// Truncated strings and nested values must fail without reading past the terminating zero.
TEST_F(WebServerOpenAPITest, RejectEveryTruncation)
{
    static const char json[] = "{\"/truncated\":{" GET_OPERATION ",\"x-data\":[true,false,null,-1.2e+3,\"\\ud83d\\ude00\",{\"key\":\"value\"}]}}";
    for (uint32_t length = 0; length < sizeof(json) - 1; ++length)
    {
        char* truncated = new char[length + 1];
        memcpy(truncated, json, length);
        truncated[length] = 0;
        dmWebServer::Result result = dmWebServer::AddHandler(m_Server, "/truncated", &m_Params, truncated);
        delete[] truncated;
        ASSERT_EQ(dmWebServer::RESULT_ERROR_INVAL, result);
    }
}

int main(int argc, char** argv)
{
    dmSocket::Initialize();
    jc_test_init(&argc, argv);
    int result = jc_test_run_all();
    dmSocket::Finalize();
    return result;
}
