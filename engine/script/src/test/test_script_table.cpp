// Copyright 2020-2024 The Defold Foundation
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

#include <stdlib.h>

#include <testmain/testmain.h>
#include <dlib/align.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/memory.h>

#include "../script.h"
#include "test_script.h"
#include "test_script_private.h"
#include "test/test_ddf.h"

#include "data/table_cos_v0.dat.embed.h"
#include "data/table_sin_v0.dat.embed.h"
#include "data/table_cos_v1.dat.embed.h"
#include "data/table_sin_v1.dat.embed.h"
#include "data/table_v818192.dat.embed.h"
#include "data/table_tstring_v1.dat.embed.h"
#include "data/table_tstring_v2.dat.embed.h"
#include "data/table_tstring_v3.dat.embed.h"

class LuaTableTest* g_LuaTableTest = 0;

char DM_ALIGNED(16) g_Buf[256];

class LuaTableTest : public dmScriptTest::ScriptTest
{
};

TEST_F(LuaTableTest, EmptyTable)
{
    lua_newtable(L);
    char DM_ALIGNED(16) buf[8 + 4];
    uint32_t buffer_used = dmScript::CheckTable(L, buf, sizeof(buf), -1);
    // 4 bytes for count
    ASSERT_EQ(12U, buffer_used);
    lua_pop(L, 1);
}

/**
 * A helper function used when validating serialized data in original or v1 format.
 */
typedef double (*TableGenFunc)(double);
static int ReadSerializedTable(lua_State* L, uint8_t* source, uint32_t source_length, TableGenFunc fn, int key_stride)
{
    int error = 0;
    const double epsilon = 1.0e-7f;
    char* aligned_buf = (char*)source;

    dmScript::PushTable(L, aligned_buf, source_length);

    for (uint32_t i=0; i<0xfff; ++i)
    {
        lua_pushnumber(L, i * key_stride);
        lua_gettable(L, -2);
        EXPECT_EQ(LUA_TNUMBER, lua_type(L, -1));
        if  (LUA_TNUMBER != lua_type(L, -1))
        {
            printf("Invalid key on row %d\n", i);
        }
        double value_read = lua_tonumber(L, -1);
        double value_expected = fn(2.0 * M_PI * (double)i / (double)0xffff);
        double diff = fabs(value_read - value_expected);
        EXPECT_GT(epsilon, diff);
        lua_pop(L, 1);

        if (epsilon < diff)
        {
            error = 1;
            break;
        }
    }

    lua_pop(L, 1);

    return error;
}

static int ReadUnsupportedVersion(lua_State* L)
{
    dmScript::PushTable(L, (const char*)TABLE_V818192_DAT, TABLE_V818192_DAT_SIZE);
    return 1;
}


// The v0.0 tables were generated with dense keys.
static int ReadCosTableDataOriginal(lua_State* L)
{
    return ReadSerializedTable(L, TABLE_COS_V0_DAT, TABLE_COS_V0_DAT_SIZE, cos, 1);
}

static int ReadSinTableDataOriginal(lua_State* L)
{
    return ReadSerializedTable(L, TABLE_SIN_V0_DAT, TABLE_SIN_V0_DAT_SIZE, sin, 1);
}

TEST_F(LuaTableTest, AttemptReadUnsupportedVersion)
{
    int result = lua_cpcall(L, ReadUnsupportedVersion, 0x0);
    ASSERT_NE(0, result);
    char str[256];
    dmSnPrintf(str, sizeof(str), "Unsupported serialized table data: version = 0x%x (current = 0x%x)", 818192, 4);
    ASSERT_STREQ(str, lua_tostring(L, -1));
    // pop error message
    lua_pop(L, 1);
}

TEST_F(LuaTableTest, VerifyCosTableOriginal)
{
    int result = lua_cpcall(L, ReadCosTableDataOriginal, 0x0);
    ASSERT_EQ(0, result);
}

TEST_F(LuaTableTest, VerifySinTableOriginal)
{
    int result = lua_cpcall(L, ReadSinTableDataOriginal, 0x0);
    ASSERT_EQ(0, result);
}


