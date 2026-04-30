// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrRegAllocA64.h"

#include "Luau/AssemblyBuilderA64.h"
#include "Luau/IrUtils.h"
#include "Luau/LoweringStats.h"

#include "BitUtils.h"
#include "EmitCommonA64.h"

#include <string.h>

LUAU_FASTFLAGVARIABLE(DebugCodegenChaosA64)
LUAU_FASTFLAG(LuauCodegenNewRegSplit)

namespace Luau
{
namespace CodeGen
{
namespace A64
{

static const int8_t kInvalidSpill = 64;
static_assert(kSpillSlots + kExtraSpillSlots < 64, "arm64 lowering can only handle 63 spill slots");

static int allocSpill(uint64_t& free, KindA64 kind)
{
    CODEGEN_ASSERT(kStackSize <= 256); // to support larger stack frames, we need to ensure qN is allocated at 16b boundary to fit in ldr/str encoding

    if (FFlag::LuauCodegenNewRegSplit)
    {
        uint64_t search = free;

        // qN registers use two consecutive slots
        if (kind == KindA64::q)
        {
            // Make sure bit N is set only if bit N+1 is also set
            search = free & (free >> 1);

            // Prevent qN from allocating at stack/extra spill storage boundary (by reserving last stack slot)
            search &= ~(1ull << (kSpillSlots - 1));
        }

        int slot = countrz(search);
        if (slot == 64)
            return -1;

        uint64_t mask = (kind == KindA64::q ? 3ull : 1ull) << (unsigned long long)slot;

        CODEGEN_ASSERT((free & mask) == mask);
        free &= ~mask;

        return slot;
    }
    else
    {
        // qN registers use two consecutive slots
        int slot = countrz(kind == KindA64::q ? free & (free >> 1) : free);
        if (slot == 64)
            return -1;

        uint64_t mask = (kind == KindA64::q ? 3ull : 1ull) << (unsigned long long)slot;

        CODEGEN_ASSERT((free & mask) == mask);
        free &= ~mask;

        return slot;
    }
}

static void freeSpill(uint64_t& free, KindA64 kind, uint8_t slot)
{
    // qN registers use two consecutive slots
    uint64_t mask = (kind == KindA64::q ? 3ull : 1ull) << (unsigned long long)slot;

    CODEGEN_ASSERT((free & mask) == 0);
    free |= mask;
}

static int getReloadOffset(IrValueKind kind)
{
    switch (kind)
    {
    case IrValueKind::Unknown:
    case IrValueKind::None:
    case IrValueKind::Float:
    case IrValueKind::Count:
        CODEGEN_ASSERT(!"Invalid operand restore value kind");
        break;
    case IrValueKind::Tag:
        return offsetof(TValue, tt);
    case IrValueKind::Int:
        return offsetof(TValue, value);
    case IrValueKind::Int64:
        return offsetof(TValue, value.l);
    case IrValueKind::Pointer:
        return offsetof(TValue, value.gc);
    case IrValueKind::Double:
        return offsetof(TValue, value.n);
    case IrValueKind::Tvalue:
        return 0;
    }

    CODEGEN_ASSERT(!"Invalid operand restore value kind");
    LUAU_UNREACHABLE();
}

static AddressA64 getReloadAddress(ValueRestoreLocation location)
{
    IrOp op = location.op;

    if (op.kind == IrOpKind::VmReg)
        return mem(rBase, vmRegOp(op) * sizeof(TValue) + getReloadOffset(location.kind));

    // loads are 4/8/16 bytes; we conservatively limit the offset to fit assuming a 4b index
    if (op.kind == IrOpKind::VmConst && vmConstOp(op) * sizeof(TValue) <= AddressA64::kMaxOffset * 4)
        return mem(rConstants, vmConstOp(op) * sizeof(TValue) + getReloadOffset(location.kind));

    return AddressA64(xzr); // dummy
}

IrRegAllocA64::IrRegAllocA64(
    AssemblyBuilderA64& build,
    IrFunction& function,
    LoweringStats* stats,
    std::initializer_list<std::pair<RegisterA64, RegisterA64>> regs
)
    : build(build)
    , function(function)
    , stats(stats)
{
    for (auto& p : regs)
    {
        CODEGEN_ASSERT(p.first.kind == p.second.kind && p.first.index <= p.second.index);

        Set& set = getSet(p.first.kind);

        for (int i = p.first.index; i <= p.second.index; ++i)
            set.base |= 1u << i;
    }

    gpr.free = gpr.base;
    simd.free = simd.base;

    memset(gpr.defs, -1, sizeof(gpr.defs));
    memset(simd.defs, -1, sizeof(simd.defs));

    CODEGEN_ASSERT(kSpillSlots + kExtraSpillSlots < 64);
    freeSpillSlots = (1ull << (kSpillSlots + kExtraSpillSlots)) - 1ull;
}

RegisterA64 IrRegAllocA64::allocReg(KindA64 kind, uint32_t index)
{
    Set& set = getSet(kind);

    if (set.free == 0)
    {
        // Try to find and spill a register that is not used in the current instruction and has the furthest next use
        if (uint32_t furthestUseTarget = findInstructionWithFurthestNextUse(set); furthestUseTarget != kInvalidInstIdx)
        {
            spill(set, index, furthestUseTarget);
            CODEGEN_ASSERT(set.free != 0);
        }
        else
        {
            error = true;
            return RegisterA64{kind, 0};
        }
    }

    int reg = 31 - countlz(set.free);

    if (FFlag::DebugCodegenChaosA64)
        reg = countrz(set.free); // allocate from low end; this causes extra conflicts for calls

    set.free &= ~(1u << reg);
    set.defs[reg] = index;

    return RegisterA64{kind, uint8_t(reg)};
}

RegisterA64 IrRegAllocA64::allocTemp(KindA64 kind)
{
    Set& set = getSet(kind);

    if (set.free == 0)
    {
        // Try to find and spill a register that is not used in the current instruction and has the furthest next use
        if (uint32_t furthestUseTarget = findInstructionWithFurthestNextUse(set); furthestUseTarget != kInvalidInstIdx)
        {
            spill(set, currInstIdx, furthestUseTarget);
            CODEGEN_ASSERT(set.free != 0);
        }
        else
        {
            error = true;
            return RegisterA64{kind, 0};
        }
    }

    int reg = 31 - countlz(set.free);

    if (FFlag::DebugCodegenChaosA64)
        reg = countrz(set.free); // allocate from low end; this causes extra conflicts for calls

    set.free &= ~(1u << reg);
    set.temp |= 1u << reg;
    CODEGEN_ASSERT(set.defs[reg] == kInvalidInstIdx);

    return RegisterA64{kind, uint8_t(reg)};
}

RegisterA64 IrRegAllocA64::allocReuse(KindA64 kind, uint32_t index, std::initializer_list<IrOp> oprefs)
{
    for (IrOp op : oprefs)
    {
        if (op.kind != IrOpKind::Inst)
            continue;

        IrInst& source = function.instructions[op.index];

        if (source.lastUse == index && !source.reusedReg && source.regA64 != noreg)
        {
            CODEGEN_ASSERT(!source.spilled && !source.needsReload);
            CODEGEN_ASSERT(source.regA64.kind == kind);

            Set& set = getSet(kind);
            CODEGEN_ASSERT(set.defs[source.regA64.index] == op.index);
            set.defs[source.regA64.index] = index;

            source.reusedReg = true;
            return source.regA64;
        }
    }

    return allocReg(kind, index);
}

RegisterA64 IrRegAllocA64::takeReg(RegisterA64 reg, uint32_t index)
{
    Set& set = getSet(reg.kind);

    CODEGEN_ASSERT(set.free & (1u << reg.index));
    CODEGEN_ASSERT(set.defs[reg.index] == kInvalidInstIdx);

    set.free &= ~(1u << reg.index);
    set.defs[reg.index] = index;

    return reg;
}

void IrRegAllocA64::freeReg(RegisterA64 reg)
{
    Set& set = getSet(reg.kind);

    CODEGEN_ASSERT((set.base & (1u << reg.index)) != 0);
    CODEGEN_ASSERT((set.free & (1u << reg.index)) == 0);
    CODEGEN_ASSERT((set.temp & (1u << reg.index)) == 0);

    set.free |= 1u << reg.index;
    set.defs[reg.index] = kInvalidInstIdx;
}

void IrRegAllocA64::freeLastUseReg(IrInst& target, uint32_t index)
{
    if (target.lastUse == index && !target.reusedReg)
    {
        CODEGEN_ASSERT(!target.spilled && !target.needsReload);

        // Register might have already been freed if it had multiple uses inside a single instruction
        if (target.regA64 == noreg)
            return;

        freeReg(target.regA64);
        target.regA64 = noreg;
    }
}

void IrRegAllocA64::freeLastUseRegs(const IrInst& inst, uint32_t index)
{
    auto checkOp = [this, index](IrOp op)
    {
        if (op.kind == IrOpKind::Inst)
            freeLastUseReg(function.instructions[op.index], index);
    };

    for (const IrOp& op : inst.ops)
        checkOp(op);
}

void IrRegAllocA64::freeTemp(RegisterA64 reg)
{
    Set& set = getSet(reg.kind);

    CODEGEN_ASSERT((set.base & (1u << reg.index)) != 0);
    CODEGEN_ASSERT((set.free & (1u << reg.index)) == 0);
    CODEGEN_ASSERT((set.temp & (1u << reg.index)) != 0);

    set.free |= 1u << reg.index;
    set.temp &= ~(1u << reg.index);
}

void IrRegAllocA64::freeTempRegs()
{
    CODEGEN_ASSERT((gpr.free & gpr.temp) == 0);
    gpr.free |= gpr.temp;
    gpr.temp = 0;

    CODEGEN_ASSERT((simd.free & simd.temp) == 0);
    simd.free |= simd.temp;
    simd.temp = 0;
}

size_t IrRegAllocA64::spill(uint32_t index, std::initializer_list<RegisterA64> live)
{
    static const KindA64 sets[] = {KindA64::x, KindA64::q};

    size_t start = spills.size();

    uint32_t poisongpr = 0;
    uint32_t poisonsimd = 0;

    if (FFlag::DebugCodegenChaosA64)
    {
        poisongpr = gpr.base & ~gpr.free;
        poisonsimd = simd.base & ~simd.free;

        for (RegisterA64 reg : live)
        {
            Set& set = getSet(reg.kind);
            (&set == &simd ? poisonsimd : poisongpr) &= ~(1u << reg.index);
        }
    }

    for (KindA64 kind : sets)
    {
        Set& set = getSet(kind);

        // early-out
        if (set.free == set.base)
            continue;

        // free all temp registers
        CODEGEN_ASSERT((set.free & set.temp) == 0);
        set.free |= set.temp;
        set.temp = 0;

        // spill all allocated registers unless they aren't used anymore
        uint32_t regs = set.base & ~set.free;

        while (regs)
        {
            int reg = 31 - countlz(regs);

            uint32_t targetInstIdx = set.defs[reg];

            CODEGEN_ASSERT(targetInstIdx != kInvalidInstIdx);
            CODEGEN_ASSERT(function.instructions[targetInstIdx].regA64.index == reg);

            spill(set, index, targetInstIdx);

            regs &= ~(1u << reg);
        }

        CODEGEN_ASSERT(set.free == set.base);
    }

    if (FFlag::DebugCodegenChaosA64)
    {
        for (int reg = 0; reg < 32; ++reg)
        {
            if (poisongpr & (1u << reg))
                build.mov(RegisterA64{KindA64::x, uint8_t(reg)}, 0xdead);
            if (poisonsimd & (1u << reg))
                build.fmov(RegisterA64{KindA64::d, uint8_t(reg)}, -0.125);
        }
    }

    return start;
}

void IrRegAllocA64::restore(size_t start)
{
    CODEGEN_ASSERT(start <= spills.size());

    if (start < spills.size())
    {
        for (size_t i = start; i < spills.size(); ++i)
        {
            Spill s = spills[i]; // copy in case takeReg reallocates spills
            RegisterA64 reg = takeReg(s.origin, s.inst);

            restore(s, reg);
        }

        spills.resize(start);
    }
}

void IrRegAllocA64::restoreReg(IrInst& inst)
{
    uint32_t index = function.getInstIndex(inst);

    for (size_t i = 0; i < spills.size(); ++i)
    {
        if (spills[i].inst == index)
        {
            Spill s = spills[i]; // copy in case allocReg reallocates spills
            RegisterA64 reg = allocReg(s.origin.kind, index);

            restore(s, reg);

            spills[i] = spills.back();
            spills.pop_back();
            return;
        }
    }

    CODEGEN_ASSERT(!"Expected to find a spill record");
}

void IrRegAllocA64::restore(const IrRegAllocA64::Spill& s, RegisterA64 reg)
{
    IrInst& inst = function.instructions[s.inst];
    CODEGEN_ASSERT(inst.regA64 == noreg);

    if (s.slot >= 0)
    {
        if (isExtraSpillSlot(s.slot))
        {
            int extraOffset = getExtraSpillAddressOffset(s.slot);

            // Need to calculate an address, but everything might be taken
            // If we are restoring an integer register, we can just use it as a temporary
            RegisterA64 emergencyTemp = reg.kind == KindA64::w ? castReg(KindA64::x, reg) : (reg.kind == KindA64::x ? reg : x17);

            if (reg.kind != KindA64::w && reg.kind != KindA64::x)
                build.str(emergencyTemp, sTemporary);

            build.ldr(emergencyTemp, mem(rState, offsetof(lua_State, global)));
            build.ldr(emergencyTemp, mem(emergencyTemp, offsetof(global_State, ecbdata)));

            build.ldr(reg, mem(emergencyTemp, extraOffset));

            if (reg.kind != KindA64::w && reg.kind != KindA64::x)
                build.ldr(emergencyTemp, sTemporary);
        }
        else
        {
            build.ldr(reg, mem(sp, sSpillArea.data + s.slot * 8));
        }

        if (s.slot != kInvalidSpill)
            freeSpill(freeSpillSlots, reg.kind, s.slot);
    }
    else
    {
        CODEGEN_ASSERT(!inst.spilled && inst.needsReload);

        // When restoring the value, we allow cross-block restore because we have commited to the target location at spill time
        ValueRestoreLocation restoreLocation = function.findRestoreLocation(inst, /*limitToCurrentBlock*/ false);

        AddressA64 addr = getReloadAddress(restoreLocation);
        CODEGEN_ASSERT(addr.base != xzr);

        IrValueKind spillValueKind = getCmdValueKind(inst.cmd);

        if (spillValueKind == IrValueKind::Int && restoreLocation.kind == IrValueKind::Double)
        {
            // Handle restore of an int/uint value from a location storing a double number
            RegisterA64 temp = allocTemp(KindA64::d);
            build.ldr(temp, addr);

            if (restoreLocation.conversionCmd == IrCmd::INT_TO_NUM)
                build.fcvtzs(reg, temp);
            else if (restoreLocation.conversionCmd == IrCmd::UINT_TO_NUM)
                build.fcvtzs(castReg(KindA64::x, reg), temp); // note: we don't use fcvtzu for consistency with C++ code
            else
                CODEGEN_ASSERT(!"re-materialization not supported for this conversion command");

            // Temporary might have taken a spot needed for other registers in spill restore process
            freeTemp(temp);
        }
        else
        {
            build.ldr(reg, addr);
        }
    }

    inst.spilled = false;
    inst.needsReload = false;
    inst.regA64 = reg;
}

void IrRegAllocA64::spill(Set& set, uint32_t index, uint32_t targetInstIdx)
{
    IrInst& def = function.instructions[targetInstIdx];
    int reg = def.regA64.index;

    CODEGEN_ASSERT(!def.reusedReg);
    CODEGEN_ASSERT(!def.spilled);
    CODEGEN_ASSERT(!def.needsReload);

    if (def.lastUse == index)
    {
        // instead of spilling the register to never reload it, we assume the register is not needed anymore
    }
    else if (function.hasRestoreLocation(def, /*limitToCurrentBlock*/ true))
    {
        // when checking if value has a restore operation to spill it, we only allow it in the same block
        // instead of spilling the register to stack, we can reload it from VM stack/constants
        // we still need to record the spill for restore(start) to work
        Spill s = {targetInstIdx, def.regA64, -1};
        spills.push_back(s);

        def.needsReload = true;

        if (stats)
            stats->spillsToRestore++;
    }
    else
    {
        int slot = allocSpill(freeSpillSlots, def.regA64.kind);
        if (slot < 0)
        {
            slot = kInvalidSpill;
            error = true;
        }

        if (isExtraSpillSlot(slot))
        {
            int extraOffset = getExtraSpillAddressOffset(slot);

            // Tricky situation, no registers left, but need a register to calculate an address
            // We will try to take x17 unless it's actually the register being spilled
            RegisterA64 emergencyTemp = def.regA64 == x17 || def.regA64 == w17 ? x16 : x17;
            build.str(emergencyTemp, sTemporary);

            build.ldr(emergencyTemp, mem(rState, offsetof(lua_State, global)));
            build.ldr(emergencyTemp, mem(emergencyTemp, offsetof(global_State, ecbdata)));

            build.str(def.regA64, mem(emergencyTemp, extraOffset));

            build.ldr(emergencyTemp, sTemporary);
        }
        else
        {
            build.str(def.regA64, mem(sp, sSpillArea.data + slot * 8));
        }

        Spill s = {targetInstIdx, def.regA64, int8_t(slot)};
        spills.push_back(s);

        def.spilled = true;

        if (stats)
        {
            stats->spillsToSlot++;

            if (slot != kInvalidSpill && unsigned(slot + 1) > stats->maxSpillSlotsUsed)
                stats->maxSpillSlotsUsed = slot + 1;
        }
    }

    def.regA64 = noreg;

    set.free |= 1u << reg;
    set.defs[reg] = kInvalidInstIdx;
}

uint32_t IrRegAllocA64::findInstructionWithFurthestNextUse(Set& set) const
{
    if (currInstIdx == kInvalidInstIdx)
        return kInvalidInstIdx;

    uint32_t furthestUseTarget = kInvalidInstIdx;
    uint32_t furthestUseLocation = 0;

    for (uint32_t regInstUser : set.defs)
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


bool IrRegAllocA64::isExtraSpillSlot(unsigned slot) const
{
    return slot >= kSpillSlots;
}

int IrRegAllocA64::getExtraSpillAddressOffset(unsigned slot) const
{
    CODEGEN_ASSERT(isExtraSpillSlot(slot));

    return (slot - kSpillSlots) * 8;
}

IrRegAllocA64::Set& IrRegAllocA64::getSet(KindA64 kind)
{
    switch (kind)
    {
    case KindA64::x:
    case KindA64::w:
        return gpr;

    case KindA64::s:
    case KindA64::d:
    case KindA64::q:
        return simd;

    default:
        CODEGEN_ASSERT(!"Unexpected register kind");
        LUAU_UNREACHABLE();
    }
}

} // namespace A64
} // namespace CodeGen
} // namespace Luau
