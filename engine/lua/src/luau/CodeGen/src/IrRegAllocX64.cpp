// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/IrRegAllocX64.h"

#include "Luau/IrUtils.h"
#include "Luau/LoweringStats.h"

#include "EmitCommonX64.h"

#include "lstate.h"

LUAU_FASTFLAGVARIABLE(LuauCodegenNewRegSplit)

namespace Luau
{
namespace CodeGen
{
namespace X64
{

static constexpr unsigned kValueDwordSize[] = {0, 0, 1, 1, 2, 2, 1, 2, 4};
static_assert(sizeof(kValueDwordSize) / sizeof(kValueDwordSize[0]) == size_t(IrValueKind::Count), "all kinds have to be covered");

static const RegisterX64 kGprAllocOrder[] = {rax, rdx, rcx, rbx, rsi, rdi, r8, r9, r10, r11};

IrRegAllocX64::IrRegAllocX64(AssemblyBuilderX64& build, IrFunction& function, LoweringStats* stats)
    : build(build)
    , function(function)
    , stats(stats)
    , usableXmmRegCount(getXmmRegisterCount(build.abi))
{
    freeGprMap.fill(true);
    gprInstUsers.fill(kInvalidInstIdx);
    freeXmmMap.fill(true);
    xmmInstUsers.fill(kInvalidInstIdx);
}

RegisterX64 IrRegAllocX64::allocReg(SizeX64 size, uint32_t instIdx)
{
    if (size == SizeX64::xmmword)
    {
        for (size_t i = 0; i < usableXmmRegCount; ++i)
        {
            if (freeXmmMap[i])
            {
                freeXmmMap[i] = false;
                xmmInstUsers[i] = instIdx;
                return RegisterX64{size, uint8_t(i)};
            }
        }
    }
    else
    {
        for (RegisterX64 reg : kGprAllocOrder)
        {
            if (freeGprMap[reg.index])
            {
                freeGprMap[reg.index] = false;
                gprInstUsers[reg.index] = instIdx;
                return RegisterX64{size, reg.index};
            }
        }
    }

    // Out of registers, spill the value with the furthest next use
    const std::array<uint32_t, 16>& regInstUsers = size == SizeX64::xmmword ? xmmInstUsers : gprInstUsers;
    if (uint32_t furthestUseTarget = findInstructionWithFurthestNextUse(regInstUsers); furthestUseTarget != kInvalidInstIdx)
    {
        RegisterX64 reg = function.instructions[furthestUseTarget].regX64;
        reg.size = size; // Adjust size to the requested

        return takeReg(reg, instIdx);
    }

    CODEGEN_ASSERT(!"Out of registers to allocate");
    return noreg;
}

RegisterX64 IrRegAllocX64::allocRegOrReuse(SizeX64 size, uint32_t instIdx, std::initializer_list<IrOp> oprefs)
{
    for (IrOp op : oprefs)
    {
        if (op.kind != IrOpKind::Inst)
            continue;

        IrInst& source = function.instructions[op.index];

        if (source.lastUse == instIdx && !source.reusedReg && !source.spilled && !source.needsReload)
        {
            // Not comparing size directly because we only need matching register set
            if ((size == SizeX64::xmmword) != (source.regX64.size == SizeX64::xmmword))
                continue;

            CODEGEN_ASSERT(source.regX64 != noreg);

            source.reusedReg = true;

            if (size == SizeX64::xmmword)
                xmmInstUsers[source.regX64.index] = instIdx;
            else
                gprInstUsers[source.regX64.index] = instIdx;

            return RegisterX64{size, source.regX64.index};
        }
    }

    return allocReg(size, instIdx);
}

RegisterX64 IrRegAllocX64::takeReg(RegisterX64 reg, uint32_t instIdx)
{
    if (reg.size == SizeX64::xmmword)
    {
        if (!freeXmmMap[reg.index])
        {
            CODEGEN_ASSERT(xmmInstUsers[reg.index] != kInvalidInstIdx);
            preserve(function.instructions[xmmInstUsers[reg.index]]);
        }

        CODEGEN_ASSERT(freeXmmMap[reg.index]);
        freeXmmMap[reg.index] = false;
        xmmInstUsers[reg.index] = instIdx;
    }
    else
    {
        if (!freeGprMap[reg.index])
        {
            CODEGEN_ASSERT(gprInstUsers[reg.index] != kInvalidInstIdx);
            preserve(function.instructions[gprInstUsers[reg.index]]);
        }

        CODEGEN_ASSERT(freeGprMap[reg.index]);
        freeGprMap[reg.index] = false;
        gprInstUsers[reg.index] = instIdx;
    }

    return reg;
}

bool IrRegAllocX64::canTakeReg(RegisterX64 reg) const
{
    const std::array<bool, 16>& freeMap = reg.size == SizeX64::xmmword ? freeXmmMap : freeGprMap;
    const std::array<uint32_t, 16>& instUsers = reg.size == SizeX64::xmmword ? xmmInstUsers : gprInstUsers;

    return freeMap[reg.index] || instUsers[reg.index] != kInvalidInstIdx;
}

void IrRegAllocX64::freeReg(RegisterX64 reg)
{
    if (reg.size == SizeX64::xmmword)
    {
        CODEGEN_ASSERT(!freeXmmMap[reg.index]);
        freeXmmMap[reg.index] = true;
        xmmInstUsers[reg.index] = kInvalidInstIdx;
    }
    else
    {
        CODEGEN_ASSERT(!freeGprMap[reg.index]);
        freeGprMap[reg.index] = true;
        gprInstUsers[reg.index] = kInvalidInstIdx;
    }
}

void IrRegAllocX64::freeLastUseReg(IrInst& target, uint32_t instIdx)
{
    if (isLastUseReg(target, instIdx))
    {
        CODEGEN_ASSERT(!target.spilled && !target.needsReload);

        // Register might have already been freed if it had multiple uses inside a single instruction
        if (target.regX64 == noreg)
            return;

        freeReg(target.regX64);
        target.regX64 = noreg;
    }
}

void IrRegAllocX64::freeLastUseRegs(const IrInst& inst, uint32_t instIdx)
{
    auto checkOp = [this, instIdx](IrOp op)
    {
        if (op.kind == IrOpKind::Inst)
            freeLastUseReg(function.instructions[op.index], instIdx);
    };

    for (const IrOp& op : inst.ops)
        checkOp(op);
}

bool IrRegAllocX64::isLastUseReg(const IrInst& target, uint32_t instIdx) const
{
    return target.lastUse == instIdx && !target.reusedReg;
}

void IrRegAllocX64::preserve(IrInst& inst)
{
    IrSpillX64 spill;
    spill.instIdx = function.getInstIndex(inst);
    spill.valueKind = getCmdValueKind(inst.cmd);
    spill.spillId = nextSpillId++;
    spill.originalLoc = inst.regX64;

    // Loads from VmReg/VmConst don't have to be spilled, they can be restored from a register later
    // When checking if value has a restore operation to spill it, we only allow it in the same block
    if (!function.hasRestoreLocation(inst, /*limitToCurrentBlock*/ true))
    {
        unsigned i = findSpillStackSlot(spill.valueKind);

        if (isExtraSpillSlot(i))
        {
            int extraOffset = getExtraSpillAddressOffset(i);

            // Tricky situation, no registers left, but need a register to calculate an address
            // We will try to take r11 unless it's actually the register being spilled
            RegisterX64 emergencyTemp = inst.regX64.size == SizeX64::xmmword || inst.regX64.index != 11 ? r11 : r10;

            build.mov(qword[sTemporarySlot + 0], emergencyTemp);

            build.mov(emergencyTemp, qword[rState + offsetof(lua_State, global)]);
            build.lea(emergencyTemp, addr[emergencyTemp + offsetof(global_State, ecbdata) + extraOffset]);

            if (spill.valueKind == IrValueKind::Tvalue)
                build.vmovups(xmmword[emergencyTemp], inst.regX64);
            else if (spill.valueKind == IrValueKind::Double)
                build.vmovsd(qword[emergencyTemp], inst.regX64);
            else if (spill.valueKind == IrValueKind::Pointer || spill.valueKind == IrValueKind::Int64)
                build.mov(qword[emergencyTemp], inst.regX64);
            else if (spill.valueKind == IrValueKind::Tag || spill.valueKind == IrValueKind::Int)
                build.mov(dword[emergencyTemp], inst.regX64);
            else if (spill.valueKind == IrValueKind::Float)
                build.vmovss(dword[emergencyTemp], inst.regX64);
            else
                CODEGEN_ASSERT(!"Unsupported value kind");

            build.mov(emergencyTemp, qword[sTemporarySlot + 0]);
        }
        else
        {
            if (spill.valueKind == IrValueKind::Tvalue)
                build.vmovups(xmmword[sSpillArea + i * 4], inst.regX64);
            else if (spill.valueKind == IrValueKind::Double)
                build.vmovsd(qword[sSpillArea + i * 4], inst.regX64);
            else if (spill.valueKind == IrValueKind::Pointer || spill.valueKind == IrValueKind::Int64)
                build.mov(qword[sSpillArea + i * 4], inst.regX64);
            else if (spill.valueKind == IrValueKind::Tag || spill.valueKind == IrValueKind::Int)
                build.mov(dword[sSpillArea + i * 4], inst.regX64);
            else if (spill.valueKind == IrValueKind::Float)
                build.vmovss(dword[sSpillArea + i * 4], inst.regX64);
            else
                CODEGEN_ASSERT(!"Unsupported value kind");
        }

        unsigned end = i + kValueDwordSize[int(spill.valueKind)];

        for (unsigned pos = i; pos < end; pos++)
            usedSpillSlotHalfs.set(pos);

        if ((end + 1) / 2 > maxUsedSlot)
            maxUsedSlot = (end + 1) / 2;

        spill.stackSlot = uint8_t(i);
        inst.spilled = true;

        if (stats)
            stats->spillsToSlot++;
    }
    else
    {
        inst.needsReload = true;

        if (stats)
            stats->spillsToRestore++;
    }

    spills.push_back(spill);

    freeReg(inst.regX64);
    inst.regX64 = noreg;
}

void IrRegAllocX64::restore(IrInst& inst, bool intoOriginalLocation)
{
    uint32_t instIdx = function.getInstIndex(inst);

    for (size_t i = 0; i < spills.size(); i++)
    {
        if (spills[i].instIdx == instIdx)
        {
            RegisterX64 reg = intoOriginalLocation ? takeReg(spills[i].originalLoc, instIdx) : allocReg(spills[i].originalLoc.size, instIdx);

            // When restoring the value, we allow cross-block restore because we have commited to the target location at spill time
            ValueRestoreLocation restoreLocation = function.findRestoreLocation(inst, /*limitToCurrentBlock*/ false);

            OperandX64 restoreAddr = noreg;

            RegisterX64 emergencyTemp = reg.size == SizeX64::xmmword ? r11 : qwordReg(reg);

            // Previous call might have relocated the spill vector, so this reference can't be taken earlier
            const IrSpillX64& spill = spills[i];

            if (spill.stackSlot != kNoStackSlot)
            {
                if (isExtraSpillSlot(spill.stackSlot))
                {
                    int extraOffset = getExtraSpillAddressOffset(spill.stackSlot);

                    // Need to calculate an address, but everything might be taken
                    if (reg.size == SizeX64::xmmword)
                        build.mov(qword[sTemporarySlot + 0], emergencyTemp);

                    build.mov(emergencyTemp, qword[rState + offsetof(lua_State, global)]);
                    build.lea(emergencyTemp, addr[emergencyTemp + offsetof(global_State, ecbdata) + extraOffset]);

                    restoreAddr = addr[emergencyTemp];
                    restoreAddr.memSize = reg.size;
                }
                else
                {
                    restoreAddr = addr[sSpillArea + spill.stackSlot * 4];
                    restoreAddr.memSize = reg.size;
                }

                if (spill.valueKind == IrValueKind::Double || spill.valueKind == IrValueKind::Int64)
                    restoreAddr.memSize = SizeX64::qword;
                else if (spill.valueKind == IrValueKind::Float)
                    restoreAddr.memSize = SizeX64::dword;

                unsigned end = spill.stackSlot + kValueDwordSize[int(spill.valueKind)];

                for (unsigned pos = spill.stackSlot; pos < end; pos++)
                    usedSpillSlotHalfs.set(pos, false);
            }
            else
            {
                restoreAddr = getRestoreAddress(inst, restoreLocation);
            }

            if (spill.valueKind == IrValueKind::Tvalue)
            {
                build.vmovups(reg, restoreAddr);
            }
            else if (spill.valueKind == IrValueKind::Double)
            {
                build.vmovsd(reg, restoreAddr);
            }
            else if (spill.valueKind == IrValueKind::Int && restoreLocation.kind == IrValueKind::Double)
            {
                // Handle restore of an int/uint value from a location storing a double number
                if (restoreLocation.conversionCmd == IrCmd::INT_TO_NUM)
                    build.vcvttsd2si(reg, restoreAddr);
                else if (restoreLocation.conversionCmd == IrCmd::UINT_TO_NUM)
                    build.vcvttsd2si(qwordReg(reg), restoreAddr); // Note: we perform 'uint64_t = (long long)double' for consistency with C++ code
                else
                    CODEGEN_ASSERT(!"re-materialization not supported for this conversion command");
            }
            else if (spill.valueKind == IrValueKind::Tag || spill.valueKind == IrValueKind::Int || spill.valueKind == IrValueKind::Int64 ||
                     spill.valueKind == IrValueKind::Pointer)
            {
                build.mov(reg, restoreAddr);
            }
            else if (spill.valueKind == IrValueKind::Float)
            {
                build.vmovss(reg, restoreAddr);
            }
            else
            {
                CODEGEN_ASSERT(!"value kind not supported for restore");
            }

            if (spill.stackSlot != kNoStackSlot && isExtraSpillSlot(spill.stackSlot))
            {
                if (reg.size == SizeX64::xmmword)
                    build.mov(emergencyTemp, qword[sTemporarySlot + 0]);
            }

            inst.regX64 = reg;
            inst.spilled = false;
            inst.needsReload = false;

            spills[i] = spills.back();
            spills.pop_back();
            return;
        }
    }
}

void IrRegAllocX64::preserveAndFreeInstValues()
{
    for (uint32_t instIdx : gprInstUsers)
    {
        if (instIdx != kInvalidInstIdx)
            preserve(function.instructions[instIdx]);
    }

    for (uint32_t instIdx : xmmInstUsers)
    {
        if (instIdx != kInvalidInstIdx)
            preserve(function.instructions[instIdx]);
    }
}

bool IrRegAllocX64::shouldFreeGpr(RegisterX64 reg) const
{
    if (reg == noreg)
        return false;

    CODEGEN_ASSERT(reg.size != SizeX64::xmmword);

    for (RegisterX64 gpr : kGprAllocOrder)
    {
        if (reg.index == gpr.index)
            return true;
    }

    return false;
}

unsigned IrRegAllocX64::findSpillStackSlot(IrValueKind valueKind)
{
    if (valueKind == IrValueKind::Float || valueKind == IrValueKind::Int)
    {
        for (unsigned i = 0; i < unsigned(usedSpillSlotHalfs.size()); ++i)
        {
            if (usedSpillSlotHalfs.test(i))
                continue;

            return i;
        }
    }
    else
    {
        unsigned numHalves = kValueDwordSize[int(valueKind)];
        unsigned boundary = kSpillSlots * 2;

        // Find a free stack slot. Four consecutive slots might be required for 16 byte TValues, so '- 3' is used
        // For 8 and 16 byte types we search in steps of 2 to return slot indices aligned by 2
        for (unsigned i = 0; i < unsigned(usedSpillSlotHalfs.size() - 3); i += 2)
        {
            // Prevent large value from allocating at stack/extra spill storage boundary
            if (FFlag::LuauCodegenNewRegSplit && i < boundary && i + numHalves > boundary)
            {
                i = boundary - 2;
                continue;
            }

            if (usedSpillSlotHalfs.test(i) || usedSpillSlotHalfs.test(i + 1))
                continue;

            if (valueKind == IrValueKind::Tvalue)
            {
                if (usedSpillSlotHalfs.test(i + 2) || usedSpillSlotHalfs.test(i + 3))
                {
                    i += 2; // No need to retest this double position
                    continue;
                }
            }

            return i;
        }
    }

    CODEGEN_ASSERT(!"Nowhere to spill");
    return ~0u;
}

OperandX64 IrRegAllocX64::getRestoreAddress(const IrInst& inst, ValueRestoreLocation restoreLocation)
{
    IrOp op = restoreLocation.op;
    CODEGEN_ASSERT(op.kind != IrOpKind::None);

    [[maybe_unused]] IrValueKind instKind = getCmdValueKind(inst.cmd);

    switch (restoreLocation.kind)
    {
    case IrValueKind::Unknown:
    case IrValueKind::None:
    case IrValueKind::Float:
    case IrValueKind::Count:
    case IrValueKind::Int64:
        return restoreLocation.op.kind == IrOpKind::VmReg ? luauRegValueInt64(vmRegOp(op)) : luauConstantValue(vmConstOp(op));
    case IrValueKind::Tag:
        return op.kind == IrOpKind::VmReg ? luauRegTag(vmRegOp(op)) : luauConstantTag(vmConstOp(op));
    case IrValueKind::Int:
        CODEGEN_ASSERT(op.kind == IrOpKind::VmReg);
        return luauRegValueInt(vmRegOp(op));
    case IrValueKind::Pointer:
        return restoreLocation.op.kind == IrOpKind::VmReg ? luauRegValue(vmRegOp(op)) : luauConstantValue(vmConstOp(op));
    case IrValueKind::Double:
        return restoreLocation.op.kind == IrOpKind::VmReg ? luauRegValue(vmRegOp(op)) : luauConstantValue(vmConstOp(op));
    case IrValueKind::Tvalue:
        return restoreLocation.op.kind == IrOpKind::VmReg ? luauReg(vmRegOp(op)) : luauConstant(vmConstOp(op));
    }

    CODEGEN_ASSERT(!"Failed to find restore operand location");
    return noreg;
}

uint32_t IrRegAllocX64::findInstructionWithFurthestNextUse(const std::array<uint32_t, 16>& regInstUsers) const
{
    if (currInstIdx == kInvalidInstIdx)
        return kInvalidInstIdx;

    uint32_t furthestUseTarget = kInvalidInstIdx;
    uint32_t furthestUseLocation = 0;

    for (uint32_t regInstUser : regInstUsers)
    {
        // Cannot spill temporary registers or the register of the value that's defined in the current instruction
        if (regInstUser == kInvalidInstIdx || regInstUser == currInstIdx)
            continue;

        uint32_t nextUse = getNextInstUse(function, regInstUser, currInstIdx);

        // Cannot spill value that is about to be used in the current instruction
        if (nextUse == currInstIdx)
            continue;

        if (furthestUseTarget == kInvalidInstIdx || nextUse > furthestUseLocation)
        {
            furthestUseLocation = nextUse;
            furthestUseTarget = regInstUser;
        }
    }

    return furthestUseTarget;
}

bool IrRegAllocX64::isExtraSpillSlot(unsigned slot) const
{
    CODEGEN_ASSERT(slot != kNoStackSlot);

    return slot >= (FFlag::LuauCodegenNewRegSplit ? kSpillSlots : kSpillSlots_NEW) * 2;
}

int IrRegAllocX64::getExtraSpillAddressOffset(unsigned slot) const
{
    CODEGEN_ASSERT(isExtraSpillSlot(slot));

    return (slot - (FFlag::LuauCodegenNewRegSplit ? kSpillSlots : kSpillSlots_NEW) * 2) * 4;
}

void IrRegAllocX64::assertFree(RegisterX64 reg) const
{
    if (reg.size == SizeX64::xmmword)
        CODEGEN_ASSERT(freeXmmMap[reg.index]);
    else
        CODEGEN_ASSERT(freeGprMap[reg.index]);
}

void IrRegAllocX64::assertAllFree() const
{
    for (RegisterX64 reg : kGprAllocOrder)
        CODEGEN_ASSERT(freeGprMap[reg.index]);

    for (bool free : freeXmmMap)
        CODEGEN_ASSERT(free);
}

void IrRegAllocX64::assertNoSpills() const
{
    CODEGEN_ASSERT(spills.empty());
}

ScopedRegX64::ScopedRegX64(IrRegAllocX64& owner)
    : owner(owner)
    , reg(noreg)
{
}

ScopedRegX64::ScopedRegX64(IrRegAllocX64& owner, SizeX64 size)
    : owner(owner)
    , reg(noreg)
{
    alloc(size);
}

ScopedRegX64::ScopedRegX64(IrRegAllocX64& owner, RegisterX64 reg)
    : owner(owner)
    , reg(reg)
{
}

ScopedRegX64::~ScopedRegX64()
{
    if (reg != noreg)
        owner.freeReg(reg);
}

void ScopedRegX64::take(RegisterX64 reg)
{
    CODEGEN_ASSERT(this->reg == noreg);
    this->reg = owner.takeReg(reg, kInvalidInstIdx);
}

void ScopedRegX64::alloc(SizeX64 size)
{
    CODEGEN_ASSERT(reg == noreg);
    reg = owner.allocReg(size, kInvalidInstIdx);
}

void ScopedRegX64::free()
{
    CODEGEN_ASSERT(reg != noreg);
    owner.freeReg(reg);
    reg = noreg;
}

RegisterX64 ScopedRegX64::release()
{
    RegisterX64 tmp = reg;
    reg = noreg;
    return tmp;
}

ScopedSpills::ScopedSpills(IrRegAllocX64& owner)
    : owner(owner)
{
    startSpillId = owner.nextSpillId;
}

ScopedSpills::~ScopedSpills()
{
    unsigned endSpillId = owner.nextSpillId;

    for (size_t i = 0; i < owner.spills.size();)
    {
        IrSpillX64& spill = owner.spills[i];

        // Restoring spills inside this scope cannot create new spills
        CODEGEN_ASSERT(spill.spillId < endSpillId);

        // If spill was created inside current scope, it has to be restored
        if (spill.spillId >= startSpillId)
        {
            IrInst& inst = owner.function.instructions[spill.instIdx];

            owner.restore(inst, /*intoOriginalLocation*/ true);

            // Spill restore removes the spill entry, so loop is repeated at the same 'i'
        }
        else
        {
            i++;
        }
    }
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
