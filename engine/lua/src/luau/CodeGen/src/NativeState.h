// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Bytecode.h"
#include "Luau/CodeAllocator.h"
#include "Luau/Label.h"

#include <memory>

#include <stdint.h>

#include "ldebug.h"
#include "lobject.h"
#include "ltm.h"
#include "lstate.h"

typedef int (*luau_FastFunction)(lua_State* L, StkId res, TValue* arg0, int nresults, StkId args, int nparams);

namespace Luau
{
namespace CodeGen
{

class UnwindBuilder;

struct NativeContext
{
    // Gateway (C => native transition) entry & exit, compiled at runtime
    uint8_t* gateEntry = nullptr;
    uint8_t* gateExit = nullptr;

    // Helper functions, implemented in C
    int (*luaV_lessthan)(lua_State* L, const TValue* l, const TValue* r) = nullptr;
    int (*luaV_lessequal)(lua_State* L, const TValue* l, const TValue* r) = nullptr;
    int (*luaV_equalval)(lua_State* L, const TValue* t1, const TValue* t2) = nullptr;
    void (*luaV_doarithadd)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithsub)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithmul)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithdiv)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithidiv)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithmod)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithpow)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_doarithunm)(lua_State* L, StkId ra, const TValue* rb, const TValue* rc) = nullptr;
    void (*luaV_dolen)(lua_State* L, StkId ra, const TValue* rb) = nullptr;
    void (*luaV_gettable)(lua_State* L, const TValue* t, TValue* key, StkId val) = nullptr;
    void (*luaV_settable)(lua_State* L, const TValue* t, TValue* key, StkId val) = nullptr;
    void (*luaV_concat)(lua_State* L, int total, int last) = nullptr;

    int (*luaH_getn)(LuaTable* t) = nullptr;
    LuaTable* (*luaH_new)(lua_State* L, int narray, int lnhash) = nullptr;
    LuaTable* (*luaH_clone)(lua_State* L, LuaTable* tt) = nullptr;
    void (*luaH_resizearray)(lua_State* L, LuaTable* t, int nasize) = nullptr;
    TValue* (*luaH_setnum)(lua_State* L, LuaTable* t, int key);

    void (*luaC_barriertable)(lua_State* L, LuaTable* t, GCObject* v) = nullptr;
    void (*luaC_barrierf)(lua_State* L, GCObject* o, GCObject* v) = nullptr;
    void (*luaC_barrierback)(lua_State* L, GCObject* o, GCObject** gclist) = nullptr;
    size_t (*luaC_step)(lua_State* L, bool assist) = nullptr;

    void (*luaF_close)(lua_State* L, StkId level) = nullptr;
    UpVal* (*luaF_findupval)(lua_State* L, StkId level) = nullptr;
    Closure* (*luaF_newLclosure)(lua_State* L, int nelems, LuaTable* e, Proto* p) = nullptr;

    const TValue* (*luaT_gettm)(LuaTable* events, TMS event, TString* ename) = nullptr;
    const TString* (*luaT_objtypenamestr)(lua_State* L, const TValue* o) = nullptr;

    double (*libm_exp)(double) = nullptr;
    double (*libm_pow)(double, double) = nullptr;
    double (*libm_fmod)(double, double) = nullptr;
    double (*libm_asin)(double) = nullptr;
    double (*libm_sin)(double) = nullptr;
    double (*libm_sinh)(double) = nullptr;
    double (*libm_acos)(double) = nullptr;
    double (*libm_cos)(double) = nullptr;
    double (*libm_cosh)(double) = nullptr;
    double (*libm_atan)(double) = nullptr;
    double (*libm_atan2)(double, double) = nullptr;
    double (*libm_tan)(double) = nullptr;
    double (*libm_tanh)(double) = nullptr;
    double (*libm_log)(double) = nullptr;
    double (*libm_log2)(double) = nullptr;
    double (*libm_log10)(double) = nullptr;
    double (*libm_ldexp)(double, int) = nullptr;
    double (*libm_round)(double) = nullptr;
    double (*libm_frexp)(double, int*) = nullptr;
    double (*libm_modf)(double, double*) = nullptr;

    // Helper functions
    bool (*forgLoopTableIter)(lua_State* L, LuaTable* h, int index, TValue* ra) = nullptr;
    bool (*forgLoopNodeIter)(lua_State* L, LuaTable* h, int index, TValue* ra) = nullptr;
    bool (*forgLoopNonTableFallback)(lua_State* L, int insnA, int aux) = nullptr;
    void (*forgPrepXnextFallback)(lua_State* L, TValue* ra, int pc) = nullptr;
    Closure* (*callProlog)(lua_State* L, TValue* ra, StkId argtop, int nresults) = nullptr;
    void (*callEpilogC)(lua_State* L, int nresults, int n) = nullptr;
    Udata* (*newUserdata)(lua_State* L, size_t s, int tag) = nullptr;
    void (*getImport)(lua_State* L, StkId res, unsigned id, unsigned pc) = nullptr;

    Closure* (*callFallback)(lua_State* L, StkId ra, StkId argtop, int nresults) = nullptr;

    // Opcode fallbacks, implemented in C
    const Instruction* (*executeGETGLOBAL)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executeSETGLOBAL)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executeGETTABLEKS)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executeSETTABLEKS)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executeNAMECALL)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executeSETLIST)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executeFORGPREP)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    void (*executeGETVARARGSMultRet)(lua_State* L, const Instruction* pc, StkId base, int rai) = nullptr;
    void (*executeGETVARARGSConst)(lua_State* L, StkId base, int rai, int b) = nullptr;
    const Instruction* (*executeDUPCLOSURE)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;
    const Instruction* (*executePREPVARARGS)(lua_State* L, const Instruction* pc, StkId base, TValue* k) = nullptr;

    // Fast call methods, implemented in C
    luau_FastFunction luauF_table[256] = {};
};

using GateFn = int (*)(lua_State*, Proto*, uintptr_t, NativeContext*);

void initFunctions(NativeContext& context);

} // namespace CodeGen
} // namespace Luau
