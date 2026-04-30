// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "EmitCommonX64.h"

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrCallWrapperX64.h"
#include "Luau/IrData.h"
#include "Luau/IrRegAllocX64.h"
#include "Luau/IrUtils.h"

#include "NativeState.h"

#include "lgc.h"
#include "lstate.h"

#include <utility>

LUAU_DYNAMIC_FASTFLAGVARIABLE(AddReturnExectargetCheck, false)
LUAU_FASTFLAG(LuauCodegenSuggestArgumentRegisterX64)

namespace Luau
{
namespace CodeGen
{
namespace X64
{

void jumpOnNumberCmp(AssemblyBuilderX64& build, RegisterX64 tmp, OperandX64 lhs, OperandX64 rhs, IrCondition cond, Label& label, bool floatPrecision)
{
    // Refresher on comi/ucomi EFLAGS:
    // all zero: greater
    // CF only: less
    // ZF only: equal
    // PF+CF+ZF: unordered (NaN)

    // To avoid the lack of conditional jumps that check for "greater" conditions in IEEE 754 compliant way, we use "less" forms to emulate these
    if (cond == IrCondition::Greater || cond == IrCondition::GreaterEqual || cond == IrCondition::NotGreater || cond == IrCondition::NotGreaterEqual)
        std::swap(lhs, rhs);

    if (floatPrecision)
    {
        if (rhs.cat == CategoryX64::reg)
        {
            build.vucomiss(rhs, lhs);
        }
        else
        {
            build.vmovss(tmp, rhs);
            build.vucomiss(tmp, lhs);
        }
    }
    else
    {
        if (rhs.cat == CategoryX64::reg)
        {
            build.vucomisd(rhs, lhs);
        }
        else
        {
            build.vmovsd(tmp, rhs);
            build.vucomisd(tmp, lhs);
        }
    }

    // Keep in mind that 'Not' conditions want 'true' for comparisons with NaN
    // And because of NaN, integer check interchangeability like 'not less or equal' <-> 'greater' does not hold
    switch (cond)
    {
    case IrCondition::NotLessEqual:
    case IrCondition::NotGreaterEqual:
        // (b < a) is the same as !(a <= b). jnae checks CF=1 which means < or NaN
        build.jcc(ConditionX64::NotAboveEqual, label);
        break;
    case IrCondition::LessEqual:
    case IrCondition::GreaterEqual:
        // (b >= a) is the same as (a <= b). jae checks CF=0 which means >= and not NaN
        build.jcc(ConditionX64::AboveEqual, label);
        break;
    case IrCondition::NotLess:
    case IrCondition::NotGreater:
        // (b <= a) is the same as !(a < b). jna checks CF=1 or ZF=1 which means <= or NaN
        build.jcc(ConditionX64::NotAbove, label);
        break;
    case IrCondition::Less:
    case IrCondition::Greater:
        // (b > a) is the same as (a < b). ja checks CF=0 and ZF=0 which means > and not NaN
        build.jcc(ConditionX64::Above, label);
        break;
    case IrCondition::NotEqual:
        // ZF=0 or PF=1 means != or NaN
        build.jcc(ConditionX64::NotZero, label);
        build.jcc(ConditionX64::Parity, label);
        break;
    default:
        CODEGEN_ASSERT(!"Unsupported condition");
    }
}

ConditionX64 getConditionInt(IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return ConditionX64::Equal;
    case IrCondition::NotEqual:
        return ConditionX64::NotEqual;
    case IrCondition::Less:
        return ConditionX64::Less;
    case IrCondition::NotLess:
        return ConditionX64::NotLess;
    case IrCondition::LessEqual:
        return ConditionX64::LessEqual;
    case IrCondition::NotLessEqual:
        return ConditionX64::NotLessEqual;
    case IrCondition::Greater:
        return ConditionX64::Greater;
    case IrCondition::NotGreater:
        return ConditionX64::NotGreater;
    case IrCondition::GreaterEqual:
        return ConditionX64::GreaterEqual;
    case IrCondition::NotGreaterEqual:
        return ConditionX64::NotGreaterEqual;
    case IrCondition::UnsignedLess:
        return ConditionX64::Below;
    case IrCondition::UnsignedLessEqual:
        return ConditionX64::BelowEqual;
    case IrCondition::UnsignedGreater:
        return ConditionX64::Above;
    case IrCondition::UnsignedGreaterEqual:
        return ConditionX64::AboveEqual;
    default:
        CODEGEN_ASSERT(!"Unsupported condition");
        return ConditionX64::Zero;
    }
}

void getTableNodeAtCachedSlot(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 node, RegisterX64 table, int pcpos)
{
    CODEGEN_ASSERT(tmp != node);
    CODEGEN_ASSERT(table != node);

    build.mov(node, qword[table + offsetof(LuaTable, node)]);

    // compute cached slot
    build.mov(tmp, sCode);
    build.movzx(dwordReg(tmp), byte[tmp + pcpos * sizeof(Instruction) + kOffsetOfInstructionC]);
    build.and_(byteReg(tmp), byte[table + offsetof(LuaTable, nodemask8)]);

    // LuaNode* n = &h->node[slot];
    build.shl(dwordReg(tmp), kLuaNodeSizeLog2);
    build.add(node, tmp);
}

void convertNumberToIndexOrJump(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 numd, RegisterX64 numi, Label& label)
{
    CODEGEN_ASSERT(numi.size == SizeX64::dword);

    // Convert to integer, NaN is converted into 0x80000000
    build.vcvttsd2si(numi, numd);

    // Convert that integer back to double
    build.vcvtsi2sd(tmp, numd, numi);

    build.vucomisd(tmp, numd); // Sets ZF=1 if equal or NaN
    // We don't need non-integer values
    // But to skip the PF=1 check, we proceed with NaN because 0x80000000 index is out of bounds
    build.jcc(ConditionX64::NotZero, label);
}

void callArithHelper(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, OperandX64 b, OperandX64 c, TMS tm)
{
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::qword, rState);
    callWrap.addArgument(SizeX64::qword, luauRegAddress(ra));
    callWrap.addArgument(SizeX64::qword, b);
    callWrap.addArgument(SizeX64::qword, c);

