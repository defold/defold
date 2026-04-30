// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/IrAnalysis.h"

#include "Luau/DenseHash.h"
#include "Luau/IrData.h"
#include "Luau/IrUtils.h"
#include "Luau/IrVisitUseDef.h"

#include "lobject.h"

#include <algorithm>
#include <bitset>

#include <stddef.h>

namespace Luau
{
namespace CodeGen
{

void updateUseCounts(IrFunction& function)
{
    std::vector<IrBlock>& blocks = function.blocks;
    std::vector<IrInst>& instructions = function.instructions;

    for (IrBlock& block : blocks)
        block.useCount = 0;

    for (IrInst& inst : instructions)
        inst.useCount = 0;

    auto checkOp = [&](IrOp op)
    {
        if (op.kind == IrOpKind::Inst)
        {
            IrInst& target = instructions[op.index];
            CODEGEN_ASSERT(target.useCount < 0xffff);
            target.useCount++;
        }
        else if (op.kind == IrOpKind::Block)
        {
            IrBlock& target = blocks[op.index];
            CODEGEN_ASSERT(target.useCount < 0xffff);
            target.useCount++;
        }
    };

    for (IrInst& inst : instructions)
    {
        for (IrOp& op : inst.ops)
            checkOp(op);
    }
}

void updateLastUseLocations(IrFunction& function, const std::vector<uint32_t>& sortedBlocks)
{
    std::vector<IrInst>& instructions = function.instructions;

#if defined(LUAU_ASSERTENABLED)
    // Last use assignments should be called only once
    for (IrInst& inst : instructions)
        CODEGEN_ASSERT(inst.lastUse == 0);
#endif

    for (size_t i = 0; i < sortedBlocks.size(); ++i)
    {
        uint32_t blockIndex = sortedBlocks[i];
        IrBlock& block = function.blocks[blockIndex];

        if (block.kind == IrBlockKind::Dead)
            continue;

        CODEGEN_ASSERT(block.start != ~0u);
        CODEGEN_ASSERT(block.finish != ~0u);

        for (uint32_t instIdx = block.start; instIdx <= block.finish; instIdx++)
        {
            CODEGEN_ASSERT(instIdx < function.instructions.size());
            IrInst& inst = instructions[instIdx];

            auto checkOp = [&](IrOp op)
            {
                if (op.kind == IrOpKind::Inst)
                    instructions[op.index].lastUse = uint32_t(instIdx);
            };

            if (isPseudo(inst.cmd))
                continue;

            for (IrOp& op : inst.ops)
                checkOp(op);
        }
    }
}

uint32_t getNextInstUse(IrFunction& function, uint32_t targetInstIdx, uint32_t startInstIdx)
{
    CODEGEN_ASSERT(startInstIdx < function.instructions.size());
    IrInst& targetInst = function.instructions[targetInstIdx];

    for (uint32_t i = startInstIdx; i <= targetInst.lastUse; i++)
    {
        IrInst& inst = function.instructions[i];

        if (isPseudo(inst.cmd))
            continue;

        for (IrOp& op : inst.ops)
            if (op.kind == IrOpKind::Inst && op.index == targetInstIdx)
                return i;
    }

    // There must be a next use since there is the last use location
    CODEGEN_ASSERT(!"Failed to find next use");
    return targetInst.lastUse;
}

std::pair<uint32_t, uint32_t> getLiveInOutValueCount(IrFunction& function, IrBlock& start, bool visitChain)
{
    // TODO: the function is not called often, but having a small vector would help here
    std::vector<uint32_t> blocks;

    if (visitChain)
    {
        for (IrBlock* block = &start; block; block = tryGetNextBlockInChain(function, *block))
            blocks.push_back(function.getBlockIndex(*block));
    }
    else
    {
        blocks.push_back(function.getBlockIndex(start));
    }

    uint32_t liveIns = 0;
    uint32_t liveOuts = 0;

    for (uint32_t blockIdx : blocks)
    {
        const IrBlock& block = function.blocks[blockIdx];

        // If an operand refers to something inside the current block chain, it completes the instruction we marked as 'live out'
        // If it refers to something outside, it has to be a 'live in'
        auto checkOp = [&function, &blocks, &liveIns, &liveOuts](IrOp op)
        {
            if (op.kind == IrOpKind::Inst)
            {
                for (uint32_t blockIdx : blocks)
                {
                    const IrBlock& block = function.blocks[blockIdx];

                    if (op.index >= block.start && op.index <= block.finish)
                    {
                        CODEGEN_ASSERT(liveOuts != 0);
                        liveOuts--;
                        return;
                    }
                }

                liveIns++;
            }
        };

        for (uint32_t instIdx = block.start; instIdx <= block.finish; instIdx++)
        {
            IrInst& inst = function.instructions[instIdx];

            if (isPseudo(inst.cmd))
                continue;

            liveOuts += inst.useCount;

            for (IrOp& op : inst.ops)
                checkOp(op);
        }
    }

    return std::make_pair(liveIns, liveOuts);
}

uint32_t getLiveInValueCount(IrFunction& function, IrBlock& block)
{
    return getLiveInOutValueCount(function, block, false).first;
}

uint32_t getLiveOutValueCount(IrFunction& function, IrBlock& block)
{
    return getLiveInOutValueCount(function, block, false).second;
}

void requireVariadicSequence(RegisterSet& sourceRs, const RegisterSet& defRs, uint8_t varargStart)
{
    if (!defRs.varargSeq)
    {
        // Peel away registers from variadic sequence that we define
        while (defRs.regs.test(varargStart))
            varargStart++;

        CODEGEN_ASSERT(!sourceRs.varargSeq || sourceRs.varargStart == varargStart);

        sourceRs.varargSeq = true;
        sourceRs.varargStart = varargStart;
    }
    else
    {
        // Variadic use sequence might include registers before def sequence
        for (int i = varargStart; i < defRs.varargStart; i++)
        {
            if (!defRs.regs.test(i))
                sourceRs.regs.set(i);
        }
    }
}

struct BlockVmRegLiveInComputation
{
    BlockVmRegLiveInComputation(RegisterSet& defRs, std::bitset<256>& capturedRegs)
        : defRs(defRs)
        , capturedRegs(capturedRegs)
    {
    }

