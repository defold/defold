// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/IrUtils.h"

#include "Luau/CodeGenCommon.h"
#include "Luau/CodeGenOptions.h"
#include "Luau/IrBuilder.h"

#include "BitUtils.h"
#include "Luau/IrData.h"
#include "NativeState.h"

#include "lua.h"
#include "lnumutils.h"

#include <algorithm>
#include <vector>

#include <limits.h>
#include <math.h>

LUAU_FASTFLAG(LuauCodegenBufferRangeMerge4)
LUAU_FASTFLAG(LuauCodegenPropagateTagsAcrossChains2)
LUAU_FASTFLAGVARIABLE(LuauCodegenConsistentHasResult)

namespace Luau
{
namespace CodeGen
{

constexpr double kDoubleMaxExactInteger = 9007199254740992.0;

int getOpLength(LuauOpcode op)
{
    switch (int(op))
    {
    case LOP_GETGLOBAL:
    case LOP_SETGLOBAL:
    case LOP_GETIMPORT:
    case LOP_GETTABLEKS:
    case LOP_SETTABLEKS:
    case LOP_NAMECALL:
    case LOP_JUMPIFEQ:
    case LOP_JUMPIFLE:
    case LOP_JUMPIFLT:
    case LOP_JUMPIFNOTEQ:
    case LOP_JUMPIFNOTLE:
    case LOP_JUMPIFNOTLT:
    case LOP_NEWTABLE:
    case LOP_SETLIST:
    case LOP_FORGLOOP:
    case LOP_LOADKX:
    case LOP_FASTCALL2:
    case LOP_FASTCALL2K:
    case LOP_FASTCALL3:
    case LOP_JUMPXEQKNIL:
    case LOP_JUMPXEQKB:
    case LOP_JUMPXEQKN:
    case LOP_JUMPXEQKS:
    case LOP_GETUDATAKS:
    case LOP_SETUDATAKS:
    case LOP_NAMECALLUDATA:
        return 2;

    default:
        return 1;
    }
}

bool isJumpD(LuauOpcode op)
{
    switch (int(op))
    {
    case LOP_JUMP:
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
    case LOP_FORGPREP:
    case LOP_FORGLOOP:
    case LOP_FORGPREP_INEXT:
    case LOP_FORGPREP_NEXT:
    case LOP_JUMPBACK:
    case LOP_JUMPXEQKNIL:
    case LOP_JUMPXEQKB:
    case LOP_JUMPXEQKN:
    case LOP_JUMPXEQKS:
        return true;

    default:
        return false;
    }
}

bool isSkipC(LuauOpcode op)
{
    switch (int(op))
    {
    case LOP_LOADB:
        return true;

    default:
        return false;
    }
}

bool isFastCall(LuauOpcode op)
{
    switch (int(op))
    {
    case LOP_FASTCALL:
    case LOP_FASTCALL1:
    case LOP_FASTCALL2:
    case LOP_FASTCALL2K:
    case LOP_FASTCALL3:
        return true;

    default:
        return false;
    }
}

int getJumpTarget(uint32_t insn, uint32_t pc)
{
    LuauOpcode op = LuauOpcode(LUAU_INSN_OP(insn));

    if (isJumpD(op))
        return int(pc + LUAU_INSN_D(insn) + 1);
    else if (isFastCall(op))
        return int(pc + LUAU_INSN_C(insn) + 2);
    else if (isSkipC(op) && LUAU_INSN_C(insn))
        return int(pc + LUAU_INSN_C(insn) + 1);
    else if (int(op) == LOP_JUMPX)
        return int(pc + LUAU_INSN_E(insn) + 1);
    else
        return -1;
}

IrValueKind getCmdValueKind(IrCmd cmd)
{
    switch (cmd)
    {
    case IrCmd::NOP:
        return IrValueKind::None;
    case IrCmd::LOAD_TAG:
        return IrValueKind::Tag;
    case IrCmd::LOAD_POINTER:
        return IrValueKind::Pointer;
    case IrCmd::LOAD_DOUBLE:
        return IrValueKind::Double;
    case IrCmd::LOAD_INT:
        return IrValueKind::Int;
    case IrCmd::LOAD_FLOAT:
        return IrValueKind::Float;
    case IrCmd::LOAD_TVALUE:
        return IrValueKind::Tvalue;
    case IrCmd::LOAD_ENV:
    case IrCmd::GET_ARR_ADDR:
    case IrCmd::GET_SLOT_NODE_ADDR:
    case IrCmd::GET_HASH_NODE_ADDR:
    case IrCmd::GET_CLOSURE_UPVAL_ADDR:
        return IrValueKind::Pointer;
    case IrCmd::STORE_TAG:
    case IrCmd::STORE_EXTRA:
    case IrCmd::STORE_POINTER:
    case IrCmd::STORE_DOUBLE:
    case IrCmd::STORE_INT:
    case IrCmd::STORE_INT64:
    case IrCmd::STORE_VECTOR:
    case IrCmd::STORE_TVALUE:
    case IrCmd::STORE_SPLIT_TVALUE:
    case IrCmd::CHECK_DIV_INT64:
        return IrValueKind::None;
    case IrCmd::LOAD_INT64:
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
        return IrValueKind::Int64;
    case IrCmd::ADD_INT:
    case IrCmd::SUB_INT:
    case IrCmd::SEXTI8_INT:
    case IrCmd::SEXTI16_INT:
        return IrValueKind::Int;
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
    case IrCmd::SELECT_NUM:
    case IrCmd::MULADD_NUM:
        return IrValueKind::Double;
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
        return IrValueKind::Float;
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
    case IrCmd::SELECT_VEC:
    case IrCmd::SELECT_IF_TRUTHY:
    case IrCmd::MULADD_VEC:
        return IrValueKind::Tvalue;
    case IrCmd::DOT_VEC:
    case IrCmd::EXTRACT_VEC:
        return IrValueKind::Float;
    case IrCmd::NOT_ANY:
    case IrCmd::CMP_ANY:
    case IrCmd::CMP_INT:
    case IrCmd::CMP_INT64:
    case IrCmd::CMP_TAG:
    case IrCmd::CMP_SPLIT_TVALUE:
        return IrValueKind::Int;
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
        return IrValueKind::None;
    case IrCmd::TABLE_LEN:
        return IrValueKind::Int;
    case IrCmd::TABLE_SETNUM:
        return IrValueKind::Pointer;
    case IrCmd::STRING_LEN:
        return IrValueKind::Int;
    case IrCmd::NEW_TABLE:
    case IrCmd::DUP_TABLE:
        return IrValueKind::Pointer;
    case IrCmd::TRY_NUM_TO_INDEX:
        return IrValueKind::Int;
    case IrCmd::TRY_CALL_FASTGETTM:
    case IrCmd::NEW_USERDATA:
        return IrValueKind::Pointer;
    case IrCmd::INT64_TO_NUM:
    case IrCmd::INT_TO_NUM:
    case IrCmd::UINT_TO_NUM:
        return IrValueKind::Double;
    case IrCmd::UINT_TO_FLOAT:
        return IrValueKind::Float;
    case IrCmd::NUM_TO_INT:
    case IrCmd::NUM_TO_UINT:
        return IrValueKind::Int;
    case IrCmd::NUM_TO_INT64:
        return IrValueKind::Int64;
    case IrCmd::FLOAT_TO_NUM:
        return IrValueKind::Double;
    case IrCmd::NUM_TO_FLOAT:
        return IrValueKind::Float;
    case IrCmd::FLOAT_TO_VEC:
    case IrCmd::TAG_VECTOR:
        return IrValueKind::Tvalue;
    case IrCmd::TRUNCATE_UINT:
        return IrValueKind::Int;
    case IrCmd::ADJUST_STACK_TO_REG:
    case IrCmd::ADJUST_STACK_TO_TOP:
        return IrValueKind::None;
    case IrCmd::FASTCALL:
        return IrValueKind::None;
    case IrCmd::INVOKE_FASTCALL:
        return IrValueKind::Int;
    case IrCmd::CHECK_FASTCALL_RES:
    case IrCmd::DO_ARITH:
    case IrCmd::DO_LEN:
    case IrCmd::GET_TABLE:
    case IrCmd::SET_TABLE:
    case IrCmd::GET_CACHED_IMPORT:
    case IrCmd::CONCAT:
        return IrValueKind::None;
    case IrCmd::GET_UPVALUE:
        return IrValueKind::Tvalue;
    case IrCmd::SET_UPVALUE:
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
    case IrCmd::INTERRUPT:
    case IrCmd::CHECK_GC:
    case IrCmd::BARRIER_OBJ:
    case IrCmd::BARRIER_TABLE_BACK:
    case IrCmd::BARRIER_TABLE_FORWARD:
    case IrCmd::SET_SAVEDPC:
    case IrCmd::CLOSE_UPVALS:
    case IrCmd::CAPTURE:
    case IrCmd::SETLIST:
    case IrCmd::CALL:
    case IrCmd::RETURN:
    case IrCmd::FORGLOOP:
    case IrCmd::FORGLOOP_FALLBACK:
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
    case IrCmd::COVERAGE:
    case IrCmd::FALLBACK_GETGLOBAL:
    case IrCmd::FALLBACK_SETGLOBAL:
    case IrCmd::FALLBACK_GETTABLEKS:
    case IrCmd::FALLBACK_SETTABLEKS:
    case IrCmd::FALLBACK_NAMECALL:
    case IrCmd::FALLBACK_PREPVARARGS:
    case IrCmd::FALLBACK_GETVARARGS:
        return IrValueKind::None;
    case IrCmd::NEWCLOSURE:
        return IrValueKind::Pointer;
    case IrCmd::FALLBACK_DUPCLOSURE:
    case IrCmd::FALLBACK_FORGPREP:
        return IrValueKind::None;
    case IrCmd::SUBSTITUTE:
        return IrValueKind::Unknown;
    case IrCmd::MARK_USED:
    case IrCmd::MARK_DEAD:
        return IrValueKind::None;
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
    case IrCmd::BYTESWAP_UINT:
        return IrValueKind::Int;
    case IrCmd::INVOKE_LIBM:
        return IrValueKind::Double;
    case IrCmd::GET_TYPE:
    case IrCmd::GET_TYPEOF:
        return IrValueKind::Pointer;
    case IrCmd::FINDUPVAL:
        return IrValueKind::Pointer;
    case IrCmd::BUFFER_READI8:
    case IrCmd::BUFFER_READU8:
    case IrCmd::BUFFER_READI16:
    case IrCmd::BUFFER_READU16:
    case IrCmd::BUFFER_READI32:
        return IrValueKind::Int;
    case IrCmd::BUFFER_WRITEI8:
    case IrCmd::BUFFER_WRITEI16:
    case IrCmd::BUFFER_WRITEI32:
    case IrCmd::BUFFER_WRITEF32:
    case IrCmd::BUFFER_WRITEF64:
        return IrValueKind::None;
    case IrCmd::BUFFER_READF32:
        return IrValueKind::Float;
    case IrCmd::BUFFER_READF64:
        return IrValueKind::Double;
    }

    LUAU_UNREACHABLE();
}

IrValueKind getConstValueKind(const IrConst& constant)
{
    switch (constant.kind)
    {
    case IrConstKind::Int:
        return IrValueKind::Int;
    case IrConstKind::Int64:
        return IrValueKind::Int64;
    case IrConstKind::Uint:
        return IrValueKind::Int;
    case IrConstKind::Double:
        return IrValueKind::Double;
    case IrConstKind::Tag:
        return IrValueKind::Tag;
    case IrConstKind::Import:
        CODEGEN_ASSERT(!"Import constants cannot be used as IR values");
        return IrValueKind::Unknown;
    }

    LUAU_UNREACHABLE();
}

static void removeInstUse(IrFunction& function, uint32_t instIdx)
{
    IrInst& inst = function.instructions[instIdx];

    CODEGEN_ASSERT(inst.useCount);
    inst.useCount--;

    if (inst.useCount == 0)
        kill(function, inst);
}

static void removeBlockUse(IrFunction& function, uint32_t blockIdx)
{
    IrBlock& block = function.blocks[blockIdx];

    CODEGEN_ASSERT(block.useCount);
    block.useCount--;

    // Entry block is never removed because is has an implicit use
    if (block.useCount == 0 && blockIdx != 0)
        kill(function, block);
}

void addUse(IrFunction& function, IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        function.instructions[op.index].useCount++;
    else if (op.kind == IrOpKind::Block)
        function.blocks[op.index].useCount++;
}

void removeUse(IrFunction& function, IrOp op)
{
    if (op.kind == IrOpKind::Inst)
        removeInstUse(function, op.index);
    else if (op.kind == IrOpKind::Block)
        removeBlockUse(function, op.index);
}

bool isGCO(uint8_t tag)
{
    CODEGEN_ASSERT(tag < LUA_T_COUNT);

    // mirrors iscollectable(o) from VM/lobject.h
    return tag >= LUA_TSTRING;
}

bool isUserdataBytecodeType(uint8_t ty)
{
    return ty == LBC_TYPE_USERDATA || isCustomUserdataBytecodeType(ty);
}

bool isCustomUserdataBytecodeType(uint8_t ty)
{
    return ty >= LBC_TYPE_TAGGED_USERDATA_BASE && ty < LBC_TYPE_TAGGED_USERDATA_END;
}

bool isExpectedOrUnknownBytecodeType(uint8_t ty, LuauBytecodeType expected)
{
    return ty == LBC_TYPE_ANY || ty == expected;
}

HostMetamethod tmToHostMetamethod(int tm)
{
    switch (TMS(tm))
    {
    case TM_ADD:
        return HostMetamethod::Add;
    case TM_SUB:
        return HostMetamethod::Sub;
    case TM_MUL:
        return HostMetamethod::Mul;
    case TM_DIV:
        return HostMetamethod::Div;
    case TM_IDIV:
        return HostMetamethod::Idiv;
    case TM_MOD:
        return HostMetamethod::Mod;
    case TM_POW:
        return HostMetamethod::Pow;
    case TM_UNM:
        return HostMetamethod::Minus;
    case TM_EQ:
        return HostMetamethod::Equal;
    case TM_LT:
        return HostMetamethod::LessThan;
    case TM_LE:
        return HostMetamethod::LessEqual;
    case TM_LEN:
        return HostMetamethod::Length;
    case TM_CONCAT:
        return HostMetamethod::Concat;
    default:
        CODEGEN_ASSERT(!"invalid tag method for host");
        break;
    }

    return HostMetamethod::Add;
}

void kill(IrFunction& function, IrInst& inst)
{
    CODEGEN_ASSERT(inst.useCount == 0);

    inst.cmd = IrCmd::NOP;

    for (auto& op : inst.ops)
        removeUse(function, op);
    inst.ops.clear();
}

void kill(IrFunction& function, uint32_t start, uint32_t end)
{
    // Kill instructions in reverse order to avoid killing instructions that are still marked as used
    for (int i = int(end); i >= int(start); i--)
    {
        CODEGEN_ASSERT(unsigned(i) < function.instructions.size());
        IrInst& curr = function.instructions[i];

        if (curr.cmd == IrCmd::NOP)
            continue;

        // Do not force destruction of instructions that are still in use
        // When the operands are released, the instruction will be released automatically
        if (curr.useCount != 0)
            continue;

        kill(function, curr);
    }
}

void kill(IrFunction& function, IrBlock& block)
{
    CODEGEN_ASSERT(block.useCount == 0);

    block.kind = IrBlockKind::Dead;

    kill(function, block.start, block.finish);
    block.start = ~0u;
    block.finish = ~0u;
}

void replace(IrFunction& function, IrOp& original, IrOp replacement)
{
    // Add use before removing new one if that's the last one keeping target operand alive
    addUse(function, replacement);
    removeUse(function, original);

    original = replacement;
}

void replace(IrFunction& function, IrBlock& block, uint32_t instIdx, IrInst replacement)
{
    IrInst& inst = function.instructions[instIdx];

    // Add uses before removing new ones if those are the last ones keeping target operand alive
    for (auto& op : replacement.ops)
        addUse(function, op);

    // If we introduced an earlier terminating instruction, all following instructions become dead
    if (!isBlockTerminator(inst.cmd) && isBlockTerminator(replacement.cmd))
    {
        // Block has has to be fully constructed before replacement is performed
        CODEGEN_ASSERT(block.finish != ~0u);
        CODEGEN_ASSERT(instIdx + 1 <= block.finish);

        kill(function, instIdx + 1, block.finish);

        // If killing that range killed the current block we have to undo replacement instruction uses and exit
        if (block.kind == IrBlockKind::Dead)
        {
            for (auto& op : replacement.ops)
                removeUse(function, op);
            return;
        }

        block.finish = instIdx;
    }

    // Before we remove old argument uses, we have to place our new instruction
    IrInst copy = inst;

    // Inherit existing use count (last use is skipped as it will be defined later)
    replacement.useCount = inst.useCount;

    inst = replacement;

    for (auto& op : copy.ops)
        removeUse(function, op);
}

void substitute(IrFunction& function, IrInst& inst, IrOp replacement)
{
    CODEGEN_ASSERT(!isBlockTerminator(inst.cmd));

    inst.cmd = IrCmd::SUBSTITUTE;

    addUse(function, replacement);

    for (auto& op : inst.ops)
        removeUse(function, op);

    inst.ops.resize(1);
    OP_A(inst) = replacement;
}

void applySubstitutions(IrFunction& function, IrOp& op)
{
    if (op.kind == IrOpKind::Inst)
    {
        IrInst& src = function.instructions[op.index];

        if (src.cmd == IrCmd::SUBSTITUTE)
        {
            op.kind = OP_A(src).kind;
            op.index = OP_A(src).index;

            // If we substitute with the result of a different instruction, update the use count
            if (op.kind == IrOpKind::Inst)
            {
                IrInst& dst = function.instructions[op.index];
                CODEGEN_ASSERT(dst.cmd != IrCmd::SUBSTITUTE && "chained substitutions are not allowed");

                dst.useCount++;
            }

            CODEGEN_ASSERT(src.useCount > 0);
            src.useCount--;

            if (src.useCount == 0)
            {
                src.cmd = IrCmd::NOP;
                removeUse(function, OP_A(src));
                src.ops.clear();
            }
        }
    }
}

void applySubstitutions(IrFunction& function, IrInst& inst)
{
    for (auto& op : inst.ops)
        applySubstitutions(function, op);
}

bool compare(double a, double b, IrCondition cond)
{
    // Note: redundant bool() casts work around invalid MSVC optimization that merges cases in this switch, violating IEEE754 comparison semantics
    switch (cond)
    {
    case IrCondition::Equal:
        return a == b;
    case IrCondition::NotEqual:
        return a != b;
    case IrCondition::Less:
        return a < b;
    case IrCondition::NotLess:
        return !bool(a < b);
    case IrCondition::LessEqual:
        return a <= b;
    case IrCondition::NotLessEqual:
        return !bool(a <= b);
    case IrCondition::Greater:
        return a > b;
    case IrCondition::NotGreater:
        return !bool(a > b);
    case IrCondition::GreaterEqual:
        return a >= b;
    case IrCondition::NotGreaterEqual:
        return !bool(a >= b);
    default:
        CODEGEN_ASSERT(!"Unsupported condition");
    }

    return false;
}

bool compare(int a, int b, IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return a == b;
    case IrCondition::NotEqual:
        return a != b;
    case IrCondition::Less:
        return a < b;
    case IrCondition::NotLess:
        return !(a < b);
    case IrCondition::LessEqual:
        return a <= b;
    case IrCondition::NotLessEqual:
        return !(a <= b);
    case IrCondition::Greater:
        return a > b;
    case IrCondition::NotGreater:
        return !(a > b);
    case IrCondition::GreaterEqual:
        return a >= b;
    case IrCondition::NotGreaterEqual:
        return !(a >= b);
    case IrCondition::UnsignedLess:
        return unsigned(a) < unsigned(b);
    case IrCondition::UnsignedLessEqual:
        return unsigned(a) <= unsigned(b);
    case IrCondition::UnsignedGreater:
        return unsigned(a) > unsigned(b);
    case IrCondition::UnsignedGreaterEqual:
        return unsigned(a) >= unsigned(b);
    default:
        CODEGEN_ASSERT(!"Unsupported condition");
    }

