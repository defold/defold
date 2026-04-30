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
#include <string.h>

#define DLIB_LOG_DOMAIN "test_lua"
#include <dlib/log.h>
#include <dlib/time.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#if defined(DM_LUA_TEST_LUAU_CODEGEN)
#include "luacodegen.h"
#endif

#include <dmsdk/dlua/dlua.h>

#if defined(DM_LUA_TEST_LUAU)
#define DM_LUA_TEST_NAME "dmLuau"
#elif defined(DM_LUA_TEST_LUAJIT)
#define DM_LUA_TEST_NAME "dmLuaJIT"
#else
#define DM_LUA_TEST_NAME "dmLua"
#endif

static const uint32_t LUA_CALC_ITERATIONS = 1000000;
static const uint32_t TABLE_OPERATION_ITERATIONS = 200000;
static const uint32_t FUNCTION_CALL_ITERATIONS = 500000;
static const uint32_t INTEROP_ITERATIONS = 200000;
static const uint32_t TABLE_ALLOCATIONS = 20000;

static const char* LUA_DEBUG_SCRIPT =
    "function call_inspect_caller()\n"
    "    return inspect_caller()\n"
    "end\n"
    "function debug_target()\n"
    "    return 1\n"
    "end\n";

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

#if defined(DM_LUA_TEST_LUAJIT)
static const char* LUAJIT_PREALLOCATED_TABLE_SCRIPT =
    "function table_preallocated_operations(iterations)\n"
    "    local create_table = table.new\n"
    "    local t = create_table and create_table(iterations, 0) or {}\n"
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
#endif

#if defined(DM_LUA_TEST_LUAU)
static const char* LUAU_PREALLOCATED_TABLE_SCRIPT =
    "function table_preallocated_operations(iterations)\n"
    "    local t = table.create(iterations)\n"
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
#endif

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

#if defined(DM_LUA_TEST_LUAU)
static const char* LUAU_TYPED_CALC_SCRIPT =
    "function tight_loop(iterations: number): number\n"
    "    local acc: number = 0\n"
    "    for i = 1, iterations do\n"
    "        acc = acc + ((i * 3) % 97) - ((i * 7) % 31)\n"
    "    end\n"
    "    return acc\n"
    "end\n";

static const char* LUAU_TYPED_TABLE_SCRIPT =
    "function table_operations(iterations: number): number\n"
    "    local t: { [number]: number } = {}\n"
    "    local acc: number = 0\n"
    "    for i = 1, iterations do\n"
    "        t[i] = i * 3\n"
    "    end\n"
    "    for i = 1, iterations do\n"
    "        local v: number = t[i]\n"
    "        t[i] = v + 1\n"
    "        acc = acc + v\n"
    "    end\n"
    "    return acc + t[iterations]\n"
    "end\n";

static const char* LUAU_TYPED_PREALLOCATED_TABLE_SCRIPT =
    "function table_preallocated_operations(iterations: number): number\n"
    "    local t: { [number]: number } = table.create(iterations)\n"
    "    local acc: number = 0\n"
    "    for i = 1, iterations do\n"
    "        t[i] = i * 3\n"
    "    end\n"
    "    for i = 1, iterations do\n"
    "        local v: number = t[i]\n"
    "        t[i] = v + 1\n"
    "        acc = acc + v\n"
    "    end\n"
    "    return acc + t[iterations]\n"
    "end\n";

static const char* LUAU_TYPED_FUNCTION_CALL_SCRIPT =
    "function function_call_overhead(iterations: number): number\n"
    "    local function add(a: number, b: number): number\n"
    "        return a + b\n"
    "    end\n"
    "    local acc: number = 0\n"
    "    for i = 1, iterations do\n"
    "        acc = acc + add(i, 17)\n"
    "    end\n"
    "    return acc\n"
    "end\n";

static const char* LUAU_TYPED_INTEROP_SCRIPT =
    "function lua_add(a: number, b: number): number\n"
    "    return a + b\n"
    "end\n"
    "function call_cpp(iterations: number): number\n"
    "    local acc: number = 0\n"
    "    for i = 1, iterations do\n"
    "        acc = acc + cpp_add(i, 17)\n"
    "    end\n"
    "    return acc\n"
    "end\n";

