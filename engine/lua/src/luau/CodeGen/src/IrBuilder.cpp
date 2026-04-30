// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/IrBuilder.h"

#include "Luau/Bytecode.h"
#include "Luau/BytecodeAnalysis.h"
#include "Luau/IrData.h"
#include "Luau/IrUtils.h"

#include "IrTranslation.h"

#include "lapi.h"

#include <string.h>

LUAU_FASTFLAG(LuauCodegenSetBlockEntryState3)

namespace Luau
{
namespace CodeGen
{

constexpr unsigned kNoAssociatedBlockIndex = ~0u;

IrBuilder::IrBuilder(const HostIrHooks& hostHooks)
    : hostHooks(hostHooks)
    , constantMap({IrConstKind::Tag, ~0ull})
{
}

static bool hasTypedParameters(const BytecodeTypeInfo& typeInfo)
{
    for (auto el : typeInfo.argumentTypes)
    {
        if (el != LBC_TYPE_ANY)
            return true;
    }

    return false;
}

static void buildArgumentTypeChecks(IrBuilder& build, IrOp entry)
{
    const BytecodeTypeInfo& typeInfo = FFlag::LuauCodegenSetBlockEntryState3 ? build.function.bcOriginalTypeInfo : build.function.bcTypeInfo;
    CODEGEN_ASSERT(hasTypedParameters(typeInfo));

    if (FFlag::LuauCodegenSetBlockEntryState3)
        build.function.blockOp(entry).flags |= kBlockFlagEntryArgCheck;

    for (size_t i = 0; i < typeInfo.argumentTypes.size(); i++)
    {
        uint8_t et = typeInfo.argumentTypes[i];

        uint8_t tag = et & ~LBC_TYPE_OPTIONAL_BIT;
        uint8_t optional = et & LBC_TYPE_OPTIONAL_BIT;

        if (tag == LBC_TYPE_ANY)
            continue;

        IrOp load = build.inst(IrCmd::LOAD_TAG, build.vmReg(uint8_t(i)));

        IrOp nextCheck;
        if (optional)
        {
            nextCheck = build.block(IrBlockKind::Internal);
            IrOp fallbackCheck = build.block(IrBlockKind::Internal);

            build.inst(IrCmd::JUMP_EQ_TAG, load, build.constTag(LUA_TNIL), nextCheck, fallbackCheck);

            build.beginBlock(fallbackCheck);

            if (FFlag::LuauCodegenSetBlockEntryState3)
                build.function.blockOp(fallbackCheck).flags |= kBlockFlagEntryArgCheck;
        }

        switch (tag)
        {
        case LBC_TYPE_NIL:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TNIL), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_BOOLEAN:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TBOOLEAN), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_NUMBER:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TNUMBER), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_INTEGER:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TINTEGER), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_STRING:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TSTRING), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_TABLE:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TTABLE), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_FUNCTION:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TFUNCTION), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_THREAD:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TTHREAD), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_USERDATA:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TUSERDATA), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_VECTOR:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TVECTOR), build.vmExit(kVmExitEntryGuardPc));
            break;
        case LBC_TYPE_BUFFER:
            build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TBUFFER), build.vmExit(kVmExitEntryGuardPc));
            break;
        default:
            if (tag >= LBC_TYPE_TAGGED_USERDATA_BASE && tag < LBC_TYPE_TAGGED_USERDATA_END)
            {
                build.inst(IrCmd::CHECK_TAG, load, build.constTag(LUA_TUSERDATA), build.vmExit(kVmExitEntryGuardPc));
            }
            else
            {
                CODEGEN_ASSERT(!"unknown argument type tag");
            }
            break;
        }

        if (optional)
        {
            build.inst(IrCmd::JUMP, nextCheck);

            build.beginBlock(nextCheck);

            if (FFlag::LuauCodegenSetBlockEntryState3)
                build.function.blockOp(nextCheck).flags |= kBlockFlagEntryArgCheck;
        }
    }

    // If the last argument is optional, we can skip creating a new internal block since one will already have been created.
    if (!(typeInfo.argumentTypes.back() & LBC_TYPE_OPTIONAL_BIT))
    {
        IrOp next = build.block(IrBlockKind::Internal);
        build.inst(IrCmd::JUMP, next);

        build.beginBlock(next);
    }
}