    return false;
}

bool compare(int64_t a, int64_t b, IrCondition cond)
{
    switch (cond)
    {
    case IrCondition::Equal:
        return a == b;
    case IrCondition::NotEqual:
        return a != b;
    case IrCondition::Less:
        return a < b;
    case IrCondition::NotLess:
        return !(a < b);
    case IrCondition::LessEqual:
        return a <= b;
    case IrCondition::NotLessEqual:
        return !(a <= b);
    case IrCondition::Greater:
        return a > b;
    case IrCondition::NotGreater:
        return !(a > b);
    case IrCondition::GreaterEqual:
        return a >= b;
    case IrCondition::NotGreaterEqual:
        return !(a >= b);
    case IrCondition::UnsignedLess:
        return uint64_t(a) < uint64_t(b);
    case IrCondition::UnsignedLessEqual:
        return uint64_t(a) <= uint64_t(b);
    case IrCondition::UnsignedGreater:
        return uint64_t(a) > uint64_t(b);
    case IrCondition::UnsignedGreaterEqual:
        return uint64_t(a) >= uint64_t(b);
    default:
        CODEGEN_ASSERT(!"Unsupported condition");
    }

    return false;
}

static void substituteWithTruncatedUint(IrFunction& function, IrBlock& block, IrInst& inst, IrOp op)
{
    if (IrInst* srcOfSrc = function.asInstOp(op); srcOfSrc && producesDirtyHighRegisterBits(srcOfSrc->cmd))
        replace(function, block, function.getInstIndex(inst), IrInst{IrCmd::TRUNCATE_UINT, {op}});
    else
        substitute(function, inst, op);
}

void foldConstants(IrBuilder& build, IrFunction& function, IrBlock& block, uint32_t index)
{
    IrInst& inst = function.instructions[index];

    switch (inst.cmd)
    {
    case IrCmd::ADD_INT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            // We need to avoid signed integer overflow, but we also have to produce a result
            // So we add numbers as unsigned and use fixed-width integer types to force a two's complement evaluation
            int32_t lhs = function.intOp(OP_A(inst));
            int32_t rhs = function.intOp(OP_B(inst));
            int sum = int32_t(uint32_t(lhs) + uint32_t(rhs));

            substitute(function, inst, build.constInt(sum));
        }
        break;
    case IrCmd::SUB_INT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            // We need to avoid signed integer overflow, but we also have to produce a result
            // So we subtract numbers as unsigned and use fixed-width integer types to force a two's complement evaluation
            int32_t lhs = function.intOp(OP_A(inst));
            int32_t rhs = function.intOp(OP_B(inst));
            int sum = int32_t(uint32_t(lhs) - uint32_t(rhs));

            substitute(function, inst, build.constInt(sum));
        }
        break;
    case IrCmd::SEXTI8_INT:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            int32_t value = int8_t(function.intOp(OP_A(inst)));
            substitute(function, inst, build.constInt(value));
        }
        break;
    case IrCmd::SEXTI16_INT:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            int32_t value = int16_t(function.intOp(OP_A(inst)));
            substitute(function, inst, build.constInt(value));
        }
        break;
    case IrCmd::ADD_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(function.doubleOp(OP_A(inst)) + function.doubleOp(OP_B(inst))));
        break;
    case IrCmd::SUB_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(function.doubleOp(OP_A(inst)) - function.doubleOp(OP_B(inst))));
        break;
    case IrCmd::MUL_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(function.doubleOp(OP_A(inst)) * function.doubleOp(OP_B(inst))));
        break;
    case IrCmd::DIV_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(function.doubleOp(OP_A(inst)) / function.doubleOp(OP_B(inst))));
        break;
    case IrCmd::IDIV_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(luai_numidiv(function.doubleOp(OP_A(inst)), function.doubleOp(OP_B(inst)))));
        break;
    case IrCmd::MOD_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(luai_nummod(function.doubleOp(OP_A(inst)), function.doubleOp(OP_B(inst)))));
        break;
    case IrCmd::MIN_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            double a1 = function.doubleOp(OP_A(inst));
            double a2 = function.doubleOp(OP_B(inst));

            substitute(function, inst, build.constDouble(a1 < a2 ? a1 : a2));
        }
        break;
    case IrCmd::MAX_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            double a1 = function.doubleOp(OP_A(inst));
            double a2 = function.doubleOp(OP_B(inst));

            substitute(function, inst, build.constDouble(a1 > a2 ? a1 : a2));
        }
        break;
    case IrCmd::UNM_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(-function.doubleOp(OP_A(inst))));
        break;
    case IrCmd::FLOOR_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(floor(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::CEIL_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(ceil(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::ROUND_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(round(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::SQRT_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(sqrt(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::ABS_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(fabs(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::SIGN_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            double v = function.doubleOp(OP_A(inst));

            substitute(function, inst, build.constDouble(v > 0.0 ? 1.0 : v < 0.0 ? -1.0 : 0.0));
        }
        break;
    case IrCmd::ADD_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(float(function.doubleOp(OP_A(inst))) + float(function.doubleOp(OP_B(inst)))));
        break;
    case IrCmd::SUB_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(float(function.doubleOp(OP_A(inst))) - float(function.doubleOp(OP_B(inst)))));
        break;
    case IrCmd::MUL_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(float(function.doubleOp(OP_A(inst))) * float(function.doubleOp(OP_B(inst)))));
        break;
    case IrCmd::DIV_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(float(function.doubleOp(OP_A(inst))) / float(function.doubleOp(OP_B(inst)))));
        break;
    case IrCmd::MIN_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            float a1 = float(function.doubleOp(OP_A(inst)));
            float a2 = float(function.doubleOp(OP_B(inst)));

            substitute(function, inst, build.constDouble(a1 < a2 ? a1 : a2));
        }
        break;
    case IrCmd::MAX_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            float a1 = float(function.doubleOp(OP_A(inst)));
            float a2 = float(function.doubleOp(OP_B(inst)));

            substitute(function, inst, build.constDouble(a1 > a2 ? a1 : a2));
        }
        break;
    case IrCmd::UNM_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(-float(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::FLOOR_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(floorf(float(function.doubleOp(OP_A(inst))))));
        break;
    case IrCmd::CEIL_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(ceilf(float(function.doubleOp(OP_A(inst))))));
        break;
    case IrCmd::SQRT_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(sqrtf(float(function.doubleOp(OP_A(inst))))));
        break;
    case IrCmd::ABS_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(fabsf(float(function.doubleOp(OP_A(inst))))));
        break;
    case IrCmd::SIGN_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            float v = float(function.doubleOp(OP_A(inst)));

            substitute(function, inst, build.constDouble(v > 0.0f ? 1.0f : v < 0.0f ? -1.0f : 0.0f));
        }
        break;
    case IrCmd::SELECT_NUM:
        if (OP_C(inst).kind == IrOpKind::Constant && OP_D(inst).kind == IrOpKind::Constant)
        {
            double c = function.doubleOp(OP_C(inst));
            double d = function.doubleOp(OP_D(inst));

            substitute(function, inst, c == d ? OP_B(inst) : OP_A(inst));
        }
        else if (OP_A(inst) == OP_B(inst))
        {
            // If the values are the same, no need to worry about the condition check
            substitute(function, inst, OP_A(inst));
        }
        break;
    case IrCmd::SELECT_VEC:
        if (OP_A(inst) == OP_B(inst))
        {
            // If the values are the same, no need to worry about the condition check
            substitute(function, inst, OP_A(inst));
        }
        break;
    case IrCmd::SELECT_IF_TRUTHY:
        if (OP_B(inst) == OP_C(inst))
        {
            // If the values are the same, no need to worry about the condition check
            substitute(function, inst, OP_B(inst));
        }
        break;
    case IrCmd::NOT_ANY:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            uint8_t a = function.tagOp(OP_A(inst));

            if (a == LUA_TNIL)
                substitute(function, inst, build.constInt(1));
            else if (a != LUA_TBOOLEAN)
                substitute(function, inst, build.constInt(0));
            else if (OP_B(inst).kind == IrOpKind::Constant)
                substitute(function, inst, build.constInt(function.intOp(OP_B(inst)) == 1 ? 0 : 1));
        }
        break;
    case IrCmd::CMP_INT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.intOp(OP_A(inst)), function.intOp(OP_B(inst)), conditionOp(OP_C(inst))))
                substitute(function, inst, build.constInt(1));
            else
                substitute(function, inst, build.constInt(0));
        }
        break;
    case IrCmd::CMP_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.int64Op(OP_A(inst)), function.int64Op(OP_B(inst)), conditionOp(OP_C(inst))))
                substitute(function, inst, build.constInt(1));
            else
                substitute(function, inst, build.constInt(0));
        }
        break;
    case IrCmd::CMP_TAG:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            IrCondition cond = conditionOp(OP_C(inst));
            CODEGEN_ASSERT(cond == IrCondition::Equal || cond == IrCondition::NotEqual);

            if (cond == IrCondition::Equal)
                substitute(function, inst, build.constInt(function.tagOp(OP_A(inst)) == function.tagOp(OP_B(inst)) ? 1 : 0));
            else
                substitute(function, inst, build.constInt(function.tagOp(OP_A(inst)) != function.tagOp(OP_B(inst)) ? 1 : 0));
        }
        break;
    case IrCmd::CMP_SPLIT_TVALUE:
    {
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);

        IrCondition cond = conditionOp(OP_E(inst));
        CODEGEN_ASSERT(cond == IrCondition::Equal || cond == IrCondition::NotEqual);

        if (cond == IrCondition::Equal)
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.tagOp(OP_A(inst)) != function.tagOp(OP_B(inst)))
            {
                substitute(function, inst, build.constInt(0));
            }
            else if (OP_C(inst).kind == IrOpKind::Constant && OP_D(inst).kind == IrOpKind::Constant)
            {
                // If the tag is a constant, this means previous condition has failed because tags are the same
                bool knownSameTag = OP_A(inst).kind == IrOpKind::Constant;
                bool sameValue = false;

                if (function.tagOp(OP_B(inst)) == LUA_TBOOLEAN)
                    sameValue = compare(function.intOp(OP_C(inst)), function.intOp(OP_D(inst)), IrCondition::Equal);
                else if (function.tagOp(OP_B(inst)) == LUA_TNUMBER)
                    sameValue = compare(function.doubleOp(OP_C(inst)), function.doubleOp(OP_D(inst)), IrCondition::Equal);
                else if (function.tagOp(OP_B(inst)) == LUA_TINTEGER)
                    sameValue = compare(function.int64Op(OP_C(inst)), function.int64Op(OP_D(inst)), IrCondition::Equal);
                else
                    CODEGEN_ASSERT(!"unsupported type");

                if (knownSameTag && sameValue)
                    substitute(function, inst, build.constInt(1));
                else if (sameValue)
                    replace(function, block, index, {IrCmd::CMP_TAG, {OP_A(inst), OP_B(inst), OP_E(inst)}});
                else
                    substitute(function, inst, build.constInt(0));
            }
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.tagOp(OP_A(inst)) != function.tagOp(OP_B(inst)))
            {
                substitute(function, inst, build.constInt(1));
            }
            else if (OP_C(inst).kind == IrOpKind::Constant && OP_D(inst).kind == IrOpKind::Constant)
            {
                // If the tag is a constant, this means previous condition has failed because tags are the same
                bool knownSameTag = OP_A(inst).kind == IrOpKind::Constant;
                bool differentValue = false;

                if (function.tagOp(OP_B(inst)) == LUA_TBOOLEAN)
                    differentValue = compare(function.intOp(OP_C(inst)), function.intOp(OP_D(inst)), IrCondition::NotEqual);
                else if (function.tagOp(OP_B(inst)) == LUA_TNUMBER)
                    differentValue = compare(function.doubleOp(OP_C(inst)), function.doubleOp(OP_D(inst)), IrCondition::NotEqual);
                else if (function.tagOp(OP_B(inst)) == LUA_TINTEGER)
                    differentValue = compare(function.int64Op(OP_C(inst)), function.int64Op(OP_D(inst)), IrCondition::NotEqual);
                else
                    CODEGEN_ASSERT(!"unsupported type");

                if (differentValue)
                    substitute(function, inst, build.constInt(1));
                else if (knownSameTag)
                    substitute(function, inst, build.constInt(0));
                else
                    replace(function, block, index, {IrCmd::CMP_TAG, {OP_A(inst), OP_B(inst), OP_E(inst)}});
            }
        }
    }
    break;
    case IrCmd::JUMP_EQ_TAG:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (function.tagOp(OP_A(inst)) == function.tagOp(OP_B(inst)))
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
        }
        break;
    case IrCmd::JUMP_CMP_INT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.intOp(OP_A(inst)), function.intOp(OP_B(inst)), conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
        }
        break;
    case IrCmd::JUMP_CMP_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.doubleOp(OP_A(inst)), function.doubleOp(OP_B(inst)), conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
        }
        break;
    case IrCmd::JUMP_CMP_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(float(function.doubleOp(OP_A(inst))), float(function.doubleOp(OP_B(inst))), conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
        }
        break;
    case IrCmd::TRY_NUM_TO_INDEX:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            double value = function.doubleOp(OP_A(inst));

            // To avoid undefined behavior of casting a value not representable in the target type, we check the range
            if (value >= INT_MIN && value <= INT_MAX)
            {
                int arrIndex = int(value);

                if (double(arrIndex) == value)
                    substitute(function, inst, build.constInt(arrIndex));
                else
                    replace(function, block, index, {IrCmd::JUMP, {OP_B(inst)}});
            }
            else
            {
                replace(function, block, index, {IrCmd::JUMP, {OP_B(inst)}});
            }
        }
        break;
    case IrCmd::INT_TO_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(double(function.intOp(OP_A(inst)))));
        break;
    case IrCmd::INT64_TO_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(double(function.int64Op(OP_A(inst)))));
        break;
    case IrCmd::UINT_TO_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(double(unsigned(function.intOp(OP_A(inst))))));
        break;
    case IrCmd::UINT_TO_FLOAT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(float(unsigned(function.intOp(OP_A(inst))))));
        break;
    case IrCmd::NUM_TO_INT:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            double value = function.doubleOp(OP_A(inst));

            // To avoid undefined behavior of casting a value not representable in the target type, check the range (matches luai_num2int)
            if (value >= INT_MIN && value <= INT_MAX)
                substitute(function, inst, build.constInt(int(value)));
        }
        break;
    case IrCmd::NUM_TO_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            double value = function.doubleOp(OP_A(inst));

            // To avoid undefined behavior of casting a value not representable in the target type, check the range (matches luai_num2unsigned)
            if (value >= -kDoubleMaxExactInteger && value <= kDoubleMaxExactInteger)
                substitute(function, inst, build.constInt(unsigned((long long)function.doubleOp(OP_A(inst)))));
        }
        break;
    case IrCmd::NUM_TO_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            double value = function.doubleOp(OP_A(inst));

            // To avoid undefined behavior of casting a value not representable in the target type, check the range
            if (value >= double(INT64_MIN) && value < double(INT64_MAX))
                substitute(function, inst, build.constInt64(int64_t(value)));
        }
        break;
    case IrCmd::FLOAT_TO_NUM:
        // float -> double for a constant is a no-op
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(function.doubleOp(OP_A(inst))));
        break;
    case IrCmd::NUM_TO_FLOAT:
        // double -> float for a constant just needs to lower precision
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constDouble(float(function.doubleOp(OP_A(inst)))));
        break;
    case IrCmd::TRUNCATE_UINT:
        // Truncating a constant integer is a no-op as constant integers only store 32 bits
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, OP_A(inst));
        break;
    case IrCmd::CHECK_TAG:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (function.tagOp(OP_A(inst)) == function.tagOp(OP_B(inst)))
                kill(function, inst);
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}}); // Shows a conflict in assumptions on this path
        }
        break;
    case IrCmd::CHECK_TRUTHY:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            if (function.tagOp(OP_A(inst)) == LUA_TNIL)
            {
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}}); // Shows a conflict in assumptions on this path
            }
            else if (function.tagOp(OP_A(inst)) == LUA_TBOOLEAN)
            {
                if (OP_B(inst).kind == IrOpKind::Constant)
                {
                    if (function.intOp(OP_B(inst)) == 0)
                        replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}}); // Shows a conflict in assumptions on this path
                    else
                        kill(function, inst);
                }
            }
            else
            {
                kill(function, inst);
            }
        }
        break;
    case IrCmd::CHECK_CMP_NUM:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.doubleOp(OP_A(inst)), function.doubleOp(OP_B(inst)), conditionOp(OP_C(inst))))
                kill(function, inst);
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
        }
        break;
    case IrCmd::CHECK_CMP_INT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.intOp(OP_A(inst)), function.intOp(OP_B(inst)), conditionOp(OP_C(inst))))
                kill(function, inst);
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}}); // Shows a conflict in assumptions on this path
        }
        break;
    case IrCmd::ADD_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            substitute(function, inst, build.constInt64(int64_t(uint64_t(lhs) + uint64_t(rhs))));
        }
        break;
    case IrCmd::SUB_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            substitute(function, inst, build.constInt64(int64_t(uint64_t(lhs) - uint64_t(rhs))));
        }
        break;
    case IrCmd::MUL_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            substitute(function, inst, build.constInt64(int64_t(uint64_t(lhs) * uint64_t(rhs))));
        }
        break;
    case IrCmd::DIV_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            if (rhs != 0 && !(lhs == INT64_MIN && rhs == -1))
                substitute(function, inst, build.constInt64(lhs / rhs));
        }
        break;
    case IrCmd::IDIV_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            if (rhs != 0 && !(lhs == INT64_MIN && rhs == -1))
            {
                int64_t q = lhs / rhs;
                // Floored division: adjust if signs differ and there's a remainder
                if ((lhs ^ rhs) < 0 && q * rhs != lhs)
                    q -= 1;
                substitute(function, inst, build.constInt64(q));
            }
        }
        break;
    case IrCmd::UDIV_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            uint64_t lhs = uint64_t(function.int64Op(OP_A(inst)));
            uint64_t rhs = uint64_t(function.int64Op(OP_B(inst)));
            if (rhs != 0)
                substitute(function, inst, build.constInt64(int64_t(lhs / rhs)));
        }
        break;
    case IrCmd::REM_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            if (rhs != 0 && !(lhs == INT64_MIN && rhs == -1))
                substitute(function, inst, build.constInt64(lhs % rhs));
        }
        break;
    case IrCmd::UREM_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            uint64_t lhs = uint64_t(function.int64Op(OP_A(inst)));
            uint64_t rhs = uint64_t(function.int64Op(OP_B(inst)));
            if (rhs != 0)
                substitute(function, inst, build.constInt64(int64_t(lhs % rhs)));
        }
        break;
    case IrCmd::MOD_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            if (rhs != 0 && !(lhs == INT64_MIN && rhs == -1))
            {
                int64_t rem = lhs % rhs;
                // Floored modulus: adjust if remainder != 0 and signs differ
                if (rem != 0 && (rem ^ rhs) < 0)
                    rem += rhs;
                substitute(function, inst, build.constInt64(rem));
            }
        }
        break;
    case IrCmd::CHECK_DIV_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t lhs = function.int64Op(OP_A(inst));
            int64_t rhs = function.int64Op(OP_B(inst));
            if (rhs != 0 && !(lhs == INT64_MIN && rhs == -1))
                kill(function, inst); // guard is satisfied, eliminate it
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
        }
        break;
    case IrCmd::CHECK_CMP_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            if (compare(function.int64Op(OP_A(inst)), function.int64Op(OP_B(inst)), conditionOp(OP_C(inst))))
                kill(function, inst);
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
        }
        break;
    case IrCmd::BITAND_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t op1 = function.int64Op(OP_A(inst));
            int64_t op2 = function.int64Op(OP_B(inst));
            substitute(function, inst, build.constInt64(op1 & op2));
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.int64Op(OP_A(inst)) == 0) // (0 & b) -> 0
            {
                substitute(function, inst, build.constInt64(0));
            }
            else if (OP_A(inst).kind == IrOpKind::Constant && function.int64Op(OP_A(inst)) == -1) // (-1 & b) -> b
            {
                substitute(function, inst, OP_B(inst));
            }
            else if (OP_B(inst).kind == IrOpKind::Constant && function.int64Op(OP_B(inst)) == 0) // (a & 0) -> 0
            {
                substitute(function, inst, build.constInt64(0));
            }
            else if (OP_B(inst).kind == IrOpKind::Constant && function.int64Op(OP_B(inst)) == -1) // (a & -1) -> a
            {
                substitute(function, inst, OP_A(inst));
            }
        }
        break;
    case IrCmd::BITXOR_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t op1 = function.int64Op(OP_A(inst));
            int64_t op2 = function.int64Op(OP_B(inst));
            substitute(function, inst, build.constInt64(op1 ^ op2));
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.int64Op(OP_A(inst)) == 0) // (0 ^ b) -> b
            {
                substitute(function, inst, OP_B(inst));
            }
            else if (OP_B(inst).kind == IrOpKind::Constant && function.int64Op(OP_B(inst)) == 0) // (a ^ 0) -> a
            {
                substitute(function, inst, OP_A(inst));
            }
        }
        break;
    case IrCmd::BITOR_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t op1 = function.int64Op(OP_A(inst));
            int64_t op2 = function.int64Op(OP_B(inst));
            substitute(function, inst, build.constInt64(op1 | op2));
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.int64Op(OP_A(inst)) == 0) // (0 | b) -> b
            {
                substitute(function, inst, OP_B(inst));
            }
            else if (OP_A(inst).kind == IrOpKind::Constant && function.int64Op(OP_A(inst)) == -1) // (-1 | b) -> -1
            {
                substitute(function, inst, build.constInt64(-1));
            }
            else if (OP_B(inst).kind == IrOpKind::Constant && function.int64Op(OP_B(inst)) == 0) // (a | 0) -> a
            {
                substitute(function, inst, OP_A(inst));
            }
            else if (OP_B(inst).kind == IrOpKind::Constant && function.int64Op(OP_B(inst)) == -1) // (a | -1) -> -1
            {
                substitute(function, inst, build.constInt64(-1));
            }
        }
        break;
    case IrCmd::BITNOT_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            int64_t op1 = function.int64Op(OP_A(inst));
            substitute(function, inst, build.constInt64(~op1));
        }
        break;
    case IrCmd::BITLSHIFT_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            uint64_t n = uint64_t(function.int64Op(OP_A(inst)));
            int64_t i = function.int64Op(OP_B(inst));
            int64_t result;
            if (i >= -63 && i <= 63)
                result = int64_t((i < 0) ? (n >> (-i)) : (n << i));
            else
                result = 0;
            substitute(function, inst, build.constInt64(result));
        }
        break;
    case IrCmd::BITRSHIFT_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            uint64_t n = uint64_t(function.int64Op(OP_A(inst)));
            int64_t i = function.int64Op(OP_B(inst));
            int64_t result;
            if (i >= -63 && i <= 63)
                result = int64_t((i < 0) ? (n << (-i)) : (n >> i));
            else
                result = 0;
            substitute(function, inst, build.constInt64(result));
        }
        break;
    case IrCmd::BITARSHIFT_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t n = function.int64Op(OP_A(inst));
            int64_t i = function.int64Op(OP_B(inst));
            int64_t result;
            if (i >= -63 && i <= 63)
                result =
                    (i < 0) ? int64_t(uint64_t(n) << (-i)) : (n >> i); // signed right shift is implementation-defined in C++17, well-defined in C++20
            else if (i < -63)
                result = 0;
            else
                result = (n < 0) ? int64_t(-1) : int64_t(0);
            substitute(function, inst, build.constInt64(result));
        }
        break;
    case IrCmd::BITLROTATE_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            uint64_t n = uint64_t(function.int64Op(OP_A(inst)));
            unsigned s = unsigned(uint64_t(function.int64Op(OP_B(inst))) % 64);
            substitute(function, inst, build.constInt64(int64_t(s != 0 ? (n << s) | (n >> (64 - s)) : n)));
        }
        break;
    case IrCmd::BITRROTATE_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            uint64_t n = uint64_t(function.int64Op(OP_A(inst)));
            unsigned s = unsigned(uint64_t(function.int64Op(OP_B(inst))) % 64);
            substitute(function, inst, build.constInt64(int64_t(s != 0 ? (n >> s) | (n << (64 - s)) : n)));
        }
        break;
    case IrCmd::BITCOUNTLZ_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            uint64_t n = uint64_t(function.int64Op(OP_A(inst)));
            substitute(function, inst, build.constInt64(countlz(n)));
        }
        break;
    case IrCmd::BITCOUNTRZ_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            uint64_t n = uint64_t(function.int64Op(OP_A(inst)));
            substitute(function, inst, build.constInt64(countrz(n)));
        }
        break;
    case IrCmd::BYTESWAP_INT64:
        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            uint64_t a = uint64_t(function.int64Op(OP_A(inst)));
            uint64_t result = byteswap(a);

            substitute(function, inst, build.constInt64(int64_t(result)));
        }
        break;
    case IrCmd::BITAND_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            unsigned op1 = unsigned(function.intOp(OP_A(inst)));
            unsigned op2 = unsigned(function.intOp(OP_B(inst)));
            substitute(function, inst, build.constInt(op1 & op2));
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.intOp(OP_A(inst)) == 0) // (0 & b) -> 0
                substitute(function, inst, build.constInt(0));
            else if (OP_A(inst).kind == IrOpKind::Constant && function.intOp(OP_A(inst)) == -1) // (-1 & b) -> b
                substituteWithTruncatedUint(function, block, inst, OP_B(inst));
            else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0) // (a & 0) -> 0
                substitute(function, inst, build.constInt(0));
            else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == -1) // (a & -1) -> a
                substituteWithTruncatedUint(function, block, inst, OP_A(inst));
        }
        break;
    case IrCmd::BITXOR_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            unsigned op1 = unsigned(function.intOp(OP_A(inst)));
            unsigned op2 = unsigned(function.intOp(OP_B(inst)));
            substitute(function, inst, build.constInt(op1 ^ op2));
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.intOp(OP_A(inst)) == 0) // (0 ^ b) -> b
                substituteWithTruncatedUint(function, block, inst, OP_B(inst));
            else if (OP_A(inst).kind == IrOpKind::Constant && function.intOp(OP_A(inst)) == -1) // (-1 ^ b) -> ~b
                replace(function, block, index, {IrCmd::BITNOT_UINT, {OP_B(inst)}});
            else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0) // (a ^ 0) -> a
                substituteWithTruncatedUint(function, block, inst, OP_A(inst));
            else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == -1) // (a ^ -1) -> ~a
                replace(function, block, index, {IrCmd::BITNOT_UINT, {OP_A(inst)}});
        }
        break;
    case IrCmd::BITOR_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            unsigned op1 = unsigned(function.intOp(OP_A(inst)));
            unsigned op2 = unsigned(function.intOp(OP_B(inst)));
            substitute(function, inst, build.constInt(op1 | op2));
        }
        else
        {
            if (OP_A(inst).kind == IrOpKind::Constant && function.intOp(OP_A(inst)) == 0) // (0 | b) -> b
                substituteWithTruncatedUint(function, block, inst, OP_B(inst));
            else if (OP_A(inst).kind == IrOpKind::Constant && function.intOp(OP_A(inst)) == -1) // (-1 | b) -> -1
                substitute(function, inst, build.constInt(-1));
            else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0) // (a | 0) -> a
                substituteWithTruncatedUint(function, block, inst, OP_A(inst));
            else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == -1) // (a | -1) -> -1
                substitute(function, inst, build.constInt(-1));
        }
        break;
    case IrCmd::BITNOT_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constInt(~unsigned(function.intOp(OP_A(inst)))));
        break;
    case IrCmd::BITLSHIFT_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            unsigned op1 = unsigned(function.intOp(OP_A(inst)));
            int op2 = function.intOp(OP_B(inst));

            substitute(function, inst, build.constInt(op1 << (op2 & 31)));
        }
        else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0)
        {
            substituteWithTruncatedUint(function, block, inst, OP_A(inst));
        }
        break;
    case IrCmd::BITRSHIFT_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            unsigned op1 = unsigned(function.intOp(OP_A(inst)));
            int op2 = function.intOp(OP_B(inst));

            substitute(function, inst, build.constInt(op1 >> (op2 & 31)));
        }
        else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0)
        {
            substituteWithTruncatedUint(function, block, inst, OP_A(inst));
        }
        break;
    case IrCmd::BITARSHIFT_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
        {
            int op1 = function.intOp(OP_A(inst));
            int op2 = function.intOp(OP_B(inst));

            // note: technically right shift of negative values is UB, but this behavior is getting defined in C++20 and all compilers do the
            // right (shift) thing.
            substitute(function, inst, build.constInt(op1 >> (op2 & 31)));
        }
        else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0)
        {
            substituteWithTruncatedUint(function, block, inst, OP_A(inst));
        }
        break;
    case IrCmd::BITLROTATE_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constInt(lrotate(unsigned(function.intOp(OP_A(inst))), function.intOp(OP_B(inst)))));
        else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0)
            substituteWithTruncatedUint(function, block, inst, OP_A(inst));
        break;
    case IrCmd::BITRROTATE_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant && OP_B(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constInt(rrotate(unsigned(function.intOp(OP_A(inst))), function.intOp(OP_B(inst)))));
        else if (OP_B(inst).kind == IrOpKind::Constant && function.intOp(OP_B(inst)) == 0)
            substituteWithTruncatedUint(function, block, inst, OP_A(inst));
        break;
    case IrCmd::BITCOUNTLZ_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constInt(countlz(unsigned(function.intOp(OP_A(inst))))));
        break;
    case IrCmd::BITCOUNTRZ_UINT:
        if (OP_A(inst).kind == IrOpKind::Constant)
            substitute(function, inst, build.constInt(countrz(unsigned(function.intOp(OP_A(inst))))));
        break;
    case IrCmd::CHECK_BUFFER_LEN:
        if (FFlag::LuauCodegenBufferRangeMerge4)
        {
            if (OP_B(inst).kind == IrOpKind::Constant && OP_E(inst).kind == IrOpKind::Constant)
            {
                // If base offset and base offset source double value are both constants, we can get rid of that check or fallback
                if (double(function.intOp(OP_B(inst))) == function.doubleOp(OP_E(inst)))
                    replace(function, OP_E(inst), build.undef()); // This disables equality check at runtime
                else
                    replace(function, block, index, {IrCmd::JUMP, {OP_F(inst)}}); // Shows a conflict in assumptions on this path
            }
            else if (OP_B(inst).kind == IrOpKind::Inst && OP_E(inst).kind == IrOpKind::Constant)
            {
                // If only the base offset source double value is a constant, it means we couldn't constant-fold NUM_TO_INT
                CODEGEN_ASSERT(function.instOp(OP_B(inst)).cmd == IrCmd::NUM_TO_INT && OP_A(function.instOp(OP_B(inst))) == OP_E(inst));

                replace(function, block, index, {IrCmd::JUMP, {OP_F(inst)}}); // Shows a conflict in assumptions on this path
            }
        }
        break;
    default:
        break;
    }
}

