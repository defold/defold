// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrLoweringA64.h"

#include "Luau/DenseHash.h"
#include "Luau/IrData.h"
#include "Luau/IrUtils.h"
#include "Luau/LoweringStats.h"

#include "EmitCommonA64.h"
#include "NativeState.h"

#include "lstate.h"
#include "lgc.h"

LUAU_FASTFLAG(LuauCodegenBufferRangeMerge4)
LUAU_FASTFLAG(LuauCodegenBufNoDefTag)
LUAU_FASTFLAG(LuauCodegenCallWrapImproved)


namespace Luau
{
namespace CodeGen
{
namespace A64
{

inline ConditionA64 getConditionFP(IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return ConditionA64::Equal;

    case IrCondition::NotEqual:
        return ConditionA64::NotEqual;

    case IrCondition::Less:
        return ConditionA64::Minus;

    case IrCondition::NotLess:
        return ConditionA64::Plus;

    case IrCondition::LessEqual:
        return ConditionA64::UnsignedLessEqual;

    case IrCondition::NotLessEqual:
        return ConditionA64::UnsignedGreater;

    case IrCondition::Greater:
        return ConditionA64::Greater;

    case IrCondition::NotGreater:
        return ConditionA64::LessEqual;

    case IrCondition::GreaterEqual:
        return ConditionA64::GreaterEqual;

    case IrCondition::NotGreaterEqual:
        return ConditionA64::Less;

    default:
        CODEGEN_ASSERT(!"Unexpected condition code");
        return ConditionA64::Always;
    }
}

inline ConditionA64 getConditionInt(IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return ConditionA64::Equal;

    case IrCondition::NotEqual:
        return ConditionA64::NotEqual;

    // Minus/Plus (MI/PL) check the N flag only, which is correct for small
    // integer comparisons where overflow cannot occur.
    case IrCondition::Less:
        return ConditionA64::Minus;

    case IrCondition::NotLess:
        return ConditionA64::Plus;

    case IrCondition::LessEqual:
        return ConditionA64::LessEqual;

    case IrCondition::NotLessEqual:
        return ConditionA64::Greater;

    case IrCondition::Greater:
        return ConditionA64::Greater;

    case IrCondition::NotGreater:
        return ConditionA64::LessEqual;

    case IrCondition::GreaterEqual:
        return ConditionA64::GreaterEqual;

    case IrCondition::NotGreaterEqual:
        return ConditionA64::Less;

    case IrCondition::UnsignedLess:
        return ConditionA64::CarryClear;

    case IrCondition::UnsignedLessEqual:
        return ConditionA64::UnsignedLessEqual;

    case IrCondition::UnsignedGreater:
        return ConditionA64::UnsignedGreater;

    case IrCondition::UnsignedGreaterEqual:
        return ConditionA64::CarrySet;

    default:
        CODEGEN_ASSERT(!"Unexpected condition code");
        return ConditionA64::Always;
    }
}

// Full-range signed integer condition mapping for int64 comparisons.
// Unlike getConditionInt which uses MI/PL (N flag only) for Less/NotLess,
// this uses LT/GE (N!=V / N==V) which correctly handles overflow.
// Example: CMP INT64_MAX, -1 sets N=1,V=1 so MI fires but LT does not.
// Helpful cheatsheet: https://gist.github.com/ryo/31017f265cc2f9ade124aea64543df22
inline ConditionA64 getConditionInt64(IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return ConditionA64::Equal;

    case IrCondition::NotEqual:
        return ConditionA64::NotEqual;

    case IrCondition::Less:
        return ConditionA64::Less;

    case IrCondition::NotLess:
        return ConditionA64::GreaterEqual;

    case IrCondition::LessEqual:
        return ConditionA64::LessEqual;

    case IrCondition::NotLessEqual:
        return ConditionA64::Greater;

    case IrCondition::Greater:
        return ConditionA64::Greater;

    case IrCondition::NotGreater:
        return ConditionA64::LessEqual;

    case IrCondition::GreaterEqual:
        return ConditionA64::GreaterEqual;

    case IrCondition::NotGreaterEqual:
        return ConditionA64::Less;

    case IrCondition::UnsignedLess:
        return ConditionA64::CarryClear;

    case IrCondition::UnsignedLessEqual:
        return ConditionA64::UnsignedLessEqual;

    case IrCondition::UnsignedGreater:
        return ConditionA64::UnsignedGreater;

    case IrCondition::UnsignedGreaterEqual:
        return ConditionA64::CarrySet;

    default:
        CODEGEN_ASSERT(!"Unexpected condition code");
        return ConditionA64::Always;
    }
}

static void emitAddOffset(AssemblyBuilderA64& build, RegisterA64 dst, RegisterA64 src, size_t offset)
{
    CODEGEN_ASSERT(dst != src);
    CODEGEN_ASSERT(offset <= INT_MAX);

    if (offset <= AssemblyBuilderA64::kMaxImmediate)
    {
        build.add(dst, src, uint16_t(offset));
    }
    else
    {
        build.mov(dst, int(offset));
        build.add(dst, dst, src);
    }
}

static void emitAbort(AssemblyBuilderA64& build, Label& abort)
{
    Label skip;
    build.b(skip);
    build.setLabel(abort);
    build.udf();
    build.setLabel(skip);
}

static void emitFallback(AssemblyBuilderA64& build, int offset, int pcpos)
{
    // fallback(L, instruction, base, k)
    build.mov(x0, rState);
    emitAddOffset(build, x1, rCode, pcpos * sizeof(Instruction));
    build.mov(x2, rBase);
    build.mov(x3, rConstants);
    build.ldr(x4, mem(rNativeContext, offset));
    build.blr(x4);

    emitUpdateBase(build);
}

static void emitInvokeLibm1P(AssemblyBuilderA64& build, size_t func, int arg)
{
    CODEGEN_ASSERT(kTempSlots >= 1);
    CODEGEN_ASSERT(unsigned(sTemporary.data) <= AssemblyBuilderA64::kMaxImmediate);
    build.ldr(d0, mem(rBase, arg * sizeof(TValue) + offsetof(TValue, value.n)));
    build.add(x0, sp, uint16_t(sTemporary.data)); // sp-relative offset
    build.ldr(x1, mem(rNativeContext, uint32_t(func)));
    build.blr(x1);
}

static bool emitBuiltin(AssemblyBuilderA64& build, IrFunction& function, IrRegAllocA64& regs, int bfid, int res, int arg, int nresults)
{
    switch (bfid)
    {
    case LBF_MATH_FREXP:
    {
        CODEGEN_ASSERT(nresults == 1 || nresults == 2);
        emitInvokeLibm1P(build, offsetof(NativeContext, libm_frexp), arg);
        build.str(d0, mem(rBase, res * sizeof(TValue) + offsetof(TValue, value.n)));

        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.mov(temp, LUA_TNUMBER);
        build.str(temp, mem(rBase, res * sizeof(TValue) + offsetof(TValue, tt)));

        if (nresults == 2)
        {
            build.ldr(w0, sTemporary);
            build.scvtf(d1, w0);
            build.str(d1, mem(rBase, (res + 1) * sizeof(TValue) + offsetof(TValue, value.n)));
            build.str(temp, mem(rBase, (res + 1) * sizeof(TValue) + offsetof(TValue, tt)));
        }
        return true;
    }
    case LBF_MATH_MODF:
    {
        CODEGEN_ASSERT(nresults == 1 || nresults == 2);
        emitInvokeLibm1P(build, offsetof(NativeContext, libm_modf), arg);
        build.ldr(d1, sTemporary);
        build.str(d1, mem(rBase, res * sizeof(TValue) + offsetof(TValue, value.n)));

        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.mov(temp, LUA_TNUMBER);
        build.str(temp, mem(rBase, res * sizeof(TValue) + offsetof(TValue, tt)));

        if (nresults == 2)
        {
            build.str(d0, mem(rBase, (res + 1) * sizeof(TValue) + offsetof(TValue, value.n)));
            build.str(temp, mem(rBase, (res + 1) * sizeof(TValue) + offsetof(TValue, tt)));
        }
        return true;
    }

    default:
        CODEGEN_ASSERT(!"Missing A64 lowering");
        return false;
    }
}

static uint64_t getDoubleBits(double value)
{
    uint64_t result;
    static_assert(sizeof(result) == sizeof(value), "Expecting double to be 64-bit");
    memcpy(&result, &value, sizeof(value));
    return result;
}

static uint32_t getFloatBits(float value)
{
    uint32_t result;
    static_assert(sizeof(result) == sizeof(value), "Expecting float to be 32-bit");
    memcpy(&result, &value, sizeof(value));
    return result;
}

IrLoweringA64::IrLoweringA64(AssemblyBuilderA64& build, ModuleHelpers& helpers, IrFunction& function, LoweringStats* stats)
    : build(build)
    , helpers(helpers)
    , function(function)
    , stats(stats)
    , regs(build, function, stats, {{x0, x15}, {x16, x17}, {q0, q7}, {q16, q31}})
    , valueTracker(function)
    , exitHandlerMap(~0u)
{
    valueTracker.setRestoreCallback(
        this,
        [](void* context, IrInst& inst)
        {
            IrLoweringA64* self = static_cast<IrLoweringA64*>(context);
            self->regs.restoreReg(inst);
        }
    );
}

