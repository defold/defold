// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace Luau
{
namespace CodeGen
{

struct BlockLinearizationStats
{
    unsigned int constPropInstructionCount = 0;
    double timeSeconds = 0.0;

    BlockLinearizationStats& operator+=(const BlockLinearizationStats& that)
    {
        this->constPropInstructionCount += that.constPropInstructionCount;
        this->timeSeconds += that.timeSeconds;

        return *this;
    }

    BlockLinearizationStats operator+(const BlockLinearizationStats& other) const
    {
        BlockLinearizationStats result(*this);
        result += other;
        return result;
    }
};

enum FunctionStatsFlags
{
    // Enable stats collection per function
    FunctionStats_Enable = 1 << 0,
    // Compute function bytecode summary
    FunctionStats_BytecodeSummary = 1 << 1,
};

struct FunctionStats
{
    std::string name;
    int line = -1;
    unsigned bcodeCount = 0;
    unsigned irCount = 0;
    unsigned asmCount = 0;
    unsigned asmSize = 0;
    std::vector<std::vector<unsigned>> bytecodeSummary;
};

struct LoweringStats
{
    unsigned totalFunctions = 0;
    unsigned skippedFunctions = 0;
    int spillsToSlot = 0;
    int spillsToRestore = 0;
    unsigned maxSpillSlotsUsed = 0;
    unsigned blocksPreOpt = 0;
    unsigned blocksPostOpt = 0;
    unsigned maxBlockInstructions = 0;

    int regAllocErrors = 0;
    int loweringErrors = 0;

    BlockLinearizationStats blockLinearizationStats;

    unsigned functionStatsFlags = 0;
    std::vector<FunctionStats> functions;

    LoweringStats operator+(const LoweringStats& other) const
    {
        LoweringStats result(*this);
        result += other;
        return result;
    }

    LoweringStats& operator+=(const LoweringStats& that)
    {
        this->totalFunctions += that.totalFunctions;
        this->skippedFunctions += that.skippedFunctions;
        this->spillsToSlot += that.spillsToSlot;
        this->spillsToRestore += that.spillsToRestore;
        this->maxSpillSlotsUsed = std::max(this->maxSpillSlotsUsed, that.maxSpillSlotsUsed);
        this->blocksPreOpt += that.blocksPreOpt;
        this->blocksPostOpt += that.blocksPostOpt;
        this->maxBlockInstructions = std::max(this->maxBlockInstructions, that.maxBlockInstructions);

        this->regAllocErrors += that.regAllocErrors;
        this->loweringErrors += that.loweringErrors;

        this->blockLinearizationStats += that.blockLinearizationStats;

        if (this->functionStatsFlags & FunctionStats_Enable)
            this->functions.insert(this->functions.end(), that.functions.begin(), that.functions.end());

        return *this;
    }
};

} // namespace CodeGen
} // namespace Luau