    RegisterSet& defRs;
    std::bitset<256>& capturedRegs;

    RegisterSet inRs;

    void def(IrOp op, int offset = 0)
    {
        defRs.regs.set(vmRegOp(op) + offset, true);
    }

    void use(IrOp op, int offset = 0)
    {
        if (!defRs.regs.test(vmRegOp(op) + offset))
            inRs.regs.set(vmRegOp(op) + offset, true);
    }

    void maybeDef(IrOp op)
    {
        if (op.kind == IrOpKind::VmReg)
            defRs.regs.set(vmRegOp(op), true);
    }

    void maybeUse(IrOp op)
    {
        if (op.kind == IrOpKind::VmReg)
        {
            if (!defRs.regs.test(vmRegOp(op)))
                inRs.regs.set(vmRegOp(op), true);
        }
    }

    void defVarargs(uint8_t varargStart)
    {
        defRs.varargSeq = true;
        defRs.varargStart = varargStart;
    }

    void useVarargs(uint8_t varargStart)
    {
        requireVariadicSequence(inRs, defRs, varargStart);

        // Variadic sequence has been consumed
        defRs.varargSeq = false;
        defRs.varargStart = 0;
    }

    void defRange(int start, int count)
    {
        if (count == -1)
        {
            defVarargs(start);
        }
        else
        {
            for (int i = start; i < start + count; i++)
                defRs.regs.set(i, true);
        }
    }

    void useRange(int start, int count)
    {
        if (count == -1)
        {
            useVarargs(start);
        }
        else
        {
            for (int i = start; i < start + count; i++)
            {
                if (!defRs.regs.test(i))
                    inRs.regs.set(i, true);
            }
        }
    }

