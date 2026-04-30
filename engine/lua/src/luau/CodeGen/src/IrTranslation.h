// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <stdint.h>

#include "ltm.h"

typedef uint32_t Instruction;

namespace Luau
{
namespace CodeGen
{

enum class IrCondition : uint8_t;
struct IrOp;
struct IrBuilder;
enum class IrCmd : uint8_t;

void translateInstLoadNil(IrBuilder& build, const Instruction* pc);
void translateInstLoadB(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstLoadN(IrBuilder& build, const Instruction* pc);
void translateInstLoadK(IrBuilder& build, const Instruction* pc);
void translateInstLoadKX(IrBuilder& build, const Instruction* pc);
void translateInstMove(IrBuilder& build, const Instruction* pc);
void translateInstJump(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpBack(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpIf(IrBuilder& build, const Instruction* pc, int pcpos, bool not_);
void translateInstJumpIfEq(IrBuilder& build, const Instruction* pc, int pcpos, bool not_);
void translateInstJumpIfEqShortcut(IrBuilder& build, const Instruction* pc, int pcpos, bool not_);
void translateInstJumpIfCond(IrBuilder& build, const Instruction* pc, int pcpos, IrCondition cond);
void translateInstJumpX(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqNil(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqNilShortcut(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqB(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqBShortcut(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqN(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqNShortcut(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqS(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstJumpxEqSShortcut(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstBinary(IrBuilder& build, const Instruction* pc, int pcpos, TMS tm);
void translateInstBinaryK(IrBuilder& build, const Instruction* pc, int pcpos, TMS tm);
void translateInstBinaryRK(IrBuilder& build, const Instruction* pc, int pcpos, TMS tm);
void translateInstNot(IrBuilder& build, const Instruction* pc);
void translateInstMinus(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstLength(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstNewTable(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstDupTable(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstGetUpval(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstSetUpval(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstCloseUpvals(IrBuilder& build, const Instruction* pc);
IrOp translateFastCallN(
    IrBuilder& build,
    const Instruction* pc,
    int pcpos,
    bool customParams,
    int customParamCount,
    IrOp customArgs,
    IrOp customArg3
);
void translateInstForNPrep(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstForNLoop(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstForGPrepNext(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstForGPrepInext(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstForGLoopIpairs(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstGetTableN(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstSetTableN(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstGetTable(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstSetTable(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstGetImport(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstGetTableKS(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstSetTableKS(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstGetGlobal(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstSetGlobal(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstConcat(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstCapture(IrBuilder& build, const Instruction* pc, int pcpos);
bool translateInstNamecall(IrBuilder& build, const Instruction* pc, int pcpos);
void translateInstAndX(IrBuilder& build, const Instruction* pc, int pcpos, IrOp c);
void translateInstOrX(IrBuilder& build, const Instruction* pc, int pcpos, IrOp c);
void translateInstNewClosure(IrBuilder& build, const Instruction* pc, int pcpos);

void beforeInstForNPrep(IrBuilder& build, const Instruction* pc, int pcpos);
void afterInstForNLoop(IrBuilder& build, const Instruction* pc);

} // namespace CodeGen
} // namespace Luau
