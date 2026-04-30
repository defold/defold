// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrTranslation.h"

#include "Luau/Bytecode.h"
#include "Luau/CodeGenOptions.h"
#include "Luau/IrBuilder.h"
#include "Luau/IrUtils.h"

#include "IrTranslateBuiltins.h"

#include "lobject.h"
#include "lstate.h"
#include "ltm.h"

LUAU_FASTFLAG(LuauCodegenInteger2)
LUAU_FASTFLAG(LuauCodegenDseOnCondJump)
LUAU_FASTFLAG(LuauCodegenMarkDeadRegisters2)
LUAU_FASTFLAGVARIABLE(LuauCodegenIntegerFastcall2k)

namespace Luau
{
namespace CodeGen
{

// Helper to consistently define a switch to instruction fallback code
struct FallbackStreamScope
{
    FallbackStreamScope(IrBuilder& build, IrOp fallback, IrOp next)
        : build(build)
        , next(next)
    {
        CODEGEN_ASSERT(fallback.kind == IrOpKind::Block);
        CODEGEN_ASSERT(next.kind == IrOpKind::Block);

        build.inst(IrCmd::JUMP, next);
        build.beginBlock(fallback);
    }

    ~FallbackStreamScope()
    {
        build.beginBlock(next);
    }

    IrBuilder& build;
    IrOp next;
};

static IrOp getInitializedFallback(IrBuilder& build, IrOp& fallback, int pcpos)
{
    if (fallback.kind == IrOpKind::None)
        fallback = build.fallbackBlock(pcpos);

    return fallback;
}

static IrOp loadDoubleOrConstant(IrBuilder& build, IrOp arg)
{
    if (arg.kind == IrOpKind::VmConst)
    {
        CODEGEN_ASSERT(build.function.proto);
        TValue protok = build.function.proto->k[vmConstOp(arg)];

        CODEGEN_ASSERT(protok.tt == LUA_TNUMBER);

        return build.constDouble(protok.value.n);
    }

    return build.inst(IrCmd::LOAD_DOUBLE, arg);
}

void translateInstLoadNil(IrBuilder& build, const Instruction* pc)
{
    int ra = LUAU_INSN_A(*pc);

    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNIL));
}

void translateInstLoadB(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);

    build.inst(IrCmd::STORE_INT, build.vmReg(ra), build.constInt(LUAU_INSN_B(*pc)));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));

    if (int target = LUAU_INSN_C(*pc))
        build.inst(IrCmd::JUMP, build.blockAtInst(pcpos + 1 + target));
}

void translateInstLoadN(IrBuilder& build, const Instruction* pc)
{
    int ra = LUAU_INSN_A(*pc);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), build.constDouble(double(LUAU_INSN_D(*pc))));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
}

static void translateInstLoadConstant(IrBuilder& build, int ra, int k)
{
    TValue protok = build.function.proto->k[k];

    // Compiler only generates LOADK for source-level constants, so dynamic imports are not affected
    if (protok.tt == LUA_TNIL)
    {
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNIL));
    }
    else if (protok.tt == LUA_TBOOLEAN)
    {
        build.inst(IrCmd::STORE_INT, build.vmReg(ra), build.constInt(protok.value.b));
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));
    }
    else if (FFlag::LuauCodegenInteger2 && protok.tt == LUA_TINTEGER)
    {
        build.inst(IrCmd::STORE_INT64, build.vmReg(ra), build.constInt64(protok.value.l));
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));
    }
    else if (protok.tt == LUA_TNUMBER)
    {
        build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), build.constDouble(protok.value.n));
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
    }
    else
    {
        // Tag could be LUA_TSTRING or LUA_TVECTOR; for TSTRING we could generate LOAD_POINTER/STORE_POINTER/STORE_TAG, but it's not profitable;
        // however, it's still valuable to preserve the tag throughout the optimization pipeline to eliminate tag checks.
        IrOp load = build.inst(IrCmd::LOAD_TVALUE, build.vmConst(k), build.constInt(0), build.constTag(protok.tt));
        build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), load);
    }
}

void translateInstLoadK(IrBuilder& build, const Instruction* pc)
{
    translateInstLoadConstant(build, LUAU_INSN_A(*pc), LUAU_INSN_D(*pc));
}

void translateInstLoadKX(IrBuilder& build, const Instruction* pc)
{
    translateInstLoadConstant(build, LUAU_INSN_A(*pc), pc[1]);
}

void translateInstMove(IrBuilder& build, const Instruction* pc)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    IrOp load = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(rb));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), load);
}

void translateInstJump(IrBuilder& build, const Instruction* pc, int pcpos)
{
    build.inst(IrCmd::JUMP, build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc)));
}

void translateInstJumpBack(IrBuilder& build, const Instruction* pc, int pcpos)
{
    build.inst(IrCmd::INTERRUPT, build.constUint(pcpos));
    build.inst(IrCmd::JUMP, build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc)));
}

void translateInstJumpIf(IrBuilder& build, const Instruction* pc, int pcpos, bool not_)
{
    int ra = LUAU_INSN_A(*pc);

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 1);

    // TODO: falsy/truthy conditions should be deconstructed into more primitive operations
    if (not_)
        build.inst(IrCmd::JUMP_IF_FALSY, build.vmReg(ra), target, next);
    else
        build.inst(IrCmd::JUMP_IF_TRUTHY, build.vmReg(ra), target, next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpIfEq(IrBuilder& build, const Instruction* pc, int pcpos, bool not_)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = pc[1];

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 2);

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    // fast-path: number (when both operands are expected to be a number or are unknown)
    if (isExpectedOrUnknownBytecodeType(bcTypes.a, LBC_TYPE_NUMBER) && isExpectedOrUnknownBytecodeType(bcTypes.b, LBC_TYPE_NUMBER))
    {
        IrOp fallback = build.fallbackBlock(pcpos);

        IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
        build.inst(IrCmd::CHECK_TAG, ta, build.constTag(LUA_TNUMBER), fallback);

        IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
        build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TNUMBER), fallback);

        IrOp va = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra));
        IrOp vb = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(rb));

        build.inst(IrCmd::JUMP_CMP_NUM, va, vb, build.cond(IrCondition::NotEqual), not_ ? target : next, not_ ? next : target);

        build.beginBlock(fallback);
    }

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));

    IrOp result = build.inst(IrCmd::CMP_ANY, build.vmReg(ra), build.vmReg(rb), build.cond(IrCondition::Equal));
    build.inst(IrCmd::JUMP_CMP_INT, result, build.constInt(0), build.cond(IrCondition::Equal), not_ ? target : next, not_ ? next : target);

    build.beginBlock(next);
}

void translateInstJumpIfEqShortcut(IrBuilder& build, const Instruction* pc, int pcpos, bool not_)
{
    int rr = LUAU_INSN_A(pc[2]);

    int ra = LUAU_INSN_A(*pc);
    int rb = pc[1];

    IrOp next = build.blockAtInst(pcpos + 4);
    IrOp fallback;

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    // fast-path: number (when both operands are expected to be a number or are unknown)
    if (isExpectedOrUnknownBytecodeType(bcTypes.a, LBC_TYPE_NUMBER) && isExpectedOrUnknownBytecodeType(bcTypes.b, LBC_TYPE_NUMBER))
    {
        IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
        build.inst(
            IrCmd::CHECK_TAG,
            ta,
            build.constTag(LUA_TNUMBER),
            bcTypes.a == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
        );

        IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
        build.inst(
            IrCmd::CHECK_TAG,
            tb,
            build.constTag(LUA_TNUMBER),
            bcTypes.b == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
        );

        IrOp va = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra));
        IrOp vb = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(rb));

        IrOp result = build.inst(
            IrCmd::CMP_SPLIT_TVALUE,
            build.constTag(LUA_TNUMBER),
            build.constTag(LUA_TNUMBER),
            va,
            vb,
            build.cond(not_ ? IrCondition::NotEqual : IrCondition::Equal)
        );

        build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
        build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
        build.inst(IrCmd::JUMP, next);

        // If we don't need a fallback, we are done
        if (fallback.kind == IrOpKind::None)
            return;

        // Otherwise, start the fallback block
        // Note that if the number fast-path is not taken at all code that would have been in the fallback is actually the main path
        build.beginBlock(fallback);
    }
    else if (FFlag::LuauCodegenInteger2 && isExpectedOrUnknownBytecodeType(bcTypes.a, LBC_TYPE_INTEGER) &&
             isExpectedOrUnknownBytecodeType(bcTypes.b, LBC_TYPE_INTEGER))
    {
        IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
        build.inst(
            IrCmd::CHECK_TAG,
            ta,
            build.constTag(LUA_TINTEGER),
            bcTypes.a == LBC_TYPE_INTEGER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
        );

        IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
        build.inst(
            IrCmd::CHECK_TAG,
            tb,
            build.constTag(LUA_TINTEGER),
            bcTypes.b == LBC_TYPE_INTEGER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
        );

        IrOp va = build.inst(IrCmd::LOAD_INT64, build.vmReg(ra));
        IrOp vb = build.inst(IrCmd::LOAD_INT64, build.vmReg(rb));

        IrOp result = build.inst(
            IrCmd::CMP_SPLIT_TVALUE,
            build.constTag(LUA_TINTEGER),
            build.constTag(LUA_TINTEGER),
            va,
            vb,
            build.cond(not_ ? IrCondition::NotEqual : IrCondition::Equal)
        );

        build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
        build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
        build.inst(IrCmd::JUMP, next);

        // If we don't need a fallback, we are done
        if (fallback.kind == IrOpKind::None)
            return;

        // Otherwise, start the fallback block
        // Note that if the number fast-path is not taken at all code that would have been in the fallback is actually the main path
        build.beginBlock(fallback);
    }

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));

    IrOp result = build.inst(IrCmd::CMP_ANY, build.vmReg(ra), build.vmReg(rb), build.cond(IrCondition::Equal));

    // CMP_ANY doesn't support NotEqual, but we can compute !result as 1-result
    if (not_)
        result = build.inst(IrCmd::SUB_INT, build.constInt(1), result);

    build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
    build.inst(IrCmd::JUMP, next);
}