void IrLoweringA64::lowerInst(IrInst& inst, uint32_t index, const IrBlock& next)
{
    regs.currInstIdx = index;

    valueTracker.beforeInstLowering(inst);

    switch (inst.cmd)
    {
    case IrCmd::LOAD_TAG:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, tt));
        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_POINTER:
    {
        inst.regA64 = regs.allocReg(KindA64::x, index);
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value.gc));
        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_DOUBLE:
    {
        inst.regA64 = regs.allocReg(KindA64::d, index);
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value.n));
        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_INT:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value));
        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_INT64:
    {
        inst.regA64 = regs.allocReg(KindA64::x, index);
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value.l));
        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_FLOAT:
    {
        inst.regA64 = regs.allocReg(KindA64::s, index);
        AddressA64 addr = tempAddr(OP_A(inst), intOp(OP_B(inst)));

        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_TVALUE:
    {
        inst.regA64 = regs.allocReg(KindA64::q, index);

        int addrOffset = HAS_OP_B(inst) ? intOp(OP_B(inst)) : 0;
        AddressA64 addr = tempAddr(OP_A(inst), addrOffset);
        build.ldr(inst.regA64, addr);
        break;
    }
    case IrCmd::LOAD_ENV:
        inst.regA64 = regs.allocReg(KindA64::x, index);
        build.ldr(inst.regA64, mem(rClosure, offsetof(Closure, env)));
        break;
    case IrCmd::GET_ARR_ADDR:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        build.ldr(inst.regA64, mem(regOp(OP_A(inst)), offsetof(LuaTable, array)));

        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.add(inst.regA64, inst.regA64, regOp(OP_B(inst)), kTValueSizeLog2); // implicit uxtw
        }
        else if (OP_B(inst).kind == IrOpKind::Constant)
        {
            if (intOp(OP_B(inst)) == 0)
            {
                // no offset required
            }
            else if (intOp(OP_B(inst)) * sizeof(TValue) <= AssemblyBuilderA64::kMaxImmediate)
            {
                build.add(inst.regA64, inst.regA64, uint16_t(intOp(OP_B(inst)) * sizeof(TValue)));
            }
            else
            {
                RegisterA64 temp = regs.allocTemp(KindA64::x);
                build.mov(temp, intOp(OP_B(inst)) * sizeof(TValue));
                build.add(inst.regA64, inst.regA64, temp);
            }
        }
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    }
    case IrCmd::GET_SLOT_NODE_ADDR:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp1w = castReg(KindA64::w, temp1);
        RegisterA64 temp2 = regs.allocTemp(KindA64::w);
        RegisterA64 temp2x = castReg(KindA64::x, temp2);

        // note: since the stride of the load is the same as the destination register size, we can range check the array index, not the byte offset
        if (uintOp(OP_B(inst)) <= AddressA64::kMaxOffset)
            build.ldr(temp1w, mem(rCode, uintOp(OP_B(inst)) * sizeof(Instruction)));
        else
        {
            build.mov(temp1, uintOp(OP_B(inst)) * sizeof(Instruction));
            build.ldr(temp1w, mem(rCode, temp1));
        }

        // C field can be shifted as long as it's at the most significant byte of the instruction word
        CODEGEN_ASSERT(kOffsetOfInstructionC == 3);
        build.ldrb(temp2, mem(regOp(OP_A(inst)), offsetof(LuaTable, nodemask8)));
        build.and_(temp2, temp2, temp1w, -24);

        // note: this may clobber OP_A(inst), so it's important that we don't use it after this
        build.ldr(inst.regA64, mem(regOp(OP_A(inst)), offsetof(LuaTable, node)));
        build.add(inst.regA64, inst.regA64, temp2x, kLuaNodeSizeLog2); // "zero extend" temp2 to get a larger shift (top 32 bits are zero)
        break;
    }
    case IrCmd::GET_HASH_NODE_ADDR:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 temp1 = regs.allocTemp(KindA64::w);
        RegisterA64 temp2 = regs.allocTemp(KindA64::w);
        RegisterA64 temp2x = castReg(KindA64::x, temp2);

        // hash & ((1 << lsizenode) - 1) == hash & ~(-1 << lsizenode)
        build.mov(temp1, -1);
        build.ldrb(temp2, mem(regOp(OP_A(inst)), offsetof(LuaTable, lsizenode)));
        build.lsl(temp1, temp1, temp2);
        build.mov(temp2, uintOp(OP_B(inst)));
        build.bic(temp2, temp2, temp1);

        // note: this may clobber OP_A(inst), so it's important that we don't use it after this
        build.ldr(inst.regA64, mem(regOp(OP_A(inst)), offsetof(LuaTable, node)));
        build.add(inst.regA64, inst.regA64, temp2x, kLuaNodeSizeLog2); // "zero extend" temp2 to get a larger shift (top 32 bits are zero)
        break;
    }
    case IrCmd::GET_CLOSURE_UPVAL_ADDR:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 cl = OP_A(inst).kind == IrOpKind::Undef ? rClosure : regOp(OP_A(inst));

        build.add(inst.regA64, cl, uint16_t(offsetof(Closure, l.uprefs) + sizeof(TValue) * vmUpvalueOp(OP_B(inst))));
        break;
    }
    case IrCmd::STORE_TAG:
    {
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, tt));
        if (tagOp(OP_B(inst)) == 0)
        {
            build.str(wzr, addr);
        }
        else
        {
            RegisterA64 temp = regs.allocTemp(KindA64::w);
            build.mov(temp, tagOp(OP_B(inst)));
            build.str(temp, addr);
        }
        break;
    }
    case IrCmd::STORE_POINTER:
    {
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value));
        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            CODEGEN_ASSERT(intOp(OP_B(inst)) == 0);
            build.str(xzr, addr);
        }
        else
        {
            build.str(regOp(OP_B(inst)), addr);
        }
        break;
    }
    case IrCmd::STORE_EXTRA:
    {
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, extra));
        if (intOp(OP_B(inst)) == 0)
        {
            build.str(wzr, addr);
        }
        else
        {
            RegisterA64 temp = regs.allocTemp(KindA64::w);
            build.mov(temp, intOp(OP_B(inst)));
            build.str(temp, addr);
        }
        break;
    }
    case IrCmd::STORE_DOUBLE:
    {
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value));
        if (OP_B(inst).kind == IrOpKind::Constant && getDoubleBits(doubleOp(OP_B(inst))) == 0)
        {
            build.str(xzr, addr);
        }
        else
        {
            RegisterA64 temp = tempDouble(OP_B(inst));
            build.str(temp, addr);
        }
        break;
    }
    case IrCmd::STORE_INT:
    {
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value));
        if (OP_B(inst).kind == IrOpKind::Constant && intOp(OP_B(inst)) == 0)
        {
            build.str(wzr, addr);
        }
        else
        {
            RegisterA64 temp = tempInt(OP_B(inst));
            build.str(temp, addr);
        }
        break;
    }
    case IrCmd::STORE_INT64:
    {
        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value));
        if (OP_B(inst).kind == IrOpKind::Constant && int64Op(OP_B(inst)) == 0)
        {
            build.str(xzr, addr);
        }
        else
        {
            RegisterA64 temp = tempInt64(OP_B(inst));
            build.str(temp, addr);
        }
        break;
    }
    case IrCmd::STORE_VECTOR:
    {
        RegisterA64 temp1 = tempFloat(OP_B(inst));
        RegisterA64 temp2 = tempFloat(OP_C(inst));
        RegisterA64 temp3 = tempFloat(OP_D(inst));

        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value));
        CODEGEN_ASSERT(addr.kind == AddressKindA64::imm && addr.data % 4 == 0 && unsigned(addr.data + 8) / 4 <= AddressA64::kMaxOffset);

        build.str(temp1, AddressA64(addr.base, addr.data + 0));
        build.str(temp2, AddressA64(addr.base, addr.data + 4));
        build.str(temp3, AddressA64(addr.base, addr.data + 8));

        if (HAS_OP_E(inst))
        {
            RegisterA64 temp = regs.allocTemp(KindA64::w);
            build.mov(temp, tagOp(OP_E(inst)));
            build.str(temp, tempAddr(OP_A(inst), offsetof(TValue, tt)));
        }
        break;
    }
    case IrCmd::STORE_TVALUE:
    {
        int addrOffset = HAS_OP_C(inst) ? intOp(OP_C(inst)) : 0;
        AddressA64 addr = tempAddr(OP_A(inst), addrOffset);
        build.str(regOp(OP_B(inst)), addr);
        break;
    }
    case IrCmd::STORE_SPLIT_TVALUE:
    {
        int addrOffset = HAS_OP_D(inst) ? intOp(OP_D(inst)) : 0;

        RegisterA64 tempt = regs.allocTemp(KindA64::w);
        AddressA64 addrt = tempAddr(OP_A(inst), offsetof(TValue, tt) + addrOffset);
        build.mov(tempt, tagOp(OP_B(inst)));
        build.str(tempt, addrt);

        AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, value) + addrOffset);

        if (tagOp(OP_B(inst)) == LUA_TBOOLEAN)
        {
            if (OP_C(inst).kind == IrOpKind::Constant)
            {
                // note: we reuse tag temp register as value for true booleans, and use built-in zero register for false values
                CODEGEN_ASSERT(LUA_TBOOLEAN == 1);
                build.str(intOp(OP_C(inst)) ? tempt : wzr, addr);
            }
            else
                build.str(regOp(OP_C(inst)), addr);
        }
        else if (tagOp(OP_B(inst)) == LUA_TNUMBER)
        {
            RegisterA64 temp = tempDouble(OP_C(inst));
            build.str(temp, addr);
        }
        else if (tagOp(OP_B(inst)) == LUA_TINTEGER)
        {
            RegisterA64 temp = tempInt64(OP_C(inst));
            build.str(temp, addr);
        }
        else if (isGCO(tagOp(OP_B(inst))))
        {
            build.str(regOp(OP_C(inst)), addr);
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::ADD_INT:
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_B(inst).kind == IrOpKind::Constant && unsigned(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            build.add(inst.regA64, regOp(OP_A(inst)), uint16_t(intOp(OP_B(inst))));
        else if (OP_A(inst).kind == IrOpKind::Constant && unsigned(intOp(OP_A(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            build.add(inst.regA64, regOp(OP_B(inst)), uint16_t(intOp(OP_A(inst))));
        else
        {
            RegisterA64 temp1 = tempInt(OP_A(inst));
            RegisterA64 temp2 = tempInt(OP_B(inst));
            build.add(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::SUB_INT:
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_B(inst).kind == IrOpKind::Constant && unsigned(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            build.sub(inst.regA64, regOp(OP_A(inst)), uint16_t(intOp(OP_B(inst))));
        else
        {
            RegisterA64 temp1 = tempInt(OP_A(inst));
            RegisterA64 temp2 = tempInt(OP_B(inst));
            build.sub(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::ADD_INT64:
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        if (OP_B(inst).kind == IrOpKind::Constant && uint64_t(int64Op(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            build.add(inst.regA64, tempInt64(OP_A(inst)), uint16_t(int64Op(OP_B(inst))));
        else if (OP_A(inst).kind == IrOpKind::Constant && uint64_t(int64Op(OP_A(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            build.add(inst.regA64, tempInt64(OP_B(inst)), uint16_t(int64Op(OP_A(inst))));
        else
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.add(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::SUB_INT64:
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        if (OP_B(inst).kind == IrOpKind::Constant && uint64_t(int64Op(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            build.sub(inst.regA64, tempInt64(OP_A(inst)), uint16_t(int64Op(OP_B(inst))));
        else
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.sub(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::MUL_INT64:
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.mul(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::DIV_INT64:
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.sdiv(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::IDIV_INT64:
        // floored division: q = a / b, then if (q < 0 && a % b != 0) q -= 1
        inst.regA64 = regs.allocReg(KindA64::x, index); // can't reuse: both operands needed for remainder
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            RegisterA64 tempRem = regs.allocTemp(KindA64::x);
            RegisterA64 tempAdj = regs.allocTemp(KindA64::x);

            build.sdiv(inst.regA64, temp1, temp2); // result = a / b
            build.mov(tempRem, inst.regA64);       // copy quotient; rem requires dst to initially hold quotient
            build.rem(tempRem, temp1, temp2);

            build.sub(tempAdj, inst.regA64, uint16_t(1)); // adjusted = result - 1

            build.cmp(tempRem, uint16_t(0));
            build.csel(tempAdj, tempAdj, inst.regA64, ConditionA64::NotEqual); // (remainder != 0) ? result-1 : result

            build.cmp(inst.regA64, uint16_t(0));
            build.csel(inst.regA64, tempAdj, inst.regA64, ConditionA64::Less); // (result < 0) ? tempAdj : result
        }
        break;
    case IrCmd::CHECK_DIV_INT64:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_C(inst), fresh);

        // guard against divide by zero
        RegisterA64 regB = tempInt64(OP_B(inst));
        build.cbz(regB, fail);

        // guard against if a is -2^63 and b is -1
        RegisterA64 regA = tempInt64(OP_A(inst));
        RegisterA64 tempRotate = regs.allocTemp(KindA64::x);

        // bit trick, if we are integer.minsigned (0x8000000000000000), then if we rotate by 63, we will get 1
        build.ror(tempRotate, regA, 63);

        build.cmp(tempRotate, uint16_t(1));

        // nzcv = 0000 EQ
        // nzcv = 0001 NE
        build.ccmn(regB, 1, getConditionInt64(IrCondition::Equal), 1);
        build.b(getConditionInt64(IrCondition::Equal), fail);

        finalizeTargetLabel(OP_C(inst), fresh);
        break;
    }
    case IrCmd::UDIV_INT64:
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.udiv(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::REM_INT64:
        inst.regA64 = regs.allocReg(KindA64::x, index);
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.sdiv(inst.regA64, temp1, temp2);
            build.rem(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::MOD_INT64:
        // floored modulo: rem = a % b (C truncated); if (rem != 0 && sign(rem) != sign(b)) rem += b
        inst.regA64 = regs.allocReg(KindA64::x, index); // can't reuse: dividend (temp1) needed after sdiv
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            RegisterA64 tempRem = regs.allocTemp(KindA64::x);
            RegisterA64 tempAdj = regs.allocTemp(KindA64::x);

            build.sdiv(inst.regA64, temp1, temp2); // quotient = a / b
            build.mov(tempRem, inst.regA64);       // tempRem = quotient
            build.rem(tempRem, temp1, temp2);      // tempRem = C-style remainder

            build.add(tempAdj, tempRem, temp2);     // tempAdj = rem + b (floored candidate)
            build.eor(inst.regA64, tempRem, temp2); // sign check: negative if signs differ

            build.cmp(inst.regA64, uint16_t(0));
            build.csel(tempAdj, tempAdj, tempRem, ConditionA64::Less); // if signs differ then rem+b else rem

            build.cmp(tempRem, uint16_t(0));
            build.csel(inst.regA64, tempAdj, tempRem, ConditionA64::NotEqual); // if rem != 0 then adjusted else 0
        }
        break;
    case IrCmd::UREM_INT64:
        inst.regA64 = regs.allocReg(KindA64::x, index);
        {
            RegisterA64 temp1 = tempInt64(OP_A(inst));
            RegisterA64 temp2 = tempInt64(OP_B(inst));
            build.udiv(inst.regA64, temp1, temp2);
            build.rem(inst.regA64, temp1, temp2);
        }
        break;
    case IrCmd::SEXTI8_INT:
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});

        build.sbfx(inst.regA64, regOp(OP_A(inst)), 0, 8); // sextb
        break;
    case IrCmd::SEXTI16_INT:
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});

        build.sbfx(inst.regA64, regOp(OP_A(inst)), 0, 16); // sexth
        break;
    case IrCmd::ADD_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fadd(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::SUB_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fsub(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::MUL_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fmul(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::DIV_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fdiv(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::IDIV_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fdiv(inst.regA64, temp1, temp2);
        build.frintm(inst.regA64, inst.regA64);
        break;
    }
    case IrCmd::MOD_NUM:
    {
        inst.regA64 = regs.allocReg(KindA64::d, index); // can't allocReuse because both A and B are used twice
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fdiv(inst.regA64, temp1, temp2);
        build.frintm(inst.regA64, inst.regA64);
        build.fmul(inst.regA64, inst.regA64, temp2);
        build.fsub(inst.regA64, temp1, inst.regA64);
        break;
    }
    case IrCmd::MULADD_NUM:
    {
        RegisterA64 tempA = tempDouble(OP_A(inst));
        RegisterA64 tempB = tempDouble(OP_B(inst));
        RegisterA64 tempC = tempDouble(OP_C(inst));

        if ((build.features & Feature_AdvSIMD) != 0)
        {
            inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_C(inst)});
            if (inst.regA64 != tempC)
                build.fmov(inst.regA64, tempC);
            build.fmla(inst.regA64, tempB, tempA);
        }
        else
        {
            inst.regA64 = regs.allocReg(KindA64::d, index);
            build.fmul(inst.regA64, tempB, tempA);
            build.fadd(inst.regA64, inst.regA64, tempC);
        }
        break;
    }
    case IrCmd::MIN_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fcmp(temp1, temp2);
        build.fcsel(inst.regA64, temp1, temp2, getConditionFP(IrCondition::Less));
        break;
    }
    case IrCmd::MAX_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        build.fcmp(temp1, temp2);
        build.fcsel(inst.regA64, temp1, temp2, getConditionFP(IrCondition::Greater));
        break;
    }
    case IrCmd::UNM_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.fneg(inst.regA64, temp);
        break;
    }
    case IrCmd::FLOOR_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.frintm(inst.regA64, temp);
        break;
    }
    case IrCmd::CEIL_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.frintp(inst.regA64, temp);
        break;
    }
    case IrCmd::ROUND_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.frinta(inst.regA64, temp);
        break;
    }
    case IrCmd::SQRT_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.fsqrt(inst.regA64, temp);
        break;
    }
    case IrCmd::ABS_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.fabs(inst.regA64, temp);
        break;
    }
    case IrCmd::SIGN_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst)});

        RegisterA64 temp = tempDouble(OP_A(inst));
        RegisterA64 temp0 = regs.allocTemp(KindA64::d);
        RegisterA64 temp1 = regs.allocTemp(KindA64::d);

        build.fcmpz(temp);
        build.fmov(temp0, 0.0);
        build.fmov(temp1, 1.0);
        build.fcsel(inst.regA64, temp1, temp0, getConditionFP(IrCondition::Greater));
        build.fmov(temp1, -1.0);
        build.fcsel(inst.regA64, temp1, inst.regA64, getConditionFP(IrCondition::Less));
        break;
    }
    case IrCmd::ADD_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempFloat(OP_A(inst));
        RegisterA64 temp2 = tempFloat(OP_B(inst));
        build.fadd(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::SUB_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempFloat(OP_A(inst));
        RegisterA64 temp2 = tempFloat(OP_B(inst));
        build.fsub(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::MUL_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempFloat(OP_A(inst));
        RegisterA64 temp2 = tempFloat(OP_B(inst));
        build.fmul(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::DIV_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempFloat(OP_A(inst));
        RegisterA64 temp2 = tempFloat(OP_B(inst));
        build.fdiv(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::MIN_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempFloat(OP_A(inst));
        RegisterA64 temp2 = tempFloat(OP_B(inst));
        build.fcmp(temp1, temp2);
        build.fcsel(inst.regA64, temp1, temp2, getConditionFP(IrCondition::Less));
        break;
    }
    case IrCmd::MAX_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempFloat(OP_A(inst));
        RegisterA64 temp2 = tempFloat(OP_B(inst));
        build.fcmp(temp1, temp2);
        build.fcsel(inst.regA64, temp1, temp2, getConditionFP(IrCondition::Greater));
        break;
    }
    case IrCmd::UNM_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst)});
        RegisterA64 temp = tempFloat(OP_A(inst));
        build.fneg(inst.regA64, temp);
        break;
    }
    case IrCmd::FLOOR_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst)});
        RegisterA64 temp = tempFloat(OP_A(inst));
        build.frintm(inst.regA64, temp);
        break;
    }
    case IrCmd::CEIL_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst)});
        RegisterA64 temp = tempFloat(OP_A(inst));
        build.frintp(inst.regA64, temp);
        break;
    }
    case IrCmd::SQRT_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst)});
        RegisterA64 temp = tempFloat(OP_A(inst));
        build.fsqrt(inst.regA64, temp);
        break;
    }
    case IrCmd::ABS_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst)});
        RegisterA64 temp = tempFloat(OP_A(inst));
        build.fabs(inst.regA64, temp);
        break;
    }
    case IrCmd::SIGN_FLOAT:
    {
        inst.regA64 = regs.allocReuse(KindA64::s, index, {OP_A(inst)});

        RegisterA64 temp = tempFloat(OP_A(inst));
        RegisterA64 temp0 = regs.allocTemp(KindA64::s);
        RegisterA64 temp1 = regs.allocTemp(KindA64::s);

        build.fcmpz(temp);
        build.fmov(temp0, 0.0f);
        build.fmov(temp1, 1.0f);
        build.fcsel(inst.regA64, temp1, temp0, getConditionFP(IrCondition::Greater));
        build.fmov(temp1, -1.0f);
        build.fcsel(inst.regA64, temp1, inst.regA64, getConditionFP(IrCondition::Less));
        break;
    }
    case IrCmd::SELECT_NUM:
    {
        inst.regA64 = regs.allocReuse(KindA64::d, index, {OP_A(inst), OP_B(inst), OP_C(inst), OP_D(inst)});

        RegisterA64 temp1 = tempDouble(OP_A(inst));
        RegisterA64 temp2 = tempDouble(OP_B(inst));
        RegisterA64 temp3 = tempDouble(OP_C(inst));
        RegisterA64 temp4 = tempDouble(OP_D(inst));

        build.fcmp(temp3, temp4);
        build.fcsel(inst.regA64, temp2, temp1, getConditionFP(IrCondition::Equal));
        break;
    }
    case IrCmd::SELECT_INT64:
    {
        IrCondition cond = conditionOp(OP_E(inst));

        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst), OP_C(inst), OP_D(inst)});

        RegisterA64 temp1 = tempInt64(OP_A(inst));
        RegisterA64 temp2 = tempInt64(OP_B(inst));
        RegisterA64 temp3 = tempInt64(OP_C(inst));
        RegisterA64 temp4 = tempInt64(OP_D(inst));

        build.cmp(temp3, temp4);
        build.csel(inst.regA64, temp2, temp1, getConditionInt64(cond));
        break;
    }
    case IrCmd::SELECT_VEC:
    {
        // `OP_B(inst)` cannot be reused for return value, because it can be overwritten with A before the first usage
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_C(inst), OP_D(inst)});

        RegisterA64 temp1 = regOp(OP_A(inst));
        RegisterA64 temp2 = regOp(OP_B(inst));
        RegisterA64 temp3 = regOp(OP_C(inst));
        RegisterA64 temp4 = regOp(OP_D(inst));

        RegisterA64 mask = regs.allocTemp(KindA64::q);

        // Evaluate predicate and calculate mask.
        build.fcmeq_4s(mask, temp3, temp4);
        // mov A to res register
        build.mov(inst.regA64, temp1);
        // If numbers are equal override A with B in res register.
        build.bit(inst.regA64, temp2, mask);
        break;
    }
    case IrCmd::SELECT_IF_TRUTHY:
    {
        inst.regA64 = regs.allocReg(KindA64::q, index);

        // Place lhs as the result, we will overwrite it with rhs if 'A' is falsy later
        build.mov(inst.regA64, regOp(OP_B(inst)));

        // Get rhs register early, so a potential restore happens on both sides of a conditional control flow
        RegisterA64 c = regOp(OP_C(inst));

        RegisterA64 temp = regs.allocTemp(KindA64::w);
        Label saveRhs, exit;

        // Check tag first
        build.umov_4s(temp, regOp(OP_A(inst)), 3);
        build.cmp(temp, uint16_t(LUA_TBOOLEAN));

        build.b(ConditionA64::UnsignedLess, saveRhs); // rhs if 'A' is nil
        build.b(ConditionA64::UnsignedGreater, exit); // Keep lhs if 'A' is not a boolean

        // Check the boolean value
        build.umov_4s(temp, regOp(OP_A(inst)), 0);
        build.cbnz(temp, exit); // Keep lhs if 'A' is true

        build.setLabel(saveRhs);
        build.mov(inst.regA64, c);

        build.setLabel(exit);
        break;
    }
    case IrCmd::MULADD_VEC:
    {
        RegisterA64 tempA = regOp(OP_A(inst));
        RegisterA64 tempB = regOp(OP_B(inst));
        RegisterA64 tempC = regOp(OP_C(inst));

        if ((build.features & Feature_AdvSIMD) != 0)
        {
            inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_C(inst)});
            if (inst.regA64 != tempC)
                build.mov(inst.regA64, tempC);
            build.fmla(inst.regA64, tempB, tempA);
        }
        else
        {
            inst.regA64 = regs.allocReg(KindA64::q, index);
            build.fmul(inst.regA64, tempB, tempA);
            build.fadd(inst.regA64, inst.regA64, tempC);
        }
        break;
    }
    case IrCmd::ADD_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        build.fadd(inst.regA64, regOp(OP_A(inst)), regOp(OP_B(inst)));
        break;
    }
    case IrCmd::SUB_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        build.fsub(inst.regA64, regOp(OP_A(inst)), regOp(OP_B(inst)));
        break;
    }
    case IrCmd::MUL_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        build.fmul(inst.regA64, regOp(OP_A(inst)), regOp(OP_B(inst)));
        break;
    }
    case IrCmd::DIV_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        build.fdiv(inst.regA64, regOp(OP_A(inst)), regOp(OP_B(inst)));
        break;
    }
    case IrCmd::IDIV_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        build.fdiv(inst.regA64, regOp(OP_A(inst)), regOp(OP_B(inst)));
        build.frintm(inst.regA64, inst.regA64);
        break;
    }
    case IrCmd::UNM_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst)});

        build.fneg(inst.regA64, regOp(OP_A(inst)));
        break;
    }
    case IrCmd::MIN_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        RegisterA64 temp1 = regOp(OP_A(inst));
        RegisterA64 temp2 = regOp(OP_B(inst));

        RegisterA64 mask = regs.allocTemp(KindA64::q);

        // b > a == a < b
        build.fcmgt_4s(mask, temp2, temp1);

        // If A is already at the target, select B where mask is 0
        if (inst.regA64 == temp1)
        {
            build.bif(inst.regA64, temp2, mask);
        }
        else
        {
            // Store B at the target unless it's there, select A where mask is 1
            if (inst.regA64 != temp2)
                build.mov(inst.regA64, temp2);

            build.bit(inst.regA64, temp1, mask);
        }
        break;
    }
    case IrCmd::MAX_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst), OP_B(inst)});

        RegisterA64 temp1 = regOp(OP_A(inst));
        RegisterA64 temp2 = regOp(OP_B(inst));

        RegisterA64 mask = regs.allocTemp(KindA64::q);

        build.fcmgt_4s(mask, temp1, temp2);

        // If A is already at the target, select B where mask is 0
        if (inst.regA64 == temp1)
        {
            build.bif(inst.regA64, temp2, mask);
        }
        else
        {
            // Store B at the target unless it's there, select A where mask is 1
            if (inst.regA64 != temp2)
                build.mov(inst.regA64, temp2);

            build.bit(inst.regA64, temp1, mask);
        }
        break;
    }
    case IrCmd::FLOOR_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst)});

        build.frintm(inst.regA64, regOp(OP_A(inst)));
        break;
    }
    case IrCmd::CEIL_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst)});

        build.frintp(inst.regA64, regOp(OP_A(inst)));
        break;
    }
    case IrCmd::ABS_VEC:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst)});
        build.fabs(inst.regA64, regOp(OP_A(inst)));
        break;
    }
    case IrCmd::DOT_VEC:
    {
        inst.regA64 = regs.allocReg(KindA64::s, index);

        RegisterA64 temp = regs.allocTemp(KindA64::q);
        RegisterA64 temps = castReg(KindA64::s, temp);

        build.fmul(temp, regOp(OP_A(inst)), regOp(OP_B(inst)));
        build.faddp(inst.regA64, temps); // x+y
        build.dup_4s(temp, temp, 2);
        build.fadd(inst.regA64, inst.regA64, temps); // +z
        break;
    }
    case IrCmd::EXTRACT_VEC:
    {
        inst.regA64 = regs.allocReg(KindA64::s, index);

        if (intOp(OP_B(inst)) == 0)
        {
            // Lane vN.s[0] can just be read directly as sN
            build.fmov(inst.regA64, castReg(KindA64::s, regOp(OP_A(inst))));
        }
        else
        {
            build.dup_4s(inst.regA64, regOp(OP_A(inst)), intOp(OP_B(inst)));
        }
        break;
    }
    case IrCmd::NOT_ANY:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            // other cases should've been constant folded
            CODEGEN_ASSERT(tagOp(OP_A(inst)) == LUA_TBOOLEAN);
            build.eor(inst.regA64, regOp(OP_B(inst)), 1);
        }
        else
        {
            Label notBool, exit;

            // use the fact that NIL is the only value less than BOOLEAN to do two tag comparisons at once
            CODEGEN_ASSERT(LUA_TNIL == 0 && LUA_TBOOLEAN == 1);
            build.cmp(regOp(OP_A(inst)), uint16_t(LUA_TBOOLEAN));
            build.b(ConditionA64::NotEqual, notBool);

            if (OP_B(inst).kind == IrOpKind::Constant)
                build.mov(inst.regA64, intOp(OP_B(inst)) == 0 ? 1 : 0);
            else
                build.eor(inst.regA64, regOp(OP_B(inst)), 1); // boolean => invert value

            build.b(exit);

            // not boolean => result is true iff tag was nil
            build.setLabel(notBool);
            build.cset(inst.regA64, ConditionA64::Less);

            build.setLabel(exit);
        }
        break;
    }
    case IrCmd::CMP_INT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});

        IrCondition cond = conditionOp(OP_C(inst));

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            if (unsigned(intOp(OP_A(inst))) <= AssemblyBuilderA64::kMaxImmediate)
                build.cmp(regOp(OP_B(inst)), uint16_t(intOp(OP_A(inst))));
            else
                build.cmp(regOp(OP_B(inst)), tempInt(OP_A(inst)));

            build.cset(inst.regA64, getInverseCondition(getConditionInt(cond)));
        }
        else if (OP_A(inst).kind == IrOpKind::Inst)
        {
            if (unsigned(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
                build.cmp(regOp(OP_A(inst)), uint16_t(intOp(OP_B(inst))));
            else
                build.cmp(regOp(OP_A(inst)), tempInt(OP_B(inst)));

            build.cset(inst.regA64, getConditionInt(cond));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::CMP_INT64:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);

        IrCondition cond = conditionOp(OP_C(inst));

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            if (uint64_t(int64Op(OP_A(inst))) <= AssemblyBuilderA64::kMaxImmediate)
                build.cmp(regOp(OP_B(inst)), uint16_t(int64Op(OP_A(inst))));
            else
                build.cmp(regOp(OP_B(inst)), tempInt64(OP_A(inst)));

            build.cset(inst.regA64, getInverseCondition(getConditionInt64(cond)));
        }
        else if (OP_A(inst).kind == IrOpKind::Inst)
        {
            if (OP_B(inst).kind == IrOpKind::Constant && uint64_t(int64Op(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
                build.cmp(regOp(OP_A(inst)), uint16_t(int64Op(OP_B(inst))));
            else
                build.cmp(regOp(OP_A(inst)), tempInt64(OP_B(inst)));

            build.cset(inst.regA64, getConditionInt64(cond));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::CMP_ANY:
    {
        CODEGEN_ASSERT(OP_A(inst).kind == IrOpKind::VmReg && OP_B(inst).kind == IrOpKind::VmReg);
        IrCondition cond = conditionOp(OP_C(inst));

        if (FFlag::LuauCodegenCallWrapImproved)
            inst.regA64 = regs.allocReg(KindA64::w, index);

        Label skip, exit;

        // For equality comparison, 'luaV_equalval' expects tag to be equal before the call
        if (cond == IrCondition::Equal)
        {
            RegisterA64 tempa = regs.allocTemp(KindA64::w);
            RegisterA64 tempb = regs.allocTemp(KindA64::w);

            build.ldr(tempa, tempAddr(OP_A(inst), offsetof(TValue, tt)));
            build.ldr(tempb, tempAddr(OP_B(inst), offsetof(TValue, tt)));
            build.cmp(tempa, tempb);

            // If the tags are not equal, skip the call and set result to 0
            build.b(ConditionA64::NotEqual, skip);
        }

        if (FFlag::LuauCodegenCallWrapImproved)
        {
            // We have reserved the result register, so we can free it now so it is not recorded in the spill sequence
            regs.freeReg(inst.regA64);
        }

        size_t spills = regs.spill(index);

        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.add(x2, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));

        if (cond == IrCondition::LessEqual)
            build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaV_lessequal)));
        else if (cond == IrCondition::Less)
            build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaV_lessthan)));
        else if (cond == IrCondition::Equal)
            build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaV_equalval)));
        else
            CODEGEN_ASSERT(!"Unsupported condition");

        build.blr(x3);

        if (FFlag::LuauCodegenCallWrapImproved)
        {
            if (inst.regA64 != w0)
                build.mov(inst.regA64, w0);

            inst.regA64 = regs.takeReg(inst.regA64, index);

            emitUpdateBase(build);

            regs.restore(spills);
        }
        else
        {
            emitUpdateBase(build);

            inst.regA64 = regs.takeReg(w0, index);
        }

        if (cond == IrCondition::Equal)
        {
            build.b(exit);
            build.setLabel(skip);

            build.mov(inst.regA64, 0);
            build.setLabel(exit);
        }

        // In case we made a call, skip high register bits clear, only consumer is JUMP_CMP_INT which doesn't read them
        break;
    }
    case IrCmd::CMP_TAG:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});

        IrCondition cond = conditionOp(OP_C(inst));
        CODEGEN_ASSERT(cond == IrCondition::Equal || cond == IrCondition::NotEqual);
        RegisterA64 aReg = noreg;
        RegisterA64 bReg = noreg;

        if (OP_A(inst).kind == IrOpKind::Inst)
        {
            aReg = regOp(OP_A(inst));
        }
        else if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            aReg = regs.allocTemp(KindA64::w);
            AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, tt));
            build.ldr(aReg, addr);
        }
        else
        {
            CODEGEN_ASSERT(OP_A(inst).kind == IrOpKind::Constant);
        }

        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            bReg = regOp(OP_B(inst));
        }
        else if (OP_B(inst).kind == IrOpKind::VmReg)
        {
            bReg = regs.allocTemp(KindA64::w);
            AddressA64 addr = tempAddr(OP_B(inst), offsetof(TValue, tt));
            build.ldr(bReg, addr);
        }
        else
        {
            CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);
        }

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            build.cmp(bReg, uint16_t(tagOp(OP_A(inst))));
            build.cset(inst.regA64, getInverseCondition(getConditionInt(cond)));
        }
        else if (OP_B(inst).kind == IrOpKind::Constant)
        {
            build.cmp(aReg, uint16_t(tagOp(OP_B(inst))));
            build.cset(inst.regA64, getConditionInt(cond));
        }
        else
        {
            build.cmp(aReg, bReg);
            build.cset(inst.regA64, getConditionInt(cond));
        }
        break;
    }
    case IrCmd::CMP_SPLIT_TVALUE:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});

        // Second operand of this instruction must be a constant
        // Without a constant type, we wouldn't know the correct way to compare the values at lowering time
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);

        IrCondition cond = conditionOp(OP_E(inst));
        CODEGEN_ASSERT(cond == IrCondition::Equal || cond == IrCondition::NotEqual);

        // Check tag equality first
        RegisterA64 temp = regs.allocTemp(KindA64::w);

        if (OP_A(inst).kind != IrOpKind::Constant)
        {
            build.cmp(regOp(OP_A(inst)), uint16_t(tagOp(OP_B(inst))));
            build.cset(temp, getConditionInt(cond));
        }
        else
        {
            // Constant folding had to handle different constant tags
            CODEGEN_ASSERT(tagOp(OP_A(inst)) == tagOp(OP_B(inst)));
        }

        if (tagOp(OP_B(inst)) == LUA_TBOOLEAN)
        {
            if (OP_C(inst).kind == IrOpKind::Constant)
            {
                CODEGEN_ASSERT(intOp(OP_C(inst)) == 0 || intOp(OP_C(inst)) == 1);
                build.cmp(regOp(OP_D(inst)), uint16_t(intOp(OP_C(inst)))); // swapped arguments
            }
            else if (OP_D(inst).kind == IrOpKind::Constant)
            {
                CODEGEN_ASSERT(intOp(OP_D(inst)) == 0 || intOp(OP_D(inst)) == 1);
                build.cmp(regOp(OP_C(inst)), uint16_t(intOp(OP_D(inst))));
            }
            else
            {
                build.cmp(regOp(OP_C(inst)), regOp(OP_D(inst)));
            }

            build.cset(inst.regA64, getConditionInt(cond));
        }
        else if (tagOp(OP_B(inst)) == LUA_TSTRING)
        {
            build.cmp(regOp(OP_C(inst)), regOp(OP_D(inst)));
            build.cset(inst.regA64, getConditionInt(cond));
        }
        else if (tagOp(OP_B(inst)) == LUA_TNUMBER)
        {
            RegisterA64 temp1 = tempDouble(OP_C(inst));
            RegisterA64 temp2 = tempDouble(OP_D(inst));

            build.fcmp(temp1, temp2);
            build.cset(inst.regA64, getConditionFP(cond));
        }
        else if (tagOp(OP_B(inst)) == LUA_TINTEGER)
        {
            RegisterA64 temp1 = tempInt64(OP_C(inst));
            RegisterA64 temp2 = tempInt64(OP_D(inst));

            build.cmp(temp1, temp2);
            build.cset(inst.regA64, getConditionInt64(cond));
        }
        else
        {
            CODEGEN_ASSERT(!"unsupported type tag in CMP_SPLIT_TVALUE");
        }

        if (OP_A(inst).kind != IrOpKind::Constant)
        {
            if (cond == IrCondition::Equal)
                build.and_(inst.regA64, inst.regA64, temp);
            else
                build.orr(inst.regA64, inst.regA64, temp);
        }
        break;
    }
    case IrCmd::JUMP:
        if (OP_A(inst).kind == IrOpKind::Undef || OP_A(inst).kind == IrOpKind::VmExit)
        {
            Label fresh;
            build.b(getTargetLabel(OP_A(inst), fresh));
            finalizeTargetLabel(OP_A(inst), fresh);
        }
        else
        {
            jumpOrFallthrough(blockOp(OP_A(inst)), next);
        }
        break;
    case IrCmd::JUMP_IF_TRUTHY:
    {
        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.ldr(temp, mem(rBase, vmRegOp(OP_A(inst)) * sizeof(TValue) + offsetof(TValue, tt)));
        // nil => falsy
        CODEGEN_ASSERT(LUA_TNIL == 0);
        build.cbz(temp, labelOp(OP_C(inst)));
        // not boolean => truthy
        build.cmp(temp, uint16_t(LUA_TBOOLEAN));
        build.b(ConditionA64::NotEqual, labelOp(OP_B(inst)));
        // compare boolean value
        build.ldr(temp, mem(rBase, vmRegOp(OP_A(inst)) * sizeof(TValue) + offsetof(TValue, value)));
        build.cbnz(temp, labelOp(OP_B(inst)));
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    }
    case IrCmd::JUMP_IF_FALSY:
    {
        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.ldr(temp, mem(rBase, vmRegOp(OP_A(inst)) * sizeof(TValue) + offsetof(TValue, tt)));
        // nil => falsy
        CODEGEN_ASSERT(LUA_TNIL == 0);
        build.cbz(temp, labelOp(OP_B(inst)));
        // not boolean => truthy
        build.cmp(temp, uint16_t(LUA_TBOOLEAN));
        build.b(ConditionA64::NotEqual, labelOp(OP_C(inst)));
        // compare boolean value
        build.ldr(temp, mem(rBase, vmRegOp(OP_A(inst)) * sizeof(TValue) + offsetof(TValue, value)));
        build.cbz(temp, labelOp(OP_B(inst)));
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    }
    case IrCmd::JUMP_EQ_TAG:
    {
        RegisterA64 zr = noreg;
        RegisterA64 aReg = noreg;
        RegisterA64 bReg = noreg;

        if (OP_A(inst).kind == IrOpKind::Inst)
        {
            aReg = regOp(OP_A(inst));
        }
        else if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            aReg = regs.allocTemp(KindA64::w);
            AddressA64 addr = tempAddr(OP_A(inst), offsetof(TValue, tt));
            build.ldr(aReg, addr);
        }
        else
        {
            CODEGEN_ASSERT(OP_A(inst).kind == IrOpKind::Constant);
        }

        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            bReg = regOp(OP_B(inst));
        }
        else if (OP_B(inst).kind == IrOpKind::VmReg)
        {
            bReg = regs.allocTemp(KindA64::w);
            AddressA64 addr = tempAddr(OP_B(inst), offsetof(TValue, tt));
            build.ldr(bReg, addr);
        }
        else
        {
            CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);
        }

        if (OP_A(inst).kind == IrOpKind::Constant && tagOp(OP_A(inst)) == 0)
            zr = bReg;
        else if (OP_B(inst).kind == IrOpKind::Constant && tagOp(OP_B(inst)) == 0)
            zr = aReg;
        else if (OP_B(inst).kind == IrOpKind::Constant)
            build.cmp(aReg, uint16_t(tagOp(OP_B(inst))));
        else if (OP_A(inst).kind == IrOpKind::Constant)
            build.cmp(bReg, uint16_t(tagOp(OP_A(inst))));
        else
            build.cmp(aReg, bReg);

        if (isFallthroughBlock(blockOp(OP_D(inst)), next))
        {
            if (zr != noreg)
                build.cbz(zr, labelOp(OP_C(inst)));
            else
                build.b(ConditionA64::Equal, labelOp(OP_C(inst)));
            jumpOrFallthrough(blockOp(OP_D(inst)), next);
        }
        else
        {
            if (zr != noreg)
                build.cbnz(zr, labelOp(OP_D(inst)));
            else
                build.b(ConditionA64::NotEqual, labelOp(OP_D(inst)));
            jumpOrFallthrough(blockOp(OP_C(inst)), next);
        }
        break;
    }
    case IrCmd::JUMP_CMP_INT:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        if (cond == IrCondition::Equal && intOp(OP_B(inst)) == 0)
        {
            build.cbz(regOp(OP_A(inst)), labelOp(OP_D(inst)));
        }
        else if (cond == IrCondition::NotEqual && intOp(OP_B(inst)) == 0)
        {
            build.cbnz(regOp(OP_A(inst)), labelOp(OP_D(inst)));
        }
        else
        {
            CODEGEN_ASSERT(unsigned(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate);
            build.cmp(regOp(OP_A(inst)), uint16_t(intOp(OP_B(inst))));
            build.b(getConditionInt(cond), labelOp(OP_D(inst)));
        }
        jumpOrFallthrough(blockOp(OP_E(inst)), next);
        break;
    }
    case IrCmd::JUMP_EQ_POINTER:
        build.cmp(regOp(OP_A(inst)), regOp(OP_B(inst)));
        build.b(ConditionA64::Equal, labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    case IrCmd::JUMP_CMP_NUM:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        if (OP_B(inst).kind == IrOpKind::Constant && doubleOp(OP_B(inst)) == 0.0)
        {
            RegisterA64 temp = tempDouble(OP_A(inst));

            build.fcmpz(temp);
        }
        else
        {
            RegisterA64 temp1 = tempDouble(OP_A(inst));
            RegisterA64 temp2 = tempDouble(OP_B(inst));

            build.fcmp(temp1, temp2);
        }

        build.b(getConditionFP(cond), labelOp(OP_D(inst)));
        jumpOrFallthrough(blockOp(OP_E(inst)), next);
        break;
    }
    case IrCmd::JUMP_CMP_FLOAT:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        if (OP_B(inst).kind == IrOpKind::Constant && float(doubleOp(OP_B(inst))) == 0.0f)
        {
            RegisterA64 temp = tempFloat(OP_A(inst));

            build.fcmpz(temp);
        }
        else
        {
            RegisterA64 temp1 = tempFloat(OP_A(inst));
            RegisterA64 temp2 = tempFloat(OP_B(inst));

            build.fcmp(temp1, temp2);
        }

        build.b(getConditionFP(cond), labelOp(OP_D(inst)));
        jumpOrFallthrough(blockOp(OP_E(inst)), next);
        break;
    }
    case IrCmd::JUMP_FORN_LOOP_COND:
    {
        RegisterA64 index = tempDouble(OP_A(inst));
        RegisterA64 limit = tempDouble(OP_B(inst));
        RegisterA64 step = tempDouble(OP_C(inst));

        Label direct;

        // step > 0
        build.fcmpz(step);
        build.b(getConditionFP(IrCondition::Greater), direct);

        // !(limit <= index)
        build.fcmp(limit, index);
        build.b(getConditionFP(IrCondition::NotLessEqual), labelOp(OP_E(inst)));
        build.b(labelOp(OP_D(inst)));

        // !(index <= limit)
        build.setLabel(direct);

        build.fcmp(index, limit);
        build.b(getConditionFP(IrCondition::NotLessEqual), labelOp(OP_E(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    }
    // IrCmd::JUMP_SLOT_MATCH implemented below
    case IrCmd::TABLE_LEN:
    {
        RegisterA64 reg = regOp(OP_A(inst)); // note: we need to call regOp before spill so that we don't do redundant reloads
        regs.spill(index, {reg});
        build.mov(x0, reg);
        build.ldr(x1, mem(rNativeContext, offsetof(NativeContext, luaH_getn)));
        build.blr(x1);

        inst.regA64 = regs.takeReg(w0, index);

        build.ubfx(inst.regA64, inst.regA64, 0, 32); // Ensure high register bits are cleared
        break;
    }
    case IrCmd::STRING_LEN:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);

        build.ldr(inst.regA64, mem(regOp(OP_A(inst)), offsetof(TString, len)));
        break;
    }
    case IrCmd::TABLE_SETNUM:
    {
        // note: we need to call regOp before spill so that we don't do redundant reloads
        RegisterA64 table = regOp(OP_A(inst));
        RegisterA64 key = regOp(OP_B(inst));
        RegisterA64 temp = regs.allocTemp(KindA64::w);

        regs.spill(index, {table, key});

        if (w1 != key)
        {
            build.mov(x1, table);
            build.mov(w2, key);
        }
        else
        {
            build.mov(temp, w1);
            build.mov(x1, table);
            build.mov(w2, temp);
        }

        build.mov(x0, rState);
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaH_setnum)));
        build.blr(x3);
        inst.regA64 = regs.takeReg(x0, index);
        break;
    }
    case IrCmd::NEW_TABLE:
    {
        regs.spill(index);
        build.mov(x0, rState);
        build.mov(x1, uintOp(OP_A(inst)));
        build.mov(x2, uintOp(OP_B(inst)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaH_new)));
        build.blr(x3);
        inst.regA64 = regs.takeReg(x0, index);
        break;
    }
    case IrCmd::DUP_TABLE:
    {
        RegisterA64 reg = regOp(OP_A(inst)); // note: we need to call regOp before spill so that we don't do redundant reloads
        regs.spill(index, {reg});
        build.mov(x1, reg);
        build.mov(x0, rState);
        build.ldr(x2, mem(rNativeContext, offsetof(NativeContext, luaH_clone)));
        build.blr(x2);
        inst.regA64 = regs.takeReg(x0, index);
        break;
    }
    case IrCmd::TRY_NUM_TO_INDEX:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);
        RegisterA64 temp1 = tempDouble(OP_A(inst));

        if (build.features & Feature_JSCVT)
        {
            build.fjcvtzs(inst.regA64, temp1); // fjcvtzs sets PSTATE.Z (equal) iff conversion is exact
            build.b(ConditionA64::NotEqual, labelOp(OP_B(inst)));
        }
        else
        {
            RegisterA64 temp2 = regs.allocTemp(KindA64::d);

            build.fcvtzs(inst.regA64, temp1);
            build.scvtf(temp2, inst.regA64);
            build.fcmp(temp1, temp2);
            build.b(ConditionA64::NotEqual, labelOp(OP_B(inst)));
        }
        break;
    }
    case IrCmd::TRY_CALL_FASTGETTM:
    {
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::w);

        build.ldr(temp1, mem(regOp(OP_A(inst)), offsetof(LuaTable, metatable)));
        build.cbz(temp1, labelOp(OP_C(inst))); // no metatable

        build.ldrb(temp2, mem(temp1, offsetof(LuaTable, tmcache)));
        build.tst(temp2, 1 << intOp(OP_B(inst)));             // can't use tbz/tbnz because their jump offsets are too short
        build.b(ConditionA64::NotEqual, labelOp(OP_C(inst))); // Equal = Zero after tst; tmcache caches *absence* of metamethods

        regs.spill(index, {temp1});
        build.mov(x0, temp1);
        build.mov(w1, intOp(OP_B(inst)));
        build.ldr(x2, mem(rGlobalState, offsetof(global_State, tmname) + intOp(OP_B(inst)) * sizeof(TString*)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaT_gettm)));
        build.blr(x3);

        build.cbz(x0, labelOp(OP_C(inst))); // no tag method

        inst.regA64 = regs.takeReg(x0, index);
        break;
    }
    case IrCmd::NEW_USERDATA:
    {
        regs.spill(index);
        build.mov(x0, rState);
        build.mov(x1, intOp(OP_A(inst)));
        build.mov(x2, intOp(OP_B(inst)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, newUserdata)));
        build.blr(x3);
        inst.regA64 = regs.takeReg(x0, index);
        break;
    }
    case IrCmd::INT64_TO_NUM:
    {
        inst.regA64 = regs.allocReg(KindA64::d, index);
        RegisterA64 temp = tempInt64(OP_A(inst));
        build.scvtf(inst.regA64, temp);
        break;
    }
    case IrCmd::INT_TO_NUM:
    {
        inst.regA64 = regs.allocReg(KindA64::d, index);
        RegisterA64 temp = tempInt(OP_A(inst));
        build.scvtf(inst.regA64, temp);
        break;
    }
    case IrCmd::UINT_TO_NUM:
    {
        inst.regA64 = regs.allocReg(KindA64::d, index);
        RegisterA64 temp = tempInt(OP_A(inst));
        build.ucvtf(inst.regA64, temp);
        break;
    }
    case IrCmd::UINT_TO_FLOAT:
    {
        inst.regA64 = regs.allocReg(KindA64::s, index);
        RegisterA64 temp = tempInt(OP_A(inst));
        build.ucvtf(inst.regA64, temp);
        break;
    }
    case IrCmd::NUM_TO_INT:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.fcvtzs(inst.regA64, temp);
        break;
    }
    case IrCmd::NUM_TO_INT64:
    {
        inst.regA64 = regs.allocReg(KindA64::x, index);
        RegisterA64 temp = tempDouble(OP_A(inst));
        build.fcvtzs(inst.regA64, temp);
        break;
    }
    case IrCmd::NUM_TO_UINT:
    {
        inst.regA64 = regs.allocReg(KindA64::w, index);
        RegisterA64 temp = tempDouble(OP_A(inst));
        // note: we don't use fcvtzu for consistency with C++ code
        build.fcvtzs(castReg(KindA64::x, inst.regA64), temp);
        break;
    }
    case IrCmd::FLOAT_TO_NUM:
        inst.regA64 = regs.allocReg(KindA64::d, index);

        build.fcvt(inst.regA64, regOp(OP_A(inst)));
        break;
    case IrCmd::NUM_TO_FLOAT:
        inst.regA64 = regs.allocReg(KindA64::s, index);

        build.fcvt(inst.regA64, regOp(OP_A(inst)));
        break;
    case IrCmd::FLOAT_TO_VEC:
    {
        inst.regA64 = regs.allocReg(KindA64::q, index);

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            float value = float(doubleOp(OP_A(inst)));
            uint32_t asU32;
            static_assert(sizeof(asU32) == sizeof(value), "Expecting float to be 32-bit");
            memcpy(&asU32, &value, sizeof(value));

            if (AssemblyBuilderA64::isFmovSupportedFp32(value))
            {
                build.fmov(inst.regA64, value);
            }
            else
            {
                RegisterA64 temp = regs.allocTemp(KindA64::x);

                uint32_t vec[4] = {asU32, asU32, asU32, 0};
                build.adr(temp, vec, sizeof(vec));
                build.ldr(inst.regA64, temp);
            }
        }
        else
        {
            RegisterA64 temp = tempFloat(OP_A(inst));

            build.dup_4s(inst.regA64, castReg(KindA64::q, temp), 0);
        }
        break;
    }
    case IrCmd::TAG_VECTOR:
    {
        inst.regA64 = regs.allocReuse(KindA64::q, index, {OP_A(inst)});

        RegisterA64 reg = regOp(OP_A(inst));
        RegisterA64 tempw = regs.allocTemp(KindA64::w);

        if (inst.regA64 != reg)
            build.mov(inst.regA64, reg);

        build.mov(tempw, LUA_TVECTOR);
        build.ins_4s(inst.regA64, tempw, 3);
        break;
    }
    case IrCmd::TRUNCATE_UINT:
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});

        build.ubfx(castReg(KindA64::x, inst.regA64), castReg(KindA64::x, regOp(OP_A(inst))), 0, 32); // explicit uxtw
        break;
    case IrCmd::ADJUST_STACK_TO_REG:
    {
        RegisterA64 temp = regs.allocTemp(KindA64::x);

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            build.add(temp, rBase, uint16_t((vmRegOp(OP_A(inst)) + intOp(OP_B(inst))) * sizeof(TValue)));
            build.str(temp, mem(rState, offsetof(lua_State, top)));
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.add(temp, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
            build.add(temp, temp, regOp(OP_B(inst)), kTValueSizeLog2); // implicit uxtw
            build.str(temp, mem(rState, offsetof(lua_State, top)));
        }
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    }
    case IrCmd::ADJUST_STACK_TO_TOP:
    {
        RegisterA64 temp = regs.allocTemp(KindA64::x);
        build.ldr(temp, mem(rState, offsetof(lua_State, ci)));
        build.ldr(temp, mem(temp, offsetof(CallInfo, top)));
        build.str(temp, mem(rState, offsetof(lua_State, top)));
        break;
    }
    case IrCmd::FASTCALL:
        regs.spill(index);

        error |= !emitBuiltin(build, function, regs, uintOp(OP_A(inst)), vmRegOp(OP_B(inst)), vmRegOp(OP_C(inst)), intOp(OP_D(inst)));
        break;
    case IrCmd::INVOKE_FASTCALL:
    {
        // We might need a temporary and we have to preserve it over the spill
        RegisterA64 temp = regs.allocTemp(KindA64::q);
        regs.spill(index, {temp});

        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));
        build.add(x2, rBase, uint16_t(vmRegOp(OP_C(inst)) * sizeof(TValue)));
        build.mov(w3, intOp(OP_G(inst))); // nresults

        // 'E' argument can only be produced by LOP_FASTCALL3 lowering
        if (OP_E(inst).kind != IrOpKind::Undef)
        {
            CODEGEN_ASSERT(intOp(OP_F(inst)) == 3);

            build.ldr(x4, mem(rState, offsetof(lua_State, top)));

            build.ldr(temp, mem(rBase, vmRegOp(OP_D(inst)) * sizeof(TValue)));
            build.str(temp, mem(x4, 0));

            build.ldr(temp, mem(rBase, vmRegOp(OP_E(inst)) * sizeof(TValue)));
            build.str(temp, mem(x4, sizeof(TValue)));
        }
        else
        {
            if (OP_D(inst).kind == IrOpKind::VmReg)
                build.add(x4, rBase, uint16_t(vmRegOp(OP_D(inst)) * sizeof(TValue)));
            else if (OP_D(inst).kind == IrOpKind::VmConst)
                emitAddOffset(build, x4, rConstants, vmConstOp(OP_D(inst)) * sizeof(TValue));
            else
                CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::Undef);
        }

        // nparams
        if (intOp(OP_F(inst)) == LUA_MULTRET)
        {
            // L->top - (ra + 1)
            build.ldr(x5, mem(rState, offsetof(lua_State, top)));
            build.sub(x5, x5, rBase);
            build.sub(x5, x5, uint16_t((vmRegOp(OP_B(inst)) + 1) * sizeof(TValue)));
            build.lsr(x5, x5, kTValueSizeLog2);
        }
        else
            build.mov(w5, intOp(OP_F(inst)));

        build.ldr(x6, mem(rNativeContext, offsetof(NativeContext, luauF_table) + uintOp(OP_A(inst)) * sizeof(luau_FastFunction)));
        build.blr(x6);

        inst.regA64 = regs.takeReg(w0, index);
        // Skipping high register bits clear, only consumer is CHECK_FASTCALL_RES which doesn't read them
        break;
    }
    case IrCmd::CHECK_FASTCALL_RES:
        build.cmp(regOp(OP_A(inst)), uint16_t(0));
        build.b(ConditionA64::Less, labelOp(OP_B(inst)));
        break;
    case IrCmd::DO_ARITH:
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));

        if (OP_B(inst).kind == IrOpKind::VmConst)
            emitAddOffset(build, x2, rConstants, vmConstOp(OP_B(inst)) * sizeof(TValue));
        else
            build.add(x2, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));

        if (OP_C(inst).kind == IrOpKind::VmConst)
            emitAddOffset(build, x3, rConstants, vmConstOp(OP_C(inst)) * sizeof(TValue));
        else
            build.add(x3, rBase, uint16_t(vmRegOp(OP_C(inst)) * sizeof(TValue)));

        switch (TMS(intOp(OP_D(inst))))
        {
        case TM_ADD:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithadd)));
            break;
        case TM_SUB:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithsub)));
            break;
        case TM_MUL:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithmul)));
            break;
        case TM_DIV:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithdiv)));
            break;
        case TM_IDIV:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithidiv)));
            break;
        case TM_MOD:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithmod)));
            break;
        case TM_POW:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithpow)));
            break;
        case TM_UNM:
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_doarithunm)));
            break;
        default:
            CODEGEN_ASSERT(!"Invalid doarith helper operation tag");
            break;
        }

        build.blr(x4);

        emitUpdateBase(build);
        break;
    case IrCmd::DO_LEN:
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.add(x2, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaV_dolen)));
        build.blr(x3);

        emitUpdateBase(build);
        break;
    case IrCmd::GET_TABLE:
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));

        if (OP_C(inst).kind == IrOpKind::VmReg)
            build.add(x2, rBase, uint16_t(vmRegOp(OP_C(inst)) * sizeof(TValue)));
        else if (OP_C(inst).kind == IrOpKind::Constant)
        {
            TValue n = {};
            setnvalue(&n, uintOp(OP_C(inst)));
            build.adr(x2, &n, sizeof(n));
        }
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");

        build.add(x3, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_gettable)));
        build.blr(x4);

        emitUpdateBase(build);
        break;
    case IrCmd::SET_TABLE:
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));

        if (OP_C(inst).kind == IrOpKind::VmReg)
            build.add(x2, rBase, uint16_t(vmRegOp(OP_C(inst)) * sizeof(TValue)));
        else if (OP_C(inst).kind == IrOpKind::Constant)
        {
            TValue n = {};
            setnvalue(&n, uintOp(OP_C(inst)));
            build.adr(x2, &n, sizeof(n));
        }
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");

        build.add(x3, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaV_settable)));
        build.blr(x4);

        emitUpdateBase(build);
        break;
    case IrCmd::GET_CACHED_IMPORT:
    {
        regs.spill(index);

        Label skip, exit;

        RegisterA64 tempTag = regs.allocTemp(KindA64::w);

        AddressA64 addrConstTag = tempAddr(OP_B(inst), offsetof(TValue, tt));
        build.ldr(tempTag, addrConstTag);

        // If the constant for the import is set, we will use it directly, otherwise we have to call an import path lookup function
        CODEGEN_ASSERT(LUA_TNIL == 0);
        build.cbnz(tempTag, skip);

        {
            build.mov(x0, rState);
            build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
            build.mov(w2, importOp(OP_C(inst)));
            build.mov(w3, uintOp(OP_D(inst)));
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, getImport)));
            build.blr(x4);

            emitUpdateBase(build);
            build.b(exit);
        }

        build.setLabel(skip);

        RegisterA64 tempTv = regs.allocTemp(KindA64::q);

        AddressA64 addrConst = tempAddr(OP_B(inst), 0);
        build.ldr(tempTv, addrConst);

        AddressA64 addrReg = tempAddr(OP_A(inst), 0);
        build.str(tempTv, addrReg);

        build.setLabel(exit);
        break;
    }
    case IrCmd::CONCAT:
        regs.spill(index);
        build.mov(x0, rState);
        build.mov(w1, uintOp(OP_B(inst)));
        build.mov(w2, vmRegOp(OP_A(inst)) + uintOp(OP_B(inst)) - 1);
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaV_concat)));
        build.blr(x3);

        emitUpdateBase(build);
        break;
    case IrCmd::GET_UPVALUE:
    {
        inst.regA64 = regs.allocReg(KindA64::q, index);

        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::w);

        build.add(temp1, rClosure, uint16_t(offsetof(Closure, l.uprefs) + sizeof(TValue) * vmUpvalueOp(OP_A(inst))));

        // uprefs[] is either an actual value, or it points to UpVal object which has a pointer to value
        Label skip;
        build.ldr(temp2, mem(temp1, offsetof(TValue, tt)));
        build.cmp(temp2, uint16_t(LUA_TUPVAL));
        build.b(ConditionA64::NotEqual, skip);

        // UpVal.v points to the value (either on stack, or on heap inside each UpVal, but we can deref it unconditionally)
        build.ldr(temp1, mem(temp1, offsetof(TValue, value.gc)));
        build.ldr(temp1, mem(temp1, offsetof(UpVal, v)));

        build.setLabel(skip);

        build.ldr(inst.regA64, temp1);
        break;
    }
    case IrCmd::SET_UPVALUE:
    {
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::x);

        // UpVal*
        build.ldr(temp1, mem(rClosure, offsetof(Closure, l.uprefs) + sizeof(TValue) * vmUpvalueOp(OP_A(inst)) + offsetof(TValue, value.gc)));

        build.ldr(temp2, mem(temp1, offsetof(UpVal, v)));
        build.str(regOp(OP_B(inst)), temp2);

        if (OP_C(inst).kind == IrOpKind::Undef || isGCO(tagOp(OP_C(inst))))
        {
            RegisterA64 value = regOp(OP_B(inst));

            Label skip;
            checkObjectBarrierConditions(temp1, temp2, value, OP_B(inst), OP_C(inst).kind == IrOpKind::Undef ? -1 : tagOp(OP_C(inst)), skip);

            size_t spills = regs.spill(index, {temp1, value});

            build.mov(x1, temp1);
            build.mov(x0, rState);
            build.fmov(x2, castReg(KindA64::d, value));
            build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaC_barrierf)));
            build.blr(x3);

            regs.restore(spills); // need to restore before skip so that registers are in a consistent state

            // note: no emitUpdateBase necessary because luaC_ barriers do not reallocate stack
            build.setLabel(skip);
        }
        break;
    }
    case IrCmd::CHECK_TAG:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_C(inst), fresh);

        if (tagOp(OP_B(inst)) == 0)
        {
            build.cbnz(regOp(OP_A(inst)), fail);
        }
        else
        {
            build.cmp(regOp(OP_A(inst)), uint16_t(tagOp(OP_B(inst))));
            build.b(ConditionA64::NotEqual, fail);
        }

        finalizeTargetLabel(OP_C(inst), fresh);
        break;
    }
    case IrCmd::CHECK_TRUTHY:
    {
        // Constant tags which don't require boolean value check should've been removed in constant folding
        CODEGEN_ASSERT(OP_A(inst).kind != IrOpKind::Constant || tagOp(OP_A(inst)) == LUA_TBOOLEAN);

        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& target = getTargetLabel(OP_C(inst), fresh);

        Label skip;

        if (OP_A(inst).kind != IrOpKind::Constant)
        {
            // fail to fallback on 'nil' (falsy)
            CODEGEN_ASSERT(LUA_TNIL == 0);
            build.cbz(regOp(OP_A(inst)), target);

            // skip value test if it's not a boolean (truthy)
            build.cmp(regOp(OP_A(inst)), uint16_t(LUA_TBOOLEAN));
            build.b(ConditionA64::NotEqual, skip);
        }

        // fail to fallback on 'false' boolean value (falsy)
        if (OP_B(inst).kind != IrOpKind::Constant)
        {
            build.cbz(regOp(OP_B(inst)), target);
        }
        else
        {
            if (intOp(OP_B(inst)) == 0)
                build.b(target);
        }

        if (OP_A(inst).kind != IrOpKind::Constant)
            build.setLabel(skip);

        finalizeTargetLabel(OP_C(inst), fresh);
        break;
    }
    case IrCmd::CHECK_READONLY:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.ldrb(temp, mem(regOp(OP_A(inst)), offsetof(LuaTable, readonly)));
        build.cbnz(temp, getTargetLabel(OP_B(inst), fresh));
        finalizeTargetLabel(OP_B(inst), fresh);
        break;
    }
    case IrCmd::CHECK_NO_METATABLE:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        RegisterA64 temp = regs.allocTemp(KindA64::x);
        build.ldr(temp, mem(regOp(OP_A(inst)), offsetof(LuaTable, metatable)));
        build.cbnz(temp, getTargetLabel(OP_B(inst), fresh));
        finalizeTargetLabel(OP_B(inst), fresh);
        break;
    }
    case IrCmd::CHECK_SAFE_ENV:
    {
        checkSafeEnv(OP_A(inst), next);
        break;
    }
    case IrCmd::CHECK_ARRAY_SIZE:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_C(inst), fresh);

        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.ldr(temp, mem(regOp(OP_A(inst)), offsetof(LuaTable, sizearray)));

        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.cmp(temp, regOp(OP_B(inst)));
            build.b(ConditionA64::UnsignedLessEqual, fail);
        }
        else if (OP_B(inst).kind == IrOpKind::Constant)
        {
            if (intOp(OP_B(inst)) == 0)
            {
                build.cbz(temp, fail);
            }
            else if (size_t(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
            {
                build.cmp(temp, uint16_t(intOp(OP_B(inst))));
                build.b(ConditionA64::UnsignedLessEqual, fail);
            }
            else
            {
                RegisterA64 temp2 = regs.allocTemp(KindA64::w);
                build.mov(temp2, intOp(OP_B(inst)));
                build.cmp(temp, temp2);
                build.b(ConditionA64::UnsignedLessEqual, fail);
            }
        }
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");

        finalizeTargetLabel(OP_C(inst), fresh);
        break;
    }
    case IrCmd::JUMP_SLOT_MATCH:
    case IrCmd::CHECK_SLOT_MATCH:
    {
        Label abort; // used when guard aborts execution
        const IrOp& mismatchOp = inst.cmd == IrCmd::JUMP_SLOT_MATCH ? OP_D(inst) : OP_C(inst);
        Label& mismatch = mismatchOp.kind == IrOpKind::Undef ? abort : labelOp(mismatchOp);

        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp1w = castReg(KindA64::w, temp1);
        RegisterA64 temp2 = regs.allocTemp(KindA64::x);

        static_assert(offsetof(LuaNode, key.value) == offsetof(LuaNode, key) && kOffsetOfTKeyTagNext >= 8 && kOffsetOfTKeyTagNext < 16);
        build.ldp(
            temp1, temp2, mem(regOp(OP_A(inst)), offsetof(LuaNode, key))
        ); // load key.value into temp1 and key.tt (alongside other bits) into temp2
        build.ubfx(temp2, temp2, (kOffsetOfTKeyTagNext - 8) * 8, kTKeyTagBits); // .tt is right before .next, and 8 bytes are skipped by ldp
        build.cmp(temp2, uint16_t(LUA_TSTRING));
        build.b(ConditionA64::NotEqual, mismatch);

        AddressA64 addr = tempAddr(OP_B(inst), offsetof(TValue, value));
        build.ldr(temp2, addr);
        build.cmp(temp1, temp2);
        build.b(ConditionA64::NotEqual, mismatch);

        build.ldr(temp1w, mem(regOp(OP_A(inst)), offsetof(LuaNode, val.tt)));
        CODEGEN_ASSERT(LUA_TNIL == 0);
        build.cbz(temp1w, mismatch);

        if (inst.cmd == IrCmd::JUMP_SLOT_MATCH)
            jumpOrFallthrough(blockOp(OP_C(inst)), next);
        else if (abort.id)
            emitAbort(build, abort);
        break;
    }
    case IrCmd::CHECK_NODE_NO_NEXT:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        RegisterA64 temp = regs.allocTemp(KindA64::w);

        build.ldr(temp, mem(regOp(OP_A(inst)), offsetof(LuaNode, key) + kOffsetOfTKeyTagNext));
        build.lsr(temp, temp, kTKeyTagBits);
        build.cbnz(temp, getTargetLabel(OP_B(inst), fresh));
        finalizeTargetLabel(OP_B(inst), fresh);
        break;
    }
    case IrCmd::CHECK_NODE_VALUE:
    {
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        RegisterA64 temp = regs.allocTemp(KindA64::w);

        build.ldr(temp, mem(regOp(OP_A(inst)), offsetof(LuaNode, val.tt)));
        CODEGEN_ASSERT(LUA_TNIL == 0);
        build.cbz(temp, getTargetLabel(OP_B(inst), fresh));
        finalizeTargetLabel(OP_B(inst), fresh);
        break;
    }
    case IrCmd::CHECK_BUFFER_LEN:
    {
        if (FFlag::LuauCodegenBufferRangeMerge4)
        {
            int minOffset = intOp(OP_C(inst));
            int maxOffset = intOp(OP_D(inst));
            CODEGEN_ASSERT(minOffset < maxOffset);
            CODEGEN_ASSERT(minOffset >= -int(AssemblyBuilderA64::kMaxImmediate) && minOffset <= int(AssemblyBuilderA64::kMaxImmediate));

            int accessSize = maxOffset - minOffset;
            CODEGEN_ASSERT(accessSize > 0 && accessSize <= int(AssemblyBuilderA64::kMaxImmediate));

            Label fresh; // used when guard aborts execution or jumps to a VM exit
            Label& target = getTargetLabel(OP_F(inst), fresh);

            // Check if we are acting not only as a guard for the size, but as a guard that offset represents an exact integer
            if (OP_E(inst).kind != IrOpKind::Undef)
            {
                CODEGEN_ASSERT(getCmdValueKind(function.instOp(OP_B(inst)).cmd) == IrValueKind::Int);
                CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(OP_B(inst)).cmd)); // Ensure that high register bits are cleared

                if ((build.features & Feature_JSCVT) != 0)
                {
                    RegisterA64 temp = regs.allocTemp(KindA64::w);

                    build.fjcvtzs(temp, regOp(OP_E(inst))); // fjcvtzs sets PSTATE.Z (equal) iff conversion is exact
                    build.b(ConditionA64::NotEqual, target);
                }
                else
                {
                    RegisterA64 temp = regs.allocTemp(KindA64::d);

                    build.scvtf(temp, regOp(OP_B(inst)));
                    build.fcmp(regOp(OP_E(inst)), temp);
                    build.b(ConditionA64::NotEqual, target);
                }
            }

            RegisterA64 temp = regs.allocTemp(KindA64::w);
            build.ldr(temp, mem(regOp(OP_A(inst)), offsetof(Buffer, len)));

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(OP_B(inst)).cmd)); // Ensure that high register bits are cleared

                if (accessSize == 1 && minOffset == 0)
                {
                    // fails if offset >= len
                    build.cmp(temp, regOp(OP_B(inst)));
                    build.b(ConditionA64::UnsignedLessEqual, target);
                }
                else if (minOffset >= 0 && maxOffset <= int(AssemblyBuilderA64::kMaxImmediate))
                {
                    // fails if offset + size > len; we compute it as len - offset < size
                    RegisterA64 tempx = castReg(KindA64::x, temp);
                    build.sub(tempx, tempx, regOp(OP_B(inst))); // implicit uxtw
                    build.cmp(tempx, uint16_t(maxOffset));
                    build.b(ConditionA64::Less, target); // note: this is a signed 64-bit comparison so that out of bounds offset fails
                }
                else
                {
                    RegisterA64 tempx = castReg(KindA64::x, temp);
                    RegisterA64 temp2 = regs.allocTemp(KindA64::x);

                    // Get the base offset in 32 bits
                    if (minOffset >= 0)
                        build.add(castReg(KindA64::w, temp2), regOp(OP_B(inst)), uint16_t(minOffset));
                    else
                        build.sub(castReg(KindA64::w, temp2), regOp(OP_B(inst)), uint16_t(-minOffset));

                    // fail if uint64_t(uint32_t(offset + minOffset)) + accessSize > length
                    build.add(temp2, temp2, uint16_t(accessSize));
                    build.cmp(temp2, tempx);
                    build.b(ConditionA64::UnsignedGreater, target);
                }
            }
            else if (OP_B(inst).kind == IrOpKind::Constant)
            {
                int offset = intOp(OP_B(inst));

                // Constant folding can take care of it, but for safety we avoid overflow/underflow cases here
                if (offset < 0 || unsigned(offset) + unsigned(accessSize) >= unsigned(INT_MAX))
                {
                    build.b(target);
                }
                else if (offset + accessSize <= int(AssemblyBuilderA64::kMaxImmediate))
                {
                    build.cmp(temp, uint16_t(offset + accessSize));
                    build.b(ConditionA64::UnsignedLessEqual, target);
                }
                else
                {
                    RegisterA64 temp2 = regs.allocTemp(KindA64::w);
                    build.mov(temp2, offset + accessSize);
                    build.cmp(temp, temp2);
                    build.b(ConditionA64::UnsignedLessEqual, target);
                }
            }
            else
            {
                CODEGEN_ASSERT(!"Unsupported instruction form");
            }
            finalizeTargetLabel(OP_F(inst), fresh);
        }
        else
        {
            int accessSize = intOp(OP_C(inst));
            CODEGEN_ASSERT(accessSize > 0 && accessSize <= int(AssemblyBuilderA64::kMaxImmediate));

            Label fresh; // used when guard aborts execution or jumps to a VM exit
            Label& target = getTargetLabel(OP_D(inst), fresh);

            RegisterA64 temp = regs.allocTemp(KindA64::w);
            build.ldr(temp, mem(regOp(OP_A(inst)), offsetof(Buffer, len)));

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(OP_B(inst)).cmd)); // Ensure that high register bits are cleared

                if (accessSize == 1)
                {
                    // fails if offset >= len
                    build.cmp(temp, regOp(OP_B(inst)));
                    build.b(ConditionA64::UnsignedLessEqual, target);
                }
                else
                {
                    // fails if offset + size > len; we compute it as len - offset < size
                    RegisterA64 tempx = castReg(KindA64::x, temp);
                    build.sub(tempx, tempx, regOp(OP_B(inst))); // implicit uxtw
                    build.cmp(tempx, uint16_t(accessSize));
                    build.b(ConditionA64::Less, target); // note: this is a signed 64-bit comparison so that out of bounds offset fails
                }
            }
            else if (OP_B(inst).kind == IrOpKind::Constant)
            {
                int offset = intOp(OP_B(inst));

                // Constant folding can take care of it, but for safety we avoid overflow/underflow cases here
                if (offset < 0 || unsigned(offset) + unsigned(accessSize) >= unsigned(INT_MAX))
                {
                    build.b(target);
                }
                else if (offset + accessSize <= int(AssemblyBuilderA64::kMaxImmediate))
                {
                    build.cmp(temp, uint16_t(offset + accessSize));
                    build.b(ConditionA64::UnsignedLessEqual, target);
                }
                else
                {
                    RegisterA64 temp2 = regs.allocTemp(KindA64::w);
                    build.mov(temp2, offset + accessSize);
                    build.cmp(temp, temp2);
                    build.b(ConditionA64::UnsignedLessEqual, target);
                }
            }
            else
            {
                CODEGEN_ASSERT(!"Unsupported instruction form");
            }
            finalizeTargetLabel(OP_D(inst), fresh);
        }
        break;
    }
    case IrCmd::CHECK_USERDATA_TAG:
    {
        CODEGEN_ASSERT(unsigned(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate);

        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_C(inst), fresh);
        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.ldrb(temp, mem(regOp(OP_A(inst)), offsetof(Udata, tag)));
        build.cmp(temp, uint16_t(intOp(OP_B(inst))));
        build.b(ConditionA64::NotEqual, fail);
        finalizeTargetLabel(OP_C(inst), fresh);
        break;
    }
    case IrCmd::CHECK_CMP_NUM:
    {
        IrCondition cond = conditionOp(OP_C(inst));
        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_D(inst), fresh);

        RegisterA64 tempA = tempDouble(OP_A(inst));

        build.fcmp(tempA, tempDouble(OP_B(inst)));
        build.b(getConditionFP(getNegatedCondition(cond)), fail);

        finalizeTargetLabel(OP_D(inst), fresh);
        break;
    }
    case IrCmd::CHECK_CMP_INT:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_D(inst), fresh);

        if (cond == IrCondition::Equal && intOp(OP_B(inst)) == 0)
        {
            build.cbnz(regOp(OP_A(inst)), fail);
        }
        else if (cond == IrCondition::NotEqual && intOp(OP_B(inst)) == 0)
        {
            build.cbz(regOp(OP_A(inst)), fail);
        }
        else
        {
            RegisterA64 tempA = tempInt(OP_A(inst));

            if (OP_B(inst).kind == IrOpKind::Constant && unsigned(intOp(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
                build.cmp(tempA, uint16_t(intOp(OP_B(inst))));
            else
                build.cmp(tempA, tempInt(OP_B(inst)));

            build.b(getConditionInt(getNegatedCondition(cond)), fail);
        }
        finalizeTargetLabel(OP_D(inst), fresh);
        break;
    }
    case IrCmd::CHECK_CMP_INT64:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        Label fresh; // used when guard aborts execution or jumps to a VM exit
        Label& fail = getTargetLabel(OP_D(inst), fresh);

        if (cond == IrCondition::Equal && OP_B(inst).kind == IrOpKind::Constant && int64Op(OP_B(inst)) == 0)
        {
            build.cbnz(regOp(OP_A(inst)), fail);
        }
        else if (cond == IrCondition::NotEqual && OP_B(inst).kind == IrOpKind::Constant && int64Op(OP_B(inst)) == 0)
        {
            build.cbz(regOp(OP_A(inst)), fail);
        }
        else
        {
            RegisterA64 tempA = tempInt64(OP_A(inst));

            if (OP_B(inst).kind == IrOpKind::Constant && uint64_t(int64Op(OP_B(inst))) <= AssemblyBuilderA64::kMaxImmediate)
                build.cmp(tempA, uint16_t(int64Op(OP_B(inst))));
            else
                build.cmp(tempA, tempInt64(OP_B(inst)));

            build.b(getConditionInt64(getNegatedCondition(cond)), fail);
        }
        finalizeTargetLabel(OP_D(inst), fresh);
        break;
    }
    case IrCmd::INTERRUPT:
    {
        regs.spill(index);

        Label self;

        build.ldr(x0, mem(rGlobalState, offsetof(global_State, cb.interrupt)));
        build.cbnz(x0, self);

        Label next = build.setLabel();

        interruptHandlers.push_back({self, uintOp(OP_A(inst)), next});
        break;
    }
    case IrCmd::CHECK_GC:
    {
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::x);

        static_assert(offsetof(global_State, totalbytes) == offsetof(global_State, GCthreshold) + sizeof(global_State::GCthreshold));
        Label skip;
        build.ldp(temp1, temp2, mem(rGlobalState, offsetof(global_State, GCthreshold)));
        build.cmp(temp1, temp2);
        build.b(ConditionA64::UnsignedGreater, skip);

        size_t spills = regs.spill(index);

        build.mov(x0, rState);
        build.mov(w1, 1);
        build.ldr(x2, mem(rNativeContext, offsetof(NativeContext, luaC_step)));
        build.blr(x2);

        emitUpdateBase(build);

        regs.restore(spills); // need to restore before skip so that registers are in a consistent state

        build.setLabel(skip);
        break;
    }
    case IrCmd::BARRIER_OBJ:
    {
        RegisterA64 temp = regs.allocTemp(KindA64::x);

        Label skip;
        checkObjectBarrierConditions(regOp(OP_A(inst)), temp, noreg, OP_B(inst), OP_C(inst).kind == IrOpKind::Undef ? -1 : tagOp(OP_C(inst)), skip);

        RegisterA64 reg = regOp(OP_A(inst)); // note: we need to call regOp before spill so that we don't do redundant reloads
        size_t spills = regs.spill(index, {reg});
        build.mov(x1, reg);
        build.mov(x0, rState);
        build.ldr(x2, mem(rBase, vmRegOp(OP_B(inst)) * sizeof(TValue) + offsetof(TValue, value)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaC_barrierf)));
        build.blr(x3);

        regs.restore(spills); // need to restore before skip so that registers are in a consistent state

        // note: no emitUpdateBase necessary because luaC_ barriers do not reallocate stack
        build.setLabel(skip);
        break;
    }
    case IrCmd::BARRIER_TABLE_BACK:
    {
        Label skip;
        RegisterA64 temp = regs.allocTemp(KindA64::w);

        // isblack(obj2gco(t))
        build.ldrb(temp, mem(regOp(OP_A(inst)), offsetof(GCheader, marked)));
        build.tbz(temp, BLACKBIT, skip);

        RegisterA64 reg = regOp(OP_A(inst)); // note: we need to call regOp before spill so that we don't do redundant reloads
        size_t spills = regs.spill(index, {reg});
        build.mov(x1, reg);
        build.mov(x0, rState);
        build.add(x2, x1, uint16_t(offsetof(LuaTable, gclist)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaC_barrierback)));
        build.blr(x3);

        regs.restore(spills); // need to restore before skip so that registers are in a consistent state

        // note: no emitUpdateBase necessary because luaC_ barriers do not reallocate stack
        build.setLabel(skip);
        break;
    }
    case IrCmd::BARRIER_TABLE_FORWARD:
    {
        RegisterA64 temp = regs.allocTemp(KindA64::x);

        Label skip;
        checkObjectBarrierConditions(regOp(OP_A(inst)), temp, noreg, OP_B(inst), OP_C(inst).kind == IrOpKind::Undef ? -1 : tagOp(OP_C(inst)), skip);

        RegisterA64 reg = regOp(OP_A(inst)); // note: we need to call regOp before spill so that we don't do redundant reloads
        AddressA64 addr = tempAddr(OP_B(inst), offsetof(TValue, value));
        size_t spills = regs.spill(index, {reg});
        build.mov(x1, reg);
        build.mov(x0, rState);
        build.ldr(x2, addr);
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, luaC_barriertable)));
        build.blr(x3);

        regs.restore(spills); // need to restore before skip so that registers are in a consistent state

        // note: no emitUpdateBase necessary because luaC_ barriers do not reallocate stack
        build.setLabel(skip);
        break;
    }
    case IrCmd::SET_SAVEDPC:
    {
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::x);

        emitAddOffset(build, temp1, rCode, uintOp(OP_A(inst)) * sizeof(Instruction));
        build.ldr(temp2, mem(rState, offsetof(lua_State, ci)));
        build.str(temp1, mem(temp2, offsetof(CallInfo, savedpc)));
        break;
    }
    case IrCmd::CLOSE_UPVALS:
    {
        Label skip;
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::x);

        // L->openupval != 0
        build.ldr(temp1, mem(rState, offsetof(lua_State, openupval)));
        build.cbz(temp1, skip);

        // ra <= L->openupval->v
        build.ldr(temp1, mem(temp1, offsetof(UpVal, v)));
        build.add(temp2, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.cmp(temp2, temp1);
        build.b(ConditionA64::UnsignedGreater, skip);

        size_t spills = regs.spill(index, {temp2});
        build.mov(x1, temp2);
        build.mov(x0, rState);
        build.ldr(x2, mem(rNativeContext, offsetof(NativeContext, luaF_close)));
        build.blr(x2);

        regs.restore(spills); // need to restore before skip so that registers are in a consistent state

        build.setLabel(skip);
        break;
    }
    case IrCmd::CAPTURE:
        // no-op
        break;
    case IrCmd::SETLIST:
        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeSETLIST), uintOp(OP_A(inst)));
        break;
    case IrCmd::CALL:
        regs.spill(index);
        // argtop = (nparams == LUA_MULTRET) ? L->top : ra + 1 + nparams;
        if (intOp(OP_B(inst)) == LUA_MULTRET)
            build.ldr(x2, mem(rState, offsetof(lua_State, top)));
        else
            build.add(x2, rBase, uint16_t((vmRegOp(OP_A(inst)) + 1 + intOp(OP_B(inst))) * sizeof(TValue)));

        // callFallback(L, ra, argtop, nresults)
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.mov(w3, intOp(OP_C(inst)));
        build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, callFallback)));
        build.blr(x4);

        emitUpdateBase(build);

        // reentry with x0=closure (NULL implies C function; CALL_FALLBACK_YIELD will trigger exit)
        build.cbnz(x0, helpers.continueCall);
        break;
    case IrCmd::RETURN:
        regs.spill(index);

        if (function.variadic)
        {
            build.ldr(x1, mem(rState, offsetof(lua_State, ci)));
            build.ldr(x1, mem(x1, offsetof(CallInfo, func)));
        }
        else if (intOp(OP_B(inst)) != 1)
            build.sub(x1, rBase, uint16_t(sizeof(TValue))); // invariant: ci->func + 1 == ci->base for non-variadic frames

        if (intOp(OP_B(inst)) == 0)
        {
            build.mov(w2, 0);
            build.b(helpers.return_);
        }
        else if (intOp(OP_B(inst)) == 1 && !function.variadic)
        {
            // fast path: minimizes x1 adjustments
            // note that we skipped x1 computation for this specific case above
            build.ldr(q0, mem(rBase, vmRegOp(OP_A(inst)) * sizeof(TValue)));
            build.str(q0, mem(rBase, -int(sizeof(TValue))));
            build.mov(x1, rBase);
            build.mov(w2, 1);
            build.b(helpers.return_);
        }
        else if (intOp(OP_B(inst)) >= 1 && intOp(OP_B(inst)) <= 3)
        {
            for (int r = 0; r < intOp(OP_B(inst)); ++r)
            {
                build.ldr(q0, mem(rBase, (vmRegOp(OP_A(inst)) + r) * sizeof(TValue)));
                build.str(q0, mem(x1, sizeof(TValue), AddressKindA64::post));
            }
            build.mov(w2, intOp(OP_B(inst)));
            build.b(helpers.return_);
        }
        else
        {
            build.mov(w2, 0);

            // vali = ra
            build.add(x3, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));

            // valend = (n == LUA_MULTRET) ? L->top : ra + n
            if (intOp(OP_B(inst)) == LUA_MULTRET)
                build.ldr(x4, mem(rState, offsetof(lua_State, top)));
            else
                build.add(x4, rBase, uint16_t((vmRegOp(OP_A(inst)) + intOp(OP_B(inst))) * sizeof(TValue)));

            Label repeatValueLoop, exitValueLoop;

            if (intOp(OP_B(inst)) == LUA_MULTRET)
            {
                build.cmp(x3, x4);
                build.b(ConditionA64::CarrySet, exitValueLoop); // CarrySet == UnsignedGreaterEqual
            }

            build.setLabel(repeatValueLoop);
            build.ldr(q0, mem(x3, sizeof(TValue), AddressKindA64::post));
            build.str(q0, mem(x1, sizeof(TValue), AddressKindA64::post));
            build.add(w2, w2, uint16_t(1));
            build.cmp(x3, x4);
            build.b(ConditionA64::CarryClear, repeatValueLoop); // CarryClear == UnsignedLess

            build.setLabel(exitValueLoop);
            build.b(helpers.return_);
        }
        break;
    case IrCmd::FORGLOOP:
        // register layout: ra + 1 = table, ra + 2 = internal index, ra + 3 .. ra + aux = iteration variables
        regs.spill(index);
        // clear extra variables since we might have more than two
        if (intOp(OP_B(inst)) > 2)
        {
            CODEGEN_ASSERT(LUA_TNIL == 0);
            for (int i = 2; i < intOp(OP_B(inst)); ++i)
                build.str(wzr, mem(rBase, (vmRegOp(OP_A(inst)) + 3 + i) * sizeof(TValue) + offsetof(TValue, tt)));
        }
        // we use full iter fallback for now; in the future it could be worthwhile to accelerate array iteration here
        build.mov(x0, rState);
        build.ldr(x1, mem(rBase, (vmRegOp(OP_A(inst)) + 1) * sizeof(TValue) + offsetof(TValue, value.gc)));
        build.ldr(w2, mem(rBase, (vmRegOp(OP_A(inst)) + 2) * sizeof(TValue) + offsetof(TValue, value.p)));
        build.add(x3, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, forgLoopTableIter)));
        build.blr(x4);
        // note: no emitUpdateBase necessary because forgLoopTableIter does not reallocate stack
        build.cbnz(w0, labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    case IrCmd::FORGLOOP_FALLBACK:
        regs.spill(index);
        build.mov(x0, rState);
        build.mov(w1, vmRegOp(OP_A(inst)));
        build.mov(w2, intOp(OP_B(inst)));
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, forgLoopNonTableFallback)));
        build.blr(x3);
        emitUpdateBase(build);
        build.cbnz(w0, labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_B(inst)) * sizeof(TValue)));
        build.mov(w2, uintOp(OP_A(inst)) + 1);
        build.ldr(x3, mem(rNativeContext, offsetof(NativeContext, forgPrepXnextFallback)));
        build.blr(x3);
        // note: no emitUpdateBase necessary because forgLoopNonTableFallback does not reallocate stack
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    case IrCmd::COVERAGE:
    {
        RegisterA64 temp1 = regs.allocTemp(KindA64::x);
        RegisterA64 temp2 = regs.allocTemp(KindA64::w);
        RegisterA64 temp3 = regs.allocTemp(KindA64::w);

        build.mov(temp1, uintOp(OP_A(inst)) * sizeof(Instruction));
        build.ldr(temp2, mem(rCode, temp1));

        // increments E (high 24 bits); if the result overflows a 23-bit counter, high bit becomes 1
        // note: cmp can be eliminated with adds but we aren't concerned with code size for coverage
        build.add(temp3, temp2, uint16_t(256));
        build.cmp(temp3, uint16_t(0));
        build.csel(temp2, temp2, temp3, ConditionA64::Less);

        build.str(temp2, mem(rCode, temp1));
        break;
    }

        // Full instruction fallbacks
    case IrCmd::FALLBACK_GETGLOBAL:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmConst);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeGETGLOBAL), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_SETGLOBAL:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmConst);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeSETGLOBAL), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_GETTABLEKS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmConst);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeGETTABLEKS), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_SETTABLEKS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmConst);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeSETTABLEKS), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_NAMECALL:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmConst);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeNAMECALL), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_PREPVARARGS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executePREPVARARGS), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_GETVARARGS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::Constant);

        regs.spill(index);
        build.mov(x0, rState);

        if (intOp(OP_C(inst)) == LUA_MULTRET)
        {
            emitAddOffset(build, x1, rCode, uintOp(OP_A(inst)) * sizeof(Instruction));
            build.mov(x2, rBase);
            build.mov(w3, vmRegOp(OP_B(inst)));
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, executeGETVARARGSMultRet)));
            build.blr(x4);

            emitUpdateBase(build);
        }
        else
        {
            build.mov(x1, rBase);
            build.mov(w2, vmRegOp(OP_B(inst)));
            build.mov(w3, intOp(OP_C(inst)));
            build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, executeGETVARARGSConst)));
            build.blr(x4);

            // note: no emitUpdateBase necessary because executeGETVARARGSConst does not reallocate stack
        }
        break;
    case IrCmd::NEWCLOSURE:
    {
        RegisterA64 reg = regOp(OP_B(inst)); // note: we need to call regOp before spill so that we don't do redundant reloads

        regs.spill(index, {reg});
        build.mov(x2, reg);

        build.mov(x0, rState);
        build.mov(w1, uintOp(OP_A(inst)));

        build.ldr(x3, mem(rClosure, offsetof(Closure, l.p)));
        build.ldr(x3, mem(x3, offsetof(Proto, p)));

        unsigned protoIndex = uintOp(OP_C(inst)); // 0..32767
        int protoOffset = int(sizeof(Proto*) * protoIndex);

        if (protoIndex <= AddressA64::kMaxOffset)
        {
            build.ldr(x3, mem(x3, protoOffset));
        }
        else
        {
            build.mov(x4, protoOffset);
            build.ldr(x3, mem(x3, x4));
        }

        build.ldr(x4, mem(rNativeContext, offsetof(NativeContext, luaF_newLclosure)));
        build.blr(x4);

        inst.regA64 = regs.takeReg(x0, index);
        break;
    }
    case IrCmd::FALLBACK_DUPCLOSURE:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmConst);

        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeDUPCLOSURE), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_FORGPREP:
        regs.spill(index);
        emitFallback(build, offsetof(NativeContext, executeFORGPREP), uintOp(OP_A(inst)));
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;

    // Pseudo instructions
    case IrCmd::NOP:
    case IrCmd::SUBSTITUTE:
    case IrCmd::MARK_USED:
    case IrCmd::MARK_DEAD:
        CODEGEN_ASSERT(!"Pseudo instructions should not be lowered");
        break;

    case IrCmd::BITAND_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempInt64(OP_A(inst));
        RegisterA64 temp2 = tempInt64(OP_B(inst));
        build.and_(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::BITXOR_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempInt64(OP_A(inst));
        RegisterA64 temp2 = tempInt64(OP_B(inst));
        build.eor(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::BITOR_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 temp1 = tempInt64(OP_A(inst));
        RegisterA64 temp2 = tempInt64(OP_B(inst));
        build.orr(inst.regA64, temp1, temp2);
        break;
    }
    case IrCmd::BITNOT_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 temp = tempInt64(OP_A(inst));
        build.mvn_(inst.regA64, temp);
        break;
    }
    case IrCmd::BITLSHIFT_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 source = tempInt64(OP_A(inst));
        RegisterA64 amount = tempInt64(OP_B(inst));
        RegisterA64 temp = regs.allocTemp(KindA64::x);

        Label done, negative, outOfRange;

        // (amount + 63) > 126 = |amount| > 63
        build.add(temp, amount, uint16_t(63));
        build.cmp(temp, uint16_t(126));
        build.b(ConditionA64::UnsignedGreater, outOfRange);

        // check sign of amount
        build.cmp(amount, uint16_t(0));
        build.b(ConditionA64::Less, negative);

        // left shift
        build.lsl(inst.regA64, source, amount);
        build.b(done);

        // right shift by -amount
        build.setLabel(negative);
        build.neg(temp, amount);
        build.lsr(inst.regA64, source, temp);
        build.b(done);

        build.setLabel(outOfRange);
        build.mov(inst.regA64, 0);

        build.setLabel(done);
        break;
    }
    case IrCmd::BITRSHIFT_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 source = tempInt64(OP_A(inst));
        RegisterA64 amount = tempInt64(OP_B(inst));
        RegisterA64 temp = regs.allocTemp(KindA64::x);

        Label done, negative, outOfRange;

        // (amount + 63) > 126 = |amount| > 63
        build.add(temp, amount, uint16_t(63));
        build.cmp(temp, uint16_t(126));
        build.b(ConditionA64::UnsignedGreater, outOfRange);

        // check sign of amount
        build.cmp(amount, uint16_t(0));
        build.b(ConditionA64::Less, negative);

        // unsigned right shift
        build.lsr(inst.regA64, source, amount);
        build.b(done);

        // left shift by -amount
        build.setLabel(negative);
        build.neg(temp, amount);
        build.lsl(inst.regA64, source, temp);
        build.b(done);

        build.setLabel(outOfRange);
        build.mov(inst.regA64, 0);

        build.setLabel(done);
        break;
    }
    case IrCmd::BITARSHIFT_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 source = tempInt64(OP_A(inst));
        RegisterA64 amount = tempInt64(OP_B(inst));
        RegisterA64 temp = regs.allocTemp(KindA64::x);

        Label done, negative, outOfRangePositive, outOfRangeNegative;

        // amount > 63 (arithmetic right shift fills with sign)
        build.cmp(amount, uint16_t(63));
        build.b(ConditionA64::Greater, outOfRangePositive);

        // add 63, if < 0 then amount < -63
        build.add(temp, amount, uint16_t(63));
        build.cmp(temp, uint16_t(0));
        build.b(ConditionA64::Less, outOfRangeNegative);

        // check sign of amount
        build.cmp(amount, uint16_t(0));
        build.b(ConditionA64::Less, negative);

        // arithmetic right shift that sign extends
        build.asr(inst.regA64, source, amount);
        build.b(done);

        // left shift by -amount (unsigned)
        build.setLabel(negative);
        build.neg(temp, amount);
        build.lsl(inst.regA64, source, temp);
        build.b(done);

        // amount > 63 = sign-fill (n < 0 ? -1 : 0)
        build.setLabel(outOfRangePositive);
        build.asr(inst.regA64, source, uint8_t(63));
        build.b(done);

        // amount < -63 = result is 0
        build.setLabel(outOfRangeNegative);
        build.mov(inst.regA64, 0);

        build.setLabel(done);
        break;
    }
    case IrCmd::BITLROTATE_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_B(inst)}); // can't reuse A because it would be clobbered by neg
        RegisterA64 source = tempInt64(OP_A(inst));
        RegisterA64 amount = tempInt64(OP_B(inst));
        // left rotate = rotate by negative
        build.neg(inst.regA64, amount);
        build.ror(inst.regA64, source, inst.regA64);
        break;
    }
    case IrCmd::BITRROTATE_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst), OP_B(inst)});
        RegisterA64 source = tempInt64(OP_A(inst));
        RegisterA64 amount = tempInt64(OP_B(inst));
        build.ror(inst.regA64, source, amount);
        break;
    }
    case IrCmd::BITCOUNTLZ_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 temp = tempInt64(OP_A(inst));
        build.clz(inst.regA64, temp);
        break;
    }
    case IrCmd::BITCOUNTRZ_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 temp = tempInt64(OP_A(inst));
        build.rbit(inst.regA64, temp);
        build.clz(inst.regA64, inst.regA64);
        break;
    }
    case IrCmd::BYTESWAP_INT64:
    {
        inst.regA64 = regs.allocReuse(KindA64::x, index, {OP_A(inst)});
        RegisterA64 temp = tempInt64(OP_A(inst));
        build.rev(inst.regA64, temp);
        break;
    }
    case IrCmd::BITAND_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant &&
            AssemblyBuilderA64::isMaskSupported(unsigned(intOp(OP_B(inst)))))
            build.and_(inst.regA64, regOp(OP_A(inst)), unsigned(intOp(OP_B(inst))));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.and_(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITXOR_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant &&
            AssemblyBuilderA64::isMaskSupported(unsigned(intOp(OP_B(inst)))))
            build.eor(inst.regA64, regOp(OP_A(inst)), unsigned(intOp(OP_B(inst))));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.eor(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITOR_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant &&
            AssemblyBuilderA64::isMaskSupported(unsigned(intOp(OP_B(inst)))))
            build.orr(inst.regA64, regOp(OP_A(inst)), unsigned(intOp(OP_B(inst))));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.orr(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITNOT_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});
        RegisterA64 temp = tempUint(OP_A(inst));
        build.mvn_(inst.regA64, temp);
        break;
    }
    case IrCmd::BITLSHIFT_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant)
            build.lsl(inst.regA64, regOp(OP_A(inst)), uint8_t(unsigned(intOp(OP_B(inst))) & 31));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.lsl(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITRSHIFT_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant)
            build.lsr(inst.regA64, regOp(OP_A(inst)), uint8_t(unsigned(intOp(OP_B(inst))) & 31));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.lsr(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITARSHIFT_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant)
            build.asr(inst.regA64, regOp(OP_A(inst)), uint8_t(unsigned(intOp(OP_B(inst))) & 31));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.asr(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITLROTATE_UINT:
    {
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant)
        {
            inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});
            build.ror(inst.regA64, regOp(OP_A(inst)), uint8_t((32 - unsigned(intOp(OP_B(inst)))) & 31));
        }
        else
        {
            inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_B(inst)}); // can't reuse a because it would be clobbered by neg
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.neg(inst.regA64, temp2);
            build.ror(inst.regA64, temp1, inst.regA64);
        }
        break;
    }
    case IrCmd::BITRROTATE_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst), OP_B(inst)});
        if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Constant)
            build.ror(inst.regA64, regOp(OP_A(inst)), uint8_t(unsigned(intOp(OP_B(inst))) & 31));
        else
        {
            RegisterA64 temp1 = tempUint(OP_A(inst));
            RegisterA64 temp2 = tempUint(OP_B(inst));
            build.ror(inst.regA64, temp1, temp2);
        }
        break;
    }
    case IrCmd::BITCOUNTLZ_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});
        RegisterA64 temp = tempUint(OP_A(inst));
        build.clz(inst.regA64, temp);
        break;
    }
    case IrCmd::BITCOUNTRZ_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});
        RegisterA64 temp = tempUint(OP_A(inst));
        build.rbit(inst.regA64, temp);
        build.clz(inst.regA64, inst.regA64);
        break;
    }
    case IrCmd::BYTESWAP_UINT:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_A(inst)});
        RegisterA64 temp = tempUint(OP_A(inst));
        build.rev(inst.regA64, temp);
        break;
    }
    case IrCmd::INVOKE_LIBM:
    {
        if (HAS_OP_C(inst))
        {
            bool isInt = (OP_C(inst).kind == IrOpKind::Constant) ? constOp(OP_C(inst)).kind == IrConstKind::Int
                                                                 : getCmdValueKind(function.instOp(OP_C(inst)).cmd) == IrValueKind::Int;

            RegisterA64 temp1 = tempDouble(OP_B(inst));
            RegisterA64 temp2 = isInt ? tempInt(OP_C(inst)) : tempDouble(OP_C(inst));
            RegisterA64 temp3 = isInt ? noreg : regs.allocTemp(KindA64::d); // note: spill() frees all registers so we need to avoid alloc after spill
            regs.spill(index, {temp1, temp2});

            if (isInt)
            {
                build.fmov(d0, temp1);
                build.mov(w0, temp2);
            }
            else if (d0 != temp2)
            {
                build.fmov(d0, temp1);
                build.fmov(d1, temp2);
            }
            else
            {
                build.fmov(temp3, d0);
                build.fmov(d0, temp1);
                build.fmov(d1, temp3);
            }
        }
        else
        {
            RegisterA64 temp1 = tempDouble(OP_B(inst));
            regs.spill(index, {temp1});
            build.fmov(d0, temp1);
        }

        build.ldr(x1, mem(rNativeContext, getNativeContextOffset(uintOp(OP_A(inst)))));
        build.blr(x1);
        inst.regA64 = regs.takeReg(d0, index);
        break;
    }
    case IrCmd::GET_TYPE:
    {
        inst.regA64 = regs.allocReg(KindA64::x, index);

        CODEGEN_ASSERT(sizeof(TString*) == 8);

        if (OP_A(inst).kind == IrOpKind::Inst)
            build.add(inst.regA64, rGlobalState, regOp(OP_A(inst)), 3); // implicit uxtw
        else if (OP_A(inst).kind == IrOpKind::Constant)
            build.add(inst.regA64, rGlobalState, uint16_t(tagOp(OP_A(inst)) * 8));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");

        build.ldr(inst.regA64, mem(inst.regA64, offsetof(global_State, ttname)));
        break;
    }
    case IrCmd::GET_TYPEOF:
    {
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.ldr(x2, mem(rNativeContext, offsetof(NativeContext, luaT_objtypenamestr)));
        build.blr(x2);

        inst.regA64 = regs.takeReg(x0, index);
        break;
    }

    case IrCmd::FINDUPVAL:
    {
        regs.spill(index);
        build.mov(x0, rState);
        build.add(x1, rBase, uint16_t(vmRegOp(OP_A(inst)) * sizeof(TValue)));
        build.ldr(x2, mem(rNativeContext, offsetof(NativeContext, luaF_findupval)));
        build.blr(x2);

        inst.regA64 = regs.takeReg(x0, index);
        break;
    }

    case IrCmd::BUFFER_READI8:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_B(inst)});
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldrsb(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_READU8:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_B(inst)});
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldrb(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_WRITEI8:
    {
        RegisterA64 temp = tempInt(OP_C(inst));
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)));

        build.strb(temp, addr);
        break;
    }

    case IrCmd::BUFFER_READI16:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_B(inst)});
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldrsh(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_READU16:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_B(inst)});
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldrh(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_WRITEI16:
    {
        RegisterA64 temp = tempInt(OP_C(inst));
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)));

        build.strh(temp, addr);
        break;
    }

    case IrCmd::BUFFER_READI32:
    {
        inst.regA64 = regs.allocReuse(KindA64::w, index, {OP_B(inst)});
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldr(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_WRITEI32:
    {
        RegisterA64 temp = tempInt(OP_C(inst));
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)));

        build.str(temp, addr);
        break;
    }

    case IrCmd::BUFFER_READF32:
    {
        inst.regA64 = regs.allocReg(KindA64::s, index);
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldr(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_WRITEF32:
    {
        RegisterA64 temp = tempFloat(OP_C(inst));
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)));

        build.str(temp, addr);
        break;
    }

    case IrCmd::BUFFER_READF64:
    {
        inst.regA64 = regs.allocReg(KindA64::d, index);
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)));

        build.ldr(inst.regA64, addr);
        break;
    }

    case IrCmd::BUFFER_WRITEF64:
    {
        RegisterA64 temp = tempDouble(OP_C(inst));
        AddressA64 addr = tempAddrBuffer(OP_A(inst), OP_B(inst), !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)));

        build.str(temp, addr);
        break;
    }

        // To handle unsupported instructions, add "case IrCmd::OP" and make sure to set error = true!
    }

    valueTracker.afterInstLowering(inst, index);

    regs.currInstIdx = kInvalidInstIdx;

    regs.freeLastUseRegs(inst, index);
    regs.freeTempRegs();
}

