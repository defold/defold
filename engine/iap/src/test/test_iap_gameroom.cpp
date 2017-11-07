#include <gtest/gtest.h>
#include <script/script.h>
#include <dlib/log.h>
#include <dlib/json.h>
#include <dlib/dstrings.h>
#include "../iap_private.h"
#include "../iap.h"

extern "C"
{
    #include <lua/lauxlib.h>
    #include <lua/lualib.h>
}

extern unsigned char IAP_GAMEROOM_LUA[];
extern uint32_t IAP_GAMEROOM_LUA_SIZE;

/////////////////////////////////////////////////////////////////////
// Test urls
//
#define TEST_URL_INVALID      "http://a.com/invalid.html"
#define TEST_URL_INVALID_HEAD "http://a.com/a.html"
#define TEST_URL_VALID        "http://a.com/b.html"
#define TEST_URL_VALID_2      "http://a.com/c.html"
#define TEST_URL_VALID_3      "http://a.com/d.html"
#define TEST_URL_VALID_CAPS   "http://a.com/e.html"

/////////////////////////////////////////////////////////////////////
// Test data for urls
//
static const char g_HTMLData_INVALID_HEAD[] = "asdasd";
static const char g_HTMLData_VALID[] = "<html>\n"
" <head prefix=\n"
"    \"og: http://ogp.me/ns#\n"
"     fb: http://ogp.me/ns/fb#\n"
"     product: http://ogp.me/ns/product#\">\n"
"    <meta property=\"og:type\"                   content=\"og:product\" />\n"
"    <meta property=\"fb:app_id\"                 content=\"123456\" />\n"
"    <meta property=\"og:title\"                  content=\"Valid product\" />\n"
"    <meta property=\"og:image\"                  content=\"http://a.com/smallpackage.png\" />\n"
"    <meta property=\"og:description\"            content=\"Example description\" />\n"
"    <meta property=\"og:url\"                    content=\"http://a.com/b.html\" />\n"
"    <meta property=\"product:price:amount\"      content=\"0.99\"/>\n"
"    <meta property=\"product:price:currency\"    content=\"USD\"/>\n"
"  </head>\n"
"</html>\n";

static const char g_HTMLData_VALID_2[] = "<html>\n"
" <head prefix=\n"
"    \"og: http://ogp.me/ns#\n"
"     fb: http://ogp.me/ns/fb#\n"
"     product: http://ogp.me/ns/product#\">\n"
"    <meta property=\"og:type\"                   content=\"og:product\" />\n"
"    <meta property=\"fb:app_id\"                 content=\"123456\" />\n"
"    <meta property=\"og:title\"                  content=\"Second product\" />\n"
"    <meta property=\"og:image\"                  content=\"http://a.com/smallpackage.png\" />\n"
"    <meta property=\"og:description\"            content=\"Different description\" />\n"
"    <meta property=\"og:url\"                    content=\"http://a.com/c.html\" />\n"
"    <meta property=\"product:price:amount\"      content=\"100.0\"/>\n"
"    <meta property=\"product:price:currency\"    content=\"SEK\"/>\n"
"  </head>\n"
"</html>\n";


static const char g_HTMLData_VALID_3[] = "<html><head prefix=\"og: http://ogp.me/ns# fb: http://ogp.me/ns/fb# product: http://ogp.me/ns/product#\"><meta property=\"og:type\" content=\"og:product\" /><meta property=\"fb:app_id\" content=\"123456\" /><meta property=\"og:title\" content=\"Third product\" /><meta property=\"og:image\" content=\"http://a.com/smallpackage.png\" /><meta property=\"og:description\" content=\"Different description\" /><meta property=\"og:url\" content=\"http://a.com/d.html\" /><meta property=\"product:price:amount\" content=\"100.0\"/><meta property=\"product:price:currency\" content=\"SEK\"/></head></html>\n";


