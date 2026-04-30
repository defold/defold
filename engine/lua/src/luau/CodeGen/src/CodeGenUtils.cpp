// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "CodeGenUtils.h"

#include "lvm.h"

#include "lbuiltins.h"
#include "lbytecode.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lnumutils.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ludata.h"

#include <string.h>

LUAU_FASTFLAGVARIABLE(LuauNativeCodeTargetCheck)

// All external function calls that can cause stack realloc or Lua calls have to be wrapped in VM_PROTECT
// This makes sure that we save the pc (in case the Lua call needs to generate a backtrace) before the call,
// and restores the stack pointer after in case stack gets reallocated
// Should only be used on the slow paths.
#define VM_PROTECT(x) \
    { \
        L->ci->savedpc = pc; \
        { \
            x; \
        }; \
        base = L->base; \
    }

// Some external functions can cause an error, but never reallocate the stack; for these, VM_PROTECT_PC() is
// a cheaper version of VM_PROTECT that can be called before the external call.
#define VM_PROTECT_PC() L->ci->savedpc = pc

#define VM_REG(i) (LUAU_ASSERT(unsigned(i) < unsigned(L->top - base)), &base[i])
#define VM_KV(i) (LUAU_ASSERT(unsigned(i) < unsigned(cl->l.p->sizek)), &k[i])
#define VM_UV(i) (LUAU_ASSERT(unsigned(i) < unsigned(cl->nupvalues)), &cl->l.uprefs[i])

#define VM_PATCH_C(pc, slot) *const_cast<Instruction*>(pc) = ((uint8_t(slot) << 24) | (0x00ffffffu & *(pc)))
#define VM_PATCH_E(pc, slot) *const_cast<Instruction*>(pc) = ((uint32_t(slot) << 8) | (0x000000ffu & *(pc)))

#define VM_INTERRUPT() \
    { \
        void (*interrupt)(lua_State*, int) = L->global->cb.interrupt; \
        if (LUAU_UNLIKELY(!!interrupt)) \
        { /* the interrupt hook is called right before we advance pc */ \
            VM_PROTECT(L->ci->savedpc++; interrupt(L, -1)); \
            if (L->status != 0) \
            { \
                L->ci->savedpc--; \
                return NULL; \
            } \
        } \
    }

namespace Luau
{
namespace CodeGen
{

bool forgLoopTableIter(lua_State* L, LuaTable* h, int index, TValue* ra)
{
    int sizearray = h->sizearray;

    // first we advance index through the array portion
    while (unsigned(index) < unsigned(sizearray))
    {
        TValue* e = &h->array[index];

        if (!ttisnil(e))
        {
            setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(index + 1)), LU_TAG_ITERATOR);
            setnvalue(ra + 3, double(index + 1));
            setobj2s(L, ra + 4, e);

            return true;
        }

        index++;
    }

    int sizenode = 1 << h->lsizenode;

    // then we advance index through the hash portion
    while (unsigned(index - h->sizearray) < unsigned(sizenode))
    {
        LuaNode* n = &h->node[index - sizearray];

        if (!ttisnil(gval(n)))
        {
            setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(index + 1)), LU_TAG_ITERATOR);
            getnodekey(L, ra + 3, n);
            setobj(L, ra + 4, gval(n));

            return true;
        }

        index++;
    }

    return false;
}

bool forgLoopNodeIter(lua_State* L, LuaTable* h, int index, TValue* ra)
{
    int sizearray = h->sizearray;
    int sizenode = 1 << h->lsizenode;

    // then we advance index through the hash portion
    while (unsigned(index - sizearray) < unsigned(sizenode))
    {
        LuaNode* n = &h->node[index - sizearray];

        if (!ttisnil(gval(n)))
        {
            setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(index + 1)), LU_TAG_ITERATOR);
            getnodekey(L, ra + 3, n);
            setobj(L, ra + 4, gval(n));

            return true;
        }

        index++;
    }

    return false;
}

