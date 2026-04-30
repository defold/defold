// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/AssemblyBuilderX64.h"

#include "EmitCommon.h"

#include "lobject.h"
#include "ltm.h"

// MS x64 ABI reminder:
// Arguments: rcx, rdx, r8, r9 ('overlapped' with xmm0-xmm3)
// Return: rax, xmm0
// Nonvolatile: r12-r15, rdi, rsi, rbx, rbp
// SIMD: only xmm6-xmm15 are non-volatile, all ymm upper parts are volatile

// AMD64 ABI reminder:
// Arguments: rdi, rsi, rdx, rcx, r8, r9 (xmm0-xmm7)
// Return: rax, rdx, xmm0, xmm1
// Nonvolatile: r12-r15, rbx, rbp
// SIMD: all volatile

namespace Luau
{
namespace CodeGen
{

enum class IrCondition : uint8_t;
struct IrOp;

namespace X64
{

struct IrRegAllocX64;

inline constexpr uint32_t kFunctionAlignment = 32;

// Data that is very common to access is placed in non-volatile registers
inline constexpr RegisterX64 rState = r15;         // lua_State* L
inline constexpr RegisterX64 rBase = r14;          // StkId base
inline constexpr RegisterX64 rNativeContext = r13; // NativeContext* context
inline constexpr RegisterX64 rConstants = r12;     // TValue* k

inline constexpr unsigned kExtraLocals = 3;            // Number of 8 byte slots available for specialized local variables specified below
inline constexpr unsigned kSpillSlots = 13;            // Number of 8 byte slots available for register allocator to spill data into
inline constexpr unsigned kSpillSlots_NEW = 12;        // TODO: remove with FFlagLuauCodegenNewRegSplit
static_assert((kExtraLocals + kSpillSlots) * 8 % 16 == 0, "locals have to preserve 16 byte alignment");
inline constexpr unsigned kExtraSpillSlots = 64;
static_assert(kExtraSpillSlots * 8 <= LUA_EXECUTION_CALLBACK_STORAGE, "can't use more extra slots than Luau global state provides");

inline constexpr uint8_t kWindowsFirstNonVolXmmReg = 6;

inline constexpr uint8_t kWindowsUsableXmmRegs = 10; // Some xmm regs are non-volatile, we have to balance how many we want to use/preserve
inline constexpr uint8_t kSystemVUsableXmmRegs = 16; // All xmm regs are volatile

inline uint8_t getXmmRegisterCount(ABIX64 abi)
{
    return abi == ABIX64::SystemV ? kSystemVUsableXmmRegs : kWindowsUsableXmmRegs;
}

// Native code is as stackless as the interpreter, so we can place some data on the stack once and have it accessible at any point
// Stack is separated into sections for different data. See CodeGenX64.cpp for layout overview
inline constexpr unsigned kStackAlign = 8; // Bytes we need to align the stack for non-vol xmm register storage
inline constexpr unsigned kStackLocalStorage = 8 * kExtraLocals;
inline constexpr unsigned kStackSpillStorage = 8 * kSpillSlots;
inline constexpr unsigned kStackExtraArgumentStorage = 2 * 8; // Bytes for 5th and 6th function call arguments used under Windows ABI
inline constexpr unsigned kStackRegHomeStorage = 4 * 8;       // Register 'home' locations that can be used by callees under Windows ABI

inline unsigned getNonVolXmmStorageSize(ABIX64 abi, uint8_t xmmRegCount)
{
    if (abi == ABIX64::SystemV)
        return 0;

    // First 6 are volatile
    if (xmmRegCount <= kWindowsFirstNonVolXmmReg)
        return 0;

    CODEGEN_ASSERT(xmmRegCount <= 16);
    return (xmmRegCount - kWindowsFirstNonVolXmmReg) * 16;
}

// Useful offsets to specific parts
inline constexpr unsigned kStackOffsetToLocals = kStackExtraArgumentStorage + kStackRegHomeStorage;
inline constexpr unsigned kStackOffsetToSpillSlots = kStackOffsetToLocals + kStackLocalStorage;

inline unsigned getFullStackSize(ABIX64 abi, uint8_t xmmRegCount)
{
    return kStackOffsetToSpillSlots + kStackSpillStorage + getNonVolXmmStorageSize(abi, xmmRegCount) + kStackAlign;
}

inline constexpr OperandX64 sClosure = qword[rsp + kStackOffsetToLocals + 0]; // Closure* cl
inline constexpr OperandX64 sCode = qword[rsp + kStackOffsetToLocals + 8];    // Instruction* code
inline constexpr OperandX64 sTemporarySlot = addr[rsp + kStackOffsetToLocals + 16];

inline constexpr OperandX64 sSpillArea = addr[rsp + kStackOffsetToSpillSlots];

inline OperandX64 luauReg(int ri)
{
    return xmmword[rBase + ri * sizeof(TValue)];
}

inline OperandX64 luauRegAddress(int ri)
{
    return addr[rBase + ri * sizeof(TValue)];
}

inline OperandX64 luauRegValue(int ri)
{
    return qword[rBase + ri * sizeof(TValue) + offsetof(TValue, value)];
}

inline OperandX64 luauRegTag(int ri)
{
    return dword[rBase + ri * sizeof(TValue) + offsetof(TValue, tt)];
}

inline OperandX64 luauRegExtra(int ri)
{
    return dword[rBase + ri * sizeof(TValue) + offsetof(TValue, extra)];
}

inline OperandX64 luauRegValueInt(int ri)
{
    return dword[rBase + ri * sizeof(TValue) + offsetof(TValue, value)];
}

inline OperandX64 luauRegValueInt64(int ri)
{
    return qword[rBase + ri * sizeof(TValue) + offsetof(TValue, value.l)];
}

inline OperandX64 luauRegValueVector(int ri, int index)
{
    return dword[rBase + ri * sizeof(TValue) + offsetof(TValue, value) + (sizeof(float) * index)];
}

inline OperandX64 luauConstant(int ki)
{
    return xmmword[rConstants + ki * sizeof(TValue)];
}

inline OperandX64 luauConstantAddress(int ki)
{
    return addr[rConstants + ki * sizeof(TValue)];
}

inline OperandX64 luauConstantTag(int ki)
{
    return dword[rConstants + ki * sizeof(TValue) + offsetof(TValue, tt)];
}

inline OperandX64 luauConstantValue(int ki)
{
    return qword[rConstants + ki * sizeof(TValue) + offsetof(TValue, value)];
}

inline OperandX64 luauNodeKeyValue(RegisterX64 node)
{
    return qword[node + offsetof(LuaNode, key) + offsetof(TKey, value)];
}

// Note: tag has dirty upper bits
inline OperandX64 luauNodeKeyTag(RegisterX64 node)
{
    return dword[node + offsetof(LuaNode, key) + kOffsetOfTKeyTagNext];
}

inline void setLuauReg(AssemblyBuilderX64& build, RegisterX64 tmp, int ri, OperandX64 op)
{
    CODEGEN_ASSERT(op.cat == CategoryX64::mem);

    build.vmovups(tmp, op);
    build.vmovups(luauReg(ri), tmp);
}

inline void jumpIfTagIs(AssemblyBuilderX64& build, int ri, lua_Type tag, Label& label)
{
    build.cmp(luauRegTag(ri), tag);
    build.jcc(ConditionX64::Equal, label);
}

inline void jumpIfTagIsNot(AssemblyBuilderX64& build, int ri, lua_Type tag, Label& label)
{
    build.cmp(luauRegTag(ri), tag);
    build.jcc(ConditionX64::NotEqual, label);
}

// Note: fallthrough label should be placed after this condition
inline void jumpIfFalsy(AssemblyBuilderX64& build, int ri, Label& target, Label& fallthrough)
{
    jumpIfTagIs(build, ri, LUA_TNIL, target);             // false if nil
    jumpIfTagIsNot(build, ri, LUA_TBOOLEAN, fallthrough); // true if not nil or boolean

    build.cmp(luauRegValueInt(ri), 0);
    build.jcc(ConditionX64::Equal, target); // true if boolean value is 'true'
}

// Note: fallthrough label should be placed after this condition
inline void jumpIfTruthy(AssemblyBuilderX64& build, int ri, Label& target, Label& fallthrough)
{
    jumpIfTagIs(build, ri, LUA_TNIL, fallthrough);   // false if nil
    jumpIfTagIsNot(build, ri, LUA_TBOOLEAN, target); // true if not nil or boolean

    build.cmp(luauRegValueInt(ri), 0);
    build.jcc(ConditionX64::NotEqual, target); // true if boolean value is 'true'
}

void jumpOnNumberCmp(AssemblyBuilderX64& build, RegisterX64 tmp, OperandX64 lhs, OperandX64 rhs, IrCondition cond, Label& label, bool floatPrecision);

ConditionX64 getConditionInt(IrCondition cond);

void getTableNodeAtCachedSlot(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 node, RegisterX64 table, int pcpos);
void convertNumberToIndexOrJump(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 numd, RegisterX64 numi, Label& label);

void callArithHelper(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, OperandX64 b, OperandX64 c, TMS tm);
void callLengthHelper(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int rb);
void callGetTable(IrRegAllocX64& regs, AssemblyBuilderX64& build, int rb, OperandX64 c, int ra);
void callSetTable(IrRegAllocX64& regs, AssemblyBuilderX64& build, int rb, OperandX64 c, int ra);
void checkObjectBarrierConditions(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 object, RegisterX64 ra, IrOp raOp, int ratag, Label& skip);
void checkObjectBarrierConditions_DEPRECATED(AssemblyBuilderX64& build, RegisterX64 tmp, RegisterX64 object, IrOp ra, int ratag, Label& skip);
void callBarrierObject(IrRegAllocX64& regs, AssemblyBuilderX64& build, RegisterX64 object, IrOp objectOp, RegisterX64 ra, IrOp raOp, int ratag);
void callBarrierObject_DEPRECATED(IrRegAllocX64& regs, AssemblyBuilderX64& build, RegisterX64 object, IrOp objectOp, IrOp ra, int ratag);
void callBarrierTableFast(IrRegAllocX64& regs, AssemblyBuilderX64& build, RegisterX64 table, IrOp tableOp);
void callStepGc(IrRegAllocX64& regs, AssemblyBuilderX64& build);

void emitClearNativeFlag(AssemblyBuilderX64& build);
void emitExit(AssemblyBuilderX64& build, bool continueInVm);
void emitUpdateBase(AssemblyBuilderX64& build);
void emitInterrupt(AssemblyBuilderX64& build);
void emitFallback(IrRegAllocX64& regs, AssemblyBuilderX64& build, int offset, int pcpos);

void emitUpdatePcForExit(AssemblyBuilderX64& build);

void emitReturn(AssemblyBuilderX64& build, ModuleHelpers& helpers);

} // namespace X64
} // namespace CodeGen
} // namespace Luau
