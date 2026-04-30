// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrLoweringX64.h"

#include "Luau/CodeGenOptions.h"
#include "Luau/DenseHash.h"
#include "Luau/IrCallWrapperX64.h"
#include "Luau/IrData.h"
#include "Luau/IrUtils.h"
#include "Luau/LoweringStats.h"

#include "EmitBuiltinsX64.h"
#include "EmitCommonX64.h"
#include "EmitInstructionX64.h"
#include "NativeState.h"

#include "lstate.h"
#include "lgc.h"

LUAU_FASTFLAG(LuauCodegenBufferRangeMerge4)
LUAU_FASTFLAG(LuauCodegenBufNoDefTag)
LUAU_FASTFLAG(LuauCodegenCallWrapImproved)
LUAU_FASTFLAG(LuauCodegenNewRegSplit)

namespace Luau
{
namespace CodeGen
{
namespace X64
{

IrLoweringX64::IrLoweringX64(AssemblyBuilderX64& build, ModuleHelpers& helpers, IrFunction& function, LoweringStats* stats)
    : build(build)
    , helpers(helpers)
    , function(function)
    , stats(stats)
    , regs(build, function, stats)
    , valueTracker(function)
    , exitHandlerMap(~0u)
{
    valueTracker.setRestoreCallback(
        &regs,
        [](void* context, IrInst& inst)
        {
            ((IrRegAllocX64*)context)->restore(inst, false);
        }
    );

    build.align(kFunctionAlignment, X64::AlignmentDataX64::Ud2);
}

void IrLoweringX64::lowerInst(IrInst& inst, uint32_t index, const IrBlock& next)
{
    regs.currInstIdx = index;

    valueTracker.beforeInstLowering(inst);

    switch (inst.cmd)
    {
    case IrCmd::LOAD_TAG:
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.mov(inst.regX64, luauRegTag(vmRegOp(OP_A(inst))));
        else if (OP_A(inst).kind == IrOpKind::VmConst)
            build.mov(inst.regX64, luauConstantTag(vmConstOp(OP_A(inst))));
        // If we have a register, we assume it's a pointer to TValue
        // We might introduce explicit operand types in the future to make this more robust
        else if (OP_A(inst).kind == IrOpKind::Inst)
            build.mov(inst.regX64, dword[regOp(OP_A(inst)) + offsetof(TValue, tt)]);
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::LOAD_POINTER:
        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.mov(inst.regX64, luauRegValue(vmRegOp(OP_A(inst))));
        else if (OP_A(inst).kind == IrOpKind::VmConst)
            build.mov(inst.regX64, luauConstantValue(vmConstOp(OP_A(inst))));
        // If we have a register, we assume it's a pointer to TValue
        // We might introduce explicit operand types in the future to make this more robust
        else if (OP_A(inst).kind == IrOpKind::Inst)
            build.mov(inst.regX64, qword[regOp(OP_A(inst)) + offsetof(TValue, value)]);
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::LOAD_DOUBLE:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.vmovsd(inst.regX64, luauRegValue(vmRegOp(OP_A(inst))));
        else if (OP_A(inst).kind == IrOpKind::VmConst)
            build.vmovsd(inst.regX64, luauConstantValue(vmConstOp(OP_A(inst))));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::LOAD_INT:
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        build.mov(inst.regX64, luauRegValueInt(vmRegOp(OP_A(inst))));
        break;
    case IrCmd::LOAD_INT64:
        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.mov(inst.regX64, luauRegValueInt64(vmRegOp(OP_A(inst))));
        else if (OP_A(inst).kind == IrOpKind::VmConst)
            build.mov(inst.regX64, luauConstantValue(vmConstOp(OP_A(inst))));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::LOAD_FLOAT:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.vmovss(inst.regX64, dword[rBase + vmRegOp(OP_A(inst)) * sizeof(TValue) + offsetof(TValue, value) + intOp(OP_B(inst))]);
        else if (OP_A(inst).kind == IrOpKind::VmConst)
            build.vmovss(inst.regX64, dword[rConstants + vmConstOp(OP_A(inst)) * sizeof(TValue) + offsetof(TValue, value) + intOp(OP_B(inst))]);
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::LOAD_TVALUE:
    {
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        int addrOffset = HAS_OP_B(inst) ? intOp(OP_B(inst)) : 0;

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.vmovups(inst.regX64, luauReg(vmRegOp(OP_A(inst))));
        else if (OP_A(inst).kind == IrOpKind::VmConst)
            build.vmovups(inst.regX64, luauConstant(vmConstOp(OP_A(inst))));
        else if (OP_A(inst).kind == IrOpKind::Inst)
            build.vmovups(inst.regX64, xmmword[regOp(OP_A(inst)) + addrOffset]);
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    }
    case IrCmd::LOAD_ENV:
        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        build.mov(inst.regX64, sClosure);
        build.mov(inst.regX64, qword[inst.regX64 + offsetof(Closure, env)]);
        break;
    case IrCmd::GET_ARR_ADDR:
        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_B(inst)});

            if (dwordReg(inst.regX64) != regOp(OP_B(inst)))
                build.mov(dwordReg(inst.regX64), regOp(OP_B(inst)));

            build.shl(dwordReg(inst.regX64), kTValueSizeLog2);
            build.add(inst.regX64, qword[regOp(OP_A(inst)) + offsetof(LuaTable, array)]);
        }
        else if (OP_B(inst).kind == IrOpKind::Constant)
        {
            inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

            build.mov(inst.regX64, qword[regOp(OP_A(inst)) + offsetof(LuaTable, array)]);

            if (intOp(OP_B(inst)) != 0)
                build.lea(inst.regX64, addr[inst.regX64 + intOp(OP_B(inst)) * sizeof(TValue)]);
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::GET_SLOT_NODE_ADDR:
    {
        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        ScopedRegX64 tmp{regs, SizeX64::qword};

        getTableNodeAtCachedSlot(build, tmp.reg, inst.regX64, regOp(OP_A(inst)), uintOp(OP_B(inst)));
        break;
    }
    case IrCmd::GET_HASH_NODE_ADDR:
    {
        // Custom bit shift value can only be placed in cl
        ScopedRegX64 shiftTmp{regs, regs.takeReg(rcx, kInvalidInstIdx)};

        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        ScopedRegX64 tmp{regs, SizeX64::qword};

        build.mov(inst.regX64, qword[regOp(OP_A(inst)) + offsetof(LuaTable, node)]);
        build.mov(dwordReg(tmp.reg), 1);
        build.mov(byteReg(shiftTmp.reg), byte[regOp(OP_A(inst)) + offsetof(LuaTable, lsizenode)]);
        build.shl(dwordReg(tmp.reg), byteReg(shiftTmp.reg));
        build.dec(dwordReg(tmp.reg));
        build.and_(dwordReg(tmp.reg), uintOp(OP_B(inst)));
        build.shl(tmp.reg, kLuaNodeSizeLog2);
        build.add(inst.regX64, tmp.reg);
        break;
    };
    case IrCmd::GET_CLOSURE_UPVAL_ADDR:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind == IrOpKind::Undef)
        {
            build.mov(inst.regX64, sClosure);
        }
        else
        {
            RegisterX64 cl = regOp(OP_A(inst));
            if (inst.regX64 != cl)
                build.mov(inst.regX64, cl);
        }