void IrBuilder::buildFunctionIr(Proto* proto)
{
    function.proto = proto;
    function.variadic = proto->is_vararg != 0;

    loadBytecodeTypeInfo(function);

    // Reserve entry block
    bool generateTypeChecks = hasTypedParameters(function.bcTypeInfo);
    IrOp entry = generateTypeChecks ? block(IrBlockKind::Internal) : IrOp{};

    // Rebuild original control flow blocks
    rebuildBytecodeBasicBlocks(proto);

    // Infer register tags in bytecode
    analyzeBytecodeTypes(function, hostHooks);

    function.bcMapping.resize(proto->sizecode, {~0u, ~0u});

    if (generateTypeChecks)
    {
        beginBlock(entry);

        buildArgumentTypeChecks(*this, entry);

        inst(IrCmd::JUMP, blockAtInst(0));
    }
    else
    {
        entry = blockAtInst(0);
    }

    function.entryBlock = entry.index;

    // Translate all instructions to IR inside blocks
    for (int i = 0; i < proto->sizecode;)
    {
        const Instruction* pc = &proto->code[i];
        LuauOpcode op = LuauOpcode(LUAU_INSN_OP(*pc));

        int nexti = i + getOpLength(op);
        CODEGEN_ASSERT(nexti <= proto->sizecode);

        function.bcMapping[i] = {uint32_t(function.instructions.size()), ~0u};

        // Begin new block at this instruction if it was in the bytecode or requested during translation
        if (instIndexToBlock[i] != kNoAssociatedBlockIndex)
        {
            IrOp block = blockAtInst(i);

            beginBlock(block);

            function.blockOp(block).startpc = uint32_t(i);
        }

        // Numeric for loops require additional processing to maintain loop stack
        // Notably, this must be performed even when the block is dead so that we maintain the pairing FORNPREP-FORNLOOP
        if (int(op) == LOP_FORNPREP)
            beforeInstForNPrep(*this, pc, i);

        // We skip dead bytecode instructions when they appear after block was already terminated
        if (!inTerminatedBlock)
        {
            if (interruptRequested)
            {
                interruptRequested = false;
                inst(IrCmd::INTERRUPT, constUint(i));
            }

            translateInst(op, pc, i);

            if (cmdSkipTarget != -1)
            {
                nexti = cmdSkipTarget;
                cmdSkipTarget = -1;
            }
        }

        // See above for FORNPREP..FORNLOOP processing
        if (int(op) == LOP_FORNLOOP)
            afterInstForNLoop(*this, pc);

        i = nexti;
        CODEGEN_ASSERT(i <= proto->sizecode);

        // If we are going into a new block at the next instruction and it's a fallthrough, jump has to be placed to mark block termination
        if (i < int(instIndexToBlock.size()) && instIndexToBlock[i] != kNoAssociatedBlockIndex)
        {
            if (!isBlockTerminator(function.instructions.back().cmd))
                inst(IrCmd::JUMP, blockAtInst(i));
        }
    }

    // Now that all has been generated, compute use counts
    updateUseCounts(function);
}

void IrBuilder::rebuildBytecodeBasicBlocks(Proto* proto)
{
    instIndexToBlock.resize(proto->sizecode, kNoAssociatedBlockIndex);

    // Mark jump targets
    std::vector<uint8_t> jumpTargets(proto->sizecode, 0);

    for (int i = 0; i < proto->sizecode;)
    {
        const Instruction* pc = &proto->code[i];
        LuauOpcode op = LuauOpcode(LUAU_INSN_OP(*pc));

        int target = getJumpTarget(*pc, uint32_t(i));

        if (target >= 0 && !isFastCall(op))
            jumpTargets[target] = true;

        i += getOpLength(op);
        CODEGEN_ASSERT(i <= proto->sizecode);
    }

    // Bytecode blocks are created at bytecode jump targets and the start of a function
    jumpTargets[0] = true;

    for (int i = 0; i < proto->sizecode; i++)
    {
        if (jumpTargets[i])
        {
            IrOp b = block(IrBlockKind::Bytecode);
            instIndexToBlock[i] = b.index;
        }
    }

    buildBytecodeBlocks(function, jumpTargets);
}

