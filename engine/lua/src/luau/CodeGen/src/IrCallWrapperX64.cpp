// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/IrCallWrapperX64.h"

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrRegAllocX64.h"

#include "EmitCommonX64.h"

LUAU_FASTFLAGVARIABLE(LuauCodegenCallWrapImproved)

namespace Luau
{
namespace CodeGen
{
namespace X64
{

static const std::array<OperandX64, 6> kWindowsGprOrder = {rcx, rdx, r8, r9, addr[rsp + kStackRegHomeStorage], addr[rsp + kStackRegHomeStorage + 8]};
static const std::array<OperandX64, 6> kSystemvGprOrder = {rdi, rsi, rdx, rcx, r8, r9};
static const std::array<OperandX64, 4> kXmmOrder = {xmm0, xmm1, xmm2, xmm3}; // Common order for first 4 fp arguments on Windows/SystemV

static bool sameUnderlyingRegister(RegisterX64 a, RegisterX64 b)
{
    SizeX64 underlyingSizeA = a.size == SizeX64::xmmword ? SizeX64::xmmword : SizeX64::qword;
    SizeX64 underlyingSizeB = b.size == SizeX64::xmmword ? SizeX64::xmmword : SizeX64::qword;

    return underlyingSizeA == underlyingSizeB && a.index == b.index;
}

IrCallWrapperX64::IrCallWrapperX64(IrRegAllocX64& regs, AssemblyBuilderX64& build, uint32_t instIdx)
    : regs(regs)
    , build(build)
    , instIdx(instIdx)
    , funcOp(noreg)
{
    gprUses.fill(0);
    xmmUses.fill(0);
}

void IrCallWrapperX64::addArgument(SizeX64 targetSize, OperandX64 source, IrOp sourceOp)
{
    // Instruction operands rely on current instruction index for lifetime tracking
    CODEGEN_ASSERT(instIdx != kInvalidInstIdx || sourceOp.kind == IrOpKind::None);

    CODEGEN_ASSERT(argCount < kMaxCallArguments);
    CallArgument& arg = args[argCount++];
    arg = {targetSize, source, sourceOp};

    arg.target = getNextArgumentTarget(targetSize);

    if (build.abi == ABIX64::Windows)
    {
        // On Windows, gpr/xmm register positions move in sync
        gprPos++;
        xmmPos++;
    }
    else
    {
        if (targetSize == SizeX64::xmmword)
            xmmPos++;
        else
            gprPos++;
    }
}

void IrCallWrapperX64::addArgument(SizeX64 targetSize, ScopedRegX64& scopedReg)
{
    addArgument(targetSize, scopedReg.release(), {});
}

void IrCallWrapperX64::setResultRegister(RegisterX64 reg, uint32_t instIdx)
{
    CODEGEN_ASSERT(reg != noreg);

    resultReg = reg;
    resultInstIdx = instIdx;
}

void IrCallWrapperX64::call(const OperandX64& func)
{
    funcOp = func;

    // Free the result register before handling arguments so that no live value is preserved from it
    if (FFlag::LuauCodegenCallWrapImproved && resultReg != noreg)
        regs.freeReg(resultReg);

    countRegisterUses();

    for (int i = 0; i < argCount; ++i)
    {
        CallArgument& arg = args[i];

        if (arg.sourceOp.kind != IrOpKind::None)
        {
            if (IrInst* inst = regs.function.asInstOp(arg.sourceOp))
            {
                // Source registers are recorded separately from source operands in CallArgument
                // If source is the last use of IrInst, clear the register from the operand
                if (regs.isLastUseReg(*inst, instIdx))
                    inst->regX64 = noreg;
                // If it's not the last use and register is volatile, register ownership is taken, which also spills the operand
                else if (inst->regX64.size == SizeX64::xmmword || regs.shouldFreeGpr(inst->regX64))
                    regs.takeReg(inst->regX64, kInvalidInstIdx);
            }
        }

        // Immediate values are stored at the end since they are not interfering and target register can still be used temporarily
        if (arg.source.cat == CategoryX64::imm)
        {
            arg.candidate = false;
        }
        // Arguments passed through stack can be handled immediately
        else if (arg.target.cat == CategoryX64::mem)
        {
            if (arg.source.cat == CategoryX64::mem)
            {
                ScopedRegX64 tmp{regs, arg.target.memSize};

                freeSourceRegisters(arg);

                if (arg.source.memSize == SizeX64::none)
                    build.lea(tmp.reg, arg.source);
                else
                    build.mov(tmp.reg, arg.source);

                build.mov(arg.target, tmp.reg);
            }
            else
            {
                freeSourceRegisters(arg);

                build.mov(arg.target, arg.source);
            }

            arg.candidate = false;
        }
        // Skip arguments that are already in their place
        else if (arg.source.cat == CategoryX64::reg && sameUnderlyingRegister(arg.target.base, arg.source.base))
        {
            freeSourceRegisters(arg);

            // If target is not used as source in other arguments, prevent register allocator from giving it out
            if (getRegisterUses(arg.target.base) == 0)
                regs.takeReg(arg.target.base, kInvalidInstIdx);
            else // Otherwise, make sure we won't free it when last source use is completed
                addRegisterUse(arg.target.base);

            arg.candidate = false;
        }
    }

    // Repeat until we run out of arguments to pass
    while (true)
    {
        // Find target argument register that is not an active source
        if (CallArgument* candidate = findNonInterferingArgument())
        {
            // This section is only for handling register targets
            CODEGEN_ASSERT(candidate->target.cat == CategoryX64::reg);

            freeSourceRegisters(*candidate);

            CODEGEN_ASSERT(getRegisterUses(candidate->target.base) == 0);
            regs.takeReg(candidate->target.base, kInvalidInstIdx);

            moveToTarget(*candidate);

            candidate->candidate = false;
        }
        // If all registers cross-interfere (rcx <- rdx, rdx <- rcx), one has to be renamed
        else if (RegisterX64 conflict = findConflictingTarget(); conflict != noreg)
        {
            renameConflictingRegister(conflict);
        }
        else
        {
            for (int i = 0; i < argCount; ++i)
                CODEGEN_ASSERT(!args[i].candidate);
            break;
        }
    }

    // Handle immediate arguments last
    for (int i = 0; i < argCount; ++i)
    {
        CallArgument& arg = args[i];

        if (arg.source.cat == CategoryX64::imm)
        {
            // There could be a conflict with the function source register, make this argument a candidate to find it
            arg.candidate = true;

            if (RegisterX64 conflict = findConflictingTarget(); conflict != noreg)
                renameConflictingRegister(conflict);

            if (arg.target.cat == CategoryX64::reg)
                regs.takeReg(arg.target.base, kInvalidInstIdx);

            moveToTarget(arg);

            arg.candidate = false;
        }
    }

    // Free registers used in the function call
    removeRegisterUse(funcOp.base);
    removeRegisterUse(funcOp.index);

    // Just before the call is made, argument registers are all marked as free in register allocator
    for (int i = 0; i < argCount; ++i)
    {
        CallArgument& arg = args[i];

        if (arg.target.cat == CategoryX64::reg)
            regs.freeReg(arg.target.base);
    }

    regs.preserveAndFreeInstValues();

    regs.assertAllFree();

    build.call(funcOp);

    if (FFlag::LuauCodegenCallWrapImproved && resultReg != noreg)
    {
        // Result register was allocated before call was made, we freed it temporarily and taking it back
        regs.takeReg(resultReg, resultInstIdx);

        // Skip move to eax/rax/xmm0 result
        if (resultReg.index != 0)
        {
            RegisterX64 returnReg = RegisterX64{resultReg.size, 0};
            build.mov(resultReg, returnReg);
        }
    }
}

RegisterX64 IrCallWrapperX64::suggestNextArgumentRegister(SizeX64 size) const
{
    OperandX64 target = getNextArgumentTarget(size);

    if (target.cat != CategoryX64::reg)
        return regs.allocReg(size, kInvalidInstIdx);

    if (!regs.canTakeReg(target.base))
        return regs.allocReg(size, kInvalidInstIdx);

    return regs.takeReg(target.base, kInvalidInstIdx);
}

template<size_t N>
RegisterX64 IrCallWrapperX64::suggestArgumentRegister(SizeX64 size, AssemblyBuilderX64& build)
{
    static_assert(N <= 3, "Argument index must be 0-3 (Windows passes args 4+ on the stack)");

    if (size == SizeX64::xmmword)
        return kXmmOrder[N].base;

    const std::array<OperandX64, 6>& gprOrder = build.abi == ABIX64::Windows ? kWindowsGprOrder : kSystemvGprOrder;

    OperandX64 target = gprOrder[N];
    CODEGEN_ASSERT(target.cat == CategoryX64::reg);

    target.base.size = size;
    return target.base;
}

template RegisterX64 IrCallWrapperX64::suggestArgumentRegister<0>(SizeX64 size, AssemblyBuilderX64& build);
template RegisterX64 IrCallWrapperX64::suggestArgumentRegister<1>(SizeX64 size, AssemblyBuilderX64& build);
template RegisterX64 IrCallWrapperX64::suggestArgumentRegister<2>(SizeX64 size, AssemblyBuilderX64& build);
template RegisterX64 IrCallWrapperX64::suggestArgumentRegister<3>(SizeX64 size, AssemblyBuilderX64& build);

OperandX64 IrCallWrapperX64::getNextArgumentTarget(SizeX64 size) const
{
    if (size == SizeX64::xmmword)
    {
        CODEGEN_ASSERT(size_t(xmmPos) < kXmmOrder.size());
        return kXmmOrder[xmmPos];
    }

    const std::array<OperandX64, 6>& gprOrder = build.abi == ABIX64::Windows ? kWindowsGprOrder : kSystemvGprOrder;

    CODEGEN_ASSERT(size_t(gprPos) < gprOrder.size());
    OperandX64 target = gprOrder[gprPos];

    // Keep requested argument size
    if (target.cat == CategoryX64::reg)
        target.base.size = size;
    else if (target.cat == CategoryX64::mem)
        target.memSize = size;

    return target;
}

void IrCallWrapperX64::countRegisterUses()
{
    for (int i = 0; i < argCount; ++i)
    {
        addRegisterUse(args[i].source.base);
        addRegisterUse(args[i].source.index);
    }

    addRegisterUse(funcOp.base);
    addRegisterUse(funcOp.index);
}

CallArgument* IrCallWrapperX64::findNonInterferingArgument()
{
    for (int i = 0; i < argCount; ++i)
    {
        CallArgument& arg = args[i];

        if (arg.candidate && !interferesWithActiveSources(arg, i) && !interferesWithOperand(funcOp, arg.target.base))
            return &arg;
    }

    return nullptr;
}

bool IrCallWrapperX64::interferesWithOperand(const OperandX64& op, RegisterX64 reg) const
{
    return sameUnderlyingRegister(op.base, reg) || sameUnderlyingRegister(op.index, reg);
}

bool IrCallWrapperX64::interferesWithActiveSources(const CallArgument& targetArg, int targetArgIndex) const
{
    for (int i = 0; i < argCount; ++i)
    {
        const CallArgument& arg = args[i];

        if (arg.candidate && i != targetArgIndex && interferesWithOperand(arg.source, targetArg.target.base))
            return true;
    }

    return false;
}

bool IrCallWrapperX64::interferesWithActiveTarget(RegisterX64 sourceReg) const
{
    for (int i = 0; i < argCount; ++i)
    {
        const CallArgument& arg = args[i];

        if (arg.candidate && sameUnderlyingRegister(arg.target.base, sourceReg))
            return true;
    }

    return false;
}

void IrCallWrapperX64::moveToTarget(CallArgument& arg)
{
    if (arg.source.cat == CategoryX64::reg)
    {
        RegisterX64 source = arg.source.base;

        if (source.size == SizeX64::xmmword)
            build.vmovsd(arg.target, source, source);
        else
            build.mov(arg.target, source);
    }
    else if (arg.source.cat == CategoryX64::imm)
    {
        build.mov(arg.target, arg.source);
    }
    else
    {
        if (arg.source.memSize == SizeX64::none)
            build.lea(arg.target, arg.source);
        else if (arg.target.base.size == SizeX64::xmmword && arg.source.memSize == SizeX64::xmmword)
            build.vmovups(arg.target, arg.source);
        else if (arg.target.base.size == SizeX64::xmmword)
            build.vmovsd(arg.target, arg.source);
        else
            build.mov(arg.target, arg.source);
    }
}

void IrCallWrapperX64::freeSourceRegisters(CallArgument& arg)
{
    removeRegisterUse(arg.source.base);
    removeRegisterUse(arg.source.index);
}

void IrCallWrapperX64::renameRegister(RegisterX64& target, RegisterX64 reg, RegisterX64 replacement)
{
    if (sameUnderlyingRegister(target, reg))
    {
        addRegisterUse(replacement);
        removeRegisterUse(target);

        target.index = replacement.index; // Only change index, size is preserved
    }
}

void IrCallWrapperX64::renameSourceRegisters(RegisterX64 reg, RegisterX64 replacement)
{
    for (int i = 0; i < argCount; ++i)
    {
        CallArgument& arg = args[i];

        if (arg.candidate)
        {
            renameRegister(arg.source.base, reg, replacement);
            renameRegister(arg.source.index, reg, replacement);
        }
    }

    renameRegister(funcOp.base, reg, replacement);
    renameRegister(funcOp.index, reg, replacement);
}

RegisterX64 IrCallWrapperX64::findConflictingTarget() const
{
    for (int i = 0; i < argCount; ++i)
    {
        const CallArgument& arg = args[i];

        if (arg.candidate)
        {
            if (interferesWithActiveTarget(arg.source.base))
                return arg.source.base;

            if (interferesWithActiveTarget(arg.source.index))
                return arg.source.index;
        }
    }

    if (interferesWithActiveTarget(funcOp.base))
        return funcOp.base;

    if (interferesWithActiveTarget(funcOp.index))
        return funcOp.index;

    return noreg;
}

void IrCallWrapperX64::renameConflictingRegister(RegisterX64 conflict)
{
    // Get a fresh register
    RegisterX64 freshReg = regs.allocReg(conflict.size, kInvalidInstIdx);

    if (conflict.size == SizeX64::xmmword)
        build.vmovsd(freshReg, conflict, conflict);
    else
        build.mov(freshReg, conflict);

    renameSourceRegisters(conflict, freshReg);
}

int IrCallWrapperX64::getRegisterUses(RegisterX64 reg) const
{
    return reg.size == SizeX64::xmmword ? xmmUses[reg.index] : (reg.size != SizeX64::none ? gprUses[reg.index] : 0);
}

void IrCallWrapperX64::addRegisterUse(RegisterX64 reg)
{
    if (reg.size == SizeX64::xmmword)
        xmmUses[reg.index]++;
    else if (reg.size != SizeX64::none)
        gprUses[reg.index]++;
}

void IrCallWrapperX64::removeRegisterUse(RegisterX64 reg)
{
    if (reg.size == SizeX64::xmmword)
    {
        CODEGEN_ASSERT(xmmUses[reg.index] != 0);
        xmmUses[reg.index]--;

        if (xmmUses[reg.index] == 0) // we don't use persistent xmm regs so no need to call shouldFreeRegister
            regs.freeReg(reg);
    }
    else if (reg.size != SizeX64::none)
    {
        CODEGEN_ASSERT(gprUses[reg.index] != 0);
        gprUses[reg.index]--;

        if (gprUses[reg.index] == 0 && regs.shouldFreeGpr(reg))
            regs.freeReg(reg);
    }
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