// The v1 tables were generated with sparse keys: every other integer over the defined range.
int ReadCosTableDataVersion01(lua_State* L)
{
    return ReadSerializedTable(L, TABLE_COS_V1_DAT, TABLE_COS_V1_DAT_SIZE, cos, 2);
}


int ReadSinTableDataVersion01(lua_State* L)
{
    return ReadSerializedTable(L, TABLE_SIN_V1_DAT, TABLE_SIN_V1_DAT_SIZE, sin, 2);
}

TEST_F(LuaTableTest, VerifyCosTable01)
{
    int result = lua_cpcall(L, ReadCosTableDataVersion01, 0x0);
    ASSERT_EQ(0, result);
}

TEST_F(LuaTableTest, VerifySinTable01)
{
    int result = lua_cpcall(L, ReadSinTableDataVersion01, 0x0);
    ASSERT_EQ(0, result);
}

TEST_F(LuaTableTest, TestSerializeLargeNumbers)
{
    lua_Number numbers[] = {0, -1, -4294967295, 4294967295, 0x1234, 0x8765, 0xffff, 0x12345678, 0x7fffffff, 0x87654321, 268435456, 0xffffffff, 0xfffffffe};
    const uint32_t count = sizeof(numbers) / sizeof(lua_Number);

    lua_newtable(L);
    for (uint32_t i=0;i!=count;i++)
    {
        // same key & value
        lua_pushnumber(L, numbers[i]);
        lua_pushnumber(L, numbers[i]);
        lua_settable(L, -3);
    }

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_Number found[count] = { 0 };

    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        lua_Number value = lua_tonumber(L, -1);
        lua_Number key = lua_tonumber(L, -2);
        ASSERT_EQ(key, value);

        for (uint32_t i=0;i!=count;i++)
        {
            if (key == numbers[i])
                found[i]++;
        }

        lua_pop(L, 1);
    }

    for (uint32_t i=0;i!=count;i++)
        ASSERT_EQ(found[i], 1);

    lua_pop(L, 1);
}

const uint32_t IOOB_BUFFER_SIZE = 8 + 2 + 2 + (sizeof(char) + sizeof(char) + 5 * sizeof(char) + sizeof(lua_Number));

static int ProduceIndexOutOfBounds(lua_State *L)
{
    char DM_ALIGNED(16) buf[IOOB_BUFFER_SIZE];
    lua_newtable(L);
    // invalid key
    lua_pushnumber(L, 0xffffffffLL+1);
    // value
    lua_pushnumber(L, 0);
    // store pair
    lua_settable(L, -3);
    uint32_t buffer_used = dmScript::CheckTable(L, buf, IOOB_BUFFER_SIZE, -1);
    // expect it to fail, avoid warning
    (void)buffer_used;
    return 1;
}

TEST_F(LuaTableTest, IndexOutOfBounds)
{
    int result = lua_cpcall(L, ProduceIndexOutOfBounds, 0x0);
    // 2 bytes for count
    ASSERT_NE(0, result);
    char expected_error[64];
    dmSnPrintf(expected_error, 64, "index out of bounds, max is %d", 0xffffffff);
    ASSERT_STREQ(expected_error, lua_tostring(L, -1));
    // pop error message
    lua_pop(L, 1);
}

static int LuaCheckTable(lua_State* L) {
  int index = 1;
  char* buf = (char*) lua_topointer(L, 2);
  int buffer_size = (int) lua_tonumber(L, 3);
  dmScript::CheckTable(L, buf, buffer_size, index);
  return 0;
}

#define abs_index(L, i) ((i) > 0 || (i) <= LUA_REGISTRYINDEX ? (i) : \
             lua_gettop(L) + (i) + 1)

static int PCallCheckTable(lua_State* L, char* buffer, uint32_t buffer_size, int index, bool do_log)
{
  index = abs_index(L, index);

  int oldtop = lua_gettop(L);
  lua_pushcfunction(L, LuaCheckTable);
  lua_pushvalue(L, index); // add table again...
  lua_pushlightuserdata(L, buffer);
  lua_pushnumber(L, buffer_size);
  int result = lua_pcall(L, 3, 0, 0);
  if (result != 0) {
    if (do_log)
        printf("error %s\n", lua_tostring(L, oldtop + 1));
    lua_pop(L, 1);
  }
  assert(lua_gettop(L) == oldtop);
  return result;
}

