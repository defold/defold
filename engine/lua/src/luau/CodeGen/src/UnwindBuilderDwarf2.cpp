// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/UnwindBuilderDwarf2.h"

#include "ByteUtils.h"

#include <string.h>

// General information about Dwarf2 format can be found at:
// https://dwarfstd.org/doc/dwarf-2.0.0.pdf [DWARF Debugging Information Format]
// Main part for async exception unwinding is in section '6.4 Call Frame Information'

// Information about System V ABI (AMD64) can be found at:
// https://refspecs.linuxbase.org/elf/x86_64-abi-0.99.pdf [System V Application Binary Interface (AMD64 Architecture Processor Supplement)]
// Interaction between Dwarf2 and System V ABI can be found in sections '3.6.2 DWARF Register Number Mapping' and '4.2.4 EH_FRAME sections'

// Call frame instruction opcodes (Dwarf2, page 78, ch. 7.23 figure 37)
#define DW_CFA_advance_loc 0x40
#define DW_CFA_offset 0x80
#define DW_CFA_restore 0xc0
#define DW_CFA_set_loc 0x01
#define DW_CFA_advance_loc1 0x02
#define DW_CFA_advance_loc2 0x03
#define DW_CFA_advance_loc4 0x04
#define DW_CFA_offset_extended 0x05
#define DW_CFA_restore_extended 0x06
#define DW_CFA_undefined 0x07
#define DW_CFA_same_value 0x08
#define DW_CFA_register 0x09
#define DW_CFA_remember_state 0x0a
#define DW_CFA_restore_state 0x0b
#define DW_CFA_def_cfa 0x0c
#define DW_CFA_def_cfa_register 0x0d
#define DW_CFA_def_cfa_offset 0x0e
#define DW_CFA_def_cfa_expression 0x0f
#define DW_CFA_nop 0x00
#define DW_CFA_lo_user 0x1c
#define DW_CFA_hi_user 0x3f

// Register numbers for X64 (System V ABI, page 57, ch. 3.7, figure 3.36)
#define DW_REG_X64_RAX 0
#define DW_REG_X64_RDX 1
#define DW_REG_X64_RCX 2
#define DW_REG_X64_RBX 3
#define DW_REG_X64_RSI 4
#define DW_REG_X64_RDI 5
#define DW_REG_X64_RBP 6
#define DW_REG_X64_RSP 7
#define DW_REG_X64_RA 16

// Register numbers for A64 (DWARF for the Arm 64-bit Architecture, ch. 4.1)
#define DW_REG_A64_FP 29
#define DW_REG_A64_LR 30
#define DW_REG_A64_SP 31

// X64 register mapping from real register index to DWARF2 (r8..r15 are mapped 1-1, but named registers aren't)
const int regIndexToDwRegX64[16] = {
    DW_REG_X64_RAX,
    DW_REG_X64_RCX,
    DW_REG_X64_RDX,
    DW_REG_X64_RBX,
    DW_REG_X64_RSP,
    DW_REG_X64_RBP,
    DW_REG_X64_RSI,
    DW_REG_X64_RDI,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15
};

const int kCodeAlignFactor = 1;
const int kDataAlignFactor = 8;
const int kDwarfAlign = 8;
const int kFdeInitialLocationOffset = 8;
const int kFdeAddressRangeOffset = 16;

// Define canonical frame address expression as [reg + offset]
static uint8_t* defineCfaExpression(uint8_t* pos, int dwReg, uint32_t stackOffset)
{
    pos = writeu8(pos, DW_CFA_def_cfa);
    pos = writeuleb128(pos, dwReg);
    pos = writeuleb128(pos, stackOffset);
    return pos;
}

// Update offset value in canonical frame address expression
static uint8_t* defineCfaExpressionOffset(uint8_t* pos, uint32_t stackOffset)
{
    pos = writeu8(pos, DW_CFA_def_cfa_offset);
    pos = writeuleb128(pos, stackOffset);
    return pos;
}

