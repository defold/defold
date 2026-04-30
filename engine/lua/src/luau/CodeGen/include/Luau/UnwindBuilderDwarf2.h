// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/RegisterX64.h"
#include "UnwindBuilder.h"

#include <vector>

namespace Luau
{
namespace CodeGen
{

struct UnwindFunctionDwarf2
{
    uint32_t beginOffset;
    uint32_t endOffset;
    uint32_t fdeEntryStartPos;
};

class UnwindBuilderDwarf2 : public UnwindBuilder
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

    std::vector<UnwindFunctionDwarf2> unwindFunctions;

    static const unsigned kRawDataLimit = 1024;
    uint8_t rawData[kRawDataLimit];
    uint8_t* pos = rawData;

    // We will remember the FDE location to write some of the fields like entry length, function start and size later
    uint8_t* fdeEntryStart = nullptr;
};

} // namespace CodeGen
} // namespace Luau