TEST_F(LuaTableTest, Table01)
{
    // Create table
    lua_newtable(L);
    lua_pushinteger(L, 123);
    lua_setfield(L, -2, "a");

    lua_pushinteger(L, 456);
    lua_setfield(L, -2, "b");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "a");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(123, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "b");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(456, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);

    // Create table again
    lua_newtable(L);
    lua_pushinteger(L, 123);
    lua_setfield(L, -2, "a");
    lua_pushinteger(L, 456);
    lua_setfield(L, -2, "b");

    int result = PCallCheckTable(L, g_Buf, buffer_used-1, -1, true);

    ASSERT_NE(result, 0);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Table02)
{
    // Create table
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "foo");

    lua_pushstring(L, "kalle");
    lua_setfield(L, -2, "foo2");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "foo");
    ASSERT_EQ(LUA_TBOOLEAN, lua_type(L, -1));
    ASSERT_EQ(1, lua_toboolean(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "foo2");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("kalle", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);

    // Create table again
    lua_newtable(L);
    lua_pushboolean(L, 1);
    lua_setfield(L, -2, "foo");

    lua_pushstring(L, "kalle");
    lua_setfield(L, -2, "foo2");

    int result = PCallCheckTable(L, g_Buf, buffer_used-1, -1, true);

    ASSERT_NE(result, 0);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, case1308)
{
    // Create table
    lua_newtable(L);
    lua_pushstring(L, "ab");
    lua_setfield(L, -2, "a");

    lua_newtable(L);
    lua_pushinteger(L, 123);
    lua_setfield(L, -2, "x");

    lua_setfield(L, -2, "t");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "a");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("ab", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "t");
    ASSERT_EQ(LUA_TTABLE, lua_type(L, -1));
    lua_getfield(L, -1, "x");
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(123, lua_tonumber(L, -1));
    lua_pop(L, 1);
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, NestedTableSizeCheck)
{
    /**
     * This table structure was guaranteed to crash on frist version of #6676
     * Fix in https://github.com/defold/defold/pull/6991
     *
     * {
     *   foo = 1234,
     *   b = {
     *       a = 1234,
     *   }
     * }
     *
     */
    // create outer table
    lua_newtable(L);

    // foo = 1234
    lua_pushnumber(L, 1234);
    lua_setfield(L, -2, "foo");

    // create inner table "b"
    lua_newtable(L);

    // a = 1234
    lua_pushnumber(L, 1234);
    lua_setfield(L, -2, "a");

    // b = {}
    lua_setfield(L, -2, "b");

    uint32_t calculated_table_size = dmScript::CheckTableSize(L, -1);
    uint32_t actual_table_size = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);

    lua_pop(L, 1);

    ASSERT_EQ(calculated_table_size, actual_table_size);
}

static int g_CustomPanicFunctionCalled = 0;
static int CustomPanicFn(lua_State* L)
{
    fprintf(stderr, "PANIC: unprotected error in call to Lua API (%s) (custom CustomPanicFn)\n", lua_tostring(L, -1));
    return ++g_CustomPanicFunctionCalled; // In our case, we pass this to the longjmp, and we check it with setjmp()
}

static void SetupCyclicTable(lua_State* L)
{
    /**
     *  local x1 = {}
     *  local x2 = {}
     *  x1.x2 = x2
     *  x2.x1 = x1
     */

    lua_newtable(L); // x1
    // -1: x1

    lua_newtable(L); // x2
    // -2: x1
    // -1: x2

    lua_pushvalue(L, -2);
    // -3: x1
    // -2: x2
    // -1: x1

    lua_setfield(L, -2, "x1"); // x2.x1 = x1
    // -2: x1
    // -1: x2

    lua_setfield(L, -2, "x2"); // x1.x2 = x2
    // -1: x1
}

