// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/UnwindBuilderWin.h"

#include <string.h>

// Information about the Windows x64 unwinding data setup can be found at:
// https://docs.microsoft.com/en-us/cpp/build/exception-handling-x64 [x64 exception handling]

#define UWOP_PUSH_NONVOL 0
#define UWOP_ALLOC_LARGE 1
#define UWOP_ALLOC_SMALL 2
#define UWOP_SET_FPREG 3
#define UWOP_SAVE_NONVOL 4
#define UWOP_SAVE_NONVOL_FAR 5
#define UWOP_SAVE_XMM128 8
#define UWOP_SAVE_XMM128_FAR 9
#define UWOP_PUSH_MACHFRAME 10

namespace Luau
{
namespace CodeGen
{

void UnwindBuilderWin::setBeginOffset(size_t beginOffset)
{
    this->beginOffset = beginOffset;
}

size_t UnwindBuilderWin::getBeginOffset() const
{
    return beginOffset;
}

void UnwindBuilderWin::startInfo(Arch arch)
{
    CODEGEN_ASSERT(arch == X64);
}

void UnwindBuilderWin::startFunction()
{
    // End offset is filled in later and everything gets adjusted at the end
    UnwindFunctionWin func;
    func.beginOffset = 0;
    func.endOffset = 0;
    func.unwindInfoOffset = uint32_t(rawDataPos - rawData);
    unwindFunctions.push_back(func);

    unwindCodes.clear();
    unwindCodes.reserve(16);

    prologSize = 0;

    // rax has register index 0, which in Windows unwind info means that frame register is not used
    frameReg = X64::rax;
    frameRegOffset = 0;
}

void UnwindBuilderWin::finishFunction(uint32_t beginOffset, uint32_t endOffset)
{
    unwindFunctions.back().beginOffset = beginOffset;
    unwindFunctions.back().endOffset = endOffset;

    // Windows unwind code count is stored in uint8_t, so we can't have more
    CODEGEN_ASSERT(unwindCodes.size() < 256);

    UnwindInfoWin info;
    info.version = 1;
    info.flags = 0; // No EH
    info.prologsize = prologSize;
    info.unwindcodecount = uint8_t(unwindCodes.size());

    CODEGEN_ASSERT(frameReg.index < 16);
    info.framereg = frameReg.index;

    CODEGEN_ASSERT(frameRegOffset < 16);
    info.frameregoff = frameRegOffset;

    CODEGEN_ASSERT(rawDataPos + sizeof(info) <= rawData + kRawDataLimit);
    memcpy(rawDataPos, &info, sizeof(info));
    rawDataPos += sizeof(info);

    if (!unwindCodes.empty())
    {
        // Copy unwind codes in reverse order
        // Some unwind codes take up two array slots, we write those in reverse order
        uint8_t* unwindCodePos = rawDataPos + sizeof(UnwindCodeWin) * (unwindCodes.size() - 1);
        CODEGEN_ASSERT(unwindCodePos <= rawData + kRawDataLimit);

        for (size_t i = 0; i < unwindCodes.size(); i++)
        {
            memcpy(unwindCodePos, &unwindCodes[i], sizeof(UnwindCodeWin));
            unwindCodePos -= sizeof(UnwindCodeWin);
        }
    }

    rawDataPos += sizeof(UnwindCodeWin) * unwindCodes.size();

    // Size has to be even, but unwind code count doesn't have to
    if (unwindCodes.size() % 2 != 0)
        rawDataPos += sizeof(UnwindCodeWin);

    CODEGEN_ASSERT(rawDataPos <= rawData + kRawDataLimit);
}

void UnwindBuilderWin::finishInfo() {}

void UnwindBuilderWin::prologueA64(uint32_t prologueSize, uint32_t stackSize, std::initializer_list<A64::RegisterA64> regs)
{
    CODEGEN_ASSERT(!"Not implemented");
}

void UnwindBuilderWin::prologueX64(
    uint32_t prologueSize,
    uint32_t stackSize,
    bool setupFrame,
    std::initializer_list<X64::RegisterX64> gpr,
    const std::vector<X64::RegisterX64>& simd
)
{
    CODEGEN_ASSERT(stackSize > 0 && stackSize < 4096 && stackSize % 8 == 0);
    CODEGEN_ASSERT(prologueSize < 256);

    unsigned int stackOffset = 8; // Return address was pushed by calling the function
    unsigned int prologueOffset = 0;

    if (setupFrame)
    {
        // push rbp
        stackOffset += 8;
        prologueOffset += 2;
        unwindCodes.push_back({uint8_t(prologueOffset), UWOP_PUSH_NONVOL, X64::rbp.index});

        // mov rbp, rsp
        prologueOffset += 3;
        frameReg = X64::rbp;
        frameRegOffset = 0;
        unwindCodes.push_back({uint8_t(prologueOffset), UWOP_SET_FPREG, frameRegOffset});
    }

    // push reg
    for (X64::RegisterX64 reg : gpr)
    {
        CODEGEN_ASSERT(reg.size == X64::SizeX64::qword);

        stackOffset += 8;
        prologueOffset += 2;
        unwindCodes.push_back({uint8_t(prologueOffset), UWOP_PUSH_NONVOL, reg.index});
    }

    // If frame pointer is used, simd register storage is not implemented, it will require reworking store offsets
    CODEGEN_ASSERT(!setupFrame || simd.size() == 0);

    unsigned int simdStorageSize = unsigned(simd.size()) * 16;

    // It's the responsibility of the caller to provide simd register storage in 'stackSize', including alignment to 16 bytes
    if (!simd.empty() && stackOffset % 16 == 8)
        simdStorageSize += 8;

    // sub rsp, stackSize
    if (stackSize <= 128)
    {
        stackOffset += stackSize;
        prologueOffset += stackSize == 128 ? 7 : 4;
        unwindCodes.push_back({uint8_t(prologueOffset), UWOP_ALLOC_SMALL, uint8_t((stackSize - 8) / 8)});
    }
    else
    {
        // This command can handle allocations up to 512K-8 bytes, but that potentially requires stack probing
        CODEGEN_ASSERT(stackSize < 4096);

        stackOffset += stackSize;
        prologueOffset += 7;

        uint16_t encodedOffset = stackSize / 8;
        unwindCodes.push_back(UnwindCodeWin());
        memcpy(&unwindCodes.back(), &encodedOffset, sizeof(encodedOffset));

        unwindCodes.push_back({uint8_t(prologueOffset), UWOP_ALLOC_LARGE, 0});
    }

    // It's the responsibility of the caller to provide simd register storage in 'stackSize'
    unsigned int xmmStoreOffset = stackSize - simdStorageSize;

    // vmovaps [rsp+n], xmm
    for (X64::RegisterX64 reg : simd)
    {
        CODEGEN_ASSERT(reg.size == X64::SizeX64::xmmword);
        CODEGEN_ASSERT(xmmStoreOffset % 16 == 0 && "simd stores have to be performed to aligned locations");

        prologueOffset += xmmStoreOffset >= 128 ? 10 : 7;
        unwindCodes.push_back({uint8_t(xmmStoreOffset / 16), 0, 0});
        unwindCodes.push_back({uint8_t(prologueOffset), UWOP_SAVE_XMM128, reg.index});
        xmmStoreOffset += 16;
    }

    CODEGEN_ASSERT(stackOffset % 16 == 0);
    CODEGEN_ASSERT(prologueOffset == prologueSize);

    this->prologSize = prologueSize;
}

size_t UnwindBuilderWin::getUnwindInfoSize(size_t blockSize) const
{
    return sizeof(UnwindFunctionWin) * unwindFunctions.size() + size_t(rawDataPos - rawData);
}

size_t UnwindBuilderWin::finalize(char* target, size_t offset, void* funcAddress, size_t blockSize) const
{
    // Copy adjusted function information
    for (UnwindFunctionWin func : unwindFunctions)
    {
        // Code will start after the unwind info
        func.beginOffset += uint32_t(offset);

        // Whole block is a part of a 'single function'
        if (func.endOffset == kFullBlockFunction)
            func.endOffset = uint32_t(blockSize);
        else
            func.endOffset += uint32_t(offset);

        // Unwind data is placed right after the RUNTIME_FUNCTION data
        func.unwindInfoOffset += uint32_t(sizeof(UnwindFunctionWin) * unwindFunctions.size());
        memcpy(target, &func, sizeof(func));
        target += sizeof(func);
    }

    // Copy unwind codes
    memcpy(target, rawData, size_t(rawDataPos - rawData));

    return unwindFunctions.size();
}

} // namespace CodeGen
} // namespace Luau