bool forgLoopNonTableFallback(lua_State* L, int insnA, int aux)
{
    TValue* base = L->base;
    TValue* ra = VM_REG(insnA);

    // note: it's safe to push arguments past top for complicated reasons (see lvmexecute.cpp)
    setobj2s(L, ra + 3 + 2, ra + 2);
    setobj2s(L, ra + 3 + 1, ra + 1);
    setobj2s(L, ra + 3, ra);

    L->top = ra + 3 + 3; // func + 2 args (state and index)
    LUAU_ASSERT(L->top <= L->stack_last);

    luaD_call(L, ra + 3, uint8_t(aux));
    L->top = L->ci->top;

    // recompute ra since stack might have been reallocated
    base = L->base;
    ra = VM_REG(insnA);

    // copy first variable back into the iteration index
    setobj2s(L, ra + 2, ra + 3);

    return !ttisnil(ra + 3);
}

void forgPrepXnextFallback(lua_State* L, TValue* ra, int pc)
{
    if (!ttisfunction(ra))
    {
        Closure* cl = clvalue(L->ci->func);
        L->ci->savedpc = cl->l.p->code + pc;

        luaG_typeerror(L, ra, "iterate over");
    }
}

Closure* callProlog(lua_State* L, TValue* ra, StkId argtop, int nresults)
{
    // slow-path: not a function call
    if (LUAU_UNLIKELY(!ttisfunction(ra)))
    {
        luaV_tryfuncTM(L, ra);
        argtop++; // __call adds an extra self
    }

    Closure* ccl = clvalue(ra);

    CallInfo* ci = incr_ci(L);
    ci->func = ra;
    ci->base = ra + 1;
    ci->top = argtop + ccl->stacksize; // note: technically UB since we haven't reallocated the stack yet
    ci->savedpc = NULL;
    ci->flags = 0;
    ci->nresults = nresults;

    L->base = ci->base;
    L->top = argtop;

    // note: this reallocs stack, but we don't need to VM_PROTECT this
    // this is because we're going to modify base/savedpc manually anyhow
    // crucially, we can't use ra/argtop after this line
    luaD_checkstackfornewci(L, ccl->stacksize);

    return ccl;
}

void callEpilogC(lua_State* L, int nresults, int n)
{
    // ci is our callinfo, cip is our parent
    CallInfo* ci = L->ci;
    CallInfo* cip = ci - 1;

    // copy return values into parent stack (but only up to nresults!), fill the rest with nil
    // note: in MULTRET context nresults starts as -1 so i != 0 condition never activates intentionally
    StkId res = ci->func;
    StkId vali = L->top - n;
    StkId valend = L->top;

    int i;
    for (i = nresults; i != 0 && vali < valend; i--)
        setobj2s(L, res++, vali++);
    while (i-- > 0)
        setnilvalue(res++);

    // pop the stack frame
    L->ci = cip;
    L->base = cip->base;
    L->top = (nresults == LUA_MULTRET) ? res : cip->top;
}

Udata* newUserdata(lua_State* L, size_t s, int tag)
{
    Udata* u = luaU_newudata(L, s, tag);

    if (LuaTable* h = L->global->udatamt[tag])
    {
        // currently, we always allocate unmarked objects, so forward barrier can be skipped
        LUAU_ASSERT(!isblack(obj2gco(u)));

        u->metatable = h;
    }

    return u;
}

void getImport(lua_State* L, StkId res, unsigned id, unsigned pc)
{
    Closure* cl = clvalue(L->ci->func);
    L->ci->savedpc = cl->l.p->code + pc;

    luaV_getimport(L, cl->env, cl->l.p->k, res, id, /*propagatenil*/ false);
}