static const char* LUAU_TYPED_MEMORY_SCRIPT =
    "function allocate_tables(iterations: number): any\n"
    "    local tables = {}\n"
    "    for i = 1, iterations do\n"
    "        tables[i] = { i, i + 1, i + 2, label = 'benchmark' }\n"
    "    end\n"
    "    return tables\n"
    "end\n";
#endif

struct ScriptSet
{
    const char* m_Mode;
    const char* m_CalcScript;
    const char* m_TableScript;
    const char* m_PreallocatedTableScript;
    const char* m_FunctionCallScript;
    const char* m_InteropScript;
    const char* m_MemoryScript;
};

static const ScriptSet LUA_SCRIPT_SET =
{
    "lua",
    LUA_CALC_SCRIPT,
    LUA_TABLE_SCRIPT,
#if defined(DM_LUA_TEST_LUAU)
    LUAU_PREALLOCATED_TABLE_SCRIPT,
#elif defined(DM_LUA_TEST_LUAJIT)
    LUAJIT_PREALLOCATED_TABLE_SCRIPT,
#else
    0,
#endif
    LUA_FUNCTION_CALL_SCRIPT,
    LUA_INTEROP_SCRIPT,
    LUA_MEMORY_SCRIPT
};

#if defined(DM_LUA_TEST_LUAU)
static const ScriptSet LUAU_TYPED_SCRIPT_SET =
{
    "luau-types",
    LUAU_TYPED_CALC_SCRIPT,
    LUAU_TYPED_TABLE_SCRIPT,
    LUAU_TYPED_PREALLOCATED_TABLE_SCRIPT,
    LUAU_TYPED_FUNCTION_CALL_SCRIPT,
    LUAU_TYPED_INTEROP_SCRIPT,
    LUAU_TYPED_MEMORY_SCRIPT
};
#endif

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

