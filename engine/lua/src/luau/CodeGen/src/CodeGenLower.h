// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/AssemblyBuilderA64.h"
#include "Luau/AssemblyBuilderX64.h"
#include "Luau/CodeGen.h"
#include "Luau/IrBuilder.h"
#include "Luau/IrDump.h"
#include "Luau/IrUtils.h"
#include "Luau/LoweringStats.h"
#include "Luau/OptimizeConstProp.h"
#include "Luau/OptimizeDeadStore.h"
#include "Luau/OptimizeFinalX64.h"

#include "EmitCommon.h"
#include "IrLoweringA64.h"
#include "IrLoweringX64.h"

#include "lobject.h"
#include "lstate.h"

#include <algorithm>
#include <vector>

LUAU_FASTFLAG(DebugCodegenOptSize)
LUAU_FASTINT(CodegenHeuristicsInstructionLimit)
LUAU_FASTINT(CodegenHeuristicsBlockLimit)
LUAU_FASTINT(CodegenHeuristicsBlockInstructionLimit)

namespace Luau
{
namespace CodeGen
{

inline void gatherFunctionsHelper(
    std::vector<Proto*>& results,
    Proto* proto,
    const unsigned int flags,
    const bool hasNativeFunctions,
    const bool root
)
{
    if (results.size() <= size_t(proto->bytecodeid))
        results.resize(proto->bytecodeid + 1);

    // Skip protos that we've already compiled in this run: this happens because at -O2, inlined functions get their protos reused
    if (results[proto->bytecodeid])
        return;

    // if native module, compile cold functions if requested
    // if not native module, compile function if it has native attribute and is not root
    bool shouldGather = hasNativeFunctions ? (!root && (proto->flags & LPF_NATIVE_FUNCTION) != 0)
                                           : ((proto->flags & LPF_NATIVE_COLD) == 0 || (flags & CodeGen_ColdFunctions) != 0);

    if (shouldGather)
        results[proto->bytecodeid] = proto;

    // Recursively traverse child protos even if we aren't compiling this one
    for (int i = 0; i < proto->sizep; i++)
        gatherFunctionsHelper(results, proto->p[i], flags, hasNativeFunctions, false);
}

inline void gatherFunctions(std::vector<Proto*>& results, Proto* root, const unsigned int flags, const bool hasNativeFunctions = false)
{
    gatherFunctionsHelper(results, root, flags, hasNativeFunctions, true);
}

inline unsigned getInstructionCount(const std::vector<IrInst>& instructions, IrCmd cmd)
{
    return unsigned(std::count_if(
        instructions.begin(),
        instructions.end(),
        [&cmd](const IrInst& inst)
        {
            return inst.cmd == cmd;
        }
    ));
}

template<typename AssemblyBuilder, typename IrLowering>
inline bool lowerImpl(
    AssemblyBuilder& build,
    IrLowering& lowering,
    IrFunction& function,
    const std::vector<uint32_t>& sortedBlocks,
    int bytecodeid,
    AssemblyOptions options
)
{
    // For each IR instruction that begins a bytecode instruction, which bytecode instruction is it?
    std::vector<uint32_t> bcLocations(function.instructions.size() + 1, ~0u);

    for (size_t i = 0; i < function.bcMapping.size(); ++i)
    {
        uint32_t irLocation = function.bcMapping[i].irLocation;

        if (irLocation != ~0u)
            bcLocations[irLocation] = uint32_t(i);
    }

    bool outputEnabled = options.includeAssembly || options.includeIr;

    IrToStringContext ctx{build.text, function.blocks, function.constants, function.cfg, function.proto};

    // We use this to skip outlined fallback blocks from IR/asm text output
    size_t textSize = build.text.length();
    uint32_t codeSize = build.getCodeSize();
    bool seenFallback = false;

    IrBlock dummy;
    dummy.start = ~0u;

    // Make sure entry block is first
    CODEGEN_ASSERT(sortedBlocks[0] == 0);
    CODEGEN_ASSERT(function.entryBlock == 0);

    for (size_t i = 0; i < sortedBlocks.size(); ++i)
    {
        uint32_t blockIndex = sortedBlocks[i];
        IrBlock& block = function.blocks[blockIndex];

        if (block.kind == IrBlockKind::Dead)
            continue;

        CODEGEN_ASSERT(block.start != ~0u);
        CODEGEN_ASSERT(block.finish != ~0u);
        CODEGEN_ASSERT(!seenFallback || block.kind == IrBlockKind::Fallback);

        // If we want to skip fallback code IR/asm, we'll record when those blocks start once we see them
        if (block.kind == IrBlockKind::Fallback && !seenFallback)
        {
            textSize = build.text.length();
            codeSize = build.getCodeSize();
            seenFallback = true;
        }

        if (options.includeIr)
        {
            if (options.includeIrPrefix == IncludeIrPrefix::Yes)
                build.logAppend("# ");

            toStringDetailed(ctx, block, blockIndex, options.includeUseInfo, options.includeCfgInfo, options.includeRegFlowInfo);
        }

        // Values can only reference restore operands in the current block chain
        function.validRestoreOpBlocks.push_back(blockIndex);

        build.setLabel(block.label);

        if (blockIndex == function.entryBlock)
        {
            function.entryLocation = build.getLabelOffset(block.label);
        }

        lowering.startBlock(block);

        IrBlock& nextBlock = getNextBlock(function, sortedBlocks, dummy, i);

        // Optimizations often propagate information between blocks
        // To make sure the register and spill state is correct when blocks are lowered, we check that sorted block order matches the expected one
        if (block.expectedNextBlock != ~0u)
            CODEGEN_ASSERT(function.getBlockIndex(nextBlock) == block.expectedNextBlock);

        // Block might establish a safe environment right at the start
        if ((block.flags & kBlockFlagSafeEnvCheck) != 0)
        {
            if (options.includeIr)
            {
                if (options.includeIrPrefix == IncludeIrPrefix::Yes)
                    build.logAppend("# ");

                build.logAppend("  implicit CHECK_SAFE_ENV exit(%u)\n", block.startpc);
            }

            CODEGEN_ASSERT(block.startpc != kBlockNoStartPc);
            lowering.checkSafeEnv(IrOp{IrOpKind::VmExit, block.startpc}, nextBlock);
        }

        for (uint32_t index = block.start; index <= block.finish; index++)
        {
            CODEGEN_ASSERT(index < function.instructions.size());

            uint32_t bcLocation = bcLocations[index];

            // If IR instruction is the first one for the original bytecode, we can annotate it with source code text
            if (outputEnabled && options.annotator && bcLocation != ~0u)
            {
                options.annotator(options.annotatorContext, build.text, bytecodeid, bcLocation);

                // If available, report inferred register tags
                BytecodeTypes bcTypes = function.getBytecodeTypesAt(bcLocation);

                if (bcTypes.result != LBC_TYPE_ANY || bcTypes.a != LBC_TYPE_ANY || bcTypes.b != LBC_TYPE_ANY || bcTypes.c != LBC_TYPE_ANY)
                {
                    toString(ctx.result, bcTypes, options.compilationOptions.userdataTypes);

                    build.logAppend("\n");
                }
            }

            // If bytecode needs the location of this instruction for jumps, record it
            if (bcLocation != ~0u)
            {
                Label label = (index == block.start) ? block.label : build.setLabel();
                function.bcMapping[bcLocation].asmLocation = build.getLabelOffset(label);
            }

            IrInst& inst = function.instructions[index];

            // Skip pseudo instructions, but make sure they are not used at this stage
            // This also prevents them from getting into text output when that's enabled
            if (isPseudo(inst.cmd))
            {
                CODEGEN_ASSERT(inst.useCount == 0);
                continue;
            }

            // Either instruction result value is not referenced or the use count is not zero
            CODEGEN_ASSERT(inst.lastUse == 0 || inst.useCount != 0);

            if (options.includeIr)
            {
                if (options.includeIrPrefix == IncludeIrPrefix::Yes)
                    build.logAppend("# ");

                toStringDetailed(ctx, block, blockIndex, inst, index, options.includeUseInfo);
            }

            lowering.lowerInst(inst, index, nextBlock);

            if (lowering.hasError())
            {
                // Place labels for all blocks that we're skipping
                // This is needed to avoid AssemblyBuilder assertions about jumps in earlier blocks with unplaced labels
                for (size_t j = i + 1; j < sortedBlocks.size(); ++j)
                {
                    IrBlock& abandoned = function.blocks[sortedBlocks[j]];

                    build.setLabel(abandoned.label);
                }

                lowering.finishFunction();

                return false;
            }
        }

        lowering.finishBlock(block, nextBlock);

        if (options.includeIr && options.includeIrPrefix == IncludeIrPrefix::Yes)
            build.logAppend("#\n");

        if (block.expectedNextBlock == ~0u)
            function.validRestoreOpBlocks.clear();
    }

    if (!seenFallback)
    {
        textSize = build.text.length();
        codeSize = build.getCodeSize();
    }

    lowering.finishFunction();

    if (outputEnabled && !options.includeOutlinedCode && textSize < build.text.size())
    {
        build.text.resize(textSize);

        if (options.includeAssembly)
            build.logAppend("; skipping %u bytes of outlined code\n", unsigned((build.getCodeSize() - codeSize) * sizeof(build.code[0])));
    }

    return true;
}

inline bool lowerIr(
    X64::AssemblyBuilderX64& build,
    IrBuilder& ir,
    const std::vector<uint32_t>& sortedBlocks,
    ModuleHelpers& helpers,
    Proto* proto,
    AssemblyOptions options,
    LoweringStats* stats
)
{
    optimizeMemoryOperandsX64(ir.function);

    X64::IrLoweringX64 lowering(build, helpers, ir.function, stats);

    return lowerImpl(build, lowering, ir.function, sortedBlocks, proto->bytecodeid, options);
}

inline bool lowerIr(
    A64::AssemblyBuilderA64& build,
    IrBuilder& ir,
    const std::vector<uint32_t>& sortedBlocks,
    ModuleHelpers& helpers,
    Proto* proto,
    AssemblyOptions options,
    LoweringStats* stats
)
{
    A64::IrLoweringA64 lowering(build, helpers, ir.function, stats);

    return lowerImpl(build, lowering, ir.function, sortedBlocks, proto->bytecodeid, options);
}

template<typename AssemblyBuilder>
inline bool lowerFunction(
    IrBuilder& ir,
    AssemblyBuilder& build,
    ModuleHelpers& helpers,
    Proto* proto,
    AssemblyOptions options,
    LoweringStats* stats,
    CodeGenCompilationResult& codeGenCompilationResult
)
{
    ir.function.stats = stats;
    ir.function.recordCounters = options.compilationOptions.recordCounters;

    killUnusedBlocks(ir.function);

    unsigned preOptBlockCount = 0;
    unsigned maxBlockInstructions = 0;

    for (const IrBlock& block : ir.function.blocks)
    {
        preOptBlockCount += (block.kind != IrBlockKind::Dead);
        unsigned blockInstructions = block.finish - block.start;
        maxBlockInstructions = std::max(maxBlockInstructions, blockInstructions);
    }

    // we update stats before checking the heuristic so that even if we bail out
    // our stats include information about the limit that was exceeded.
    if (stats)
    {
        stats->blocksPreOpt += preOptBlockCount;
        stats->maxBlockInstructions = maxBlockInstructions;
    }

    if (preOptBlockCount >= unsigned(FInt::CodegenHeuristicsBlockLimit.value))
    {
        codeGenCompilationResult = CodeGenCompilationResult::CodeGenOverflowBlockLimit;
        return false;
    }

    if (maxBlockInstructions >= unsigned(FInt::CodegenHeuristicsBlockInstructionLimit.value))
    {
        codeGenCompilationResult = CodeGenCompilationResult::CodeGenOverflowBlockInstructionLimit;
        return false;
    }

    computeCfgInfo(ir.function);

    constPropInBlockChains(ir);

    if (!FFlag::DebugCodegenOptSize)
    {
        double startTime = 0.0;
        unsigned constPropInstructionCount = 0;

        if (stats)
        {
            constPropInstructionCount = getInstructionCount(ir.function.instructions, IrCmd::SUBSTITUTE);
            startTime = lua_clock();
        }

        createLinearBlocks(ir);

        if (stats)
        {
            stats->blockLinearizationStats.timeSeconds += lua_clock() - startTime;
            constPropInstructionCount = getInstructionCount(ir.function.instructions, IrCmd::SUBSTITUTE) - constPropInstructionCount;
            stats->blockLinearizationStats.constPropInstructionCount += constPropInstructionCount;
        }
    }

    markDeadStoresInBlockChains(ir);

    // Recompute the CFG predecessors/successors to match block uses after optimizations
    computeCfgBlockEdges(ir.function);

    std::vector<uint32_t> sortedBlocks = getSortedBlockOrder(ir.function);

    // In order to allocate registers during lowering, we need to know where instruction results are last used
    updateLastUseLocations(ir.function, sortedBlocks);

    if (stats)
    {
        for (const IrBlock& block : ir.function.blocks)
        {
            if (block.kind != IrBlockKind::Dead)
                ++stats->blocksPostOpt;
        }
    }

    bool result = lowerIr(build, ir, sortedBlocks, helpers, proto, options, stats);

    if (!result)
        codeGenCompilationResult = CodeGenCompilationResult::CodeGenLoweringFailure;

    return result;
}

} // namespace CodeGen
} // namespace Luau