static bool isDirectCompare(Proto* proto, const Instruction* pc, int i)
{
    // Matching the compiler sequence for generating 0 or 1 based on a comparison between values:
    // LOP_JUMP** Lx
    // [aux]
    // LOADB Rx, 0 +1
    // Lx: LOADB Rx, 1
    if (i + 3 < proto->sizecode && LUAU_INSN_D(*pc) == 2)
    {
        const Instruction loadTrue = pc[2];
        const Instruction loadFalse = pc[3];

        if (LUAU_INSN_OP(loadTrue) == LOP_LOADB && LUAU_INSN_OP(loadFalse) == LOP_LOADB)
        {
            bool sameTarget = LUAU_INSN_A(loadTrue) == LUAU_INSN_A(loadFalse);
            bool zeroAndOne = LUAU_INSN_B(loadTrue) == 0 && LUAU_INSN_B(loadFalse) == 1;
            bool correctJumps = LUAU_INSN_C(loadTrue) == 1 && LUAU_INSN_C(loadFalse) == 0;

            return sameTarget && zeroAndOne && correctJumps;
        }
    }

    return false;
}

void IrBuilder::translateInst(LuauOpcode op, const Instruction* pc, int i)
{
    switch (int(op))
    {
    case LOP_NOP:
        break;
    case LOP_LOADNIL:
        translateInstLoadNil(*this, pc);
        break;
    case LOP_LOADB:
        translateInstLoadB(*this, pc, i);
        break;
    case LOP_LOADN:
        translateInstLoadN(*this, pc);
        break;
    case LOP_LOADK:
        translateInstLoadK(*this, pc);
        break;
    case LOP_LOADKX:
        translateInstLoadKX(*this, pc);
        break;
    case LOP_MOVE:
        translateInstMove(*this, pc);
        break;
    case LOP_GETGLOBAL:
        translateInstGetGlobal(*this, pc, i);
        break;
    case LOP_SETGLOBAL:
        translateInstSetGlobal(*this, pc, i);
        break;
    case LOP_CALL:
        inst(IrCmd::INTERRUPT, constUint(i));
        inst(IrCmd::SET_SAVEDPC, constUint(i + 1));

        inst(IrCmd::CALL, vmReg(LUAU_INSN_A(*pc)), constInt(LUAU_INSN_B(*pc) - 1), constInt(LUAU_INSN_C(*pc) - 1));

        if (activeFastcallFallback)
        {
            inst(IrCmd::JUMP, fastcallFallbackReturn);

            beginBlock(fastcallFallbackReturn);

            activeFastcallFallback = false;
        }
        break;
    case LOP_RETURN:
        inst(IrCmd::INTERRUPT, constUint(i));

        inst(IrCmd::RETURN, vmReg(LUAU_INSN_A(*pc)), constInt(LUAU_INSN_B(*pc) - 1));
        break;
    case LOP_GETTABLE:
        translateInstGetTable(*this, pc, i);
        break;
    case LOP_SETTABLE:
        translateInstSetTable(*this, pc, i);
        break;
    case LOP_GETTABLEKS:
    case LOP_GETUDATAKS:
        translateInstGetTableKS(*this, pc, i);
        break;
    case LOP_SETTABLEKS:
    case LOP_SETUDATAKS:
        translateInstSetTableKS(*this, pc, i);
        break;
    case LOP_GETTABLEN:
        translateInstGetTableN(*this, pc, i);
        break;
    case LOP_SETTABLEN:
        translateInstSetTableN(*this, pc, i);
        break;
    case LOP_JUMP:
        translateInstJump(*this, pc, i);
        break;
    case LOP_JUMPBACK:
        translateInstJumpBack(*this, pc, i);
        break;
    case LOP_JUMPIF:
        translateInstJumpIf(*this, pc, i, /* not_ */ false);
        break;
    case LOP_JUMPIFNOT:
        translateInstJumpIf(*this, pc, i, /* not_ */ true);
        break;
    case LOP_JUMPIFEQ:
        if (isDirectCompare(function.proto, pc, i))
        {
            translateInstJumpIfEqShortcut(*this, pc, i, /* not_ */ false);

            // We complete the current instruction and the first LOADB, but we do not skip the second LOADB
            // This is because the second LOADB was a jump target so there is a block prepared to handle it
            cmdSkipTarget = i + 3;
            break;
        }

        translateInstJumpIfEq(*this, pc, i, /* not_ */ false);
        break;
    case LOP_JUMPIFLE:
        translateInstJumpIfCond(*this, pc, i, IrCondition::LessEqual);
        break;
    case LOP_JUMPIFLT:
        translateInstJumpIfCond(*this, pc, i, IrCondition::Less);
        break;
    case LOP_JUMPIFNOTEQ:
        if (isDirectCompare(function.proto, pc, i))
        {
            translateInstJumpIfEqShortcut(*this, pc, i, /* not_ */ true);

            // We complete the current instruction and the first LOADB, but we do not skip the second LOADB
            // This is because the second LOADB was a jump target so there is a block prepared to handle it
            cmdSkipTarget = i + 3;
            break;
        }

        translateInstJumpIfEq(*this, pc, i, /* not_ */ true);
        break;
    case LOP_JUMPIFNOTLE:
        translateInstJumpIfCond(*this, pc, i, IrCondition::NotLessEqual);
        break;
    case LOP_JUMPIFNOTLT:
        translateInstJumpIfCond(*this, pc, i, IrCondition::NotLess);
        break;
    case LOP_JUMPX:
        translateInstJumpX(*this, pc, i);
        break;
    case LOP_JUMPXEQKNIL:
        if (isDirectCompare(function.proto, pc, i))
        {
            translateInstJumpxEqNilShortcut(*this, pc, i);

            // We complete the current instruction and the first LOADB, but we do not skip the second LOADB
            // This is because the second LOADB was a jump target so there is a block prepared to handle it
            cmdSkipTarget = i + 3;
            break;
        }

        translateInstJumpxEqNil(*this, pc, i);
        break;
    case LOP_JUMPXEQKB:
        if (isDirectCompare(function.proto, pc, i))
        {
            translateInstJumpxEqBShortcut(*this, pc, i);

            // We complete the current instruction and the first LOADB, but we do not skip the second LOADB
            // This is because the second LOADB was a jump target so there is a block prepared to handle it
            cmdSkipTarget = i + 3;
            break;
        }

        translateInstJumpxEqB(*this, pc, i);
        break;
    case LOP_JUMPXEQKN:
        if (isDirectCompare(function.proto, pc, i))
        {
            translateInstJumpxEqNShortcut(*this, pc, i);

            // We complete the current instruction and the first LOADB, but we do not skip the second LOADB
            // This is because the second LOADB was a jump target so there is a block prepared to handle it
            cmdSkipTarget = i + 3;
            break;
        }

        translateInstJumpxEqN(*this, pc, i);
        break;
    case LOP_JUMPXEQKS:
        if (isDirectCompare(function.proto, pc, i))
        {
            translateInstJumpxEqSShortcut(*this, pc, i);

            // We complete the current instruction and the first LOADB, but we do not skip the second LOADB
            // This is because the second LOADB was a jump target so there is a block prepared to handle it
            cmdSkipTarget = i + 3;
            break;
        }

        translateInstJumpxEqS(*this, pc, i);
        break;
    case LOP_ADD:
        translateInstBinary(*this, pc, i, TM_ADD);
        break;
    case LOP_SUB:
        translateInstBinary(*this, pc, i, TM_SUB);
        break;
    case LOP_MUL:
        translateInstBinary(*this, pc, i, TM_MUL);
        break;
    case LOP_DIV:
        translateInstBinary(*this, pc, i, TM_DIV);
        break;
    case LOP_IDIV:
        translateInstBinary(*this, pc, i, TM_IDIV);
        break;
    case LOP_MOD:
        translateInstBinary(*this, pc, i, TM_MOD);
        break;
    case LOP_POW:
        translateInstBinary(*this, pc, i, TM_POW);
        break;
    case LOP_ADDK:
        translateInstBinaryK(*this, pc, i, TM_ADD);
        break;
    case LOP_SUBK:
        translateInstBinaryK(*this, pc, i, TM_SUB);
        break;
    case LOP_MULK:
        translateInstBinaryK(*this, pc, i, TM_MUL);
        break;
    case LOP_DIVK:
        translateInstBinaryK(*this, pc, i, TM_DIV);
        break;
    case LOP_IDIVK:
        translateInstBinaryK(*this, pc, i, TM_IDIV);
        break;
    case LOP_MODK:
        translateInstBinaryK(*this, pc, i, TM_MOD);
        break;
    case LOP_POWK:
        translateInstBinaryK(*this, pc, i, TM_POW);
        break;
    case LOP_SUBRK:
        translateInstBinaryRK(*this, pc, i, TM_SUB);
        break;
    case LOP_DIVRK:
        translateInstBinaryRK(*this, pc, i, TM_DIV);
        break;
    case LOP_NOT:
        translateInstNot(*this, pc);
        break;
    case LOP_MINUS:
        translateInstMinus(*this, pc, i);
        break;
    case LOP_LENGTH:
        translateInstLength(*this, pc, i);
        break;
    case LOP_NEWTABLE:
        translateInstNewTable(*this, pc, i);
        break;
    case LOP_DUPTABLE:
        translateInstDupTable(*this, pc, i);
        break;
    case LOP_SETLIST:
        inst(
            IrCmd::SETLIST, constUint(i), vmReg(LUAU_INSN_A(*pc)), vmReg(LUAU_INSN_B(*pc)), constInt(LUAU_INSN_C(*pc) - 1), constUint(pc[1]), undef()
        );
        break;
    case LOP_GETUPVAL:
        translateInstGetUpval(*this, pc, i);
        break;
    case LOP_SETUPVAL:
        translateInstSetUpval(*this, pc, i);
        break;
    case LOP_CLOSEUPVALS:
        translateInstCloseUpvals(*this, pc);
        break;
    case LOP_FASTCALL:
        handleFastcallFallback(translateFastCallN(*this, pc, i, false, 0, {}, {}), pc, i);
        break;
    case LOP_FASTCALL1:
        handleFastcallFallback(translateFastCallN(*this, pc, i, true, 1, undef(), undef()), pc, i);
        break;
    case LOP_FASTCALL2:
        handleFastcallFallback(translateFastCallN(*this, pc, i, true, 2, vmReg(pc[1]), undef()), pc, i);
        break;
    case LOP_FASTCALL2K:
        handleFastcallFallback(translateFastCallN(*this, pc, i, true, 2, vmConst(pc[1]), undef()), pc, i);
        break;
    case LOP_FASTCALL3:
        handleFastcallFallback(translateFastCallN(*this, pc, i, true, 3, vmReg(pc[1] & 0xff), vmReg((pc[1] >> 8) & 0xff)), pc, i);
        break;
    case LOP_FORNPREP:
        translateInstForNPrep(*this, pc, i);
        break;
    case LOP_FORNLOOP:
        translateInstForNLoop(*this, pc, i);
        break;
    case LOP_FORGLOOP:
    {
        int aux = int(pc[1]);

        // We have a translation for ipairs-style traversal, general loop iteration is still too complex
        if (aux < 0)
        {
            translateInstForGLoopIpairs(*this, pc, i);
        }
        else
        {
            int ra = LUAU_INSN_A(*pc);

            IrOp loopRepeat = blockAtInst(i + 1 + LUAU_INSN_D(*pc));
            IrOp loopExit = blockAtInst(i + getOpLength(LuauOpcode(LOP_FORGLOOP)));
            IrOp fallback = fallbackBlock(i);

            inst(IrCmd::INTERRUPT, constUint(i));
            loadAndCheckTag(vmReg(ra), LUA_TNIL, fallback);

            inst(IrCmd::FORGLOOP, vmReg(ra), constInt(aux), loopRepeat, loopExit);

            beginBlock(fallback);
            inst(IrCmd::SET_SAVEDPC, constUint(i + 1));
            inst(IrCmd::FORGLOOP_FALLBACK, vmReg(ra), constInt(aux), loopRepeat, loopExit);

            beginBlock(loopExit);
        }
        break;
    }
    case LOP_FORGPREP_NEXT:
        translateInstForGPrepNext(*this, pc, i);
        break;
    case LOP_FORGPREP_INEXT:
        translateInstForGPrepInext(*this, pc, i);
        break;
    case LOP_AND:
        translateInstAndX(*this, pc, i, vmReg(LUAU_INSN_C(*pc)));
        break;
    case LOP_ANDK:
        translateInstAndX(*this, pc, i, vmConst(LUAU_INSN_C(*pc)));
        break;
    case LOP_OR:
        translateInstOrX(*this, pc, i, vmReg(LUAU_INSN_C(*pc)));
        break;
    case LOP_ORK:
        translateInstOrX(*this, pc, i, vmConst(LUAU_INSN_C(*pc)));
        break;
    case LOP_COVERAGE:
        inst(IrCmd::COVERAGE, constUint(i));
        break;
    case LOP_GETIMPORT:
        translateInstGetImport(*this, pc, i);
        break;
    case LOP_CONCAT:
        translateInstConcat(*this, pc, i);
        break;
    case LOP_CAPTURE:
        translateInstCapture(*this, pc, i);
        break;
    case LOP_NAMECALL:
    case LOP_NAMECALLUDATA:
        if (translateInstNamecall(*this, pc, i))
            cmdSkipTarget = i + 3;
        break;
    case LOP_PREPVARARGS:
        inst(IrCmd::FALLBACK_PREPVARARGS, constUint(i), constInt(LUAU_INSN_A(*pc)));
        break;
    case LOP_GETVARARGS:
        inst(IrCmd::FALLBACK_GETVARARGS, constUint(i), vmReg(LUAU_INSN_A(*pc)), constInt(LUAU_INSN_B(*pc) - 1));
        break;
    case LOP_NEWCLOSURE:
        translateInstNewClosure(*this, pc, i);
        break;
    case LOP_DUPCLOSURE:
        inst(IrCmd::FALLBACK_DUPCLOSURE, constUint(i), vmReg(LUAU_INSN_A(*pc)), vmConst(LUAU_INSN_D(*pc)));
        break;
    case LOP_FORGPREP:
    {
        IrOp loopStart = blockAtInst(i + 1 + LUAU_INSN_D(*pc));

        inst(IrCmd::FALLBACK_FORGPREP, constUint(i), vmReg(LUAU_INSN_A(*pc)), loopStart);
        break;
    }
    default:
        CODEGEN_ASSERT(!"Unknown instruction");
    }
}