        build.add(inst.regX64, offsetof(Closure, l.uprefs) + sizeof(TValue) * vmUpvalueOp(OP_B(inst)));
        break;
    }
    case IrCmd::STORE_TAG:
        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
                build.mov(dword[regOp(OP_A(inst)) + offsetof(TValue, tt)], tagOp(OP_B(inst)));
            else
                build.mov(luauRegTag(vmRegOp(OP_A(inst))), tagOp(OP_B(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::STORE_POINTER:
    {
        OperandX64 valueLhs =
            OP_A(inst).kind == IrOpKind::Inst ? qword[regOp(OP_A(inst)) + offsetof(TValue, value)] : luauRegValue(vmRegOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            CODEGEN_ASSERT(intOp(OP_B(inst)) == 0);
            build.mov(valueLhs, 0);
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.mov(valueLhs, regOp(OP_B(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::STORE_EXTRA:
        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
                build.mov(dword[regOp(OP_A(inst)) + offsetof(TValue, extra)], intOp(OP_B(inst)));
            else
                build.mov(luauRegExtra(vmRegOp(OP_A(inst))), intOp(OP_B(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::STORE_DOUBLE:
    {
        OperandX64 valueLhs =
            OP_A(inst).kind == IrOpKind::Inst ? qword[regOp(OP_A(inst)) + offsetof(TValue, value)] : luauRegValue(vmRegOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, build.f64(doubleOp(OP_B(inst))));
            build.vmovsd(valueLhs, tmp.reg);
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.vmovsd(valueLhs, regOp(OP_B(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::STORE_INT:
        if (OP_B(inst).kind == IrOpKind::Constant)
            build.mov(luauRegValueInt(vmRegOp(OP_A(inst))), intOp(OP_B(inst)));
        else if (OP_B(inst).kind == IrOpKind::Inst)
            build.mov(luauRegValueInt(vmRegOp(OP_A(inst))), regOp(OP_B(inst)));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::STORE_INT64:
        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t value = int64Op(OP_B(inst));

            // x64 mov r/m64, imm32 sign-extends
            // otherwise we use register for values outside that range
            if (value >= INT32_MIN && value <= INT32_MAX)
            {
                build.mov(luauRegValueInt64(vmRegOp(OP_A(inst))), int32_t(value));
            }
            else
            {
                ScopedRegX64 tmp{regs, SizeX64::qword};
                build.mov64(tmp.reg, value);
                build.mov(luauRegValueInt64(vmRegOp(OP_A(inst))), tmp.reg);
            }
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
            build.mov(luauRegValueInt64(vmRegOp(OP_A(inst))), regOp(OP_B(inst)));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    case IrCmd::STORE_VECTOR:
        storeFloat(luauRegValueVector(vmRegOp(OP_A(inst)), 0), OP_B(inst));
        storeFloat(luauRegValueVector(vmRegOp(OP_A(inst)), 1), OP_C(inst));
        storeFloat(luauRegValueVector(vmRegOp(OP_A(inst)), 2), OP_D(inst));

        if (HAS_OP_E(inst))
            build.mov(luauRegTag(vmRegOp(OP_A(inst))), tagOp(OP_E(inst)));
        break;
    case IrCmd::STORE_TVALUE:
    {
        int addrOffset = HAS_OP_C(inst) ? intOp(OP_C(inst)) : 0;

        if (OP_A(inst).kind == IrOpKind::VmReg)
            build.vmovups(luauReg(vmRegOp(OP_A(inst))), regOp(OP_B(inst)));
        else if (OP_A(inst).kind == IrOpKind::Inst)
            build.vmovups(xmmword[regOp(OP_A(inst)) + addrOffset], regOp(OP_B(inst)));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    }
    case IrCmd::STORE_SPLIT_TVALUE:
    {
        int addrOffset = HAS_OP_D(inst) ? intOp(OP_D(inst)) : 0;

        OperandX64 tagLhs =
            OP_A(inst).kind == IrOpKind::Inst ? dword[regOp(OP_A(inst)) + offsetof(TValue, tt) + addrOffset] : luauRegTag(vmRegOp(OP_A(inst)));
        build.mov(tagLhs, tagOp(OP_B(inst)));

        if (tagOp(OP_B(inst)) == LUA_TBOOLEAN)
        {
            OperandX64 valueLhs = OP_A(inst).kind == IrOpKind::Inst ? dword[regOp(OP_A(inst)) + offsetof(TValue, value) + addrOffset]
                                                                    : luauRegValueInt(vmRegOp(OP_A(inst)));
            build.mov(valueLhs, OP_C(inst).kind == IrOpKind::Constant ? OperandX64(intOp(OP_C(inst))) : regOp(OP_C(inst)));
        }
        else if (tagOp(OP_B(inst)) == LUA_TNUMBER)
        {
            OperandX64 valueLhs = OP_A(inst).kind == IrOpKind::Inst ? qword[regOp(OP_A(inst)) + offsetof(TValue, value) + addrOffset]
                                                                    : luauRegValue(vmRegOp(OP_A(inst)));

            if (OP_C(inst).kind == IrOpKind::Constant)
            {
                ScopedRegX64 tmp{regs, SizeX64::xmmword};

                build.vmovsd(tmp.reg, build.f64(doubleOp(OP_C(inst))));
                build.vmovsd(valueLhs, tmp.reg);
            }
            else
            {
                build.vmovsd(valueLhs, regOp(OP_C(inst)));
            }
        }
        else if (tagOp(OP_B(inst)) == LUA_TINTEGER)
        {
            OperandX64 valueLhs = OP_A(inst).kind == IrOpKind::Inst ? qword[regOp(OP_A(inst)) + offsetof(TValue, value) + addrOffset]
                                                                    : luauRegValueInt64(vmRegOp(OP_A(inst)));

            if (OP_C(inst).kind == IrOpKind::Constant)
            {
                int64_t value = int64Op(OP_C(inst));

                // x64 mov r/m64, imm32 sign-extends
                if (value >= INT32_MIN && value <= INT32_MAX)
                {
                    build.mov(valueLhs, int32_t(value));
                }
                else
                {
                    ScopedRegX64 tmp{regs, SizeX64::qword};
                    build.mov64(tmp.reg, value);
                    build.mov(valueLhs, tmp.reg);
                }
            }
            else
            {
                build.mov(valueLhs, regOp(OP_C(inst)));
            }
        }
        else if (isGCO(tagOp(OP_B(inst))))
        {
            OperandX64 valueLhs = OP_A(inst).kind == IrOpKind::Inst ? qword[regOp(OP_A(inst)) + offsetof(TValue, value) + addrOffset]
                                                                    : luauRegValue(vmRegOp(OP_A(inst)));
            build.mov(valueLhs, regOp(OP_C(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::ADD_INT:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            build.lea(inst.regX64, addr[regOp(OP_B(inst)) + intOp(OP_A(inst))]);
        }
        else if (OP_A(inst).kind == IrOpKind::Inst)
        {
            if (inst.regX64 == regOp(OP_A(inst)))
            {
                if (OP_B(inst).kind == IrOpKind::Inst)
                    build.add(inst.regX64, regOp(OP_B(inst)));
                else if (intOp(OP_B(inst)) == 1)
                    build.inc(inst.regX64);
                else
                    build.add(inst.regX64, intOp(OP_B(inst)));
            }
            else
            {
                if (OP_B(inst).kind == IrOpKind::Inst)
                    build.lea(inst.regX64, addr[regOp(OP_A(inst)) + regOp(OP_B(inst))]);
                else
                    build.lea(inst.regX64, addr[regOp(OP_A(inst)) + intOp(OP_B(inst))]);
            }
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::ADD_INT64:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            int64_t value = int64Op(OP_A(inst));

            if (value >= INT32_MIN && value <= INT32_MAX)
            {
                build.lea(inst.regX64, addr[regOp(OP_B(inst)) + int32_t(value)]);
            }
            else
            {
                build.mov64(inst.regX64, value);
                build.add(inst.regX64, regOp(OP_B(inst)));
            }
        }
        else if (OP_A(inst).kind == IrOpKind::Inst)
        {
            if (inst.regX64 == regOp(OP_A(inst)))
            {
                if (OP_B(inst).kind == IrOpKind::Inst)
                    build.add(inst.regX64, regOp(OP_B(inst)));
                else if (int64Op(OP_B(inst)) == 1)
                    build.inc(inst.regX64);
                else
                {
                    int64_t value = int64Op(OP_B(inst));

                    if (value >= INT32_MIN && value <= INT32_MAX)
                    {
                        build.add(inst.regX64, int32_t(value));
                    }
                    else
                    {
                        ScopedRegX64 tmp{regs, SizeX64::qword};
                        build.mov64(tmp.reg, value);
                        build.add(inst.regX64, tmp.reg);
                    }
                }
            }
            else
            {
                if (OP_B(inst).kind == IrOpKind::Inst)
                {
                    build.lea(inst.regX64, addr[regOp(OP_A(inst)) + regOp(OP_B(inst))]);
                }
                else
                {
                    int64_t value = int64Op(OP_B(inst));

                    if (value >= INT32_MIN && value <= INT32_MAX)
                    {
                        build.lea(inst.regX64, addr[regOp(OP_A(inst)) + int32_t(value)]);
                    }
                    else
                    {
                        build.mov64(inst.regX64, value);
                        build.add(inst.regX64, regOp(OP_A(inst)));
                    }
                }
            }
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }

        break;
    }
    case IrCmd::SUB_INT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind == IrOpKind::Inst)
        {
            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                if (inst.regX64 != regOp(OP_A(inst)))
                    build.lea(inst.regX64, addr[regOp(OP_A(inst)) - intOp(OP_B(inst))]);
                else
                    build.sub(inst.regX64, intOp(OP_B(inst)));
            }
            else
            {
                // If result reuses the source, we can subtract in place, otherwise we need to setup our initial value
                if (inst.regX64 != regOp(OP_A(inst)))
                    build.mov(inst.regX64, regOp(OP_A(inst)));

                build.sub(inst.regX64, regOp(OP_B(inst)));
            }
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.mov(inst.regX64, intOp(OP_A(inst)));
            build.sub(inst.regX64, regOp(OP_B(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::SUB_INT64:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind == IrOpKind::Inst)
        {
            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                int64_t value = int64Op(OP_B(inst));

                if (value >= INT32_MIN && value <= INT32_MAX)
                {
                    if (inst.regX64 != regOp(OP_A(inst)))
                        build.lea(inst.regX64, addr[regOp(OP_A(inst)) - int32_t(value)]);
                    else
                        build.sub(inst.regX64, int32_t(value));
                }
                else
                {
                    ScopedRegX64 tmp{regs, SizeX64::qword};
                    build.mov64(tmp.reg, value);

                    if (inst.regX64 != regOp(OP_A(inst)))
                        build.mov(inst.regX64, regOp(OP_A(inst)));

                    build.sub(inst.regX64, tmp.reg);
                }
            }
            else
            {
                // If result reuses the source, we can subtract in place, otherwise we need to setup our initial value
                if (inst.regX64 != regOp(OP_A(inst)))
                    build.mov(inst.regX64, regOp(OP_A(inst)));

                build.sub(inst.regX64, regOp(OP_B(inst)));
            }
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.mov64(inst.regX64, int64Op(OP_A(inst)));
            build.sub(inst.regX64, regOp(OP_B(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::SEXTI8_INT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        build.movsx(inst.regX64, byteReg(regOp(OP_A(inst))));
        break;
    case IrCmd::SEXTI16_INT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        build.movsx(inst.regX64, wordReg(regOp(OP_A(inst))));
        break;
    case IrCmd::ADD_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vaddsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vaddsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        break;
    case IrCmd::SUB_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vsubsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vsubsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        break;
    case IrCmd::MUL_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vmulsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vmulsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        break;
    case IrCmd::MUL_INT64:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));
        build.imul(inst.regX64, memRegInt64Op(OP_B(inst)));
        break;
    case IrCmd::DIV_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vdivsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vdivsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        break;
    case IrCmd::DIV_INT64:
    {
        // idiv clobbers rax (quotient) and rdx (remainder)
        ScopedRegX64 divRax{regs};
        ScopedRegX64 divRdx{regs};
        divRax.take(rax);
        divRdx.take(rdx);

        build.mov(rax, memRegInt64Op(OP_A(inst)));
        build.cqo(); // sign-extend RAX into RDX:RAX
        build.idiv(memRegInt64Op(OP_B(inst)));

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst), OP_B(inst)});
        build.mov(inst.regX64, rax);
        break;
    }
    case IrCmd::IDIV_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vdivsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vdivsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        build.vroundsd(inst.regX64, inst.regX64, inst.regX64, RoundingModeX64::RoundToNegativeInfinity);
        break;
    case IrCmd::IDIV_INT64:
    {
        // idiv clobbers rax (quotient) and rdx (remainder)
        ScopedRegX64 divRax{regs};
        divRax.take(rax);
        ScopedRegX64 divRdx{regs};
        divRdx.take(rdx);
        ScopedRegX64 tempB{regs, SizeX64::qword};

        build.mov(tempB.reg, memRegInt64Op(OP_B(inst)));

        // idiv divides RDX:RAX by operand; quotient in RAX, remainder in RDX
        build.mov(rax, memRegInt64Op(OP_A(inst)));
        build.cqo(); // sign-extend RAX into RDX:RAX
        build.idiv(tempB.reg);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst), OP_B(inst)});
        build.mov(inst.regX64, rax); // start with truncated quotient

        Label done;
        build.test(rdx, rdx);
        build.jcc(ConditionX64::Equal, done); // remainder == 0, no adjustment needed

        build.xor_(rdx, tempB.reg);
        build.jcc(ConditionX64::GreaterEqual, done); // same sign, no adjustment

        build.sub(inst.regX64, 1); // floor adjustment
        build.setLabel(done);

        break;
    }
    case IrCmd::UDIV_INT64:
    {
        // div clobbers rax (quotient) and rdx (remainder)
        ScopedRegX64 divRax{regs};
        ScopedRegX64 divRdx{regs};
        divRax.take(rax);
        divRdx.take(rdx);

        build.mov(rax, memRegInt64Op(OP_A(inst)));
        build.xor_(rdx, rdx); // zero-extend RAX into RDX:RAX
        build.div(memRegInt64Op(OP_B(inst)));

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst), OP_B(inst)});
        build.mov(inst.regX64, rax);
        break;
    }
    case IrCmd::REM_INT64:
    {
        // idiv clobbers rax (quotient) and rdx (remainder)
        ScopedRegX64 divRax{regs};
        ScopedRegX64 divRdx{regs};
        divRax.take(rax);
        divRdx.take(rdx);
        ScopedRegX64 tempB{regs, SizeX64::qword};
        ScopedRegX64 tempA{regs, SizeX64::qword};
        build.mov(tempA.reg, memRegInt64Op(OP_A(inst)));
        build.mov(tempB.reg, memRegInt64Op(OP_B(inst)));

        // guard against dividend == INT64_MIN && divisor == -1 (signed overflow)
        // if that occurs, we must return 0
        Label skip, done;

        build.cmp(tempB.reg, -1);
        build.jcc(ConditionX64::NotEqual, skip);

        ScopedRegX64 tmpMin{regs, SizeX64::qword};
        build.mov(rdx, 0);
        build.mov64(tmpMin.reg, INT64_MIN);
        build.cmp(tempA.reg, tmpMin.reg);
        build.jcc(ConditionX64::Equal, done);

        build.setLabel(skip);

        build.mov(rax, tempA.reg);
        build.cqo(); // sign-extend RAX into RDX:RAX
        build.idiv(tempB.reg);

        build.setLabel(done);
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst), OP_B(inst)});
        build.mov(inst.regX64, rdx);
        break;
    }
    case IrCmd::UREM_INT64:
    {
        // div clobbers rax (quotient) and rdx (remainder)
        ScopedRegX64 divRax{regs};
        ScopedRegX64 divRdx{regs};
        divRax.take(rax);
        divRdx.take(rdx);

        build.mov(rax, memRegInt64Op(OP_A(inst)));
        build.xor_(rdx, rdx); // zero-extend RAX into RDX:RAX
        build.div(memRegInt64Op(OP_B(inst)));

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst), OP_B(inst)});
        build.mov(inst.regX64, rdx);
        break;
    }
    case IrCmd::MULADD_NUM:
    {
        if ((build.features & Feature_FMA3) != 0)
        {
            if (OP_A(inst).kind != IrOpKind::Inst)
            {
                inst.regX64 = regs.allocReg(SizeX64::xmmword, index);
                build.vmovsd(inst.regX64, memRegDoubleOp(OP_A(inst)));
            }
            else
            {
                inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});
                RegisterX64 aReg = regOp(OP_A(inst));
                if (inst.regX64 != aReg)
                    build.vmovupd(inst.regX64, aReg);
            }

            ScopedRegX64 optBTmp{regs};
            RegisterX64 bReg{};

            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                optBTmp.alloc(SizeX64::xmmword);

                build.vmovsd(optBTmp.reg, memRegDoubleOp(OP_B(inst)));
                bReg = optBTmp.reg;
            }
            else
            {
                bReg = regOp(OP_B(inst));
            }

            build.vfmadd213pd(inst.regX64, bReg, memRegDoubleOp(OP_C(inst)));
        }
        else
        {
            inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

            if (OP_A(inst).kind != IrOpKind::Inst && OP_B(inst).kind != IrOpKind::Inst)
            {
                build.vmovsd(inst.regX64, memRegDoubleOp(OP_A(inst)));
                build.vmulsd(inst.regX64, inst.regX64, memRegDoubleOp(OP_B(inst)));
            }
            else if (OP_A(inst).kind == IrOpKind::Inst)
            {
                build.vmulsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
            }
            else
            {
                CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Inst);
                build.vmulsd(inst.regX64, regOp(OP_B(inst)), memRegDoubleOp(OP_A(inst)));
            }

            build.vaddsd(inst.regX64, inst.regX64, memRegDoubleOp(OP_C(inst)));
        }
        break;
    }
    case IrCmd::MOD_NUM:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 optLhsTmp{regs};
        RegisterX64 lhs;

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            optLhsTmp.alloc(SizeX64::xmmword);

            build.vmovsd(optLhsTmp.reg, memRegDoubleOp(OP_A(inst)));
            lhs = optLhsTmp.reg;
        }
        else
        {
            lhs = regOp(OP_A(inst));
        }

        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vdivsd(tmp.reg, lhs, memRegDoubleOp(OP_B(inst)));
            build.vroundsd(tmp.reg, tmp.reg, tmp.reg, RoundingModeX64::RoundToNegativeInfinity);
            build.vmulsd(tmp.reg, tmp.reg, memRegDoubleOp(OP_B(inst)));
            build.vsubsd(inst.regX64, lhs, tmp.reg);
        }
        else
        {
            ScopedRegX64 tmp1{regs, SizeX64::xmmword};
            ScopedRegX64 tmp2{regs, SizeX64::xmmword};

            build.vmovsd(tmp1.reg, memRegDoubleOp(OP_B(inst)));
            build.vdivsd(tmp2.reg, lhs, tmp1.reg);
            build.vroundsd(tmp2.reg, tmp2.reg, tmp2.reg, RoundingModeX64::RoundToNegativeInfinity);
            build.vmulsd(tmp1.reg, tmp2.reg, tmp1.reg);
            build.vsubsd(inst.regX64, lhs, tmp1.reg);
        }
        break;
    }
    case IrCmd::MOD_INT64:
    {
        // idiv clobbers rax (quotient) and rdx (remainder)
        ScopedRegX64 divRax{regs};
        divRax.take(rax);
        ScopedRegX64 divRdx{regs};
        divRdx.take(rdx);
        ScopedRegX64 tempB{regs, SizeX64::qword};
        ScopedRegX64 tempA{regs, SizeX64::qword};
        build.mov(tempA.reg, memRegInt64Op(OP_A(inst)));
        build.mov(tempB.reg, memRegInt64Op(OP_B(inst)));

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst), OP_B(inst)});

        // guard against dividend == INT64_MIN && divisor == -1 (signed overflow)
        // if that occurs, we must return 0
        Label skip, done;

        build.cmp(tempB.reg, -1);
        build.jcc(ConditionX64::NotEqual, skip);

        ScopedRegX64 tmpMin{regs, SizeX64::qword};
        build.mov(inst.regX64, 0);
        build.mov64(tmpMin.reg, INT64_MIN);
        build.cmp(tempA.reg, tmpMin.reg);
        build.jcc(ConditionX64::Equal, done);

        build.setLabel(skip);

        build.mov(rax, tempA.reg);
        build.cqo();
        build.idiv(tempB.reg);

        build.mov(inst.regX64, rdx);

        build.test(rdx, rdx);
        build.jcc(ConditionX64::Equal, done);

        build.xor_(rdx, tempB.reg);
        build.jcc(ConditionX64::GreaterEqual, done);

        build.add(inst.regX64, tempB.reg);
        build.setLabel(done);

        break;
    }
    case IrCmd::MIN_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vminsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vminsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        break;
    case IrCmd::MAX_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovsd(tmp.reg, memRegDoubleOp(OP_A(inst)));
            build.vmaxsd(inst.regX64, tmp.reg, memRegDoubleOp(OP_B(inst)));
        }
        else
        {
            build.vmaxsd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)));
        }
        break;
    case IrCmd::UNM_NUM:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vxorpd(inst.regX64, regOp(OP_A(inst)), build.f64(-0.0));
        break;
    }
    case IrCmd::FLOOR_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vroundsd(inst.regX64, inst.regX64, memRegDoubleOp(OP_A(inst)), RoundingModeX64::RoundToNegativeInfinity);
        break;
    case IrCmd::CEIL_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vroundsd(inst.regX64, inst.regX64, memRegDoubleOp(OP_A(inst)), RoundingModeX64::RoundToPositiveInfinity);
        break;
    case IrCmd::ROUND_NUM:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        ScopedRegX64 tmp1{regs, SizeX64::xmmword};
        ScopedRegX64 tmp2{regs, SizeX64::xmmword};

        if (OP_A(inst).kind != IrOpKind::Inst)
            build.vmovsd(inst.regX64, memRegDoubleOp(OP_A(inst)));
        else if (regOp(OP_A(inst)) != inst.regX64)
            build.vmovsd(inst.regX64, inst.regX64, regOp(OP_A(inst)));

        build.vandpd(tmp1.reg, inst.regX64, build.f64x2(-0.0, -0.0));
        build.vmovsd(tmp2.reg, build.i64(0x3fdfffffffffffff)); // 0.49999999999999994
        build.vorpd(tmp1.reg, tmp1.reg, tmp2.reg);
        build.vaddsd(inst.regX64, inst.regX64, tmp1.reg);
        build.vroundsd(inst.regX64, inst.regX64, inst.regX64, RoundingModeX64::RoundToZero);
        break;
    }
    case IrCmd::SQRT_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vsqrtsd(inst.regX64, inst.regX64, memRegDoubleOp(OP_A(inst)));
        break;
    case IrCmd::ABS_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst)
            build.vmovsd(inst.regX64, memRegDoubleOp(OP_A(inst)));
        else if (regOp(OP_A(inst)) != inst.regX64)
            build.vmovsd(inst.regX64, inst.regX64, regOp(OP_A(inst)));

        build.vandpd(inst.regX64, inst.regX64, build.i64(~(1LL << 63)));
        break;
    case IrCmd::SIGN_NUM:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        ScopedRegX64 tmp0{regs, SizeX64::xmmword};
        ScopedRegX64 tmp1{regs, SizeX64::xmmword};
        ScopedRegX64 tmp2{regs, SizeX64::xmmword};

        build.vxorpd(tmp0.reg, tmp0.reg, tmp0.reg);

        // Set tmp1 to -1 if arg < 0, else 0
        build.vcmpltsd(tmp1.reg, regOp(OP_A(inst)), tmp0.reg);
        build.vmovsd(tmp2.reg, build.f64(-1));
        build.vandpd(tmp1.reg, tmp1.reg, tmp2.reg);

        // Set mask bit to 1 if 0 < arg, else 0
        build.vcmpltsd(inst.regX64, tmp0.reg, regOp(OP_A(inst)));

        // Result = (mask-bit == 1) ? 1.0 : tmp1
        // If arg < 0 then tmp1 is -1 and mask-bit is 0, result is -1
        // If arg == 0 then tmp1 is 0 and mask-bit is 0, result is 0
        // If arg > 0 then tmp1 is 0 and mask-bit is 1, result is 1
        build.vblendvpd(inst.regX64, tmp1.reg, build.f64x2(1, 1), inst.regX64);
        break;
    }
    case IrCmd::ADD_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovss(tmp.reg, memRegFloatOp(OP_A(inst)));
            build.vaddss(inst.regX64, tmp.reg, memRegFloatOp(OP_B(inst)));
        }
        else
        {
            build.vaddss(inst.regX64, regOp(OP_A(inst)), memRegFloatOp(OP_B(inst)));
        }
        break;
    case IrCmd::SUB_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovss(tmp.reg, memRegFloatOp(OP_A(inst)));
            build.vsubss(inst.regX64, tmp.reg, memRegFloatOp(OP_B(inst)));
        }
        else
        {
            build.vsubss(inst.regX64, regOp(OP_A(inst)), memRegFloatOp(OP_B(inst)));
        }
        break;
    case IrCmd::MUL_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovss(tmp.reg, memRegFloatOp(OP_A(inst)));
            build.vmulss(inst.regX64, tmp.reg, memRegFloatOp(OP_B(inst)));
        }
        else
        {
            build.vmulss(inst.regX64, regOp(OP_A(inst)), memRegFloatOp(OP_B(inst)));
        }
        break;
    case IrCmd::DIV_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovss(tmp.reg, memRegFloatOp(OP_A(inst)));
            build.vdivss(inst.regX64, tmp.reg, memRegFloatOp(OP_B(inst)));
        }
        else
        {
            build.vdivss(inst.regX64, regOp(OP_A(inst)), memRegFloatOp(OP_B(inst)));
        }
        break;
    case IrCmd::MIN_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovss(tmp.reg, memRegFloatOp(OP_A(inst)));
            build.vminss(inst.regX64, tmp.reg, memRegFloatOp(OP_B(inst)));
        }
        else
        {
            build.vminss(inst.regX64, regOp(OP_A(inst)), memRegFloatOp(OP_B(inst)));
        }
        break;
    case IrCmd::MAX_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};

            build.vmovss(tmp.reg, memRegFloatOp(OP_A(inst)));
            build.vmaxss(inst.regX64, tmp.reg, memRegFloatOp(OP_B(inst)));
        }
        else
        {
            build.vmaxss(inst.regX64, regOp(OP_A(inst)), memRegFloatOp(OP_B(inst)));
        }
        break;
    case IrCmd::UNM_FLOAT:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vxorps(inst.regX64, regOp(OP_A(inst)), build.f32(-0.0));
        break;
    }
    case IrCmd::FLOOR_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vroundss(inst.regX64, inst.regX64, memRegFloatOp(OP_A(inst)), RoundingModeX64::RoundToNegativeInfinity);
        break;
    case IrCmd::CEIL_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vroundss(inst.regX64, inst.regX64, memRegFloatOp(OP_A(inst)), RoundingModeX64::RoundToPositiveInfinity);
        break;
    case IrCmd::SQRT_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vsqrtss(inst.regX64, inst.regX64, memRegFloatOp(OP_A(inst)));
        break;
    case IrCmd::ABS_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst)
            build.vmovss(inst.regX64, memRegFloatOp(OP_A(inst)));
        else if (regOp(OP_A(inst)) != inst.regX64)
            build.vmovss(inst.regX64, inst.regX64, regOp(OP_A(inst)));

        build.vandps(inst.regX64, inst.regX64, build.i32(0x7fffffff));
        break;
    case IrCmd::SIGN_FLOAT:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        ScopedRegX64 tmp0{regs, SizeX64::xmmword};
        ScopedRegX64 tmp1{regs, SizeX64::xmmword};
        ScopedRegX64 tmp2{regs, SizeX64::xmmword};

        build.vxorps(tmp0.reg, tmp0.reg, tmp0.reg);

        // Set tmp1 to -1 if arg < 0, else 0
        build.vcmpltss(tmp1.reg, regOp(OP_A(inst)), tmp0.reg);
        build.vmovss(tmp2.reg, build.f32(-1.0f));
        build.vandps(tmp1.reg, tmp1.reg, tmp2.reg);

        // Set mask bit to 1 if 0 < arg, else 0
        build.vcmpltss(inst.regX64, tmp0.reg, regOp(OP_A(inst)));

        // Result = (mask-bit == 1) ? 1.0 : tmp1
        // If arg < 0 then tmp1 is -1 and mask-bit is 0, result is -1
        // If arg == 0 then tmp1 is 0 and mask-bit is 0, result is 0
        // If arg > 0 then tmp1 is 0 and mask-bit is 1, result is 1
        build.vblendvps(inst.regX64, tmp1.reg, build.f32x4(1, 1, 1, 1), inst.regX64);
        break;
    }
    case IrCmd::SELECT_NUM:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_C(inst), OP_D(inst)}); // can't reuse b if a is a memory operand

        ScopedRegX64 tmp{regs, SizeX64::xmmword};

        if (OP_C(inst).kind == IrOpKind::Inst)
            build.vcmpeqsd(tmp.reg, regOp(OP_C(inst)), memRegDoubleOp(OP_D(inst)));
        else
        {
            build.vmovsd(tmp.reg, memRegDoubleOp(OP_C(inst)));
            build.vcmpeqsd(tmp.reg, tmp.reg, memRegDoubleOp(OP_D(inst)));
        }

        if (OP_A(inst).kind == IrOpKind::Inst)
            build.vblendvpd(inst.regX64, regOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)), tmp.reg);
        else
        {
            build.vmovsd(inst.regX64, memRegDoubleOp(OP_A(inst)));
            build.vblendvpd(inst.regX64, inst.regX64, memRegDoubleOp(OP_B(inst)), tmp.reg);
        }
        break;
    }
    case IrCmd::SELECT_INT64:
    {
        // Select B if C cond D, otherwise select A
        // A, B: int64 (endpoints), C, D: int64 (condition arguments), E: condition
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        IrCondition cond = conditionOp(OP_E(inst));

        // Start with falseVal (A), conditionally replace with trueVal (B)
        build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        ScopedRegX64 tmp{regs, SizeX64::qword};
        // Compare C vs D
        if (OP_C(inst).kind == IrOpKind::Inst)
            build.cmp(regOp(OP_C(inst)), memRegInt64Op(OP_D(inst)));
        else
        {
            build.mov(tmp.reg, memRegInt64Op(OP_C(inst)));
            build.cmp(tmp.reg, memRegInt64Op(OP_D(inst)));
        }

        // If condition is true, select B instead
        build.cmov(getConditionInt(cond), inst.regX64, memRegInt64Op(OP_B(inst)));

        break;
    }
    case IrCmd::SELECT_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_C(inst), OP_D(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};
        RegisterX64 tmpc = vecOp(OP_C(inst), tmp1);
        RegisterX64 tmpd = vecOp(OP_D(inst), tmp2);

        build.vcmpeqps(inst.regX64, tmpc, tmpd);
        build.vblendvps(inst.regX64, vecOp(OP_A(inst), tmp1), vecOp(OP_B(inst), tmp2), inst.regX64);

        break;
    }
    case IrCmd::SELECT_IF_TRUTHY:
    {
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index); // No reuse since multiple inputs can be shared

        // Place lhs as the result, we will overwrite it with rhs if 'A' is falsy later
        build.vmovaps(inst.regX64, regOp(OP_B(inst)));

        // Get rhs register early, so a potential restore happens on both sides of a conditional control flow
        RegisterX64 c = regOp(OP_C(inst));

        ScopedRegX64 tmp{regs, SizeX64::dword};
        Label saveRhs, exit;

        // Check tag first
        build.vpextrd(tmp.reg, regOp(OP_A(inst)), 3);
        build.cmp(tmp.reg, LUA_TBOOLEAN);

        build.jcc(ConditionX64::Below, saveRhs); // rhs if 'A' is nil
        build.jcc(ConditionX64::Above, exit);    // Keep lhs if 'A' is not a boolean

        // Check the boolean value
        build.vpextrd(tmp.reg, regOp(OP_A(inst)), 0);
        build.test(tmp.reg, tmp.reg);
        build.jcc(ConditionX64::NotZero, exit); // Keep lhs if 'A' is true

        build.setLabel(saveRhs);
        build.vmovaps(inst.regX64, c);

        build.setLabel(exit);
        break;
    }
    case IrCmd::ADD_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vaddps(inst.regX64, tmpa, tmpb);
        break;
    }
    case IrCmd::SUB_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vsubps(inst.regX64, tmpa, tmpb);
        break;
    }
    case IrCmd::MUL_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vmulps(inst.regX64, tmpa, tmpb);
        break;
    }
    case IrCmd::DIV_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vdivps(inst.regX64, tmpa, tmpb);
        break;
    }
    case IrCmd::IDIV_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vdivps(inst.regX64, tmpa, tmpb);
        build.vroundps(inst.regX64, inst.regX64, RoundingModeX64::RoundToNegativeInfinity);
        break;
    }
    case IrCmd::MULADD_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});
        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};
        ScopedRegX64 tmp3{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = vecOp(OP_B(inst), tmp2);
        RegisterX64 tmpc = vecOp(OP_C(inst), tmp3);

        if ((build.features & Feature_FMA3) != 0)
        {
            if (inst.regX64 != tmpa)
                build.vmovups(inst.regX64, tmpa);

            build.vfmadd213ps(inst.regX64, tmpb, tmpc);
        }
        else
        {
            build.vmulps(inst.regX64, tmpa, tmpb);
            build.vaddps(inst.regX64, inst.regX64, tmpc);
        }

        break;
    }
    case IrCmd::UNM_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vxorpd(inst.regX64, regOp(OP_A(inst)), build.f32x4(-0.0, -0.0, -0.0, -0.0));
        break;
    }
    case IrCmd::MIN_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vminps(inst.regX64, tmpa, tmpb);
        break;
    }
    case IrCmd::MAX_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vmaxps(inst.regX64, tmpa, tmpb);
        break;
    }
    case IrCmd::FLOOR_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        ScopedRegX64 tmp1{regs};
        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);

        build.vroundps(inst.regX64, tmpa, RoundingModeX64::RoundToNegativeInfinity);
        break;
    }
    case IrCmd::CEIL_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        ScopedRegX64 tmp1{regs};
        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);

        build.vroundps(inst.regX64, tmpa, RoundingModeX64::RoundToPositiveInfinity);
        break;
    }
    case IrCmd::ABS_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        ScopedRegX64 tmp1{regs};
        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);

        build.vandps(inst.regX64, tmpa, build.u32x4(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
        break;
    }
    case IrCmd::DOT_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst), OP_B(inst)});

        ScopedRegX64 tmp1{regs};
        ScopedRegX64 tmp2{regs};

        RegisterX64 tmpa = vecOp(OP_A(inst), tmp1);
        RegisterX64 tmpb = (OP_A(inst) == OP_B(inst)) ? tmpa : vecOp(OP_B(inst), tmp2);

        build.vdpps(inst.regX64, tmpa, tmpb, 0x71); // 7 = 0b0111, sum first 3 products into first float
        break;
    }
    case IrCmd::EXTRACT_VEC:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vpshufps(inst.regX64, regOp(OP_A(inst)), regOp(OP_A(inst)), intOp(OP_B(inst)));
        break;
    }
    case IrCmd::NOT_ANY:
    {
        // TODO: if we have a single user which is a STORE_INT, we are missing the opportunity to write directly to target
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst), OP_B(inst)});

        Label saveOne, saveZero, exit;

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            // Other cases should've been constant folded
            CODEGEN_ASSERT(tagOp(OP_A(inst)) == LUA_TBOOLEAN);
        }
        else
        {
            build.cmp(regOp(OP_A(inst)), LUA_TNIL);
            build.jcc(ConditionX64::Equal, saveOne);

            build.cmp(regOp(OP_A(inst)), LUA_TBOOLEAN);
            build.jcc(ConditionX64::NotEqual, saveZero);
        }

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            // If value is 1, we fallthrough to storing 0
            if (intOp(OP_B(inst)) == 0)
                build.jmp(saveOne);
        }
        else
        {
            build.cmp(regOp(OP_B(inst)), 0);
            build.jcc(ConditionX64::Equal, saveOne);
        }

        build.setLabel(saveZero);
        build.mov(inst.regX64, 0);
        build.jmp(exit);

        build.setLabel(saveOne);
        build.mov(inst.regX64, 1);

        build.setLabel(exit);
        break;
    }
    case IrCmd::CMP_INT:
    {
        // Cannot reuse operand registers as a target because we have to modify it before the comparison
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        // We are going to operate on byte register, those do not clear high bits on write
        build.xor_(inst.regX64, inst.regX64);

        IrCondition cond = conditionOp(OP_C(inst));

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            build.cmp(regOp(OP_B(inst)), intOp(OP_A(inst)));
            build.setcc(getInverseCondition(getConditionInt(cond)), byteReg(inst.regX64));
        }
        else if (OP_A(inst).kind == IrOpKind::Inst)
        {
            build.cmp(regOp(OP_A(inst)), intOp(OP_B(inst)));
            build.setcc(getConditionInt(cond), byteReg(inst.regX64));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::CMP_ANY:
    {
        CODEGEN_ASSERT(OP_A(inst).kind == IrOpKind::VmReg && OP_B(inst).kind == IrOpKind::VmReg);
        IrCondition cond = conditionOp(OP_C(inst));

        if (FFlag::LuauCodegenCallWrapImproved)
        {
            inst.regX64 = regs.allocReg(SizeX64::dword, index);

            Label skip, exit;

            // For equality comparison, 'luaV_equalval' expects tag to be equal before the call
            if (cond == IrCondition::Equal)
            {
                ScopedRegX64 tmp{regs, SizeX64::dword};

                build.mov(tmp.reg, memRegTagOp(OP_A(inst)));
                build.cmp(memRegTagOp(OP_B(inst)), tmp.reg);

                // If the tags are not equal, skip the call and set result to 0
                build.jcc(ConditionX64::NotEqual, skip);
            }

            {
                ScopedSpills spillGuard(regs);

                IrCallWrapperX64 callWrap(regs, build);
                callWrap.addArgument(SizeX64::qword, rState);
                callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_A(inst))));
                callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_B(inst))));
                callWrap.setResultRegister(inst.regX64, index);

                if (cond == IrCondition::LessEqual)
                    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_lessequal)]);
                else if (cond == IrCondition::Less)
                    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_lessthan)]);
                else if (cond == IrCondition::Equal)
                    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_equalval)]);
                else
                    CODEGEN_ASSERT(!"Unsupported condition");

                emitUpdateBase(build);
            }

            if (cond == IrCondition::Equal)
            {
                build.jmp(exit);
                build.setLabel(skip);

                build.xor_(inst.regX64, inst.regX64);
                build.setLabel(exit);
            }
        }
        else
        {
            Label skip, exit;

            // For equality comparison, 'luaV_lessequal' expects tag to be equal before the call
            if (cond == IrCondition::Equal)
            {
                ScopedRegX64 tmp{regs, SizeX64::dword};

                build.mov(tmp.reg, memRegTagOp(OP_A(inst)));
                build.cmp(memRegTagOp(OP_B(inst)), tmp.reg);

                // If the tags are not equal, skip 'luaV_lessequal' call and set result to 0
                build.jcc(ConditionX64::NotEqual, skip);
            }

            {
                ScopedSpills spillGuard(regs);

                IrCallWrapperX64 callWrap(regs, build);
                callWrap.addArgument(SizeX64::qword, rState);
                callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_A(inst))));
                callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_B(inst))));

                if (cond == IrCondition::LessEqual)
                    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_lessequal)]);
                else if (cond == IrCondition::Less)
                    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_lessthan)]);
                else if (cond == IrCondition::Equal)
                    callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_equalval)]);
                else
                    CODEGEN_ASSERT(!"Unsupported condition");
            }

            emitUpdateBase(build);

            inst.regX64 = regs.takeReg(eax, index);

            if (cond == IrCondition::Equal)
            {
                build.jmp(exit);
                build.setLabel(skip);

                build.xor_(inst.regX64, inst.regX64);
                build.setLabel(exit);
            }
        }

        // If case we made a call, skip high register bits clear, only consumer is JUMP_CMP_INT which doesn't read them
        break;
    }
    case IrCmd::CMP_TAG:
    {
        // Cannot reuse operand registers as a target because we have to modify it before the comparison
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        // We are going to operate on byte register, those do not clear high bits on write
        build.xor_(inst.regX64, inst.regX64);

        IrCondition cond = conditionOp(OP_C(inst));
        CODEGEN_ASSERT(cond == IrCondition::Equal || cond == IrCondition::NotEqual);
        ConditionX64 condX64 = getConditionInt(cond);

        if (tagOp(OP_B(inst)) == LUA_TNIL && OP_A(inst).kind == IrOpKind::Inst)
            build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
        else
            build.cmp(memRegTagOp(OP_A(inst)), tagOp(OP_B(inst)));

        build.setcc(condX64, byteReg(inst.regX64));

        break;
    }
    case IrCmd::CMP_SPLIT_TVALUE:
    {
        // Cannot reuse operand registers as a target because we have to modify it before the comparison
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        // Second operand of this instruction must be a constant
        // Without a constant type, we wouldn't know the correct way to compare the values at lowering time
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);

        // We are going to operate on byte registers, those do not clear high bits on write
        build.xor_(inst.regX64, inst.regX64);

        IrCondition cond = conditionOp(OP_E(inst));
        CODEGEN_ASSERT(cond == IrCondition::Equal || cond == IrCondition::NotEqual);

        // Check tag equality first
        ScopedRegX64 tmp1{regs, SizeX64::byte};

        if (OP_A(inst).kind != IrOpKind::Constant)
        {
            build.cmp(regOp(OP_A(inst)), tagOp(OP_B(inst)));
            build.setcc(getConditionInt(cond), byteReg(tmp1.reg));
        }
        else
        {
            // Constant folding had to handle different constant tags
            CODEGEN_ASSERT(tagOp(OP_A(inst)) == tagOp(OP_B(inst)));
        }

        if (tagOp(OP_B(inst)) == LUA_TBOOLEAN)
        {
            if (OP_C(inst).kind == IrOpKind::Constant)
                build.cmp(regOp(OP_D(inst)), intOp(OP_C(inst))); // swapped arguments
            else if (OP_D(inst).kind == IrOpKind::Constant)
                build.cmp(regOp(OP_C(inst)), intOp(OP_D(inst)));
            else
                build.cmp(regOp(OP_C(inst)), regOp(OP_D(inst)));

            build.setcc(getConditionInt(cond), byteReg(inst.regX64));
        }
        else if (tagOp(OP_B(inst)) == LUA_TSTRING)
        {
            build.cmp(regOp(OP_C(inst)), regOp(OP_D(inst)));
            build.setcc(getConditionInt(cond), byteReg(inst.regX64));
        }
        else if (tagOp(OP_B(inst)) == LUA_TNUMBER)
        {
            if (OP_C(inst).kind == IrOpKind::Constant)
                build.vucomisd(regOp(OP_D(inst)), memRegDoubleOp(OP_C(inst))); // swapped arguments
            else if (OP_D(inst).kind == IrOpKind::Constant)
                build.vucomisd(regOp(OP_C(inst)), memRegDoubleOp(OP_D(inst)));
            else
                build.vucomisd(regOp(OP_C(inst)), regOp(OP_D(inst)));

            if (OP_C(inst) == OP_D(inst))
            {
                // When numbers are the same, we only need to check parity to detect NaN
                if (cond == IrCondition::Equal)
                    build.setcc(ConditionX64::NotParity, byteReg(inst.regX64));
                else
                    build.setcc(ConditionX64::Parity, byteReg(inst.regX64));
            }
            else
            {
                ScopedRegX64 tmp2{regs, SizeX64::dword};

                if (cond == IrCondition::Equal)
                {
                    build.mov(tmp2.reg, 0);
                    build.setcc(ConditionX64::NotParity, byteReg(inst.regX64));
                    build.cmov(ConditionX64::NotEqual, inst.regX64, tmp2.reg);
                }
                else
                {
                    build.mov(tmp2.reg, 1);
                    build.setcc(ConditionX64::Parity, byteReg(inst.regX64));
                    build.cmov(ConditionX64::NotEqual, inst.regX64, tmp2.reg);
                }
            }
        }
        else if (tagOp(OP_B(inst)) == LUA_TINTEGER)
        {
            if (OP_C(inst).kind == IrOpKind::Constant)
                build.cmp(regOp(OP_D(inst)), memRegInt64Op(OP_C(inst))); // swapped arguments
            else if (OP_D(inst).kind == IrOpKind::Constant)
                build.cmp(regOp(OP_C(inst)), memRegInt64Op(OP_D(inst)));
            else
                build.cmp(regOp(OP_C(inst)), regOp(OP_D(inst)));

            build.setcc(getConditionInt(cond), byteReg(inst.regX64));
        }
        else
        {
            CODEGEN_ASSERT(!"unsupported type tag in CMP_SPLIT_TVALUE");
        }

        if (OP_A(inst).kind != IrOpKind::Constant)
        {
            if (cond == IrCondition::Equal)
                build.and_(byteReg(inst.regX64), byteReg(tmp1.reg));
            else
                build.or_(byteReg(inst.regX64), byteReg(tmp1.reg));
        }
        break;
    }
    case IrCmd::JUMP:
        jumpOrAbortOnUndef(OP_A(inst), next);
        break;
    case IrCmd::JUMP_IF_TRUTHY:
        jumpIfTruthy(build, vmRegOp(OP_A(inst)), labelOp(OP_B(inst)), labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    case IrCmd::JUMP_IF_FALSY:
        jumpIfFalsy(build, vmRegOp(OP_A(inst)), labelOp(OP_B(inst)), labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    case IrCmd::JUMP_EQ_TAG:
    {
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Inst || OP_B(inst).kind == IrOpKind::Constant);
        OperandX64 opb = OP_B(inst).kind == IrOpKind::Inst ? regOp(OP_B(inst)) : OperandX64(tagOp(OP_B(inst)));

        if (OP_A(inst).kind == IrOpKind::Constant)
            build.cmp(opb, tagOp(OP_A(inst)));
        else
            build.cmp(memRegTagOp(OP_A(inst)), opb);

        if (isFallthroughBlock(blockOp(OP_D(inst)), next))
        {
            build.jcc(ConditionX64::Equal, labelOp(OP_C(inst)));
            jumpOrFallthrough(blockOp(OP_D(inst)), next);
        }
        else
        {
            build.jcc(ConditionX64::NotEqual, labelOp(OP_D(inst)));
            jumpOrFallthrough(blockOp(OP_C(inst)), next);
        }
        break;
    }
    case IrCmd::JUMP_CMP_INT:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        if ((cond == IrCondition::Equal || cond == IrCondition::NotEqual) && intOp(OP_B(inst)) == 0)
        {
            bool invert = cond == IrCondition::NotEqual;

            build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));

            if (isFallthroughBlock(blockOp(OP_D(inst)), next))
            {
                build.jcc(invert ? ConditionX64::Zero : ConditionX64::NotZero, labelOp(OP_E(inst)));
                jumpOrFallthrough(blockOp(OP_D(inst)), next);
            }
            else
            {
                build.jcc(invert ? ConditionX64::NotZero : ConditionX64::Zero, labelOp(OP_D(inst)));
                jumpOrFallthrough(blockOp(OP_E(inst)), next);
            }
        }
        else
        {
            build.cmp(regOp(OP_A(inst)), intOp(OP_B(inst)));

            build.jcc(getConditionInt(cond), labelOp(OP_D(inst)));
            jumpOrFallthrough(blockOp(OP_E(inst)), next);
        }
        break;
    }
    case IrCmd::JUMP_EQ_POINTER:
        build.cmp(regOp(OP_A(inst)), regOp(OP_B(inst)));

        build.jcc(ConditionX64::Equal, labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    case IrCmd::JUMP_CMP_NUM:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        ScopedRegX64 tmp{regs, SizeX64::xmmword};

        jumpOnNumberCmp(
            build, tmp.reg, memRegDoubleOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)), cond, labelOp(OP_D(inst)), /* floatPrecision */ false
        );
        jumpOrFallthrough(blockOp(OP_E(inst)), next);
        break;
    }
    case IrCmd::JUMP_CMP_FLOAT:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        ScopedRegX64 tmp{regs, SizeX64::xmmword};

        jumpOnNumberCmp(build, tmp.reg, memRegFloatOp(OP_A(inst)), memRegFloatOp(OP_B(inst)), cond, labelOp(OP_D(inst)), /* floatPrecision */ true);
        jumpOrFallthrough(blockOp(OP_E(inst)), next);
        break;
    }
    case IrCmd::JUMP_FORN_LOOP_COND:
    {
        ScopedRegX64 tmp1{regs, SizeX64::xmmword};
        ScopedRegX64 tmp2{regs, SizeX64::xmmword};
        ScopedRegX64 tmp3{regs, SizeX64::xmmword};

        RegisterX64 index = OP_A(inst).kind == IrOpKind::Inst ? regOp(OP_A(inst)) : tmp1.reg;
        RegisterX64 limit = OP_B(inst).kind == IrOpKind::Inst ? regOp(OP_B(inst)) : tmp2.reg;

        if (OP_A(inst).kind != IrOpKind::Inst)
            build.vmovsd(tmp1.reg, memRegDoubleOp(OP_A(inst)));

        if (OP_B(inst).kind != IrOpKind::Inst)
            build.vmovsd(tmp2.reg, memRegDoubleOp(OP_B(inst)));

        Label direct;

        // step > 0
        jumpOnNumberCmp(build, tmp3.reg, memRegDoubleOp(OP_C(inst)), build.f64(0.0), IrCondition::Greater, direct, /* floatPrecision */ false);

        // !(limit <= index)
        jumpOnNumberCmp(build, noreg, limit, index, IrCondition::NotLessEqual, labelOp(OP_E(inst)), /* floatPrecision */ false);
        build.jmp(labelOp(OP_D(inst)));

        // !(index <= limit)
        build.setLabel(direct);
        jumpOnNumberCmp(build, noreg, index, limit, IrCondition::NotLessEqual, labelOp(OP_E(inst)), /* floatPrecision */ false);
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    }
    case IrCmd::TABLE_LEN:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, regOp(OP_A(inst)), OP_A(inst));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaH_getn)]);

        inst.regX64 = regs.takeReg(eax, index);

        build.mov(inst.regX64, inst.regX64); // Ensure high register bits are cleared
        break;
    }
    case IrCmd::TABLE_SETNUM:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, regOp(OP_A(inst)), OP_A(inst));
        callWrap.addArgument(SizeX64::dword, regOp(OP_B(inst)), OP_B(inst));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaH_setnum)]);
        inst.regX64 = regs.takeReg(rax, index);
        break;
    }
    case IrCmd::STRING_LEN:
    {
        RegisterX64 ptr = regOp(OP_A(inst));
        inst.regX64 = regs.allocReg(SizeX64::dword, index);
        build.mov(inst.regX64, dword[ptr + offsetof(TString, len)]);
        break;
    }
    case IrCmd::NEW_TABLE:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::dword, int32_t(uintOp(OP_A(inst))));
        callWrap.addArgument(SizeX64::dword, int32_t(uintOp(OP_B(inst))));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaH_new)]);
        inst.regX64 = regs.takeReg(rax, index);
        break;
    }
    case IrCmd::DUP_TABLE:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, regOp(OP_A(inst)), OP_A(inst));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaH_clone)]);
        inst.regX64 = regs.takeReg(rax, index);
        break;
    }
    case IrCmd::TRY_NUM_TO_INDEX:
    {
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        ScopedRegX64 tmp{regs, SizeX64::xmmword};

        convertNumberToIndexOrJump(build, tmp.reg, regOp(OP_A(inst)), inst.regX64, labelOp(OP_B(inst)));
        break;
    }
    case IrCmd::TRY_CALL_FASTGETTM:
    {
        if (FFlag::LuauCodegenCallWrapImproved)
        {
            inst.regX64 = regs.allocReg(SizeX64::qword, index);

            ScopedRegX64 tmp{regs, SizeX64::qword};

            build.mov(tmp.reg, qword[regOp(OP_A(inst)) + offsetof(LuaTable, metatable)]);
            regs.freeLastUseReg(function.instOp(OP_A(inst)), index); // Release before the call if it's the last use

            build.test(tmp.reg, tmp.reg);
            build.jcc(ConditionX64::Zero, labelOp(OP_C(inst))); // No metatable

            build.test(byte[tmp.reg + offsetof(LuaTable, tmcache)], 1 << intOp(OP_B(inst)));
            build.jcc(ConditionX64::NotZero, labelOp(OP_C(inst))); // No tag method

            ScopedRegX64 tmp2{regs, SizeX64::qword};
            build.mov(tmp2.reg, qword[rState + offsetof(lua_State, global)]);

            {
                ScopedSpills spillGuard(regs);

                IrCallWrapperX64 callWrap(regs, build, index);
                callWrap.addArgument(SizeX64::qword, tmp);
                callWrap.addArgument(SizeX64::qword, intOp(OP_B(inst)));
                callWrap.addArgument(SizeX64::qword, qword[tmp2.release() + offsetof(global_State, tmname) + intOp(OP_B(inst)) * sizeof(TString*)]);
                callWrap.setResultRegister(inst.regX64, index);
                callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaT_gettm)]);
            }

            build.test(inst.regX64, inst.regX64);
            build.jcc(ConditionX64::Zero, labelOp(OP_C(inst))); // No tag method
        }
        else
        {
            ScopedRegX64 tmp{regs, SizeX64::qword};

            build.mov(tmp.reg, qword[regOp(OP_A(inst)) + offsetof(LuaTable, metatable)]);
            regs.freeLastUseReg(function.instOp(OP_A(inst)), index); // Release before the call if it's the last use

            build.test(tmp.reg, tmp.reg);
            build.jcc(ConditionX64::Zero, labelOp(OP_C(inst))); // No metatable

            build.test(byte[tmp.reg + offsetof(LuaTable, tmcache)], 1 << intOp(OP_B(inst)));
            build.jcc(ConditionX64::NotZero, labelOp(OP_C(inst))); // No tag method

            ScopedRegX64 tmp2{regs, SizeX64::qword};
            build.mov(tmp2.reg, qword[rState + offsetof(lua_State, global)]);

            {
                ScopedSpills spillGuard(regs);

                IrCallWrapperX64 callWrap(regs, build, index);
                callWrap.addArgument(SizeX64::qword, tmp);
                callWrap.addArgument(SizeX64::qword, intOp(OP_B(inst)));
                callWrap.addArgument(SizeX64::qword, qword[tmp2.release() + offsetof(global_State, tmname) + intOp(OP_B(inst)) * sizeof(TString*)]);
                callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaT_gettm)]);
            }

            build.test(rax, rax);
            build.jcc(ConditionX64::Zero, labelOp(OP_C(inst))); // No tag method

            inst.regX64 = regs.takeReg(rax, index);
        }
        break;
    }
    case IrCmd::NEW_USERDATA:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, intOp(OP_A(inst)));
        callWrap.addArgument(SizeX64::dword, intOp(OP_B(inst)));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, newUserdata)]);
        inst.regX64 = regs.takeReg(rax, index);
        break;
    }
    case IrCmd::INT_TO_NUM:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        build.vcvtsi2sd(inst.regX64, inst.regX64, regOp(OP_A(inst)));
        break;
    case IrCmd::UINT_TO_NUM:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        // AVX has no uint->double conversion; the source must come from UINT op and they all should clear top 32 bits so we can usually
        // use 64-bit reg; the one exception is NUM_TO_UINT which doesn't clear top bits
        if (IrCmd source = function.instOp(OP_A(inst)).cmd; source == IrCmd::NUM_TO_UINT)
        {
            ScopedRegX64 tmp{regs, SizeX64::dword};
            build.mov(tmp.reg, regOp(OP_A(inst)));
            build.vcvtsi2sd(inst.regX64, inst.regX64, qwordReg(tmp.reg));
        }
        else
        {
            CODEGEN_ASSERT(source != IrCmd::SUBSTITUTE); // we don't process substitutions
            build.vcvtsi2sd(inst.regX64, inst.regX64, qwordReg(regOp(OP_A(inst))));
        }
        break;
    case IrCmd::UINT_TO_FLOAT:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        // AVX has no uint->float conversion; the source must come from UINT op and they all should clear top 32 bits so we can usually
        // use 64-bit reg; the one exception is NUM_TO_UINT which doesn't clear top bits
        if (IrCmd source = function.instOp(OP_A(inst)).cmd; source == IrCmd::NUM_TO_UINT)
        {
            ScopedRegX64 tmp{regs, SizeX64::dword};
            build.mov(tmp.reg, regOp(OP_A(inst)));
            build.vcvtsi2ss(inst.regX64, inst.regX64, qwordReg(tmp.reg));
        }
        else
        {
            CODEGEN_ASSERT(source != IrCmd::SUBSTITUTE); // we don't process substitutions
            build.vcvtsi2ss(inst.regX64, inst.regX64, qwordReg(regOp(OP_A(inst))));
        }
        break;
    case IrCmd::NUM_TO_INT:
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        build.vcvttsd2si(inst.regX64, memRegDoubleOp(OP_A(inst)));
        break;
    case IrCmd::NUM_TO_UINT:
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        // Note: we perform 'uint64_t = (long long)double' for consistency with C++ code
        build.vcvttsd2si(qwordReg(inst.regX64), memRegDoubleOp(OP_A(inst)));
        break;

    case IrCmd::FLOAT_TO_NUM:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vcvtss2sd(inst.regX64, inst.regX64, memRegDoubleOp(OP_A(inst)));
        break;

    case IrCmd::NUM_TO_FLOAT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vcvtsd2ss(inst.regX64, inst.regX64, memRegDoubleOp(OP_A(inst)));
        break;
    case IrCmd::FLOAT_TO_VEC:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            float value = float(doubleOp(OP_A(inst)));
            uint32_t asU32;
            static_assert(sizeof(asU32) == sizeof(value), "Expecting float to be 32-bit");
            memcpy(&asU32, &value, sizeof(value));

            build.vmovaps(inst.regX64, build.u32x4(asU32, asU32, asU32, 0));
        }
        else
        {
            build.vpshufps(inst.regX64, regOp(OP_A(inst)), regOp(OP_A(inst)), 0b00'00'00'00);
        }
        break;
    case IrCmd::TAG_VECTOR:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::xmmword, index, {OP_A(inst)});

        build.vpinsrd(inst.regX64, regOp(OP_A(inst)), build.i32(LUA_TVECTOR), 3);
        break;
    case IrCmd::TRUNCATE_UINT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        // Might generate mov with the same source and destination register which is not a no-op
        build.mov(inst.regX64, regOp(OP_A(inst)));
        break;
    case IrCmd::ADJUST_STACK_TO_REG:
    {
        ScopedRegX64 tmp{regs, SizeX64::qword};

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            build.lea(tmp.reg, addr[rBase + (vmRegOp(OP_A(inst)) + intOp(OP_B(inst))) * sizeof(TValue)]);
            build.mov(qword[rState + offsetof(lua_State, top)], tmp.reg);
        }
        else if (OP_B(inst).kind == IrOpKind::Inst)
        {
            build.mov(dwordReg(tmp.reg), regOp(OP_B(inst)));
            build.shl(tmp.reg, kTValueSizeLog2);
            build.lea(tmp.reg, addr[rBase + tmp.reg + vmRegOp(OP_A(inst)) * sizeof(TValue)]);
            build.mov(qword[rState + offsetof(lua_State, top)], tmp.reg);
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::ADJUST_STACK_TO_TOP:
    {
        ScopedRegX64 tmp{regs, SizeX64::qword};
        build.mov(tmp.reg, qword[rState + offsetof(lua_State, ci)]);
        build.mov(tmp.reg, qword[tmp.reg + offsetof(CallInfo, top)]);
        build.mov(qword[rState + offsetof(lua_State, top)], tmp.reg);
        break;
    }

    case IrCmd::FASTCALL:
    {
        emitBuiltin(regs, build, uintOp(OP_A(inst)), vmRegOp(OP_B(inst)), vmRegOp(OP_C(inst)), intOp(OP_D(inst)));
        break;
    }
    case IrCmd::INVOKE_FASTCALL:
    {
        unsigned bfid = uintOp(OP_A(inst));

        OperandX64 args = 0;
        ScopedRegX64 argsAlt{regs};

        // 'E' argument can only be produced by LOP_FASTCALL3
        if (OP_E(inst).kind != IrOpKind::Undef)
        {
            CODEGEN_ASSERT(intOp(OP_F(inst)) == 3);

            ScopedRegX64 tmp{regs, SizeX64::xmmword};
            argsAlt.alloc(SizeX64::qword);

            build.mov(argsAlt.reg, qword[rState + offsetof(lua_State, top)]);

            build.vmovups(tmp.reg, luauReg(vmRegOp(OP_D(inst))));
            build.vmovups(xmmword[argsAlt.reg], tmp.reg);

            build.vmovups(tmp.reg, luauReg(vmRegOp(OP_E(inst))));
            build.vmovups(xmmword[argsAlt.reg + sizeof(TValue)], tmp.reg);
        }
        else
        {
            if (OP_D(inst).kind == IrOpKind::VmReg)
                args = luauRegAddress(vmRegOp(OP_D(inst)));
            else if (OP_D(inst).kind == IrOpKind::VmConst)
                args = luauConstantAddress(vmConstOp(OP_D(inst)));
            else
                CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::Undef);
        }

        int ra = vmRegOp(OP_B(inst));
        int arg = vmRegOp(OP_C(inst));
        int nparams = intOp(OP_F(inst));
        int nresults = intOp(OP_G(inst));

        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, luauRegAddress(ra));
        callWrap.addArgument(SizeX64::qword, luauRegAddress(arg));
        callWrap.addArgument(SizeX64::dword, nresults);

        if (OP_E(inst).kind != IrOpKind::Undef)
            callWrap.addArgument(SizeX64::qword, argsAlt);
        else
            callWrap.addArgument(SizeX64::qword, args);

        if (nparams == LUA_MULTRET)
        {
            RegisterX64 reg = callWrap.suggestNextArgumentRegister(SizeX64::qword);
            ScopedRegX64 tmp{regs, SizeX64::qword};

            // L->top - (ra + 1)
            build.mov(reg, qword[rState + offsetof(lua_State, top)]);
            build.lea(tmp.reg, addr[rBase + (ra + 1) * sizeof(TValue)]);
            build.sub(reg, tmp.reg);
            build.shr(reg, kTValueSizeLog2);

            callWrap.addArgument(SizeX64::dword, dwordReg(reg));
        }
        else
        {
            callWrap.addArgument(SizeX64::dword, nparams);
        }

        ScopedRegX64 func{regs, SizeX64::qword};
        build.mov(func.reg, qword[rNativeContext + offsetof(NativeContext, luauF_table) + bfid * sizeof(luau_FastFunction)]);

        callWrap.call(func.release());
        inst.regX64 = regs.takeReg(eax, index); // Result of a builtin call is returned in eax
        // Skipping high register bits clear, only consumer is CHECK_FASTCALL_RES which doesn't read them
        break;
    }
    case IrCmd::CHECK_FASTCALL_RES:
    {
        RegisterX64 res = regOp(OP_A(inst));

        build.test(res, res);                               // test here will set SF=1 for a negative number and it always sets OF to 0
        build.jcc(ConditionX64::Less, labelOp(OP_B(inst))); // jl jumps if SF != OF
        break;
    }
    case IrCmd::DO_ARITH:
    {
        OperandX64 opb = OP_B(inst).kind == IrOpKind::VmReg ? luauRegAddress(vmRegOp(OP_B(inst))) : luauConstantAddress(vmConstOp(OP_B(inst)));
        OperandX64 opc = OP_C(inst).kind == IrOpKind::VmReg ? luauRegAddress(vmRegOp(OP_C(inst))) : luauConstantAddress(vmConstOp(OP_C(inst)));
        callArithHelper(regs, build, vmRegOp(OP_A(inst)), opb, opc, TMS(intOp(OP_D(inst))));
        break;
    }
    case IrCmd::DO_LEN:
        callLengthHelper(regs, build, vmRegOp(OP_A(inst)), vmRegOp(OP_B(inst)));
        break;
    case IrCmd::GET_TABLE:
        if (OP_C(inst).kind == IrOpKind::VmReg)
        {
            callGetTable(regs, build, vmRegOp(OP_B(inst)), luauRegAddress(vmRegOp(OP_C(inst))), vmRegOp(OP_A(inst)));
        }
        else if (OP_C(inst).kind == IrOpKind::Constant)
        {
            TValue n = {};
            setnvalue(&n, uintOp(OP_C(inst)));
            callGetTable(regs, build, vmRegOp(OP_B(inst)), build.bytes(&n, sizeof(n)), vmRegOp(OP_A(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::SET_TABLE:
        if (OP_C(inst).kind == IrOpKind::VmReg)
        {
            callSetTable(regs, build, vmRegOp(OP_B(inst)), luauRegAddress(vmRegOp(OP_C(inst))), vmRegOp(OP_A(inst)));
        }
        else if (OP_C(inst).kind == IrOpKind::Constant)
        {
            TValue n = {};
            setnvalue(&n, uintOp(OP_C(inst)));
            callSetTable(regs, build, vmRegOp(OP_B(inst)), build.bytes(&n, sizeof(n)), vmRegOp(OP_A(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    case IrCmd::GET_CACHED_IMPORT:
    {
        regs.assertAllFree();
        regs.assertNoSpills();

        Label skip, exit;

        // If the constant for the import is set, we will use it directly, otherwise we have to call an import path lookup function
        build.cmp(luauConstantTag(vmConstOp(OP_B(inst))), LUA_TNIL);
        build.jcc(ConditionX64::NotEqual, skip);

        {
            ScopedSpills spillGuard(regs);

            IrCallWrapperX64 callWrap(regs, build, index);
            callWrap.addArgument(SizeX64::qword, rState);
            callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_A(inst))));
            callWrap.addArgument(SizeX64::dword, importOp(OP_C(inst)));
            callWrap.addArgument(SizeX64::dword, uintOp(OP_D(inst)));
            callWrap.call(qword[rNativeContext + offsetof(NativeContext, getImport)]);

            emitUpdateBase(build);
        }

        build.jmp(exit);

        build.setLabel(skip);

        ScopedRegX64 tmp1{regs, SizeX64::xmmword};

        build.vmovups(tmp1.reg, luauConstant(vmConstOp(OP_B(inst))));
        build.vmovups(luauReg(vmRegOp(OP_A(inst))), tmp1.reg);
        build.setLabel(exit);
        break;
    }
    case IrCmd::CONCAT:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::dword, int32_t(uintOp(OP_B(inst))));
        callWrap.addArgument(SizeX64::dword, int32_t(vmRegOp(OP_A(inst)) + uintOp(OP_B(inst)) - 1));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaV_concat)]);

        emitUpdateBase(build);
        break;
    }
    case IrCmd::GET_UPVALUE:
    {
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        ScopedRegX64 tmp1{regs, SizeX64::qword};

        build.mov(tmp1.reg, sClosure);
        build.add(tmp1.reg, offsetof(Closure, l.uprefs) + sizeof(TValue) * vmUpvalueOp(OP_A(inst)));

        // uprefs[] is either an actual value, or it points to UpVal object which has a pointer to value
        Label skip;
        build.cmp(dword[tmp1.reg + offsetof(TValue, tt)], LUA_TUPVAL);
        build.jcc(ConditionX64::NotEqual, skip);

        // UpVal.v points to the value (either on stack, or on heap inside each UpVal, but we can deref it unconditionally)
        build.mov(tmp1.reg, qword[tmp1.reg + offsetof(TValue, value.gc)]);
        build.mov(tmp1.reg, qword[tmp1.reg + offsetof(UpVal, v)]);

        build.setLabel(skip);

        build.vmovups(inst.regX64, xmmword[tmp1.reg]);
        break;
    }
    case IrCmd::SET_UPVALUE:
    {
        ScopedRegX64 tmp1{regs, SizeX64::qword};
        ScopedRegX64 tmp2{regs, SizeX64::qword};

        build.mov(tmp1.reg, sClosure);
        build.mov(tmp2.reg, qword[tmp1.reg + offsetof(Closure, l.uprefs) + sizeof(TValue) * vmUpvalueOp(OP_A(inst)) + offsetof(TValue, value.gc)]);

        build.mov(tmp1.reg, qword[tmp2.reg + offsetof(UpVal, v)]);
        build.vmovups(xmmword[tmp1.reg], regOp(OP_B(inst)));

        tmp1.free();

        if (OP_C(inst).kind == IrOpKind::Undef || isGCO(tagOp(OP_C(inst))))
        {
            callBarrierObject(
                regs, build, tmp2.release(), {}, regOp(OP_B(inst)), OP_B(inst), OP_C(inst).kind == IrOpKind::Undef ? -1 : tagOp(OP_C(inst))
            );
        }
        break;
    }
    case IrCmd::CHECK_TAG:
        build.cmp(memRegTagOp(OP_A(inst)), tagOp(OP_B(inst)));
        jumpOrAbortOnUndef(ConditionX64::NotEqual, OP_C(inst), next);
        break;
    case IrCmd::CHECK_TRUTHY:
    {
        // Constant tags which don't require boolean value check should've been removed in constant folding
        CODEGEN_ASSERT(OP_A(inst).kind != IrOpKind::Constant || tagOp(OP_A(inst)) == LUA_TBOOLEAN);

        Label skip;

        if (OP_A(inst).kind != IrOpKind::Constant)
        {
            // Fail to fallback on 'nil' (falsy)
            build.cmp(memRegTagOp(OP_A(inst)), LUA_TNIL);
            jumpOrAbortOnUndef(ConditionX64::Equal, OP_C(inst), next);

            // Skip value test if it's not a boolean (truthy)
            build.cmp(memRegTagOp(OP_A(inst)), LUA_TBOOLEAN);
            build.jcc(ConditionX64::NotEqual, skip);
        }

        // fail to fallback on 'false' boolean value (falsy)
        if (OP_B(inst).kind != IrOpKind::Constant)
        {
            build.cmp(memRegUintOp(OP_B(inst)), 0);
            jumpOrAbortOnUndef(ConditionX64::Equal, OP_C(inst), next);
        }
        else
        {
            if (intOp(OP_B(inst)) == 0)
                jumpOrAbortOnUndef(OP_C(inst), next);
        }

        if (OP_A(inst).kind != IrOpKind::Constant)
            build.setLabel(skip);
        break;
    }
    case IrCmd::CHECK_READONLY:
        build.cmp(byte[regOp(OP_A(inst)) + offsetof(LuaTable, readonly)], 0);
        jumpOrAbortOnUndef(ConditionX64::NotEqual, OP_B(inst), next);
        break;
    case IrCmd::CHECK_NO_METATABLE:
        build.cmp(qword[regOp(OP_A(inst)) + offsetof(LuaTable, metatable)], 0);
        jumpOrAbortOnUndef(ConditionX64::NotEqual, OP_B(inst), next);
        break;
    case IrCmd::CHECK_SAFE_ENV:
    {
        checkSafeEnv(OP_A(inst), next);
        break;
    }
    case IrCmd::CHECK_ARRAY_SIZE:
        if (OP_B(inst).kind == IrOpKind::Inst)
            build.cmp(dword[regOp(OP_A(inst)) + offsetof(LuaTable, sizearray)], regOp(OP_B(inst)));
        else if (OP_B(inst).kind == IrOpKind::Constant)
            build.cmp(dword[regOp(OP_A(inst)) + offsetof(LuaTable, sizearray)], intOp(OP_B(inst)));
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");

        jumpOrAbortOnUndef(ConditionX64::BelowEqual, OP_C(inst), next);
        break;
    case IrCmd::JUMP_SLOT_MATCH:
    case IrCmd::CHECK_SLOT_MATCH:
    {
        Label abort; // Used when guard aborts execution
        const IrOp& mismatchOp = inst.cmd == IrCmd::JUMP_SLOT_MATCH ? OP_D(inst) : OP_C(inst);
        Label& mismatch = mismatchOp.kind == IrOpKind::Undef ? abort : labelOp(mismatchOp);

        ScopedRegX64 tmp{regs, SizeX64::qword};

        // Check if node key tag is a string
        build.mov(dwordReg(tmp.reg), luauNodeKeyTag(regOp(OP_A(inst))));
        build.and_(dwordReg(tmp.reg), kTKeyTagMask);
        build.cmp(dwordReg(tmp.reg), LUA_TSTRING);
        build.jcc(ConditionX64::NotEqual, mismatch);

        // Check that node key value matches the expected one
        build.mov(tmp.reg, luauConstantValue(vmConstOp(OP_B(inst))));
        build.cmp(tmp.reg, luauNodeKeyValue(regOp(OP_A(inst))));
        build.jcc(ConditionX64::NotEqual, mismatch);

        // Check that node value is not nil
        build.cmp(dword[regOp(OP_A(inst)) + offsetof(LuaNode, val) + offsetof(TValue, tt)], LUA_TNIL);
        build.jcc(ConditionX64::Equal, mismatch);

        if (inst.cmd == IrCmd::JUMP_SLOT_MATCH)
        {
            jumpOrFallthrough(blockOp(OP_C(inst)), next);
        }
        else if (mismatchOp.kind == IrOpKind::Undef)
        {
            Label skip;
            build.jmp(skip);
            build.setLabel(abort);
            build.ud2();
            build.setLabel(skip);
        }
        break;
    }
    case IrCmd::CHECK_NODE_NO_NEXT:
    {
        ScopedRegX64 tmp{regs, SizeX64::dword};

        build.mov(tmp.reg, dword[regOp(OP_A(inst)) + offsetof(LuaNode, key) + kOffsetOfTKeyTagNext]);
        build.shr(tmp.reg, kTKeyTagBits);
        jumpOrAbortOnUndef(ConditionX64::NotZero, OP_B(inst), next);
        break;
    }
    case IrCmd::CHECK_NODE_VALUE:
    {
        build.cmp(dword[regOp(OP_A(inst)) + offsetof(LuaNode, val) + offsetof(TValue, tt)], LUA_TNIL);
        jumpOrAbortOnUndef(ConditionX64::Equal, OP_B(inst), next);
        break;
    }
    case IrCmd::CHECK_BUFFER_LEN:
    {
        if (FFlag::LuauCodegenBufferRangeMerge4)
        {
            int minOffset = intOp(OP_C(inst));
            int maxOffset = intOp(OP_D(inst));
            CODEGEN_ASSERT(minOffset < maxOffset);

            int accessSize = maxOffset - minOffset;
            CODEGEN_ASSERT(accessSize > 0);

            // Check if we are acting not only as a guard for the size, but as a guard that offset represents an exact integer
            if (OP_E(inst).kind != IrOpKind::Undef)
            {
                CODEGEN_ASSERT(getCmdValueKind(function.instOp(OP_B(inst)).cmd) == IrValueKind::Int);
                CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(OP_B(inst)).cmd)); // Ensure that high register bits are cleared

                ScopedRegX64 tmp{regs, SizeX64::xmmword};

                // Convert integer back to double
                build.vcvtsi2sd(tmp.reg, tmp.reg, regOp(OP_B(inst)));

                build.vucomisd(tmp.reg, regOp(OP_E(inst))); // Sets ZF=1 if equal or NaN, PF=1 on NaN

                // We don't allow non-integer values
                jumpOrAbortOnUndef(ConditionX64::NotZero, OP_F(inst), next); // exit on ZF=0
                jumpOrAbortOnUndef(ConditionX64::Parity, OP_F(inst), next);  // exit on PF=1
            }

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(OP_B(inst)).cmd)); // Ensure that high register bits are cleared

                if (accessSize == 1 && minOffset == 0)
                {
                    // Simpler check for a single byte access
                    build.cmp(dword[regOp(OP_A(inst)) + offsetof(Buffer, len)], regOp(OP_B(inst)));
                    jumpOrAbortOnUndef(ConditionX64::BelowEqual, OP_F(inst), next);
                }
                else
                {
                    ScopedRegX64 tmp1{regs, SizeX64::qword};
                    ScopedRegX64 tmp2{regs, SizeX64::dword};

                    // To perform the bounds check using a single branch, we take index that is limited to a 32 bit int
                    // Max offset is then added using a 64 bit addition
                    // This will make sure that addition will not wrap around for values like 0xffffffff

                    if (minOffset >= 0)
                    {
                        build.lea(tmp1.reg, addr[qwordReg(regOp(OP_B(inst))) + maxOffset]);
                    }
                    else
                    {
                        // When the min offset is negative, we subtract it from offset first (in 32 bits)
                        build.lea(dwordReg(tmp1.reg), addr[regOp(OP_B(inst)) + minOffset]);

                        // And then add the full access size like before
                        build.lea(tmp1.reg, addr[tmp1.reg + accessSize]);
                    }

                    build.mov(tmp2.reg, dword[regOp(OP_A(inst)) + offsetof(Buffer, len)]);
                    build.cmp(qwordReg(tmp2.reg), tmp1.reg);

                    jumpOrAbortOnUndef(ConditionX64::Below, OP_F(inst), next);
                }
            }
            else if (OP_B(inst).kind == IrOpKind::Constant)
            {
                int offset = intOp(OP_B(inst));

                // Constant folding can take care of it, but for safety we avoid overflow/underflow cases here
                if (offset < 0 || unsigned(offset) + unsigned(accessSize) >= unsigned(INT_MAX))
                    jumpOrAbortOnUndef(OP_F(inst), next);
                else
                    build.cmp(dword[regOp(OP_A(inst)) + offsetof(Buffer, len)], offset + accessSize);

                jumpOrAbortOnUndef(ConditionX64::Below, OP_F(inst), next);
            }
            else
            {
                CODEGEN_ASSERT(!"Unsupported instruction form");
            }
        }
        else
        {
            int accessSize = intOp(OP_C(inst));
            CODEGEN_ASSERT(accessSize > 0);

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(OP_B(inst)).cmd)); // Ensure that high register bits are cleared

                if (accessSize == 1)
                {
                    // Simpler check for a single byte access
                    build.cmp(dword[regOp(OP_A(inst)) + offsetof(Buffer, len)], regOp(OP_B(inst)));
                    jumpOrAbortOnUndef(ConditionX64::BelowEqual, OP_D(inst), next);
                }
                else
                {
                    ScopedRegX64 tmp1{regs, SizeX64::qword};
                    ScopedRegX64 tmp2{regs, SizeX64::dword};

                    // To perform the bounds check using a single branch, we take index that is limited to 32 bit int
                    // Access size is then added using a 64 bit addition
                    // This will make sure that addition will not wrap around for values like 0xffffffff
                    build.lea(tmp1.reg, addr[qwordReg(regOp(OP_B(inst))) + accessSize]);
                    build.mov(tmp2.reg, dword[regOp(OP_A(inst)) + offsetof(Buffer, len)]);
                    build.cmp(qwordReg(tmp2.reg), tmp1.reg);

                    jumpOrAbortOnUndef(ConditionX64::Below, OP_D(inst), next);
                }
            }
            else if (OP_B(inst).kind == IrOpKind::Constant)
            {
                int offset = intOp(OP_B(inst));

                // Constant folding can take care of it, but for safety we avoid overflow/underflow cases here
                if (offset < 0 || unsigned(offset) + unsigned(accessSize) >= unsigned(INT_MAX))
                    jumpOrAbortOnUndef(OP_D(inst), next);
                else
                    build.cmp(dword[regOp(OP_A(inst)) + offsetof(Buffer, len)], offset + accessSize);

                jumpOrAbortOnUndef(ConditionX64::Below, OP_D(inst), next);
            }
            else
            {
                CODEGEN_ASSERT(!"Unsupported instruction form");
            }
        }
        break;
    }
    case IrCmd::CHECK_USERDATA_TAG:
    {
        build.cmp(byte[regOp(OP_A(inst)) + offsetof(Udata, tag)], intOp(OP_B(inst)));
        jumpOrAbortOnUndef(ConditionX64::NotEqual, OP_C(inst), next);
        break;
    }
    case IrCmd::CHECK_CMP_NUM:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        Label fresh;
        Label& fail = getTargetLabel(OP_D(inst), fresh);

        ScopedRegX64 tmp{regs, SizeX64::xmmword};

        jumpOnNumberCmp(build, tmp.reg, memRegDoubleOp(OP_A(inst)), memRegDoubleOp(OP_B(inst)), getNegatedCondition(cond), fail, false);

        finalizeTargetLabel(OP_D(inst), fresh);
        break;
    }
    case IrCmd::CHECK_CMP_INT:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        if ((cond == IrCondition::Equal || cond == IrCondition::NotEqual) && OP_B(inst).kind == IrOpKind::Constant && intOp(OP_B(inst)) == 0)
        {
            build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
            jumpOrAbortOnUndef(cond == IrCondition::Equal ? ConditionX64::NotZero : ConditionX64::Zero, OP_D(inst), next);
        }
        else if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::dword};
            build.mov(tmp.reg, memRegIntOp(OP_A(inst)));
            build.cmp(tmp.reg, memRegIntOp(OP_B(inst)));
            jumpOrAbortOnUndef(getConditionInt(getNegatedCondition(cond)), OP_D(inst), next);
        }
        else
        {
            build.cmp(regOp(OP_A(inst)), memRegIntOp(OP_B(inst)));
            jumpOrAbortOnUndef(getConditionInt(getNegatedCondition(cond)), OP_D(inst), next);
        }
        break;
    }
    case IrCmd::INTERRUPT:
    {
        unsigned pcpos = uintOp(OP_A(inst));

        // We unconditionally spill values here because that allows us to ignore register state when we synthesize interrupt handler
        // This can be changed in the future if we can somehow record interrupt handler code separately
        // Since interrupts are loop edges or call/ret, we don't have a significant opportunity for register reuse here anyway
        regs.preserveAndFreeInstValues();

        ScopedRegX64 tmp{regs, SizeX64::qword};

        Label self;

        build.mov(tmp.reg, qword[rState + offsetof(lua_State, global)]);
        build.cmp(qword[tmp.reg + offsetof(global_State, cb.interrupt)], 0);
        build.jcc(ConditionX64::NotEqual, self);

        Label next = build.setLabel();

        interruptHandlers.push_back({self, pcpos, next});
        break;
    }
    case IrCmd::CHECK_GC:
        callStepGc(regs, build);
        break;
    case IrCmd::BARRIER_OBJ:
        callBarrierObject(regs, build, regOp(OP_A(inst)), OP_A(inst), noreg, OP_B(inst), OP_C(inst).kind == IrOpKind::Undef ? -1 : tagOp(OP_C(inst)));
        break;
    case IrCmd::BARRIER_TABLE_BACK:
        callBarrierTableFast(regs, build, regOp(OP_A(inst)), OP_A(inst));
        break;
    case IrCmd::BARRIER_TABLE_FORWARD:
    {
        Label skip;

        ScopedRegX64 tmp{regs, SizeX64::qword};

        checkObjectBarrierConditions(
            build, tmp.reg, regOp(OP_A(inst)), noreg, OP_B(inst), OP_C(inst).kind == IrOpKind::Undef ? -1 : tagOp(OP_C(inst)), skip
        );

        {
            ScopedSpills spillGuard(regs);

            IrCallWrapperX64 callWrap(regs, build, index);
            callWrap.addArgument(SizeX64::qword, rState);
            callWrap.addArgument(SizeX64::qword, regOp(OP_A(inst)), OP_A(inst));
            callWrap.addArgument(SizeX64::qword, tmp);
            callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaC_barriertable)]);
        }

        build.setLabel(skip);
        break;
    }
    case IrCmd::SET_SAVEDPC:
    {
        ScopedRegX64 tmp1{regs, SizeX64::qword};
        ScopedRegX64 tmp2{regs, SizeX64::qword};

        build.mov(tmp2.reg, sCode);
        build.add(tmp2.reg, uintOp(OP_A(inst)) * sizeof(Instruction));
        build.mov(tmp1.reg, qword[rState + offsetof(lua_State, ci)]);
        build.mov(qword[tmp1.reg + offsetof(CallInfo, savedpc)], tmp2.reg);
        break;
    }
    case IrCmd::CLOSE_UPVALS:
    {
        Label next;
        ScopedRegX64 tmp1{regs, SizeX64::qword};
        ScopedRegX64 tmp2{regs, SizeX64::qword};

        // L->openupval != 0
        build.mov(tmp1.reg, qword[rState + offsetof(lua_State, openupval)]);
        build.test(tmp1.reg, tmp1.reg);
        build.jcc(ConditionX64::Zero, next);

        // ra <= L->openupval->v
        build.lea(tmp2.reg, addr[rBase + vmRegOp(OP_A(inst)) * sizeof(TValue)]);
        build.cmp(tmp2.reg, qword[tmp1.reg + offsetof(UpVal, v)]);
        build.jcc(ConditionX64::Above, next);

        tmp1.free();

        {
            ScopedSpills spillGuard(regs);

            IrCallWrapperX64 callWrap(regs, build, index);
            callWrap.addArgument(SizeX64::qword, rState);
            callWrap.addArgument(SizeX64::qword, tmp2);
            callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaF_close)]);
        }

        build.setLabel(next);
        break;
    }
    case IrCmd::CAPTURE:
        // No-op right now
        break;

        // Fallbacks to non-IR instruction implementations
    case IrCmd::SETLIST:
        regs.assertAllFree();
        emitInstSetList(
            regs,
            build,
            vmRegOp(OP_B(inst)),
            vmRegOp(OP_C(inst)),
            intOp(OP_D(inst)),
            uintOp(OP_E(inst)),
            OP_F(inst).kind == IrOpKind::Undef ? -1 : int(uintOp(OP_F(inst)))
        );
        break;
    case IrCmd::CALL:
        regs.assertAllFree();
        regs.assertNoSpills();
        emitInstCall(regs, build, helpers, vmRegOp(OP_A(inst)), intOp(OP_B(inst)), intOp(OP_C(inst)));
        break;
    case IrCmd::RETURN:
        regs.assertAllFree();
        regs.assertNoSpills();
        emitInstReturn(build, helpers, vmRegOp(OP_A(inst)), intOp(OP_B(inst)), function.variadic);
        break;
    case IrCmd::FORGLOOP:
        regs.assertAllFree();
        emitInstForGLoop(regs, build, vmRegOp(OP_A(inst)), intOp(OP_B(inst)), labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    case IrCmd::FORGLOOP_FALLBACK:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::dword, vmRegOp(OP_A(inst)));
        callWrap.addArgument(SizeX64::dword, intOp(OP_B(inst)));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, forgLoopNonTableFallback)]);

        emitUpdateBase(build);

        build.test(al, al);
        build.jcc(ConditionX64::NotZero, labelOp(OP_C(inst)));
        jumpOrFallthrough(blockOp(OP_D(inst)), next);
        break;
    }
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_B(inst))));
        callWrap.addArgument(SizeX64::dword, uintOp(OP_A(inst)) + 1);
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, forgPrepXnextFallback)]);
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    }
    case IrCmd::COVERAGE:
    {
        ScopedRegX64 tmp1{regs, SizeX64::qword};
        ScopedRegX64 tmp2{regs, SizeX64::dword};
        ScopedRegX64 tmp3{regs, SizeX64::dword};

        build.mov(tmp1.reg, sCode);
        build.add(tmp1.reg, uintOp(OP_A(inst)) * sizeof(Instruction));

        // hits = LUAU_INSN_E(*pc)
        build.mov(tmp2.reg, dword[tmp1.reg]);
        build.sar(tmp2.reg, 8);

        // hits = (hits < (1 << 23) - 1) ? hits + 1 : hits;
        build.xor_(tmp3.reg, tmp3.reg);
        build.cmp(tmp2.reg, (1 << 23) - 1);
        build.setcc(ConditionX64::NotEqual, byteReg(tmp3.reg));
        build.add(tmp2.reg, tmp3.reg);

        // VM_PATCH_E(pc, hits);
        build.sal(tmp2.reg, 8);
        build.movzx(tmp3.reg, byte[tmp1.reg]);
        build.or_(tmp3.reg, tmp2.reg);
        build.mov(dword[tmp1.reg], tmp3.reg);
        break;
    }

        // Full instruction fallbacks
    case IrCmd::FALLBACK_GETGLOBAL:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmConst);

        emitFallback(regs, build, offsetof(NativeContext, executeGETGLOBAL), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_SETGLOBAL:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmConst);

        emitFallback(regs, build, offsetof(NativeContext, executeSETGLOBAL), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_GETTABLEKS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmConst);

        emitFallback(regs, build, offsetof(NativeContext, executeGETTABLEKS), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_SETTABLEKS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmConst);

        emitFallback(regs, build, offsetof(NativeContext, executeSETTABLEKS), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_NAMECALL:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmConst);

        emitFallback(regs, build, offsetof(NativeContext, executeNAMECALL), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_PREPVARARGS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::Constant);

        emitFallback(regs, build, offsetof(NativeContext, executePREPVARARGS), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_GETVARARGS:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::Constant);

        if (intOp(OP_C(inst)) == LUA_MULTRET)
        {
            IrCallWrapperX64 callWrap(regs, build);
            callWrap.addArgument(SizeX64::qword, rState);

            RegisterX64 reg = callWrap.suggestNextArgumentRegister(SizeX64::qword);
            build.mov(reg, sCode);
            callWrap.addArgument(SizeX64::qword, addr[reg + uintOp(OP_A(inst)) * sizeof(Instruction)]);

            callWrap.addArgument(SizeX64::qword, rBase);
            callWrap.addArgument(SizeX64::dword, vmRegOp(OP_B(inst)));
            callWrap.call(qword[rNativeContext + offsetof(NativeContext, executeGETVARARGSMultRet)]);

            emitUpdateBase(build);
        }
        else
        {
            IrCallWrapperX64 callWrap(regs, build);
            callWrap.addArgument(SizeX64::qword, rState);
            callWrap.addArgument(SizeX64::qword, rBase);
            callWrap.addArgument(SizeX64::dword, vmRegOp(OP_B(inst)));
            callWrap.addArgument(SizeX64::dword, intOp(OP_C(inst)));
            callWrap.call(qword[rNativeContext + offsetof(NativeContext, executeGETVARARGSConst)]);
        }
        break;
    case IrCmd::NEWCLOSURE:
    {
        ScopedRegX64 tmp2{regs, SizeX64::qword};
        build.mov(tmp2.reg, sClosure);
        build.mov(tmp2.reg, qword[tmp2.reg + offsetof(Closure, l.p)]);
        build.mov(tmp2.reg, qword[tmp2.reg + offsetof(Proto, p)]);
        build.mov(tmp2.reg, qword[tmp2.reg + sizeof(Proto*) * uintOp(OP_C(inst))]);

        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::dword, uintOp(OP_A(inst)), OP_A(inst));
        callWrap.addArgument(SizeX64::qword, regOp(OP_B(inst)), OP_B(inst));
        callWrap.addArgument(SizeX64::qword, tmp2);

        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaF_newLclosure)]);

        inst.regX64 = regs.takeReg(rax, index);
        break;
    }
    case IrCmd::FALLBACK_DUPCLOSURE:
        CODEGEN_ASSERT(OP_B(inst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_C(inst).kind == IrOpKind::VmConst);

        emitFallback(regs, build, offsetof(NativeContext, executeDUPCLOSURE), uintOp(OP_A(inst)));
        break;
    case IrCmd::FALLBACK_FORGPREP:
        emitFallback(regs, build, offsetof(NativeContext, executeFORGPREP), uintOp(OP_A(inst)));
        jumpOrFallthrough(blockOp(OP_C(inst)), next);
        break;
    case IrCmd::BITAND_UINT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        build.and_(inst.regX64, memRegUintOp(OP_B(inst)));
        break;
    case IrCmd::BITXOR_UINT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        build.xor_(inst.regX64, memRegUintOp(OP_B(inst)));
        break;
    case IrCmd::BITOR_UINT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        build.or_(inst.regX64, memRegUintOp(OP_B(inst)));
        break;
    case IrCmd::BITNOT_UINT:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        build.not_(inst.regX64);
        break;
    case IrCmd::BITLSHIFT_UINT:
    {
        ScopedRegX64 shiftTmp{regs};

        // Custom bit shift value can only be placed in cl
        // but we use it if the shift value is not a constant stored in b
        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(ecx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            // if shift value is a constant, we extract the byte-sized shift amount
            int8_t shift = int8_t(unsigned(intOp(OP_B(inst))));
            build.shl(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegUintOp(OP_B(inst)));
            build.shl(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITRSHIFT_UINT:
    {
        ScopedRegX64 shiftTmp{regs};

        // Custom bit shift value can only be placed in cl
        // but we use it if the shift value is not a constant stored in b
        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(ecx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            // if shift value is a constant, we extract the byte-sized shift amount
            int8_t shift = int8_t(unsigned(intOp(OP_B(inst))));
            build.shr(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegUintOp(OP_B(inst)));
            build.shr(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITARSHIFT_UINT:
    {
        ScopedRegX64 shiftTmp{regs};

        // Custom bit shift value can only be placed in cl
        // but we use it if the shift value is not a constant stored in b
        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(ecx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            // if shift value is a constant, we extract the byte-sized shift amount
            int8_t shift = int8_t(unsigned(intOp(OP_B(inst))));
            build.sar(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegUintOp(OP_B(inst)));
            build.sar(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITLROTATE_UINT:
    {
        ScopedRegX64 shiftTmp{regs};

        // Custom bit shift value can only be placed in cl
        // but we use it if the shift value is not a constant stored in b
        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(ecx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            // if shift value is a constant, we extract the byte-sized shift amount
            int8_t shift = int8_t(unsigned(intOp(OP_B(inst))));
            build.rol(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegUintOp(OP_B(inst)));
            build.rol(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITRROTATE_UINT:
    {
        ScopedRegX64 shiftTmp{regs};

        // Custom bit shift value can only be placed in cl
        // but we use it if the shift value is not a constant stored in b
        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(ecx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            // if shift value is a constant, we extract the byte-sized shift amount
            int8_t shift = int8_t(unsigned(intOp(OP_B(inst))));
            build.ror(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegUintOp(OP_B(inst)));
            build.ror(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITCOUNTLZ_UINT:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        Label zero, exit;

        build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
        build.jcc(ConditionX64::Equal, zero);

        build.bsr(inst.regX64, regOp(OP_A(inst)));
        build.xor_(inst.regX64, 0x1f);
        build.jmp(exit);

        build.setLabel(zero);
        build.mov(inst.regX64, 32);

        build.setLabel(exit);
        break;
    }
    case IrCmd::BITCOUNTRZ_UINT:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        Label zero, exit;

        build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
        build.jcc(ConditionX64::Equal, zero);

        build.bsf(inst.regX64, regOp(OP_A(inst)));
        build.jmp(exit);

        build.setLabel(zero);
        build.mov(inst.regX64, 32);

        build.setLabel(exit);
        break;
    }
    case IrCmd::BYTESWAP_UINT:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegUintOp(OP_A(inst)));

        build.bswap(inst.regX64);
        break;
    }
    case IrCmd::INVOKE_LIBM:
    {
        IrCallWrapperX64 callWrap(regs, build, index);
        callWrap.addArgument(SizeX64::xmmword, memRegDoubleOp(OP_B(inst)), OP_B(inst));

        if (HAS_OP_C(inst))
        {
            bool isInt = (OP_C(inst).kind == IrOpKind::Constant) ? constOp(OP_C(inst)).kind == IrConstKind::Int
                                                                 : getCmdValueKind(function.instOp(OP_C(inst)).cmd) == IrValueKind::Int;

            if (isInt)
                callWrap.addArgument(SizeX64::dword, memRegUintOp(OP_C(inst)), OP_C(inst));
            else
                callWrap.addArgument(SizeX64::xmmword, memRegDoubleOp(OP_C(inst)), OP_C(inst));
        }

        callWrap.call(qword[rNativeContext + getNativeContextOffset(uintOp(OP_A(inst)))]);
        inst.regX64 = regs.takeReg(xmm0, index);
        break;
    }
    case IrCmd::GET_TYPE:
    {
        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        build.mov(inst.regX64, qword[rState + offsetof(lua_State, global)]);

        if (OP_A(inst).kind == IrOpKind::Inst)
            build.mov(inst.regX64, qword[inst.regX64 + qwordReg(regOp(OP_A(inst))) * sizeof(TString*) + offsetof(global_State, ttname)]);
        else if (OP_A(inst).kind == IrOpKind::Constant)
            build.mov(inst.regX64, qword[inst.regX64 + tagOp(OP_A(inst)) * sizeof(TString*) + offsetof(global_State, ttname)]);
        else
            CODEGEN_ASSERT(!"Unsupported instruction form");
        break;
    }
    case IrCmd::GET_TYPEOF:
    {
        IrCallWrapperX64 callWrap(regs, build);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_A(inst))));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaT_objtypenamestr)]);

        inst.regX64 = regs.takeReg(rax, index);
        break;
    }

    case IrCmd::FINDUPVAL:
    {
        IrCallWrapperX64 callWrap(regs, build);
        callWrap.addArgument(SizeX64::qword, rState);
        callWrap.addArgument(SizeX64::qword, luauRegAddress(vmRegOp(OP_A(inst))));
        callWrap.call(qword[rNativeContext + offsetof(NativeContext, luaF_findupval)]);

        inst.regX64 = regs.takeReg(rax, index);
        break;
    }

    case IrCmd::BUFFER_READI8:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst), OP_B(inst)});

        if (FFlag::LuauCodegenBufNoDefTag)
            build.movsx(inst.regX64, byte[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.movsx(inst.regX64, byte[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_READU8:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst), OP_B(inst)});

        if (FFlag::LuauCodegenBufNoDefTag)
            build.movzx(inst.regX64, byte[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.movzx(inst.regX64, byte[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_WRITEI8:
    {
        OperandX64 value = OP_C(inst).kind == IrOpKind::Inst ? byteReg(regOp(OP_C(inst))) : OperandX64(int8_t(intOp(OP_C(inst))));

        if (FFlag::LuauCodegenBufNoDefTag)
            build.mov(byte[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_D(inst)))], value);
        else
            build.mov(byte[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)))], value);
        break;
    }

    case IrCmd::BUFFER_READI16:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst), OP_B(inst)});

        if (FFlag::LuauCodegenBufNoDefTag)
            build.movsx(inst.regX64, word[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.movsx(inst.regX64, word[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_READU16:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst), OP_B(inst)});

        if (FFlag::LuauCodegenBufNoDefTag)
            build.movzx(inst.regX64, word[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.movzx(inst.regX64, word[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_WRITEI16:
    {
        OperandX64 value = OP_C(inst).kind == IrOpKind::Inst ? wordReg(regOp(OP_C(inst))) : OperandX64(int16_t(intOp(OP_C(inst))));

        if (FFlag::LuauCodegenBufNoDefTag)
            build.mov(word[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_D(inst)))], value);
        else
            build.mov(word[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)))], value);
        break;
    }

    case IrCmd::BUFFER_READI32:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::dword, index, {OP_A(inst), OP_B(inst)});

        if (FFlag::LuauCodegenBufNoDefTag)
            build.mov(inst.regX64, dword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.mov(inst.regX64, dword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_WRITEI32:
    {
        OperandX64 value = OP_C(inst).kind == IrOpKind::Inst ? regOp(OP_C(inst)) : OperandX64(intOp(OP_C(inst)));

        if (FFlag::LuauCodegenBufNoDefTag)
            build.mov(dword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_D(inst)))], value);
        else
            build.mov(dword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)))], value);
        break;
    }

    case IrCmd::BUFFER_READF32:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        if (FFlag::LuauCodegenBufNoDefTag)
            build.vmovss(inst.regX64, dword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.vmovss(inst.regX64, dword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_WRITEF32:
        if (FFlag::LuauCodegenBufNoDefTag)
            storeFloat(dword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_D(inst)))], OP_C(inst));
        else
            storeFloat(dword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)))], OP_C(inst));
        break;

    case IrCmd::BUFFER_READF64:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        if (FFlag::LuauCodegenBufNoDefTag)
            build.vmovsd(inst.regX64, qword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_C(inst)))]);
        else
            build.vmovsd(inst.regX64, qword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_C(inst) ? LUA_TBUFFER : tagOp(OP_C(inst)))]);
        break;

    case IrCmd::BUFFER_WRITEF64:
        if (OP_C(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::xmmword};
            build.vmovsd(tmp.reg, build.f64(doubleOp(OP_C(inst))));

            if (FFlag::LuauCodegenBufNoDefTag)
                build.vmovsd(qword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_D(inst)))], tmp.reg);
            else
                build.vmovsd(qword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)))], tmp.reg);
        }
        else if (OP_C(inst).kind == IrOpKind::Inst)
        {
            if (FFlag::LuauCodegenBufNoDefTag)
                build.vmovsd(qword[bufferAddrOp(OP_A(inst), OP_B(inst), tagOp(OP_D(inst)))], regOp(OP_C(inst)));
            else
                build.vmovsd(qword[bufferAddrOp(OP_A(inst), OP_B(inst), !HAS_OP_D(inst) ? LUA_TBUFFER : tagOp(OP_D(inst)))], regOp(OP_C(inst)));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;

    case IrCmd::CHECK_DIV_INT64:
    {
        ScopedRegX64 tmpA{regs, SizeX64::qword};
        ScopedRegX64 tmpB{regs, SizeX64::qword};
        build.mov(tmpA.reg, memRegInt64Op(OP_A(inst)));
        build.mov(tmpB.reg, memRegInt64Op(OP_B(inst)));

        // guard against division by zero
        build.test(tmpB.reg, tmpB.reg);
        jumpOrAbortOnUndef(ConditionX64::Equal, OP_C(inst), next);

        // guard against dividend == INT64_MIN && divisor == -1 (signed overflow)
        {
            Label skip;

            build.cmp(tmpB.reg, -1);
            build.jcc(ConditionX64::NotEqual, skip);

            ScopedRegX64 tmpMin{regs, SizeX64::qword};
            build.mov64(tmpMin.reg, INT64_MIN);
            build.cmp(tmpA.reg, tmpMin.reg);
            jumpOrAbortOnUndef(ConditionX64::Equal, OP_C(inst), next);

            build.setLabel(skip);
        }
        break;
    }
    case IrCmd::CHECK_CMP_INT64:
    {
        IrCondition cond = conditionOp(OP_C(inst));

        if ((cond == IrCondition::Equal || cond == IrCondition::NotEqual) && OP_B(inst).kind == IrOpKind::Constant && int64Op(OP_B(inst)) == 0)
        {
            build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
            jumpOrAbortOnUndef(cond == IrCondition::Equal ? ConditionX64::NotZero : ConditionX64::Zero, OP_D(inst), next);
        }
        else if (OP_A(inst).kind == IrOpKind::Constant)
        {
            ScopedRegX64 tmp{regs, SizeX64::qword};
            build.mov(tmp.reg, memRegInt64Op(OP_A(inst)));
            build.cmp(tmp.reg, memRegInt64Op(OP_B(inst)));
            jumpOrAbortOnUndef(getConditionInt(getNegatedCondition(cond)), OP_D(inst), next);
        }
        else
        {
            build.cmp(regOp(OP_A(inst)), memRegInt64Op(OP_B(inst)));
            jumpOrAbortOnUndef(getConditionInt(getNegatedCondition(cond)), OP_D(inst), next);
        }
        break;
    }

    case IrCmd::CMP_INT64:
    {
        // cannot reuse operand registers as a target because we have to modify it before the comparison
        inst.regX64 = regs.allocReg(SizeX64::dword, index);

        // We are going to operate on byte register, those do not clear high bits on write
        build.xor_(inst.regX64, inst.regX64);

        IrCondition cond = conditionOp(OP_C(inst));

        if (OP_A(inst).kind == IrOpKind::Constant)
        {
            build.cmp(regOp(OP_B(inst)), memRegInt64Op(OP_A(inst)));
            build.setcc(getInverseCondition(getConditionInt(cond)), byteReg(inst.regX64));
        }
        else if (OP_A(inst).kind == IrOpKind::Inst)
        {
            build.cmp(regOp(OP_A(inst)), memRegInt64Op(OP_B(inst)));
            build.setcc(getConditionInt(cond), byteReg(inst.regX64));
        }
        else
        {
            CODEGEN_ASSERT(!"Unsupported instruction form");
        }
        break;
    }
    case IrCmd::INT64_TO_NUM:
        inst.regX64 = regs.allocReg(SizeX64::xmmword, index);

        build.vcvtsi2sd(inst.regX64, inst.regX64, regOp(OP_A(inst)));
        break;
    case IrCmd::NUM_TO_INT64:
        inst.regX64 = regs.allocReg(SizeX64::qword, index);

        build.vcvttsd2si(inst.regX64, memRegDoubleOp(OP_A(inst)));
        break;
    case IrCmd::BITAND_INT64:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        build.and_(inst.regX64, memRegInt64Op(OP_B(inst)));
        break;
    case IrCmd::BITXOR_INT64:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        build.xor_(inst.regX64, memRegInt64Op(OP_B(inst)));
        break;
    case IrCmd::BITOR_INT64:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        build.or_(inst.regX64, memRegInt64Op(OP_B(inst)));
        break;
    case IrCmd::BITNOT_INT64:
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        build.not_(inst.regX64);
        break;
    case IrCmd::BITLSHIFT_INT64:
    {
        ScopedRegX64 shiftTmp{regs};

        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(rcx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t shift = int64Op(OP_B(inst));

            if (shift < 0)
            {
                // Negative left shift = right shift by -amount
                uint8_t amount = uint8_t(-shift);
                if (amount > 63)
                    build.xor_(inst.regX64, inst.regX64);
                else
                    build.shr(inst.regX64, int8_t(amount));
            }
            else if (shift > 63)
            {
                build.xor_(inst.regX64, inst.regX64);
            }
            else
            {
                build.shl(inst.regX64, int8_t(shift));
            }
        }
        else
        {
            ScopedRegX64 tmp{regs, SizeX64::qword};

            Label negative, outOfRange, done;

            build.mov(shiftTmp.reg, memRegInt64Op(OP_B(inst)));

            // Check |amount| > 63: (amount + 63) unsigned > 126
            build.lea(tmp.reg, addr[shiftTmp.reg + 63]);
            build.cmp(tmp.reg, 126);
            build.jcc(ConditionX64::Above, outOfRange);

            // Check sign of amount
            build.test(shiftTmp.reg, shiftTmp.reg);
            build.jcc(ConditionX64::Less, negative);

            // Left shift
            build.shl(inst.regX64, byteReg(shiftTmp.reg));
            build.jmp(done);

            // Right shift by -amount
            build.setLabel(negative);
            build.neg(shiftTmp.reg);
            build.shr(inst.regX64, byteReg(shiftTmp.reg));
            build.jmp(done);

            build.setLabel(outOfRange);
            build.xor_(inst.regX64, inst.regX64);

            build.setLabel(done);
        }

        break;
    }
    case IrCmd::BITRSHIFT_INT64:
    {
        ScopedRegX64 shiftTmp{regs};

        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(rcx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t shift = int64Op(OP_B(inst));

            if (shift < 0)
            {
                // Negative right shift = left shift by -amount
                uint8_t amount = uint8_t(-shift);
                if (amount > 63)
                    build.xor_(inst.regX64, inst.regX64);
                else
                    build.shl(inst.regX64, int8_t(amount));
            }
            else if (shift > 63)
            {
                build.xor_(inst.regX64, inst.regX64);
            }
            else
            {
                build.shr(inst.regX64, int8_t(shift));
            }
        }
        else
        {
            ScopedRegX64 tmp{regs, SizeX64::qword};

            Label negative, outOfRange, done;

            build.mov(shiftTmp.reg, memRegInt64Op(OP_B(inst)));

            // Check |amount| > 63: (amount + 63) unsigned > 126
            build.lea(tmp.reg, addr[shiftTmp.reg + 63]);
            build.cmp(tmp.reg, 126);
            build.jcc(ConditionX64::Above, outOfRange);

            // Check sign of amount
            build.test(shiftTmp.reg, shiftTmp.reg);
            build.jcc(ConditionX64::Less, negative);

            // Unsigned right shift
            build.shr(inst.regX64, byteReg(shiftTmp.reg));
            build.jmp(done);

            // Left shift by -amount
            build.setLabel(negative);
            build.neg(shiftTmp.reg);
            build.shl(inst.regX64, byteReg(shiftTmp.reg));
            build.jmp(done);

            build.setLabel(outOfRange);
            build.xor_(inst.regX64, inst.regX64);

            build.setLabel(done);
        }

        break;
    }
    case IrCmd::BITARSHIFT_INT64:
    {
        ScopedRegX64 shiftTmp{regs};

        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(rcx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            int64_t shift = int64Op(OP_B(inst));

            if (shift < -63)
            {
                // Left shift by > 63 = 0
                build.xor_(inst.regX64, inst.regX64);
            }
            else if (shift < 0)
            {
                // Negative arshift = left shift by -amount
                build.shl(inst.regX64, int8_t(-shift));
            }
            else if (shift > 63)
            {
                // Arithmetic right shift by > 63 = sign-fill
                build.sar(inst.regX64, int8_t(63));
            }
            else
            {
                build.sar(inst.regX64, int8_t(shift));
            }
        }
        else
        {
            ScopedRegX64 tmp{regs, SizeX64::qword};

            Label negative, outOfRangePositive, outOfRangeNegative, done;

            build.mov(shiftTmp.reg, memRegInt64Op(OP_B(inst)));

            // amount > 63: sign-fill
            build.cmp(shiftTmp.reg, 63);
            build.jcc(ConditionX64::Greater, outOfRangePositive);

            // Check amount < -63: (amount + 63) < 0
            build.lea(tmp.reg, addr[shiftTmp.reg + 63]);
            build.test(tmp.reg, tmp.reg);
            build.jcc(ConditionX64::Less, outOfRangeNegative);

            // Check sign of amount
            build.test(shiftTmp.reg, shiftTmp.reg);
            build.jcc(ConditionX64::Less, negative);

            // Arithmetic right shift
            build.sar(inst.regX64, byteReg(shiftTmp.reg));
            build.jmp(done);

            // Left shift by -amount
            build.setLabel(negative);
            build.neg(shiftTmp.reg);
            build.shl(inst.regX64, byteReg(shiftTmp.reg));
            build.jmp(done);

            // amount > 63: sign-fill (n < 0 ? -1 : 0)
            build.setLabel(outOfRangePositive);
            build.sar(inst.regX64, int8_t(63));
            build.jmp(done);

            // amount < -63: result is 0
            build.setLabel(outOfRangeNegative);
            build.xor_(inst.regX64, inst.regX64);

            build.setLabel(done);
        }

        break;
    }
    case IrCmd::BITLROTATE_INT64:
    {
        ScopedRegX64 shiftTmp{regs};

        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(rcx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            int8_t shift = int8_t(unsigned(int64Op(OP_B(inst))));
            build.rol(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegInt64Op(OP_B(inst)));
            build.rol(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITRROTATE_INT64:
    {
        ScopedRegX64 shiftTmp{regs};

        if (OP_B(inst).kind != IrOpKind::Constant)
            shiftTmp.take(rcx);

        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        if (OP_B(inst).kind == IrOpKind::Constant)
        {
            int8_t shift = int8_t(unsigned(int64Op(OP_B(inst))));
            build.ror(inst.regX64, shift);
        }
        else
        {
            build.mov(shiftTmp.reg, memRegInt64Op(OP_B(inst)));
            build.ror(inst.regX64, byteReg(shiftTmp.reg));
        }

        break;
    }
    case IrCmd::BITCOUNTLZ_INT64:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        Label zero, exit;

        build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
        build.jcc(ConditionX64::Equal, zero);

        build.bsr(inst.regX64, regOp(OP_A(inst)));
        build.xor_(inst.regX64, 0x3f);
        build.jmp(exit);

        build.setLabel(zero);
        build.mov(inst.regX64, 64);

        build.setLabel(exit);
        break;
    }
    case IrCmd::BITCOUNTRZ_INT64:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        Label zero, exit;

        build.test(regOp(OP_A(inst)), regOp(OP_A(inst)));
        build.jcc(ConditionX64::Equal, zero);

        build.bsf(inst.regX64, regOp(OP_A(inst)));
        build.jmp(exit);

        build.setLabel(zero);
        build.mov(inst.regX64, 64);

        build.setLabel(exit);
        break;
    }
    case IrCmd::BYTESWAP_INT64:
    {
        inst.regX64 = regs.allocRegOrReuse(SizeX64::qword, index, {OP_A(inst)});

        if (OP_A(inst).kind != IrOpKind::Inst || inst.regX64 != regOp(OP_A(inst)))
            build.mov(inst.regX64, memRegInt64Op(OP_A(inst)));

        build.bswap(inst.regX64);
        break;
    }

    // Pseudo instructions
    case IrCmd::NOP:
    case IrCmd::SUBSTITUTE:
    case IrCmd::MARK_USED:
    case IrCmd::MARK_DEAD:
        CODEGEN_ASSERT(!"Pseudo instructions should not be lowered");
        break;
    }

    valueTracker.afterInstLowering(inst, index);

    regs.currInstIdx = kInvalidInstIdx;

    regs.freeLastUseRegs(inst, index);
}

