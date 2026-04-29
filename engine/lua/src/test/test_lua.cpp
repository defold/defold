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
#include <stdio.h>

#define DLIB_LOG_DOMAIN "test_lua"
#include <dlib/log.h>
#include <dlib/time.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#include <dmsdk/dlua/dlua.h>

#if defined(DM_LUA_TEST_LUAJIT)
#define DM_LUA_TEST_NAME "dmLuaJIT"
#else
#define DM_LUA_TEST_NAME "dmLua"
#endif

static const uint32_t LUA_CALC_ITERATIONS = 1000000;
static const uint32_t TABLE_OPERATION_ITERATIONS = 200000;
static const uint32_t FUNCTION_CALL_ITERATIONS = 500000;
static const uint32_t INTEROP_ITERATIONS = 200000;
static const uint32_t TABLE_ALLOCATIONS = 20000;

static const char* LUA_CALC_SCRIPT =
    "function tight_loop(iterations)\n"
    "    local acc = 0\n"
    "    for i = 1, iterations do\n"
    "        acc = acc + ((i * 3) % 97) - ((i * 7) % 31)\n"
    "    end\n"
    "    return acc\n"
    "end\n";

static const char* LUA_TABLE_SCRIPT =
    "function table_operations(iterations)\n"
    "    local t = {}\n"
    "    local acc = 0\n"
    "    for i = 1, iterations do\n"
    "        t[i] = i * 3\n"
    "    end\n"
    "    for i = 1, iterations do\n"
    "        local v = t[i]\n"
    "        t[i] = v + 1\n"
    "        acc = acc + v\n"
    "    end\n"
    "    return acc + t[iterations]\n"
    "end\n";

static const char* LUA_FUNCTION_CALL_SCRIPT =
    "function function_call_overhead(iterations)\n"
    "    local function add(a, b)\n"
    "        return a + b\n"
    "    end\n"
    "    local acc = 0\n"
    "    for i = 1, iterations do\n"
    "        acc = acc + add(i, 17)\n"
    "    end\n"
    "    return acc\n"
    "end\n";

static const char* LUA_INTEROP_SCRIPT =
    "function lua_add(a, b)\n"
    "    return a + b\n"
    "end\n"
    "function call_cpp(iterations)\n"
    "    local acc = 0\n"
    "    for i = 1, iterations do\n"
    "        acc = acc + cpp_add(i, 17)\n"
    "    end\n"
    "    return acc\n"
    "end\n";

static const char* LUA_MEMORY_SCRIPT =
    "function allocate_tables(iterations)\n"
    "    local tables = {}\n"
    "    for i = 1, iterations do\n"
    "        tables[i] = { i, i + 1, i + 2, label = 'benchmark' }\n"
    "    end\n"
    "    return tables\n"
    "end\n";

struct BenchmarkResult
{
    const char* m_Name;
    uint64_t    m_ElapsedUs;
    uint64_t    m_Iterations;
    double      m_Value;
};

static int CppAdd(dlua_State* L)
{
    dlua_Integer a = dlua_tointeger(L, 1);
    dlua_Integer b = dlua_tointeger(L, 2);
    dlua_pushinteger(L, a + b);
    return 1;
}

static bool RunString(dlua_State* L, const char* script)
{
    if (dluaL_dostring(L, script) != 0)
    {
        dmLogError("%s", dlua_tostring(L, -1));
        dlua_pop(L, 1);
        return false;
    }
    return true;
}

static int GetLuaMemoryKB(dlua_State* L)
{
    dlua_gc(L, DLUA_GCCOLLECT, 0);
    return dlua_gc(L, DLUA_GCCOUNT, 0);
}

static const char* GetLuaDialect(dlua_State* L)
{
    dlua_getglobal(L, "jit");
    bool has_jit = dlua_istable(L, -1);
    dlua_pop(L, 1);
    if (has_jit)
    {
        return "LuaJIT";
    }

    dlua_getglobal(L, "_VERSION");
    const char* version = dlua_tostring(L, -1);
    static char buffer[64];
    if (version != 0)
    {
        snprintf(buffer, sizeof(buffer), "%s", version);
    }
    else
    {
        snprintf(buffer, sizeof(buffer), "unknown");
    }
    dlua_pop(L, 1);
    return buffer;
}