void IrBuilder::handleFastcallFallback(IrOp fallbackOrUndef, const Instruction* pc, int i)
{
    int skip = LUAU_INSN_C(*pc);

    if (fallbackOrUndef.kind != IrOpKind::Undef)
    {
        IrOp next = blockAtInst(i + skip + 2);
        inst(IrCmd::JUMP, next);
        beginBlock(fallbackOrUndef);

        activeFastcallFallback = true;
        fastcallFallbackReturn = next;
    }
    else
    {
        cmdSkipTarget = i + skip + 2;
    }
}

bool IrBuilder::isInternalBlock(IrOp block)
{
    IrBlock& target = function.blocks[block.index];

    return target.kind == IrBlockKind::Internal;
}

void IrBuilder::beginBlock(IrOp block)
{
    IrBlock& target = function.blocks[block.index];
    activeBlockIdx = block.index;

    CODEGEN_ASSERT(target.start == ~0u || target.start == uint32_t(function.instructions.size()));

    target.start = uint32_t(function.instructions.size());
    target.sortkey = target.start;

    inTerminatedBlock = false;
}

void IrBuilder::loadAndCheckTag(IrOp loc, uint8_t tag, IrOp fallback)
{
    inst(IrCmd::CHECK_TAG, inst(IrCmd::LOAD_TAG, loc), constTag(tag), fallback);
}

