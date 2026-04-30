// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/AssemblyBuilderA64.h"
#include "Luau/CodeGenOptions.h"
#include "Luau/DenseHash.h"
#include "Luau/IrData.h"

#include "IrRegAllocA64.h"
#include "IrValueLocationTracking.h"

#include <vector>

namespace Luau
{
namespace CodeGen
{

struct ModuleHelpers;
struct AssemblyOptions;
struct LoweringStats;
enum class CodeGenCounter : unsigned;

namespace A64
{

struct IrLoweringA64
{
    IrLoweringA64(AssemblyBuilderA64& build, ModuleHelpers& helpers, IrFunction& function, LoweringStats* stats);

    void lowerInst(IrInst& inst, uint32_t index, const IrBlock& next);
    void startBlock(const IrBlock& curr);
    void finishBlock(const IrBlock& curr, const IrBlock& next);
    void finishFunction();

    bool hasError() const;

    bool isFallthroughBlock(const IrBlock& target, const IrBlock& next);
    void jumpOrFallthrough(IrBlock& target, const IrBlock& next);

    Label& getTargetLabel(IrOp op, Label& fresh);
    void finalizeTargetLabel(IrOp op, Label& fresh);

    void checkSafeEnv(IrOp target, const IrBlock& next);

    void allocAndIncrementCounterAt(CodeGenCounter kind, uint32_t pcpos);
    void incrementCounterAt(size_t offset);

    void checkObjectBarrierConditions(RegisterA64 object, RegisterA64 temp, RegisterA64 ra, IrOp raOp, int ratag, Label& skip);

    // Operand data build helpers
    // May emit data/address synthesis instructions
    RegisterA64 tempDouble(IrOp op);
    RegisterA64 tempFloat(IrOp op);
    RegisterA64 tempInt(IrOp op);
    RegisterA64 tempInt64(IrOp op);
    RegisterA64 tempUint(IrOp op);
    AddressA64 tempAddr(IrOp op, int offset, RegisterA64 tempStorage = noreg); // Existing temporary register can be provided
    AddressA64 tempAddrBuffer(IrOp bufferOp, IrOp indexOp, uint8_t tag);

    // May emit restore instructions
    RegisterA64 regOp(IrOp op);

    // Operand data lookup helpers
    IrConst constOp(IrOp op) const;
    uint8_t tagOp(IrOp op) const;
    int intOp(IrOp op) const;
    int64_t int64Op(IrOp op) const;
    unsigned uintOp(IrOp op) const;
    unsigned importOp(IrOp op) const;
    double doubleOp(IrOp op) const;

    IrBlock& blockOp(IrOp op) const;
    Label& labelOp(IrOp op) const;

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

    AssemblyBuilderA64& build;
    ModuleHelpers& helpers;

    IrFunction& function;
    LoweringStats* stats = nullptr;

    IrRegAllocA64 regs;

    IrValueLocationTracking valueTracker;

    std::vector<InterruptHandler> interruptHandlers;
    std::vector<ExitHandler> exitHandlers;
    DenseHashMap<uint32_t, uint32_t> exitHandlerMap;

    bool error = false;
};

} // namespace A64
} // namespace CodeGen
} // namespace Luau
