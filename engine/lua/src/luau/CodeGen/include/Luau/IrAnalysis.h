// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeGenCommon.h"

#include <bitset>
#include <queue>
#include <utility>
#include <vector>

#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

struct IrBlock;
struct IrFunction;

void updateUseCounts(IrFunction& function);

void updateLastUseLocations(IrFunction& function, const std::vector<uint32_t>& sortedBlocks);

uint32_t getNextInstUse(IrFunction& function, uint32_t targetInstIdx, uint32_t startInstIdx);

// Returns how many values are coming into the block (live in) and how many are coming out of the block (live out)
std::pair<uint32_t, uint32_t> getLiveInOutValueCount(IrFunction& function, IrBlock& start, bool visitChain);
uint32_t getLiveInValueCount(IrFunction& function, IrBlock& block);
uint32_t getLiveOutValueCount(IrFunction& function, IrBlock& block);

struct RegisterSet
{
    std::bitset<256> regs;

    // If variadic sequence is active, we track register from which it starts
    bool varargSeq = false;
    uint8_t varargStart = 0;
};

void requireVariadicSequence(RegisterSet& sourceRs, const RegisterSet& defRs, uint8_t varargStart);

struct BlockOrdering
{
    uint32_t depth = 0;

    uint32_t preOrder = ~0u;
    uint32_t postOrder = ~0u;

    bool visited = false;
};

struct CfgInfo
{
    std::vector<uint32_t> predecessors;
    std::vector<uint32_t> predecessorsOffsets;

    std::vector<uint32_t> successors;
    std::vector<uint32_t> successorsOffsets;

    // Immediate dominators (unique parent in the dominator tree)
    std::vector<uint32_t> idoms;

    // Children in the dominator tree
    std::vector<uint32_t> domChildren;
    std::vector<uint32_t> domChildrenOffsets;

    std::vector<BlockOrdering> domOrdering;

    // VM registers that are live when the block is entered
    // Additionally, an active variadic sequence can exist at the entry of the block
    std::vector<RegisterSet> in;

    // VM registers that are defined inside the block
    // It can also contain a variadic sequence definition if that hasn't been consumed inside the block
    // Note that this means that checking 'def' set might not be enough to say that register has not been written to
    std::vector<RegisterSet> def;

    // VM registers that are coming out from the block
    // These might be registers that are defined inside the block or have been defined at the entry of the block
    // Additionally, an active variadic sequence can exist at the exit of the block
    std::vector<RegisterSet> out;

    // VM registers captured by nested closures
    // This set can never have an active variadic sequence
    RegisterSet captured;

    // VM registers defined in any block
    // Lowest variadic sequence start is stored to mark variadic writes
    RegisterSet written;
};

// Calculate lists of block predecessors and successors
void computeCfgBlockEdges(IrFunction& function);

// A quick refresher on dominance and dominator trees:
// * If A is a dominator of B (A dom B), you can never execute B without executing A first
// * A is a strict dominator of B (A sdom B) is similar to previous one but A != B
// * Immediate dominator node N (idom N) is a unique node T so that T sdom N,
//   but T does not strictly dominate any other node that dominates N.
// * Dominance frontier is a set of nodes where dominance of a node X ends.
//   In practice this is where values established by node X might no longer hold because of join edges from other nodes coming in.
//   This is also where PHI instructions in SSA are placed.
void computeCfgImmediateDominators(IrFunction& function);
void computeCfgDominanceTreeChildren(IrFunction& function);

struct IdfContext
{
    struct BlockAndOrdering
    {
        uint32_t blockIdx;
        BlockOrdering ordering;

        bool operator<(const BlockAndOrdering& rhs) const
        {
            if (ordering.depth != rhs.ordering.depth)
                return ordering.depth < rhs.ordering.depth;

            return ordering.preOrder < rhs.ordering.preOrder;
        }
    };

    // Using priority queue to work on nodes in the order from the bottom of the dominator tree to the top
    // If the depth of keys is equal, DFS order is used to provide strong ordering
    std::priority_queue<BlockAndOrdering> queue;
    std::vector<uint32_t> worklist;

    struct IdfVisitMarks
    {
        bool seenInQueue = false;
        bool seenInWorklist = false;
    };

    std::vector<IdfVisitMarks> visits;

    std::vector<uint32_t> idf;
};

// Compute iterated dominance frontier (IDF or DF+) for a variable, given the set of blocks where that variable is defined
// Providing a set of blocks where the variable is a live-in at the entry helps produce a pruned SSA form (inserted phi nodes will not be dead)
//
// 'Iterated' comes from the definition where we recompute the IDFn+1 = DF(S) while adding IDFn to S until a fixed point is reached
// Iterated dominance frontier has been shown to be equal to the set of nodes where phi instructions have to be inserted
void computeIteratedDominanceFrontierForDefs(
    IdfContext& ctx,
    const IrFunction& function,
    const std::vector<uint32_t>& defBlocks,
    const std::vector<uint32_t>& liveInBlocks
);

// Function used to update all CFG data
void computeCfgInfo(IrFunction& function);

struct BlockIteratorWrapper
{
    const uint32_t* itBegin = nullptr;
    const uint32_t* itEnd = nullptr;

    bool empty() const
    {
        return itBegin == itEnd;
    }

    size_t size() const
    {
        return size_t(itEnd - itBegin);
    }

    const uint32_t* begin() const
    {
        return itBegin;
    }

    const uint32_t* end() const
    {
        return itEnd;
    }

    uint32_t operator[](size_t pos) const
    {
        CODEGEN_ASSERT(pos < size_t(itEnd - itBegin));
        return itBegin[pos];
    }
};

BlockIteratorWrapper predecessors(const CfgInfo& cfg, uint32_t blockIdx);
BlockIteratorWrapper successors(const CfgInfo& cfg, uint32_t blockIdx);
BlockIteratorWrapper domChildren(const CfgInfo& cfg, uint32_t blockIdx);

} // namespace CodeGen
} // namespace Luau