    switch (tm)
    {
    case TM_ADD:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithadd)]);
        break;
    case TM_SUB:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithsub)]);
        break;
    case TM_MUL:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithmul)]);
        break;
    case TM_DIV:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithdiv)]);
        break;
    case TM_IDIV:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithidiv)]);
        break;
    case TM_MOD:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithmod)]);
        break;
    case TM_POW:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithpow)]);
        break;
    case TM_UNM:
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_doarithunm)]);
        break;
    default:
        CODEGEN_ASSERT(!"Invalid doarith helper operation tag");
        break;
    }

    emitUpdateBase(build);
}

void callLengthHelper(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int rb)
{
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::qword, rState);
    callWrap.addArgument(SizeX64::qword, luauRegAddress(ra));
    callWrap.addArgument(SizeX64::qword, luauRegAddress(rb));
    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_dolen)]);

    emitUpdateBase(build);
}

void callGetTable(IrRegAllocX64& regs, AssemblyBuilderX64& build, int rb, OperandX64 c, int ra)
{
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::qword, rState);
    callWrap.addArgument(SizeX64::qword, luauRegAddress(rb));
    callWrap.addArgument(SizeX64::qword, c);
    callWrap.addArgument(SizeX64::qword, luauRegAddress(ra));
    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_gettable)]);

    emitUpdateBase(build);
}

void callSetTable(IrRegAllocX64& regs, AssemblyBuilderX64& build, int rb, OperandX64 c, int ra)
{
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::qword, rState);
    callWrap.addArgument(SizeX64::qword, luauRegAddress(rb));
    callWrap.addArgument(SizeX64::qword, c);
    callWrap.addArgument(SizeX64::qword, luauRegAddress(ra));
    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_settable)]);

    emitUpdateBase(build);
}