// Extracted as-is from lvmexecute.cpp with the exception of control flow (reentry) and removed interrupts/savedpc
Closure* callFallback(lua_State* L, StkId ra, StkId argtop, int nresults)
{
    // slow-path: not a function call
    if (LUAU_UNLIKELY(!ttisfunction(ra)))
    {
        luaV_tryfuncTM(L, ra);
        argtop++; // __call adds an extra self
    }

    Closure* ccl = clvalue(ra);

    CallInfo* ci = incr_ci(L);
    ci->func = ra;
    ci->base = ra + 1;
    ci->top = argtop + ccl->stacksize; // note: technically UB since we haven't reallocated the stack yet
    ci->savedpc = NULL;
    ci->flags = 0;
    ci->nresults = nresults;

    L->base = ci->base;
    L->top = argtop;

    // note: this reallocs stack, but we don't need to VM_PROTECT this
    // this is because we're going to modify base/savedpc manually anyhow
    // crucially, we can't use ra/argtop after this line
    luaD_checkstackfornewci(L, ccl->stacksize);

    LUAU_ASSERT(ci->top <= L->stack_last);

    if (!ccl->isC)
    {
        Proto* p = ccl->l.p;

        // fill unused parameters with nil
        StkId argi = L->top;
        StkId argend = L->base + p->numparams;
        while (argi < argend)
            setnilvalue(argi++); // complete missing arguments
        L->top = p->is_vararg ? argi : ci->top;

        // keep executing new function
        ci->savedpc = p->code;

        if (LUAU_LIKELY(FFlag::LuauNativeCodeTargetCheck ? p->exectarget != 0 : p->execdata != NULL))
            ci->flags = LUA_CALLINFO_NATIVE;

        return ccl;
    }
    else
    {
        lua_CFunction func = ccl->c.f;
        int n = func(L);

        // yield
        if (n < 0)
            return (Closure*)CALL_FALLBACK_YIELD;

        // ci is our callinfo, cip is our parent
        CallInfo* ci = L->ci;
        CallInfo* cip = ci - 1;

        // copy return values into parent stack (but only up to nresults!), fill the rest with nil
        // note: in MULTRET context nresults starts as -1 so i != 0 condition never activates intentionally
        StkId res = ci->func;
        StkId vali = L->top - n;
        StkId valend = L->top;

        int i;
        for (i = nresults; i != 0 && vali < valend; i--)
            setobj2s(L, res++, vali++);
        while (i-- > 0)
            setnilvalue(res++);

        // pop the stack frame
        L->ci = cip;
        L->base = cip->base;
        L->top = (nresults == LUA_MULTRET) ? res : cip->top;

        // keep executing current function
        return NULL;
    }
}

const Instruction* executeGETGLOBAL(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    uint32_t aux = *pc++;
    TValue* kv = VM_KV(aux);
    LUAU_ASSERT(ttisstring(kv));

    // fast-path should already have been checked, so we skip checking for it here
    LuaTable* h = cl->env;
    int slot = LUAU_INSN_C(insn) & h->nodemask8;

    // slow-path, may invoke Lua calls via __index metamethod
    TValue g;
    sethvalue(L, &g, h);
    L->cachedslot = slot;
    VM_PROTECT(luaV_gettable(L, &g, kv, ra));
    // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
    VM_PATCH_C(pc - 2, L->cachedslot);
    return pc;
}

const Instruction* executeSETGLOBAL(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    uint32_t aux = *pc++;
    TValue* kv = VM_KV(aux);
    LUAU_ASSERT(ttisstring(kv));

    // fast-path should already have been checked, so we skip checking for it here
    LuaTable* h = cl->env;
    int slot = LUAU_INSN_C(insn) & h->nodemask8;

    // slow-path, may invoke Lua calls via __newindex metamethod
    TValue g;
    sethvalue(L, &g, h);
    L->cachedslot = slot;
    VM_PROTECT(luaV_settable(L, &g, kv, ra));
    // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
    VM_PATCH_C(pc - 2, L->cachedslot);
    return pc;
}