void translateInstJumpIfCond(IrBuilder& build, const Instruction* pc, int pcpos, IrCondition cond)
{
    CODEGEN_ASSERT(cond != IrCondition::Equal && cond != IrCondition::NotEqual);

    int ra = LUAU_INSN_A(*pc);
    int rb = pc[1];

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 2);

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    // fast-path: number (when both operands are expected to be a number or are unknown)
    if (isExpectedOrUnknownBytecodeType(bcTypes.a, LBC_TYPE_NUMBER) && isExpectedOrUnknownBytecodeType(bcTypes.b, LBC_TYPE_NUMBER))
    {
        IrOp fallback = build.fallbackBlock(pcpos);

        IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
        build.inst(IrCmd::CHECK_TAG, ta, build.constTag(LUA_TNUMBER), fallback);

        IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
        build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TNUMBER), fallback);

        IrOp va = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra));
        IrOp vb = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(rb));

        build.inst(IrCmd::JUMP_CMP_NUM, va, vb, build.cond(cond), target, next);

        build.beginBlock(fallback);
    }

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));

    bool reverse = false;

    if (cond == IrCondition::NotLessEqual)
    {
        reverse = true;
        cond = IrCondition::LessEqual;
    }
    else if (cond == IrCondition::NotLess)
    {
        reverse = true;
        cond = IrCondition::Less;
    }
    else if (cond == IrCondition::NotEqual)
    {
        reverse = true;
        cond = IrCondition::Equal;
    }

    IrOp result = build.inst(IrCmd::CMP_ANY, build.vmReg(ra), build.vmReg(rb), build.cond(cond));
    build.inst(IrCmd::JUMP_CMP_INT, result, build.constInt(0), build.cond(IrCondition::Equal), reverse ? target : next, reverse ? next : target);

    build.beginBlock(next);
}

void translateInstJumpX(IrBuilder& build, const Instruction* pc, int pcpos)
{
    build.inst(IrCmd::INTERRUPT, build.constUint(pcpos));
    build.inst(IrCmd::JUMP, build.blockAtInst(pcpos + 1 + LUAU_INSN_E(*pc)));
}

void translateInstJumpxEqNil(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    bool not_ = (pc[1] & 0x80000000) != 0;

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 2);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
    build.inst(IrCmd::JUMP_EQ_TAG, ta, build.constTag(LUA_TNIL), not_ ? next : target, not_ ? target : next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqNilShortcut(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int rr = LUAU_INSN_A(pc[2]);

    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp next = build.blockAtInst(pcpos + 4);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));

    IrOp result = build.inst(IrCmd::CMP_TAG, ta, build.constTag(LUA_TNIL), build.cond(not_ ? IrCondition::NotEqual : IrCondition::Equal));

    build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
    build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
    build.inst(IrCmd::JUMP, next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqB(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 2);
    IrOp checkValue = build.block(IrBlockKind::Internal);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));

    build.inst(IrCmd::JUMP_EQ_TAG, ta, build.constTag(LUA_TBOOLEAN), checkValue, not_ ? target : next);

    build.beginBlock(checkValue);
    IrOp va = build.inst(IrCmd::LOAD_INT, build.vmReg(ra));

    build.inst(
        IrCmd::JUMP_CMP_INT, va, build.constInt(LUAU_INSN_AUX_KB(aux)), build.cond(IrCondition::Equal), not_ ? next : target, not_ ? target : next
    );

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqBShortcut(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int rr = LUAU_INSN_A(pc[2]);

    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp next = build.blockAtInst(pcpos + 4);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
    IrOp va = build.inst(IrCmd::LOAD_INT, build.vmReg(ra));
    IrOp vb = build.constInt(LUAU_INSN_AUX_KB(aux));

    IrOp result =
        build.inst(IrCmd::CMP_SPLIT_TVALUE, ta, build.constTag(LUA_TBOOLEAN), va, vb, build.cond(not_ ? IrCondition::NotEqual : IrCondition::Equal));

    build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
    build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
    build.inst(IrCmd::JUMP, next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqN(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 2);
    IrOp checkValue = build.block(IrBlockKind::Internal);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));

    build.inst(IrCmd::JUMP_EQ_TAG, ta, build.constTag(LUA_TNUMBER), checkValue, not_ ? target : next);

    build.beginBlock(checkValue);
    IrOp va = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra));

    CODEGEN_ASSERT(build.function.proto);
    TValue protok = build.function.proto->k[LUAU_INSN_AUX_KV(aux)];

    CODEGEN_ASSERT(protok.tt == LUA_TNUMBER);
    IrOp vb = build.constDouble(protok.value.n);

    build.inst(IrCmd::JUMP_CMP_NUM, va, vb, build.cond(IrCondition::NotEqual), not_ ? target : next, not_ ? next : target);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqNShortcut(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int rr = LUAU_INSN_A(pc[2]);

    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp next = build.blockAtInst(pcpos + 4);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
    IrOp va = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra));

    CODEGEN_ASSERT(build.function.proto);
    TValue protok = build.function.proto->k[LUAU_INSN_AUX_KV(aux)];

    CODEGEN_ASSERT(protok.tt == LUA_TNUMBER);
    IrOp vb = build.constDouble(protok.value.n);

    IrOp result =
        build.inst(IrCmd::CMP_SPLIT_TVALUE, ta, build.constTag(LUA_TNUMBER), va, vb, build.cond(not_ ? IrCondition::NotEqual : IrCondition::Equal));

    build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
    build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
    build.inst(IrCmd::JUMP, next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqS(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp next = build.blockAtInst(pcpos + 2);
    IrOp checkValue = build.block(IrBlockKind::Internal);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
    build.inst(IrCmd::JUMP_EQ_TAG, ta, build.constTag(LUA_TSTRING), checkValue, not_ ? target : next);

    build.beginBlock(checkValue);
    IrOp va = build.inst(IrCmd::LOAD_POINTER, build.vmReg(ra));
    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmConst(LUAU_INSN_AUX_KV(aux)));

    build.inst(IrCmd::JUMP_EQ_POINTER, va, vb, not_ ? next : target, not_ ? target : next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

void translateInstJumpxEqSShortcut(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int rr = LUAU_INSN_A(pc[2]);

    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];
    bool not_ = LUAU_INSN_AUX_NOT(aux) != 0;

    IrOp next = build.blockAtInst(pcpos + 4);

    IrOp ta = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
    IrOp va = build.inst(IrCmd::LOAD_POINTER, build.vmReg(ra));
    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmConst(LUAU_INSN_AUX_KV(aux)));

    IrOp result =
        build.inst(IrCmd::CMP_SPLIT_TVALUE, ta, build.constTag(LUA_TSTRING), va, vb, build.cond(not_ ? IrCondition::NotEqual : IrCondition::Equal));

    build.inst(IrCmd::STORE_TAG, build.vmReg(rr), build.constTag(LUA_TBOOLEAN));
    build.inst(IrCmd::STORE_INT, build.vmReg(rr), result);
    build.inst(IrCmd::JUMP, next);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(next))
        build.beginBlock(next);
}

