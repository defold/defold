// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/DenseHash.h"
#include "Luau/IrData.h"
#include "Luau/IrRegAllocX64.h"

#include "IrValueLocationTracking.h"

#include <vector>

struct Proto;

namespace Luau
{
namespace CodeGen
{

struct ModuleHelpers;
struct AssemblyOptions;
struct LoweringStats;
enum class CodeGenCounter : unsigned;

namespace X64
{

struct IrLoweringX64
{
    IrLoweringX64(AssemblyBuilderX64& build, ModuleHelpers& helpers, IrFunction& function, LoweringStats* stats);

    void lowerInst(IrInst& inst, uint32_t index, const IrBlock& next);
    void startBlock(const IrBlock& curr);
    void finishBlock(const IrBlock& curr, const IrBlock& next);
    void finishFunction();

    bool hasError() const;

    bool isFallthroughBlock(const IrBlock& target, const IrBlock& next);
    void jumpOrFallthrough(IrBlock& target, const IrBlock& next);

    Label& getTargetLabel(IrOp op, Label& fresh);
    void finalizeTargetLabel(IrOp op, Label& fresh);

    void jumpOrAbortOnUndef(ConditionX64 cond, IrOp target, const IrBlock& next);
    void jumpOrAbortOnUndef(IrOp target, const IrBlock& next);

    void storeFloat(OperandX64 dst, IrOp src);
    void storeDoubleAsFloat(OperandX64 dst, IrOp src);
    void checkSafeEnv(IrOp target, const IrBlock& next);

    void allocAndIncrementCounterAt(CodeGenCounter kind, uint32_t pcpos);
    void incrementCounterAt(size_t offset);

    // Operand data lookup helpers
    OperandX64 memRegDoubleOp(IrOp op);
    OperandX64 memRegFloatOp(IrOp op);
    OperandX64 memRegUintOp(IrOp op);
    OperandX64 memRegIntOp(IrOp op);
    OperandX64 memRegInt64Op(IrOp op);
    OperandX64 memRegTagOp(IrOp op);
    RegisterX64 regOp(IrOp op);
    OperandX64 bufferAddrOp(IrOp bufferOp, IrOp indexOp, uint8_t tag);
    RegisterX64 vecOp(IrOp op, ScopedRegX64& tmp);

    IrConst constOp(IrOp op) const;
    uint8_t tagOp(IrOp op) const;
    int intOp(IrOp op) const;
    int64_t int64Op(IrOp op) const;
    unsigned uintOp(IrOp op) const;
    unsigned importOp(IrOp op) const;
    double doubleOp(IrOp op) const;

    IrBlock& blockOp(IrOp op) const;
    Label& labelOp(IrOp op) const;

    OperandX64 vectorAndMaskOp();

    struct InterruptHandler
    {
        Label self;
        unsigned int pcpos;
        Label next;
    };

    struct ExitHandler
    {
        Label self;
        unsigned int pcpos;
    };

    AssemblyBuilderX64& build;
    ModuleHelpers& helpers;

    IrFunction& function;
    LoweringStats* stats = nullptr;

    IrRegAllocX64 regs;

    IrValueLocationTracking valueTracker;

    std::vector<InterruptHandler> interruptHandlers;
    std::vector<ExitHandler> exitHandlers;
    DenseHashMap<uint32_t, uint32_t> exitHandlerMap;

    OperandX64 vectorAndMask = noreg;
    OperandX64 vectorOrMask = noreg;
};

} // namespace X64
} // namespace CodeGen
} // namespace Luau
