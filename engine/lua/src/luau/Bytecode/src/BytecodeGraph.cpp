#include "Luau/BytecodeBuilder.h"
#include "Luau/BytecodeGraph.h"
#include "Luau/BytecodeUtils.h"
#include "Luau/BytecodeWire.h"

#include <unordered_set>
#include <algorithm>

namespace Luau
{
namespace Bytecode
{

static std::string_view readString(std::vector<std::string_view>& strings, const char* data, size_t& offset)
{
    uint32_t stringId = readVarInt(data, offset);
    LUAU_ASSERT(stringId <= strings.size());
    if (stringId == 0)
        return "";
    return strings[stringId - 1];
}

void BcBlock::addSuccessor(BcFunction& func, BcOp block, BcBlockEdgeKind kind)
{
    uint32_t idx = func.getBlockIndex(*this);
    successors.push_back({kind, block});
    func.blockOp(block).predecessors.push_back({kind, BcOp{BcOpKind::Block, idx}});
}

void addSuccessor(BcFunction& func, BcOp from, BcOp to, BcBlockEdgeKind kind)
{
    func.blockOp(from).addSuccessor(func, to, kind);
}

bool isJumpTrampoline(uint32_t pc, const Instruction* code, uint32_t codesize)
{
    return LuauOpcode(LUAU_INSN_OP(code[pc])) == LOP_JUMP && pc + 1 < codesize && LuauOpcode(LUAU_INSN_OP(code[pc + 1])) == LOP_JUMPX &&
           static_cast<uint32_t>(getJumpTarget(code[pc + 2], pc + 2)) == pc + 1;
}

std::pair<std::unordered_map<uint32_t, BcOp>, size_t> rebuildBlocks(BcFunction& func, const Instruction code[], uint32_t codesize)
{
    std::unordered_map<uint32_t, BcOp> blockByPC;
    auto makeBlock = [&](uint32_t pc) -> BcOp
    {
        BcOp newBlockOp = func.addBlock();
        blockByPC[pc] = newBlockOp;
        BcBlock& newBlock = func.blockOp(newBlockOp);
        newBlock.sortkey = pc;
        return newBlockOp;
    };
    BcOp entryBlock = func.entryBlock = makeBlock(0);
    BcOp exitBlock = func.exitBlock = makeBlock(kBlockNoStartPc);
    uint32_t i = 0;
    BcOp currentBlock = entryBlock;
    size_t instructionCount = 0;
    while (i < codesize)
    {
        Instruction insn = code[i];
        LuauOpcode op = LuauOpcode(LUAU_INSN_OP(insn));
        int target = getJumpTarget(insn, i);
        if (target >= 0 && LuauOpcode(LUAU_INSN_OP(code[target])) == LOP_JUMPX)
            target = getJumpTarget(code[target], target);

        bool needsBlock = target >= 0 && !isFastCall(op) && op != LOP_JUMPX && !isJumpTrampoline(i, code, codesize);
        if (needsBlock)
        {
            if (blockByPC.count(target) == 0)
            {
                BcOp newBlockOp = makeBlock(target);
                if (target < static_cast<int>(i)) // We are jumping back.
                {
                    // The new block was created in the middle of the existing one.
                    // We need to maintain predecessor/successor relations.
                    uint32_t blockStartPc = target - 1;
                    while (blockByPC.count(blockStartPc) == 0 && blockStartPc-- != 0) ;
                    LUAU_ASSERT(blockByPC.count(blockStartPc) > 0);
                    BcOp prevBlockOp = blockByPC[blockStartPc];
                    BcBlock& prevBlock = func.blockOp(prevBlockOp);
                    BcBlock& newBlock = func.blockOp(newBlockOp);
                    // Steal successors of the previous block.
                    newBlock.successors = prevBlock.successors;
                    // Now it should only fallsthrough to the new block.
                    prevBlock.successors.clear();
                    addSuccessor(func, prevBlockOp, newBlockOp, BcBlockEdgeKind::Fallthrough);
                    // Update all successors to have the new block as a predecessor instead of the old one.
                    for (auto& edge : newBlock.successors)
                        for (auto& backEdge : func.blockOp(edge.target).predecessors)
                            if (backEdge.target == prevBlockOp)
                                backEdge.target = newBlockOp;
                }
            }
            addSuccessor(func, currentBlock, blockByPC[target], isLoopJump(op) ? BcBlockEdgeKind::Loop : BcBlockEdgeKind::Branch);
        }
        if (op == LOP_RETURN)
            addSuccessor(func, currentBlock, exitBlock, BcBlockEdgeKind::Fallthrough);
        i += getOpLength(op);
        if ((needsBlock || (op == LOP_RETURN && i < codesize)) && blockByPC.count(i) == 0)
            makeBlock(i);

        if (blockByPC.count(i) != 0)
        {
            if (isFallthrough(op))
                addSuccessor(func, currentBlock, blockByPC[i], BcBlockEdgeKind::Fallthrough);
            currentBlock = blockByPC[i];
        }
        instructionCount++;
    }
    return {blockByPC, instructionCount};
}

struct LoopInfo
{
    BcOp entry;
    BcOp exit;
};

struct BlockProducers
{
    std::unordered_map<Reg, BcOp> own;
    std::unordered_map<Reg, BcOp> cached;
    BcOp multiReturn;
    Reg multiReturnStart;
    int invalidAfter = 255;
};

using Producers = std::vector<BlockProducers>;

std::optional<BcOp> findProducer(Producers& producers, BcFunction& func, BcOp block, Reg reg, std::unordered_set<BcOp, BcOpHash>& visited)
{
    visited.insert(block);
    LUAU_ASSERT(block.index < producers.size());
    BlockProducers& blockProducers = producers.at(block.index);
    if (static_cast<int>(reg) > blockProducers.invalidAfter)
        return {};

    if (auto local = blockProducers.own.find(reg); local != blockProducers.own.end())
    {
        return {local->second};
    }

    if (auto cached = blockProducers.cached.find(reg); cached != blockProducers.cached.end())
    {
        return {cached->second};
    }

    if (blockProducers.multiReturn.kind != BcOpKind::None && reg >= blockProducers.multiReturnStart)
        return func.addProj(blockProducers.multiReturn, reg - blockProducers.multiReturnStart);

    std::unordered_set<BcOp, BcOpHash> results;
    BcBlock& bl = func.blockOp(block);
    for (auto [ctrl, pred] : bl.predecessors)
    {
        if (ctrl == BcBlockEdgeKind::Loop || visited.count(pred) > 0)
            continue;
        LUAU_ASSERT(block != pred);
        if (std::optional<BcOp> op = findProducer(producers, func, pred, reg, visited))
        {
            if (op->kind == BcOpKind::Phi)
                for (BcOp& proj : func.phiOp(*op).ops)
                    results.insert(proj);
            else
                results.insert(*op);
        }
    }
    if (results.size() == 0)
        return {};
    BcOp res;
    if (results.size() == 1)
        res = *results.begin();
    else
    {
        res = func.addPhi();
        BcPhi& phi = func.phiOp(res);
        for (auto op : results)
            phi.ops.push_back(op);
    }
    blockProducers.cached[reg] = res;
    return res;
}

std::optional<BcOp> findProducer(Producers& producers, BcFunction& func, BcOp block, Reg reg)
{
    std::unordered_set<BcOp, BcOpHash> visited;
    return findProducer(producers, func, block, reg, visited);
}

bool hasProducerBefore(
    Producers& producers,
    BcFunction& func,
    BcOp rangeStart,
    BcOp rangeEnd,
    BcOp startOp,
    Reg reg,
    bool checkCached,
    std::unordered_set<BcOp, BcOpHash>& visited
)
{
    LUAU_ASSERT(startOp.kind == BcOpKind::Inst);
    visited.insert(rangeEnd);
    LUAU_ASSERT(rangeEnd.index < producers.size());
    BlockProducers& blockProducers = producers.at(rangeEnd.index);
    if (static_cast<int>(reg) > blockProducers.invalidAfter)
        return false;
    BcBlock& bl = func.blockOp(rangeEnd);
    if (blockProducers.multiReturn.kind != BcOpKind::None && reg >= blockProducers.multiReturnStart)
        return true;
    if (checkCached)
    {
        if (blockProducers.own.count(reg) > 0)
            return true;
    }
    else
        for (auto op : bl.ops)
        {
            // We have reached the end of range.
            if (op == startOp)
                break;
            auto opReg = func.regs.find(op);
            if (opReg != func.regs.end() && opReg->second == reg)
                return true;
        }
    if (rangeEnd == rangeStart)
        return false;
    for (auto [ctrl, pred] : bl.predecessors)
    {
        if (ctrl == BcBlockEdgeKind::Loop || visited.count(pred) > 0)
            continue;
        if (hasProducerBefore(producers, func, rangeStart, pred, startOp, reg, true, visited))
            return true;
    }
    return false;
}

bool hasProducerBefore(Producers& producers, BcFunction& func, BcOp rangeStart, BcOp rangeEnd, BcOp startOp, Reg reg)
{
    std::unordered_set<BcOp, BcOpHash> visited;
    return hasProducerBefore(producers, func, rangeStart, rangeEnd, startOp, reg, false, visited);
}

std::optional<BcOp> findForwardProducerInRange(
    Producers& producers,
    BcFunction& func,
    BcOp rangeStart,
    BcOp rangeEnd,
    BcOp startOp,
    Reg reg,
    std::unordered_set<BcOp, BcOpHash>& visited
)
{
    LUAU_ASSERT(startOp.kind == BcOpKind::Inst);
    visited.insert(rangeEnd);
    LUAU_ASSERT(rangeEnd.index < producers.size());
    BlockProducers& blockProducers = producers.at(rangeEnd.index);
    if (static_cast<int>(reg) > blockProducers.invalidAfter)
        return {};
    BcBlock& bl = func.blockOp(rangeEnd);

    if (auto local = blockProducers.own.find(reg); local != blockProducers.own.end())
        return {local->second};

    if (rangeStart == rangeEnd)
        return {};

    if (blockProducers.multiReturn.kind != BcOpKind::None && reg >= blockProducers.multiReturnStart)
        return blockProducers.multiReturn;

    std::unordered_set<BcOp, BcOpHash> results;
    for (auto [ctrl, pred] : bl.predecessors)
    {
        if (ctrl == BcBlockEdgeKind::Loop || visited.count(pred) > 0)
            continue;
        LUAU_ASSERT(rangeEnd != pred);
        if (std::optional<BcOp> op = findForwardProducerInRange(producers, func, rangeStart, pred, startOp, reg, visited))
        {
            if (op->kind == BcOpKind::Phi)
                for (BcOp& proj : func.phiOp(*op).ops)
                    results.insert(proj);
            else
                results.insert(*op);
        }
    }
    if (results.size() == 0)
        return {};
    BcOp res;
    if (results.size() == 1)
        res = *results.begin();
    else
    {
        res = func.addPhi();
        BcPhi& phi = func.phiOp(res);
        for (auto op : results)
            phi.ops.push_back(op);
    }

    return res;
}

std::optional<BcOp> findForwardProducerInRange(Producers& producers, BcFunction& func, BcOp rangeStart, BcOp rangeEnd, BcOp startOp, Reg reg)
{
    std::unordered_set<BcOp, BcOpHash> visited;
    return findForwardProducerInRange(producers, func, rangeStart, rangeEnd, startOp, reg, visited);
}

std::vector<BcOp> findProducersUpToTop(Producers& producers, BcFunction& func, BcOp block, Reg reg)
{
    // We assume it called only for search of var return calls.
    LUAU_ASSERT(block.index < producers.size());
    BlockProducers& blockProducers = producers.at(block.index);
    // So we need to find all producers from reg to blockProducers.multiReturnStart.
    LUAU_ASSERT(blockProducers.multiReturn.kind == BcOpKind::Inst);
    std::vector<BcOp> res;
    res.reserve(blockProducers.multiReturnStart - reg + 1);
    for (; reg < blockProducers.multiReturnStart; reg++)
    {
        auto staticRegOp = findProducer(producers, func, block, reg);
        LUAU_ASSERT(staticRegOp);
        res.push_back(*staticRegOp);
    }
    res.push_back(blockProducers.multiReturn);
    // multireturn is consumed, clean it up
    blockProducers.multiReturn = BcOp{};
    blockProducers.multiReturnStart = 0xFF;
    return res;
}

bool isUnreachable(BcFunction& func, BcOp blockOp)
{
    if (blockOp == func.entryBlock)
        return false;
    BcBlock& block = func.blockOp(blockOp);
    for (auto [ctrl, pred] : block.predecessors)
    {
        if (ctrl == BcBlockEdgeKind::Loop)
            continue;
        if (!isUnreachable(func, pred))
            return false;
    }
    return true;
}

std::optional<BcOp> getFallthrough(BcBlock& block)
{
    for (auto [ctrl, target] : block.successors)
        if (ctrl == BcBlockEdgeKind::Fallthrough)
            return {target};
    return {};
}

void addProducer(RegMap& regs, Producers& producers, BcOp block, Reg reg, BcOp op)
{
    BlockProducers& blockProducers = producers[block.index];
    blockProducers.own[reg] = op;
    regs[op] = reg;
    blockProducers.invalidAfter = std::max(static_cast<int>(reg), blockProducers.invalidAfter);
}

void applyCall(BlockProducers& producers, BcOp callOp, Reg targetReg, int nresults)
{
    for (auto it = producers.own.begin(); it != producers.own.end();)
    {
        if (it->first >= targetReg)
        {
            it = producers.own.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = producers.cached.begin(); it != producers.cached.end();)
    {
        if (it->first >= targetReg)
        {
            it = producers.cached.erase(it);
        }
        else
        {
            ++it;
        }
    }
    if (nresults < 0)
    {
        producers.multiReturn = callOp;
        producers.multiReturnStart = targetReg;
        producers.invalidAfter = 255;
    }
    else
    {
        producers.invalidAfter = static_cast<int>(targetReg) - 1 + nresults;
    }
}

void addImmInput(BcFunction& func, BcInst& inst, bool value)
{
    BcOp op{BcOpKind::Imm, 0};
    size_t i = 0;
    for (; i < func.immediates.size(); i++)
    {
        BcImm& imm = func.immediates[i];
        if (imm.kind == BcImmKind::Boolean && imm.valueBoolean == value)
        {
            op.index = i;
            break;
        }
    }
    if (i == func.immediates.size())
    {
        func.immediates.push_back({BcImmKind::Boolean, {value}});
        op.index = i;
    }
    inst.ops.push_back(op);
}

void addImmInput(BcFunction& func, BcInst& inst, int32_t value)
{
    BcOp op{BcOpKind::Imm, 0};
    size_t i = 0;
    for (; i < func.immediates.size(); i++)
    {
        BcImm& imm = func.immediates[i];
        if (imm.kind == BcImmKind::Int && imm.valueInt == value)
        {
            op.index = i;
            break;
        }
    }
    if (i == func.immediates.size())
    {
        func.immediates.push_back({BcImmKind::Int});
        func.immediates.back().valueInt = value;
        op.index = i;
    }
    inst.ops.push_back(op);
}

void addImmInput(BcFunction& func, BcInst& inst, uint32_t value)
{
    BcOp op{BcOpKind::Imm, 0};
    func.immediates.push_back({BcImmKind::Import});
    func.immediates.back().valueImport = value;
    op.index = func.immediates.size() - 1;
    inst.ops.push_back(op);
}

void addVmConstInput(BcFunction& func, BcInst& inst, uint32_t idx)
{
    LUAU_ASSERT(idx < func.constants.size());
    inst.ops.push_back(BcOp{BcOpKind::VmConst, idx});
}

void addUpvalInput(BcFunction& func, BcInst& inst, uint32_t idx)
{
    LUAU_ASSERT(idx < func.nups);
    inst.ops.push_back(BcOp{BcOpKind::VmUpvalue, idx});
}

void addProtoInput(BcFunction& func, BcInst& inst, uint32_t idx)
{
    inst.ops.push_back(BcOp{BcOpKind::VmProto, idx});
}

void addVmRegInput(Producers& producers, BcFunction& func, BcOp block, BcInst& inst, Reg reg)
{
    std::optional<BcOp> source = findProducer(producers, func, block, reg);
    if (!source && isUnreachable(func, block))
    {
        inst.ops.push_back(BcOp{BcOpKind::VmReg, reg});
        return;
    }
    LUAU_ASSERT(source);
    inst.ops.push_back(*source);
}

void addJumpInput(std::unordered_map<uint32_t, BcOp>& blockByPC, BcInst& inst, int target)
{
    LUAU_ASSERT(!isFastCall(inst.op));
    if (target < 0)
    {
        LUAU_ASSERT(inst.op == LOP_LOADB);
        return;
    }
    auto it = blockByPC.find(target);
    LUAU_ASSERT(it != blockByPC.end());
    inst.ops.push_back(it->second);
}

BcOp addToPhi(BcFunction& func, BcOp op, BcOp proj)
{
    if (op.kind == BcOpKind::Phi)
    {
        BcPhi& phi = func.phiOp(op);
        for (auto p : phi.ops)
            if (p == proj)
                return op;
        phi.ops.push_back(proj);
        return op;
    }
    else
    {
        BcOp res = func.addPhi();
        BcPhi& phi = func.phiOp(res);
        phi.ops = {op, proj};
        return res;
    }
}

static const uint32_t kMaxCFGBlocks = 1000;

bool buildFunctionGraph(BcFunction& func, const Instruction code[], uint32_t codesize, std::vector<uint32_t>& lines, std::vector<uint32_t>& pcs)
{
    auto blocksResult = rebuildBlocks(func, code, codesize);
    std::unordered_map<uint32_t, BcOp> blockByPC = std::move(blocksResult.first);
    if (blockByPC.size() > kMaxCFGBlocks)
        return false;

    std::vector<LoopInfo> loops;

    Producers producers(func.blocks.size());
    pcs.resize(codesize);

    for (Reg i = 0; i < func.numparams; i++)
        addProducer(func.regs, producers, func.entryBlock, i, {BcOpKind::VmReg, i});

    // Create instructions.
    BcOp currentBlock = func.entryBlock;
    func.instructions.reserve(blocksResult.second);

    for (uint32_t i = 0; i < codesize;)
    {
        Instruction insn = code[i];
        LuauOpcode op = LuauOpcode(LUAU_INSN_OP(insn));
        int opLength = getOpLength(op);
        uint32_t aux = (opLength > 1 && i + 1 < codesize) ? code[i + 1] : 0;
        BcOp nodeOp = func.addInst();
        func.blockOp(currentBlock).appendInstruction(nodeOp);
        BcInst& node = func.instOp(nodeOp);
        if (i < lines.size())
            node.line = lines[i];
        node.op = op;

        pcs[i] = nodeOp.index;

        auto parseJump = [&](LuauOpcode op, int jumpTarget) -> void
        {
            node.op = op;
            switch (op)
            {
            case LOP_JUMPXEQKNIL:
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addImmInput(func, node, static_cast<bool>(aux >> 31));
                addJumpInput(blockByPC, node, jumpTarget);
                break;

            case LOP_JUMPXEQKB:
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addImmInput(func, node, static_cast<bool>(aux >> 31));
                addJumpInput(blockByPC, node, jumpTarget);
                addImmInput(func, node, static_cast<bool>(aux & 0x1));
                break;

            case LOP_JUMPXEQKN:
            case LOP_JUMPXEQKS:
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addImmInput(func, node, static_cast<bool>(aux >> 31));
                addJumpInput(blockByPC, node, jumpTarget);
                addVmConstInput(func, node, aux & 0xFFFFFF);
                break;

            case LOP_JUMPIF:
            case LOP_JUMPIFNOT:
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addJumpInput(blockByPC, node, jumpTarget);
                break;

            case LOP_JUMPIFEQ:
            case LOP_JUMPIFLE:
            case LOP_JUMPIFLT:
            case LOP_JUMPIFNOTEQ:
            case LOP_JUMPIFNOTLE:
            case LOP_JUMPIFNOTLT:
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addVmRegInput(producers, func, currentBlock, node, aux);
                addJumpInput(blockByPC, node, jumpTarget);
                break;

            case LOP_FORNPREP:
                // forg loop protocol: A, A+1, A+2 are used for iteration protocol; A+3, ... are loop variables
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 1);
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 2);
                addJumpInput(blockByPC, node, jumpTarget);
                func.regs[nodeOp] = LUAU_INSN_A(insn);
                addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), func.addProj(nodeOp, 0));
                addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn) + 1, func.addProj(nodeOp, 1));
                addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn) + 2, func.addProj(nodeOp, 2));
                break;

            case LOP_FORNLOOP:
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 1);
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 2);
                addJumpInput(blockByPC, node, jumpTarget);
                break;

            default:
                LUAU_UNREACHABLE();
            }
        };
        switch (op)
        {
        case LOP_NOP:
        case LOP_BREAK:
        case LOP_NATIVECALL:
            break;

        case LOP_LOADNIL:
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_LOADB:
            addImmInput(func, node, static_cast<bool>(LUAU_INSN_B(insn)));
            addJumpInput(blockByPC, node, getJumpTarget(insn, i));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_LOADN:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_D(insn)));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_LOADK:
            addVmConstInput(func, node, LUAU_INSN_D(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_MOVE:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_GETGLOBAL:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            addVmConstInput(func, node, aux);
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_SETGLOBAL:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addImmInput(func, node, static_cast<uint8_t>(LUAU_INSN_C(insn)));
            addVmConstInput(func, node, aux);
            break;

        case LOP_GETUPVAL:
            addUpvalInput(func, node, LUAU_INSN_B(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_SETUPVAL:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addUpvalInput(func, node, LUAU_INSN_B(insn));
            break;

        case LOP_CLOSEUPVALS:
            node.ops.push_back(BcOp{BcOpKind::VmReg, LUAU_INSN_A(insn)});
            break;

        case LOP_GETIMPORT:
        {
            addVmConstInput(func, node, LUAU_INSN_D(insn));
            addImmInput(func, node, aux);
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;
        }

        case LOP_GETTABLE:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_C(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_SETTABLE:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_C(insn));
            break;

        case LOP_GETUDATAKS:
        case LOP_GETTABLEKS:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            addVmConstInput(func, node, aux);
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_SETUDATAKS:
        case LOP_SETTABLEKS:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            addVmConstInput(func, node, aux);
            break;

        case LOP_GETTABLEN:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn) + 1));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_SETTABLEN:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn) + 1));
            break;

        case LOP_NEWCLOSURE:
            addProtoInput(func, node, LUAU_INSN_D(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_NAMECALLUDATA:
        case LOP_NAMECALL:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            addVmConstInput(func, node, aux);
            func.regs[nodeOp] = LUAU_INSN_A(insn);
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), func.addProj(nodeOp, 0));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn) + 1, func.addProj(nodeOp, 1));
            break;

        case LOP_CALL:
        {
            int nparams = LUAU_INSN_B(insn) - 1;
            int nresults = LUAU_INSN_C(insn) - 1;
            addImmInput(func, node, static_cast<int32_t>(nparams));
            addImmInput(func, node, static_cast<int32_t>(nresults));

            // Call target.
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            // Fixed arguments.
            for (int i = 1; i <= nparams; i++)
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + i);

            if (nparams < 0)
            {
                // all arguments prepared before call in the same block
                for (auto& inp : findProducersUpToTop(producers, func, currentBlock, LUAU_INSN_A(insn) + 1))
                    node.ops.push_back(inp);
            }

            BlockProducers& blockProducers = producers[currentBlock.index];
            applyCall(blockProducers, nodeOp, LUAU_INSN_A(insn), nresults);

            func.regs[nodeOp] = LUAU_INSN_A(insn);
            for (int i = 0; i < nresults; i++)
                addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn) + i, func.addProj(nodeOp, i));
            break;
        }

        case LOP_RETURN:
        {
            int nresults = LUAU_INSN_B(insn) - 1;
            addImmInput(func, node, static_cast<int32_t>(nresults));
            for (int i = 0; i < nresults; i++)
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + i);
            if (nresults < 0)
                for (auto& inp : findProducersUpToTop(producers, func, currentBlock, LUAU_INSN_A(insn)))
                    node.ops.push_back(inp);
            if (nresults == 0)
                node.ops.push_back(BcOp{BcOpKind::VmReg, LUAU_INSN_A(insn)});
            break;
        }

        case LOP_JUMP:
        {
            if (isJumpTrampoline(i, code, codesize))
            {
                // it is long jump trampoline
                int longOffset = LUAU_INSN_E(code[i + 1]);
                i += getOpLength(LOP_JUMP) + getOpLength(LOP_JUMPX);
                op = LuauOpcode(LUAU_INSN_OP(code[i]));
                opLength = getOpLength(op);
                aux = (opLength > 1 && i + 1 < codesize) ? code[i + 1] : 0;
                parseJump(op, i + longOffset);
            }
            else
                addJumpInput(blockByPC, node, getJumpTarget(insn, i));
            break;
        }

        case LOP_JUMPBACK:
            // repeat .. until loops use it for back edge.
            addJumpInput(blockByPC, node, getJumpTarget(insn, i));
            break;

        case LOP_JUMPXEQKNIL:
        case LOP_JUMPXEQKB:
        case LOP_JUMPXEQKN:
        case LOP_JUMPXEQKS:
        case LOP_JUMPIF:
        case LOP_JUMPIFNOT:
        case LOP_JUMPIFEQ:
        case LOP_JUMPIFLE:
        case LOP_JUMPIFLT:
        case LOP_JUMPIFNOTEQ:
        case LOP_JUMPIFNOTLE:
        case LOP_JUMPIFNOTLT:
        case LOP_FORNPREP:
        case LOP_FORNLOOP:
            parseJump(op, getJumpTarget(insn, i));
            break;

        case LOP_ADD:
        case LOP_SUB:
        case LOP_MUL:
        case LOP_DIV:
        case LOP_MOD:
        case LOP_POW:
        case LOP_AND:
        case LOP_OR:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_C(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_ADDK:
        case LOP_SUBK:
        case LOP_MULK:
        case LOP_DIVK:
        case LOP_MODK:
        case LOP_POWK:
        case LOP_ANDK:
        case LOP_ORK:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmConstInput(func, node, LUAU_INSN_C(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_CONCAT:
        {
            LUAU_ASSERT(LUAU_INSN_B(insn) <= LUAU_INSN_C(insn));
            for (Reg param = LUAU_INSN_B(insn); param <= LUAU_INSN_C(insn); param++)
                addVmRegInput(producers, func, currentBlock, node, param);
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;
        }

        case LOP_NOT:
        case LOP_MINUS:
        case LOP_LENGTH:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_NEWTABLE:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_B(insn)));
            addImmInput(func, node, static_cast<int32_t>(aux));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_DUPTABLE:
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            addVmConstInput(func, node, LUAU_INSN_D(insn));
            break;

        case LOP_SETLIST:
        {
            int count = LUAU_INSN_C(insn) - 1;
            addImmInput(func, node, static_cast<int32_t>(aux));
            addImmInput(func, node, static_cast<int32_t>(count));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            for (Reg param = 0; param < count; param++)
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn) + param);
            if (count < 0)
                for (auto inp : findProducersUpToTop(producers, func, currentBlock, LUAU_INSN_B(insn)))
                    node.ops.push_back(inp);
            break;
        }

        case LOP_FORGPREP:
        case LOP_FORGPREP_NEXT:
        case LOP_FORGPREP_INEXT:
        {
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 1);
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 2);
            int loopInsnPc = getJumpTarget(insn, i);
            addJumpInput(blockByPC, node, loopInsnPc);
            LUAU_ASSERT(loopInsnPc + 1 < static_cast<int>(codesize) && LuauOpcode(LUAU_INSN_OP(code[loopInsnPc])) == LOP_FORGLOOP);
            int32_t vars = code[loopInsnPc + 1] & 0xFF;
            func.regs[nodeOp] = LUAU_INSN_A(insn);
            for (int i = 0; i <= std::max(vars, 2); i++)
                addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn) + 2 + i, func.addProj(nodeOp, 2 + i));
            break;
        }

        case LOP_FORGLOOP:
        {
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 1);
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_A(insn) + 2);
            addImmInput(func, node, static_cast<bool>(aux >> 31));
            int32_t vars = aux & 0xFF;
            addImmInput(func, node, vars);
            addJumpInput(blockByPC, node, getJumpTarget(insn, i));
            break;
        }

        case LOP_FASTCALL:
            // Note that FASTCALL will read the actual call arguments, such as argument/result registers and counts, from the CALL instruction
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_A(insn)));
            // turn it in BcOp to CALL BcInst&.
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            break;

        case LOP_FASTCALL1:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_A(insn)));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            // turn it in BcOp to CALL BcInst&.
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            break;

        case LOP_FASTCALL2:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_A(insn)));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, aux & 0xFF);
            // turn it in BcOp to CALL BcInst&.
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            break;

        case LOP_FASTCALL2K:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_A(insn)));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmConstInput(func, node, aux);
            // turn it in BcOp to CALL BcInst&.
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            break;

        case LOP_FASTCALL3:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_A(insn)));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, aux & 0xFF);
            addVmRegInput(producers, func, currentBlock, node, (aux >> 8) & 0xFF);
            // turn it in BcOp to CALL BcInst&.
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            break;

        case LOP_GETVARARGS:
        {
            node.ops.push_back(BcOp{BcOpKind::VmReg, LUAU_INSN_A(insn)});
            int count = LUAU_INSN_B(insn) - 1;
            addImmInput(func, node, static_cast<int32_t>(count));
            func.regs[nodeOp] = LUAU_INSN_A(insn);
            if (count < 0)
            {
                BlockProducers& blockProducers = producers[currentBlock.index];
                blockProducers.multiReturn = nodeOp;
                blockProducers.multiReturnStart = LUAU_INSN_A(insn);
                blockProducers.invalidAfter = 255;
            }
            else
                for (int i = 0; i < count; i++)
                    addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn) + i, func.addProj(nodeOp, i));
            break;
        }

        case LOP_DUPCLOSURE:
            addVmConstInput(func, node, LUAU_INSN_D(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_PREPVARARGS:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_A(insn)));
            break;

        case LOP_LOADKX:
            addVmConstInput(func, node, aux);
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_JUMPX:
            LUAU_ASSERT(!"Shouldn't parse it directly");
            addJumpInput(blockByPC, node, getJumpTarget(insn, i));
            break;

        case LOP_COVERAGE:
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_E(insn)));
            break;

        case LOP_CAPTURE:
        {
            uint8_t captureType = LUAU_INSN_A(insn);
            addImmInput(func, node, static_cast<int32_t>(captureType));
            if (captureType == LCT_VAL || captureType == LCT_REF)
                addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            else
                addUpvalInput(func, node, LUAU_INSN_B(insn));
            addImmInput(func, node, static_cast<int32_t>(LUAU_INSN_C(insn)));
            break;
        }

        case LOP_SUBRK:
        case LOP_DIVRK:
            addVmConstInput(func, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_C(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_IDIV:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_C(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP_IDIVK:
            addVmRegInput(producers, func, currentBlock, node, LUAU_INSN_B(insn));
            addVmConstInput(func, node, LUAU_INSN_C(insn));
            addProducer(func.regs, producers, currentBlock, LUAU_INSN_A(insn), nodeOp);
            break;

        case LOP__COUNT:
            LUAU_UNREACHABLE();
        }

        if (isLoopJump(op))
        {
            int target = getJumpTarget(insn, i);
            LUAU_ASSERT(target >= 0 && blockByPC.count(target) > 0);
            loops.push_back({blockByPC[target], currentBlock});
        }

        i += opLength;
        if (blockByPC.count(i) > 0)
            currentBlock = blockByPC[i];
    }

    for (auto& loop : loops)
    {
        std::unordered_set<BcOp, BcOpHash> visited;
        std::vector<BcOp> queue;
        queue.push_back(loop.exit);
        while (queue.size() > 0)
        {
            BcOp cur = queue.back();
            queue.pop_back();
            if (visited.count(cur) > 0)
                continue;
            visited.insert(cur);
            BcBlock& curBlock = func.blockOp(cur);

            for (auto op : curBlock.ops)
                for (auto& inp : func.instOp(op).ops)
                {
                    auto regIt = func.regs.find(inp);
                    if (regIt == func.regs.end())
                        continue;
                    // try to find it in the same loop before
                    if (hasProducerBefore(producers, func, loop.entry, cur, op, regIt->second))
                        continue;
                    if (auto forwardInput = findForwardProducerInRange(producers, func, cur, loop.exit, op, regIt->second))
                    {
                        inp = addToPhi(func, inp, *forwardInput);
                        func.regs[inp] = regIt->second;
                    }
                }

            for (auto& [ctrl, pred] : curBlock.predecessors)
                if (ctrl != BcBlockEdgeKind::Loop && visited.count(pred) == 0)
                    queue.push_back(pred);
        }
    }
    return true;
}