const Instruction* executeGETTABLEKS(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    int op = LUAU_INSN_OP(insn);
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    StkId rb = VM_REG(LUAU_INSN_B(insn));
    uint32_t aux = *pc++;
    TValue* kv = VM_KV(op == LOP_GETUDATAKS ? LUAU_INSN_AUX_KV16(aux) : aux);
    LUAU_ASSERT(ttisstring(kv));

    // fast-path: built-in table
    if (ttistable(rb))
    {
        LuaTable* h = hvalue(rb);

        // we ignore the fast path that checks for the cached slot since IrTranslation already checks for it.

        if (!h->metatable)
        {
            // fast-path: value is not in expected slot, but the table lookup doesn't involve metatable
            const TValue* res = luaH_getstr(h, tsvalue(kv));

            if (res != luaO_nilobject)
            {
                int cachedslot = gval2slot(h, res);
                // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
                VM_PATCH_C(pc - 2, cachedslot);
            }

            setobj2s(L, ra, res);
            return pc;
        }
        else
        {
            // slow-path, may invoke Lua calls via __index metamethod
            int slot = LUAU_INSN_C(insn) & h->nodemask8;
            L->cachedslot = slot;
            VM_PROTECT(luaV_gettable(L, rb, kv, ra));
            // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
            VM_PATCH_C(pc - 2, L->cachedslot);
            return pc;
        }
    }
    else
    {
        // fast-path: user data with C __index TM
        const TValue* fn = 0;
        if (ttisuserdata(rb) && (fn = fasttm(L, uvalue(rb)->metatable, TM_INDEX)) && ttisfunction(fn) && clvalue(fn)->isC)
        {
            // note: it's safe to push arguments past top for complicated reasons (see top of the file)
            LUAU_ASSERT(L->top + 3 < L->stack + L->stacksize);
            StkId top = L->top;
            setobj2s(L, top + 0, fn);
            setobj2s(L, top + 1, rb);
            setobj2s(L, top + 2, kv);
            L->top = top + 3;

            L->cachedslot = LUAU_INSN_C(insn);
            VM_PROTECT(luaV_callTM(L, 2, LUAU_INSN_A(insn)));
            // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
            VM_PATCH_C(pc - 2, L->cachedslot);
            return pc;
        }
        else if (ttisvector(rb))
        {
            // fast-path: quick case-insensitive comparison with "X"/"Y"/"Z"
            const char* name = getstr(tsvalue(kv));
            int ic = (name[0] | ' ') - 'x';

#if LUA_VECTOR_SIZE == 4
            // 'w' is before 'x' in ascii, so ic is -1 when indexing with 'w'
            if (ic == -1)
                ic = 3;
#endif

            if (unsigned(ic) < LUA_VECTOR_SIZE && name[1] == '\0')
            {
                const float* v = vvalue(rb); // silences ubsan when indexing v[]
                setnvalue(ra, v[ic]);
                return pc;
            }

            fn = fasttm(L, L->global->mt[LUA_TVECTOR], TM_INDEX);

            if (fn && ttisfunction(fn) && clvalue(fn)->isC)
            {
                // note: it's safe to push arguments past top for complicated reasons (see top of the file)
                LUAU_ASSERT(L->top + 3 < L->stack + L->stacksize);
                StkId top = L->top;
                setobj2s(L, top + 0, fn);
                setobj2s(L, top + 1, rb);
                setobj2s(L, top + 2, kv);
                L->top = top + 3;

                L->cachedslot = LUAU_INSN_C(insn);
                VM_PROTECT(luaV_callTM(L, 2, LUAU_INSN_A(insn)));
                // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
                VM_PATCH_C(pc - 2, L->cachedslot);
                return pc;
            }

            // fall through to slow path
        }

        // fall through to slow path
    }

    // slow-path, may invoke Lua calls via __index metamethod
    VM_PROTECT(luaV_gettable(L, rb, kv, ra));
    return pc;
}