void IrBuilder::checkSafeEnv(int pcpos)
{
    IrBlock& active = function.blocks[activeBlockIdx];

    // If the block start is associated with a bytecode position, we can perform an early safeenv check
    if (active.startpc != kBlockNoStartPc)
    {
        // If the block hasn't cleared the safeenv flag yet, we can still set it at block entry
        if ((active.flags & kBlockFlagSafeEnvClear) == 0)
            active.flags |= kBlockFlagSafeEnvCheck;
    }

    inst(IrCmd::CHECK_SAFE_ENV, vmExit(pcpos));
}

void IrBuilder::clone(std::vector<uint32_t> sourceIdxs, bool removeCurrentTerminator)
{
    DenseHashMap<uint32_t, uint32_t> instRedir{~0u};

    auto redirect = [&instRedir](IrOp& op)
    {
        if (op.kind == IrOpKind::Inst)
        {
            if (const uint32_t* newIndex = instRedir.find(op.index))
                op.index = *newIndex;
            else
                CODEGEN_ASSERT(!"Values can only be used if they are defined in the same block");
        }
    };

    for (uint32_t sourceIdx : sourceIdxs)
    {
        const IrBlock& source = function.blocks[sourceIdx];

        if (removeCurrentTerminator && inTerminatedBlock)
        {
            IrBlock& active = function.blocks[activeBlockIdx];
            IrInst& term = function.instructions[active.finish];

            kill(function, term);
            inTerminatedBlock = false;
        }

        // Implicit safe environment checks become materialized as real ones
        if ((source.flags & kBlockFlagSafeEnvCheck) != 0)
        {
            CODEGEN_ASSERT(source.startpc != kBlockNoStartPc);
            inst(IrCmd::CHECK_SAFE_ENV, vmExit(source.startpc));
        }

        for (uint32_t index = source.start; index <= source.finish; index++)
        {
            CODEGEN_ASSERT(index < function.instructions.size());
            IrInst clone = function.instructions[index];

            // Skip pseudo instructions to make clone more compact, but validate that they have no users
            if (isPseudo(clone.cmd))
            {
                CODEGEN_ASSERT(clone.useCount == 0);
                continue;
            }

            for (auto& op : clone.ops)
                redirect(op);

            for (auto& op : clone.ops)
                addUse(function, op);

            // Instructions that referenced the original will have to be adjusted to use the clone
            instRedir[index] = uint32_t(function.instructions.size());

            // Reconstruct the fresh clone
            inst(clone.cmd, clone.ops);
        }
    }
}