void IrLoweringX64::startBlock(const IrBlock& curr)
{
    if (curr.startpc != kBlockNoStartPc)
        allocAndIncrementCounterAt(
            curr.kind == IrBlockKind::Fallback ? CodeGenCounter::FallbackBlockExecuted : CodeGenCounter::RegularBlockExecuted, curr.startpc
        );
}

void IrLoweringX64::finishBlock(const IrBlock& curr, const IrBlock& next)
{
    if (!regs.spills.empty())
    {
        // If we have spills remaining, we have to immediately lower the successor block
        for (uint32_t predIdx : predecessors(function.cfg, function.getBlockIndex(next)))
            CODEGEN_ASSERT(predIdx == function.getBlockIndex(curr) || function.blocks[predIdx].kind == IrBlockKind::Dead);

        // And the next block cannot be a join block in cfg
        CODEGEN_ASSERT(next.useCount == 1);
    }
}

void IrLoweringX64::finishFunction()
{
    if (build.logText)
        build.logAppend("; interrupt handlers\n");

    for (InterruptHandler& handler : interruptHandlers)
    {
        build.setLabel(handler.self);
        build.mov(eax, handler.pcpos + 1);
        build.lea(rbx, handler.next);
        build.jmp(helpers.interrupt);
    }

    if (build.logText)
        build.logAppend("; exit handlers\n");

    for (ExitHandler& handler : exitHandlers)
    {
        if (handler.pcpos == kVmExitEntryGuardPc)
        {
            build.setLabel(handler.self);

            allocAndIncrementCounterAt(CodeGenCounter::VmExitTaken, ~0u);

            build.jmp(helpers.exitContinueVmClearNativeFlag);
        }
        else
        {
            build.setLabel(handler.self);

            allocAndIncrementCounterAt(CodeGenCounter::VmExitTaken, handler.pcpos);

            build.mov(edx, handler.pcpos * sizeof(Instruction));
            build.jmp(helpers.updatePcAndContinueInVm);
        }
    }

    // An undefined instruction is placed after the function to be used as an aborting jump offset
    function.endLocation = build.getLabelOffset(build.setLabel());
    build.ud2();

    if (stats)
    {
        if (regs.maxUsedSlot > (FFlag::LuauCodegenNewRegSplit ? kSpillSlots : kSpillSlots_NEW) + kExtraSpillSlots)
            stats->regAllocErrors++;

        if (regs.maxUsedSlot > stats->maxSpillSlotsUsed)
            stats->maxSpillSlotsUsed = regs.maxUsedSlot;
    }
}