    void capture(int reg)
    {
        capturedRegs.set(reg, true);
    }
};

static RegisterSet computeBlockLiveInRegSet(IrFunction& function, const IrBlock& block, RegisterSet& defRs, std::bitset<256>& capturedRegs)
{
    BlockVmRegLiveInComputation visitor(defRs, capturedRegs);
    visitVmRegDefsUses(visitor, function, block);
    return visitor.inRs;
}

// The algorithm used here is commonly known as backwards data-flow analysis.
// For each block, we track 'upward-exposed' (live-in) uses of registers - a use of a register that hasn't been defined in the block yet.
// We also track the set of registers that were defined in the block.
// When initial live-in sets of registers are computed, propagation of those uses upwards through predecessors is performed.
// If predecessor doesn't define the register, we have to add it to the live-in set.
// Extending the set of live-in registers of a block requires re-checking of that block.
// Propagation runs iteratively, using a worklist of blocks to visit until a fixed point is reached.
// This algorithm can be easily extended to cover phi instructions, but we don't use those yet.
static void computeCfgLiveInOutRegSets(IrFunction& function)
{
    CfgInfo& info = function.cfg;

    // Clear existing data
    // 'in' and 'captured' data is not cleared because it will be overwritten below
    info.def.clear();
    info.out.clear();

    // Try to compute Luau VM register use-def info
    info.in.resize(function.blocks.size());
    info.def.resize(function.blocks.size());
    info.out.resize(function.blocks.size());

    // Captured registers are tracked for the whole function
    // It should be possible to have a more precise analysis for them in the future
    std::bitset<256> capturedRegs;

    // First we compute live-in set of each block
    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        const IrBlock& block = function.blocks[blockIdx];

        if (block.kind == IrBlockKind::Dead)
            continue;

        info.in[blockIdx] = computeBlockLiveInRegSet(function, block, info.def[blockIdx], capturedRegs);
    }

    info.captured.regs = capturedRegs;

    // With live-in sets ready, we can arrive at a fixed point for both in/out registers by requesting required registers from predecessors
    std::vector<uint32_t> worklist;

    std::vector<uint8_t> inWorklist;
    inWorklist.resize(function.blocks.size(), false);

    // We will have to visit each block at least once, so we add all of them to the worklist immediately
    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        const IrBlock& block = function.blocks[blockIdx];

        if (block.kind == IrBlockKind::Dead)
            continue;

        worklist.push_back(uint32_t(blockIdx));
        inWorklist[blockIdx] = true;
    }

    while (!worklist.empty())
    {
        uint32_t blockIdx = worklist.back();
        worklist.pop_back();
        inWorklist[blockIdx] = false;

        IrBlock& curr = function.blocks[blockIdx];
        RegisterSet& inRs = info.in[blockIdx];
        RegisterSet& defRs = info.def[blockIdx];
        RegisterSet& outRs = info.out[blockIdx];

        // Current block has to provide all registers in successor blocks
        BlockIteratorWrapper successorsIt = successors(info, blockIdx);
        for (uint32_t succIdx : successorsIt)
        {
            IrBlock& succ = function.blocks[succIdx];

            // This is a step away from the usual definition of live range flow through CFG
            // Exit from a regular block to a fallback block is not considered a block terminator
            // This is because fallback blocks define an alternative implementation of the same operations
            // This can cause the current block to define more registers that actually were available at fallback entry
            if (curr.kind != IrBlockKind::Fallback && succ.kind == IrBlockKind::Fallback)
            {
                // If this is the only successor, this skip will not be valid
                CODEGEN_ASSERT(successorsIt.size() != 1);
                continue;
            }

            const RegisterSet& succRs = info.in[succIdx];

            outRs.regs |= succRs.regs;

            if (succRs.varargSeq)
            {
                CODEGEN_ASSERT(!outRs.varargSeq || outRs.varargStart == succRs.varargStart);

                outRs.varargSeq = true;
                outRs.varargStart = succRs.varargStart;
            }
        }

        RegisterSet oldInRs = inRs;

        // If current block didn't define a live-out, it has to be live-in
        inRs.regs |= outRs.regs & ~defRs.regs;

        if (outRs.varargSeq)
            requireVariadicSequence(inRs, defRs, outRs.varargStart);

        // If we have new live-ins, we have to notify all predecessors
        // We don't allow changes to the start of the variadic sequence, so we skip checking that member
        if (inRs.regs != oldInRs.regs || inRs.varargSeq != oldInRs.varargSeq)
        {
            for (uint32_t predIdx : predecessors(info, blockIdx))
            {
                if (!inWorklist[predIdx])
                {
                    worklist.push_back(predIdx);
                    inWorklist[predIdx] = true;
                }
            }
        }
    }

    // Collect data on all registers that are written
    function.cfg.written.regs.reset();

    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        const IrBlock& block = function.blocks[blockIdx];

        if (block.kind == IrBlockKind::Dead)
            continue;

        RegisterSet& defRs = info.def[blockIdx];

        function.cfg.written.regs |= defRs.regs;