std::optional<BcFunction> fromFunctionBytecode(std::string bytecode, std::vector<std::string_view>& strings)
{
    BcFunction fn;
    size_t offset = 0;
    const char* data = bytecode.data();
    fn.maxstacksize = read<uint8_t>(data, offset);
    fn.numparams = read<uint8_t>(data, offset);
    fn.nups = read<uint8_t>(data, offset);
    fn.is_vararg = read<uint8_t>(data, offset);
    fn.flags = read<uint8_t>(data, offset);

    uint32_t typesSize = readVarInt(data, offset);
    if (typesSize > 0)
    {
        uint32_t typeInfoSize = readVarInt(data, offset);
        uint32_t typedUpvalSize = readVarInt(data, offset);
        uint32_t typedLocalSize = readVarInt(data, offset);
        fn.typeInfo = bytecode.substr(offset, typeInfoSize);
        offset += typeInfoSize;

        fn.upvalueTypes.resize(typedUpvalSize);
        for (uint32_t i = 0; i < typedUpvalSize; i++)
            fn.upvalueTypes[i] = static_cast<LuauBytecodeType>(read<uint8_t>(data, offset));

        fn.localTypes.resize(typedLocalSize);
        for (uint32_t i = 0; i < typedLocalSize; i++)
        {
            LuauBytecodeType type = static_cast<LuauBytecodeType>(read<uint8_t>(data, offset));
            uint8_t reg = read<uint8_t>(data, offset);
            uint32_t startpc = readVarInt(data, offset);
            uint32_t endpc = startpc + readVarInt(data, offset);
            fn.localTypes[i] = {type, reg, startpc, endpc};
        }
    }

    // store pointer to bytecode
    int32_t codesize = readVarInt(data, offset);
    const Instruction* code = reinterpret_cast<const Instruction*>(data + offset);

    offset += codesize * sizeof(Instruction);

    // read constants
    const uint32_t sizek = readVarInt(data, offset);
    fn.constants.resize(sizek);
    for (uint32_t i = 0; i < sizek; i++)
    {
        uint8_t constType = read<uint8_t>(data, offset);
        switch (constType)
        {
        case LBC_CONSTANT_NIL:
            fn.constants[i].kind = BcVmConstKind::Nil;
            break;

        case LBC_CONSTANT_BOOLEAN:
        {
            fn.constants[i].kind = BcVmConstKind::Boolean;
            fn.constants[i].valueBoolean = read<uint8_t>(data, offset);
            break;
        }

        case LBC_CONSTANT_NUMBER:
        {
            fn.constants[i].kind = BcVmConstKind::Number;
            fn.constants[i].valueNumber = read<double>(data, offset);
            break;
        }

        case LBC_CONSTANT_VECTOR:
        {
            fn.constants[i].kind = BcVmConstKind::Vector;
            fn.constants[i].valueVector[0] = read<float>(data, offset);
            fn.constants[i].valueVector[1] = read<float>(data, offset);
            fn.constants[i].valueVector[2] = read<float>(data, offset);
            fn.constants[i].valueVector[3] = read<float>(data, offset);
            break;
        }

        case LBC_CONSTANT_STRING:
        {
            fn.constants[i].kind = BcVmConstKind::String;
            fn.constants[i].valueString = readString(strings, data, offset);
            break;
        }

        case LBC_CONSTANT_IMPORT:
        {
            fn.constants[i].kind = BcVmConstKind::Import;
            fn.constants[i].valueImport = read<uint32_t>(data, offset);
            break;
        }

        case LBC_CONSTANT_TABLE:
        case LBC_CONSTANT_TABLE_WITH_CONSTANTS:
        {
            fn.constants[i].kind = BcVmConstKind::Table;
            fn.constants[i].valueTable = uint32_t(fn.tableShapes.size());

            BytecodeBuilder::TableShape shape;
            shape.length = readVarInt(data, offset);
            shape.hasConstants = constType == LBC_CONSTANT_TABLE_WITH_CONSTANTS;

            for (uint32_t i = 0; i < shape.length; ++i)
            {
                uint32_t key = readVarInt(data, offset);
                LUAU_ASSERT(key < sizek);
                shape.keys[i] = key;
                if (shape.hasConstants)
                {
                    int32_t value = read<int32_t>(data, offset);
                    LUAU_ASSERT(value < static_cast<int32_t>(sizek));
                    shape.constants[i] = value;
                }
            }
            fn.tableShapes.push_back(shape);
            break;
        }

        case LBC_CONSTANT_CLOSURE:
        {
            fn.constants[i].kind = BcVmConstKind::Closure;
            fn.constants[i].valueClosure = readVarInt(data, offset);
            break;
        }

        case LBC_CONSTANT_INTEGER:
        {
            fn.constants[i].kind = BcVmConstKind::Integer;
            bool isNegative = read<uint8_t>(data, offset);
            uint64_t magnitude = readVarInt64(data, offset);
            fn.constants[i].valueInteger = isNegative ? (int64_t)(~magnitude + 1) : (int64_t)magnitude;
            break;
        }
        default:
            LUAU_ASSERT(!"Unknown constant type!");
        }
    }

    uint32_t psize = readVarInt(data, offset);
    fn.protos.resize(psize);
    for (uint32_t i = 0; i < psize; i++)
        fn.protos[i] = readVarInt(data, offset);

    fn.linedefined = readVarInt(data, offset);
    fn.debugname = readString(strings, data, offset);

    uint8_t lineinfo = read<uint8_t>(data, offset);
    std::vector<uint32_t> lines;

    if (lineinfo != 0)
    {
        uint8_t linegaplog2 = read<uint8_t>(data, offset);

        int intervals = ((codesize - 1) >> linegaplog2) + 1;
        int absoffset = (codesize + 3) & ~3;

        const int sizelineinfo = absoffset + intervals * sizeof(int);
        std::vector<uint8_t> lineinfo;
        lineinfo.resize(sizelineinfo);
        int* abslineinfo = reinterpret_cast<int*>(lineinfo.data() + absoffset);

        uint8_t lastoffset = 0;
        for (int i = 0; i < codesize; i++)
        {
            lastoffset += read<uint8_t>(data, offset);
            lineinfo[i] = lastoffset;
        }

        int lastline = 0;
        for (int i = 0; i < intervals; i++)
        {
            lastline += read<int32_t>(data, offset);
            abslineinfo[i] = lastline;
        }
        lines.resize(codesize);
        for (int i = 0; i < codesize; i++)
            lines[i] = abslineinfo[i >> linegaplog2] + lineinfo[i];
    }

    uint8_t debuginfo = read<uint8_t>(data, offset);

    if (debuginfo != 0)
    {
        const int sizelocvars = readVarInt(data, offset);
        fn.locals.resize(sizelocvars);

        for (int i = 0; i < sizelocvars; i++)
        {
            std::string_view varname = readString(strings, data, offset);
            uint32_t startpc = readVarInt(data, offset);
            uint32_t endpc = readVarInt(data, offset);
            uint8_t reg = read<uint8_t>(data, offset);
            fn.locals[i] = {varname, reg, startpc, endpc};
        }

        const int sizeupvalues = readVarInt(data, offset);
        fn.upvalueNames.resize(sizeupvalues);

        for (int i = 0; i < sizeupvalues; i++)
            fn.upvalueNames[i] = readString(strings, data, offset);
    }

    std::vector<uint32_t> insnsPC;
    if (!buildFunctionGraph(fn, code, codesize, lines, insnsPC))
        return {};

    for (TypedLocal& l : fn.localTypes)
    {
        l.startpc = l.startpc < insnsPC.size() ? insnsPC[l.startpc] : codesize;
        l.endpc = l.endpc < insnsPC.size() ? insnsPC[l.endpc] : codesize;
    }

    for (DebugLocal& l : fn.locals)
    {
        l.startpc = l.startpc < insnsPC.size() ? insnsPC[l.startpc] : codesize;
        l.endpc = l.endpc < insnsPC.size() ? insnsPC[l.endpc] : codesize;
    }

    return {fn};
}