bool IrLoweringX64::hasError() const
{
    // If register allocator had to use more stack slots than we have available, this function can't run natively
    if (regs.maxUsedSlot > (FFlag::LuauCodegenNewRegSplit ? kSpillSlots : kSpillSlots_NEW) + kExtraSpillSlots)
        return true;

    return false;
}

bool IrLoweringX64::isFallthroughBlock(const IrBlock& target, const IrBlock& next)
{
    return target.start == next.start;
}

Label& IrLoweringX64::getTargetLabel(IrOp op, Label& fresh)
{
    if (op.kind == IrOpKind::Undef)
        return fresh;

    if (op.kind == IrOpKind::VmExit)
    {
        if (uint32_t* index = exitHandlerMap.find(vmExitOp(op)))
            return exitHandlers[*index].self;

        return fresh;
    }

    return labelOp(op);
}

void IrLoweringX64::finalizeTargetLabel(IrOp op, Label& fresh)
{
    if (op.kind == IrOpKind::VmExit && fresh.id != 0)
    {
        exitHandlerMap[vmExitOp(op)] = uint32_t(exitHandlers.size());
        exitHandlers.push_back({fresh, vmExitOp(op)});
    }
}

void IrLoweringX64::jumpOrFallthrough(IrBlock& target, const IrBlock& next)
{
    if (!isFallthroughBlock(target, next))
        build.jmp(target.label);
}