uint32_t getNativeContextOffset(int bfid)
{
    switch (bfid)
    {
    case LBF_MATH_ACOS:
        return offsetof(NativeContext, libm_acos);
    case LBF_MATH_ASIN:
        return offsetof(NativeContext, libm_asin);
    case LBF_MATH_ATAN2:
        return offsetof(NativeContext, libm_atan2);
    case LBF_MATH_ATAN:
        return offsetof(NativeContext, libm_atan);
    case LBF_MATH_COSH:
        return offsetof(NativeContext, libm_cosh);
    case LBF_MATH_COS:
        return offsetof(NativeContext, libm_cos);
    case LBF_MATH_EXP:
        return offsetof(NativeContext, libm_exp);
    case LBF_MATH_LOG10:
        return offsetof(NativeContext, libm_log10);
    case LBF_MATH_LOG:
        return offsetof(NativeContext, libm_log);
    case LBF_MATH_SINH:
        return offsetof(NativeContext, libm_sinh);
    case LBF_MATH_SIN:
        return offsetof(NativeContext, libm_sin);
    case LBF_MATH_TANH:
        return offsetof(NativeContext, libm_tanh);
    case LBF_MATH_TAN:
        return offsetof(NativeContext, libm_tan);
    case LBF_MATH_FMOD:
        return offsetof(NativeContext, libm_fmod);
    case LBF_MATH_POW:
        return offsetof(NativeContext, libm_pow);
    case LBF_IR_MATH_LOG2:
        return offsetof(NativeContext, libm_log2);
    case LBF_MATH_LDEXP:
        return offsetof(NativeContext, libm_ldexp);
    default:
        CODEGEN_ASSERT(!"Unsupported bfid");
    }

    return 0;
}

