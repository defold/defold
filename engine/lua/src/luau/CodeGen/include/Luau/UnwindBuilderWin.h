// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/RegisterX64.h"
#include "UnwindBuilder.h"

#include <vector>

namespace Luau
{
namespace CodeGen
{

// This struct matches the layout of x64 RUNTIME_FUNCTION from winnt.h
struct UnwindFunctionWin
{
    uint32_t beginOffset;
    uint32_t endOffset;
    uint32_t unwindInfoOffset;
};

// This struct matches the layout of x64 UNWIND_INFO from ehdata.h
struct UnwindInfoWin
{
    uint8_t version : 3;
    uint8_t flags : 5;
    uint8_t prologsize;
    uint8_t unwindcodecount;
    uint8_t framereg : 4;
    uint8_t frameregoff : 4;
};

// This struct matches the layout of UNWIND_CODE from ehdata.h
struct UnwindCodeWin
{
    uint8_t offset;
    uint8_t opcode : 4;
    uint8_t opinfo : 4;
};

class UnwindBuilderWin : public UnwindBuilder
{
public:
    void setBeginOffset(size_t beginOffset) override;
    size_t getBeginOffset() const override;

    void startInfo(Arch arch) override;
    void startFunction() override;
    void finishFunction(uint32_t beginOffset, uint32_t endOffset) override;
    void finishInfo() override;

    void prologueA64(uint32_t prologueSize, uint32_t stackSize, std::initializer_list<A64::RegisterA64> regs) override;
    void prologueX64(
        uint32_t prologueSize,
        uint32_t stackSize,
        bool setupFrame,
        std::initializer_list<X64::RegisterX64> gpr,
        const std::vector<X64::RegisterX64>& simd
    ) override;

    size_t getUnwindInfoSize(size_t blockSize = 0) const override;

    size_t finalize(char* target, size_t offset, void* funcAddress, size_t blockSize) const override;

private:
    size_t beginOffset = 0;

    static const unsigned kRawDataLimit = 1024;
    uint8_t rawData[kRawDataLimit];
    uint8_t* rawDataPos = rawData;

    std::vector<UnwindFunctionWin> unwindFunctions;

    // Windows unwind codes are written in reverse, so we have to collect them all first
    std::vector<UnwindCodeWin> unwindCodes;

    uint8_t prologSize = 0;
    X64::RegisterX64 frameReg = X64::noreg;
    uint8_t frameRegOffset = 0;
};

} // namespace CodeGen
} // namespace Luau