        if (defRs.varargSeq)
        {
            if (!function.cfg.written.varargSeq || defRs.varargStart < function.cfg.written.varargStart)
                function.cfg.written.varargStart = defRs.varargStart;

            function.cfg.written.varargSeq = true;
        }
    }

    // If Proto data is available, validate that entry block arguments match required registers
    if (function.proto)
    {
        RegisterSet& entryIn = info.in[0];

        CODEGEN_ASSERT(!entryIn.varargSeq);

        for (size_t i = 0; i < entryIn.regs.size(); i++)
            CODEGEN_ASSERT(!entryIn.regs.test(i) || i < function.proto->numparams);
    }
}

void computeCfgBlockEdges(IrFunction& function)
{
    CfgInfo& info = function.cfg;

    // Clear existing data
    info.predecessorsOffsets.clear();
    info.successorsOffsets.clear();

    // Compute predecessors block edges
    info.predecessorsOffsets.reserve(function.blocks.size());
    info.successorsOffsets.reserve(function.blocks.size());

    int edgeCount = 0;

    for (const IrBlock& block : function.blocks)
    {
        info.predecessorsOffsets.push_back(edgeCount);
        edgeCount += block.useCount;
    }

    info.predecessors.resize(edgeCount);
    info.successors.resize(edgeCount);

    edgeCount = 0;

    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        const IrBlock& block = function.blocks[blockIdx];

        info.successorsOffsets.push_back(edgeCount);

        if (block.kind == IrBlockKind::Dead)
            continue;

        for (uint32_t instIdx = block.start; instIdx <= block.finish; instIdx++)
        {
            const IrInst& inst = function.instructions[instIdx];

            auto checkOp = [&](IrOp op)
            {
                if (op.kind == IrOpKind::Block)
                {
                    // We use a trick here, where we use the starting offset of the predecessor list as the position where to write next predecessor
                    // The values will be adjusted back in a separate loop later
                    info.predecessors[info.predecessorsOffsets[op.index]++] = uint32_t(blockIdx);

                    info.successors[edgeCount++] = op.index;
                }
            };

            for (const IrOp& op : inst.ops)
                checkOp(op);
        }
    }

    // Offsets into the predecessor list were used as iterators in the previous loop
    // To adjust them back, block use count is subtracted (predecessor count is equal to how many uses block has)
    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        const IrBlock& block = function.blocks[blockIdx];

        info.predecessorsOffsets[blockIdx] -= block.useCount;
    }
}

// Assign tree depth and pre- and post- DFS visit order of the tree/graph nodes
// Optionally, collect required node order into a vector
template<auto childIt>
void computeBlockOrdering(
    IrFunction& function,
    std::vector<BlockOrdering>& ordering,
    std::vector<uint32_t>* preOrder,
    std::vector<uint32_t>* postOrder
)
{
    CfgInfo& info = function.cfg;

    CODEGEN_ASSERT(info.idoms.size() == function.blocks.size());

    ordering.clear();
    ordering.resize(function.blocks.size());

    // Get depth-first post-order using manual stack instead of recursion
    struct StackItem
    {
        uint32_t blockIdx;
        uint32_t itPos;
    };
    std::vector<StackItem> stack;

    if (preOrder)
        preOrder->reserve(function.blocks.size());
    if (postOrder)
        postOrder->reserve(function.blocks.size());

    uint32_t nextPreOrder = 0;
    uint32_t nextPostOrder = 0;

    stack.push_back({0, 0});
    ordering[0].visited = true;
    ordering[0].preOrder = nextPreOrder++;

    while (!stack.empty())
    {
        StackItem& item = stack.back();
        BlockIteratorWrapper children = childIt(info, item.blockIdx);

        if (item.itPos < children.size())
        {
            uint32_t childIdx = children[item.itPos++];

            BlockOrdering& childOrdering = ordering[childIdx];

            if (!childOrdering.visited)
            {
                childOrdering.visited = true;
                childOrdering.depth = uint32_t(stack.size());
                childOrdering.preOrder = nextPreOrder++;

                if (preOrder)
                    preOrder->push_back(item.blockIdx);

                stack.push_back({childIdx, 0});
            }
        }
        else
        {
            ordering[item.blockIdx].postOrder = nextPostOrder++;

            if (postOrder)
                postOrder->push_back(item.blockIdx);

            stack.pop_back();
        }
    }
}

// Dominance tree construction based on 'A Simple, Fast Dominance Algorithm' [Keith D. Cooper, et al]
// This solution has quadratic complexity in the worst case.
// It is possible to switch to SEMI-NCA algorithm (also quadratic) mentioned in 'Linear-Time Algorithms for Dominators and Related Problems' [Loukas
// Georgiadis]