static void translateBinaryNumericFallbackIfRequired(IrBuilder& build, IrOp fallback, int ra, IrOp opb, IrOp opc, TMS tm, int pcpos)
{
    if (fallback.kind != IrOpKind::None)
    {
        IrOp next = build.blockAtInst(pcpos + 1);
        FallbackStreamScope scope(build, fallback, next);

        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::DO_ARITH, build.vmReg(ra), opb, opc, build.constInt(tm));
        build.inst(IrCmd::JUMP, next);
    }
}

static void translateInstBinaryNumeric(IrBuilder& build, int ra, int rb, int rc, IrOp opb, IrOp opc, int pcpos, TMS tm)
{
    IrOp fallback;

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    // Special fast-paths for vectors, matching the cases we have in VM
    if (bcTypes.a == LBC_TYPE_VECTOR && bcTypes.b == LBC_TYPE_VECTOR &&
        (tm == TM_ADD || tm == TM_SUB || tm == TM_MUL || tm == TM_DIV || tm == TM_IDIV))
    {
        build.inst(IrCmd::CHECK_TAG, build.inst(IrCmd::LOAD_TAG, build.vmReg(rb)), build.constTag(LUA_TVECTOR), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_TAG, build.inst(IrCmd::LOAD_TAG, build.vmReg(rc)), build.constTag(LUA_TVECTOR), build.vmExit(pcpos));

        IrOp vb = build.inst(IrCmd::LOAD_TVALUE, opb);
        IrOp vc = build.inst(IrCmd::LOAD_TVALUE, opc);
        IrOp result;

        switch (tm)
        {
        case TM_ADD:
            result = build.inst(IrCmd::ADD_VEC, vb, vc);
            break;
        case TM_SUB:
            result = build.inst(IrCmd::SUB_VEC, vb, vc);
            break;
        case TM_MUL:
            result = build.inst(IrCmd::MUL_VEC, vb, vc);
            break;
        case TM_DIV:
            result = build.inst(IrCmd::DIV_VEC, vb, vc);
            break;
        case TM_IDIV:
            result = build.inst(IrCmd::IDIV_VEC, vb, vc);
            break;
        default:
            CODEGEN_ASSERT(!"Unknown TM op");
        }

        result = build.inst(IrCmd::TAG_VECTOR, result);

        build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), result);
        return;
    }
    else if (!isUserdataBytecodeType(bcTypes.a) && bcTypes.b == LBC_TYPE_VECTOR && (tm == TM_MUL || tm == TM_DIV || tm == TM_IDIV))
    {
        if (rb != -1)
        {
            build.inst(
                IrCmd::CHECK_TAG,
                build.inst(IrCmd::LOAD_TAG, build.vmReg(rb)),
                build.constTag(LUA_TNUMBER),
                bcTypes.a == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
            );
        }

        build.inst(IrCmd::CHECK_TAG, build.inst(IrCmd::LOAD_TAG, build.vmReg(rc)), build.constTag(LUA_TVECTOR), build.vmExit(pcpos));

        IrOp vb = build.inst(IrCmd::FLOAT_TO_VEC, build.inst(IrCmd::NUM_TO_FLOAT, loadDoubleOrConstant(build, opb)));
        IrOp vc = build.inst(IrCmd::LOAD_TVALUE, opc);
        IrOp result;

        switch (tm)
        {
        case TM_MUL:
            result = build.inst(IrCmd::MUL_VEC, vb, vc);
            break;
        case TM_DIV:
            result = build.inst(IrCmd::DIV_VEC, vb, vc);
            break;
        case TM_IDIV:
            result = build.inst(IrCmd::IDIV_VEC, vb, vc);
            break;
        default:
            CODEGEN_ASSERT(!"Unknown TM op");
        }

        result = build.inst(IrCmd::TAG_VECTOR, result);

        build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), result);

        translateBinaryNumericFallbackIfRequired(build, fallback, ra, opb, opc, tm, pcpos);
        return;
    }
    else if (bcTypes.a == LBC_TYPE_VECTOR && !isUserdataBytecodeType(bcTypes.b) && (tm == TM_MUL || tm == TM_DIV || tm == TM_IDIV))
    {
        build.inst(IrCmd::CHECK_TAG, build.inst(IrCmd::LOAD_TAG, build.vmReg(rb)), build.constTag(LUA_TVECTOR), build.vmExit(pcpos));

        if (rc != -1)
        {
            build.inst(
                IrCmd::CHECK_TAG,
                build.inst(IrCmd::LOAD_TAG, build.vmReg(rc)),
                build.constTag(LUA_TNUMBER),
                bcTypes.b == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
            );
        }

        IrOp vb = build.inst(IrCmd::LOAD_TVALUE, opb);
        IrOp vc = build.inst(IrCmd::FLOAT_TO_VEC, build.inst(IrCmd::NUM_TO_FLOAT, loadDoubleOrConstant(build, opc)));
        IrOp result;

        switch (tm)
        {
        case TM_MUL:
            result = build.inst(IrCmd::MUL_VEC, vb, vc);
            break;
        case TM_DIV:
            result = build.inst(IrCmd::DIV_VEC, vb, vc);
            break;
        case TM_IDIV:
            result = build.inst(IrCmd::IDIV_VEC, vb, vc);
            break;
        default:
            CODEGEN_ASSERT(!"Unknown TM op");
        }

        result = build.inst(IrCmd::TAG_VECTOR, result);

        build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), result);

        translateBinaryNumericFallbackIfRequired(build, fallback, ra, opb, opc, tm, pcpos);
        return;
    }

    if (isUserdataBytecodeType(bcTypes.a) || isUserdataBytecodeType(bcTypes.b))
    {
        if (build.hostHooks.userdataMetamethod &&
            build.hostHooks.userdataMetamethod(build, bcTypes.a, bcTypes.b, ra, opb, opc, tmToHostMetamethod(tm), pcpos))
            return;

        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::DO_ARITH, build.vmReg(ra), opb, opc, build.constInt(tm));
        return;
    }

    // fast-path: number
    if (rb != -1)
    {
        IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
        build.inst(
            IrCmd::CHECK_TAG,
            tb,
            build.constTag(LUA_TNUMBER),
            bcTypes.a == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
        );
    }

    if (rc != -1 && rc != rb)
    {
        IrOp tc = build.inst(IrCmd::LOAD_TAG, build.vmReg(rc));
        build.inst(
            IrCmd::CHECK_TAG,
            tc,
            build.constTag(LUA_TNUMBER),
            bcTypes.b == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
        );
    }

    IrOp vb = loadDoubleOrConstant(build, opb);
    IrOp vc;
    IrOp result;

    if (opc.kind == IrOpKind::VmConst)
    {
        CODEGEN_ASSERT(build.function.proto);
        TValue protok = build.function.proto->k[vmConstOp(opc)];

        CODEGEN_ASSERT(protok.tt == LUA_TNUMBER);

        // VM has special cases for exponentiation with constants
        if (tm == TM_POW && protok.value.n == 0.5)
            result = build.inst(IrCmd::SQRT_NUM, vb);
        else if (tm == TM_POW && protok.value.n == 2.0)
            result = build.inst(IrCmd::MUL_NUM, vb, vb);
        else if (tm == TM_POW && protok.value.n == 3.0)
            result = build.inst(IrCmd::MUL_NUM, vb, build.inst(IrCmd::MUL_NUM, vb, vb));
        else
            vc = build.constDouble(protok.value.n);
    }
    else
    {
        vc = build.inst(IrCmd::LOAD_DOUBLE, opc);
    }

    if (result.kind == IrOpKind::None)
    {
        CODEGEN_ASSERT(vc.kind != IrOpKind::None);

        switch (tm)
        {
        case TM_ADD:
            result = build.inst(IrCmd::ADD_NUM, vb, vc);
            break;
        case TM_SUB:
            result = build.inst(IrCmd::SUB_NUM, vb, vc);
            break;
        case TM_MUL:
            result = build.inst(IrCmd::MUL_NUM, vb, vc);
            break;
        case TM_DIV:
            result = build.inst(IrCmd::DIV_NUM, vb, vc);
            break;
        case TM_IDIV:
            result = build.inst(IrCmd::IDIV_NUM, vb, vc);
            break;
        case TM_MOD:
            result = build.inst(IrCmd::MOD_NUM, vb, vc);
            break;
        case TM_POW:
            result = build.inst(IrCmd::INVOKE_LIBM, build.constUint(LBF_MATH_POW), vb, vc);
            break;
        default:
            CODEGEN_ASSERT(!"Unsupported binary op");
        }
    }

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), result);

    if (ra != rb && ra != rc) // TODO: optimization should handle second check, but we'll test this later
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    translateBinaryNumericFallbackIfRequired(build, fallback, ra, opb, opc, tm, pcpos);
}