TEST_F(LuaTableTest, CyclicTable_CheckTableSize)
{
    g_CustomPanicFunctionCalled = 0;
    SetupCyclicTable(L);

    int scope_top = lua_gettop(L);

    printf("\nExpected error begin -->\n");
#if !defined(_WIN32)
    {
        static int count = 0;
        int jmpval;
        DM_SCRIPT_TEST_PANIC_SCOPE(L, CustomPanicFn, jmpval);
        ++count;

        if (jmpval == 0)
        {
            dmScript::CheckTableSize(L, -1);
            ASSERT_FALSE(true && "We shouldn't get here");
        }

        ASSERT_EQ(2, count);
    }
#else
    try {
        dmScript::CheckTableSize(L, -1);
        ASSERT_FALSE(true && "We shouldn't get here");
    } catch (...)
    {
        g_CustomPanicFunctionCalled = 1;
    }
#endif
    printf("<-- Expected error end\n");

    ASSERT_EQ(1, g_CustomPanicFunctionCalled);
    lua_pop(L, lua_gettop(L) - scope_top); // Pop any items that the function put on the stack
    lua_pop(L, 1);
}

TEST_F(LuaTableTest, CyclicTable_CheckTable)
{
    g_CustomPanicFunctionCalled = 0;
    SetupCyclicTable(L);

    int scope_top = lua_gettop(L);

    printf("\nExpected error begin -->\n");
#if !defined(_WIN32)
    {
        static int count = 0;
        int jmpval;
        DM_SCRIPT_TEST_PANIC_SCOPE(L, CustomPanicFn, jmpval);
        ++count;

        if (jmpval == 0)
        {
            dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
            ASSERT_FALSE(true && "We shouldn't get here");
        }

        ASSERT_EQ(2, count);
    }
#else
    try {
        dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
        ASSERT_FALSE(true && "We shouldn't get here");
    } catch (...)
    {
        g_CustomPanicFunctionCalled = 1;
    }
#endif
    printf("<-- Expected error end\n");

    ASSERT_EQ(1, g_CustomPanicFunctionCalled);
    lua_pop(L, lua_gettop(L) - scope_top); // Pop any items that the function put on the stack
    lua_pop(L, 1);
}


// TODO!!
// TEST_F(LuaTableTest, ReadTruncatedFile)
// {
//     char buf[TABLE_TSTRING_V1_DAT_SIZE/2];
//     memcpy(buf, sizeof(buf), TABLE_TSTRING_V1_DAT);
//     dmScript::PushTable(L, (const char*)buf, sizeof(buf));
// }

// NOTE: This test is here to allow for generating an old test file
// in order to test upgrades of the table file format.
// E.g. "git checkout 1.2.113" and run this test
// Once done, copy the file and add it to the repo / wscript so we can test against it
// TEST_F(LuaTableTest, TSTRING_GenerateData)
// {
//     int top = lua_gettop(L);

//     ASSERT_TRUE(RunString(L, "TEST_PATH = 'src/test/data/table_tstring_v2.dat'"));
//     ASSERT_TRUE(RunString(L, "TEST_WRITE = 1"));
//     ASSERT_TRUE(RunString(L, "TABLE_VERSION_CURRENT = 2"));
//     ASSERT_TRUE(RunFile(L, "test_script_table.luac"));
//     ASSERT_EQ(top, lua_gettop(L));
// }


TEST_F(LuaTableTest, Verify_TSTRING_V1) // old tostring/pushstring format
{
    dmScript::PushTable(L, (const char*)TABLE_TSTRING_V1_DAT, TABLE_TSTRING_V1_DAT_SIZE);
    lua_setglobal(L, "t");

    ASSERT_TRUE(RunString(L, " \
        --assert( t['binary\\0string'] == 'payload\\1\\0\\2\\3' ) \
        assert( t['binary'] == 'payload\\1' ) \
        assert( t['vector3'] == vmath.vector3(1,2,3) ) \
        assert( t['vector4'] == vmath.vector4(4, 5, 6, 7) ) \
        assert( t['quat'] == vmath.quat(1,2,3,4) ) \
        assert( t['number'] == 0.5 ) \
        assert( t['hash'] == hash('hashed value') ) \
        assert( t['url'] == msg.url('a', 'b', 'c') ) \
    "));
}

