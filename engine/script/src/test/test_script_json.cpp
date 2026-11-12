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

#include <string.h> // memcpy
#include <locale.h>
#include <fenv.h>
#include <math.h>

#include "script.h"
#include "test_script.h"

#include <testmain/testmain.h>
#include <dlib/log.h>
#include <dlib/dstrings.h>
#include <dlib/json.hpp>

class ScriptJsonTest : public dmScriptTest::ScriptTest
{
};

TEST_F(ScriptJsonTest, TestJson)
{
    int top = lua_gettop(L);

    ASSERT_TRUE(RunFile(L, "test_json.luac"));

    lua_getglobal(L, "functions");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "test_json");
    ASSERT_EQ(LUA_TFUNCTION, lua_type(L, -1));
    int result = dmScript::PCall(L, 0, LUA_MULTRET);
    if (result == LUA_ERRRUN)
    {
        ASSERT_TRUE(false);
    }
    else
    {
        ASSERT_EQ(0, result);
    }
    lua_pop(L, 1);


    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptJsonTest, TestJsonToLua)
{
    int top = lua_gettop(L);

    {
        const char* json_original = "{\"foo\":\"bar\",\"num\":16}hello";
        size_t json_length = 22;
        // Make it fully dynamic so that ASAN can catch it
        const char* json = (const char*)malloc(json_length);
        memcpy((void*)json, (void*)json_original, json_length);

        int ret = dmScript::JsonToLua(L, json, json_length);
        ASSERT_EQ(1, ret);
        lua_pop(L, 1);
        free((void*)json);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

// Decoding must respect the caller's byte range and leave its input unchanged.
TEST_F(ScriptJsonTest, TestJsonReadOnlyBoundedStrings)
{
    const char source[] = "[\"plain\",\"a\\u0000b\",\"\\ud83d\\ude00\"]";
    size_t size = sizeof(source) - 1;
    char* text = (char*)malloc(size);
    memcpy(text, source, size);
    int top = lua_gettop(L);
    ASSERT_EQ(1, dmScript::JsonToLua(L, text, size));
    ASSERT_EQ(0, memcmp(text, source, size));
    lua_rawgeti(L, -1, 2);
    size_t length;
    const char* value = lua_tolstring(L, -1, &length);
    ASSERT_EQ(3u, length);
    ASSERT_EQ(0, memcmp(value, "a\0b", length));
    lua_settop(L, top);
    free(text);
}

// Legacy fallback must reject malformed arrays without double-freeing its scratch buffer.
TEST_F(ScriptJsonTest, TestJsonMalformedBoundedArray)
{
    const char source[] = "[\"\\ud800\"]";
    size_t size = sizeof(source) - 1;
    char* text = (char*)malloc(size);
    memcpy(text, source, size);
    int top = lua_gettop(L);
    ASSERT_EQ(0, dmScript::JsonToLua(L, text, size));
    ASSERT_EQ(top, lua_gettop(L));
    ASSERT_EQ(0, memcmp(text, source, size));
    free(text);
}

TEST_F(ScriptJsonTest, TestJsonToLua_Issue10304)
{
    int top = lua_gettop(L);

    {
        const char* json_original = "xxxx";
        size_t json_length = 4;
        // Make it fully dynamic so that ASAN can catch it
        const char* json = (const char*)malloc(json_length);
        memcpy((void*)json, (void*)json_original, json_length);

        int ret = dmScript::JsonToLua(L, json, json_length);
        ASSERT_EQ(0, ret);
        int newtop = lua_gettop(L);
        ASSERT_EQ(0, newtop - top);
        free((void*)json);
    }

    ASSERT_EQ(top, lua_gettop(L));
}


TEST_F(ScriptJsonTest, TestLuaToJson)
{
    int top = lua_gettop(L);

    {
        lua_newtable(L);
            lua_pushstring(L, "str");
            lua_setfield(L, -2, "a");
            lua_pushnumber(L, 1.5);
            lua_setfield(L, -2, "b");

            lua_newtable(L);
                lua_pushinteger(L, 7);
                lua_setfield(L, -2, "d");

            lua_setfield(L, -2, "c");

        char* json = 0;
        size_t json_length;
        int ret = dmScript::LuaToJson(L, &json, &json_length);
        ASSERT_EQ(1, ret);

        ASSERT_EQ(31u, (uint32_t)json_length);
        // Since the ordering will be different
        // {"a":"str","b":1.5,"c":{"d":7}}
        ASSERT_EQ('{', json[0]);
        ASSERT_EQ('}', json[json_length-1]);
        ASSERT_EQ('\0', json[json_length]);
        ASSERT_TRUE(strstr(json, "\"a\":\"str\"") != 0);
        ASSERT_TRUE(strstr(json, "\"b\":1.5") != 0);
        ASSERT_TRUE(strstr(json, "\"c\":{\"d\":7}") != 0);

        lua_pop(L, 1);
        free((void*)json);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptJsonTest, TestLuaToJsonWithStackIndex)
{
    int top = lua_gettop(L);

    {
        lua_newtable(L);
            lua_pushstring(L, "first");
            lua_setfield(L, -2, "name");

        lua_newtable(L);
            lua_pushstring(L, "second");
            lua_setfield(L, -2, "name");
            lua_newtable(L);
            lua_setfield(L, -2, "empty");

        lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "encode_empty_table_as_object");

        char* json = 0;
        size_t json_length;
        int ret = dmScript::LuaToJson(L, 2, 3, &json, &json_length);
        ASSERT_EQ(1, ret);
        ASSERT_TRUE(strstr(json, "\"name\":\"second\"") != 0);
        ASSERT_TRUE(strstr(json, "\"empty\":[]") != 0);
        free((void*)json);

        json = 0;
        json_length = 0;
        ret = dmScript::LuaToJson(L, 1, 3, &json, &json_length);
        ASSERT_EQ(1, ret);
        ASSERT_EQ(16u, (uint32_t)json_length);
        ASSERT_STREQ("{\"name\":\"first\"}", json);
        free((void*)json);

        lua_pop(L, 3);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(ScriptJsonTest, TestLuaToJsonWithOptionsIndex)
{
    int top = lua_gettop(L);

    {
        lua_newtable(L);
            lua_newtable(L);
            lua_setfield(L, -2, "empty");

        lua_newtable(L);
            lua_pushboolean(L, 0);
            lua_setfield(L, -2, "encode_empty_table_as_object");

        char* json = 0;
        size_t json_length = 0;
        int ret = dmScript::LuaToJson(L, 1, 2, &json, &json_length);
        ASSERT_EQ(1, ret);
        ASSERT_EQ(12u, (uint32_t)json_length);
        ASSERT_STREQ("{\"empty\":[]}", json);
        free((void*)json);

        json = 0;
        json_length = 0;
        ret = dmScript::LuaToJson(L, 1, 3, &json, &json_length);
        ASSERT_EQ(1, ret);
        ASSERT_EQ(12u, (uint32_t)json_length);
        ASSERT_STREQ("{\"empty\":{}}", json);
        free((void*)json);

        lua_pop(L, 2);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

// Encoding must preserve negative stack indices and return an owned, terminated buffer even for empty output.
TEST_F(ScriptJsonTest, TestLuaToJsonNegativeIndexAndEmptyOutput)
{
    int top = lua_gettop(L);
    lua_pushstring(L, "x\ny");
    lua_newtable(L);
    char* json = 0;
    size_t length = 0;
    ASSERT_EQ(1, dmScript::LuaToJson(L, -2, -1, &json, &length));
    ASSERT_STREQ("\"x\\ny\"", json);
    ASSERT_EQ(6u, length);
    ASSERT_EQ(top + 2, lua_gettop(L));
    free(json);
    lua_settop(L, top);

    int token;
    lua_pushlightuserdata(L, &token);
    ASSERT_EQ(1, dmScript::LuaToJson(L, -1, 0, &json, &length));
    ASSERT_EQ(0u, length);
    ASSERT_EQ('\0', json[0]);
    ASSERT_EQ(top + 1, lua_gettop(L));
    free(json);
    lua_settop(L, top);
}

// Legacy hexadecimal inputs must round correctly and never read beyond the supplied byte range.
TEST_F(ScriptJsonTest, TestJsonLegacyNumbersBounded)
{
    const struct
    {
        const char* m_Text;
        uint64_t    m_Bits;
    } cases[] = {
        { "0x1p-1074", UINT64_C(1) },
        { "-0x1p-1075", UINT64_C(0x8000000000000000) },
        { "0x1.00000000000008p0", UINT64_C(0x3ff0000000000000) },
        { "0x1.00000000000008001p0", UINT64_C(0x3ff0000000000001) },
        { "0x1.fffffffffffff8p1023", UINT64_C(0x7ff0000000000000) },
        { "0x1.fffffffffffff7fffffffffp1023", UINT64_C(0x7fefffffffffffff) },
        { "0x0.fffffffffffff8p-1022", UINT64_C(0x0010000000000000) },
        { "0x0.fffffffffffff80001p-1022", UINT64_C(0x0010000000000000) },
    };
    int top = lua_gettop(L);
    for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        size_t size = strlen(cases[i].m_Text);
        char* text = (char*)malloc(size);
        memcpy(text, cases[i].m_Text, size);
        int accepted = dmScript::JsonToLua(L, text, size);
        free(text);
        EXPECT_EQ(1, accepted);
        if (accepted)
        {
            double value = lua_tonumber(L, -1);
            uint64_t bits;
            memcpy(&bits, &value, sizeof(bits));
            EXPECT_EQ(cases[i].m_Bits, bits);
        }
        lua_settop(L, top);
    }
}

// Encoding must use fourteen-digit nearest rounding and valid JSON in every supported rounding mode.
TEST_F(ScriptJsonTest, TestJsonEncodeDecimalRounding)
{
    int top = lua_gettop(L);
    dmJson::Document document(1);
    int previous_rounding = fegetround();
    char* previous_locale = strdup(setlocale(LC_NUMERIC, 0));
    setlocale(LC_NUMERIC, "C");
    const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
    const double boundaries[] = {
        0.0, 1e-5, 1e-4, 0.001, 0.1, 1.0,
        // Exercise digit-group boundaries and zeros inside a coefficient.
        9.0, 10.0, 99.0, 100.0, 101.0, 999.0, 1000.0, 1001.0,
        10000.0, 10001.0, 99999999999999.0,
        1.010101010101, 10.00100010001, 0.0001000100010001,
        9.99999999999995,
        99.9999999999995, 99999999999999.5, 1e14, 1e300
    };
    for (uint32_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode)
    {
        // Emscripten can report success without changing the rounding mode.
        if (fesetround(modes[mode]) || fegetround() != modes[mode])
        {
            continue;
        }
        for (uint32_t i = 0; i < 18 * 10 + sizeof(boundaries) / sizeof(boundaries[0]); ++i)
        {
            double value = i < 180 ? (10000000000000.5 + (i % 10) * 9999999999999.0) / pow(10.0, i / 10) : boundaries[i - 180];
            const double neighbors[] = { nextafter(value, 0.0), value, nextafter(value, HUGE_VAL) };
            for (uint32_t neighbor = 0; neighbor < 3; ++neighbor)
            {
                for (uint32_t negative = 0; negative < 2; ++negative)
                {
                    double number = negative ? -neighbors[neighbor] : neighbors[neighbor];
                    char expected[64];
                    fesetround(FE_TONEAREST);
                    dmSnPrintf(expected, sizeof(expected), "%.14g", number);
                    fesetround(modes[mode]);
                    lua_pushnumber(L, number);
                    char* text = 0;
                    size_t size = 0;
                    EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
                    EXPECT_EQ(modes[mode], fegetround());
                    EXPECT_STREQ(expected, text);
                    EXPECT_TRUE(document.Parse(text, (uint32_t)size, 0, dmJson::PARSE_PRESERVE_VALUES));
                    free(text);
                    lua_settop(L, top);
                }
            }
        }
    }
    fesetround(previous_rounding);
    setlocale(LC_NUMERIC, previous_locale);
    free(previous_locale);
}

// Numbers appended near an output-buffer boundary must preserve every byte
// and the terminator when the Lua adapter switches between direct and scratch output.
TEST_F(ScriptJsonTest, TestJsonEncodeNumberBufferBoundaries)
{
    char prefix[129];
    memset(prefix, 'a', sizeof(prefix));
    const double numbers[] = { 1.0, -0.0, 1.010101010101, -1.25e-300, 1e300 };
    int top = lua_gettop(L);
    int previous_rounding = fegetround();
    char* previous_locale = strdup(setlocale(LC_NUMERIC, 0));
    setlocale(LC_NUMERIC, "C");
    fesetround(FE_TONEAREST);
    for (uint32_t length = 0; length <= sizeof(prefix); ++length)
    {
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(numbers); ++i)
        {
            lua_newtable(L);
            lua_pushlstring(L, prefix, length);
            lua_rawseti(L, -2, 1);
            lua_pushnumber(L, numbers[i]);
            lua_rawseti(L, -2, 2);
            char expected[256];
            dmSnPrintf(expected, sizeof(expected), "[\"%.*s\",%.14g]", (int)length, prefix, numbers[i]);
            char* text = 0;
            size_t size = 0;
            EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
            EXPECT_EQ(strlen(expected), size);
            EXPECT_STREQ(expected, text);
            EXPECT_EQ('\0', text[size]);
            free(text);
            lua_settop(L, top);
        }
    }
    fesetround(previous_rounding);
    setlocale(LC_NUMERIC, previous_locale);
    free(previous_locale);
}

// Every finite exponent, including subnormals, must retain the fourteen-digit
// representation and preserve each supported caller rounding mode.
TEST_F(ScriptJsonTest, TestJsonEncodeBinary64Range)
{
    int top = lua_gettop(L);
    int previous_rounding = fegetround();
    char* previous_locale = strdup(setlocale(LC_NUMERIC, 0));
    setlocale(LC_NUMERIC, "C");
    const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
    const uint64_t fractions[] = {
        UINT64_C(0), UINT64_C(1), UINT64_C(0x123456789abcd),
        UINT64_C(0x8000000000000), UINT64_C(0xfffffffffffff)
    };
    for (uint32_t mode = 0; mode < DM_ARRAY_SIZE(modes); ++mode)
    {
        if (fesetround(modes[mode]) || fegetround() != modes[mode])
        {
            continue;
        }
        for (uint32_t exponent = 0; exponent < 2047; ++exponent)
        {
            for (uint32_t fraction = 0; fraction < DM_ARRAY_SIZE(fractions); ++fraction)
            {
                for (uint32_t negative = 0; negative < 2; ++negative)
                {
                    uint64_t bits = ((uint64_t)negative << 63) | ((uint64_t)exponent << 52) | fractions[fraction];
                    double number;
                    memcpy(&number, &bits, sizeof(number));
                    char expected[64];
                    fesetround(FE_TONEAREST);
                    dmSnPrintf(expected, sizeof(expected), "%.14g", number);
                    fesetround(modes[mode]);
                    lua_pushnumber(L, number);
                    char* text = 0;
                    size_t size = 0;
                    EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
                    EXPECT_EQ(modes[mode], fegetround());
                    EXPECT_STREQ(expected, text);
                    free(text);
                    lua_settop(L, top);
                }
            }
        }
    }
    fesetround(previous_rounding);
    setlocale(LC_NUMERIC, previous_locale);
    free(previous_locale);
}

// Decimal halfway values must round to even with canonical trailing-zero
// trimming, including the scientific cases where some printf implementations differ.
TEST_F(ScriptJsonTest, TestJsonEncodeCanonicalMidpoints)
{
    const struct
    {
        double      m_Number;
        const char* m_Expected;
    } cases[] = {
        { 100000000000005.0, "1e+14" },
        { 100000000000015.0, "1.0000000000002e+14" },
        { 100000000000025.0, "1.0000000000002e+14" },
        { 100000000000035.0, "1.0000000000004e+14" },
        { 999999999999995.0, "1e+15" },
    };
    int top = lua_gettop(L);
    int previous_rounding = fegetround();
    const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
    for (uint32_t mode = 0; mode < DM_ARRAY_SIZE(modes); ++mode)
    {
        if (fesetround(modes[mode]) || fegetround() != modes[mode])
        {
            continue;
        }
        for (uint32_t i = 0; i < DM_ARRAY_SIZE(cases); ++i)
        {
            for (uint32_t negative = 0; negative < 2; ++negative)
            {
                char expected[64];
                dmSnPrintf(expected, sizeof(expected), "%s%s", negative ? "-" : "", cases[i].m_Expected);
                lua_pushnumber(L, negative ? -cases[i].m_Number : cases[i].m_Number);
                char* text = 0;
                size_t size = 0;
                EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
                EXPECT_EQ(modes[mode], fegetround());
                EXPECT_STREQ(expected, text);
                free(text);
                lua_settop(L, top);
            }
        }
    }
    fesetround(previous_rounding);
}

// Android's printf emitted ':' at these boundaries; encoding must use fixed expected results and restore rounding after errors.
TEST_F(ScriptJsonTest, TestJsonEncodeRoundingMode)
{
    const struct
    {
        uint64_t    m_Bits;
        const char* m_Expected;
    } cases[] = {
        { UINT64_C(0x3f50624dd2f1a9fb), "0.001" },
        { UINT64_C(0xbf50624dd2f1a9fb), "-0.001" },
        { UINT64_C(0x3fefffffffffffff), "1" },
        { UINT64_C(0xbfefffffffffffff), "-1" },
        { UINT64_C(0x42d6bcc41e8fffff), "1e+14" },
        { UINT64_C(0xc2d6bcc41e8fffff), "-1e+14" },
        { UINT64_C(0x7e37e43c8800759b), "1e+300" },
        { UINT64_C(0xfe37e43c8800759b), "-1e+300" },
    };
    int top = lua_gettop(L);
    int previous_rounding = fegetround();
    const int modes[] = { FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO };
    for (uint32_t mode = 0; mode < sizeof(modes) / sizeof(modes[0]); ++mode)
    {
        // Emscripten can report success without changing the rounding mode.
        if (fesetround(modes[mode]) || fegetround() != modes[mode])
        {
            continue;
        }
        for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        {
            double number;
            memcpy(&number, &cases[i].m_Bits, sizeof(number));
            lua_pushnumber(L, number);
            char* text = 0;
            size_t size = 0;
            EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
            EXPECT_EQ(modes[mode], fegetround());
            EXPECT_STREQ(cases[i].m_Expected, text);
            free(text);
            lua_settop(L, top);
        }
        EXPECT_TRUE(RunString(L, "assert(not pcall(json.encode, {1.25e100, math.huge}))"));
        EXPECT_EQ(modes[mode], fegetround());
        lua_settop(L, top);
    }
    fesetround(previous_rounding);
}

// Locale changes must not change JSON's decimal point or consume value separators (issue #10130).
TEST_F(ScriptJsonTest, TestJsonLocaleChanges)
{
    char* previous = strdup(setlocale(LC_NUMERIC, 0));
    const char* locales[] = { "C", "sv_SE.UTF-8", "C", "de_DE.UTF-8", "nl_NL.UTF-8", "Dutch_Netherlands.1252", "NL", "C" };
    int top = lua_gettop(L);
    for (uint32_t i = 0; i < sizeof(locales) / sizeof(locales[0]); ++i)
    {
        if (!setlocale(LC_NUMERIC, locales[i]))
        {
            continue;
        }
        lua_pushnumber(L, -1.25e-100);
        char* text = 0;
        size_t size = 0;
        EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
        EXPECT_STREQ("-1.25e-100", text);
        free(text);
        lua_settop(L, top);
        const char* input = "[1.25,0x1.8p2]";
        int accepted = dmScript::JsonToLua(L, input, strlen(input));
        EXPECT_EQ(1, accepted);
        if (accepted)
        {
            lua_rawgeti(L, -1, 1);
            EXPECT_EQ(1.25, lua_tonumber(L, -1));
            lua_pop(L, 1);
            lua_rawgeti(L, -1, 2);
            EXPECT_EQ(6.0, lua_tonumber(L, -1));
        }
        lua_settop(L, top);
        lua_newtable(L);
        lua_pushnumber(L, 1.2);
        lua_rawseti(L, -2, 1);
        text = 0;
        EXPECT_EQ(1, dmScript::LuaToJson(L, -1, 0, &text, &size));
        EXPECT_STREQ("[1.2]", text);
        free(text);
        lua_settop(L, top);
        input = "{\"number\":1,\"foo\":\"hello_world\",\"fraction\":1.2}";
        accepted = dmScript::JsonToLua(L, input, strlen(input));
        EXPECT_EQ(1, accepted);
        if (accepted)
        {
            lua_getfield(L, -1, "number");
            EXPECT_EQ(1.0, lua_tonumber(L, -1));
            lua_pop(L, 1);
            lua_getfield(L, -1, "foo");
            EXPECT_STREQ("hello_world", lua_tostring(L, -1));
            lua_pop(L, 1);
            lua_getfield(L, -1, "fraction");
            EXPECT_EQ(1.2, lua_tonumber(L, -1));
        }
        lua_settop(L, top);
    }
    setlocale(LC_NUMERIC, previous);
    free(previous);
}

// Preserve the platform's existing acceptance of punctuation in NaN payloads.
TEST_F(ScriptJsonTest, TestJsonNaNPayload)
{
    const char* input = "nan(a-b)";
    int accepted = dmScript::JsonToLua(L, input, strlen(input));
#if defined(__APPLE__)
    ASSERT_EQ(1, accepted);
    double value = lua_tonumber(L, -1);
    ASSERT_TRUE(value != value);
#else
    ASSERT_EQ(0, accepted);
#endif
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);

    int ret = jc_test_run_all();
    dmLog::LogFinalize();
    return ret;
}
