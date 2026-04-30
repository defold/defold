// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "EmitInstructionX64.h"

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrCallWrapperX64.h"
#include "Luau/IrData.h"
#include "Luau/IrRegAllocX64.h"
#include "Luau/RegisterX64.h"

#include "EmitCommonX64.h"
#include "NativeState.h"

#include "lstate.h"

LUAU_FASTFLAGVARIABLE(LuauCodeGenCallWrapperEmitInst)
LUAU_FASTFLAG(LuauCodegenSuggestArgumentRegisterX64)

namespace Luau
{
namespace CodeGen
{
namespace X64
{

void emitInstCall(IrRegAllocX64& regs, AssemblyBuilderX64& build, ModuleHelpers& helpers, int ra, int nparams, int nresults)
{
    if (FFlag::LuauCodeGenCallWrapperEmitInst)
    {
        IrCallWrapperX64 callWrapper(regs, build);

        callWrapper.addArgument(SizeX64::qword, rState);
        callWrapper.addArgument(SizeX64::qword, luauRegAddress(ra));
        if (nparams == LUA_MULTRET)
            callWrapper.addArgument(SizeX64::qword, qword[rState + offsetof(lua_State, top)]);
        else
            callWrapper.addArgument(SizeX64::qword, luauRegAddress(ra + 1 + nparams));

        callWrapper.addArgument(SizeX64::dword, nresults);
        callWrapper.call(qword[rNativeContext + offsetof(NativeContext, callProlog)]);
    }
    else
    {
        RegisterX64 rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
        RegisterX64 rArg2 = (build.abi == ABIX64::Windows) ? rdx : rsi;
        RegisterX64 rArg3 = (build.abi == ABIX64::Windows) ? r8 : rdx;
        RegisterX64 rArg4 = (build.abi == ABIX64::Windows) ? r9 : rcx;

        build.mov(rArg1, rState);
        build.lea(rArg2, luauRegAddress(ra));

        if (nparams == LUA_MULTRET)
            build.mov(rArg3, qword[rState + offsetof(lua_State, top)]);
        else
            build.lea(rArg3, luauRegAddress(ra + 1 + nparams));

        build.mov(dwordReg(rArg4), nresults);
        build.call(qword[rNativeContext + offsetof(NativeContext, callProlog)]);
    }
    RegisterX64 ccl = rax; // Returned from callProlog

    emitUpdateBase(build);

    Label cFuncCall;

    build.test(byte[ccl + offsetof(Closure, isC)], 1);
    build.jcc(ConditionX64::NotZero, cFuncCall);

    {
        RegisterX64 proto = rcx; // Sync with emitContinueCallInVm
        RegisterX64 ci = rdx;
        RegisterX64 argi = rsi;
        RegisterX64 argend = rdi;

        build.mov(proto, qword[ccl + offsetof(Closure, l.p)]);

        // Switch current Closure
        build.mov(sClosure, ccl); // Last use of 'ccl'

        build.mov(ci, qword[rState + offsetof(lua_State, ci)]);

        Label fillnil, exitfillnil;

        // argi = L->top
        build.mov(argi, qword[rState + offsetof(lua_State, top)]);

        // argend = L->base + p->numparams
        build.movzx(eax, byte[proto + offsetof(Proto, numparams)]);
        build.shl(eax, kTValueSizeLog2);
        build.lea(argend, addr[rBase + rax]);

        // while (argi < argend) setnilvalue(argi++);
        build.setLabel(fillnil);
        build.cmp(argi, argend);
        build.jcc(ConditionX64::NotBelow, exitfillnil);

        build.mov(dword[argi + offsetof(TValue, tt)], LUA_TNIL);
        build.add(argi, sizeof(TValue));
        build.jmp(fillnil); // This loop rarely runs so it's not worth repeating cmp/jcc

        build.setLabel(exitfillnil);

        // Set L->top to ci->top as most function expect (no vararg)
        build.mov(rax, qword[ci + offsetof(CallInfo, top)]);

        // But if it is vararg, update it to 'argi'
        Label skipVararg;

        build.test(byte[proto + offsetof(Proto, is_vararg)], 1);
        build.jcc(ConditionX64::Zero, skipVararg);
        build.mov(rax, argi);

        build.setLabel(skipVararg);

        build.mov(qword[rState + offsetof(lua_State, top)], rax);

        // Switch current code
        // ci->savedpc = p->code;
        build.mov(rax, qword[proto + offsetof(Proto, code)]);
        build.mov(sCode, rax); // note: this needs to be before the next store for optimal performance
        build.mov(qword[ci + offsetof(CallInfo, savedpc)], rax);

        // Switch current constants
        build.mov(rConstants, qword[proto + offsetof(Proto, k)]);

        // Get native function entry
        build.mov(rax, qword[proto + offsetof(Proto, exectarget)]);
        build.test(rax, rax);
        build.jcc(ConditionX64::Zero, helpers.exitContinueVm);

        // Mark call frame as native
        build.mov(dword[ci + offsetof(CallInfo, flags)], LUA_CALLINFO_NATIVE);

        build.jmp(rax);
    }

    build.setLabel(cFuncCall);

    {
        // results = ccl->c.f(L);
        if (FFlag::LuauCodeGenCallWrapperEmitInst)
        {
            regs.takeReg(ccl, kInvalidInstIdx); // ccl = rax, returned from callProlog, have to take ownership so the wrapper can free it
            IrCallWrapperX64 callWrapper(regs, build);
            callWrapper.addArgument(SizeX64::qword, rState);
            callWrapper.call(qword[ccl + offsetof(Closure, c.f)]); // Last use of 'ccl'
        }
        else
        {
            RegisterX64 rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
            build.mov(rArg1, rState);
            build.call(qword[ccl + offsetof(Closure, c.f)]); // Last use of 'ccl'
        }
        RegisterX64 results = eax;

        build.test(results, results);                            // test here will set SF=1 for a negative number and it always sets OF to 0
        build.jcc(ConditionX64::Less, helpers.exitNoContinueVm); // jl jumps if SF != OF

        // We have special handling for small number of expected results below
        if (nresults != 0 && nresults != 1)
        {
            if (FFlag::LuauCodeGenCallWrapperEmitInst)
            {
                regs.takeReg(results, kInvalidInstIdx); // results = eax, returned from c.f, have to take ownership so the wrapper can free it
                IrCallWrapperX64 callWrapper(regs, build);
                callWrapper.addArgument(SizeX64::qword, rState);
                callWrapper.addArgument(SizeX64::dword, nresults);
                callWrapper.addArgument(SizeX64::dword, results);
                callWrapper.call(qword[rNativeContext + offsetof(NativeContext, callEpilogC)]);
            }
            else
            {
                RegisterX64 rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
                RegisterX64 rArg2 = (build.abi == ABIX64::Windows) ? rdx : rsi;
                RegisterX64 rArg3 = (build.abi == ABIX64::Windows) ? r8 : rdx;

                build.mov(rArg1, rState);
                build.mov(dwordReg(rArg2), nresults);
                build.mov(dwordReg(rArg3), results);
                build.call(qword[rNativeContext + offsetof(NativeContext, callEpilogC)]);
            }

            emitUpdateBase(build);
            return;
        }

        RegisterX64 ci = rdx;
        RegisterX64 cip = rcx;
        RegisterX64 vali = rsi;

        build.mov(ci, qword[rState + offsetof(lua_State, ci)]);
        build.lea(cip, addr[ci - sizeof(CallInfo)]);

        // L->base = cip->base
        build.mov(rBase, qword[cip + offsetof(CallInfo, base)]);
        build.mov(qword[rState + offsetof(lua_State, base)], rBase);

        if (nresults == 1)
        {
            // Opportunistically copy the result we expected from (L->top - results)
            build.mov(vali, qword[rState + offsetof(lua_State, top)]);
            build.shl(results, kTValueSizeLog2);
            build.sub(vali, qwordReg(results));
            build.vmovups(xmm0, xmmword[vali]);
            build.vmovups(luauReg(ra), xmm0);

            Label skipnil;

            // If there was no result, override the value with 'nil'
            build.test(results, results);
            build.jcc(ConditionX64::NotZero, skipnil);
            build.mov(luauRegTag(ra), LUA_TNIL);
            build.setLabel(skipnil);
        }

        // L->ci = cip
        build.mov(qword[rState + offsetof(lua_State, ci)], cip);

        // L->top = cip->top
        build.mov(rax, qword[cip + offsetof(CallInfo, top)]);
        build.mov(qword[rState + offsetof(lua_State, top)], rax);
    }
}

void emitInstReturn(AssemblyBuilderX64& build, ModuleHelpers& helpers, int ra, int actualResults, bool functionVariadic)
{
    RegisterX64 res = rdi;
    RegisterX64 written = ecx;

    if (functionVariadic)
    {
        build.mov(res, qword[rState + offsetof(lua_State, ci)]);
        build.mov(res, qword[res + offsetof(CallInfo, func)]);
    }
    else if (actualResults != 1)
        build.lea(res, addr[rBase - sizeof(TValue)]); // invariant: ci->func + 1 == ci->base for non-variadic frames

    if (actualResults == 0)
    {
        build.xor_(written, written);
        build.jmp(helpers.return_);
    }
    else if (actualResults == 1 && !functionVariadic)
    {
        // fast path: minimizes res adjustments
        // note that we skipped res computation for this specific case above
        build.vmovups(xmm0, luauReg(ra));
        build.vmovups(xmmword[rBase - sizeof(TValue)], xmm0);
        build.mov(res, rBase);
        build.mov(written, 1);
        build.jmp(helpers.return_);
    }
    else if (actualResults >= 1 && actualResults <= 3)
    {
        for (int r = 0; r < actualResults; ++r)
        {
            build.vmovups(xmm0, luauReg(ra + r));
            build.vmovups(xmmword[res + r * sizeof(TValue)], xmm0);
        }
        build.add(res, actualResults * sizeof(TValue));
        build.mov(written, actualResults);
        build.jmp(helpers.return_);
    }
    else
    {
        RegisterX64 vali = rax;
        RegisterX64 valend = rdx;

        // vali = ra
        build.lea(vali, luauRegAddress(ra));

        // Copy as much as possible for MULTRET calls, and only as much as needed otherwise
        if (actualResults == LUA_MULTRET)
            build.mov(valend, qword[rState + offsetof(lua_State, top)]); // valend = L->top
        else
            build.lea(valend, luauRegAddress(ra + actualResults)); // valend = ra + actualResults

        build.xor_(written, written);

        Label repeatValueLoop, exitValueLoop;

        if (actualResults == LUA_MULTRET)
        {
            build.cmp(vali, valend);
            build.jcc(ConditionX64::NotBelow, exitValueLoop);
        }

        build.setLabel(repeatValueLoop);
        build.vmovups(xmm0, xmmword[vali]);
        build.vmovups(xmmword[res], xmm0);
        build.add(vali, sizeof(TValue));
        build.add(res, sizeof(TValue));
        build.inc(written);
        build.cmp(vali, valend);
        build.jcc(ConditionX64::Below, repeatValueLoop);

        build.setLabel(exitValueLoop);
        build.jmp(helpers.return_);
    }
}

void emitInstSetList(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int rb, int count, uint32_t index, int knownSize)
{
    OperandX64 last = index + count - 1;

    // Using non-volatile 'rbx' for dynamic 'count' value (for LUA_MULTRET) to skip later recomputation
    // We also keep 'count' scaled by sizeof(TValue) here as it helps in the loop below
    RegisterX64 cscaled = rbx;

    if (count == LUA_MULTRET)
    {
        RegisterX64 tmp = rax;

        // count = L->top - rb
        build.mov(cscaled, qword[rState + offsetof(lua_State, top)]);
        build.lea(tmp, luauRegAddress(rb));
        build.sub(cscaled, tmp); // Using byte difference

        // L->top = L->ci->top
        build.mov(tmp, qword[rState + offsetof(lua_State, ci)]);
        build.mov(tmp, qword[tmp + offsetof(CallInfo, top)]);
        build.mov(qword[rState + offsetof(lua_State, top)], tmp);

        // last = index + count - 1;
        last = edx;
        build.mov(last, dwordReg(cscaled));
        build.shr(last, kTValueSizeLog2);
        build.add(last, index - 1);
    }

    RegisterX64 table = regs.takeReg(rax, kInvalidInstIdx);

    build.mov(table, luauRegValue(ra));

    if (count == LUA_MULTRET || knownSize < 0 || knownSize < int(index + count - 1))
    {
        Label skipResize;

        // Resize if h->sizearray < last
        build.cmp(dword[table + offsetof(LuaTable, sizearray)], last);
        build.jcc(ConditionX64::NotBelow, skipResize);

        if (FFlag::LuauCodeGenCallWrapperEmitInst)
        {
            if (count == LUA_MULTRET)
                regs.takeReg(last.base, kInvalidInstIdx); // last = edx, preloaded above, have to take ownership so the wrapper can free it
            IrCallWrapperX64 callWrapper(regs, build);
            callWrapper.addArgument(SizeX64::qword, rState);
            callWrapper.addArgument(SizeX64::qword, table);
            callWrapper.addArgument(SizeX64::dword, last);
            callWrapper.call(qword[rNativeContext + offsetof(NativeContext, luaH_resizearray)]);
            // InstCallWrapperX64 freed table's register (rax) as a consumed source
            // we need to retake it so that the subsequent build.mov reload and callBarrierTableFast can track ownership correctly
            table = regs.takeReg(rax, kInvalidInstIdx);
        }
        else
        {
            RegisterX64 rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
            RegisterX64 rArg2 = (build.abi == ABIX64::Windows) ? rdx : rsi;
            RegisterX64 rArg3 = (build.abi == ABIX64::Windows) ? r8 : rdx;

            // Argument setup reordered to avoid conflicts
            CODEGEN_ASSERT(rArg3 != table);
            build.mov(dwordReg(rArg3), last);
            build.mov(rArg2, table);
            build.mov(rArg1, rState);
            build.call(qword[rNativeContext + offsetof(NativeContext, luaH_resizearray)]);
        }
        build.mov(table, luauRegValue(ra)); // Reload clobbered register value

        build.setLabel(skipResize);
    }

    RegisterX64 arrayDst = rdx;
    RegisterX64 offset = rcx;

    build.mov(arrayDst, qword[table + offsetof(LuaTable, array)]);

    const int kUnrollSetListLimit = 4;

    if (count != LUA_MULTRET && count <= kUnrollSetListLimit)
    {
        for (int i = 0; i < count; ++i)
        {
            // setobj2t(L, &array[index + i - 1], rb + i);
            build.vmovups(xmm0, luauRegValue(rb + i));
            build.vmovups(xmmword[arrayDst + (index + i - 1) * sizeof(TValue)], xmm0);
        }
    }
    else
    {
        CODEGEN_ASSERT(count != 0);

        build.xor_(offset, offset);
        if (index != 1)
            build.add(arrayDst, (index - 1) * sizeof(TValue));

        Label repeatLoop, endLoop;
        OperandX64 limit = count == LUA_MULTRET ? cscaled : OperandX64(count * sizeof(TValue));

        // If c is static, we will always do at least one iteration
        if (count == LUA_MULTRET)
        {
            build.cmp(offset, limit);
            build.jcc(ConditionX64::NotBelow, endLoop);
        }

        build.setLabel(repeatLoop);

        // setobj2t(L, &array[index + i - 1], rb + i);
        build.vmovups(xmm0, xmmword[offset + rBase + rb * sizeof(TValue)]); // luauReg(rb) unwrapped to add offset
        build.vmovups(xmmword[offset + arrayDst], xmm0);

        build.add(offset, sizeof(TValue));
        build.cmp(offset, limit);
        build.jcc(ConditionX64::Below, repeatLoop);

        build.setLabel(endLoop);
    }

    callBarrierTableFast(regs, build, table, {});
}

void emitInstForGLoop(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int aux, Label& loopRepeat)
{
    // ipairs-style traversal is handled in IR
    CODEGEN_ASSERT(aux >= 0);

    // This is a fast-path for builtin table iteration, tag check for 'ra' has to be performed before emitting this instruction

    // Registers are chosen in this way to simplify fallback code for the node part
    RegisterX64 table{};
    RegisterX64 index{};
    if (FFlag::LuauCodegenSuggestArgumentRegisterX64)
    {
        table = IrCallWrapperX64::suggestArgumentRegister<1>(SizeX64::qword, build);
        index = IrCallWrapperX64::suggestArgumentRegister<2>(SizeX64::qword, build);
    }
    else
    {
        table = (build.abi == ABIX64::Windows) ? rdx : rsi;
        index = (build.abi == ABIX64::Windows) ? r8 : rdx;
    }

    RegisterX64 elemPtr = rax;

    build.mov(table, luauRegValue(ra + 1));
    build.mov(index, luauRegValue(ra + 2));

    // &array[index]
    build.mov(dwordReg(elemPtr), dwordReg(index));
    build.shl(dwordReg(elemPtr), kTValueSizeLog2);
    build.add(elemPtr, qword[table + offsetof(LuaTable, array)]);

    // Clear extra variables since we might have more than two
    for (int i = 2; i < aux; ++i)
        build.mov(luauRegTag(ra + 3 + i), LUA_TNIL);

    Label skipArray, skipArrayNil;

    // First we advance index through the array portion
    // while (unsigned(index) < unsigned(sizearray))
    Label arrayLoop = build.setLabel();
    build.cmp(dwordReg(index), dword[table + offsetof(LuaTable, sizearray)]);
    build.jcc(ConditionX64::NotBelow, skipArray);

    // If element is nil, we increment the index; if it's not, we still need 'index + 1' inside
    build.inc(index);

    build.cmp(dword[elemPtr + offsetof(TValue, tt)], LUA_TNIL);
    build.jcc(ConditionX64::Equal, skipArrayNil);

    // setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(index + 1)), LU_TAG_ITERATOR);
    build.mov(luauRegValue(ra + 2), index);
    // Extra should already be set to LU_TAG_ITERATOR
    // Tag should already be set to lightuserdata

    // setnvalue(ra + 3, double(index + 1));
    build.vcvtsi2sd(xmm0, xmm0, dwordReg(index));
    build.vmovsd(luauRegValue(ra + 3), xmm0);
    build.mov(luauRegTag(ra + 3), LUA_TNUMBER);

    // setobj2s(L, ra + 4, e);
    setLuauReg(build, xmm2, ra + 4, xmmword[elemPtr]);

    build.jmp(loopRepeat);

    build.setLabel(skipArrayNil);

    // Index already incremented, advance to next array element
    build.add(elemPtr, sizeof(TValue));
    build.jmp(arrayLoop);

    build.setLabel(skipArray);

    if (FFlag::LuauCodeGenCallWrapperEmitInst)
    {
        regs.takeReg(table, kInvalidInstIdx); // table/index are preloaded above, have to take ownership so the wrapper can free them
        regs.takeReg(index, kInvalidInstIdx);
        IrCallWrapperX64 callWrapper(regs, build);
        callWrapper.addArgument(SizeX64::qword, rState);
        callWrapper.addArgument(SizeX64::qword, table);
        callWrapper.addArgument(SizeX64::qword, index);
        callWrapper.addArgument(SizeX64::qword, luauRegAddress(ra));
        callWrapper.call(qword[rNativeContext + offsetof(NativeContext, forgLoopNodeIter)]);
    }
    else
    {
        RegisterX64 rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
        RegisterX64 rArg4 = (build.abi == ABIX64::Windows) ? r9 : rcx;

        // Call helper to assign next node value or to signal loop exit
        build.mov(rArg1, rState);
        // rArg2 and rArg3 are already set
        build.lea(rArg4, luauRegAddress(ra));
        build.call(qword[rNativeContext + offsetof(NativeContext, forgLoopNodeIter)]);
    }
    build.test(al, al);
    build.jcc(ConditionX64::NotZero, loopRepeat);
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