static const char g_HTMLData_VALID_CAPS[] = "<HTML>\n"
" <HEAD PREFIX=\n"
"    \"OG: HTTP://OGP.ME/NS#\n"
"     FB: HTTP://OGP.ME/NS/FB#\n"
"     PRODUCT: HTTP://OGP.ME/NS/PRODUCT#\">\n"
"    <META PROPERTY=\"OG:TYPE\"                   CONTENT=\"OG:PRODUCT\" />\n"
"    <META PROPERTY=\"FB:APP_ID\"                 CONTENT=\"123456\" />\n"
"    <META PROPERTY=\"OG:TITLE\"                  CONTENT=\"CAPS PRODUCT\" />\n"
"    <META PROPERTY=\"OG:IMAGE\"                  CONTENT=\"http://a.com/smallpackage.png\" />\n"
"    <META PROPERTY=\"OG:DESCRIPTION\"            CONTENT=\"CAPS DESCRIPTION\" />\n"
"    <META PROPERTY=\"OG:URL\"                    CONTENT=\"http://a.com/e.html\" />\n"
"    <META PROPERTY=\"PRODUCT:PRICE:AMOUNT\"      CONTENT=\"100.0\"/>\n"
"    <META PROPERTY=\"PRODUCT:PRICE:CURRENCY\"    CONTENT=\"SEK\"/>\n"
"  </HEAD>\n"
"</HTML>\n";

/////////////////////////////////////////////////////////////////////
// Globals used to keep track of results from "iap.__facebook_helper_list".
//
bool        g_HasResponse              = false;
int         g_ResponseCallback         = LUA_NOREF;
const char* g_CurrentResponseData      = NULL;
int         g_CurrentResponseStatus    = 404;

bool        g_ListResponse_WasError    = false;
int         g_ListResponse_ErrorEnum   = -1;
char*       g_ListResponse_ErrorReason = NULL;

struct ProductResponse
{
    ProductResponse() {
        m_Ident = NULL;
        m_Title = NULL;
        m_Description = NULL;
        m_CurrencyCode = NULL;
        m_PriceString = NULL;
        m_Price = 0.0;
    }
    ~ProductResponse() {
        if (m_Ident)
            free(m_Ident);
        if (m_Title)
            free(m_Title);
        if (m_Description)
            free(m_Description);
        if (m_CurrencyCode)
            free(m_CurrencyCode);
        if (m_PriceString)
            free(m_PriceString);
    }
    char* m_Ident;
    char* m_Title;
    char* m_Description;
    double m_Price;
    char* m_CurrencyCode;
    char* m_PriceString;
};

dmArray<ProductResponse*> g_ProductResponses;

// Cleanup of global vars for list results.
static void ResetHandleListResult()
{
    // Clear error
    g_ListResponse_WasError = false;
    g_ListResponse_ErrorEnum = -1;
    if (g_ListResponse_ErrorReason) {
        free(g_ListResponse_ErrorReason);
        g_ListResponse_ErrorReason = NULL;
    }

    for (unsigned int i = 0; i < g_ProductResponses.Size(); ++i)
    {
        delete g_ProductResponses[i];
    }
    g_ProductResponses.SetSize(0);
}

// Runs the callback that was passed to http.request,
// returning the response data and status that was set in MockHTTPRequest.
static void TickHTTPResponse(lua_State* L)
{
    if (g_HasResponse) {

        lua_rawgeti(L, LUA_REGISTRYINDEX, g_ResponseCallback);

        // Push "self", and static request id
        lua_newtable(L);
        lua_pushinteger(L, 1);

        // Response table
        lua_newtable(L);
        lua_pushinteger(L, g_CurrentResponseStatus);
        lua_setfield(L, -2, "status");
        lua_pushstring(L, g_CurrentResponseData);
        lua_setfield(L, -2, "response");

        // Unregister callback and clear current response
        luaL_unref(L, LUA_REGISTRYINDEX, g_ResponseCallback);
        g_ResponseCallback = LUA_NOREF;
        g_HasResponse = false;
        g_CurrentResponseData = NULL;
        g_CurrentResponseStatus = 404;

        int ret = lua_pcall(L, 3, 0, 0);
        if (ret != 0)
        {
            dmLogError("Error while calling http.request callback: %s", lua_tostring(L, -1));
            lua_pop(L, 1);
        }


    }
}