IrOp IrBuilder::undef()
{
    return {IrOpKind::Undef, 0};
}

IrOp IrBuilder::constInt(int value)
{
    IrConst constant;
    constant.kind = IrConstKind::Int;
    constant.valueInt = value;
    return constAny(constant, uint64_t(value));
}

IrOp IrBuilder::constInt64(int64_t value)
{
    IrConst constant;
    constant.kind = IrConstKind::Int64;
    constant.valueInt64 = value;
    return constAny(constant, uint64_t(value));
}

IrOp IrBuilder::constUint(unsigned value)
{
    IrConst constant;
    constant.kind = IrConstKind::Uint;
    constant.valueUint = value;
    return constAny(constant, uint64_t(value));
}

IrOp IrBuilder::constImport(unsigned value)
{
    IrConst constant;
    constant.kind = IrConstKind::Import;
    constant.valueUint = value;
    return constAny(constant, uint64_t(value));
}

IrOp IrBuilder::constDouble(double value)
{
    IrConst constant;
    constant.kind = IrConstKind::Double;
    constant.valueDouble = value;

    uint64_t asCommonKey;
    static_assert(sizeof(asCommonKey) == sizeof(value), "Expecting double to be 64-bit");
    memcpy(&asCommonKey, &value, sizeof(value));

    return constAny(constant, asCommonKey);
}