void IrLoweringX64::jumpOrAbortOnUndef(ConditionX64 cond, IrOp target, const IrBlock& next)
{
    Label fresh;
    Label& label = getTargetLabel(target, fresh);

    if (target.kind == IrOpKind::Undef)
    {
        if (cond == ConditionX64::Count)
        {
            build.ud2(); // Unconditional jump to abort is just an abort
        }
        else
        {
            build.jcc(getNegatedCondition(cond), label);
            build.ud2();
            build.setLabel(label);
        }
    }
    else if (cond == ConditionX64::Count)
    {
        // Unconditional jump can be skipped if it's a fallthrough
        if (target.kind == IrOpKind::VmExit || !isFallthroughBlock(blockOp(target), next))
            build.jmp(label);
    }
    else
    {
        build.jcc(cond, label);
    }

    finalizeTargetLabel(target, fresh);
}

void IrLoweringX64::jumpOrAbortOnUndef(IrOp target, const IrBlock& next)
{
    jumpOrAbortOnUndef(ConditionX64::Count, target, next);
}

void IrLoweringX64::storeFloat(OperandX64 dst, IrOp src)
{
    if (src.kind == IrOpKind::Constant)
    {
        ScopedRegX64 tmp{regs, SizeX64::xmmword};
        build.vmovss(tmp.reg, build.f32(float(doubleOp(src))));
        build.vmovss(dst, tmp.reg);
    }
    else if (src.kind == IrOpKind::Inst)
    {
        CODEGEN_ASSERT(getCmdValueKind(function.instOp(src).cmd) == IrValueKind::Float);
        build.vmovss(dst, regOp(src));
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
    }
}