static int InspectCaller(dlua_State* L)
{
    dlua_Debug ar;
    bool found_script_frame = false;
    for (int level = 0; level < 4; ++level)
    {
        if (!dlua_getstack(L, level, &ar))
        {
            continue;
        }

        if (!dlua_getinfo(L, "Sln", &ar))
        {
            continue;
        }

        if (ar.source != 0 && ar.what != 0 && strcmp(ar.what, "C") != 0)
        {
            found_script_frame = true;
            break;
        }
    }

    if (!found_script_frame)
    {
        dlua_pushboolean(L, 0);
        return 1;
    }

    dlua_pushboolean(L, 1);
    dlua_pushstring(L, ar.source);
    dlua_pushinteger(L, ar.linedefined);
    dlua_pushinteger(L, ar.currentline);
    dlua_pushstring(L, ar.what ? ar.what : "");
    dlua_pushstring(L, ar.name ? ar.name : "");
    return 6;
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

static bool RunTableOperations(dlua_State* L, const char* function_name, const char* benchmark_name, BenchmarkResult* benchmark)
{
    int top = dlua_gettop(L);
    uint64_t start = dmTime::GetMonotonicTime();

    dlua_getglobal(L, function_name);
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

    benchmark->m_Name       = benchmark_name;
    benchmark->m_ElapsedUs  = dmTime::GetMonotonicTime() - start;
    benchmark->m_Iterations = TABLE_OPERATION_ITERATIONS;
    benchmark->m_Value      = value;
    return true;
}

static bool RunTableOperations(dlua_State* L, BenchmarkResult* benchmark)
{
    return RunTableOperations(L, "table_operations", "table_operations", benchmark);
}

static bool RunPreallocatedTableOperations(dlua_State* L, BenchmarkResult* benchmark)
{
    return RunTableOperations(L, "table_preallocated_operations", "table_prealloc_operations", benchmark);
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
    double value = 0.0;

    dlua_getglobal(L, "lua_add");
    int function_index = dlua_gettop(L);

    uint64_t start = dmTime::GetMonotonicTime();
    for (uint32_t i = 1; i <= INTEROP_ITERATIONS; ++i)
    {
        dlua_pushvalue(L, function_index);
        dlua_pushinteger(L, i);
        dlua_pushinteger(L, 17);
        dlua_call(L, 2, 1);
        value += dlua_tonumber(L, -1);
        dlua_pop(L, 1);
    }

    dlua_pop(L, 1);
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

static bool PrewarmBenchmarks(dlua_State* L)
{
    BenchmarkResult ignored;
    dlua_getglobal(L, "table_preallocated_operations");
    bool has_preallocated_table_operations = dlua_isfunction(L, -1);
    dlua_pop(L, 1);

    if (!RunLuaCalculation(L, &ignored))
    {
        return false;
    }
    if (!RunTableOperations(L, &ignored))
    {
        return false;
    }
    if (has_preallocated_table_operations && !RunPreallocatedTableOperations(L, &ignored))
    {
        return false;
    }
    if (!RunLuaFunctionCalls(L, &ignored))
    {
        return false;
    }
    if (!RunCppToLuaCalls(L, &ignored))
    {
        return false;
    }
    if (!RunLuaToCppCalls(L, &ignored))
    {
        return false;
    }

    dlua_gc(L, DLUA_GCCOLLECT, 0);
    return true;
}

static void PrintPerformanceHeader(dlua_State* L, const ScriptSet& scripts)
{
#if defined(DM_LUA_TEST_LUAU)
    #if defined(DM_LUA_TEST_LUAU_CODEGEN)
    printf("[lua-perf] dialect=%s mode=%s codegen=%s\n", GetLuaDialect(L), scripts.m_Mode, luau_codegen_supported() ? "enabled" : "unsupported");
    #else
    printf("[lua-perf] dialect=%s mode=%s codegen=unsupported\n", GetLuaDialect(L), scripts.m_Mode);
    #endif
#else
    printf("[lua-perf] dialect=%s mode=%s\n", GetLuaDialect(L), scripts.m_Mode);
#endif
}

static void PrintMemoryHeader(dlua_State* L, const ScriptSet& scripts, int memory_before_kb, int memory_after_alloc_kb, int memory_after_gc_kb)
{
    printf("[lua-perf] memory dialect=%s mode=%s before_kb=%d after_alloc_kb=%d after_gc_kb=%d allocations=%u\n",
           GetLuaDialect(L),
           scripts.m_Mode,
           memory_before_kb,
           memory_after_alloc_kb,
           memory_after_gc_kb,
           TABLE_ALLOCATIONS);
}

static void RunPerformanceTest(const ScriptSet& scripts)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);
    dluaL_openlibs(L);

    ASSERT_TRUE(RunString(L, scripts.m_CalcScript));
    ASSERT_TRUE(RunString(L, scripts.m_TableScript));
    if (scripts.m_PreallocatedTableScript != 0)
    {
        ASSERT_TRUE(RunString(L, scripts.m_PreallocatedTableScript));
    }
    ASSERT_TRUE(RunString(L, scripts.m_FunctionCallScript));
    ASSERT_TRUE(RunString(L, scripts.m_InteropScript));
    dlua_register(L, "cpp_add", CppAdd);

    BenchmarkResult lua_calc;
    BenchmarkResult table_operations;
    BenchmarkResult table_prealloc_operations;
    BenchmarkResult function_calls;
    BenchmarkResult cpp_to_lua;
    BenchmarkResult lua_to_cpp;

    PrintPerformanceHeader(L, scripts);
    ASSERT_TRUE(PrewarmBenchmarks(L));
    ASSERT_TRUE(RunLuaCalculation(L, &lua_calc));
    ASSERT_TRUE(RunTableOperations(L, &table_operations));
    if (scripts.m_PreallocatedTableScript != 0)
    {
        ASSERT_TRUE(RunPreallocatedTableOperations(L, &table_prealloc_operations));
    }
    ASSERT_TRUE(RunLuaFunctionCalls(L, &function_calls));
    ASSERT_TRUE(RunCppToLuaCalls(L, &cpp_to_lua));
    ASSERT_TRUE(RunLuaToCppCalls(L, &lua_to_cpp));
    PrintBenchmark(lua_calc);
    PrintBenchmark(table_operations);
    if (scripts.m_PreallocatedTableScript != 0)
    {
        PrintBenchmark(table_prealloc_operations);
    }
    PrintBenchmark(function_calls);
    PrintBenchmark(cpp_to_lua);
    PrintBenchmark(lua_to_cpp);

    dlua_close(L);
}