IrOp IrBuilder::constTag(uint8_t value)
{
    IrConst constant;
    constant.kind = IrConstKind::Tag;
    constant.valueTag = value;
    return constAny(constant, uint64_t(value));
}

IrOp IrBuilder::constAny(IrConst constant, uint64_t asCommonKey)
{
    ConstantKey key{constant.kind, asCommonKey};

    if (uint32_t* cache = constantMap.find(key))
        return {IrOpKind::Constant, *cache};

    uint32_t index = uint32_t(function.constants.size());
    function.constants.push_back(constant);

    constantMap[key] = index;

    return {IrOpKind::Constant, index};
}

IrOp IrBuilder::cond(IrCondition cond)
{
    return {IrOpKind::Condition, uint32_t(cond)};
}

IrOp IrBuilder::inst(IrCmd cmd)
{
    return inst(cmd, {});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a)
{
    return inst(cmd, {a});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a, IrOp b)
{
    return inst(cmd, {a, b});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a, IrOp b, IrOp c)
{
    return inst(cmd, {a, b, c});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d)
{
    return inst(cmd, {a, b, c, d});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d, IrOp e)
{
    return inst(cmd, {a, b, c, d, e});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d, IrOp e, IrOp f)
{
    return inst(cmd, {a, b, c, d, e, f});
}