void checkObjectBarrierConditions(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 object, RegisterX64 ra, IrOp raOp, int ratag, Label& skip)
{
    // Barrier should've been optimized away if we know that it's not collectable, checking for correctness
    if (ratag == -1 || !isGCO(ratag))
    {
        // iscollectable(ra)
        if (raOp.kind == IrOpKind::Inst)
        {
            build.vpextrd(dwordReg(tmp), ra, 3);
            build.cmp(dwordReg(tmp), LUA_TSTRING);
        }
        else
        {
            OperandX64 tag = (raOp.kind == IrOpKind::VmReg) ? luauRegTag(vmRegOp(raOp)) : luauConstantTag(vmConstOp(raOp));
            build.cmp(tag, LUA_TSTRING);
        }

        build.jcc(ConditionX64::Less, skip);
    }

    // isblack(obj2gco(o))
    build.test(byte[object + offsetof(GCheader, marked)], bitmask(BLACKBIT));
    build.jcc(ConditionX64::Zero, skip);

    // iswhite(gcvalue(ra))
    if (raOp.kind == IrOpKind::Inst)
    {
        build.vmovq(tmp, ra);
    }
    else
    {
        OperandX64 value = (raOp.kind == IrOpKind::VmReg) ? luauRegValue(vmRegOp(raOp)) : luauConstantValue(vmConstOp(raOp));
        build.mov(tmp, value);
    }
    build.test(byte[tmp + offsetof(GCheader, marked)], bit2mask(WHITE0BIT, WHITE1BIT));
    build.jcc(ConditionX64::Zero, skip);
}

void callBarrierObject(IrRegAllocX64& regs, AssemblyBuilderX64& build, RegisterX64 object, IrOp objectOp, RegisterX64 ra, IrOp raOp, int ratag)
{
    Label skip;

    ScopedRegX64 tmp{regs, SizeX64::qword};
    checkObjectBarrierConditions(build, tmp.reg, object, ra, raOp, ratag, skip);

    {
        ScopedSpills spillGuard(regs);

        IrCallWrapperX64 callWrap(regs, build);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, object, objectOp);
        callWrap.addArgument(SizeX64::qword, tmp);
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaC_barrierf)]);
    }

    build.setLabel(skip);
}

void callBarrierTableFast(IrRegAllocX64& regs, AssemblyBuilderX64& build, RegisterX64 table, IrOp tableOp)
{
    Label skip;

    // isblack(obj2gco(t))
    build.test(byte[table + offsetof(GCheader, marked)], bitmask(BLACKBIT));
    build.jcc(ConditionX64::Zero, skip);

    {
        ScopedSpills spillGuard(regs);

        IrCallWrapperX64 callWrap(regs, build);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, table, tableOp);
        callWrap.addArgument(SizeX64::qword, addr[table + offsetof(LuaTable, gclist)]);
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaC_barrierback)]);
    }

    build.setLabel(skip);
}

void callStepGc(IrRegAllocX64& regs, AssemblyBuilderX64& build)
{
    Label skip;

    {
        ScopedRegX64 tmp1{regs, SizeX64::qword};
        ScopedRegX64 tmp2{regs, SizeX64::qword};

        build.mov(tmp1.reg, qword[rState + offsetof(lua_State, global)]);
        build.mov(tmp2.reg, qword[tmp1.reg + offsetof(global_State, totalbytes)]);
        build.cmp(tmp2.reg, qword[tmp1.reg + offsetof(global_State, GCthreshold)]);
        build.jcc(ConditionX64::Below, skip);
    }

    {
        ScopedSpills spillGuard(regs);

        IrCallWrapperX64 callWrap(regs, build);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::dword, 1);
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaC_step)]);
        emitUpdateBase(build);
    }

    build.setLabel(skip);
}