TEST_F(LuaTableTest, Verify_TSTRING_V2) // tolstring / pushlstring
{
    dmScript::PushTable(L, (const char*)TABLE_TSTRING_V2_DAT, TABLE_TSTRING_V2_DAT_SIZE);
    lua_setglobal(L, "t");

    ASSERT_TRUE(RunString(L, " \
        assert( t['binary\\0string'] == 'payload\\1\\0\\2\\3' ) \
        assert( t['vector3'] == vmath.vector3(1,2,3) ) \
        assert( t['vector4'] == vmath.vector4(4, 5, 6, 7) ) \
        assert( t['quat'] == vmath.quat(1,2,3,4) ) \
        assert( t['number'] == 0.5 ) \
        assert( t['hash'] == hash('hashed value') ) \
        assert( t['url'] == msg.url('a', 'b', 'c') ) \
    "));
}

TEST_F(LuaTableTest, Verify_TSTRING_V3) // tolstring / pushlstring
{
    dmScript::PushTable(L, (const char*)TABLE_TSTRING_V3_DAT, TABLE_TSTRING_V3_DAT_SIZE);
    lua_setglobal(L, "t");

    ASSERT_TRUE(RunString(L, " \
        assert( t['binary\\0string'] == 'payload\\1\\0\\2\\3' ) \
        assert( t['vector3'] == vmath.vector3(1,2,3) ) \
        assert( t['vector4'] == vmath.vector4(4, 5, 6, 7) ) \
        assert( t['quat'] == vmath.quat(1,2,3,4) ) \
        assert( t['number'] == 0.5 ) \
        assert( t['hash'] == hash('hashed value') ) \
        assert( t['url'] == msg.url('a', 'b', 'c') ) \
    "));
}

TEST_F(LuaTableTest, TSTRING) // binary strings (def2821)
{
    lua_newtable(L);

    lua_pushstring(L, "plain string");
    lua_setfield(L, -2, "key1");

    char binary_string[] = "\0binary\0string\0"; // 16 chars in total
    lua_pushlstring(L, binary_string, 16);
    lua_setfield(L, -2, "key2");

    uint8_t binary_string2[] = {0,1,2,3,4,5,6,7};
    lua_pushlstring(L, (const char*)binary_string2, 8);
    lua_setfield(L, -2, "key3");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "key1");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    ASSERT_STREQ("plain string", lua_tostring(L, -1));
    lua_pop(L, 1);

    lua_getfield(L, -1, "key2");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    size_t length = 0;
    const char* str = lua_tolstring(L, -1, &length);
    ASSERT_EQ(16, length);
    lua_pop(L, 1);

    lua_getfield(L, -1, "key3");
    ASSERT_EQ(LUA_TSTRING, lua_type(L, -1));
    length = 0;
    str = lua_tolstring(L, -1, &length);
    ASSERT_EQ(8, length);
    ASSERT_EQ( 0u, memcmp(binary_string2, str, 8) );
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Vector3)
{
    // Create table
    lua_newtable(L);
    dmScript::PushVector3(L, dmVMath::Vector3(1,2,3));
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "v");
    dmVMath::Vector3* v1 = dmScript::ToVector3(L, -1);
    ASSERT_EQ(1, v1->getX());
    ASSERT_EQ(2, v1->getY());
    ASSERT_EQ(3, v1->getZ());

    dmVMath::Vector3* v2 = dmScript::CheckVector3(L, -1);
    ASSERT_EQ(1, v2->getX());
    ASSERT_EQ(2, v2->getY());
    ASSERT_EQ(3, v2->getZ());
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Vector4)
{
    // Create table
    lua_newtable(L);
    dmScript::PushVector4(L, dmVMath::Vector4(1,2,3,4));
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "v");
    dmVMath::Vector4* v1 = dmScript::ToVector4(L, -1);
    ASSERT_EQ(1, v1->getX());
    ASSERT_EQ(2, v1->getY());
    ASSERT_EQ(3, v1->getZ());
    ASSERT_EQ(4, v1->getW());

    dmVMath::Vector4* v2 = dmScript::CheckVector4(L, -1);
    ASSERT_EQ(1, v2->getX());
    ASSERT_EQ(2, v2->getY());
    ASSERT_EQ(3, v2->getZ());
    ASSERT_EQ(4, v2->getW());
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Quat)
{
    // Create table
    lua_newtable(L);
    dmScript::PushQuat(L, dmVMath::Quat(1,2,3,4));
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "v");
    dmVMath::Quat* v1 = dmScript::ToQuat(L, -1);
    ASSERT_EQ(1, v1->getX());
    ASSERT_EQ(2, v1->getY());
    ASSERT_EQ(3, v1->getZ());
    ASSERT_EQ(4, v1->getW());

    dmVMath::Quat* v2 = dmScript::CheckQuat(L, -1);
    ASSERT_EQ(1, v2->getX());
    ASSERT_EQ(2, v2->getY());
    ASSERT_EQ(3, v2->getZ());
    ASSERT_EQ(4, v2->getW());
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Matrix4)
{
    // Create table
    lua_newtable(L);
    dmVMath::Matrix4 m;
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            m.setElem((float) i, (float) j, (float) (i * 4 + j));
    dmScript::PushMatrix4(L, m);
    lua_setfield(L, -2, "v");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "v");
    dmVMath::Matrix4* m1 = dmScript::ToMatrix4(L, -1);
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            ASSERT_EQ(i * 4 + j, m1->getElem(i, j));

    dmVMath::Matrix4* m2 = dmScript::CheckMatrix4(L, -1);
    for (uint32_t i = 0; i < 4; ++i)
        for (uint32_t j = 0; j < 4; ++j)
            ASSERT_EQ(i * 4 + j, m2->getElem(i, j));
    lua_pop(L, 1);

    lua_pop(L, 1);
}