IrOp IrBuilder::inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d, IrOp e, IrOp f, IrOp g)
{
    return inst(cmd, {a, b, c, d, e, f, g});
}

IrOp IrBuilder::inst(IrCmd cmd, std::initializer_list<IrOp> ops)
{
    uint32_t index = uint32_t(function.instructions.size());
    function.instructions.push_back({cmd, ops});

    CODEGEN_ASSERT(!inTerminatedBlock);

    if (isBlockTerminator(cmd))
    {
        function.blocks[activeBlockIdx].finish = index;
        inTerminatedBlock = true;
    }

    if (canInvalidateSafeEnv(cmd))
    {
        // Mark that block has instruction with this flag
        function.blocks[activeBlockIdx].flags |= kBlockFlagSafeEnvClear;
    }

    return {IrOpKind::Inst, index};
}

IrOp IrBuilder::inst(IrCmd cmd, const IrOps& ops)
{
    uint32_t index = uint32_t(function.instructions.size());
    function.instructions.push_back({cmd, ops});

    CODEGEN_ASSERT(!inTerminatedBlock);

    if (isBlockTerminator(cmd))
    {
        function.blocks[activeBlockIdx].finish = index;
        inTerminatedBlock = true;
    }

    if (canInvalidateSafeEnv(cmd))
    {
        // Mark that block has instruction with this flag
        function.blocks[activeBlockIdx].flags |= kBlockFlagSafeEnvClear;
    }

    return {IrOpKind::Inst, index};
}

IrOp IrBuilder::block(IrBlockKind kind)
{
    CODEGEN_ASSERT(kind != IrBlockKind::Fallback && "fallbackBlock must be used for fallback block creation");

    if (kind == IrBlockKind::Internal && activeFastcallFallback)
        kind = IrBlockKind::Fallback;

    uint32_t index = uint32_t(function.blocks.size());
    function.blocks.push_back(IrBlock{kind});
    return IrOp{IrOpKind::Block, index};
}

IrOp IrBuilder::blockAtInst(uint32_t index)
{
    uint32_t blockIndex = instIndexToBlock[index];

    if (blockIndex != kNoAssociatedBlockIndex)
        return IrOp{IrOpKind::Block, blockIndex};

    IrOp result = block(IrBlockKind::Internal);
    function.blockOp(result).startpc = index;

    return result;
}

IrOp IrBuilder::fallbackBlock(uint32_t pcpos)
{
    uint32_t index = uint32_t(function.blocks.size());
    function.blocks.push_back(IrBlock{IrBlockKind::Fallback});
    CODEGEN_ASSERT(index != 0 && "IR cannot start with a fallback block");

    function.blocks.back().startpc = pcpos;
    return IrOp{IrOpKind::Block, index};
}

IrOp IrBuilder::vmReg(uint8_t index)
{
    return {IrOpKind::VmReg, index};
}

IrOp IrBuilder::vmConst(uint32_t index)
{
    return {IrOpKind::VmConst, index};
}

IrOp IrBuilder::vmUpvalue(uint8_t index)
{
    return {IrOpKind::VmUpvalue, index};
}

IrOp IrBuilder::vmExit(uint32_t pcpos)
{
    return {IrOpKind::VmExit, pcpos};
}

} // namespace CodeGen
} // namespace Luau