void killUnusedBlocks(IrFunction& function)
{
    // Start from 1 as the first block is the entry block
    for (unsigned i = 1; i < function.blocks.size(); i++)
    {
        IrBlock& block = function.blocks[i];

        if (block.kind != IrBlockKind::Dead && block.useCount == 0)
            kill(function, block);
    }
}

std::vector<uint32_t> getSortedBlockOrder(IrFunction& function)
{
    std::vector<uint32_t> sortedBlocks;
    sortedBlocks.reserve(function.blocks.size());
    for (uint32_t i = 0; i < function.blocks.size(); i++)
        sortedBlocks.push_back(i);

    std::sort(
        sortedBlocks.begin(),
        sortedBlocks.end(),
        [&](uint32_t idxA, uint32_t idxB)
        {
            const IrBlock& a = function.blocks[idxA];
            const IrBlock& b = function.blocks[idxB];

            // Place fallback blocks at the end
            if ((a.kind == IrBlockKind::Fallback) != (b.kind == IrBlockKind::Fallback))
                return (a.kind == IrBlockKind::Fallback) < (b.kind == IrBlockKind::Fallback);

            // Try to order by instruction order
            if (a.sortkey != b.sortkey)
                return a.sortkey < b.sortkey;

            // Chains of blocks are merged together by having the same sort key and consecutive chain key
            return a.chainkey < b.chainkey;
        }
    );

    return sortedBlocks;
}