std::vector<BcOp> reschedule(BcFunction& func)
{
    std::vector<BcOp> sortedBlocks;
    sortedBlocks.reserve(func.blocks.size());
    for (uint32_t i = 0; i < func.blocks.size(); i++)
        sortedBlocks.push_back(BcOp{BcOpKind::Block, i});

    std::sort(
        sortedBlocks.begin(),
        sortedBlocks.end(),
        [&](BcOp opA, BcOp opB)
        {
            const BcBlock& a = func.blockOp(opA);
            const BcBlock& b = func.blockOp(opB);

            return a.sortkey < b.sortkey;
        }
    );

    LUAU_ASSERT(sortedBlocks.back() == func.exitBlock);
    sortedBlocks.pop_back();

    return sortedBlocks;
}

uint8_t getRegister(BcFunction& func, BcOp op)
{
    switch (op.kind)
    {
    case BcOpKind::Phi:
    {
        BcPhi& phi = func.phiOp(op);
        LUAU_ASSERT(phi.ops.size() > 0);
        Reg res = getRegister(func, phi.ops[0]);
        for (auto phiOp : phi.ops)
            LUAU_ASSERT(res == getRegister(func, phiOp));
        return res;
    }
    case BcOpKind::Inst:
    {
        auto it = func.regs.find(op);
        LUAU_ASSERT(it != func.regs.end());
        return it->second;
    }
    case BcOpKind::Proj:
    {
        BcProj& proj = func.projOp(op);
        Reg base = getRegister(func, proj.op);
        return base + proj.index;
    }
    case BcOpKind::VmReg:
        return op.index;
    default:
        LUAU_UNREACHABLE();
    }
    return 0;
}