// Find block that is common between blocks 'a' and 'b' on the path towards the entry
static uint32_t findCommonDominator(const std::vector<uint32_t>& idoms, const std::vector<BlockOrdering>& data, uint32_t a, uint32_t b)
{
    while (a != b)
    {
        while (data[a].postOrder < data[b].postOrder)
        {
            a = idoms[a];
            CODEGEN_ASSERT(a != ~0u);
        }

        while (data[b].postOrder < data[a].postOrder)
        {
            b = idoms[b];
            CODEGEN_ASSERT(b != ~0u);
        }
    }

    return a;
}

void computeCfgImmediateDominators(IrFunction& function)
{
    CfgInfo& info = function.cfg;

    // Clear existing data
    info.idoms.clear();
    info.idoms.resize(function.blocks.size(), ~0u);

    std::vector<BlockOrdering> ordering;
    std::vector<uint32_t> blocksInPostOrder;
    computeBlockOrdering<successors>(function, ordering, /* preOrder */ nullptr, &blocksInPostOrder);

    // Entry node is temporarily marked to be an idom of itself to make algorithm work
    info.idoms[0] = 0;

    // Iteratively compute immediate dominators
    bool updated = true;

    while (updated)
    {
        updated = false;

        // Go over blocks in reverse post-order of CFG
        // '- 2' skips the root node which is last in post-order traversal
        for (int i = int(blocksInPostOrder.size() - 2); i >= 0; i--)
        {
            uint32_t blockIdx = blocksInPostOrder[i];
            uint32_t newIdom = ~0u;

            for (uint32_t predIdx : predecessors(info, blockIdx))
            {
                if (uint32_t predIdom = info.idoms[predIdx]; predIdom != ~0u)
                {
                    if (newIdom == ~0u)
                        newIdom = predIdx;
                    else
                        newIdom = findCommonDominator(info.idoms, ordering, newIdom, predIdx);
                }
            }

            if (newIdom != info.idoms[blockIdx])
            {
                info.idoms[blockIdx] = newIdom;

                // Run until a fixed point is reached
                updated = true;
            }
        }
    }

    // Entry node doesn't have an immediate dominator
    info.idoms[0] = ~0u;
}

void computeCfgDominanceTreeChildren(IrFunction& function)
{
    CfgInfo& info = function.cfg;

    // Clear existing data
    info.domChildren.clear();

    info.domChildrenOffsets.clear();
    info.domChildrenOffsets.resize(function.blocks.size());

    // First we need to know children count of each node in the dominance tree
    // We use offset array for to hold this data, counts will be readjusted to offsets later
    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        uint32_t domParent = info.idoms[blockIdx];

        if (domParent != ~0u)
            info.domChildrenOffsets[domParent]++;
    }

    // Convert counts to offsets using prefix sum
    uint32_t total = 0;

    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        uint32_t& offset = info.domChildrenOffsets[blockIdx];
        uint32_t count = offset;
        offset = total;
        total += count;
    }

    info.domChildren.resize(total);

    for (size_t blockIdx = 0; blockIdx < function.blocks.size(); blockIdx++)
    {
        // We use a trick here, where we use the starting offset of the dominance children list as the position where to write next child
        // The values will be adjusted back in a separate loop later
        uint32_t domParent = info.idoms[blockIdx];

        if (domParent != ~0u)
            info.domChildren[info.domChildrenOffsets[domParent]++] = uint32_t(blockIdx);
    }

    // Offsets into the dominance children list were used as iterators in the previous loop
    // That process basically moved the values in the array 1 step towards the start
    // Here we move them one step towards the end and restore 0 for first offset
    for (int blockIdx = int(function.blocks.size() - 1); blockIdx > 0; blockIdx--)
        info.domChildrenOffsets[blockIdx] = info.domChildrenOffsets[blockIdx - 1];
    info.domChildrenOffsets[0] = 0;

    computeBlockOrdering<domChildren>(function, info.domOrdering, /* preOrder */ nullptr, /* postOrder */ nullptr);
}