IrBlock& getNextBlock(IrFunction& function, const std::vector<uint32_t>& sortedBlocks, IrBlock& dummy, size_t i)
{
    for (size_t j = i + 1; j < sortedBlocks.size(); ++j)
    {
        IrBlock& block = function.blocks[sortedBlocks[j]];
        if (block.kind != IrBlockKind::Dead)
            return block;
    }

    return dummy;
}

IrBlock* tryGetNextBlockInChain(IrFunction& function, IrBlock& block)
{
    IrInst& termInst = function.instructions[block.finish];

    // Follow the strict block chain
    if (termInst.cmd == IrCmd::JUMP && OP_A(termInst).kind == IrOpKind::Block)
    {
        IrBlock& target = function.blockOp(OP_A(termInst));

        // Has to have the same sorting key and a consecutive chain key
        if (target.sortkey == block.sortkey && target.chainkey == block.chainkey + 1)
            return &target;
    }

    return nullptr;
}

bool isEntryBlock(const IrBlock& block)
{
    return block.useCount == 0 && block.kind != IrBlockKind::Dead;
}

std::optional<uint8_t> tryGetOperandTag(IrFunction& function, IrOp op)
{
    if (IrInst* arg = function.asInstOp(op))
    {
        if (arg->cmd == IrCmd::TAG_VECTOR)
            return LUA_TVECTOR;

        if (arg->cmd == IrCmd::LOAD_TVALUE && HAS_OP_C(*arg))
            return function.tagOp(OP_C(*arg));
    }

    return std::nullopt;
}