void IrLoweringA64::startBlock(const IrBlock& curr)
{
    if (curr.startpc != kBlockNoStartPc)
        allocAndIncrementCounterAt(
            curr.kind == IrBlockKind::Fallback ? CodeGenCounter::FallbackBlockExecuted : CodeGenCounter::RegularBlockExecuted, curr.startpc
        );
}

void IrLoweringA64::finishBlock(const IrBlock& curr, const IrBlock& next)
{
    if (!regs.spills.empty())
    {
        // If we have spills remaining, we have to immediately lower the successor block
        for (uint32_t predIdx : predecessors(function.cfg, function.getBlockIndex(next)))
            CODEGEN_ASSERT(predIdx == function.getBlockIndex(curr) || function.blocks[predIdx].kind == IrBlockKind::Dead);

        // And the next block cannot be a join block in cfg
        CODEGEN_ASSERT(next.useCount == 1);
    }
}

void IrLoweringA64::finishFunction()
{
    if (build.logText)
        build.logAppend("; interrupt handlers\n");

    for (InterruptHandler& handler : interruptHandlers)
    {
        build.setLabel(handler.self);
        build.mov(x0, (handler.pcpos + 1) * sizeof(Instruction));
        build.adr(x1, handler.next);
        build.b(helpers.interrupt);
    }

    if (build.logText)
        build.logAppend("; exit handlers\n");

    for (ExitHandler& handler : exitHandlers)
    {
        if (handler.pcpos == kVmExitEntryGuardPc)
        {
            build.setLabel(handler.self);

            allocAndIncrementCounterAt(CodeGenCounter::VmExitTaken, ~0u);

            build.b(helpers.exitContinueVmClearNativeFlag);
        }
        else
        {
            build.setLabel(handler.self);

            allocAndIncrementCounterAt(CodeGenCounter::VmExitTaken, handler.pcpos);

            build.mov(x0, handler.pcpos * sizeof(Instruction));
            build.b(helpers.updatePcAndContinueInVm);
        }
    }

    // An undefined instruction is placed after the function to be used as an aborting jump offset
    function.endLocation = build.getLabelOffset(build.setLabel());
    build.udf();

    if (stats)
    {
        if (error)
            stats->loweringErrors++;

        if (regs.error)
            stats->regAllocErrors++;
    }
}