static bool RunLuaCalculation(dlua_State* L, BenchmarkResult* benchmark)
{
    int top = dlua_gettop(L);
    uint64_t start = dmTime::GetMonotonicTime();

    dlua_getglobal(L, "tight_loop");
    dlua_pushinteger(L, LUA_CALC_ITERATIONS);
    int result = dlua_pcall(L, 1, 1, 0);
    if (result != 0)
    {
        dmLogError("%s", dlua_tostring(L, -1));
        dlua_pop(L, 1);
        return false;
    }

    double value = dlua_tonumber(L, -1);
    dlua_pop(L, 1);
    if (top != dlua_gettop(L))
    {
        return false;
    }

    benchmark->m_Name       = "tight_loop_arithmetic";
    benchmark->m_ElapsedUs  = dmTime::GetMonotonicTime() - start;
    benchmark->m_Iterations = LUA_CALC_ITERATIONS;
    benchmark->m_Value      = value;
    return true;
}

static bool RunTableOperations(dlua_State* L, BenchmarkResult* benchmark)
{
    int top = dlua_gettop(L);
    uint64_t start = dmTime::GetMonotonicTime();

    dlua_getglobal(L, "table_operations");
    dlua_pushinteger(L, TABLE_OPERATION_ITERATIONS);
    int result = dlua_pcall(L, 1, 1, 0);
    if (result != 0)
    {
        dmLogError("%s", dlua_tostring(L, -1));
        dlua_pop(L, 1);
        return false;
    }

    double value = dlua_tonumber(L, -1);
    dlua_pop(L, 1);
    if (top != dlua_gettop(L))
    {
        return false;
    }

    benchmark->m_Name       = "table_operations";
    benchmark->m_ElapsedUs  = dmTime::GetMonotonicTime() - start;
    benchmark->m_Iterations = TABLE_OPERATION_ITERATIONS;
    benchmark->m_Value      = value;
    return true;
}

static bool RunLuaFunctionCalls(dlua_State* L, BenchmarkResult* benchmark)
{
    int top = dlua_gettop(L);
    uint64_t start = dmTime::GetMonotonicTime();

    dlua_getglobal(L, "function_call_overhead");
    dlua_pushinteger(L, FUNCTION_CALL_ITERATIONS);
    int result = dlua_pcall(L, 1, 1, 0);
    if (result != 0)
    {
        dmLogError("%s", dlua_tostring(L, -1));
        dlua_pop(L, 1);
        return false;
    }

    double value = dlua_tonumber(L, -1);
    dlua_pop(L, 1);
    if (top != dlua_gettop(L))
    {
        return false;
    }

    benchmark->m_Name       = "function_call_overhead";
    benchmark->m_ElapsedUs  = dmTime::GetMonotonicTime() - start;
    benchmark->m_Iterations = FUNCTION_CALL_ITERATIONS;
    benchmark->m_Value      = value;
    return true;
}

static bool RunCppToLuaCalls(dlua_State* L, BenchmarkResult* benchmark)
{
    int top = dlua_gettop(L);
    uint64_t start = dmTime::GetMonotonicTime();
    double value = 0.0;

    for (uint32_t i = 1; i <= INTEROP_ITERATIONS; ++i)
    {
        dlua_getglobal(L, "lua_add");
        dlua_pushinteger(L, i);
        dlua_pushinteger(L, 17);
        int result = dlua_pcall(L, 2, 1, 0);
        if (result != 0)
        {
            dmLogError("%s", dlua_tostring(L, -1));
            dlua_pop(L, 1);
            return false;
        }
        value += dlua_tonumber(L, -1);
        dlua_pop(L, 1);
    }

    if (top != dlua_gettop(L))
    {
        return false;
    }

    benchmark->m_Name       = "cpp_to_lua_function";
    benchmark->m_ElapsedUs  = dmTime::GetMonotonicTime() - start;
    benchmark->m_Iterations = INTEROP_ITERATIONS;
    benchmark->m_Value      = value;
    return true;
}

static bool RunLuaToCppCalls(dlua_State* L, BenchmarkResult* benchmark)
{
    int top = dlua_gettop(L);
    uint64_t start = dmTime::GetMonotonicTime();

    dlua_getglobal(L, "call_cpp");
    dlua_pushinteger(L, INTEROP_ITERATIONS);
    int result = dlua_pcall(L, 1, 1, 0);
    if (result != 0)
    {
        dmLogError("%s", dlua_tostring(L, -1));
        dlua_pop(L, 1);
        return false;
    }

    double value = dlua_tonumber(L, -1);
    dlua_pop(L, 1);
    if (top != dlua_gettop(L))
    {
        return false;
    }

    benchmark->m_Name       = "lua_to_cpp_cfunction";
    benchmark->m_ElapsedUs  = dmTime::GetMonotonicTime() - start;
    benchmark->m_Iterations = INTEROP_ITERATIONS;
    benchmark->m_Value      = value;
    return true;
}