// Mock implementation of http.request
static int MockHTTPRequest(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TSTRING);
    luaL_checktype(L, 2, LUA_TSTRING);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    int top = lua_gettop(L);

    lua_pushvalue(L, 3);
    g_ResponseCallback = luaL_ref(L, LUA_REGISTRYINDEX);
    g_HasResponse = true;

    const char* url = lua_tostring(L, 1);

    g_CurrentResponseStatus = 200;
    if (strcmp(url, TEST_URL_INVALID_HEAD) == 0) {
        g_CurrentResponseData = g_HTMLData_INVALID_HEAD;
    } else if (strcmp(url, TEST_URL_VALID) == 0) {
        g_CurrentResponseData = g_HTMLData_VALID;
    } else if (strcmp(url, TEST_URL_VALID_2) == 0) {
        g_CurrentResponseData = g_HTMLData_VALID_2;
    } else if (strcmp(url, TEST_URL_VALID_3) == 0) {
        g_CurrentResponseData = g_HTMLData_VALID_3;
    } else if (strcmp(url, TEST_URL_VALID_CAPS) == 0) {
        g_CurrentResponseData = g_HTMLData_VALID_CAPS;
    } else {
        g_CurrentResponseStatus = 404;
        g_CurrentResponseData = NULL;
    }

    assert(top == lua_gettop(L));
    return 0;
}