TEST_F(LuaTableTest, Hash)
{
    int top = lua_gettop(L);

    // Create table
    lua_newtable(L);
    dmhash_t hash = dmHashString64("test");
    dmScript::PushHash(L, hash);
    lua_setfield(L, -2, "h");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "h");
    ASSERT_TRUE(dmScript::IsHash(L, -1));
    dmhash_t hash2 = dmScript::CheckHash(L, -1);
    ASSERT_EQ(hash, hash2);
    lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(LuaTableTest, URL)
{
    int top = lua_gettop(L);

    // Create table
    lua_newtable(L);
    dmMessage::URL url = dmMessage::URL();
    url.m_Socket = 1;
    url.m_Path = 2;
    url.m_Fragment = 3;
    dmScript::PushURL(L, url);
    lua_setfield(L, -2, "url");

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_getfield(L, -1, "url");
    ASSERT_TRUE(dmScript::IsURL(L, -1));
    dmMessage::URL url2 = *dmScript::CheckURL(L, -1);
    ASSERT_EQ(url.m_Socket, url2.m_Socket);
    ASSERT_EQ(url.m_Path, url2.m_Path);
    ASSERT_EQ(url.m_Fragment, url2.m_Fragment);
    lua_pop(L, 1);

    lua_pop(L, 1);

    ASSERT_EQ(top, lua_gettop(L));
}