const Instruction* executeSETTABLEKS(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    int op = LUAU_INSN_OP(insn);
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    StkId rb = VM_REG(LUAU_INSN_B(insn));
    uint32_t aux = *pc++;
    TValue* kv = VM_KV(op == LOP_SETUDATAKS ? LUAU_INSN_AUX_KV16(aux) : aux);
    LUAU_ASSERT(ttisstring(kv));

    // fast-path: built-in table
    if (ttistable(rb))
    {
        LuaTable* h = hvalue(rb);

        // we ignore the fast path that checks for the cached slot since IrTranslation already checks for it.

        if (fastnotm(h->metatable, TM_NEWINDEX) && !h->readonly)
        {
            VM_PROTECT_PC(); // set may fail

            TValue* res = luaH_setstr(L, h, tsvalue(kv));
            int cachedslot = gval2slot(h, res);
            // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
            VM_PATCH_C(pc - 2, cachedslot);
            setobj2t(L, res, ra);
            luaC_barriert(L, h, ra);
            return pc;
        }
        else
        {
            // slow-path, may invoke Lua calls via __newindex metamethod
            int slot = LUAU_INSN_C(insn) & h->nodemask8;
            L->cachedslot = slot;
            VM_PROTECT(luaV_settable(L, rb, kv, ra));
            // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
            VM_PATCH_C(pc - 2, L->cachedslot);
            return pc;
        }
    }
    else
    {
        // fast-path: user data with C __newindex TM
        const TValue* fn = 0;
        if (ttisuserdata(rb) && (fn = fasttm(L, uvalue(rb)->metatable, TM_NEWINDEX)) && ttisfunction(fn) && clvalue(fn)->isC)
        {
            // note: it's safe to push arguments past top for complicated reasons (see top of the file)
            LUAU_ASSERT(L->top + 4 < L->stack + L->stacksize);
            StkId top = L->top;
            setobj2s(L, top + 0, fn);
            setobj2s(L, top + 1, rb);
            setobj2s(L, top + 2, kv);
            setobj2s(L, top + 3, ra);
            L->top = top + 4;

            L->cachedslot = LUAU_INSN_C(insn);
            VM_PROTECT(luaV_callTM(L, 3, -1));
            // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
            VM_PATCH_C(pc - 2, L->cachedslot);
            return pc;
        }
        else
        {
            // slow-path, may invoke Lua calls via __newindex metamethod
            VM_PROTECT(luaV_settable(L, rb, kv, ra));
            return pc;
        }
    }
}

const Instruction* executeNAMECALL(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    int op = LUAU_INSN_OP(insn);
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    StkId rb = VM_REG(LUAU_INSN_B(insn));
    uint32_t aux = *pc++;
    TValue* kv = VM_KV(op == LOP_NAMECALLUDATA ? LUAU_INSN_AUX_KV16(aux) : aux);
    LUAU_ASSERT(ttisstring(kv));

    if (ttistable(rb))
    {
        // note: lvmexecute.cpp version of NAMECALL has two fast paths, but both fast paths are inlined into IR
        // as such, if we get here we can just use the generic path which makes the fallback path a little faster

        // slow-path: handles full table lookup
        setobj2s(L, ra + 1, rb);
        L->cachedslot = LUAU_INSN_C(insn);
        VM_PROTECT(luaV_gettable(L, rb, kv, ra));
        // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
        VM_PATCH_C(pc - 2, L->cachedslot);
        // recompute ra since stack might have been reallocated
        ra = VM_REG(LUAU_INSN_A(insn));
        if (ttisnil(ra))
            luaG_methoderror(L, ra + 1, tsvalue(kv));
    }
    else
    {
        LuaTable* mt = ttisuserdata(rb) ? uvalue(rb)->metatable : L->global->mt[ttype(rb)];
        const TValue* tmi = 0;

        // fast-path: metatable with __namecall
        if (const TValue* fn = fasttm(L, mt, TM_NAMECALL))
        {
            // note: order of copies allows rb to alias ra+1 or ra
            setobj2s(L, ra + 1, rb);
            setobj2s(L, ra, fn);

            L->namecall = tsvalue(kv);
        }
        else if ((tmi = fasttm(L, mt, TM_INDEX)) && ttistable(tmi))
        {
            LuaTable* h = hvalue(tmi);
            int slot = LUAU_INSN_C(insn) & h->nodemask8;
            LuaNode* n = &h->node[slot];

            // fast-path: metatable with __index that has method in expected slot
            if (LUAU_LIKELY(ttisstring(gkey(n)) && tsvalue(gkey(n)) == tsvalue(kv) && !ttisnil(gval(n))))
            {
                // note: order of copies allows rb to alias ra+1 or ra
                setobj2s(L, ra + 1, rb);
                setobj2s(L, ra, gval(n));
            }
            else
            {
                // slow-path: handles slot mismatch
                setobj2s(L, ra + 1, rb);
                L->cachedslot = slot;
                VM_PROTECT(luaV_gettable(L, rb, kv, ra));
                // save cachedslot to accelerate future lookups; patches currently executing instruction since pc-2 rolls back two pc++
                VM_PATCH_C(pc - 2, L->cachedslot);
                // recompute ra since stack might have been reallocated
                ra = VM_REG(LUAU_INSN_A(insn));
                if (ttisnil(ra))
                    luaG_methoderror(L, ra + 1, tsvalue(kv));
            }
        }
        else
        {
            // slow-path: handles non-table __index
            setobj2s(L, ra + 1, rb);
            VM_PROTECT(luaV_gettable(L, rb, kv, ra));
            // recompute ra since stack might have been reallocated
            ra = VM_REG(LUAU_INSN_A(insn));
            if (ttisnil(ra))
                luaG_methoderror(L, ra + 1, tsvalue(kv));
        }
    }

    // intentional fallthrough to CALL
    LUAU_ASSERT(LUAU_INSN_OP(*pc) == LOP_CALL);
    return pc;
}

