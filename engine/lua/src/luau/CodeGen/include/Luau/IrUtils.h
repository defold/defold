// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Bytecode.h"
#include "Luau/Common.h"
#include "Luau/IrData.h"

#include <optional>

LUAU_FASTFLAG(LuauCodegenMarkDeadRegisters2)
LUAU_FASTFLAG(LuauCodegenDseOnCondJump)
LUAU_FASTFLAG(LuauCodegenConsistentHasResult)

namespace Luau
{
namespace CodeGen
{

struct IrBuilder;
enum class HostMetamethod;

int getOpLength(LuauOpcode op);
bool isJumpD(LuauOpcode op);
bool isSkipC(LuauOpcode op);
bool isFastCall(LuauOpcode op);
int getJumpTarget(uint32_t insn, uint32_t pc);
IrValueKind getCmdValueKind(IrCmd cmd);
IrValueKind getConstValueKind(const IrConst& constant);

inline bool isBlockTerminator(IrCmd cmd)
{
    switch (cmd)
    {
    case IrCmd::JUMP:
    case IrCmd::JUMP_IF_TRUTHY:
    case IrCmd::JUMP_IF_FALSY:
    case IrCmd::JUMP_EQ_TAG:
    case IrCmd::JUMP_CMP_INT:
    case IrCmd::JUMP_EQ_POINTER:
    case IrCmd::JUMP_CMP_NUM:
    case IrCmd::JUMP_CMP_FLOAT:
    case IrCmd::JUMP_FORN_LOOP_COND:
    case IrCmd::JUMP_SLOT_MATCH:
    case IrCmd::RETURN:
    case IrCmd::FORGLOOP:
    case IrCmd::FORGLOOP_FALLBACK:
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
    case IrCmd::FALLBACK_FORGPREP:
        return true;
    default:
        break;
    }

    return false;
}

inline bool isNonTerminatingJump(IrCmd cmd)
{
    switch (cmd)
    {
    case IrCmd::TRY_NUM_TO_INDEX:
    case IrCmd::TRY_CALL_FASTGETTM:
    case IrCmd::CHECK_FASTCALL_RES:
    case IrCmd::CHECK_TAG:
    case IrCmd::CHECK_TRUTHY:
    case IrCmd::CHECK_READONLY:
    case IrCmd::CHECK_NO_METATABLE:
    case IrCmd::CHECK_SAFE_ENV:
    case IrCmd::CHECK_ARRAY_SIZE:
    case IrCmd::CHECK_SLOT_MATCH:
    case IrCmd::CHECK_NODE_NO_NEXT:
    case IrCmd::CHECK_NODE_VALUE:
    case IrCmd::CHECK_BUFFER_LEN:
    case IrCmd::CHECK_USERDATA_TAG:
    case IrCmd::CHECK_CMP_NUM:
    case IrCmd::CHECK_CMP_INT:
    case IrCmd::CHECK_CMP_INT64:
    case IrCmd::CHECK_DIV_INT64:
        return true;
    default:
        break;
    }

    return false;
}

inline bool hasResult(IrCmd cmd)
{
    if (FFlag::LuauCodegenConsistentHasResult)
        return getCmdValueKind(cmd) != IrValueKind::None;

    // Remove with FFlagLuauCodegenConsistentHasResult
    switch (cmd)
    {
    case IrCmd::LOAD_TAG:
    case IrCmd::LOAD_POINTER:
    case IrCmd::LOAD_DOUBLE:
    case IrCmd::LOAD_INT:
    case IrCmd::LOAD_INT64:
    case IrCmd::LOAD_FLOAT:
    case IrCmd::LOAD_TVALUE:
    case IrCmd::LOAD_ENV:
    case IrCmd::GET_ARR_ADDR:
    case IrCmd::GET_SLOT_NODE_ADDR:
    case IrCmd::GET_HASH_NODE_ADDR:
    case IrCmd::GET_CLOSURE_UPVAL_ADDR:
    case IrCmd::ADD_INT64:
    case IrCmd::SUB_INT64:
    case IrCmd::MUL_INT64:
    case IrCmd::DIV_INT64:
    case IrCmd::IDIV_INT64:
    case IrCmd::UDIV_INT64:
    case IrCmd::REM_INT64:
    case IrCmd::UREM_INT64:
    case IrCmd::MOD_INT64:
    case IrCmd::SELECT_INT64:
    case IrCmd::ADD_INT:
    case IrCmd::SUB_INT:
    case IrCmd::SEXTI8_INT:
    case IrCmd::SEXTI16_INT:
    case IrCmd::ADD_NUM:
    case IrCmd::SUB_NUM:
    case IrCmd::MUL_NUM:
    case IrCmd::DIV_NUM:
    case IrCmd::IDIV_NUM:
    case IrCmd::MOD_NUM:
    case IrCmd::MIN_NUM:
    case IrCmd::MAX_NUM:
    case IrCmd::UNM_NUM:
    case IrCmd::FLOOR_NUM:
    case IrCmd::CEIL_NUM:
    case IrCmd::ROUND_NUM:
    case IrCmd::SQRT_NUM:
    case IrCmd::ABS_NUM:
    case IrCmd::SIGN_NUM:
    case IrCmd::ADD_FLOAT:
    case IrCmd::SUB_FLOAT:
    case IrCmd::MUL_FLOAT:
    case IrCmd::DIV_FLOAT:
    case IrCmd::MIN_FLOAT:
    case IrCmd::MAX_FLOAT:
    case IrCmd::UNM_FLOAT:
    case IrCmd::FLOOR_FLOAT:
    case IrCmd::CEIL_FLOAT:
    case IrCmd::SQRT_FLOAT:
    case IrCmd::ABS_FLOAT:
    case IrCmd::SIGN_FLOAT:
    case IrCmd::SELECT_NUM:
    case IrCmd::SELECT_IF_TRUTHY:
    case IrCmd::ADD_VEC:
    case IrCmd::SUB_VEC:
    case IrCmd::MUL_VEC:
    case IrCmd::DIV_VEC:
    case IrCmd::IDIV_VEC:
    case IrCmd::UNM_VEC:
    case IrCmd::MIN_VEC:
    case IrCmd::MAX_VEC:
    case IrCmd::FLOOR_VEC:
    case IrCmd::CEIL_VEC:
    case IrCmd::ABS_VEC:
    case IrCmd::DOT_VEC:
    case IrCmd::EXTRACT_VEC:
    case IrCmd::NOT_ANY:
    case IrCmd::CMP_ANY:
    case IrCmd::CMP_INT:
    case IrCmd::CMP_INT64:
    case IrCmd::CMP_TAG:
    case IrCmd::CMP_SPLIT_TVALUE:
    case IrCmd::TABLE_LEN:
    case IrCmd::TABLE_SETNUM:
    case IrCmd::STRING_LEN:
    case IrCmd::NEW_TABLE:
    case IrCmd::DUP_TABLE:
    case IrCmd::TRY_NUM_TO_INDEX:
    case IrCmd::TRY_CALL_FASTGETTM:
    case IrCmd::NEW_USERDATA:
    case IrCmd::INT_TO_NUM:
    case IrCmd::INT64_TO_NUM:
    case IrCmd::UINT_TO_NUM:
    case IrCmd::UINT_TO_FLOAT:
    case IrCmd::NUM_TO_INT:
    case IrCmd::NUM_TO_INT64:
    case IrCmd::NUM_TO_UINT:
    case IrCmd::FLOAT_TO_NUM:
    case IrCmd::NUM_TO_FLOAT:
    case IrCmd::FLOAT_TO_VEC:
    case IrCmd::TAG_VECTOR:
    case IrCmd::TRUNCATE_UINT:
    case IrCmd::SUBSTITUTE:
    case IrCmd::INVOKE_FASTCALL:
    case IrCmd::BITAND_UINT:
    case IrCmd::BITXOR_UINT:
    case IrCmd::BITOR_UINT:
    case IrCmd::BITNOT_UINT:
    case IrCmd::BITLSHIFT_UINT:
    case IrCmd::BITRSHIFT_UINT:
    case IrCmd::BITARSHIFT_UINT:
    case IrCmd::BITLROTATE_UINT:
    case IrCmd::BITRROTATE_UINT:
    case IrCmd::BITCOUNTLZ_UINT:
    case IrCmd::BITCOUNTRZ_UINT:
    case IrCmd::BITAND_INT64:
    case IrCmd::BITXOR_INT64:
    case IrCmd::BITOR_INT64:
    case IrCmd::BITNOT_INT64:
    case IrCmd::BITLSHIFT_INT64:
    case IrCmd::BITRSHIFT_INT64:
    case IrCmd::BITARSHIFT_INT64:
    case IrCmd::BITLROTATE_INT64:
    case IrCmd::BITRROTATE_INT64:
    case IrCmd::BITCOUNTLZ_INT64:
    case IrCmd::BITCOUNTRZ_INT64:
    case IrCmd::BYTESWAP_INT64:
    case IrCmd::INVOKE_LIBM:
    case IrCmd::GET_TYPE:
    case IrCmd::GET_TYPEOF:
    case IrCmd::NEWCLOSURE:
    case IrCmd::FINDUPVAL:
    case IrCmd::BUFFER_READI8:
    case IrCmd::BUFFER_READU8:
    case IrCmd::BUFFER_READI16:
    case IrCmd::BUFFER_READU16:
    case IrCmd::BUFFER_READI32:
    case IrCmd::BUFFER_READF32:
    case IrCmd::BUFFER_READF64:
    case IrCmd::GET_UPVALUE:
        return true;
    default:
        break;
    }

    return false;
}

inline bool canInvalidateSafeEnv(IrCmd cmd)
{
    switch (cmd)
    {
    case IrCmd::CMP_ANY:
    case IrCmd::DO_ARITH:
    case IrCmd::DO_LEN:
    case IrCmd::GET_TABLE:
    case IrCmd::SET_TABLE:
    case IrCmd::CONCAT: // TODO: if only strings and numbers are concatenated, there will be no user calls
    case IrCmd::CALL:
    case IrCmd::FORGLOOP_FALLBACK:
    case IrCmd::FALLBACK_GETGLOBAL:
    case IrCmd::FALLBACK_SETGLOBAL:
    case IrCmd::FALLBACK_GETTABLEKS:
    case IrCmd::FALLBACK_SETTABLEKS:
    case IrCmd::FALLBACK_NAMECALL:
    case IrCmd::FALLBACK_FORGPREP:
        return true;
    default:
        break;
    }

    return false;
}

inline bool isPseudo(IrCmd cmd)
{
    // Instructions that are used for internal needs and are not a part of final lowering
    if (FFlag::LuauCodegenMarkDeadRegisters2 || FFlag::LuauCodegenDseOnCondJump)
        return cmd == IrCmd::NOP || cmd == IrCmd::SUBSTITUTE || cmd == IrCmd::MARK_USED || cmd == IrCmd::MARK_DEAD;
    else
        return cmd == IrCmd::NOP || cmd == IrCmd::SUBSTITUTE;
}

inline bool hasSideEffects(IrCmd cmd)
{
    if (cmd == IrCmd::INVOKE_FASTCALL)
        return true;

    if (isPseudo(cmd))
        return false;

    // Instructions that don't produce a result most likely have other side-effects to make them useful
    // Right now, a full switch would mirror the 'hasResult' function, so we use this simple condition
    return !hasResult(cmd);
}

inline bool producesDirtyHighRegisterBits(IrCmd cmd)
{
    return cmd == IrCmd::NUM_TO_UINT || cmd == IrCmd::INVOKE_FASTCALL || cmd == IrCmd::CMP_ANY;
}

// Returns a condition that for 'a op b' will result in '!(a op b)'
inline IrCondition getNegatedCondition(IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return IrCondition::NotEqual;
    case IrCondition::NotEqual:
        return IrCondition::Equal;
    case IrCondition::Less:
        return IrCondition::NotLess;
    case IrCondition::NotLess:
        return IrCondition::Less;
    case IrCondition::LessEqual:
        return IrCondition::NotLessEqual;
    case IrCondition::NotLessEqual:
        return IrCondition::LessEqual;
    case IrCondition::Greater:
        return IrCondition::NotGreater;
    case IrCondition::NotGreater:
        return IrCondition::Greater;
    case IrCondition::GreaterEqual:
        return IrCondition::NotGreaterEqual;
    case IrCondition::NotGreaterEqual:
        return IrCondition::GreaterEqual;
    case IrCondition::UnsignedLess:
        return IrCondition::UnsignedGreaterEqual;
    case IrCondition::UnsignedLessEqual:
        return IrCondition::UnsignedGreater;
    case IrCondition::UnsignedGreater:
        return IrCondition::UnsignedLessEqual;
    case IrCondition::UnsignedGreaterEqual:
        return IrCondition::UnsignedLess;
    default:
        CODEGEN_ASSERT(!"Unsupported condition");
        return IrCondition::Count;
    }
}

template<typename F>
void visitArguments(IrInst& inst, F&& func)
{
    if (isPseudo(inst.cmd))
        return;

    for (auto& op : inst.ops)
        func(op);
}
template<typename F>
bool anyArgumentMatch(IrInst& inst, F&& func)
{
    if (isPseudo(inst.cmd))
        return false;

    for (auto& op : inst.ops)
        if (func(op))
            return true;
    return false;
}

bool isGCO(uint8_t tag);

// Optional bit has to be cleared at call site, otherwise, this will return 'false' for 'userdata?'
bool isUserdataBytecodeType(uint8_t ty);
bool isCustomUserdataBytecodeType(uint8_t ty);

// Check that 'ty' is 'expected' or 'any'
bool isExpectedOrUnknownBytecodeType(uint8_t ty, LuauBytecodeType expected);

HostMetamethod tmToHostMetamethod(int tm);

// Manually add or remove use of an operand
void addUse(IrFunction& function, IrOp op);
void removeUse(IrFunction& function, IrOp op);

// Remove a single instruction
void kill(IrFunction& function, IrInst& inst);

// Remove a range of instructions
void kill(IrFunction& function, uint32_t start, uint32_t end);

// Remove a block, including all instructions inside
void kill(IrFunction& function, IrBlock& block);

// Replace a single operand and update use counts (can cause chain removal of dead code)
void replace(IrFunction& function, IrOp& original, IrOp replacement);

// Replace a single instruction
// Target instruction index instead of reference is used to handle introduction of a new block terminator
void replace(IrFunction& function, IrBlock& block, uint32_t instIdx, IrInst replacement);

// Replace instruction with a different value (using IrCmd::SUBSTITUTE)
void substitute(IrFunction& function, IrInst& inst, IrOp replacement);

// Replace instruction arguments that point to substitutions with target values
void applySubstitutions(IrFunction& function, IrOp& op);
void applySubstitutions(IrFunction& function, IrInst& inst);

// Compare numbers using IR condition value
bool compare(double a, double b, IrCondition cond);

// Perform constant folding on instruction at index
// For most instructions, successful folding results in a IrCmd::SUBSTITUTE
// But it can also be successful on conditional control-flow, replacing it with an unconditional IrCmd::JUMP
void foldConstants(IrBuilder& build, IrFunction& function, IrBlock& block, uint32_t instIdx);

uint32_t getNativeContextOffset(int bfid);

// Cleans up blocks that were created with no users
void killUnusedBlocks(IrFunction& function);

// Get blocks in order that tries to maximize fallthrough between them during lowering
// We want to mostly preserve build order with fallbacks outlined
// But we also use hints from optimization passes that chain blocks together where there's only one out-in edge between them
std::vector<uint32_t> getSortedBlockOrder(IrFunction& function);

// Returns first non-dead block that comes after block at index 'i' in the sorted blocks array
// 'dummy' block is returned if the end of array was reached
IrBlock& getNextBlock(IrFunction& function, const std::vector<uint32_t>& sortedBlocks, IrBlock& dummy, size_t i);

// Returns next block in a chain, marked by 'constPropInBlockChains' optimization pass
IrBlock* tryGetNextBlockInChain(IrFunction& function, IrBlock& block);

bool isEntryBlock(const IrBlock& block);

// When an operand is an instruction, try to extract the tag which is contained inside that value
std::optional<uint8_t> tryGetOperandTag(IrFunction& function, IrOp op);

// Propagates register tags from predecessor blocks' exit states into the current block's entry state for live in registers
// Calls getTag for each register slot to read current tag value (kUnknownTag if unknown)
// Calls setTag to update the tag value (kUnknownTag if it cannot be determined)
// Assigns the tag directly for the first predecessor
// For subsequent predecessors, intersects and sets kUnknownTag when predecessors disagree
void propagateTagsFromPredecessors(
    const IrFunction& function,
    const IrBlock& block,
    std::function<uint8_t(size_t)> getTag,
    std::function<void(size_t, uint8_t)> setTag
);

// If optional part is not ignored, types like 'number?' will fail to convert
std::optional<uint8_t> tryGetLuauTagForBcType(uint8_t bcType, bool ignoreOptionalPart);

} // namespace CodeGen
} // namespace Luau