void IrLoweringX64::storeDoubleAsFloat(OperandX64 dst, IrOp src)
{
    ScopedRegX64 tmp{regs, SizeX64::xmmword};

    if (src.kind == IrOpKind::Constant)
    {
        build.vmovss(tmp.reg, build.f32(float(doubleOp(src))));
    }
    else if (src.kind == IrOpKind::Inst)
    {
        build.vcvtsd2ss(tmp.reg, regOp(src), regOp(src));
    }
    else
    {
        CODEGEN_ASSERT(!"Unsupported instruction form");
    }
    build.vmovss(dst, tmp.reg);
}

void IrLoweringX64::checkSafeEnv(IrOp target, const IrBlock& next)
{
    ScopedRegX64 tmp{regs, SizeX64::qword};

    build.mov(tmp.reg, sClosure);
    build.mov(tmp.reg, qword[tmp.reg + offsetof(Closure, env)]);
    build.cmp(byte[tmp.reg + offsetof(LuaTable, safeenv)], 0);

    jumpOrAbortOnUndef(ConditionX64::Equal, target, next);
}

void IrLoweringX64::allocAndIncrementCounterAt(CodeGenCounter kind, uint32_t pcpos)
{
    if (!function.recordCounters)
        return;

    if (build.logText)
        build.logAppend("; counter kind %u at pcpos %d\n", unsigned(kind), pcpos);

    // {uint32_t, uint32_t, uint64_t}
    function.extraNativeData.push_back(unsigned(kind));
    function.extraNativeData.push_back(pcpos);
    incrementCounterAt(function.extraNativeData.size());
    function.extraNativeData.push_back(0);
    function.extraNativeData.push_back(0);
}