const Instruction* executeSETLIST(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    StkId rb = &base[LUAU_INSN_B(insn)]; // note: this can point to L->top if c == LUA_MULTRET making VM_REG unsafe to use
    int c = LUAU_INSN_C(insn) - 1;
    uint32_t index = *pc++;

    if (c == LUA_MULTRET)
    {
        c = int(L->top - rb);
        L->top = L->ci->top;
    }

    LuaTable* h = hvalue(ra);

    // TODO: we really don't need this anymore
    if (!ttistable(ra))
        return NULL; // temporary workaround to weaken a rather powerful exploitation primitive in case of a MITM attack on bytecode

    int last = index + c - 1;
    if (last > h->sizearray)
    {
        VM_PROTECT_PC(); // luaH_resizearray may fail due to OOM

        luaH_resizearray(L, h, last);
    }

    TValue* array = h->array;

    for (int i = 0; i < c; ++i)
        setobj2t(L, &array[index + i - 1], rb + i);

    luaC_barrierfast(L, h);
    return pc;
}

const Instruction* executeFORGPREP(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    StkId ra = VM_REG(LUAU_INSN_A(insn));

    if (ttisfunction(ra))
    {
        // will be called during FORGLOOP
    }
    else
    {
        LuaTable* mt = ttistable(ra) ? hvalue(ra)->metatable : ttisuserdata(ra) ? uvalue(ra)->metatable : cast_to(LuaTable*, NULL);

        if (const TValue* fn = fasttm(L, mt, TM_ITER))
        {
            setobj2s(L, ra + 1, ra);
            setobj2s(L, ra, fn);

            L->top = ra + 2; // func + self arg
            LUAU_ASSERT(L->top <= L->stack_last);

            VM_PROTECT(luaD_call(L, ra, 3));
            L->top = L->ci->top;

            // recompute ra since stack might have been reallocated
            ra = VM_REG(LUAU_INSN_A(insn));

            // protect against __iter returning nil, since nil is used as a marker for builtin iteration in FORGLOOP
            if (ttisnil(ra))
            {
                VM_PROTECT_PC(); // next call always errors
                luaG_typeerror(L, ra, "call");
            }
        }
        else if (fasttm(L, mt, TM_CALL))
        {
            // table or userdata with __call, will be called during FORGLOOP
            // TODO: we might be able to stop supporting this depending on whether it's used in practice
        }
        else if (ttistable(ra))
        {
            // set up registers for builtin iteration
            setobj2s(L, ra + 1, ra);
            setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(0)), LU_TAG_ITERATOR);
            setnilvalue(ra);
        }
        else
        {
            VM_PROTECT_PC(); // next call always errors
            luaG_typeerror(L, ra, "iterate over");
        }
    }

    pc += LUAU_INSN_D(insn);
    LUAU_ASSERT(unsigned(pc - cl->l.p->code) < unsigned(cl->l.p->sizecode));
    return pc;
}

void executeGETVARARGSMultRet(lua_State* L, const Instruction* pc, StkId base, int rai)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    int n = cast_int(base - L->ci->func) - cl->l.p->numparams - 1;

    VM_PROTECT(luaD_checkstack(L, n));
    StkId ra = VM_REG(rai); // previous call may change the stack

    for (int j = 0; j < n; j++)
        setobj2s(L, ra + j, base - n + j);

    L->top = ra + n;
}