bool IrLoweringA64::hasError() const
{
    return error || regs.error;
}

bool IrLoweringA64::isFallthroughBlock(const IrBlock& target, const IrBlock& next)
{
    return target.start == next.start;
}

void IrLoweringA64::jumpOrFallthrough(IrBlock& target, const IrBlock& next)
{
    if (!isFallthroughBlock(target, next))
        build.b(target.label);
}

Label& IrLoweringA64::getTargetLabel(IrOp op, Label& fresh)
{
    if (op.kind == IrOpKind::Undef)
        return fresh;

    if (op.kind == IrOpKind::VmExit)
    {
        if (uint32_t* index = exitHandlerMap.find(vmExitOp(op)))
            return exitHandlers[*index].self;

        return fresh;
    }

    return labelOp(op);
}

void IrLoweringA64::finalizeTargetLabel(IrOp op, Label& fresh)
{
    if (op.kind == IrOpKind::Undef)
    {
        emitAbort(build, fresh);
    }
    else if (op.kind == IrOpKind::VmExit && fresh.id != 0)
    {
        exitHandlerMap[vmExitOp(op)] = uint32_t(exitHandlers.size());
        exitHandlers.push_back({fresh, vmExitOp(op)});
    }
}

void IrLoweringA64::checkSafeEnv(IrOp target, const IrBlock& next)
{
    Label fresh; // used when guard aborts execution or jumps to a VM exit
    RegisterA64 temp = regs.allocTemp(KindA64::x);
    RegisterA64 tempw = castReg(KindA64::w, temp);
    build.ldr(temp, mem(rClosure, offsetof(Closure, env)));
    build.ldrb(tempw, mem(temp, offsetof(LuaTable, safeenv)));
    build.cbz(tempw, getTargetLabel(target, fresh));
    finalizeTargetLabel(target, fresh);
}