void IrLoweringX64::incrementCounterAt(size_t offset)
{
    ScopedRegX64 tmp{regs, SizeX64::qword};

    // Get counter slot
    build.mov(tmp.reg, sClosure);
    build.mov(tmp.reg, qword[tmp.reg + offsetof(Closure, l.p)]);
    build.mov(tmp.reg, qword[tmp.reg + offsetof(Proto, execdata)]);

    // Increment
    build.inc(qword[tmp.reg + uint32_t(function.proto->sizecode + uint32_t(offset)) * 4]);
}

OperandX64 IrLoweringX64::memRegDoubleOp(IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::Inst:
        return regOp(op);
    case IrOpKind::Constant:
        return build.f64(doubleOp(op));
    case IrOpKind::VmReg:
        return luauRegValue(vmRegOp(op));
    case IrOpKind::VmConst:
        return luauConstantValue(vmConstOp(op));
    default:
        CODEGEN_ASSERT(!"Unsupported operand kind");
    }

    return noreg;
}

OperandX64 IrLoweringX64::memRegFloatOp(IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::Inst:
        CODEGEN_ASSERT(getCmdValueKind(function.instructions[op.index].cmd) == IrValueKind::Float);
        return regOp(op);
    case IrOpKind::Constant:
        return build.f32(float(doubleOp(op)));
    default:
        CODEGEN_ASSERT(!"Unsupported operand kind");
    }

    return noreg;
}