void executeGETVARARGSConst(lua_State* L, StkId base, int rai, int b)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    int n = cast_int(base - L->ci->func) - cl->l.p->numparams - 1;

    StkId ra = VM_REG(rai);

    for (int j = 0; j < b && j < n; j++)
        setobj2s(L, ra + j, base - n + j);
    for (int j = n; j < b; j++)
        setnilvalue(ra + j);
}

const Instruction* executeDUPCLOSURE(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    StkId ra = VM_REG(LUAU_INSN_A(insn));
    TValue* kv = VM_KV(LUAU_INSN_D(insn));

    Closure* kcl = clvalue(kv);

    VM_PROTECT_PC(); // luaF_newLclosure may fail due to OOM

    // clone closure if the environment is not shared
    // note: we save closure to stack early in case the code below wants to capture it by value
    Closure* ncl = (kcl->env == cl->env) ? kcl : luaF_newLclosure(L, kcl->nupvalues, cl->env, kcl->l.p);
    setclvalue(L, ra, ncl);

    // this loop does three things:
    // - if the closure was created anew, it just fills it with upvalues
    // - if the closure from the constant table is used, it fills it with upvalues so that it can be shared in the future
    // - if the closure is reused, it checks if the reuse is safe via rawequal, and falls back to duplicating the closure
    // normally this would use two separate loops, for reuse check and upvalue setup, but MSVC codegen goes crazy if you do that
    for (int ui = 0; ui < kcl->nupvalues; ++ui)
    {
        Instruction uinsn = pc[ui];
        LUAU_ASSERT(LUAU_INSN_OP(uinsn) == LOP_CAPTURE);
        LUAU_ASSERT(LUAU_INSN_A(uinsn) == LCT_VAL || LUAU_INSN_A(uinsn) == LCT_UPVAL);

        TValue* uv = (LUAU_INSN_A(uinsn) == LCT_VAL) ? VM_REG(LUAU_INSN_B(uinsn)) : VM_UV(LUAU_INSN_B(uinsn));

        // check if the existing closure is safe to reuse
        if (ncl == kcl && luaO_rawequalObj(&ncl->l.uprefs[ui], uv))
            continue;

        // lazily clone the closure and update the upvalues
        if (ncl == kcl && kcl->preload == 0)
        {
            ncl = luaF_newLclosure(L, kcl->nupvalues, cl->env, kcl->l.p);
            setclvalue(L, ra, ncl);

            ui = -1; // restart the loop to fill all upvalues
            continue;
        }

        // this updates a newly created closure, or an existing closure created during preload, in which case we need a barrier
        setobj(L, &ncl->l.uprefs[ui], uv);
        luaC_barrier(L, ncl, uv);
    }

    // this is a noop if ncl is newly created or shared successfully, but it has to run after the closure is preloaded for the first time
    ncl->preload = 0;

    if (kcl != ncl)
        VM_PROTECT(luaC_checkGC(L));

    pc += kcl->nupvalues;
    return pc;
}

const Instruction* executePREPVARARGS(lua_State* L, const Instruction* pc, StkId base, TValue* k)
{
    [[maybe_unused]] Closure* cl = clvalue(L->ci->func);
    Instruction insn = *pc++;
    int numparams = LUAU_INSN_A(insn);

    // all fixed parameters are copied after the top so we need more stack space
    VM_PROTECT(luaD_checkstack(L, cl->stacksize + numparams));

    // the caller must have filled extra fixed arguments with nil
    LUAU_ASSERT(cast_int(L->top - base) >= numparams);

    // move fixed parameters to final position
    StkId fixed = base; // first fixed argument
    base = L->top;      // final position of first argument

    for (int i = 0; i < numparams; ++i)
    {
        setobj2s(L, base + i, fixed + i);
        setnilvalue(fixed + i);
    }

    // rewire our stack frame to point to the new base
    L->ci->base = base;
    L->ci->top = base + cl->stacksize;

    L->base = base;
    L->top = L->ci->top;
    return pc;
}

} // namespace CodeGen
} // namespace Luau