template<typename T>
T getImm(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::Imm);
    BcImm& imm = func.immOp(inp);
    LUAU_ASSERT(imm.kind == BcImmKind::Int);
    return static_cast<T>(imm.valueInt);
}

template<>
bool getImm(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::Imm);
    BcImm& imm = func.immOp(inp);
    LUAU_ASSERT(imm.kind == BcImmKind::Boolean);
    return imm.valueBoolean;
}

template<>
uint32_t getImm(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::Imm);
    BcImm& imm = func.immOp(inp);
    LUAU_ASSERT(imm.kind == BcImmKind::Import);
    return imm.valueImport;
}

uint8_t getVmConstInput(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::VmConst);
    LUAU_ASSERT(inp.index < func.constants.size());
    return uint8_t(inp.index);
}

uint8_t getUpvalInput(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::VmUpvalue);
    LUAU_ASSERT(inp.index < func.nups);
    return uint8_t(inp.index);
}

uint16_t getProtoInput(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::VmProto);
    return inp.index;
}

uint8_t getRegInput(BcFunction& func, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    return getRegister(func, insn.ops[index]);
}

struct JumpInfo
{
    LuauOpcode op;
    uint32_t instructionPC;
    BcOp targetBlock;
};

using Jumps = std::vector<JumpInfo>;