// This algorithm is based on 'A Linear Time Algorithm for Placing Phi-Nodes' [Vugranam C.Sreedhar]
// It uses the optimized form from LLVM that relies an implicit DJ-graph (join edges are edges of the CFG that are not part of the dominance tree)
void computeIteratedDominanceFrontierForDefs(
    IdfContext& ctx,
    const IrFunction& function,
    const std::vector<uint32_t>& defBlocks,
    const std::vector<uint32_t>& liveInBlocks
)
{
    CODEGEN_ASSERT(!function.cfg.domOrdering.empty());

    CODEGEN_ASSERT(ctx.queue.empty());
    CODEGEN_ASSERT(ctx.worklist.empty());

    ctx.idf.clear();

    ctx.visits.clear();
    ctx.visits.resize(function.blocks.size());

    for (uint32_t defBlock : defBlocks)
    {
        const BlockOrdering& ordering = function.cfg.domOrdering[defBlock];
        ctx.queue.push({defBlock, ordering});
    }

    while (!ctx.queue.empty())
    {
        IdfContext::BlockAndOrdering root = ctx.queue.top();
        ctx.queue.pop();

        CODEGEN_ASSERT(ctx.worklist.empty());
        ctx.worklist.push_back(root.blockIdx);
        ctx.visits[root.blockIdx].seenInWorklist = true;

        while (!ctx.worklist.empty())
        {
            uint32_t blockIdx = ctx.worklist.back();
            ctx.worklist.pop_back();

            // Check if successor node is the node where dominance of the current root ends, making it a part of dominance frontier set
            for (uint32_t succIdx : successors(function.cfg, blockIdx))
            {
                const BlockOrdering& succOrdering = function.cfg.domOrdering[succIdx];

                // Nodes in the DF of root always have a level that is less than or equal to the level of root
                if (succOrdering.depth > root.ordering.depth)
                    continue;

                if (ctx.visits[succIdx].seenInQueue)
                    continue;

                ctx.visits[succIdx].seenInQueue = true;

                // Skip successor block if it doesn't have our variable as a live in there
                if (std::find(liveInBlocks.begin(), liveInBlocks.end(), succIdx) == liveInBlocks.end())
                    continue;

                ctx.idf.push_back(succIdx);

                // If block doesn't have its own definition of the variable, add it to the queue
                if (std::find(defBlocks.begin(), defBlocks.end(), succIdx) == defBlocks.end())
                    ctx.queue.push({succIdx, succOrdering});
            }

            // Add dominance tree children that haven't been processed yet to the worklist
            for (uint32_t domChildIdx : domChildren(function.cfg, blockIdx))
            {
                if (ctx.visits[domChildIdx].seenInWorklist)
                    continue;

                ctx.visits[domChildIdx].seenInWorklist = true;
                ctx.worklist.push_back(domChildIdx);
            }
        }
    }
}

void computeCfgInfo(IrFunction& function)
{
    computeCfgBlockEdges(function);
    computeCfgImmediateDominators(function);
    computeCfgDominanceTreeChildren(function);
    computeCfgLiveInOutRegSets(function);
}

BlockIteratorWrapper predecessors(const CfgInfo& cfg, uint32_t blockIdx)
{
    CODEGEN_ASSERT(blockIdx < cfg.predecessorsOffsets.size());

    uint32_t start = cfg.predecessorsOffsets[blockIdx];
    uint32_t end = blockIdx + 1 < cfg.predecessorsOffsets.size() ? cfg.predecessorsOffsets[blockIdx + 1] : uint32_t(cfg.predecessors.size());

    return BlockIteratorWrapper{cfg.predecessors.data() + start, cfg.predecessors.data() + end};
}

BlockIteratorWrapper successors(const CfgInfo& cfg, uint32_t blockIdx)
{
    CODEGEN_ASSERT(blockIdx < cfg.successorsOffsets.size());

    uint32_t start = cfg.successorsOffsets[blockIdx];
    uint32_t end = blockIdx + 1 < cfg.successorsOffsets.size() ? cfg.successorsOffsets[blockIdx + 1] : uint32_t(cfg.successors.size());

    return BlockIteratorWrapper{cfg.successors.data() + start, cfg.successors.data() + end};
}

BlockIteratorWrapper domChildren(const CfgInfo& cfg, uint32_t blockIdx)
{
    CODEGEN_ASSERT(blockIdx < cfg.domChildrenOffsets.size());

    uint32_t start = cfg.domChildrenOffsets[blockIdx];
    uint32_t end = blockIdx + 1 < cfg.domChildrenOffsets.size() ? cfg.domChildrenOffsets[blockIdx + 1] : uint32_t(cfg.domChildren.size());

    return BlockIteratorWrapper{cfg.domChildren.data() + start, cfg.domChildren.data() + end};
}

} // namespace CodeGen
} // namespace Luau