class IAPGameroomTest : public ::testing::Test
{
public:
    lua_State* L;

private:
    virtual void SetUp() {
        L = luaL_newstate();
        int top = lua_gettop(L);

        // Register default Lua libs (table, string etc)
        luaL_openlibs(L);

        // Push mocked http.request function
        static const luaL_reg mocked_functions[] =
        {
            {"request", MockHTTPRequest},
            {0, 0}
        };
        luaL_register(L, "http", mocked_functions);
        lua_pop(L, 1);

        // Create "iap" table and push constants/enums.
        lua_newtable(L);
        IAP_PushConstants(L);
        lua_setglobal(L, "iap");

        // Load iap_gameroom.lua which adds __facebook_helper_list to the iap table.
        if (luaL_loadbuffer(L, (const char*)IAP_GAMEROOM_LUA, IAP_GAMEROOM_LUA_SIZE, "(internal) iap_gameroom.lua") != 0)
        {
            dmLogError("Could not load iap_gameroom.lua: %s", lua_tolstring(L, -1, 0));
            lua_pop(L, 1);
        }
        else
        {
            int ret = lua_pcall(L, 0, 0, 0);
            if (ret != 0)
            {
                dmLogError("Error while running iap_gameroom.lua: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        }

        assert(top == lua_gettop(L));

        g_ProductResponses.SetCapacity(32);
        g_ProductResponses.SetSize(0);

    }
    virtual void TearDown() {
        lua_close(L);
        ResetHandleListResult();
    }
};

// Helper function to get and allocate a string value
// from the product result table.
static void GetProductStringField(lua_State* L, int table_index, const char* field, char** str_out)
{
    lua_getfield(L, table_index, field);
    if (lua_type(L, -1) != LUA_TSTRING) {
        dmLogError("Product response field %s was not a string: %s", field, lua_typename(L, lua_type(L, -1)));
        return;
    }
    size_t len = 0;
    const char* str = lua_tolstring(L, -1, &len);
    *str_out = (char*)malloc(++len);
    dmStrlCpy(*str_out, str, len);
    lua_pop(L, 1);
}

// Callback that iap.__facebook_helper_list will call when finished.
// We set the globals vars for error or product results here that
// can then be verified in the tests.
static int HandleListResult(lua_State* L)
{
    int top = lua_gettop(L);

    // Handle list needs to have at least result
    assert(top > 1);

    // Check if error argument is set, must be a table
    if (lua_type(L, 2) == LUA_TTABLE)
    {
        g_ListResponse_WasError = true;

        lua_getfield(L, 2, "error");
        g_ListResponse_ErrorEnum = lua_tointeger(L, -1);
        lua_pop(L, 1);

        GetProductStringField(L, 2, "reason", &g_ListResponse_ErrorReason);
    } else {
        // No error, needs to have result
        assert(LUA_TTABLE == lua_type(L, 1));

        // Walk response table and store product responses
        lua_pushnil(L);
        while (lua_next(L, 1) != 0) {
            assert(LUA_TTABLE == lua_type(L, -1));

            int response_index = lua_gettop(L);
            ProductResponse* product = new ProductResponse();
            GetProductStringField(L, response_index, "ident", &(product->m_Ident));
            GetProductStringField(L, response_index, "title", &(product->m_Title));
            GetProductStringField(L, response_index, "description", &(product->m_Description));
            GetProductStringField(L, response_index, "currency_code", &(product->m_CurrencyCode));
            GetProductStringField(L, response_index, "price_string", &(product->m_PriceString));

            lua_getfield(L, response_index, "price");
            product->m_Price = lua_tonumber(L, -1);
            lua_pop(L, 1);

            g_ProductResponses.Push(product);

            // Pop key for table iteration
            lua_pop(L, 1);
        }

    }

    assert(top == lua_gettop(L));
    return 0;
}

// Test an url that will return 404
TEST_F(IAPGameroomTest, InvalidURL)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_INVALID);
    lua_rawseti(L, -2, 1);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();
    TickHTTPResponse(L);
    ASSERT_TRUE(g_ListResponse_WasError);
    ASSERT_EQ(REASON_UNSPECIFIED, g_ListResponse_ErrorEnum);
    ASSERT_EQ(0, strncmp("Could not get product", g_ListResponse_ErrorReason, 21));
}

// Test a product that doesn't have a valid <head> tag
TEST_F(IAPGameroomTest, InvalidHead)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_INVALID_HEAD);
    lua_rawseti(L, -2, 1);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();
    TickHTTPResponse(L);
    ASSERT_TRUE(g_ListResponse_WasError);
    ASSERT_EQ(REASON_UNSPECIFIED, g_ListResponse_ErrorEnum);
    ASSERT_EQ(0, strncmp("Could not parse product", g_ListResponse_ErrorReason, 23));
}

// Test a valid product, should include all the required <meta> fields
TEST_F(IAPGameroomTest, ValidProduct)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_VALID);
    lua_rawseti(L, -2, 1);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();
    TickHTTPResponse(L);
    ASSERT_FALSE(g_ListResponse_WasError);
    ASSERT_EQ(1, g_ProductResponses.Size());

    ProductResponse* product = g_ProductResponses[0];
    ASSERT_STREQ(TEST_URL_VALID, product->m_Ident);
    ASSERT_STREQ("Valid product", product->m_Title);
    ASSERT_STREQ("Example description", product->m_Description);
    ASSERT_DOUBLE_EQ(0.99, product->m_Price);
    ASSERT_STREQ("0.99USD", product->m_PriceString);
    ASSERT_STREQ("USD", product->m_CurrencyCode);
}

// Test a valid product, should include all the required <meta> fields
TEST_F(IAPGameroomTest, ValidProductCaps)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_VALID_CAPS);
    lua_rawseti(L, -2, 1);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();
    TickHTTPResponse(L);
    ASSERT_FALSE(g_ListResponse_WasError);
    ASSERT_EQ(1, g_ProductResponses.Size());

    ProductResponse* product = g_ProductResponses[0];
    ASSERT_STREQ(TEST_URL_VALID_CAPS, product->m_Ident);
    ASSERT_STREQ("CAPS PRODUCT", product->m_Title);
    ASSERT_STREQ("CAPS DESCRIPTION", product->m_Description);
    ASSERT_DOUBLE_EQ(100.0, product->m_Price);
    ASSERT_STREQ("100SEK", product->m_PriceString);
    ASSERT_STREQ("SEK", product->m_CurrencyCode);
}