void translateInstBinary(IrBuilder& build, const Instruction* pc, int pcpos, TMS tm)
{
    translateInstBinaryNumeric(
        build, LUAU_INSN_A(*pc), LUAU_INSN_B(*pc), LUAU_INSN_C(*pc), build.vmReg(LUAU_INSN_B(*pc)), build.vmReg(LUAU_INSN_C(*pc)), pcpos, tm
    );
}

void translateInstBinaryK(IrBuilder& build, const Instruction* pc, int pcpos, TMS tm)
{
    translateInstBinaryNumeric(
        build, LUAU_INSN_A(*pc), LUAU_INSN_B(*pc), -1, build.vmReg(LUAU_INSN_B(*pc)), build.vmConst(LUAU_INSN_C(*pc)), pcpos, tm
    );
}

void translateInstBinaryRK(IrBuilder& build, const Instruction* pc, int pcpos, TMS tm)
{
    translateInstBinaryNumeric(
        build, LUAU_INSN_A(*pc), -1, LUAU_INSN_C(*pc), build.vmConst(LUAU_INSN_B(*pc)), build.vmReg(LUAU_INSN_C(*pc)), pcpos, tm
    );
}

void translateInstNot(IrBuilder& build, const Instruction* pc)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    IrOp vb = build.inst(IrCmd::LOAD_INT, build.vmReg(rb));

    IrOp va = build.inst(IrCmd::NOT_ANY, tb, vb);

    build.inst(IrCmd::STORE_INT, build.vmReg(ra), va);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));
}

void translateInstMinus(IrBuilder& build, const Instruction* pc, int pcpos)
{
    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    if (bcTypes.a == LBC_TYPE_VECTOR)
    {
        build.inst(IrCmd::CHECK_TAG, build.inst(IrCmd::LOAD_TAG, build.vmReg(rb)), build.constTag(LUA_TVECTOR), build.vmExit(pcpos));

        IrOp vb = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(rb));
        IrOp va = build.inst(IrCmd::UNM_VEC, vb);
        va = build.inst(IrCmd::TAG_VECTOR, va);
        build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), va);
        return;
    }

    if (isUserdataBytecodeType(bcTypes.a))
    {
        if (build.hostHooks.userdataMetamethod &&
            build.hostHooks.userdataMetamethod(build, bcTypes.a, bcTypes.b, ra, build.vmReg(rb), {}, tmToHostMetamethod(TM_UNM), pcpos))
            return;

        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::DO_ARITH, build.vmReg(ra), build.vmReg(rb), build.vmReg(rb), build.constInt(TM_UNM));
        return;
    }

    IrOp fallback;

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    build.inst(
        IrCmd::CHECK_TAG,
        tb,
        build.constTag(LUA_TNUMBER),
        bcTypes.a == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : getInitializedFallback(build, fallback, pcpos)
    );

    // fast-path: number
    IrOp vb = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(rb));
    IrOp va = build.inst(IrCmd::UNM_NUM, vb);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), va);

    if (ra != rb)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    if (fallback.kind != IrOpKind::None)
    {
        IrOp next = build.blockAtInst(pcpos + 1);
        FallbackStreamScope scope(build, fallback, next);

        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::DO_ARITH, build.vmReg(ra), build.vmReg(rb), build.vmReg(rb), build.constInt(TM_UNM));
        build.inst(IrCmd::JUMP, next);
    }
}

void translateInstLength(IrBuilder& build, const Instruction* pc, int pcpos)
{
    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    if (isUserdataBytecodeType(bcTypes.a))
    {
        if (build.hostHooks.userdataMetamethod &&
            build.hostHooks.userdataMetamethod(build, bcTypes.a, bcTypes.b, ra, build.vmReg(rb), {}, tmToHostMetamethod(TM_LEN), pcpos))
            return;

        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::DO_LEN, build.vmReg(ra), build.vmReg(rb));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);

    // fast-path: table without __len
    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));
    build.inst(IrCmd::CHECK_NO_METATABLE, vb, fallback);

    IrOp va = build.inst(IrCmd::TABLE_LEN, vb);
    IrOp vai = build.inst(IrCmd::INT_TO_NUM, va);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), vai);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    IrOp next = build.blockAtInst(pcpos + 1);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::DO_LEN, build.vmReg(ra), build.vmReg(rb));
    build.inst(IrCmd::JUMP, next);
}

void translateInstNewTable(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int b = LUAU_INSN_B(*pc);
    uint32_t aux = pc[1];

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));

    IrOp va = build.inst(IrCmd::NEW_TABLE, build.constUint(aux), build.constUint(b == 0 ? 0 : 1 << (b - 1)));
    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra), va);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TTABLE));

    build.inst(IrCmd::CHECK_GC);
}

void translateInstDupTable(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int k = LUAU_INSN_D(*pc);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));

    IrOp table = build.inst(IrCmd::LOAD_POINTER, build.vmConst(k));
    IrOp va = build.inst(IrCmd::DUP_TABLE, table);
    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra), va);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TTABLE));

    build.inst(IrCmd::CHECK_GC);
}

void translateInstGetUpval(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int up = LUAU_INSN_B(*pc);

    IrOp value = build.inst(IrCmd::GET_UPVALUE, build.vmUpvalue(up));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), value);
}

void translateInstSetUpval(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int up = LUAU_INSN_B(*pc);

    IrOp value = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(ra));
    build.inst(IrCmd::SET_UPVALUE, build.vmUpvalue(up), value, build.undef());
}

void translateInstCloseUpvals(IrBuilder& build, const Instruction* pc)
{
    int ra = LUAU_INSN_A(*pc);

    build.inst(IrCmd::CLOSE_UPVALS, build.vmReg(ra));
}

IrOp translateFastCallN(IrBuilder& build, const Instruction* pc, int pcpos, bool customParams, int customParamCount, IrOp customArgs, IrOp customArg3)
{
    LuauOpcode opcode = LuauOpcode(LUAU_INSN_OP(*pc));
    int bfid = LUAU_INSN_A(*pc);
    int skip = LUAU_INSN_C(*pc);

    Instruction call = pc[skip + 1];
    CODEGEN_ASSERT(LUAU_INSN_OP(call) == LOP_CALL);
    int ra = LUAU_INSN_A(call);

    int nparams = customParams ? customParamCount : LUAU_INSN_B(call) - 1;
    int nresults = LUAU_INSN_C(call) - 1;
    int arg = customParams ? LUAU_INSN_B(*pc) : ra + 1;
    IrOp args = customParams ? customArgs : build.vmReg(ra + 2);

    IrOp builtinArgs = args;

    if (customArgs.kind == IrOpKind::VmConst)
    {
        CODEGEN_ASSERT(build.function.proto);
        TValue protok = build.function.proto->k[vmConstOp(customArgs)];

        if (protok.tt == LUA_TNUMBER)
            builtinArgs = build.constDouble(protok.value.n);
        else if (FFlag::LuauCodegenInteger2 && FFlag::LuauCodegenIntegerFastcall2k && protok.tt == LUA_TINTEGER)
            builtinArgs = build.constInt64(protok.value.l);
    }

    IrOp builtinArg3 = customParams ? customArg3 : build.vmReg(ra + 3);

    IrOp fallback = build.fallbackBlock(pcpos);

    // In unsafe environment, instead of retrying fastcall at 'pcpos' we side-exit directly to fallback sequence
    build.checkSafeEnv(pcpos + getOpLength(opcode));

    BuiltinImplResult br = translateBuiltin(
        build, LuauBuiltinFunction(bfid), ra, arg, builtinArgs, builtinArg3, nparams, nresults, fallback, pcpos + getOpLength(opcode)
    );

    if (br.type != BuiltinImplType::None)
    {
        CODEGEN_ASSERT(nparams != LUA_MULTRET && "builtins are not allowed to handle variadic arguments");

        if (nresults == LUA_MULTRET)
            build.inst(IrCmd::ADJUST_STACK_TO_REG, build.vmReg(ra), build.constInt(br.actualResultCount));
        else if (FFlag::LuauCodegenMarkDeadRegisters2)
            build.inst(IrCmd::MARK_DEAD, build.vmReg(ra + 1), build.constInt(-1));

        if (br.type != BuiltinImplType::UsesFallback)
        {
            // We ended up not using the fallback block, kill it
            build.function.blockOp(fallback).kind = IrBlockKind::Dead;

            return build.undef();
        }
    }
    else
    {
        IrOp arg3 = customParams ? customArg3 : build.undef();

        // TODO: we can skip saving pc for some well-behaved builtins which we didn't inline
        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + getOpLength(opcode)));

        IrOp res = build.inst(
            IrCmd::INVOKE_FASTCALL,
            build.constUint(bfid),
            build.vmReg(ra),
            build.vmReg(arg),
            args,
            arg3,
            build.constInt(nparams),
            build.constInt(nresults)
        );
        build.inst(IrCmd::CHECK_FASTCALL_RES, res, fallback);

        if (nresults == LUA_MULTRET)
            build.inst(IrCmd::ADJUST_STACK_TO_REG, build.vmReg(ra), res);
        else if (nparams == LUA_MULTRET)
            build.inst(IrCmd::ADJUST_STACK_TO_TOP);
    }

    return fallback;
}