void recordJump(BytecodeBuilder& bcb, Jumps& jumps, BcInst& insn, uint8_t index)
{
    LUAU_ASSERT(index < insn.ops.size());
    BcOp inp = insn.ops[index];
    LUAU_ASSERT(inp.kind == BcOpKind::Block);
    jumps.push_back({insn.op, static_cast<uint32_t>(bcb.getInstructionCount()), inp});
}

void patchJump(BytecodeBuilder& bcb, BcFunction& func, JumpInfo& jump)
{
    BcBlock& target = func.blockOp(jump.targetBlock);
    LUAU_ASSERT(target.startpc != kBlockNoStartPc);
    if (isJumpD(jump.op))
        LUAU_ASSERT(bcb.patchJumpD(jump.instructionPC, target.startpc));
    else if (isSkipC(jump.op))
        LUAU_ASSERT(bcb.patchSkipC(jump.instructionPC, target.startpc));
}

void emitInstruction(BytecodeBuilder& bcb, Jumps& jumps, BcFunction& func, BcOp insnOp)
{
    BcInst& insn = func.instOp(insnOp);
    bcb.setDebugLine(insn.line);
    switch (insn.op)
    {
    case LOP_NOP:
    case LOP_BREAK:
    case LOP_NATIVECALL:
        bcb.emitABC(insn.op, 0, 0, 0);
        break;

    case LOP_LOADNIL:
        bcb.emitABC(LOP_LOADNIL, getRegister(func, insnOp), 0, 0);
        break;

    case LOP_LOADB:
    {
        if (insn.ops.size() > 1)
            recordJump(bcb, jumps, insn, 1);
        bcb.emitABC(LOP_LOADB, getRegister(func, insnOp), getImm<bool>(func, insn, 0), 0);
        break;
    }

    case LOP_LOADN:
        bcb.emitAD(LOP_LOADN, getRegister(func, insnOp), getImm<int16_t>(func, insn, 0));
        break;

    case LOP_LOADK:
        bcb.emitAD(LOP_LOADK, getRegister(func, insnOp), getVmConstInput(func, insn, 0));
        break;

    case LOP_MOVE:
        bcb.emitABC(LOP_MOVE, getRegister(func, insnOp), getRegInput(func, insn, 0), 0);
        break;

    case LOP_GETGLOBAL:
        bcb.emitABC(LOP_GETGLOBAL, getRegister(func, insnOp), 0, getImm<uint8_t>(func, insn, 0));
        bcb.emitAux(getVmConstInput(func, insn, 1));
        break;

    case LOP_SETGLOBAL:
        bcb.emitABC(LOP_SETGLOBAL, getRegInput(func, insn, 0), 0, getImm<uint8_t>(func, insn, 1));
        bcb.emitAux(getVmConstInput(func, insn, 2));
        break;

    case LOP_GETUPVAL:
        bcb.emitABC(LOP_GETUPVAL, getRegister(func, insnOp), getUpvalInput(func, insn, 0), 0);
        break;

    case LOP_SETUPVAL:
        bcb.emitABC(LOP_SETUPVAL, getRegInput(func, insn, 0), getUpvalInput(func, insn, 1), 0);
        break;

    case LOP_CLOSEUPVALS:
        LUAU_ASSERT(insn.ops.size() == 1 && insn.ops[0].kind == BcOpKind::VmReg);
        bcb.emitABC(LOP_CLOSEUPVALS, insn.ops[0].index, 0, 0);
        break;

    case LOP_GETIMPORT:
    {
        bcb.emitAD(LOP_GETIMPORT, getRegister(func, insnOp), getVmConstInput(func, insn, 0));
        bcb.emitAux(getImm<uint32_t>(func, insn, 1));
        break;
    }

    case LOP_GETTABLE:
        bcb.emitABC(LOP_GETTABLE, getRegister(func, insnOp), getRegInput(func, insn, 0), getRegInput(func, insn, 1));
        break;

    case LOP_SETTABLE:
        bcb.emitABC(LOP_SETTABLE, getRegInput(func, insn, 0), getRegInput(func, insn, 1), getRegInput(func, insn, 2));
        break;

    case LOP_GETUDATAKS:
    case LOP_GETTABLEKS:
        bcb.emitABC(insn.op, getRegister(func, insnOp), getRegInput(func, insn, 0), getImm<uint8_t>(func, insn, 1));
        bcb.emitAux(getVmConstInput(func, insn, 2));
        break;

    case LOP_SETUDATAKS:
    case LOP_SETTABLEKS:
        bcb.emitABC(insn.op, getRegInput(func, insn, 0), getRegInput(func, insn, 1), getImm<uint8_t>(func, insn, 2));
        bcb.emitAux(getVmConstInput(func, insn, 3));
        break;

    case LOP_GETTABLEN:
        bcb.emitABC(LOP_GETTABLEN, getRegister(func, insnOp), getRegInput(func, insn, 0), getImm<uint8_t>(func, insn, 1) - 1);
        break;

    case LOP_SETTABLEN:
        bcb.emitABC(LOP_SETTABLEN, getRegInput(func, insn, 0), getRegInput(func, insn, 1), getImm<uint8_t>(func, insn, 2) - 1);
        break;

    case LOP_NEWCLOSURE:
        bcb.emitAD(LOP_NEWCLOSURE, getRegister(func, insnOp), getProtoInput(func, insn, 0));
        break;

    case LOP_NAMECALLUDATA:
    case LOP_NAMECALL:
        bcb.emitABC(insn.op, getRegister(func, insnOp), getRegInput(func, insn, 0), getImm<uint8_t>(func, insn, 1));
        bcb.emitAux(getVmConstInput(func, insn, 2));
        break;

    case LOP_CALL:
        bcb.emitABC(LOP_CALL, getRegInput(func, insn, 2), getImm<int32_t>(func, insn, 0) + 1, getImm<int32_t>(func, insn, 1) + 1);
        break;

    case LOP_RETURN:
    {
        LUAU_ASSERT(insn.ops.size() > 1);
        bcb.emitABC(LOP_RETURN, getRegInput(func, insn, 1), getImm<int32_t>(func, insn, 0) + 1, 0);
        break;
    }

    case LOP_JUMP:
        recordJump(bcb, jumps, insn, 0);
        bcb.emitAD(LOP_JUMP, 0, 0);
        break;

    case LOP_JUMPBACK:
        recordJump(bcb, jumps, insn, 0);
        bcb.emitAD(LOP_JUMPBACK, 0, 0);
        break;

    case LOP_JUMPIFNOT:
    case LOP_JUMPIF:
        recordJump(bcb, jumps, insn, 1);
        bcb.emitAD(insn.op, getRegInput(func, insn, 0), 0);
        break;

    case LOP_JUMPIFEQ:
    case LOP_JUMPIFLE:
    case LOP_JUMPIFLT:
    case LOP_JUMPIFNOTEQ:
    case LOP_JUMPIFNOTLE:
    case LOP_JUMPIFNOTLT:
        recordJump(bcb, jumps, insn, 2);
        bcb.emitAD(insn.op, getRegInput(func, insn, 0), 0);
        bcb.emitAux(getRegInput(func, insn, 1));
        break;

    case LOP_ADD:
    case LOP_SUB:
    case LOP_MUL:
    case LOP_DIV:
    case LOP_MOD:
    case LOP_POW:
    case LOP_AND:
    case LOP_OR:
        bcb.emitABC(insn.op, getRegister(func, insnOp), getRegInput(func, insn, 0), getRegInput(func, insn, 1));
        break;

    case LOP_ADDK:
    case LOP_SUBK:
    case LOP_MULK:
    case LOP_DIVK:
    case LOP_MODK:
    case LOP_POWK:
    case LOP_ANDK:
    case LOP_ORK:
        bcb.emitABC(insn.op, getRegister(func, insnOp), getRegInput(func, insn, 0), getVmConstInput(func, insn, 1));
        break;

    case LOP_CONCAT:
        LUAU_ASSERT(insn.ops.size() > 0);
        bcb.emitABC(LOP_CONCAT, getRegister(func, insnOp), getRegInput(func, insn, 0), getRegInput(func, insn, insn.ops.size() - 1));
        break;

    case LOP_NOT:
    case LOP_MINUS:
    case LOP_LENGTH:
        bcb.emitABC(insn.op, getRegister(func, insnOp), getRegInput(func, insn, 0), 0);
        break;

    case LOP_NEWTABLE:
        bcb.emitABC(LOP_NEWTABLE, getRegister(func, insnOp), getImm<int32_t>(func, insn, 0), 0);
        bcb.emitAux(getImm<int32_t>(func, insn, 1));
        break;

    case LOP_DUPTABLE:
        bcb.emitAD(LOP_DUPTABLE, getRegister(func, insnOp), getVmConstInput(func, insn, 0));
        break;

    case LOP_SETLIST:
        LUAU_ASSERT(insn.ops.size() > 2);
        bcb.emitABC(LOP_SETLIST, getRegInput(func, insn, 2), getRegInput(func, insn, 3), getImm<int32_t>(func, insn, 1) + 1);
        bcb.emitAux(getImm<int32_t>(func, insn, 0));
        break;

    case LOP_FORNPREP:
        recordJump(bcb, jumps, insn, 3);
        bcb.emitAD(LOP_FORNPREP, getRegInput(func, insn, 0), 0);
        break;

    case LOP_FORNLOOP:
        recordJump(bcb, jumps, insn, 3);
        bcb.emitAD(LOP_FORNLOOP, getRegInput(func, insn, 0), 0);
        break;

    case LOP_FORGPREP:
    case LOP_FORGPREP_NEXT:
    case LOP_FORGPREP_INEXT:
        recordJump(bcb, jumps, insn, 3);
        bcb.emitAD(insn.op, getRegInput(func, insn, 0), 0);
        break;

    case LOP_FORGLOOP:
        recordJump(bcb, jumps, insn, 5);
        bcb.emitAD(LOP_FORGLOOP, getRegInput(func, insn, 0), 0);
        bcb.emitAux(static_cast<uint32_t>(getImm<bool>(func, insn, 3)) << 31 | getImm<int32_t>(func, insn, 4));
        break;

    case LOP_FASTCALL:
        bcb.emitABC(LOP_FASTCALL, getImm<int32_t>(func, insn, 0), 0, getImm<int32_t>(func, insn, 1));
        break;

    case LOP_FASTCALL1:
        bcb.emitABC(LOP_FASTCALL1, getImm<int32_t>(func, insn, 0), getRegInput(func, insn, 1), getImm<int32_t>(func, insn, 2));
        break;

    case LOP_FASTCALL2:
        bcb.emitABC(LOP_FASTCALL2, getImm<int32_t>(func, insn, 0), getRegInput(func, insn, 1), getImm<int32_t>(func, insn, 3));
        bcb.emitAux(getRegInput(func, insn, 2));
        break;

    case LOP_FASTCALL2K:
        bcb.emitABC(LOP_FASTCALL2K, getImm<int32_t>(func, insn, 0), getRegInput(func, insn, 1), getImm<int32_t>(func, insn, 3));
        bcb.emitAux(getVmConstInput(func, insn, 2));
        break;

    case LOP_FASTCALL3:
        bcb.emitABC(LOP_FASTCALL3, getImm<int32_t>(func, insn, 0), getRegInput(func, insn, 1), getImm<int32_t>(func, insn, 4));
        bcb.emitAux(getRegInput(func, insn, 2) | static_cast<uint32_t>(getRegInput(func, insn, 3)) << 8);
        break;

    case LOP_GETVARARGS:
        LUAU_ASSERT(insn.ops.size() == 2 && insn.ops[0].kind == BcOpKind::VmReg);
        bcb.emitABC(LOP_GETVARARGS, insn.ops[0].index, getImm<int32_t>(func, insn, 1) + 1, 0);
        break;

    case LOP_DUPCLOSURE:
        bcb.emitAD(LOP_DUPCLOSURE, getRegister(func, insnOp), getVmConstInput(func, insn, 0));
        break;

    case LOP_PREPVARARGS:
        bcb.emitAD(LOP_PREPVARARGS, getImm<int32_t>(func, insn, 0), 0);
        break;

    case LOP_LOADKX:
        bcb.emitAD(LOP_LOADKX, getRegister(func, insnOp), 0);
        bcb.emitAux(getVmConstInput(func, insn, 0));
        break;

    case LOP_JUMPX:
        recordJump(bcb, jumps, insn, 0);
        bcb.emitE(LOP_JUMPX, 0);
        break;

    case LOP_COVERAGE:
        bcb.emitE(LOP_COVERAGE, getImm<int32_t>(func, insn, 0));
        break;

    case LOP_CAPTURE:
    {
        uint8_t captureType = getImm<int32_t>(func, insn, 0);
        if (captureType == LCT_VAL || captureType == LCT_REF)
            bcb.emitABC(LOP_CAPTURE, captureType, getRegInput(func, insn, 1), getImm<int32_t>(func, insn, 2));
        else
            bcb.emitABC(LOP_CAPTURE, captureType, getUpvalInput(func, insn, 1), getImm<int32_t>(func, insn, 2));
        break;
    }

    case LOP_SUBRK:
    case LOP_DIVRK:
        bcb.emitABC(insn.op, getRegister(func, insnOp), getVmConstInput(func, insn, 0), getRegInput(func, insn, 1));
        break;

    case LOP_JUMPXEQKNIL:
        recordJump(bcb, jumps, insn, 2);
        bcb.emitAD(LOP_JUMPXEQKNIL, getRegInput(func, insn, 0), 0);
        bcb.emitAux(static_cast<uint32_t>(getImm<bool>(func, insn, 1)) << 31);
        break;

    case LOP_JUMPXEQKB:
        recordJump(bcb, jumps, insn, 2);
        bcb.emitAD(LOP_JUMPXEQKB, getRegInput(func, insn, 0), 0);
        bcb.emitAux(static_cast<uint32_t>(getImm<bool>(func, insn, 1)) << 31 | static_cast<uint32_t>(getImm<bool>(func, insn, 3)));
        break;

    case LOP_JUMPXEQKN:
    case LOP_JUMPXEQKS:
        recordJump(bcb, jumps, insn, 2);
        bcb.emitAD(insn.op, getRegInput(func, insn, 0), 0);
        bcb.emitAux(static_cast<uint32_t>(getImm<bool>(func, insn, 1)) << 31 | getVmConstInput(func, insn, 3));
        break;

    case LOP_IDIV:
        bcb.emitABC(LOP_IDIV, getRegister(func, insnOp), getRegInput(func, insn, 0), getRegInput(func, insn, 1));
        break;

    case LOP_IDIVK:
        bcb.emitABC(LOP_IDIVK, getRegister(func, insnOp), getRegInput(func, insn, 0), getVmConstInput(func, insn, 1));
        break;

    case LOP__COUNT:
        LUAU_UNREACHABLE();
    }
}