static void RunMemoryUsageTest(const ScriptSet& scripts)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);
    dluaL_openlibs(L);
    ASSERT_TRUE(RunString(L, scripts.m_MemoryScript));

    int memory_before_kb = GetLuaMemoryKB(L);

    dlua_getglobal(L, "allocate_tables");
    dlua_pushinteger(L, TABLE_ALLOCATIONS);
    int result = dlua_pcall(L, 1, 1, 0);
    ASSERT_EQ(0, result);

    int memory_after_alloc_kb = dlua_gc(L, DLUA_GCCOUNT, 0);
    dlua_pop(L, 1);
    int memory_after_gc_kb = GetLuaMemoryKB(L);

    PrintMemoryHeader(L, scripts, memory_before_kb, memory_after_alloc_kb, memory_after_gc_kb);

    ASSERT_LT(memory_before_kb, memory_after_alloc_kb);
    ASSERT_LE(memory_after_gc_kb, memory_after_alloc_kb);

    dlua_close(L);
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

TEST3(dmLuaDebug, GetStackGetInfo, DM_LUA_TEST_NAME)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);

    dlua_register(L, "inspect_caller", InspectCaller);
    ASSERT_TRUE(RunString(L, LUA_DEBUG_SCRIPT));

    int top = dlua_gettop(L);

#if !defined(DM_LUA_TEST_LUAJIT)
    dlua_getglobal(L, "call_inspect_caller");
    ASSERT_EQ(0, dlua_pcall(L, 0, 6, 0));

    ASSERT_EQ(1, dlua_toboolean(L, -6));
    ASSERT_NE((const char*)0, dlua_tostring(L, -5));
    ASSERT_LE(1, dlua_tointeger(L, -4));
    ASSERT_LE(1, dlua_tointeger(L, -3));
    ASSERT_NE((const char*)0, dlua_tostring(L, -2));
    ASSERT_NE((const char*)0, dlua_tostring(L, -1));
    dlua_pop(L, 6);
    ASSERT_EQ(top, dlua_gettop(L));
#endif

    dlua_Debug ar;
    dlua_getglobal(L, "debug_target");
    ASSERT_EQ(top + 1, dlua_gettop(L));
    ASSERT_TRUE(dlua_getinfo(L, ">Sn", &ar) != 0);
    ASSERT_EQ(top, dlua_gettop(L));
    ASSERT_NE((const char*)0, ar.source);
    ASSERT_LE(1, ar.linedefined);

    dlua_close(L);
}

TEST3(dmLuaRefs, TableRef, DM_LUA_TEST_NAME)
{
    dlua_State* L = dluaL_newstate();
    ASSERT_NE((void*)0, (void*)L);

    int top = dlua_gettop(L);
    dlua_newtable(L);

    dlua_pushstring(L, "first");
    int first_ref = dluaL_ref(L, -2);
    ASSERT_NE(DLUA_NOREF, first_ref);
    ASSERT_NE(-1, first_ref);
    ASSERT_EQ(top + 1, dlua_gettop(L));

    dlua_rawgeti(L, -1, first_ref);
    ASSERT_STREQ("first", dlua_tostring(L, -1));
    dlua_pop(L, 1);

    dluaL_unref(L, -1, first_ref);
    dlua_rawgeti(L, -1, first_ref);
    ASSERT_TRUE(dlua_isnil(L, -1) != 0);
    dlua_pop(L, 1);

    dlua_pushstring(L, "second");
    int second_ref = dluaL_ref(L, -2);
    ASSERT_EQ(first_ref, second_ref);
    dlua_rawgeti(L, -1, second_ref);
    ASSERT_STREQ("second", dlua_tostring(L, -1));
    dlua_pop(L, 1);

    dlua_pushnil(L);
    ASSERT_EQ(-1, dluaL_ref(L, -2));

    dlua_pop(L, 1);
    ASSERT_EQ(top, dlua_gettop(L));

    dlua_close(L);
}

TEST3(dmLuaBenchmark, Performance, DM_LUA_TEST_NAME)
{
    RunPerformanceTest(LUA_SCRIPT_SET);
#if defined(DM_LUA_TEST_LUAU)
    RunPerformanceTest(LUAU_TYPED_SCRIPT_SET);
#endif
}

TEST3(dmLuaBenchmark, MemoryUsage, DM_LUA_TEST_NAME)
{
    RunMemoryUsageTest(LUA_SCRIPT_SET);
#if defined(DM_LUA_TEST_LUAU)
    RunMemoryUsageTest(LUAU_TYPED_SCRIPT_SET);
#endif
}

int main(int argc, char** argv)
{
    dmLog::LogParams params;
    dmLog::LogInitialize(&params);
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
