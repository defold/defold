// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrValueLocationTracking.h"

#include "Luau/IrUtils.h"

LUAU_FASTFLAGVARIABLE(LuauCodegenFastcallInvokeRange)

namespace Luau
{
namespace CodeGen
{

IrValueLocationTracking::IrValueLocationTracking(IrFunction& function)
    : function(function)
{
    vmRegValue.fill(kInvalidInstIdx);
}

void IrValueLocationTracking::setRestoreCallback(void* context, void (*callback)(void* context, IrInst& inst))
{
    restoreCallbackCtx = context;
    restoreCallback = callback;
}

bool IrValueLocationTracking::canBeRematerialized(IrCmd cmd)
{
    return cmd == IrCmd::UINT_TO_NUM || cmd == IrCmd::INT_TO_NUM;
}

bool IrValueLocationTracking::canRematerializeArguments(IrInst& inst)
{
    if (canBeRematerialized(inst.cmd) && OP_A(inst).kind == IrOpKind::Inst)
    {
        IrInst& depInst = function.instOp(OP_A(inst));

        // If this argument is last used in the current instructions, there's no point in preserving it
        if (depInst.lastUse != function.getInstIndex(inst))
            return true;
    }

    return false;
}

void IrValueLocationTracking::beforeInstLowering(IrInst& inst)
{
    switch (inst.cmd)
    {
    case IrCmd::STORE_TAG:
        // Tag update is a bit tricky, restore operations of values are not affected
        invalidateRestoreOp(OP_A(inst), /*skipValueInvalidation*/ true);
        break;
    case IrCmd::STORE_EXTRA:
        // While extra field update doesn't invalidate some of the values, it can invalidate a vector type field
        invalidateRestoreOp(OP_A(inst), /*skipValueInvalidation*/ false);
        break;
    case IrCmd::STORE_POINTER:
    case IrCmd::STORE_DOUBLE:
    case IrCmd::STORE_INT:
    case IrCmd::STORE_INT64:
    case IrCmd::STORE_VECTOR:
    case IrCmd::STORE_TVALUE:
    case IrCmd::STORE_SPLIT_TVALUE:
        invalidateRestoreOp(OP_A(inst), /*skipValueInvalidation*/ false);
        break;
    case IrCmd::ADJUST_STACK_TO_REG:
        invalidateRestoreVmRegs(vmRegOp(OP_A(inst)), -1);
        break;
    case IrCmd::FASTCALL:
        invalidateRestoreVmRegs(vmRegOp(OP_B(inst)), function.intOp(OP_D(inst)));
        break;
    case IrCmd::INVOKE_FASTCALL:
        if (FFlag::LuauCodegenFastcallInvokeRange)
        {
            // While ADJUST_STACK_TO_REG would semantically define the result range, we need to define it immediately
            invalidateRestoreVmRegs(vmRegOp(OP_B(inst)), function.intOp(OP_G(inst)));
        }
        else
        {
            // Multiple return sequences (count == -1) are defined by ADJUST_STACK_TO_REG
            if (int count = function.intOp(OP_G(inst)); count != -1)
                invalidateRestoreVmRegs(vmRegOp(OP_B(inst)), count);
        }
        break;
    case IrCmd::DO_ARITH:
    case IrCmd::DO_LEN:
    case IrCmd::GET_TABLE:
    case IrCmd::GET_CACHED_IMPORT:
        invalidateRestoreOp(OP_A(inst), /*skipValueInvalidation*/ false);
        break;
    case IrCmd::CONCAT:
        invalidateRestoreVmRegs(vmRegOp(OP_A(inst)), function.uintOp(OP_B(inst)));
        break;
    case IrCmd::GET_UPVALUE:
        break;
    case IrCmd::CALL:
        // Even if result count is limited, all registers starting from function (ra) might be modified
        invalidateRestoreVmRegs(vmRegOp(OP_A(inst)), -1);
        break;
    case IrCmd::FORGLOOP:
    case IrCmd::FORGLOOP_FALLBACK:
        // Even if result count is limited, all registers starting from iteration index (ra+2) might be modified
        invalidateRestoreVmRegs(vmRegOp(OP_A(inst)) + 2, -1);
        break;
    case IrCmd::FALLBACK_GETGLOBAL:
    case IrCmd::FALLBACK_GETTABLEKS:
        invalidateRestoreOp(OP_B(inst), /*skipValueInvalidation*/ false);
        break;
    case IrCmd::FALLBACK_NAMECALL:
        invalidateRestoreVmRegs(vmRegOp(OP_B(inst)), 2);
        break;
    case IrCmd::FALLBACK_GETVARARGS:
        invalidateRestoreVmRegs(vmRegOp(OP_B(inst)), function.intOp(OP_C(inst)));
        break;
    case IrCmd::FALLBACK_DUPCLOSURE:
        invalidateRestoreOp(OP_B(inst), /*skipValueInvalidation*/ false);
        break;
    case IrCmd::FALLBACK_FORGPREP:
        invalidateRestoreVmRegs(vmRegOp(OP_B(inst)), 3);
        break;

        // Make sure all VmReg referencing instructions are handled explicitly (only register reads here)
    case IrCmd::LOAD_TAG:
    case IrCmd::LOAD_POINTER:
    case IrCmd::LOAD_DOUBLE:
    case IrCmd::LOAD_INT64:
    case IrCmd::LOAD_INT:
    case IrCmd::LOAD_FLOAT:
    case IrCmd::LOAD_TVALUE:
    case IrCmd::CMP_ANY:
    case IrCmd::CMP_TAG:
    case IrCmd::JUMP_IF_TRUTHY:
    case IrCmd::JUMP_IF_FALSY:
    case IrCmd::JUMP_EQ_TAG:
    case IrCmd::SELECT_INT64:
    case IrCmd::SET_TABLE:
    case IrCmd::SET_UPVALUE:
    case IrCmd::INTERRUPT:
    case IrCmd::BARRIER_OBJ:
    case IrCmd::BARRIER_TABLE_FORWARD:
    case IrCmd::CLOSE_UPVALS:
    case IrCmd::CAPTURE:
    case IrCmd::SETLIST:
    case IrCmd::RETURN:
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
    case IrCmd::FALLBACK_SETGLOBAL:
    case IrCmd::FALLBACK_SETTABLEKS:
    case IrCmd::FALLBACK_PREPVARARGS:
    case IrCmd::ADJUST_STACK_TO_TOP:
    case IrCmd::GET_TYPEOF:
    case IrCmd::NEWCLOSURE:
    case IrCmd::FINDUPVAL:
        break;

        // These instructions read VmReg only after optimizeMemoryOperandsX64
    case IrCmd::CHECK_TAG:
    case IrCmd::CHECK_TRUTHY:
    case IrCmd::ADD_NUM:
    case IrCmd::SUB_NUM:
    case IrCmd::MUL_NUM:
    case IrCmd::DIV_NUM:
    case IrCmd::IDIV_NUM:
    case IrCmd::MOD_NUM:
    case IrCmd::MIN_NUM:
    case IrCmd::MAX_NUM:
    case IrCmd::JUMP_CMP_NUM:
    case IrCmd::FLOOR_NUM:
    case IrCmd::CEIL_NUM:
    case IrCmd::ROUND_NUM:
    case IrCmd::SQRT_NUM:
    case IrCmd::ABS_NUM:
        break;

    default:
        // All instructions which reference registers have to be handled explicitly
        for (auto& op : inst.ops)
            CODEGEN_ASSERT(op.kind != IrOpKind::VmReg);
        break;
    }
}

void IrValueLocationTracking::afterInstLowering(IrInst& inst, uint32_t instIdx)
{
    switch (inst.cmd)
    {
    case IrCmd::LOAD_TAG:
    case IrCmd::LOAD_POINTER:
    case IrCmd::LOAD_DOUBLE:
    case IrCmd::LOAD_INT:
    case IrCmd::LOAD_INT64:
    case IrCmd::LOAD_TVALUE:
        if (OP_A(inst).kind == IrOpKind::VmReg)
            invalidateRestoreOp(OP_A(inst), /*skipValueInvalidation*/ false);

        recordRestoreOp(instIdx, OP_A(inst));
        break;
    case IrCmd::STORE_POINTER:
    case IrCmd::STORE_DOUBLE:
    case IrCmd::STORE_INT:
    case IrCmd::STORE_INT64:
    case IrCmd::STORE_TVALUE:
        // If this is not the last use of the stored value, we can restore it from this new location
        // Additionally, even if it's a last use, it might allow its argument to be restored
        if (OP_B(inst).kind == IrOpKind::Inst)
        {
            IrInst& source = function.instOp(OP_B(inst));

            if (source.lastUse != instIdx || canRematerializeArguments(source))
                recordRestoreOp(OP_B(inst).index, OP_A(inst));
        }
        break;
    case IrCmd::STORE_SPLIT_TVALUE:
        // If this is not the last use of the stored value, we can restore it from this new location
        // Additionally, even if it's a last use, it might allow its argument to be restored
        if (OP_C(inst).kind == IrOpKind::Inst)
        {
            IrInst& source = function.instOp(OP_C(inst));

            if (source.lastUse != instIdx || canRematerializeArguments(source))
                recordRestoreOp(OP_C(inst).index, OP_A(inst));
        }
        break;
    default:
        break;
    }
}

void IrValueLocationTracking::recordRestoreOp(uint32_t instIdx, IrOp location)
{
    IrInst& inst = function.instructions[instIdx];

    if (location.kind == IrOpKind::VmReg)
    {
        int reg = vmRegOp(location);

        if (reg > maxReg)
            maxReg = reg;

        // Record location in register memory only if register is not captured
        bool captured = function.cfg.captured.regs.test(reg);

        if (!captured)
            function.recordRestoreLocation(instIdx, {location, getCmdValueKind(inst.cmd), IrCmd::NOP});

        vmRegValue[reg] = instIdx;

        if (canBeRematerialized(inst.cmd) && OP_A(inst).kind == IrOpKind::Inst)
        {
            uint32_t depInstIdx = OP_A(inst).index;

            if (!captured)
                function.recordRestoreLocation(depInstIdx, {location, getCmdValueKind(inst.cmd), inst.cmd});
        }
    }
    else if (location.kind == IrOpKind::VmConst)
    {
        function.recordRestoreLocation(instIdx, {location, getCmdValueKind(inst.cmd)});
    }
}

void IrValueLocationTracking::invalidateRestoreOp(IrOp location, bool skipValueInvalidation)
{
    if (location.kind == IrOpKind::VmReg)
    {
        uint32_t& instIdx = vmRegValue[vmRegOp(location)];

        if (instIdx != kInvalidInstIdx)
        {
            IrInst& inst = function.instructions[instIdx];

            // If we are only modifying the tag, we can avoid invalidating tracked location of values
            if (skipValueInvalidation)
            {
                switch (getCmdValueKind(inst.cmd))
                {
                case IrValueKind::Double:
                case IrValueKind::Pointer:
                case IrValueKind::Int:
                case IrValueKind::Int64:
                    return;
                default:
                    break;
                }
            }

            // If instruction value is spilled and memory location is about to be lost, it has to be restored immediately
            if (inst.needsReload)
                restoreCallback(restoreCallbackCtx, inst);

            // Get the current restore location of the instruction
            ValueRestoreLocation currRestoreLocation = function.findRestoreLocation(instIdx, /* limitToCurrentBlock */ false);

            // If the current location is the one that is being invalidated, we can no longer restore from it
            if (location == currRestoreLocation.op)
                function.recordRestoreLocation(instIdx, {});

            // Register loses link with instruction
            instIdx = kInvalidInstIdx;

            // Chained instruction special case
            if (canBeRematerialized(inst.cmd) && OP_A(inst).kind == IrOpKind::Inst)
            {
                uint32_t depInstIdx = OP_A(inst).index;
                IrInst& depInst = function.instructions[depInstIdx];

                if (depInst.needsReload)
                    restoreCallback(restoreCallbackCtx, depInst);

                if (location == currRestoreLocation.op)
                    function.recordRestoreLocation(depInstIdx, {});
            }
        }
    }
    else if (location.kind == IrOpKind::VmConst)
    {
        CODEGEN_ASSERT(!"VM constants are immutable");
    }
}

void IrValueLocationTracking::invalidateRestoreVmRegs(int start, int count)
{
    int end = count == -1 ? 255 : start + count;

    if (end > maxReg)
        end = maxReg;

    for (int reg = start; reg <= end; reg++)
        invalidateRestoreOp(IrOp{IrOpKind::VmReg, uint8_t(reg)}, /*skipValueInvalidation*/ false);
}

} // namespace CodeGen
} // namespace Luau
