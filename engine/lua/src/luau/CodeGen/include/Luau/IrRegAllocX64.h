// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrData.h"
#include "Luau/RegisterX64.h"

#include <array>
#include <initializer_list>

namespace Luau
{
namespace CodeGen
{

struct LoweringStats;

namespace X64
{

constexpr uint8_t kNoStackSlot = 0xff;

struct IrSpillX64
{
    uint32_t instIdx = 0;
    IrValueKind valueKind = IrValueKind::Unknown;

    unsigned spillId = 0;

    // Spill location can be a stack location or be empty
    // When it's empty, it means that instruction value can be rematerialized
    uint8_t stackSlot = kNoStackSlot;

    RegisterX64 originalLoc = noreg;
};

struct IrRegAllocX64
{
    IrRegAllocX64(AssemblyBuilderX64& build, IrFunction& function, LoweringStats* stats);

    RegisterX64 allocReg(SizeX64 size, uint32_t instIdx);
    RegisterX64 allocRegOrReuse(SizeX64 size, uint32_t instIdx, std::initializer_list<IrOp> oprefs);
    RegisterX64 takeReg(RegisterX64 reg, uint32_t instIdx);

    bool canTakeReg(RegisterX64 reg) const;

    void freeReg(RegisterX64 reg);
    void freeLastUseReg(IrInst& target, uint32_t instIdx);
    void freeLastUseRegs(const IrInst& inst, uint32_t instIdx);

    bool isLastUseReg(const IrInst& target, uint32_t instIdx) const;

    bool shouldFreeGpr(RegisterX64 reg) const;

    unsigned findSpillStackSlot(IrValueKind valueKind);

    OperandX64 getRestoreAddress(const IrInst& inst, ValueRestoreLocation restoreLocation);

    // Register used by instruction is about to be freed, have to find a way to restore value later
    void preserve(IrInst& inst);

    void restore(IrInst& inst, bool intoOriginalLocation);

    void preserveAndFreeInstValues();

    uint32_t findInstructionWithFurthestNextUse(const std::array<uint32_t, 16>& regInstUsers) const;

    bool isExtraSpillSlot(unsigned slot) const;
    int getExtraSpillAddressOffset(unsigned slot) const;

    void assertFree(RegisterX64 reg) const;
    void assertAllFree() const;
    void assertNoSpills() const;

    AssemblyBuilderX64& build;
    IrFunction& function;
    LoweringStats* stats = nullptr;

    uint32_t currInstIdx = ~0u;

    std::array<bool, 16> freeGprMap;
    std::array<uint32_t, 16> gprInstUsers;
    std::array<bool, 16> freeXmmMap;
    std::array<uint32_t, 16> xmmInstUsers;
    uint8_t usableXmmRegCount = 0;

    std::bitset<512> usedSpillSlotHalfs; // A bit for every stack slot split in 4 byte halfs
    unsigned maxUsedSlot = 0;            // Maximum number of 8 byte stack slots used

    unsigned nextSpillId = 1;
    std::vector<IrSpillX64> spills;
};

struct ScopedRegX64
{
    explicit ScopedRegX64(IrRegAllocX64& owner);
    ScopedRegX64(IrRegAllocX64& owner, SizeX64 size);
    ScopedRegX64(IrRegAllocX64& owner, RegisterX64 reg);
    ~ScopedRegX64();

    ScopedRegX64(const ScopedRegX64&) = delete;
    ScopedRegX64& operator=(const ScopedRegX64&) = delete;

    void take(RegisterX64 reg);
    void alloc(SizeX64 size);
    void free();

    RegisterX64 release();

    IrRegAllocX64& owner;
    RegisterX64 reg;
};

// When IR instruction makes a call under a condition that's not reflected as a real branch in IR,
// spilled values have to be restored to their exact original locations, so that both after a call
// and after the skip, values are found in the same place
struct ScopedSpills
{
    explicit ScopedSpills(IrRegAllocX64& owner);
    ~ScopedSpills();

    ScopedSpills(const ScopedSpills&) = delete;
    ScopedSpills& operator=(const ScopedSpills&) = delete;

    IrRegAllocX64& owner;
    unsigned startSpillId = 0;
};

} // namespace X64
} // namespace CodeGen
} // namespace Luau