OperandX64 IrLoweringX64::memRegUintOp(IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::Inst:
        return regOp(op);
    case IrOpKind::Constant:
        return OperandX64(unsigned(intOp(op)));
    case IrOpKind::VmReg:
        return luauRegValueInt(vmRegOp(op));
    default:
        CODEGEN_ASSERT(!"Unsupported operand kind");
    }

    return noreg;
}

OperandX64 IrLoweringX64::memRegIntOp(IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::Inst:
        return regOp(op);
    case IrOpKind::Constant:
        return OperandX64(intOp(op));
    case IrOpKind::VmReg:
        return luauRegValueInt(vmRegOp(op));
    default:
        CODEGEN_ASSERT(!"Unsupported operand kind");
    }

    return noreg;
}

OperandX64 IrLoweringX64::memRegInt64Op(IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::Inst:
        return regOp(op);
    case IrOpKind::Constant:
        return build.i64(int64Op(op));
    case IrOpKind::VmReg:
        return luauRegValueInt64(vmRegOp(op));
    case IrOpKind::VmConst:
        return luauConstantValue(vmConstOp(op));
    default:
        CODEGEN_ASSERT(!"Unsupported operand kind");
    }

    return noreg;
}

OperandX64 IrLoweringX64::memRegTagOp(IrOp op)
{
    switch (op.kind)
    {
    case IrOpKind::Inst:
        return regOp(op);
    case IrOpKind::VmReg:
        return luauRegTag(vmRegOp(op));
    case IrOpKind::VmConst:
        return luauConstantTag(vmConstOp(op));
    default:
        CODEGEN_ASSERT(!"Unsupported operand kind");
    }

    return noreg;
}

RegisterX64 IrLoweringX64::regOp(IrOp op)
{
    IrInst& inst = function.instOp(op);

    if (inst.spilled || inst.needsReload)
        regs.restore(inst, false);

    CODEGEN_ASSERT(inst.regX64 != noreg);
    return inst.regX64;
}

OperandX64 IrLoweringX64::bufferAddrOp(IrOp bufferOp, IrOp indexOp, uint8_t tag)
{
    CODEGEN_ASSERT(tag == LUA_TUSERDATA || tag == LUA_TBUFFER);
    int dataOffset = tag == LUA_TBUFFER ? offsetof(Buffer, data) : offsetof(Udata, data);

    if (indexOp.kind == IrOpKind::Inst)
    {
        CODEGEN_ASSERT(!producesDirtyHighRegisterBits(function.instOp(indexOp).cmd)); // Ensure that high register bits are cleared

        return regOp(bufferOp) + qwordReg(regOp(indexOp)) + dataOffset;
    }
    else if (indexOp.kind == IrOpKind::Constant)
    {
        return regOp(bufferOp) + intOp(indexOp) + dataOffset;
    }

    CODEGEN_ASSERT(!"Unsupported instruction form");
    return noreg;
}

RegisterX64 IrLoweringX64::vecOp(IrOp op, ScopedRegX64& tmp)
{
    IrInst source = function.instOp(op);
    CODEGEN_ASSERT(source.cmd != IrCmd::SUBSTITUTE); // we don't process substitutions

    // source that comes from memory or from tag instruction has .w = TVECTOR, which is denormal
    // to avoid performance degradation on some CPUs we mask this component to produce zero
    // otherwise we conservatively assume the vector is a result of a well formed math op so .w is a normal number or zero
    if (source.cmd != IrCmd::LOAD_TVALUE && source.cmd != IrCmd::GET_UPVALUE && source.cmd != IrCmd::TAG_VECTOR)
        return regOp(op);

    tmp.alloc(SizeX64::xmmword);
    build.vandps(tmp.reg, regOp(op), vectorAndMaskOp());
    return tmp.reg;
}

IrConst IrLoweringX64::constOp(IrOp op) const
{
    return function.constOp(op);
}

uint8_t IrLoweringX64::tagOp(IrOp op) const
{
    return function.tagOp(op);
}

int IrLoweringX64::intOp(IrOp op) const
{
    return function.intOp(op);
}

int64_t IrLoweringX64::int64Op(IrOp op) const
{
    return function.int64Op(op);
}

unsigned IrLoweringX64::uintOp(IrOp op) const
{
    return function.uintOp(op);
}

unsigned IrLoweringX64::importOp(IrOp op) const
{
    return function.importOp(op);
}

double IrLoweringX64::doubleOp(IrOp op) const
{
    return function.doubleOp(op);
}

IrBlock& IrLoweringX64::blockOp(IrOp op) const
{
    return function.blockOp(op);
}

Label& IrLoweringX64::labelOp(IrOp op) const
{
    return blockOp(op).label;
}

OperandX64 IrLoweringX64::vectorAndMaskOp()
{
    if (vectorAndMask.base == noreg)
        vectorAndMask = build.u32x4(~0u, ~0u, ~0u, 0);

    return vectorAndMask;
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