// Test multiple valid product urls
TEST_F(IAPGameroomTest, MultipleProducts)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_VALID);
    lua_rawseti(L, -2, 1);
    lua_pushstring(L, TEST_URL_VALID_2);
    lua_rawseti(L, -2, 2);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();

    // Tick HTTP response function twice since this
    // list will result in two HTTP requests before returning.
    TickHTTPResponse(L);
    TickHTTPResponse(L);
    ASSERT_FALSE(g_ListResponse_WasError);
    ASSERT_EQ(2, g_ProductResponses.Size());

    ProductResponse* product = g_ProductResponses[0];
    ASSERT_STREQ(TEST_URL_VALID, product->m_Ident);
    ASSERT_STREQ("Valid product", product->m_Title);
    ASSERT_STREQ("Example description", product->m_Description);
    ASSERT_DOUBLE_EQ(0.99, product->m_Price);
    ASSERT_STREQ("0.99USD", product->m_PriceString);
    ASSERT_STREQ("USD", product->m_CurrencyCode);

    product = g_ProductResponses[1];
    ASSERT_STREQ(TEST_URL_VALID_2, product->m_Ident);
    ASSERT_STREQ("Second product", product->m_Title);
    ASSERT_STREQ("Different description", product->m_Description);
    ASSERT_DOUBLE_EQ(100.0, product->m_Price);
    ASSERT_STREQ("100SEK", product->m_PriceString);
    ASSERT_STREQ("SEK", product->m_CurrencyCode);
}

// Test multiple product urls, one is invalid and should fail the request.
TEST_F(IAPGameroomTest, MixedUrls)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_VALID);
    lua_rawseti(L, -2, 1);
    lua_pushstring(L, TEST_URL_INVALID_HEAD);
    lua_rawseti(L, -2, 2);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();
    TickHTTPResponse(L);
    TickHTTPResponse(L);
    ASSERT_TRUE(g_ListResponse_WasError);
    ASSERT_EQ(REASON_UNSPECIFIED, g_ListResponse_ErrorEnum);
    ASSERT_EQ(0, strncmp("Could not parse product", g_ListResponse_ErrorReason, 23));
}

// Test product html that has no newlines.
TEST_F(IAPGameroomTest, ValidProductNoNewlines)
{
    // Get iap.__facebook_helper_list
    int top = lua_gettop(L);
    lua_getglobal(L, "iap");
    lua_getfield(L, -1, "__facebook_helper_list");

    // Push url table
    lua_newtable(L);
    lua_pushstring(L, TEST_URL_VALID_3);
    lua_rawseti(L, -2, 1);

    // Push result callback
    lua_pushcfunction(L, HandleListResult);

    ASSERT_EQ(0, lua_pcall(L, 2, 0, 0));

    // Pop "iap" table
    lua_pop(L, 1);
    assert(top == lua_gettop(L));

    // Check result
    ResetHandleListResult();
    TickHTTPResponse(L);
    ASSERT_FALSE(g_ListResponse_WasError);
    ASSERT_EQ(1, g_ProductResponses.Size());

    ProductResponse* product = g_ProductResponses[0];
    ASSERT_STREQ(TEST_URL_VALID_3, product->m_Ident);
    ASSERT_STREQ("Third product", product->m_Title);
    ASSERT_STREQ("Different description", product->m_Description);
    ASSERT_DOUBLE_EQ(100.0, product->m_Price);
    ASSERT_STREQ("100SEK", product->m_PriceString);
    ASSERT_STREQ("SEK", product->m_CurrencyCode);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