std::vector<uint32_t> emitBytecode(BytecodeBuilder& bcb, BcFunction& func)
{
    std::vector<BcOp> schedule = reschedule(func);
    std::vector<uint32_t> insnsPC;
    insnsPC.resize(func.instructions.size());
    Jumps jumps;

    for (size_t i = 0; i < schedule.size(); i++)
    {
        BcOp blockOp = schedule[i];
        BcBlock& block = func.blockOp(blockOp);
        std::optional<BcOp> fallthrough = getFallthrough(block);
        if (fallthrough && *fallthrough != func.exitBlock && (i + 1 >= schedule.size() || *fallthrough != schedule[i + 1]))
        {
            BcOp jumpOp = func.addInst();
            BcInst& jump = func.instOp(jumpOp);
            jump.op = LOP_JUMP;
            block.appendInstruction(jumpOp);
            jump.ops.push_back(*fallthrough);
        }
        block.startpc = bcb.getDebugPC();
        for (BcOp op : block.ops)
        {
            LUAU_ASSERT(op.kind == BcOpKind::Inst);
            insnsPC[op.index] = bcb.getDebugPC();
            emitInstruction(bcb, jumps, func, op);
        }
    }

    for (auto& jump : jumps)
        patchJump(bcb, func, jump);

    return insnsPC;
}