// numeric for loop always ends with the computation of step that targets ra+1
// any conditionals would result in a split basic block, so we can recover the step constants by pattern matching the IR we generated for LOADN/K
static IrOp getLoopStepK(IrBuilder& build, int ra)
{
    IrBlock& active = build.function.blocks[build.activeBlockIdx];

    if (active.start + 2 <= build.function.instructions.size())
    {
        IrInst& sv = build.function.instructions[build.function.instructions.size() - 2];
        IrInst& st = build.function.instructions[build.function.instructions.size() - 1];

        // We currently expect to match IR generated from LOADN/LOADK so we match a particular sequence of opcodes
        // In the future this can be extended to cover opposite STORE order as well as STORE_SPLIT_TVALUE
        if (sv.cmd == IrCmd::STORE_DOUBLE && OP_A(sv).kind == IrOpKind::VmReg && OP_A(sv).index == ra + 1 && OP_B(sv).kind == IrOpKind::Constant &&
            st.cmd == IrCmd::STORE_TAG && OP_A(st).kind == IrOpKind::VmReg && OP_A(st).index == ra + 1 &&
            build.function.tagOp(OP_B(st)) == LUA_TNUMBER)
            return OP_B(sv);
    }

    return build.undef();
}

void beforeInstForNPrep(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);

    IrOp stepK = getLoopStepK(build, ra);
    build.numericLoopStack.push_back({stepK, pcpos + 1});
}

void afterInstForNLoop(IrBuilder& build, const Instruction* pc)
{
    CODEGEN_ASSERT(!build.numericLoopStack.empty());
    build.numericLoopStack.pop_back();
}

void translateInstForNPrep(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);

    IrOp loopStart = build.blockAtInst(pcpos + getOpLength(LuauOpcode(LUAU_INSN_OP(*pc))));
    IrOp loopExit = build.blockAtInst(getJumpTarget(*pc, pcpos));

    CODEGEN_ASSERT(!build.numericLoopStack.empty());
    IrOp stepK = build.numericLoopStack.back().step;

    // When loop parameters are not numbers, VM tries to perform type coercion from string and raises an exception if that fails
    // Performing that fallback in native code increases code size and complicates CFG, obscuring the values when they are constant
    // To avoid that overhead for an extremely rare case (that doesn't even typecheck), we exit to VM to handle it
    IrOp tagLimit = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 0));
    build.inst(IrCmd::CHECK_TAG, tagLimit, build.constTag(LUA_TNUMBER), build.vmExit(pcpos));
    IrOp tagIdx = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 2));
    build.inst(IrCmd::CHECK_TAG, tagIdx, build.constTag(LUA_TNUMBER), build.vmExit(pcpos));

    IrOp limit = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 0));
    IrOp idx = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 2));

    if (stepK.kind == IrOpKind::Undef)
    {
        IrOp tagStep = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 1));
        build.inst(IrCmd::CHECK_TAG, tagStep, build.constTag(LUA_TNUMBER), build.vmExit(pcpos));

        IrOp step = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 1));

        build.inst(IrCmd::JUMP_FORN_LOOP_COND, idx, limit, step, loopStart, loopExit);
    }
    else
    {
        double stepN = build.function.doubleOp(stepK);

        // Condition to start the loop: step > 0 ? idx <= limit : limit <= idx
        // We invert the condition so that loopStart is the fallthrough (false) label
        if (stepN > 0)
            build.inst(IrCmd::JUMP_CMP_NUM, idx, limit, build.cond(IrCondition::NotLessEqual), loopExit, loopStart);
        else
            build.inst(IrCmd::JUMP_CMP_NUM, limit, idx, build.cond(IrCondition::NotLessEqual), loopExit, loopStart);
    }

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(loopStart))
        build.beginBlock(loopStart);

    // VM places interrupt in FORNLOOP, but that creates a likely spill point for short loops that use loop index as INTERRUPT always spills
    // We place the interrupt at the beginning of the loop body instead; VM uses FORNLOOP because it doesn't want to waste an extra instruction.
    // Because loop block may not have been started yet (as it's started when lowering the first instruction!), we need to defer INTERRUPT placement.
    build.interruptRequested = true;
}

void translateInstForNLoop(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);

    int repeatJumpTarget = getJumpTarget(*pc, pcpos);
    IrOp loopRepeat = build.blockAtInst(repeatJumpTarget);
    IrOp loopExit = build.blockAtInst(pcpos + getOpLength(LuauOpcode(LUAU_INSN_OP(*pc))));

    CODEGEN_ASSERT(!build.numericLoopStack.empty());
    IrBuilder::LoopInfo loopInfo = build.numericLoopStack.back();

    // normally, the interrupt is placed at the beginning of the loop body by FORNPREP translation
    // however, there are rare cases where FORNLOOP might not jump directly to the first loop instruction
    // we detect this by checking the starting instruction of the loop body from loop information stack
    if (repeatJumpTarget != loopInfo.startpc)
        build.inst(IrCmd::INTERRUPT, build.constUint(pcpos));

    IrOp stepK = loopInfo.step;

    IrOp limit = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 0));
    IrOp step = stepK.kind == IrOpKind::Undef ? build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 1)) : stepK;

    IrOp idx = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 2));
    idx = build.inst(IrCmd::ADD_NUM, idx, step);
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra + 2), idx);

    if (stepK.kind == IrOpKind::Undef)
    {
        build.inst(IrCmd::JUMP_FORN_LOOP_COND, idx, limit, step, loopRepeat, loopExit);
    }
    else
    {
        double stepN = build.function.doubleOp(stepK);

        if (FFlag::LuauCodegenDseOnCondJump)
        {
            // Constant step optimization removes all the uses of the step register, but it has potential uses if a VM exit is taken
            build.inst(IrCmd::MARK_USED, build.vmReg(ra + 1), build.constInt(1));
        }

        // Condition to continue the loop: step > 0 ? idx <= limit : limit <= idx
        if (stepN > 0)
            build.inst(IrCmd::JUMP_CMP_NUM, idx, limit, build.cond(IrCondition::LessEqual), loopRepeat, loopExit);
        else
            build.inst(IrCmd::JUMP_CMP_NUM, limit, idx, build.cond(IrCondition::LessEqual), loopRepeat, loopExit);
    }

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(loopExit))
        build.beginBlock(loopExit);
}

void translateInstForGPrepNext(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp fallback = build.fallbackBlock(pcpos);

    // fast-path: pairs/next
    build.checkSafeEnv(pcpos);

    IrOp tagB = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 1));
    build.inst(IrCmd::CHECK_TAG, tagB, build.constTag(LUA_TTABLE), fallback);
    IrOp tagC = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 2));
    build.inst(IrCmd::CHECK_TAG, tagC, build.constTag(LUA_TNIL), fallback);

    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNIL));

    // setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(0)), LU_TAG_ITERATOR);
    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra + 2), build.constInt(0));
    build.inst(IrCmd::STORE_EXTRA, build.vmReg(ra + 2), build.constInt(LU_TAG_ITERATOR));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra + 2), build.constTag(LUA_TLIGHTUSERDATA));

    build.inst(IrCmd::JUMP, target);

    build.beginBlock(fallback);
    build.inst(IrCmd::FORGPREP_XNEXT_FALLBACK, build.constUint(pcpos), build.vmReg(ra), target);
}