void IrLoweringA64::allocAndIncrementCounterAt(CodeGenCounter kind, uint32_t pcpos)
{
    if (!function.recordCounters)
        return;

    if (build.logText)
        build.logAppend("; counter kind %u at pcpos %d\n", unsigned(kind), pcpos);

    // {uint32_t, uint32_t, uint64_t}
    function.extraNativeData.push_back(unsigned(kind));
    function.extraNativeData.push_back(pcpos);
    incrementCounterAt(function.extraNativeData.size());
    function.extraNativeData.push_back(0);
    function.extraNativeData.push_back(0);
}

void IrLoweringA64::incrementCounterAt(size_t offset)
{
    RegisterA64 temp1 = regs.allocTemp(KindA64::x);
    RegisterA64 temp2 = regs.allocTemp(KindA64::x);

    // Get counter slot
    build.ldr(temp1, mem(rClosure, offsetof(Closure, l.p)));
    build.ldr(temp1, mem(temp1, offsetof(Proto, execdata)));
    emitAddOffset(build, temp2, temp1, (unsigned(function.proto->sizecode) + offset) * 4);

    // Increment
    build.ldr(temp1, temp2);
    build.add(temp1, temp1, uint16_t(1));
    build.str(temp1, temp2);

    regs.freeTemp(temp1);
    regs.freeTemp(temp2);
}