static void PrintBenchmark(const BenchmarkResult& benchmark)
{
    double us_per_iteration = (double)benchmark.m_ElapsedUs / (double)benchmark.m_Iterations;
    printf("[lua-perf] %-22s iterations=%llu elapsed_us=%llu us_per_iteration=%.4f result=%.0f\n",
           benchmark.m_Name,
           (unsigned long long)benchmark.m_Iterations,
           (unsigned long long)benchmark.m_ElapsedUs,
           us_per_iteration,
           benchmark.m_Value);
}

TEST3(dmLuaTypes, DluaType, DM_LUA_TEST_NAME)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);

    ASSERT_EQ(DLUA_TNONE, dlua_type(L, 1));

    dlua_pushnil(L);
    ASSERT_EQ(DLUA_TNIL, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_pushboolean(L, 1);
    ASSERT_EQ(DLUA_TBOOLEAN, dlua_type(L, -1));
    dlua_pop(L, 1);

    int light_userdata = 0;
    dlua_pushlightuserdata(L, &light_userdata);
    ASSERT_EQ(DLUA_TLIGHTUSERDATA, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_pushnumber(L, 1.0);
    ASSERT_EQ(DLUA_TNUMBER, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_pushstring(L, "type-test");
    ASSERT_EQ(DLUA_TSTRING, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_newtable(L);
    ASSERT_EQ(DLUA_TTABLE, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_pushcfunction(L, CppAdd);
    ASSERT_EQ(DLUA_TFUNCTION, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_newuserdata(L, sizeof(uint32_t));
    ASSERT_EQ(DLUA_TUSERDATA, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_pushthread(L);
    ASSERT_EQ(DLUA_TTHREAD, dlua_type(L, -1));
    dlua_pop(L, 1);

    dlua_close(L);
}

TEST3(dmLuaBenchmark, Performance, DM_LUA_TEST_NAME)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);
    dluaL_openlibs(L);

    ASSERT_TRUE(RunString(L, LUA_CALC_SCRIPT));
    ASSERT_TRUE(RunString(L, LUA_TABLE_SCRIPT));
    ASSERT_TRUE(RunString(L, LUA_FUNCTION_CALL_SCRIPT));
    ASSERT_TRUE(RunString(L, LUA_INTEROP_SCRIPT));
    dlua_register(L, "cpp_add", CppAdd);

    BenchmarkResult lua_calc;
    BenchmarkResult table_operations;
    BenchmarkResult function_calls;
    BenchmarkResult cpp_to_lua;
    BenchmarkResult lua_to_cpp;

    printf("[lua-perf] dialect=%s\n", GetLuaDialect(L));
    ASSERT_TRUE(RunLuaCalculation(L, &lua_calc));
    ASSERT_TRUE(RunTableOperations(L, &table_operations));
    ASSERT_TRUE(RunLuaFunctionCalls(L, &function_calls));
    ASSERT_TRUE(RunCppToLuaCalls(L, &cpp_to_lua));
    ASSERT_TRUE(RunLuaToCppCalls(L, &lua_to_cpp));
    PrintBenchmark(lua_calc);
    PrintBenchmark(table_operations);
    PrintBenchmark(function_calls);
    PrintBenchmark(cpp_to_lua);
    PrintBenchmark(lua_to_cpp);

    dlua_close(L);
}

TEST3(dmLuaBenchmark, MemoryUsage, DM_LUA_TEST_NAME)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);
    dluaL_openlibs(L);
    ASSERT_TRUE(RunString(L, LUA_MEMORY_SCRIPT));

    int memory_before_kb = GetLuaMemoryKB(L);

    dlua_getglobal(L, "allocate_tables");
    dlua_pushinteger(L, TABLE_ALLOCATIONS);
    int result = dlua_pcall(L, 1, 1, 0);
    ASSERT_EQ(0, result);

    int memory_after_alloc_kb = dlua_gc(L, DLUA_GCCOUNT, 0);
    dlua_pop(L, 1);
    int memory_after_gc_kb = GetLuaMemoryKB(L);

    printf("[lua-perf] memory dialect=%s before_kb=%d after_alloc_kb=%d after_gc_kb=%d allocations=%u\n",
           GetLuaDialect(L),
           memory_before_kb,
           memory_after_alloc_kb,
           memory_after_gc_kb,
           TABLE_ALLOCATIONS);

    ASSERT_LT(memory_before_kb, memory_after_alloc_kb);
    ASSERT_LE(memory_after_gc_kb, memory_after_alloc_kb);

    dlua_close(L);
}

int main(int argc, char** argv)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
