// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "lobject.h"

namespace Luau
{
namespace CodeGen
{

bool forgLoopTableIter(lua_State* L, LuaTable* h, int index, TValue* ra);
bool forgLoopNodeIter(lua_State* L, LuaTable* h, int index, TValue* ra);
bool forgLoopNonTableFallback(lua_State* L, int insnA, int aux);

void forgPrepXnextFallback(lua_State* L, TValue* ra, int pc);

Closure* callProlog(lua_State* L, TValue* ra, StkId argtop, int nresults);
void callEpilogC(lua_State* L, int nresults, int n);

Udata* newUserdata(lua_State* L, size_t s, int tag);
void getImport(lua_State* L, StkId res, unsigned id, unsigned pc);

#define CALL_FALLBACK_YIELD 1

Closure* callFallback(lua_State* L, StkId ra, StkId argtop, int nresults);

const Instruction* executeGETGLOBAL(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executeSETGLOBAL(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executeGETTABLEKS(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executeSETTABLEKS(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executeNAMECALL(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executeSETLIST(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executeFORGPREP(lua_State* L, const Instruction* pc, StkId base, TValue* k);
void executeGETVARARGSMultRet(lua_State* L, const Instruction* pc, StkId base, int rai);
void executeGETVARARGSConst(lua_State* L, StkId base, int rai, int b);
const Instruction* executeDUPCLOSURE(lua_State* L, const Instruction* pc, StkId base, TValue* k);
const Instruction* executePREPVARARGS(lua_State* L, const Instruction* pc, StkId base, TValue* k);

} // namespace CodeGen
} // namespace Luau