void emitClearNativeFlag(AssemblyBuilderX64& build)
{
    build.mov(rax, qword[rState + offsetof(lua_State, ci)]);
    build.and_(dword[rax + offsetof(CallInfo, flags)], ~LUA_CALLINFO_NATIVE);
}

void emitExit(AssemblyBuilderX64& build, bool continueInVm)
{
    if (continueInVm)
        build.mov(eax, 1);
    else
        build.xor_(eax, eax);

    build.jmp(qword[rNativeContext + offsetof(NativeContext, gateExit)]);
}

void emitUpdateBase(AssemblyBuilderX64& build)
{
    build.mov(rBase, qword[rState + offsetof(lua_State, base)]);
}

void emitInterrupt(AssemblyBuilderX64& build)
{
    // rax = pcpos + 1
    // rbx = return address in native code

    // note: rbx is non-volatile so it will be saved across interrupt call automatically

    RegisterX64 rArg1{};
    RegisterX64 rArg2{};
    if (FFlag::LuauCodegenSuggestArgumentRegisterX64)
    {
        rArg1 = IrCallWrapperX64::suggestArgumentRegister<0>(SizeX64::qword, build);
        rArg2 = IrCallWrapperX64::suggestArgumentRegister<1>(SizeX64::qword, build);
    }
    else
    {
        rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
        rArg2 = (build.abi == ABIX64::Windows) ? rdx : rsi;
    }

    Label skip;

    // Update L->ci->savedpc; required in case interrupt errors
    build.mov(rcx, sCode);
    build.lea(rcx, addr[rcx + rax * sizeof(Instruction)]);
    build.mov(rax, qword[rState + offsetof(lua_State, ci)]);
    build.mov(qword[rax + offsetof(CallInfo, savedpc)], rcx);

    // Load interrupt handler; it may be nullptr in case the update raced with the check before we got here
    build.mov(rax, qword[rState + offsetof(lua_State, global)]);
    build.mov(rax, qword[rax + offsetof(global_State, cb.interrupt)]);
    build.test(rax, rax);
    build.jcc(ConditionX64::Zero, skip);

    // Call interrupt
    build.mov(rArg1, rState);
    build.mov(dwordReg(rArg2), -1);
    build.call(rax);

    // Check if we need to exit
    build.mov(al, byte[rState + offsetof(lua_State, status)]);
    build.test(al, al);
    build.jcc(ConditionX64::Zero, skip);

    build.mov(rax, qword[rState + offsetof(lua_State, ci)]);
    build.sub(qword[rax + offsetof(CallInfo, savedpc)], sizeof(Instruction));
    emitExit(build, /* continueInVm */ false);

    build.setLabel(skip);

    emitUpdateBase(build); // interrupt may have reallocated stack

    build.jmp(rbx);
}

void emitFallback(IrRegAllocX64& regs, AssemblyBuilderX64& build, int offset, int pcpos)
{
    // fallback(L, instruction, base, k)
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::qword, rState);

    RegisterX64 reg = callWrap.suggestNextArgumentRegister(SizeX64::qword);
    build.mov(reg, sCode);
    callWrap.addArgument(SizeX64::qword, addr[reg + pcpos * sizeof(Instruction)]);

    callWrap.addArgument(SizeX64::qword, rBase);
    callWrap.addArgument(SizeX64::qword, rConstants);
    callWrap.call(qword[rNativeContext + offset]);

    emitUpdateBase(build);
}

void emitUpdatePcForExit(AssemblyBuilderX64& build)
{
    // edx = pcpos * sizeof(Instruction)
    build.add(rdx, sCode);
    build.mov(rax, qword[rState + offsetof(lua_State, ci)]);
    build.mov(qword[rax + offsetof(CallInfo, savedpc)], rdx);
}

