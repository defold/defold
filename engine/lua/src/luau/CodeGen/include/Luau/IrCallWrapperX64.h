// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrData.h"
#include "Luau/OperandX64.h"
#include "Luau/RegisterX64.h"

#include <array>

// TODO: call wrapper can be used to suggest target registers for ScopedRegX64 to compute data into argument registers directly

namespace Luau
{
namespace CodeGen
{
namespace X64
{

struct IrRegAllocX64;
struct ScopedRegX64;

struct CallArgument
{
    SizeX64 targetSize = SizeX64::none;

    OperandX64 source = noreg;
    IrOp sourceOp;

    OperandX64 target = noreg;
    bool candidate = true;
};

class IrCallWrapperX64
{
public:
    IrCallWrapperX64(IrRegAllocX64& regs, AssemblyBuilderX64& build, uint32_t instIdx = kInvalidInstIdx);

    void addArgument(SizeX64 targetSize, OperandX64 source, IrOp sourceOp = {});
    void addArgument(SizeX64 targetSize, ScopedRegX64& scopedReg);

    // Declare that the call produces a result that should be placed in the selected register
    void setResultRegister(RegisterX64 reg, uint32_t instIdx);

    void call(const OperandX64& func);

    RegisterX64 suggestNextArgumentRegister(SizeX64 size) const;
    // Returns the register for argument position N, sized according to 'size', for the given ABI.
    // N must be 0-3: on Windows, args 4+ are passed on the stack (not in registers).
    template<size_t N>
    static RegisterX64 suggestArgumentRegister(SizeX64 size, AssemblyBuilderX64& build);

    IrRegAllocX64& regs;
    AssemblyBuilderX64& build;
    uint32_t instIdx = ~0u;

private:
    OperandX64 getNextArgumentTarget(SizeX64 size) const;
    void countRegisterUses();
    CallArgument* findNonInterferingArgument();
    bool interferesWithOperand(const OperandX64& op, RegisterX64 reg) const;
    bool interferesWithActiveSources(const CallArgument& targetArg, int targetArgIndex) const;
    bool interferesWithActiveTarget(RegisterX64 sourceReg) const;
    void moveToTarget(CallArgument& arg);
    void freeSourceRegisters(CallArgument& arg);
    void renameRegister(RegisterX64& target, RegisterX64 reg, RegisterX64 replacement);
    void renameSourceRegisters(RegisterX64 reg, RegisterX64 replacement);
    RegisterX64 findConflictingTarget() const;
    void renameConflictingRegister(RegisterX64 conflict);

    int getRegisterUses(RegisterX64 reg) const;
    void addRegisterUse(RegisterX64 reg);
    void removeRegisterUse(RegisterX64 reg);

    static const int kMaxCallArguments = 6;
    std::array<CallArgument, kMaxCallArguments> args;
    int argCount = 0;

    int gprPos = 0;
    int xmmPos = 0;

    OperandX64 funcOp;

    RegisterX64 resultReg = noreg;
    uint32_t resultInstIdx = kInvalidInstIdx;

    // Internal counters for remaining register use counts
    std::array<uint8_t, 16> gprUses;
    std::array<uint8_t, 16> xmmUses;
};

} // namespace X64
} // namespace CodeGen
} // namespace Luau