void translateInstForGPrepInext(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);

    IrOp target = build.blockAtInst(pcpos + 1 + LUAU_INSN_D(*pc));
    IrOp fallback = build.fallbackBlock(pcpos);
    IrOp finish = build.block(IrBlockKind::Internal);

    // fast-path: ipairs/inext
    build.checkSafeEnv(pcpos);

    IrOp tagB = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 1));
    build.inst(IrCmd::CHECK_TAG, tagB, build.constTag(LUA_TTABLE), fallback);
    IrOp tagC = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra + 2));
    build.inst(IrCmd::CHECK_TAG, tagC, build.constTag(LUA_TNUMBER), fallback);

    IrOp numC = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(ra + 2));
    build.inst(IrCmd::JUMP_CMP_NUM, numC, build.constDouble(0.0), build.cond(IrCondition::NotEqual), fallback, finish);

    build.beginBlock(finish);

    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNIL));

    // setpvalue(ra + 2, reinterpret_cast<void*>(uintptr_t(0)), LU_TAG_ITERATOR);
    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra + 2), build.constInt(0));
    build.inst(IrCmd::STORE_EXTRA, build.vmReg(ra + 2), build.constInt(LU_TAG_ITERATOR));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra + 2), build.constTag(LUA_TLIGHTUSERDATA));

    build.inst(IrCmd::JUMP, target);

    build.beginBlock(fallback);
    build.inst(IrCmd::FORGPREP_XNEXT_FALLBACK, build.constUint(pcpos), build.vmReg(ra), target);
}

void translateInstForGLoopIpairs(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    CODEGEN_ASSERT(int(pc[1]) < 0);

    IrOp loopRepeat = build.blockAtInst(getJumpTarget(*pc, pcpos));
    IrOp loopExit = build.blockAtInst(pcpos + getOpLength(LuauOpcode(LUAU_INSN_OP(*pc))));
    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp hasElem = build.block(IrBlockKind::Internal);

    build.inst(IrCmd::INTERRUPT, build.constUint(pcpos));

    // fast-path: builtin table iteration
    IrOp tagA = build.inst(IrCmd::LOAD_TAG, build.vmReg(ra));
    build.inst(IrCmd::CHECK_TAG, tagA, build.constTag(LUA_TNIL), fallback);

    IrOp table = build.inst(IrCmd::LOAD_POINTER, build.vmReg(ra + 1));
    IrOp index = build.inst(IrCmd::LOAD_INT, build.vmReg(ra + 2));

    IrOp elemPtr = build.inst(IrCmd::GET_ARR_ADDR, table, index);

    // Terminate if array has ended
    build.inst(IrCmd::CHECK_ARRAY_SIZE, table, index, loopExit);

    // Terminate if element is nil
    IrOp elemTag = build.inst(IrCmd::LOAD_TAG, elemPtr);
    build.inst(IrCmd::JUMP_EQ_TAG, elemTag, build.constTag(LUA_TNIL), loopExit, hasElem);
    build.beginBlock(hasElem);

    IrOp nextIndex = build.inst(IrCmd::ADD_INT, index, build.constInt(1));

    // We update only a dword part of the userdata pointer that's reused in loop iteration as an index
    // Upper bits start and remain to be 0
    build.inst(IrCmd::STORE_INT, build.vmReg(ra + 2), nextIndex);
    // Tag should already be set to lightuserdata

    // setnvalue(ra + 3, double(index + 1));
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra + 3), build.inst(IrCmd::INT_TO_NUM, nextIndex));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra + 3), build.constTag(LUA_TNUMBER));

    // setobj2s(L, ra + 4, e);
    IrOp elemTV = build.inst(IrCmd::LOAD_TVALUE, elemPtr);
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra + 4), elemTV);

    build.inst(IrCmd::JUMP, loopRepeat);

    build.beginBlock(fallback);
    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::FORGLOOP_FALLBACK, build.vmReg(ra), build.constInt(int(pc[1])), loopRepeat, loopExit);

    // Fallthrough in original bytecode is implicit, so we start next internal block here
    if (build.isInternalBlock(loopExit))
        build.beginBlock(loopExit);
}

void translateInstGetTableN(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);
    int c = LUAU_INSN_C(*pc);

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    if (isUserdataBytecodeType(bcTypes.a))
    {
        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::GET_TABLE, build.vmReg(ra), build.vmReg(rb), build.constUint(c + 1));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);

    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));

    build.inst(IrCmd::CHECK_ARRAY_SIZE, vb, build.constInt(c), fallback);
    build.inst(IrCmd::CHECK_NO_METATABLE, vb, fallback);

    IrOp arrEl = build.inst(IrCmd::GET_ARR_ADDR, vb, build.constInt(0));

    IrOp arrElTval = build.inst(IrCmd::LOAD_TVALUE, arrEl, build.constInt(c * sizeof(TValue)));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), arrElTval);

    IrOp next = build.blockAtInst(pcpos + 1);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::GET_TABLE, build.vmReg(ra), build.vmReg(rb), build.constUint(c + 1));
    build.inst(IrCmd::JUMP, next);
}

void translateInstSetTableN(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);
    int c = LUAU_INSN_C(*pc);

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    if (isUserdataBytecodeType(bcTypes.a))
    {
        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::SET_TABLE, build.vmReg(ra), build.vmReg(rb), build.constUint(c + 1));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);

    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));

    build.inst(IrCmd::CHECK_ARRAY_SIZE, vb, build.constInt(c), fallback);
    build.inst(IrCmd::CHECK_NO_METATABLE, vb, fallback);
    build.inst(IrCmd::CHECK_READONLY, vb, fallback);

    IrOp arrEl = build.inst(IrCmd::GET_ARR_ADDR, vb, build.constInt(0));

    IrOp tva = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(ra));
    build.inst(IrCmd::STORE_TVALUE, arrEl, tva, build.constInt(c * sizeof(TValue)));

    build.inst(IrCmd::BARRIER_TABLE_FORWARD, vb, build.vmReg(ra), build.undef());

    IrOp next = build.blockAtInst(pcpos + 1);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::SET_TABLE, build.vmReg(ra), build.vmReg(rb), build.constUint(c + 1));
    build.inst(IrCmd::JUMP, next);
}

void translateInstGetTable(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);
    int rc = LUAU_INSN_C(*pc);

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    if (isUserdataBytecodeType(bcTypes.a) || bcTypes.b == LBC_TYPE_STRING)
    {
        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::GET_TABLE, build.vmReg(ra), build.vmReg(rb), build.vmReg(rc));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);
    IrOp tc = build.inst(IrCmd::LOAD_TAG, build.vmReg(rc));
    build.inst(IrCmd::CHECK_TAG, tc, build.constTag(LUA_TNUMBER), bcTypes.b == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : fallback);

    // fast-path: table with a number index
    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));
    IrOp vc = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(rc));

    IrOp index = build.inst(IrCmd::TRY_NUM_TO_INDEX, vc, fallback);

    index = build.inst(IrCmd::SUB_INT, index, build.constInt(1));

    build.inst(IrCmd::CHECK_ARRAY_SIZE, vb, index, fallback);
    build.inst(IrCmd::CHECK_NO_METATABLE, vb, fallback);

    IrOp arrEl = build.inst(IrCmd::GET_ARR_ADDR, vb, index);

    IrOp arrElTval = build.inst(IrCmd::LOAD_TVALUE, arrEl);
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), arrElTval);

    IrOp next = build.blockAtInst(pcpos + 1);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::GET_TABLE, build.vmReg(ra), build.vmReg(rb), build.vmReg(rc));
    build.inst(IrCmd::JUMP, next);
}

void translateInstSetTable(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);
    int rc = LUAU_INSN_C(*pc);

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    if (isUserdataBytecodeType(bcTypes.a) || bcTypes.b == LBC_TYPE_STRING)
    {
        build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
        build.inst(IrCmd::SET_TABLE, build.vmReg(ra), build.vmReg(rb), build.vmReg(rc));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));
    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);
    IrOp tc = build.inst(IrCmd::LOAD_TAG, build.vmReg(rc));
    build.inst(IrCmd::CHECK_TAG, tc, build.constTag(LUA_TNUMBER), bcTypes.b == LBC_TYPE_NUMBER ? build.vmExit(pcpos) : fallback);

    // fast-path: table with a number index
    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));
    IrOp vc = build.inst(IrCmd::LOAD_DOUBLE, build.vmReg(rc));

    IrOp index = build.inst(IrCmd::TRY_NUM_TO_INDEX, vc, fallback);

    index = build.inst(IrCmd::SUB_INT, index, build.constInt(1));

    build.inst(IrCmd::CHECK_ARRAY_SIZE, vb, index, fallback);
    build.inst(IrCmd::CHECK_NO_METATABLE, vb, fallback);
    build.inst(IrCmd::CHECK_READONLY, vb, fallback);

    IrOp arrEl = build.inst(IrCmd::GET_ARR_ADDR, vb, index);

    IrOp tva = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(ra));
    build.inst(IrCmd::STORE_TVALUE, arrEl, tva);

    build.inst(IrCmd::BARRIER_TABLE_FORWARD, vb, build.vmReg(ra), build.undef());

    IrOp next = build.blockAtInst(pcpos + 1);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::SET_TABLE, build.vmReg(ra), build.vmReg(rb), build.vmReg(rc));
    build.inst(IrCmd::JUMP, next);
}