static uint8_t* defineSavedRegisterLocation(uint8_t* pos, int dwReg, uint32_t stackOffset)
{
    CODEGEN_ASSERT(stackOffset % kDataAlignFactor == 0 && "stack offsets have to be measured in kDataAlignFactor units");

    if (dwReg <= 0x3f)
    {
        pos = writeu8(pos, DW_CFA_offset + dwReg);
    }
    else
    {
        pos = writeu8(pos, DW_CFA_offset_extended);
        pos = writeuleb128(pos, dwReg);
    }

    pos = writeuleb128(pos, stackOffset / kDataAlignFactor);
    return pos;
}

static uint8_t* advanceLocation(uint8_t* pos, unsigned int offset)
{
    CODEGEN_ASSERT(offset < 256);
    pos = writeu8(pos, DW_CFA_advance_loc1);
    pos = writeu8(pos, offset);
    return pos;
}

static uint8_t* alignPosition(uint8_t* start, uint8_t* pos)
{
    size_t size = pos - start;
    size_t pad = ((size + kDwarfAlign - 1) & ~(kDwarfAlign - 1)) - size;

    for (size_t i = 0; i < pad; i++)
        pos = writeu8(pos, DW_CFA_nop);

    return pos;
}

namespace Luau
{
namespace CodeGen
{

void UnwindBuilderDwarf2::setBeginOffset(size_t beginOffset)
{
    this->beginOffset = beginOffset;
}

size_t UnwindBuilderDwarf2::getBeginOffset() const
{
    return beginOffset;
}

void UnwindBuilderDwarf2::startInfo(Arch arch)
{
    CODEGEN_ASSERT(arch == A64 || arch == X64);

    uint8_t* cieLength = pos;
    pos = writeu32(pos, 0); // Length (to be filled later)

    pos = writeu32(pos, 0); // CIE id. 0 -- .eh_frame
    pos = writeu8(pos, 1);  // Version

    pos = writeu8(pos, 0); // CIE augmentation String ""

    int ra = arch == A64 ? DW_REG_A64_LR : DW_REG_X64_RA;

    pos = writeuleb128(pos, kCodeAlignFactor);         // Code align factor
    pos = writeuleb128(pos, -kDataAlignFactor & 0x7f); // Data align factor of (as signed LEB128)
    pos = writeu8(pos, ra);                            // Return address register

    // Optional CIE augmentation section (not present)

    // Call frame instructions (common for all FDEs)
    if (arch == A64)
    {
        pos = defineCfaExpression(pos, DW_REG_A64_SP, 0); // Define CFA to be the sp
    }
    else
    {
        pos = defineCfaExpression(pos, DW_REG_X64_RSP, 8);        // Define CFA to be the rsp + 8
        pos = defineSavedRegisterLocation(pos, DW_REG_X64_RA, 8); // Define return address register (RA) to be located at CFA - 8
    }

    pos = alignPosition(cieLength, pos);
    writeu32(cieLength, unsigned(pos - cieLength - 4)); // Length field itself is excluded from length
}

void UnwindBuilderDwarf2::startFunction()
{
    // End offset is filled in later and everything gets adjusted at the end
    UnwindFunctionDwarf2 func;
    func.beginOffset = 0;
    func.endOffset = 0;
    func.fdeEntryStartPos = uint32_t(pos - rawData);
    unwindFunctions.push_back(func);

    fdeEntryStart = pos;                          // Will be written at the end
    pos = writeu32(pos, 0);                       // Length (to be filled later)
    pos = writeu32(pos, unsigned(pos - rawData)); // CIE pointer
    pos = writeu64(pos, 0);                       // Initial location (to be filled later)
    pos = writeu64(pos, 0);                       // Address range (to be filled later)

    // Optional CIE augmentation section (not present)

    // Function call frame instructions to follow
}

void UnwindBuilderDwarf2::finishFunction(uint32_t beginOffset, uint32_t endOffset)
{
    unwindFunctions.back().beginOffset = beginOffset;
    unwindFunctions.back().endOffset = endOffset;

    CODEGEN_ASSERT(fdeEntryStart != nullptr);

    pos = alignPosition(fdeEntryStart, pos);
    writeu32(fdeEntryStart, unsigned(pos - fdeEntryStart - 4)); // Length field itself is excluded from length
}

void UnwindBuilderDwarf2::finishInfo()
{
    // Terminate section
    pos = writeu32(pos, 0);

    CODEGEN_ASSERT(getUnwindInfoSize() <= kRawDataLimit);
}

void UnwindBuilderDwarf2::prologueA64(uint32_t prologueSize, uint32_t stackSize, std::initializer_list<A64::RegisterA64> regs)
{
    CODEGEN_ASSERT(stackSize % 16 == 0);
    CODEGEN_ASSERT(regs.size() >= 2 && regs.begin()[0] == A64::x29 && regs.begin()[1] == A64::x30);
    CODEGEN_ASSERT(regs.size() * 8 <= stackSize);

    // sub sp, sp, stackSize
    pos = advanceLocation(pos, 4);
    pos = defineCfaExpressionOffset(pos, stackSize);

    // stp/str to store each register to stack in order
    pos = advanceLocation(pos, prologueSize - 4);

    for (size_t i = 0; i < regs.size(); ++i)
    {
        CODEGEN_ASSERT(regs.begin()[i].kind == A64::KindA64::x);
        pos = defineSavedRegisterLocation(pos, regs.begin()[i].index, stackSize - unsigned(i * 8));
    }
}

void UnwindBuilderDwarf2::prologueX64(
    uint32_t prologueSize,
    uint32_t stackSize,
    bool setupFrame,
    std::initializer_list<X64::RegisterX64> gpr,
    const std::vector<X64::RegisterX64>& simd
)
{
    CODEGEN_ASSERT(stackSize > 0 && stackSize < 4096 && stackSize % 8 == 0);

    unsigned int stackOffset = 8; // Return address was pushed by calling the function
    unsigned int prologueOffset = 0;

    if (setupFrame)
    {
        // push rbp
        stackOffset += 8;
        prologueOffset += 2;
        pos = advanceLocation(pos, 2);
        pos = defineCfaExpressionOffset(pos, stackOffset);
        pos = defineSavedRegisterLocation(pos, DW_REG_X64_RBP, stackOffset);

        // mov rbp, rsp
        prologueOffset += 3;
        pos = advanceLocation(pos, 3);
    }

    // push reg
    for (X64::RegisterX64 reg : gpr)
    {
        CODEGEN_ASSERT(reg.size == X64::SizeX64::qword);

        stackOffset += 8;
        prologueOffset += 2;
        pos = advanceLocation(pos, 2);
        pos = defineCfaExpressionOffset(pos, stackOffset);
        pos = defineSavedRegisterLocation(pos, regIndexToDwRegX64[reg.index], stackOffset);
    }

    CODEGEN_ASSERT(simd.empty());

    // sub rsp, stackSize
    stackOffset += stackSize;
    prologueOffset += stackSize >= 128 ? 7 : 4;
    pos = advanceLocation(pos, 4);
    pos = defineCfaExpressionOffset(pos, stackOffset);

    CODEGEN_ASSERT(stackOffset % 16 == 0);
    CODEGEN_ASSERT(prologueOffset == prologueSize);
}

size_t UnwindBuilderDwarf2::getUnwindInfoSize(size_t blockSize) const
{
    return size_t(pos - rawData);
}

size_t UnwindBuilderDwarf2::finalize(char* target, size_t offset, void* funcAddress, size_t blockSize) const
{
    memcpy(target, rawData, getUnwindInfoSize());

    for (const UnwindFunctionDwarf2& func : unwindFunctions)
    {
        uint8_t* fdeEntry = (uint8_t*)target + func.fdeEntryStartPos;

        writeu64(fdeEntry + kFdeInitialLocationOffset, uintptr_t(funcAddress) + offset + func.beginOffset);

        if (func.endOffset == kFullBlockFunction)
            writeu64(fdeEntry + kFdeAddressRangeOffset, blockSize - offset);
        else
            writeu64(fdeEntry + kFdeAddressRangeOffset, func.endOffset - func.beginOffset);
    }

    return unwindFunctions.size();
}

} // namespace CodeGen
} // namespace Luau