void IrLoweringA64::checkObjectBarrierConditions(RegisterA64 object, RegisterA64 temp, RegisterA64 ra, IrOp raOp, int ratag, Label& skip)
{
    RegisterA64 tempw = castReg(KindA64::w, temp);

    // iscollectable(ra)
    if (ratag == -1 || !isGCO(ratag))
    {
        if (raOp.kind == IrOpKind::Inst)
        {
            build.umov_4s(tempw, ra, 3);
        }
        else
        {
            AddressA64 addr = tempAddr(raOp, offsetof(TValue, tt), temp);
            build.ldr(tempw, addr);
        }

        build.cmp(tempw, uint16_t(LUA_TSTRING));
        build.b(ConditionA64::Less, skip);
    }

    // isblack(obj2gco(o))
    build.ldrb(tempw, mem(object, offsetof(GCheader, marked)));
    build.tbz(tempw, BLACKBIT, skip);

    // iswhite(gcvalue(ra))
    if (raOp.kind == IrOpKind::Inst)
    {
        build.fmov(temp, castReg(KindA64::d, ra));
    }
    else
    {
        AddressA64 addr = tempAddr(raOp, offsetof(TValue, value), temp);
        build.ldr(temp, addr);
    }

    build.ldrb(tempw, mem(temp, offsetof(GCheader, marked)));
    build.tst(tempw, bit2mask(WHITE0BIT, WHITE1BIT));
    build.b(ConditionA64::Equal, skip); // Equal = Zero after tst
}