void translateInstGetImport(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int k = LUAU_INSN_D(*pc);
    uint32_t aux = pc[1];

    build.checkSafeEnv(pcpos);

    build.inst(IrCmd::GET_CACHED_IMPORT, build.vmReg(ra), build.vmConst(k), build.constImport(aux), build.constUint(pcpos + 1));
}

void translateInstGetTableKS(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    // TODO: we keep the table access lowering for speculative userdata access instructions until a later date
    uint32_t aux = LUAU_INSN_OP(*pc) == LOP_GETUDATAKS ? LUAU_INSN_AUX_KV16(pc[1]) : pc[1];

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));

    if (bcTypes.a == LBC_TYPE_VECTOR)
    {
        build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TVECTOR), build.vmExit(pcpos));

        TString* str = gco2ts(build.function.proto->k[aux].value.gc);
        const char* field = getstr(str);

        if (str->len == 1 && (*field == 'X' || *field == 'x'))
        {
            IrOp value = build.inst(IrCmd::LOAD_FLOAT, build.vmReg(rb), build.constInt(0));

            value = build.inst(IrCmd::FLOAT_TO_NUM, value);

            build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);
            build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
        }
        else if (str->len == 1 && (*field == 'Y' || *field == 'y'))
        {
            IrOp value = build.inst(IrCmd::LOAD_FLOAT, build.vmReg(rb), build.constInt(4));

            value = build.inst(IrCmd::FLOAT_TO_NUM, value);

            build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);
            build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
        }
        else if (str->len == 1 && (*field == 'Z' || *field == 'z'))
        {
            IrOp value = build.inst(IrCmd::LOAD_FLOAT, build.vmReg(rb), build.constInt(8));

            value = build.inst(IrCmd::FLOAT_TO_NUM, value);

            build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);
            build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
        }
        else
        {
            if (build.hostHooks.vectorAccess && build.hostHooks.vectorAccess(build, field, str->len, ra, rb, pcpos))
                return;

            build.inst(IrCmd::FALLBACK_GETTABLEKS, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
        }

        return;
    }

    if (isUserdataBytecodeType(bcTypes.a))
    {
        build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TUSERDATA), build.vmExit(pcpos));

        if (build.hostHooks.userdataAccess)
        {
            TString* str = gco2ts(build.function.proto->k[aux].value.gc);
            const char* field = getstr(str);

            if (build.hostHooks.userdataAccess(build, bcTypes.a, field, str->len, ra, rb, pcpos))
                return;
        }

        build.inst(IrCmd::FALLBACK_GETTABLEKS, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);

    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));

    IrOp addrSlotEl = build.inst(IrCmd::GET_SLOT_NODE_ADDR, vb, build.constUint(pcpos), build.vmConst(aux));

    build.inst(IrCmd::CHECK_SLOT_MATCH, addrSlotEl, build.vmConst(aux), fallback);

    IrOp tvn = build.inst(IrCmd::LOAD_TVALUE, addrSlotEl, build.constInt(offsetof(LuaNode, val)));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), tvn);

    IrOp next = build.blockAtInst(pcpos + 2);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::FALLBACK_GETTABLEKS, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
    build.inst(IrCmd::JUMP, next);
}

void translateInstSetTableKS(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    // TODO: we keep the table access lowering for speculative userdata access instructions until a later date
    uint32_t aux = LUAU_INSN_OP(*pc) == LOP_SETUDATAKS ? LUAU_INSN_AUX_KV16(pc[1]) : pc[1];

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    IrOp tb = build.inst(IrCmd::LOAD_TAG, build.vmReg(rb));

    if (isUserdataBytecodeType(bcTypes.a))
    {
        build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TUSERDATA), build.vmExit(pcpos));

        build.inst(IrCmd::FALLBACK_SETTABLEKS, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
        return;
    }

    IrOp fallback = build.fallbackBlock(pcpos);

    build.inst(IrCmd::CHECK_TAG, tb, build.constTag(LUA_TTABLE), bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);

    IrOp vb = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));

    IrOp addrSlotEl = build.inst(IrCmd::GET_SLOT_NODE_ADDR, vb, build.constUint(pcpos), build.vmConst(aux));

    build.inst(IrCmd::CHECK_SLOT_MATCH, addrSlotEl, build.vmConst(aux), fallback);
    build.inst(IrCmd::CHECK_READONLY, vb, fallback);

    IrOp tva = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(ra));
    build.inst(IrCmd::STORE_TVALUE, addrSlotEl, tva, build.constInt(offsetof(LuaNode, val)));

    build.inst(IrCmd::BARRIER_TABLE_FORWARD, vb, build.vmReg(ra), build.undef());

    IrOp next = build.blockAtInst(pcpos + 2);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::FALLBACK_SETTABLEKS, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
    build.inst(IrCmd::JUMP, next);
}

void translateInstGetGlobal(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp env = build.inst(IrCmd::LOAD_ENV);
    IrOp addrSlotEl = build.inst(IrCmd::GET_SLOT_NODE_ADDR, env, build.constUint(pcpos), build.vmConst(aux));

    build.inst(IrCmd::CHECK_SLOT_MATCH, addrSlotEl, build.vmConst(aux), fallback);

    IrOp tvn = build.inst(IrCmd::LOAD_TVALUE, addrSlotEl, build.constInt(offsetof(LuaNode, val)));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), tvn);

    IrOp next = build.blockAtInst(pcpos + 2);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::FALLBACK_GETGLOBAL, build.constUint(pcpos), build.vmReg(ra), build.vmConst(aux));
    build.inst(IrCmd::JUMP, next);
}

void translateInstSetGlobal(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    uint32_t aux = pc[1];

    IrOp fallback = build.fallbackBlock(pcpos);

    IrOp env = build.inst(IrCmd::LOAD_ENV);
    IrOp addrSlotEl = build.inst(IrCmd::GET_SLOT_NODE_ADDR, env, build.constUint(pcpos), build.vmConst(aux));

    build.inst(IrCmd::CHECK_SLOT_MATCH, addrSlotEl, build.vmConst(aux), fallback);
    build.inst(IrCmd::CHECK_READONLY, env, fallback);

    IrOp tva = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(ra));
    build.inst(IrCmd::STORE_TVALUE, addrSlotEl, tva, build.constInt(offsetof(LuaNode, val)));

    build.inst(IrCmd::BARRIER_TABLE_FORWARD, env, build.vmReg(ra), build.undef());

    IrOp next = build.blockAtInst(pcpos + 2);
    FallbackStreamScope scope(build, fallback, next);

    build.inst(IrCmd::FALLBACK_SETGLOBAL, build.constUint(pcpos), build.vmReg(ra), build.vmConst(aux));
    build.inst(IrCmd::JUMP, next);
}

void translateInstConcat(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);
    int rc = LUAU_INSN_C(*pc);

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));
    build.inst(IrCmd::CONCAT, build.vmReg(rb), build.constUint(rc - rb + 1));

    IrOp tvb = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(rb));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), tvb);

    build.inst(IrCmd::CHECK_GC);
}

void translateInstCapture(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int type = LUAU_INSN_A(*pc);
    int index = LUAU_INSN_B(*pc);

    switch (type)
    {
    case LCT_VAL:
        build.inst(IrCmd::CAPTURE, build.vmReg(index), build.constUint(0));
        break;
    case LCT_REF:
        build.inst(IrCmd::CAPTURE, build.vmReg(index), build.constUint(1));
        break;
    case LCT_UPVAL:
        build.inst(IrCmd::CAPTURE, build.vmUpvalue(index), build.constUint(0));
        break;
    default:
        CODEGEN_ASSERT(!"Unknown upvalue capture type");
    }
}