TEST_F(LuaTableTest, MixedKeys)
{
    // Create table
    lua_newtable(L);

    lua_pushnumber(L, 1);
    lua_pushnumber(L, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "key1");
    lua_pushnumber(L, 3);
    lua_settable(L, -3);

    lua_pushnumber(L, 2);
    lua_pushnumber(L, 4);
    lua_settable(L, -3);

    lua_pushstring(L, "key2");
    lua_pushnumber(L, 5);
    lua_settable(L, -3);

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    dmScript::PushTable(L, g_Buf, sizeof(g_Buf));

    lua_pushnumber(L, 1);
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(2, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pushstring(L, "key1");
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(3, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pushnumber(L, 2);
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(4, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pushstring(L, "key2");
    lua_gettable(L, -2);
    ASSERT_EQ(LUA_TNUMBER, lua_type(L, -1));
    ASSERT_EQ(5, lua_tonumber(L, -1));
    lua_pop(L, 1);

    lua_pop(L, 1);
}

static int ParseTruncatedTable(lua_State* L)
{
    size_t buffer_len = 0;
    const char* buffer = lua_tolstring(L, -2, &buffer_len);
    int buffer_size = luaL_checknumber(L, -1);
    dmScript::PushTable(L, buffer, buffer_size);
    return 0;
}

TEST_F(LuaTableTest, CorruptedTables)
{
    int top = lua_gettop(L);

    // Create table
    lua_newtable(L);

    lua_pushnumber(L, 1);
    lua_pushnumber(L, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "key1");
    lua_pushnumber(L, 3);
    lua_settable(L, -3);

    lua_pushnumber(L, 2);
    lua_pushnumber(L, 4);
    lua_settable(L, -3);

    lua_pushstring(L, "key2");
    lua_pushnumber(L, 5);
    lua_settable(L, -3);

    // Make sure we write to the buffer so that Valgrind doesn't complain
    // at the lua_pushlstring below.
    memset(g_Buf, 0, sizeof(g_Buf));

    uint32_t buffer_used = dmScript::CheckTable(L, g_Buf, sizeof(g_Buf), -1);
    (void) buffer_used;
    lua_pop(L, 1);

    for (uint32_t i = buffer_used-1; i > 0; --i)
    {
        lua_pushcfunction(L, ParseTruncatedTable);
        lua_pushlstring(L, g_Buf, sizeof(g_Buf));
        lua_pushnumber(L, i);
        int res = lua_pcall(L, 2, 0, 0x0);
        ASSERT_EQ(LUA_ERRRUN, res);

        printf("Err: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }

    ASSERT_EQ(top, lua_gettop(L));
}

static void RandomString(char* s, int max_len)
{
    int n = rand() % max_len + 1;
    for (int i = 0; i < n; ++i)
    {
        (*s++) = (char)(rand() % 256);
    }
    *s = '\0';
}

#if defined(__GNUC__)
#define NO_INLINE __attribute__ ((noinline))
#elif defined(_MSC_VER)
#define NO_INLINE __declspec(noinline)
#else
#error "Unsupported compiler: cannot specify 'noinline'."
#endif

#undef NO_INLINE

TEST_F(LuaTableTest, Stress)
{
    for (int iter = 0; iter < 100; ++iter)
    {
        for (int buf_size = 0; buf_size < 256; ++buf_size)
        {
            int n = rand() % 15 + 1;

            lua_newtable(L);
            for (int i = 0; i < n; ++i)
            {
                int key_type = rand() % 2;
                if (key_type == 0)
                {
                    char key[12];
                    RandomString(key, 11);
                    lua_pushstring(L, key);
                }
                else if (key_type == 1)
                {
                    lua_pushnumber(L, rand() % (n + 1));
                }
                int value_type = rand() % 3;
                if (value_type == 0)
                {
                    lua_pushboolean(L, 1);
                }
                else if (value_type == 1)
                {
                    lua_pushnumber(L, 123);
                }
                else if (value_type == 2)
                {
                    char value[16];
                    RandomString(value, 15);
                    lua_pushstring(L, value);
                }

                lua_settable(L, -3);
            }

            // Add eight to ensure there is room for the header too.
            char* buf;
            dmMemory::AlignedMalloc((void**)&buf, 16, 8 + buf_size + 4); // add 4 extra at end for GUARD BYTES
            buf[8 + buf_size + 0] = 0xBA;
            buf[8 + buf_size + 1] = 0xAD;
            buf[8 + buf_size + 2] = 0xF0;
            buf[8 + buf_size + 3] = 0x0D;

            int result = PCallCheckTable(L, buf, buf_size, -1, false);

            if (result == 0) {
              dmScript::PushTable(L, buf, buf_size);
              lua_pop(L, 1);
            }

            uint8_t a = (uint8_t)0xBA;
            uint8_t b = (uint8_t)0xAD;
            uint8_t c = (uint8_t)0xF0;
            uint8_t d = (uint8_t)0x0D;
            uint8_t ra = (uint8_t)buf[8 + buf_size + 0];
            uint8_t rb = (uint8_t)buf[8 + buf_size + 1];
            uint8_t rc = (uint8_t)buf[8 + buf_size + 2];
            uint8_t rd = (uint8_t)buf[8 + buf_size + 3];

            if (a != ra || b != rb || c != rc || d != rd)
            {
                PCallCheckTable(L, buf, buf_size, -1, true);
            }

            ASSERT_EQ(a, ra);
            ASSERT_EQ(b, rb);
            ASSERT_EQ(c, rc);
            ASSERT_EQ(d, rd);

            lua_pop(L, 1);
            dmMemory::AlignedFree(buf);
        }
    }
}

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    TestMainPlatformInit();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