void propagateTagsFromPredecessors(
    const IrFunction& function,
    const IrBlock& block,
    std::function<uint8_t(size_t)> getTag,
    std::function<void(size_t, uint8_t)> setTag
)
{
    CODEGEN_ASSERT(FFlag::LuauCodegenPropagateTagsAcrossChains2);

    uint32_t blockIdx = function.getBlockIndex(block);

    if (blockIdx >= function.cfg.predecessorsOffsets.size())
        return;

    BlockIteratorWrapper preds = predecessors(function.cfg, blockIdx);

    if (preds.empty())
        return;

    size_t minRegsKnown = std::numeric_limits<size_t>::max();

    const size_t numBlockExitTags = function.blockExitTags.size();

    for (uint32_t predIdx : preds)
    {
        if (predIdx >= numBlockExitTags)
            return;

        minRegsKnown = std::min(minRegsKnown, function.blockExitTags[predIdx].size());
    }

    const RegisterSet& in = function.cfg.in[blockIdx];

    bool firstPredecessor = true;

    for (uint32_t predIdx : preds)
    {
        const std::vector<uint8_t>& predTags = function.blockExitTags[predIdx];

        CODEGEN_ASSERT(minRegsKnown <= predTags.size());

        for (size_t i = 0; i < minRegsKnown; ++i)
        {
            // Only registers that are live in can receive information from the predecessors
            if (in.regs.test(i) || (in.varargSeq && i >= in.varargStart))
            {
                uint8_t currentTag = getTag(i);

                if (firstPredecessor)
                    setTag(i, predTags[i]);
                else if (currentTag != kUnknownTag && currentTag != predTags[i])
                    setTag(i, kUnknownTag);
            }
        }

        firstPredecessor = false;
    }
}