bool translateInstNamecall(IrBuilder& build, const Instruction* pc, int pcpos)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    // TODO: we keep the table access lowering for speculative userdata access instructions until a later date
    uint32_t aux = LUAU_INSN_OP(*pc) == LOP_NAMECALLUDATA ? LUAU_INSN_AUX_KV16(pc[1]) : pc[1];

    BytecodeTypes bcTypes = build.function.getBytecodeTypesAt(pcpos);

    if (bcTypes.a == LBC_TYPE_VECTOR)
    {
        build.loadAndCheckTag(build.vmReg(rb), LUA_TVECTOR, build.vmExit(pcpos));

        if (build.hostHooks.vectorNamecall)
        {
            Instruction call = pc[2];
            CODEGEN_ASSERT(LUAU_INSN_OP(call) == LOP_CALL);

            int callra = LUAU_INSN_A(call);
            int nparams = LUAU_INSN_B(call) - 1;
            int nresults = LUAU_INSN_C(call) - 1;

            TString* str = gco2ts(build.function.proto->k[aux].value.gc);
            const char* field = getstr(str);

            if (build.hostHooks.vectorNamecall(build, field, str->len, callra, rb, nparams, nresults, pcpos))
                return true;
        }

        build.inst(IrCmd::FALLBACK_NAMECALL, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
        return false;
    }

    if (isUserdataBytecodeType(bcTypes.a))
    {
        build.loadAndCheckTag(build.vmReg(rb), LUA_TUSERDATA, build.vmExit(pcpos));

        if (build.hostHooks.userdataNamecall)
        {
            Instruction call = pc[2];
            CODEGEN_ASSERT(LUAU_INSN_OP(call) == LOP_CALL);

            int callra = LUAU_INSN_A(call);
            int nparams = LUAU_INSN_B(call) - 1;
            int nresults = LUAU_INSN_C(call) - 1;

            TString* str = gco2ts(build.function.proto->k[aux].value.gc);
            const char* field = getstr(str);

            if (build.hostHooks.userdataNamecall(build, bcTypes.a, field, str->len, callra, rb, nparams, nresults, pcpos))
                return true;
        }

        build.inst(IrCmd::FALLBACK_NAMECALL, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
        return false;
    }

    IrOp next = build.blockAtInst(pcpos + getOpLength(LuauOpcode(LOP_NAMECALL)));
    IrOp fallback = build.fallbackBlock(pcpos);
    IrOp firstFastPathSuccess = build.block(IrBlockKind::Internal);
    IrOp secondFastPath = build.block(IrBlockKind::Internal);

    build.loadAndCheckTag(build.vmReg(rb), LUA_TTABLE, bcTypes.a == LBC_TYPE_TABLE ? build.vmExit(pcpos) : fallback);
    IrOp table = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));

    CODEGEN_ASSERT(build.function.proto);
    IrOp addrNodeEl = build.inst(IrCmd::GET_HASH_NODE_ADDR, table, build.constUint(tsvalue(&build.function.proto->k[aux])->hash));

    // We use 'jump' version instead of 'check' guard because we are jumping away into a non-fallback block
    // This is required by CFG live range analysis because both non-fallback blocks define the same registers
    build.inst(IrCmd::JUMP_SLOT_MATCH, addrNodeEl, build.vmConst(aux), firstFastPathSuccess, secondFastPath);

    build.beginBlock(firstFastPathSuccess);
    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra + 1), table);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra + 1), build.constTag(LUA_TTABLE));

    IrOp nodeEl = build.inst(IrCmd::LOAD_TVALUE, addrNodeEl, build.constInt(offsetof(LuaNode, val)));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), nodeEl);
    build.inst(IrCmd::JUMP, next);

    build.beginBlock(secondFastPath);

    build.inst(IrCmd::CHECK_NODE_NO_NEXT, addrNodeEl, fallback);

    IrOp indexPtr = build.inst(IrCmd::TRY_CALL_FASTGETTM, table, build.constInt(TM_INDEX), fallback);

    build.loadAndCheckTag(indexPtr, LUA_TTABLE, fallback);
    IrOp index = build.inst(IrCmd::LOAD_POINTER, indexPtr);

    IrOp addrIndexNodeEl = build.inst(IrCmd::GET_SLOT_NODE_ADDR, index, build.constUint(pcpos), build.vmConst(aux));
    build.inst(IrCmd::CHECK_SLOT_MATCH, addrIndexNodeEl, build.vmConst(aux), fallback);

    // TODO: original 'table' was clobbered by a call inside 'FASTGETTM'
    // Ideally, such calls should have to effect on SSA IR values, but simple register allocator doesn't support it
    IrOp table2 = build.inst(IrCmd::LOAD_POINTER, build.vmReg(rb));
    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra + 1), table2);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra + 1), build.constTag(LUA_TTABLE));

    IrOp indexNodeEl = build.inst(IrCmd::LOAD_TVALUE, addrIndexNodeEl, build.constInt(offsetof(LuaNode, val)));
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), indexNodeEl);
    build.inst(IrCmd::JUMP, next);

    build.beginBlock(fallback);
    build.inst(IrCmd::FALLBACK_NAMECALL, build.constUint(pcpos), build.vmReg(ra), build.vmReg(rb), build.vmConst(aux));
    build.inst(IrCmd::JUMP, next);

    build.beginBlock(next);

    return false;
}

void translateInstAndX(IrBuilder& build, const Instruction* pc, int pcpos, IrOp c)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    // "b and c" -> "truthy(b) ? c : b"
    IrOp lhs = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(rb));
    IrOp rhs = build.inst(IrCmd::LOAD_TVALUE, c);

    IrOp result = build.inst(IrCmd::SELECT_IF_TRUTHY, lhs, rhs, lhs);
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), result);
}

void translateInstOrX(IrBuilder& build, const Instruction* pc, int pcpos, IrOp c)
{
    int ra = LUAU_INSN_A(*pc);
    int rb = LUAU_INSN_B(*pc);

    // "b or c" -> truthy(b) ? b : c
    IrOp lhs = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(rb));
    IrOp rhs = build.inst(IrCmd::LOAD_TVALUE, c);

    IrOp result = build.inst(IrCmd::SELECT_IF_TRUTHY, lhs, lhs, rhs);
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), result);
}

void translateInstNewClosure(IrBuilder& build, const Instruction* pc, int pcpos)
{
    CODEGEN_ASSERT(unsigned(LUAU_INSN_D(*pc)) < unsigned(build.function.proto->sizep));

    int ra = LUAU_INSN_A(*pc);
    Proto* pv = build.function.proto->p[LUAU_INSN_D(*pc)];

    build.inst(IrCmd::SET_SAVEDPC, build.constUint(pcpos + 1));

    IrOp env = build.inst(IrCmd::LOAD_ENV);
    IrOp ncl = build.inst(IrCmd::NEWCLOSURE, build.constUint(pv->nups), env, build.constUint(LUAU_INSN_D(*pc)));

    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra), ncl);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TFUNCTION));

    for (int ui = 0; ui < pv->nups; ++ui)
    {
        Instruction uinsn = pc[ui + 1];
        CODEGEN_ASSERT(LUAU_INSN_OP(uinsn) == LOP_CAPTURE);

        switch (LUAU_INSN_A(uinsn))
        {
        case LCT_VAL:
        {
            IrOp src = build.inst(IrCmd::LOAD_TVALUE, build.vmReg(LUAU_INSN_B(uinsn)));
            IrOp dst = build.inst(IrCmd::GET_CLOSURE_UPVAL_ADDR, ncl, build.vmUpvalue(ui));
            build.inst(IrCmd::STORE_TVALUE, dst, src);
            break;
        }

        case LCT_REF:
        {
            IrOp src = build.inst(IrCmd::FINDUPVAL, build.vmReg(LUAU_INSN_B(uinsn)));
            IrOp dst = build.inst(IrCmd::GET_CLOSURE_UPVAL_ADDR, ncl, build.vmUpvalue(ui));
            build.inst(IrCmd::STORE_POINTER, dst, src);
            build.inst(IrCmd::STORE_TAG, dst, build.constTag(LUA_TUPVAL));
            break;
        }

        case LCT_UPVAL:
        {
            IrOp src = build.inst(IrCmd::GET_CLOSURE_UPVAL_ADDR, build.undef(), build.vmUpvalue(LUAU_INSN_B(uinsn)));
            IrOp dst = build.inst(IrCmd::GET_CLOSURE_UPVAL_ADDR, ncl, build.vmUpvalue(ui));
            IrOp load = build.inst(IrCmd::LOAD_TVALUE, src);
            build.inst(IrCmd::STORE_TVALUE, dst, load);
            break;
        }

        default:
            CODEGEN_ASSERT(!"Unknown upvalue capture type");
            LUAU_UNREACHABLE(); // improves switch() codegen by eliding opcode bounds checks
        }
    }

    build.inst(IrCmd::CHECK_GC);
}

} // namespace CodeGen
} // namespace Luau