RegisterA64 IrLoweringA64::tempDouble(IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        return regOp(op);
    else if (op.kind == IrOpKind::Constant)
    {
        double val = doubleOp(op);

        if (AssemblyBuilderA64::isFmovSupportedFp64(val))
        {
            RegisterA64 temp = regs.allocTemp(KindA64::d);
            build.fmov(temp, val);
            return temp;
        }
        else
        {
            RegisterA64 temp1 = regs.allocTemp(KindA64::x);
            RegisterA64 temp2 = regs.allocTemp(KindA64::d);

            uint64_t vali = getDoubleBits(val);

            if ((vali << 16) == 0)
            {
                build.movz(temp1, uint16_t(vali >> 48), 48);
                build.fmov(temp2, temp1);
            }
            else if ((vali << 32) == 0)
            {
                build.movz(temp1, uint16_t(vali >> 48), 48);
                build.movk(temp1, uint16_t(vali >> 32), 32);
                build.fmov(temp2, temp1);
            }
            else
            {
                build.adr(temp1, val);
                build.ldr(temp2, temp1);
            }

            return temp2;
        }
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

RegisterA64 IrLoweringA64::tempFloat(IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        return regOp(op);
    else if (op.kind == IrOpKind::Constant)
    {
        float val = float(doubleOp(op));

        if (AssemblyBuilderA64::isFmovSupportedFp32(val))
        {
            RegisterA64 temp = regs.allocTemp(KindA64::s);
            build.fmov(temp, val);
            return temp;
        }
        else
        {
            RegisterA64 temp = regs.allocTemp(KindA64::s);

            uint32_t vali = getFloatBits(val);

            if ((vali & 0xffff) == 0)
            {
                RegisterA64 temp2 = regs.allocTemp(KindA64::w);

                build.movz(temp2, uint16_t(vali >> 16), 16);
                build.fmov(temp, temp2);
            }
            else
            {
                RegisterA64 temp2 = regs.allocTemp(KindA64::x);

                build.adr(temp2, val);
                build.ldr(temp, temp2);
            }

            return temp;
        }
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

RegisterA64 IrLoweringA64::tempInt(IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        return regOp(op);
    else if (op.kind == IrOpKind::Constant)
    {
        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.mov(temp, intOp(op));
        return temp;
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

RegisterA64 IrLoweringA64::tempInt64(IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        return regOp(op);
    else if (op.kind == IrOpKind::Constant)
    {
        RegisterA64 temp = regs.allocTemp(KindA64::x);
        uint64_t u = uint64_t(int64Op(op));

        // Count non-zero halfwords (movz path) vs non-0xFFFF halfwords (movn path)
        int movzCount = 0;
        int movnCount = 0;
        for (int shift = 0; shift < 64; shift += 16)
        {
            uint16_t hw = uint16_t(u >> shift);
            if (hw != 0)
                movzCount++;
            if (hw != 0xFFFF)
                movnCount++;
        }

        if (movzCount <= movnCount)
        {
            // movz path: emit movz for first non-zero halfword, movk for rest
            bool first = true;
            for (int shift = 0; shift < 64; shift += 16)
            {
                uint16_t hw = uint16_t(u >> shift);
                if (hw != 0)
                {
                    if (first)
                    {
                        build.movz(temp, hw, shift);
                        first = false;
                    }
                    else
                    {
                        build.movk(temp, hw, shift);
                    }
                }
            }

            if (first)
                build.movz(temp, 0);
        }
        else
        {
            // movn path: use movn for first non-0xFFFF halfword, movk for rest
            bool first = true;
            for (int shift = 0; shift < 64; shift += 16)
            {
                uint16_t hw = uint16_t(u >> shift);
                if (hw != 0xFFFF)
                {
                    if (first)
                    {
                        build.movn(temp, uint16_t(~hw), shift);
                        first = false;
                    }
                    else
                    {
                        build.movk(temp, hw, shift);
                    }
                }
            }

            if (first)
                build.movn(temp, 0);
        }

        return temp;
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

RegisterA64 IrLoweringA64::tempUint(IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        return regOp(op);
    else if (op.kind == IrOpKind::Constant)
    {
        RegisterA64 temp = regs.allocTemp(KindA64::w);
        build.mov(temp, unsigned(intOp(op)));
        return temp;
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

AddressA64 IrLoweringA64::tempAddr(IrOp op, int offset, RegisterA64 tempStorage)
{
    // This is needed to tighten the bounds checks in the VmConst case below
    CODEGEN_ASSERT(offset % 4 == 0);
    // Full encoded range is wider depending on the load size, but this assertion helps establish a smaller guaranteed working range [0..4096)
    CODEGEN_ASSERT(offset >= 0 && unsigned(offset / 4) <= AssemblyBuilderA64::kMaxImmediate);

    if (op.kind == IrOpKind::VmReg)
    {
        return mem(rBase, vmRegOp(op) * sizeof(TValue) + offset);
    }
    else if (op.kind == IrOpKind::VmConst)
    {
        size_t constantOffset = vmConstOp(op) * sizeof(TValue) + offset;

        // Note: cumulative offset is guaranteed to be divisible by 4; we can use that to expand the useful range that doesn't require temporaries
        if (constantOffset / 4 <= AddressA64::kMaxOffset)
            return mem(rConstants, int(constantOffset));

        RegisterA64 temp = tempStorage == noreg ? regs.allocTemp(KindA64::x) : tempStorage;
        CODEGEN_ASSERT(temp.kind == KindA64::x && "temp storage, when provided, must be an 'x' register");

        emitAddOffset(build, temp, rConstants, constantOffset);
        return temp;
    }
    // If we have a register, we assume it's a pointer to TValue
    else if (op.kind == IrOpKind::Inst)
    {
        CODEGEN_ASSERT(getCmdValueKind(function.instOp(op).cmd) == IrValueKind::Pointer);
        return mem(regOp(op), offset);
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

AddressA64 IrLoweringA64::tempAddrBuffer(IrOp bufferOp, IrOp indexOp, uint8_t tag)
{
    CODEGEN_ASSERT(tag == LUA_TUSERDATA || tag == LUA_TBUFFER);
    int dataOffset = tag == LUA_TBUFFER ? offsetof(Buffer, data) : offsetof(Udata, data);

    if (indexOp.kind == IrOpKind::Inst)
    {
        CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(indexOp).cmd));

        RegisterA64 temp = regs.allocTemp(KindA64::x);
        build.add(temp, regOp(bufferOp), regOp(indexOp)); // implicit uxtw
        return mem(temp, dataOffset);
    }
    else if (indexOp.kind == IrOpKind::Constant)
    {
        // Since the resulting address may be used to load any size, including 1 byte, from an unaligned offset, we are limited by unscaled
        // encoding
        if (unsigned(intOp(indexOp)) + dataOffset <= 255)
            return mem(regOp(bufferOp), int(intOp(indexOp) + dataOffset));

        // indexOp can only be negative in dead code (since offsets are checked); this avoids assertion in emitAddOffset
        if (intOp(indexOp) < 0)
            return mem(regOp(bufferOp), dataOffset);

        RegisterA64 temp = regs.allocTemp(KindA64::x);
        emitAddOffset(build, temp, regOp(bufferOp), size_t(intOp(indexOp)));
        return mem(temp, dataOffset);
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
        return noreg;
    }
}

RegisterA64 IrLoweringA64::regOp(IrOp op)
{
    IrInst& inst = function.instOp(op);

    if (inst.spilled || inst.needsReload)
        regs.restoreReg(inst);

    CODEGEN_ASSERT(inst.regA64 != noreg);
    return inst.regA64;
}

IrConst IrLoweringA64::constOp(IrOp op) const
{
    return function.constOp(op);
}

uint8_t IrLoweringA64::tagOp(IrOp op) const
{
    return function.tagOp(op);
}

int IrLoweringA64::intOp(IrOp op) const
{
    return function.intOp(op);
}

int64_t IrLoweringA64::int64Op(IrOp op) const
{
    return function.int64Op(op);
}

unsigned IrLoweringA64::uintOp(IrOp op) const
{
    return function.uintOp(op);
}

unsigned IrLoweringA64::importOp(IrOp op) const
{
    return function.importOp(op);
}

double IrLoweringA64::doubleOp(IrOp op) const
{
    return function.doubleOp(op);
}

IrBlock& IrLoweringA64::blockOp(IrOp op) const
{
    return function.blockOp(op);
}

Label& IrLoweringA64::labelOp(IrOp op) const
{
    return blockOp(op).label;
}

} // namespace A64
} // namespace CodeGen
} // namespace Luau