std::optional<uint8_t> tryGetLuauTagForBcType(uint8_t bcType, bool ignoreOptionalPart)
{
    if (ignoreOptionalPart)
        bcType = bcType & ~LBC_TYPE_OPTIONAL_BIT;

    switch (bcType)
    {
    case LBC_TYPE_NIL:
        return LUA_TNIL;
    case LBC_TYPE_BOOLEAN:
        return LUA_TBOOLEAN;
    case LBC_TYPE_NUMBER:
        return LUA_TNUMBER;
    case LBC_TYPE_INTEGER:
        return LUA_TINTEGER;
    case LBC_TYPE_STRING:
        return LUA_TSTRING;
    case LBC_TYPE_TABLE:
        return LUA_TTABLE;
    case LBC_TYPE_FUNCTION:
        return LUA_TFUNCTION;
    case LBC_TYPE_THREAD:
        return LUA_TTHREAD;
    case LBC_TYPE_USERDATA:
        return LUA_TUSERDATA;
    case LBC_TYPE_VECTOR:
        return LUA_TVECTOR;
    case LBC_TYPE_BUFFER:
        return LUA_TBUFFER;
    default:
        if (bcType >= LBC_TYPE_TAGGED_USERDATA_BASE && bcType < LBC_TYPE_TAGGED_USERDATA_END)
            return LUA_TUSERDATA;

        break;
    }

    return std::nullopt;
}

} // namespace CodeGen
} // namespace Luau