std::string toFunctionBytecode(BcFunction& fn)
{
    BytecodeBuilder bcb;
    return toFunctionBytecode(bcb, fn);
}

std::string toFunctionBytecode(BytecodeBuilder& bcb, BcFunction& fn)
{
    uint32_t functionId = bcb.beginFunction(fn.numparams, fn.is_vararg);
    if (fn.debugname != "")
        bcb.setDebugFunctionName({fn.debugname.data(), fn.debugname.size()});
    bcb.setDebugFunctionLineDefined(fn.linedefined);
    bcb.setFunctionTypeInfo(fn.typeInfo);
    for (LuauBytecodeType t : fn.upvalueTypes)
        bcb.pushUpvalTypeInfo(t);
    for (auto& upval : fn.upvalueNames)
        bcb.pushDebugUpval({upval.data(), upval.size()});

    for (auto& c : fn.constants)
    {
        switch (c.kind)
        {
        case BcVmConstKind::Nil:
            bcb.addConstantNil();
            break;

        case BcVmConstKind::Boolean:
            bcb.addConstantBoolean(c.valueBoolean);
            break;

        case BcVmConstKind::Number:
            bcb.addConstantNumber(c.valueNumber);
            break;

        case BcVmConstKind::Vector:
            bcb.addConstantVector(c.valueVector[0], c.valueVector[1], c.valueVector[2], c.valueVector[3]);
            break;

        case BcVmConstKind::String:
            bcb.addConstantString({c.valueString.data(), c.valueString.size()});
            break;

        case BcVmConstKind::Import:
            bcb.addImport(c.valueImport);
            break;

        case BcVmConstKind::Table:
        {
            LUAU_ASSERT(c.valueTable < fn.tableShapes.size());
            bcb.addConstantTable(fn.tableShapes[c.valueTable]);
            break;
        }

        case BcVmConstKind::Closure:
            bcb.addConstantClosure(c.valueClosure);
            break;

        case BcVmConstKind::Integer:
            bcb.addConstantInteger(c.valueInteger);
            break;
        }
    }

    for (auto fid : fn.protos)
        bcb.addChildFunction(fid);

    std::vector<uint32_t> insnsPC = emitBytecode(bcb, fn);

    for (auto& local : fn.localTypes)
    {
        uint32_t startpc = local.startpc < insnsPC.size() ? insnsPC[local.startpc] : bcb.getDebugPC();
        uint32_t endpc = local.endpc < insnsPC.size() ? insnsPC[local.endpc] : bcb.getDebugPC();
        bcb.pushLocalTypeInfo(local.type, local.reg, startpc, endpc);
    }
    for (auto& local : fn.locals)
    {
        uint32_t startpc = local.startpc < insnsPC.size() ? insnsPC[local.startpc] : bcb.getDebugPC();
        uint32_t endpc = local.endpc < insnsPC.size() ? insnsPC[local.endpc] : bcb.getDebugPC();
        bcb.pushDebugLocal({local.varname.data(), local.varname.size()}, local.reg, startpc, endpc);
    }

    bcb.foldJumps();

    bcb.expandJumps();

    bcb.endFunction(fn.maxstacksize, fn.nups, fn.flags);

    return bcb.getFunctionData(functionId);
}

}; // namespace Bytecode
}; // namespace Luau