void emitReturn(AssemblyBuilderX64& build, ModuleHelpers& helpers)
{
    // input: res in rdi, number of written values in ecx
    RegisterX64 res = rdi;
    RegisterX64 written = ecx;

    RegisterX64 ci = r8;
    RegisterX64 cip = r9;
    RegisterX64 nresults = esi;

    build.mov(ci, qword[rState + offsetof(lua_State, ci)]);
    build.lea(cip, addr[ci - sizeof(CallInfo)]);

    // nresults = ci->nresults
    build.mov(nresults, dword[ci + offsetof(CallInfo, nresults)]);

    Label skipResultCopy;

    // Fill the rest of the expected results (nresults - written) with 'nil'
    RegisterX64 counter = written;
    build.sub(counter, nresults); // counter = -(nresults - written)
    build.jcc(ConditionX64::GreaterEqual, skipResultCopy);

    Label repeatNilLoop = build.setLabel();
    build.mov(dword[res + offsetof(TValue, tt)], LUA_TNIL);
    build.add(res, sizeof(TValue));
    build.inc(counter);
    build.jcc(ConditionX64::NotZero, repeatNilLoop);

    build.setLabel(skipResultCopy);

    build.mov(qword[rState + offsetof(lua_State, ci)], cip);     // L->ci = cip
    build.mov(rBase, qword[cip + offsetof(CallInfo, base)]);     // sync base = L->base while we have a chance
    build.mov(qword[rState + offsetof(lua_State, base)], rBase); // L->base = cip->base

    Label skipFixedRetTop;
    build.test(nresults, nresults);                       // test here will set SF=1 for a negative number and it always sets OF to 0
    build.jcc(ConditionX64::Less, skipFixedRetTop);       // jl jumps if SF != OF
    build.mov(res, qword[cip + offsetof(CallInfo, top)]); // res = cip->top
    build.setLabel(skipFixedRetTop);

    build.mov(qword[rState + offsetof(lua_State, top)], res); // L->top = res

    // Unlikely, but this might be the last return from VM
    build.test(byte[ci + offsetof(CallInfo, flags)], LUA_CALLINFO_RETURN);
    build.jcc(ConditionX64::NotZero, helpers.exitNoContinueVm);

    // Returning back to the previous function is a bit tricky
    // Registers alive: r9 (cip)
    RegisterX64 proto = rcx;
    RegisterX64 execdata = rbx;
    RegisterX64 exectarget = r10;

    // Change closure
    build.mov(rax, qword[cip + offsetof(CallInfo, func)]);
    build.mov(rax, qword[rax + offsetof(TValue, value.gc)]);
    build.mov(sClosure, rax);

    build.mov(proto, qword[rax + offsetof(Closure, l.p)]);

    build.mov(execdata, qword[proto + offsetof(Proto, execdata)]);

    build.test(byte[cip + offsetof(CallInfo, flags)], LUA_CALLINFO_NATIVE);
    build.jcc(ConditionX64::Zero, helpers.exitContinueVm); // Continue in interpreter if function has no native data

    if (DFFlag::AddReturnExectargetCheck)
    {
        build.mov(exectarget, qword[proto + offsetof(Proto, exectarget)]);
        build.test(exectarget, exectarget);
        build.jcc(ConditionX64::Zero, helpers.exitContinueVmClearNativeFlag);
    }

    // Change constants
    build.mov(rConstants, qword[proto + offsetof(Proto, k)]);

    // Change code
    build.mov(rdx, qword[proto + offsetof(Proto, code)]);
    build.mov(sCode, rdx);

    build.mov(rax, qword[cip + offsetof(CallInfo, savedpc)]);

    // To get instruction index from instruction pointer, we need to divide byte offset by 4
    // But we will actually need to scale instruction index by 4 back to byte offset later so it cancels out
    build.sub(rax, rdx);

    // Get new instruction location and jump to it
    build.mov(edx, dword[execdata + rax]);

    if (DFFlag::AddReturnExectargetCheck)
    {
        build.add(rdx, exectarget);
    }
    else
    {
        build.add(rdx, qword[proto + offsetof(Proto, exectarget)]);
    }
    build.jmp(rdx);
}


} // namespace X64
} // namespace CodeGen
} // namespace Luau
