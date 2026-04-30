// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/IrDump.h"

#include "Luau/IrUtils.h"

#include "lua.h"
#include "lobject.h"
#include "lstate.h"

#include <stdarg.h>

LUAU_FASTFLAG(LuauIntegerType)
namespace Luau
{
namespace CodeGen
{

static const char* textForCondition[] =
    {"eq", "not_eq", "lt", "not_lt", "le", "not_le", "gt", "not_gt", "ge", "not_ge", "u_lt", "u_le", "u_gt", "u_ge"};
static_assert(sizeof(textForCondition) / sizeof(textForCondition[0]) == size_t(IrCondition::Count), "all conditions have to be covered");

const int kDetailsAlignColumn = 60;
const unsigned kMaxStringConstantPrintLength = 16;

LUAU_PRINTF_ATTR(2, 3)
static void append(std::string& result, const char* fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    result.append(buf);
}

static void padToDetailColumn(std::string& result, size_t lineStart)
{
    int pad = kDetailsAlignColumn - int(result.size() - lineStart);

    if (pad > 0)
        result.append(pad, ' ');
}

static bool isPrintableStringConstant(const char* str, size_t len)
{
    for (size_t i = 0; i < len; ++i)
    {
        if (unsigned(str[i]) < ' ')
            return false;
    }

    return true;
}

static const char* getTagName(uint8_t tag)
{
    switch (tag)
    {
    case LUA_TNIL:
        return "tnil";
    case LUA_TBOOLEAN:
        return "tboolean";
    case LUA_TLIGHTUSERDATA:
        return "tlightuserdata";
    case LUA_TNUMBER:
        return "tnumber";
    case LUA_TVECTOR:
        return "tvector";
    case LUA_TSTRING:
        return "tstring";
    case LUA_TTABLE:
        return "ttable";
    case LUA_TFUNCTION:
        return "tfunction";
    case LUA_TUSERDATA:
        return "tuserdata";
    case LUA_TTHREAD:
        return "tthread";
    case LUA_TBUFFER:
        return "tbuffer";
    case LUA_TPROTO:
        return "tproto";
    case LUA_TUPVAL:
        return "tupval";
    case LUA_TDEADKEY:
        return "tdeadkey";
    case LUA_TINTEGER:
        if (FFlag::LuauIntegerType)
            return "tinteger";
        [[fallthrough]];
    default:
        CODEGEN_ASSERT(!"Unknown type tag");
        LUAU_UNREACHABLE();
    }
}

const char* getCmdName(IrCmd cmd)
{
    switch (cmd)
    {
    case IrCmd::NOP:
        return "NOP";
    case IrCmd::LOAD_TAG:
        return "LOAD_TAG";
    case IrCmd::LOAD_POINTER:
        return "LOAD_POINTER";
    case IrCmd::LOAD_DOUBLE:
        return "LOAD_DOUBLE";
    case IrCmd::LOAD_INT:
        return "LOAD_INT";
    case IrCmd::LOAD_INT64:
        return "LOAD_INT64";
    case IrCmd::LOAD_FLOAT:
        return "LOAD_FLOAT";
    case IrCmd::LOAD_TVALUE:
        return "LOAD_TVALUE";
    case IrCmd::LOAD_ENV:
        return "LOAD_ENV";
    case IrCmd::GET_ARR_ADDR:
        return "GET_ARR_ADDR";
    case IrCmd::GET_SLOT_NODE_ADDR:
        return "GET_SLOT_NODE_ADDR";
    case IrCmd::GET_HASH_NODE_ADDR:
        return "GET_HASH_NODE_ADDR";
    case IrCmd::GET_CLOSURE_UPVAL_ADDR:
        return "GET_CLOSURE_UPVAL_ADDR";
    case IrCmd::STORE_TAG:
        return "STORE_TAG";
    case IrCmd::STORE_EXTRA:
        return "STORE_EXTRA";
    case IrCmd::STORE_POINTER:
        return "STORE_POINTER";
    case IrCmd::STORE_DOUBLE:
        return "STORE_DOUBLE";
    case IrCmd::STORE_INT:
        return "STORE_INT";
    case IrCmd::STORE_INT64:
        return "STORE_INT64";
    case IrCmd::STORE_VECTOR:
        return "STORE_VECTOR";
    case IrCmd::STORE_TVALUE:
        return "STORE_TVALUE";
    case IrCmd::STORE_SPLIT_TVALUE:
        return "STORE_SPLIT_TVALUE";
    case IrCmd::ADD_INT:
        return "ADD_INT";
    case IrCmd::SUB_INT:
        return "SUB_INT";
    case IrCmd::ADD_INT64:
        return "ADD_INT64";
    case IrCmd::SUB_INT64:
        return "SUB_INT64";
    case IrCmd::MUL_INT64:
        return "MUL_INT64";
    case IrCmd::DIV_INT64:
        return "DIV_INT64";
    case IrCmd::IDIV_INT64:
        return "IDIV_INT64";
    case IrCmd::CHECK_DIV_INT64:
        return "CHECK_DIV_INT64";
    case IrCmd::UDIV_INT64:
        return "UDIV_INT64";
    case IrCmd::REM_INT64:
        return "REM_INT64";
    case IrCmd::UREM_INT64:
        return "UREM_INT64";
    case IrCmd::MOD_INT64:
        return "MOD_INT64";
    case IrCmd::SEXTI8_INT:
        return "SEXTI8_INT";
    case IrCmd::SEXTI16_INT:
        return "SEXTI16_INT";
    case IrCmd::ADD_NUM:
        return "ADD_NUM";
    case IrCmd::SUB_NUM:
        return "SUB_NUM";
    case IrCmd::MUL_NUM:
        return "MUL_NUM";
    case IrCmd::DIV_NUM:
        return "DIV_NUM";
    case IrCmd::IDIV_NUM:
        return "IDIV_NUM";
    case IrCmd::MOD_NUM:
        return "MOD_NUM";
    case IrCmd::MIN_NUM:
        return "MIN_NUM";
    case IrCmd::MAX_NUM:
        return "MAX_NUM";
    case IrCmd::UNM_NUM:
        return "UNM_NUM";
    case IrCmd::FLOOR_NUM:
        return "FLOOR_NUM";
    case IrCmd::CEIL_NUM:
        return "CEIL_NUM";
    case IrCmd::ROUND_NUM:
        return "ROUND_NUM";
    case IrCmd::SQRT_NUM:
        return "SQRT_NUM";
    case IrCmd::ABS_NUM:
        return "ABS_NUM";
    case IrCmd::SIGN_NUM:
        return "SIGN_NUM";
    case IrCmd::ADD_FLOAT:
        return "ADD_FLOAT";
    case IrCmd::SUB_FLOAT:
        return "SUB_FLOAT";
    case IrCmd::MUL_FLOAT:
        return "MUL_FLOAT";
    case IrCmd::DIV_FLOAT:
        return "DIV_FLOAT";
    case IrCmd::MIN_FLOAT:
        return "MIN_FLOAT";
    case IrCmd::MAX_FLOAT:
        return "MAX_FLOAT";
    case IrCmd::UNM_FLOAT:
        return "UNM_FLOAT";
    case IrCmd::FLOOR_FLOAT:
        return "FLOOR_FLOAT";
    case IrCmd::CEIL_FLOAT:
        return "CEIL_FLOAT";
    case IrCmd::SQRT_FLOAT:
        return "SQRT_FLOAT";
    case IrCmd::ABS_FLOAT:
        return "ABS_FLOAT";
    case IrCmd::SIGN_FLOAT:
        return "SIGN_FLOAT";
    case IrCmd::SELECT_NUM:
        return "SELECT_NUM";
    case IrCmd::SELECT_INT64:
        return "SELECT_INT64";
    case IrCmd::MULADD_NUM:
        return "MULADD_NUM";
    case IrCmd::SELECT_VEC:
        return "SELECT_VEC";
    case IrCmd::SELECT_IF_TRUTHY:
        return "SELECT_IF_TRUTHY";
    case IrCmd::ADD_VEC:
        return "ADD_VEC";
    case IrCmd::SUB_VEC:
        return "SUB_VEC";
    case IrCmd::MUL_VEC:
        return "MUL_VEC";
    case IrCmd::DIV_VEC:
        return "DIV_VEC";
    case IrCmd::IDIV_VEC:
        return "IDIV_VEC";
    case IrCmd::MULADD_VEC:
        return "MULADD_VEC";
    case IrCmd::UNM_VEC:
        return "UNM_VEC";
    case IrCmd::MIN_VEC:
        return "MIN_VEC";
    case IrCmd::MAX_VEC:
        return "MAX_VEC";
    case IrCmd::FLOOR_VEC:
        return "FLOOR_VEC";
    case IrCmd::CEIL_VEC:
        return "CEIL_VEC";
    case IrCmd::ABS_VEC:
        return "ABS_VEC";
    case IrCmd::DOT_VEC:
        return "DOT_VEC";
    case IrCmd::EXTRACT_VEC:
        return "EXTRACT_VEC";
    case IrCmd::NOT_ANY:
        return "NOT_ANY";
    case IrCmd::CMP_ANY:
        return "CMP_ANY";
    case IrCmd::CMP_INT:
        return "CMP_INT";
    case IrCmd::CMP_INT64:
        return "CMP_INT64";
    case IrCmd::CMP_TAG:
        return "CMP_TAG";
    case IrCmd::CMP_SPLIT_TVALUE:
        return "CMP_SPLIT_TVALUE";
    case IrCmd::JUMP:
        return "JUMP";
    case IrCmd::JUMP_IF_TRUTHY:
        return "JUMP_IF_TRUTHY";
    case IrCmd::JUMP_IF_FALSY:
        return "JUMP_IF_FALSY";
    case IrCmd::JUMP_EQ_TAG:
        return "JUMP_EQ_TAG";
    case IrCmd::JUMP_CMP_INT:
        return "JUMP_CMP_INT";
    case IrCmd::JUMP_EQ_POINTER:
        return "JUMP_EQ_POINTER";
    case IrCmd::JUMP_CMP_NUM:
        return "JUMP_CMP_NUM";
    case IrCmd::JUMP_CMP_FLOAT:
        return "JUMP_CMP_FLOAT";
    case IrCmd::JUMP_FORN_LOOP_COND:
        return "JUMP_FORN_LOOP_COND";
    case IrCmd::JUMP_SLOT_MATCH:
        return "JUMP_SLOT_MATCH";
    case IrCmd::TABLE_LEN:
        return "TABLE_LEN";
    case IrCmd::TABLE_SETNUM:
        return "TABLE_SETNUM";
    case IrCmd::STRING_LEN:
        return "STRING_LEN";
    case IrCmd::NEW_TABLE:
        return "NEW_TABLE";
    case IrCmd::DUP_TABLE:
        return "DUP_TABLE";
    case IrCmd::TRY_NUM_TO_INDEX:
        return "TRY_NUM_TO_INDEX";
    case IrCmd::TRY_CALL_FASTGETTM:
        return "TRY_CALL_FASTGETTM";
    case IrCmd::NEW_USERDATA:
        return "NEW_USERDATA";
    case IrCmd::INT64_TO_NUM:
        return "INT64_TO_NUM";
    case IrCmd::INT_TO_NUM:
        return "INT_TO_NUM";
    case IrCmd::UINT_TO_NUM:
        return "UINT_TO_NUM";
    case IrCmd::UINT_TO_FLOAT:
        return "UINT_TO_FLOAT";
    case IrCmd::NUM_TO_INT:
        return "NUM_TO_INT";
    case IrCmd::NUM_TO_INT64:
        return "NUM_TO_INT64";
    case IrCmd::NUM_TO_UINT:
        return "NUM_TO_UINT";
    case IrCmd::FLOAT_TO_NUM:
        return "FLOAT_TO_NUM";
    case IrCmd::NUM_TO_FLOAT:
        return "NUM_TO_FLOAT";
    case IrCmd::FLOAT_TO_VEC:
        return "FLOAT_TO_VEC";
    case IrCmd::TAG_VECTOR:
        return "TAG_VECTOR";
    case IrCmd::TRUNCATE_UINT:
        return "TRUNCATE_UINT";
    case IrCmd::ADJUST_STACK_TO_REG:
        return "ADJUST_STACK_TO_REG";
    case IrCmd::ADJUST_STACK_TO_TOP:
        return "ADJUST_STACK_TO_TOP";
    case IrCmd::FASTCALL:
        return "FASTCALL";
    case IrCmd::INVOKE_FASTCALL:
        return "INVOKE_FASTCALL";
    case IrCmd::CHECK_FASTCALL_RES:
        return "CHECK_FASTCALL_RES";
    case IrCmd::DO_ARITH:
        return "DO_ARITH";
    case IrCmd::DO_LEN:
        return "DO_LEN";
    case IrCmd::GET_TABLE:
        return "GET_TABLE";
    case IrCmd::SET_TABLE:
        return "SET_TABLE";
    case IrCmd::GET_CACHED_IMPORT:
        return "GET_CACHED_IMPORT";
    case IrCmd::CONCAT:
        return "CONCAT";
    case IrCmd::GET_UPVALUE:
        return "GET_UPVALUE";
    case IrCmd::SET_UPVALUE:
        return "SET_UPVALUE";
    case IrCmd::CHECK_TAG:
        return "CHECK_TAG";
    case IrCmd::CHECK_TRUTHY:
        return "CHECK_TRUTHY";
    case IrCmd::CHECK_READONLY:
        return "CHECK_READONLY";
    case IrCmd::CHECK_NO_METATABLE:
        return "CHECK_NO_METATABLE";
    case IrCmd::CHECK_SAFE_ENV:
        return "CHECK_SAFE_ENV";
    case IrCmd::CHECK_ARRAY_SIZE:
        return "CHECK_ARRAY_SIZE";
    case IrCmd::CHECK_SLOT_MATCH:
        return "CHECK_SLOT_MATCH";
    case IrCmd::CHECK_NODE_NO_NEXT:
        return "CHECK_NODE_NO_NEXT";
    case IrCmd::CHECK_NODE_VALUE:
        return "CHECK_NODE_VALUE";
    case IrCmd::CHECK_BUFFER_LEN:
        return "CHECK_BUFFER_LEN";
    case IrCmd::CHECK_USERDATA_TAG:
        return "CHECK_USERDATA_TAG";
    case IrCmd::CHECK_CMP_NUM:
        return "CHECK_CMP_NUM";
    case IrCmd::CHECK_CMP_INT:
        return "CHECK_CMP_INT";
    case IrCmd::CHECK_CMP_INT64:
        return "CHECK_CMP_INT64";
    case IrCmd::INTERRUPT:
        return "INTERRUPT";
    case IrCmd::CHECK_GC:
        return "CHECK_GC";
    case IrCmd::BARRIER_OBJ:
        return "BARRIER_OBJ";
    case IrCmd::BARRIER_TABLE_BACK:
        return "BARRIER_TABLE_BACK";
    case IrCmd::BARRIER_TABLE_FORWARD:
        return "BARRIER_TABLE_FORWARD";
    case IrCmd::SET_SAVEDPC:
        return "SET_SAVEDPC";
    case IrCmd::CLOSE_UPVALS:
        return "CLOSE_UPVALS";
    case IrCmd::CAPTURE:
        return "CAPTURE";
    case IrCmd::SETLIST:
        return "SETLIST";
    case IrCmd::CALL:
        return "CALL";
    case IrCmd::RETURN:
        return "RETURN";
    case IrCmd::FORGLOOP:
        return "FORGLOOP";
    case IrCmd::FORGLOOP_FALLBACK:
        return "FORGLOOP_FALLBACK";
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
        return "FORGPREP_XNEXT_FALLBACK";
    case IrCmd::COVERAGE:
        return "COVERAGE";
    case IrCmd::FALLBACK_GETGLOBAL:
        return "FALLBACK_GETGLOBAL";
    case IrCmd::FALLBACK_SETGLOBAL:
        return "FALLBACK_SETGLOBAL";
    case IrCmd::FALLBACK_GETTABLEKS:
        return "FALLBACK_GETTABLEKS";
    case IrCmd::FALLBACK_SETTABLEKS:
        return "FALLBACK_SETTABLEKS";
    case IrCmd::FALLBACK_NAMECALL:
        return "FALLBACK_NAMECALL";
    case IrCmd::FALLBACK_PREPVARARGS:
        return "FALLBACK_PREPVARARGS";
    case IrCmd::FALLBACK_GETVARARGS:
        return "FALLBACK_GETVARARGS";
    case IrCmd::NEWCLOSURE:
        return "NEWCLOSURE";
    case IrCmd::FALLBACK_DUPCLOSURE:
        return "FALLBACK_DUPCLOSURE";
    case IrCmd::FALLBACK_FORGPREP:
        return "FALLBACK_FORGPREP";
    case IrCmd::SUBSTITUTE:
        return "SUBSTITUTE";
    case IrCmd::MARK_USED:
        return "MARK_USED";
    case IrCmd::MARK_DEAD:
        return "MARK_DEAD";
    case IrCmd::BITAND_INT64:
        return "BITAND_INT64";
    case IrCmd::BITXOR_INT64:
        return "BITXOR_INT64";
    case IrCmd::BITOR_INT64:
        return "BITOR_INT64";
    case IrCmd::BITNOT_INT64:
        return "BITNOT_INT64";
    case IrCmd::BITLSHIFT_INT64:
        return "BITLSHIFT_INT64";
    case IrCmd::BITRSHIFT_INT64:
        return "BITRSHIFT_INT64";
    case IrCmd::BITARSHIFT_INT64:
        return "BITARSHIFT_INT64";
    case IrCmd::BITLROTATE_INT64:
        return "BITLROTATE_INT64";
    case IrCmd::BITRROTATE_INT64:
        return "BITRROTATE_INT64";
    case IrCmd::BITCOUNTLZ_INT64:
        return "BITCOUNTLZ_INT64";
    case IrCmd::BITCOUNTRZ_INT64:
        return "BITCOUNTRZ_INT64";
    case IrCmd::BYTESWAP_INT64:
        return "BYTESWAP_INT64";
    case IrCmd::BITAND_UINT:
        return "BITAND_UINT";
    case IrCmd::BITXOR_UINT:
        return "BITXOR_UINT";
    case IrCmd::BITOR_UINT:
        return "BITOR_UINT";
    case IrCmd::BITNOT_UINT:
        return "BITNOT_UINT";
    case IrCmd::BITLSHIFT_UINT:
        return "BITLSHIFT_UINT";
    case IrCmd::BITRSHIFT_UINT:
        return "BITRSHIFT_UINT";
    case IrCmd::BITARSHIFT_UINT:
        return "BITARSHIFT_UINT";
    case IrCmd::BITLROTATE_UINT:
        return "BITLROTATE_UINT";
    case IrCmd::BITRROTATE_UINT:
        return "BITRROTATE_UINT";
    case IrCmd::BITCOUNTLZ_UINT:
        return "BITCOUNTLZ_UINT";
    case IrCmd::BITCOUNTRZ_UINT:
        return "BITCOUNTRZ_UINT";
    case IrCmd::BYTESWAP_UINT:
        return "BYTESWAP_UINT";
    case IrCmd::INVOKE_LIBM:
        return "INVOKE_LIBM";
    case IrCmd::GET_TYPE:
        return "GET_TYPE";
    case IrCmd::GET_TYPEOF:
        return "GET_TYPEOF";
    case IrCmd::FINDUPVAL:
        return "FINDUPVAL";
    case IrCmd::BUFFER_READI8:
        return "BUFFER_READI8";
    case IrCmd::BUFFER_READU8:
        return "BUFFER_READU8";
    case IrCmd::BUFFER_WRITEI8:
        return "BUFFER_WRITEI8";
    case IrCmd::BUFFER_READI16:
        return "BUFFER_READI16";
    case IrCmd::BUFFER_READU16:
        return "BUFFER_READU16";
    case IrCmd::BUFFER_WRITEI16:
        return "BUFFER_WRITEI16";
    case IrCmd::BUFFER_READI32:
        return "BUFFER_READI32";
    case IrCmd::BUFFER_WRITEI32:
        return "BUFFER_WRITEI32";
    case IrCmd::BUFFER_READF32:
        return "BUFFER_READF32";
    case IrCmd::BUFFER_WRITEF32:
        return "BUFFER_WRITEF32";
    case IrCmd::BUFFER_READF64:
        return "BUFFER_READF64";
    case IrCmd::BUFFER_WRITEF64:
        return "BUFFER_WRITEF64";
    }

    LUAU_UNREACHABLE();
}

const char* getBlockKindName(IrBlockKind kind)
{
    switch (kind)
    {
    case IrBlockKind::Bytecode:
        return "bb_bytecode";
    case IrBlockKind::Fallback:
        return "bb_fallback";
    case IrBlockKind::Internal:
        return "bb";
    case IrBlockKind::Linearized:
        return "bb_linear";
    case IrBlockKind::Dead:
        return "dead";
    }

    LUAU_UNREACHABLE();
}

void toString(IrToStringContext& ctx, const IrInst& inst, uint32_t index)
{
    append(ctx.result, "  ");

    // Instructions with a result display target virtual register
    if (hasResult(inst.cmd))
        append(ctx.result, "%%%u = ", index);

    ctx.result.append(getCmdName(inst.cmd));

    for (size_t i = 0; i < inst.ops.size(); i++)
    {
        if (inst.ops[i].kind == IrOpKind::None)
            continue;
        ctx.result.append(i == 0 ? " " : ", ");
        toString(ctx, inst.ops[i]);
    }
}

void toString(IrToStringContext& ctx, const IrBlock& block, uint32_t index)
{
    append(ctx.result, "%s_%u", getBlockKindName(block.kind), index);
}

static void appendVmConstant(std::string& result, Proto* proto, int index)
{
    TValue constant = proto->k[index];

    if (constant.tt == LUA_TNIL)
    {
        append(result, "nil");
    }
    else if (constant.tt == LUA_TBOOLEAN)
    {
        append(result, constant.value.b != 0 ? "true" : "false");
    }
    else if (constant.tt == LUA_TNUMBER)
    {
        if (constant.value.n != constant.value.n)
            append(result, "nan");
        else
            append(result, "%.17g", constant.value.n);
    }
    else if (constant.tt == LUA_TINTEGER)
    {
        append(result, "%lldi", (long long)constant.value.l);
    }
    else if (constant.tt == LUA_TSTRING)
    {
        TString* str = gco2ts(constant.value.gc);
        const char* data = getstr(str);

        if (isPrintableStringConstant(data, str->len))
        {
            if (str->len < kMaxStringConstantPrintLength)
                append(result, "'%.*s'", int(str->len), data);
            else
                append(result, "'%.*s'...", int(kMaxStringConstantPrintLength), data);
        }
    }
    else if (constant.tt == LUA_TVECTOR)
    {
        const float* v = constant.value.v;

#if LUA_VECTOR_SIZE == 4
        if (v[3] != 0)
            append(result, "%.9g, %.9g, %.9g, %.9g", v[0], v[1], v[2], v[3]);
        else
            append(result, "%.9g, %.9g, %.9g", v[0], v[1], v[2]);
#else
        append(result, "%.9g, %.9g, %.9g", v[0], v[1], v[2]);
#endif
    }
}

void toString(IrToStringContext& ctx, IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::None:
        break;
    case IrOpKind::Undef:
        append(ctx.result, "undef");
        break;
    case IrOpKind::Constant:
        toString(ctx.result, ctx.proto, ctx.constants[op.index]);
        break;
    case IrOpKind::Condition:
        CODEGEN_ASSERT(op.index < uint32_t(IrCondition::Count));
        ctx.result.append(textForCondition[op.index]);
        break;
    case IrOpKind::Inst:
        append(ctx.result, "%%%u", op.index);
        break;
    case IrOpKind::Block:
        append(ctx.result, "%s_%u", getBlockKindName(ctx.blocks[op.index].kind), op.index);
        break;
    case IrOpKind::VmReg:
        append(ctx.result, "R%d", vmRegOp(op));
        break;
    case IrOpKind::VmConst:
        append(ctx.result, "K%d", vmConstOp(op));

        if (ctx.proto)
        {
            append(ctx.result, " (");
            appendVmConstant(ctx.result, ctx.proto, vmConstOp(op));
            append(ctx.result, ")");
        }

        break;
    case IrOpKind::VmUpvalue:
        append(ctx.result, "U%d", vmUpvalueOp(op));
        break;
    case IrOpKind::VmExit:
        if (vmExitOp(op) == kVmExitEntryGuardPc)
            append(ctx.result, "exit(entry)");
        else
            append(ctx.result, "exit(%d)", vmExitOp(op));
        break;
    }
}

void toString(std::string& result, Proto* proto, IrConst constant)
{
    switch (constant.kind)
    {
    case IrConstKind::Int:
        append(result, "%di", constant.valueInt);
        break;
    case IrConstKind::Int64:
        append(result, "%lldi", (long long)constant.valueInt64);
        break;
    case IrConstKind::Uint:
        append(result, "%uu", constant.valueUint);
        break;
    case IrConstKind::Double:
        if (constant.valueDouble != constant.valueDouble)
            append(result, "nan");
        else
            append(result, "%.17g", constant.valueDouble);
        break;
    case IrConstKind::Tag:
        result.append(getTagName(constant.valueTag));
        break;
    case IrConstKind::Import:
        append(result, "%uu", constant.valueUint);

        if (proto)
        {
            append(result, " (");

            int count = constant.valueUint >> 30;
            int id0 = count > 0 ? int(constant.valueUint >> 20) & 1023 : -1;
            int id1 = count > 1 ? int(constant.valueUint >> 10) & 1023 : -1;
            int id2 = count > 2 ? int(constant.valueUint) & 1023 : -1;

            if (id0 != -1)
                appendVmConstant(result, proto, id0);

            if (id1 != -1)
            {
                append(result, ".");
                appendVmConstant(result, proto, id1);
            }

            if (id2 != -1)
            {
                append(result, ".");
                appendVmConstant(result, proto, id2);
            }

            append(result, ")");
        }
        break;
    }
}

const char* getBytecodeTypeName(uint8_t type, const char* const* userdataTypes)
{
    // Optional bit should be handled externally
    type = type & ~LBC_TYPE_OPTIONAL_BIT;

    if (type >= LBC_TYPE_TAGGED_USERDATA_BASE && type < LBC_TYPE_TAGGED_USERDATA_END)
    {
        if (userdataTypes)
            return userdataTypes[type - LBC_TYPE_TAGGED_USERDATA_BASE];

        return "userdata";
    }

    switch (type)
    {
    case LBC_TYPE_NIL:
        return "nil";
    case LBC_TYPE_BOOLEAN:
        return "boolean";
    case LBC_TYPE_NUMBER:
        return "number";
    case LBC_TYPE_INTEGER:
        return "integer";
    case LBC_TYPE_STRING:
        return "string";
    case LBC_TYPE_TABLE:
        return "table";
    case LBC_TYPE_FUNCTION:
        return "function";
    case LBC_TYPE_THREAD:
        return "thread";
    case LBC_TYPE_USERDATA:
        return "userdata";
    case LBC_TYPE_VECTOR:
        return "vector";
    case LBC_TYPE_BUFFER:
        return "buffer";
    case LBC_TYPE_ANY:
        return "any";
    }

    CODEGEN_ASSERT(!"Unhandled type in getBytecodeTypeName");
    return nullptr;
}

void toString(std::string& result, const BytecodeTypes& bcTypes, const char* const* userdataTypes)
{
    append(result, "%s%s", getBytecodeTypeName(bcTypes.result, userdataTypes), (bcTypes.result & LBC_TYPE_OPTIONAL_BIT) != 0 ? "?" : "");
    append(result, " <- ");
    append(result, "%s%s", getBytecodeTypeName(bcTypes.a, userdataTypes), (bcTypes.a & LBC_TYPE_OPTIONAL_BIT) != 0 ? "?" : "");
    append(result, ", ");
    append(result, "%s%s", getBytecodeTypeName(bcTypes.b, userdataTypes), (bcTypes.b & LBC_TYPE_OPTIONAL_BIT) != 0 ? "?" : "");

    if (bcTypes.c != LBC_TYPE_ANY)
    {
        append(result, ", ");
        append(result, "%s%s", getBytecodeTypeName(bcTypes.c, userdataTypes), (bcTypes.c & LBC_TYPE_OPTIONAL_BIT) != 0 ? "?" : "");
    }
}

static void appendBlockSet(IrToStringContext& ctx, BlockIteratorWrapper blocks)
{
    bool comma = false;

    for (uint32_t target : blocks)
    {
        if (comma)
            append(ctx.result, ", ");
        comma = true;

        toString(ctx, ctx.blocks[target], target);
    }
}

static void appendRegisterSet(IrToStringContext& ctx, const RegisterSet& rs, const char* separator)
{
    bool comma = false;

    for (size_t i = 0; i < rs.regs.size(); i++)
    {
        if (rs.regs.test(i))
        {
            if (comma)
                ctx.result.append(separator);
            comma = true;

            append(ctx.result, "R%d", int(i));
        }
    }

    if (rs.varargSeq)
    {
        if (comma)
            ctx.result.append(separator);

        append(ctx.result, "R%d...", rs.varargStart);
    }
}

static RegisterSet getJumpTargetExtraLiveIn(IrToStringContext& ctx, const IrBlock& block, uint32_t blockIdx, IrInst& inst)
{
    RegisterSet extraRs;

    if (blockIdx >= ctx.cfg.in.size())
        return extraRs;

    const RegisterSet& defRs = ctx.cfg.in[blockIdx];

    // Find first block argument, for guard instructions (isNonTerminatingJump), that's the first and only one
    CODEGEN_ASSERT(isNonTerminatingJump(inst.cmd));
    IrOp op = OP_A(inst);

    for (size_t i = 1; i < inst.ops.size(); i++)
        if (inst.ops[i].kind == IrOpKind::Block)
        {
            op = inst.ops[i];
            break;
        }

    if (op.kind == IrOpKind::Block && op.index < ctx.cfg.in.size())
    {
        const RegisterSet& inRs = ctx.cfg.in[op.index];

        extraRs.regs = inRs.regs & ~defRs.regs;

        if (inRs.varargSeq)
            requireVariadicSequence(extraRs, defRs, inRs.varargStart);
    }

    return extraRs;
}

void toStringDetailed(IrToStringContext& ctx, const IrBlock& block, uint32_t blockIdx, IrInst& inst, uint32_t instIdx, IncludeUseInfo includeUseInfo)
{
    size_t start = ctx.result.size();

    toString(ctx, inst, instIdx);

    if (includeUseInfo == IncludeUseInfo::Yes)
    {
        padToDetailColumn(ctx.result, start);

        if (inst.useCount == 0 && hasSideEffects(inst.cmd))
        {
            if (isNonTerminatingJump(inst.cmd))
            {
                RegisterSet extraRs = getJumpTargetExtraLiveIn(ctx, block, blockIdx, inst);

                if (extraRs.regs.any() || extraRs.varargSeq)
                {
                    append(ctx.result, "; %%%u, extra in: ", instIdx);
                    appendRegisterSet(ctx, extraRs, ", ");
                    ctx.result.append("\n");
                }
                else
                {
                    append(ctx.result, "; %%%u\n", instIdx);
                }
            }
            else
            {
                append(ctx.result, "; %%%u\n", instIdx);
            }
        }
        else
        {
            append(ctx.result, "; useCount: %d, lastUse: %%%u\n", inst.useCount, inst.lastUse);
        }
    }
    else
    {
        ctx.result.append("\n");
    }
}

void toStringDetailed(
    IrToStringContext& ctx,
    const IrBlock& block,
    uint32_t blockIdx,
    IncludeUseInfo includeUseInfo,
    IncludeCfgInfo includeCfgInfo,
    IncludeRegFlowInfo includeRegFlowInfo
)
{
    // Report captured registers for entry block
    if (includeRegFlowInfo == IncludeRegFlowInfo::Yes && isEntryBlock(block) && ctx.cfg.captured.regs.any())
    {
        append(ctx.result, "; captured regs: ");
        appendRegisterSet(ctx, ctx.cfg.captured, ", ");
        append(ctx.result, "\n\n");
    }

    size_t start = ctx.result.size();

    toString(ctx, block, blockIdx);
    append(ctx.result, ":");

    if (includeUseInfo == IncludeUseInfo::Yes)
    {
        padToDetailColumn(ctx.result, start);

        append(ctx.result, "; useCount: %d\n", block.useCount);
    }
    else
    {
        ctx.result.append("\n");
    }

    // Predecessor list
    if (includeCfgInfo == IncludeCfgInfo::Yes && blockIdx < ctx.cfg.predecessorsOffsets.size())
    {
        BlockIteratorWrapper pred = predecessors(ctx.cfg, blockIdx);

        if (!pred.empty())
        {
            append(ctx.result, "; predecessors: ");

            appendBlockSet(ctx, pred);
            append(ctx.result, "\n");
        }
    }

    // Successor list
    if (includeCfgInfo == IncludeCfgInfo::Yes && blockIdx < ctx.cfg.successorsOffsets.size())
    {
        BlockIteratorWrapper succ = successors(ctx.cfg, blockIdx);

        if (!succ.empty())
        {
            append(ctx.result, "; successors: ");

            appendBlockSet(ctx, succ);
            append(ctx.result, "\n");
        }
    }

    // Live-in VM regs
    if (includeRegFlowInfo == IncludeRegFlowInfo::Yes && blockIdx < ctx.cfg.in.size())
    {
        const RegisterSet& in = ctx.cfg.in[blockIdx];

        if (in.regs.any() || in.varargSeq)
        {
            append(ctx.result, "; in regs: ");
            appendRegisterSet(ctx, in, ", ");
            append(ctx.result, "\n");
        }
    }

    // Live-out VM regs
    if (includeRegFlowInfo == IncludeRegFlowInfo::Yes && blockIdx < ctx.cfg.out.size())
    {
        const RegisterSet& out = ctx.cfg.out[blockIdx];

        if (out.regs.any() || out.varargSeq)
        {
            append(ctx.result, "; out regs: ");
            appendRegisterSet(ctx, out, ", ");
            append(ctx.result, "\n");
        }
    }
}

std::string toString(IrFunction& function, IncludeUseInfo includeUseInfo)
{
    std::string result;
    IrToStringContext ctx{result, function.blocks, function.constants, function.cfg, function.proto};

    for (size_t i = 0; i < function.blocks.size(); i++)
    {
        const IrBlock& block = function.blocks[i];

        if (block.kind == IrBlockKind::Dead)
            continue;

        toStringDetailed(ctx, block, uint32_t(i), includeUseInfo, IncludeCfgInfo::Yes, IncludeRegFlowInfo::Yes);

        if (block.start == ~0u)
        {
            append(ctx.result, " *empty*\n\n");
            continue;
        }

        if ((block.flags & kBlockFlagSafeEnvCheck) != 0)
            append(ctx.result, "   implicit CHECK_SAFE_ENV exit(%u)\n", block.startpc);

        // To allow dumping blocks that are still being constructed, we can't rely on terminator and need a bounds check
        for (uint32_t index = block.start; index <= block.finish && index < uint32_t(function.instructions.size()); index++)
        {
            IrInst& inst = function.instructions[index];

            // Skip pseudo instructions unless they are still referenced
            if (isPseudo(inst.cmd) && inst.useCount == 0)
                continue;

            append(ctx.result, " ");
            toStringDetailed(ctx, block, uint32_t(i), inst, index, includeUseInfo);
        }

        if (block.expectedNextBlock != ~0u)
        {
            append(ctx.result, "; glued to: ");
            toString(ctx, ctx.blocks[block.expectedNextBlock], block.expectedNextBlock);
            append(ctx.result, "\n");
        }

        append(ctx.result, "\n");
    }

    return result;
}

std::string dump(IrFunction& function)
{
    std::string result = toString(function, IncludeUseInfo::Yes);

    printf("%s\n", result.c_str());

    return result;
}

static void appendLabelRegset(IrToStringContext& ctx, const std::vector<RegisterSet>& regSets, size_t blockIdx, const char* name)
{
    if (blockIdx < regSets.size())
    {
        const RegisterSet& rs = regSets[blockIdx];

        if (rs.regs.any() || rs.varargSeq)
        {
            append(ctx.result, "|{%s|", name);
            appendRegisterSet(ctx, rs, "|");
            append(ctx.result, "}");
        }
    }
}

static void appendBlocks(IrToStringContext& ctx, const IrFunction& function, bool includeInst, bool includeIn, bool includeOut, bool includeDef)
{
    for (size_t i = 0; i < function.blocks.size(); i++)
    {
        const IrBlock& block = function.blocks[i];

        append(ctx.result, "b%u [", unsigned(i));

        if (block.kind == IrBlockKind::Fallback)
            append(ctx.result, "style=filled;fillcolor=salmon;");
        else if (block.kind == IrBlockKind::Bytecode)
            append(ctx.result, "style=filled;fillcolor=palegreen;");

        append(ctx.result, "label=\"{");
        toString(ctx, block, uint32_t(i));

        if (includeIn)
            appendLabelRegset(ctx, ctx.cfg.in, i, "in");

        if (includeInst && block.start != ~0u)
        {
            for (uint32_t instIdx = block.start; instIdx <= block.finish; instIdx++)
            {
                const IrInst& inst = function.instructions[instIdx];

                // Skip pseudo instructions unless they are still referenced
                if (isPseudo(inst.cmd) && inst.useCount == 0)
                    continue;

                append(ctx.result, "|");
                toString(ctx, inst, instIdx);
            }
        }

        if (includeDef)
            appendLabelRegset(ctx, ctx.cfg.def, i, "def");

        if (includeOut)
            appendLabelRegset(ctx, ctx.cfg.out, i, "out");

        append(ctx.result, "}\"];\n");
    }
}

std::string toDot(const IrFunction& function, bool includeInst)
{
    std::string result;
    IrToStringContext ctx{result, function.blocks, function.constants, function.cfg, function.proto};

    append(ctx.result, "digraph CFG {\n");
    append(ctx.result, "node[shape=record]\n");

    appendBlocks(ctx, function, includeInst, /* includeIn */ true, /* includeOut */ true, /* includeDef */ true);

    for (size_t i = 0; i < function.blocks.size(); i++)
    {
        const IrBlock& block = function.blocks[i];

        if (block.start == ~0u)
            continue;

        for (uint32_t instIdx = block.start; instIdx != ~0u && instIdx <= block.finish; instIdx++)
        {
            const IrInst& inst = function.instructions[instIdx];

            auto checkOp = [&](IrOp op)
            {
                if (op.kind == IrOpKind::Block)
                {
                    if (function.blocks[op.index].kind != IrBlockKind::Fallback)
                        append(ctx.result, "b%u -> b%u [weight=10];\n", unsigned(i), op.index);
                    else
                        append(ctx.result, "b%u -> b%u;\n", unsigned(i), op.index);
                }
            };

            for (auto& op : inst.ops)
                checkOp(op);
        }
    }

    append(ctx.result, "}\n");

    return result;
}

std::string toDotCfg(const IrFunction& function)
{
    std::string result;
    IrToStringContext ctx{result, function.blocks, function.constants, function.cfg, function.proto};

    append(ctx.result, "digraph CFG {\n");
    append(ctx.result, "node[shape=record]\n");

    appendBlocks(ctx, function, /* includeInst */ false, /* includeIn */ false, /* includeOut */ false, /* includeDef */ true);

    for (size_t i = 0; i < function.blocks.size() && i < ctx.cfg.successorsOffsets.size(); i++)
    {
        BlockIteratorWrapper succ = successors(ctx.cfg, unsigned(i));

        for (uint32_t target : succ)
            append(ctx.result, "b%u -> b%u;\n", unsigned(i), target);
    }

    append(ctx.result, "}\n");

    return result;
}

std::string toDotDjGraph(const IrFunction& function)
{
    std::string result;
    IrToStringContext ctx{result, function.blocks, function.constants, function.cfg, function.proto};

    append(ctx.result, "digraph CFG {\n");

    for (size_t i = 0; i < ctx.blocks.size(); i++)
    {
        const IrBlock& block = ctx.blocks[i];

        append(ctx.result, "b%u [", unsigned(i));

        if (block.kind == IrBlockKind::Fallback)
            append(ctx.result, "style=filled;fillcolor=salmon;");
        else if (block.kind == IrBlockKind::Bytecode)
            append(ctx.result, "style=filled;fillcolor=palegreen;");

        append(ctx.result, "label=\"");
        toString(ctx, block, uint32_t(i));
        append(ctx.result, "\"];\n");
    }

    // Layer by depth in tree
    uint32_t depth = 0;
    bool found = true;

    while (found)
    {
        found = false;

        append(ctx.result, "{rank = same;");
        for (size_t i = 0; i < ctx.cfg.domOrdering.size(); i++)
        {
            if (ctx.cfg.domOrdering[i].depth == depth)
            {
                append(ctx.result, "b%u;", unsigned(i));
                found = true;
            }
        }
        append(ctx.result, "}\n");

        depth++;
    }

    for (size_t i = 0; i < ctx.cfg.domChildrenOffsets.size(); i++)
    {
        BlockIteratorWrapper dom = domChildren(ctx.cfg, unsigned(i));

        for (uint32_t target : dom)
            append(ctx.result, "b%u -> b%u;\n", unsigned(i), target);

        // Join edges are all successor edges that do not strongly dominate
        BlockIteratorWrapper succ = successors(ctx.cfg, unsigned(i));

        for (uint32_t successor : succ)
        {
            bool found = false;

            for (uint32_t target : dom)
            {
                if (target == successor)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                append(ctx.result, "b%u -> b%u [style=dotted];\n", unsigned(i), successor);
        }
    }

    append(ctx.result, "}\n");

    return result;
}

std::string dumpDot(const IrFunction& function, bool includeInst)
{
    std::string result = toDot(function, includeInst);

    printf("%s\n", result.c_str());

    return result;
}

} // namespace CodeGen
} // namespace Luau
