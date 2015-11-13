#include <gtest/gtest.h>
#include <script/script.h>
#include <dlib/log.h>

#include "../iac.h"


class IACTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmScript::NewContext(0x0, 0);
        dmScript::Initialize(m_Context);
        L = dmScript::GetLuaState(m_Context);

        m_ExtensionParams.m_L = L;
        dmIAC::Initialize(&m_ExtensionParams);
    }

    virtual void TearDown()
    {
        dmIAC::Finalize(&m_ExtensionParams);
        dmScript::Finalize(m_Context);
        dmScript::DeleteContext(m_Context);
    }

    dmExtension::Params m_ExtensionParams;
    dmScript::HContext m_Context;
    lua_State* L;
};


bool RunString(lua_State* L, const char* script)
{
    if (luaL_dostring(L, script) != 0)
    {
        dmLogError("%s", lua_tolstring(L, -1, 0));
        return false;
    }
    return true;
}


TEST_F(IACTest, IAC_EncodeDecodeURL)
{
    const char *test_lua =
            "scheme = \"scheme\"\n"
            "host = \"host\"\n"
            "path = \"path1/path2/path3\"\n"
            "query = {x=\"x\", y=\"x\", z=\"z\"}\n"
            "fragment = \"fragment\"\n"
            "\n"
            "eurl = iac.encode_url(scheme, nil, nil, nil, nil)\n"
            "assert(eurl == \"scheme:\")\n"
            "eurl = iac.encode_url(scheme, host, nil, nil, nil)\n"
            "assert(eurl == \"scheme://host\")\n"
            "eurl = iac.encode_url(scheme, host, path, nil, nil)\n"
            "assert(eurl == \"scheme://host/path1/path2/path3\")\n"
            "eurl = iac.encode_url(scheme, host, path, query, nil)\n"
            "assert(eurl == \"scheme://host/path1/path2/path3?y=x&x=x&z=z\")\n"
            "eurl = iac.encode_url(scheme, host, path, query, fragment)\n"
            "assert(eurl == \"scheme://host/path1/path2/path3?y=x&x=x&z=z#fragment\")\n"
            "\n"
            "durl = iac.decode_url(eurl)\n"
            "assert(durl.scheme   == scheme)\n"
            "assert(durl.host     == host)\n"
            "assert(durl.path     == path)\n"
            "assert(durl.query.x  == query.x)\n"
            "assert(durl.query.y  == query.y)\n"
            "assert(durl.query.z  == query.z)\n"
            "assert(durl.fragment == fragment)\n"
            "\n"
            "-- the below is simply checking decode_url passes with intact returned scheme result (where applicable)\n"
            "durl = iac.decode_url(\"\")\n"
            "durl = iac.decode_url(\"scheme\")\n"
            "durl = iac.decode_url(\"scheme:\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y&\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y=&\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y=x&x\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y=x&x=x&z=z\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y=x&x=x&z=z#\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3?y=x&x=x&z=z#fragment\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host/path1/path2/path3#fragment\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme://host#fragment\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme:#fragment\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"#fragment\")\n"
            "durl = iac.decode_url(\"scheme://host?y=x&x=x&z=z\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme:?y=x&x=x&z=z\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme:/path1\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme:path1\")\n"
            "assert(durl.scheme   == scheme)\n"
            "durl = iac.decode_url(\"scheme\")\n"
            "";

    ASSERT_TRUE(RunString(L, test_lua));
}


TEST_F(IACTest, IAC_Invocation)
{
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}

