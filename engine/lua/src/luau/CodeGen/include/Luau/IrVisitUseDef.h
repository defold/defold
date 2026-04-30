// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Common.h"
#include "Luau/IrData.h"

LUAU_FASTFLAG(LuauCodegenFastcallInvokeRange)

namespace Luau
{
namespace CodeGen
{

template<typename T>
static void visitVmRegDefsUses(T& visitor, IrFunction& function, IrInst& inst)
{
    // For correct analysis, all instruction uses must be handled before handling the definitions
    switch (inst.cmd)
    {
    case IrCmd::LOAD_TAG:
    case IrCmd::LOAD_POINTER:
    case IrCmd::LOAD_DOUBLE:
    case IrCmd::LOAD_INT:
    case IrCmd::LOAD_INT64:
    case IrCmd::LOAD_FLOAT:
    case IrCmd::LOAD_TVALUE:
        visitor.maybeUse(OP_A(inst)); // Argument can also be a VmConst
        break;
    case IrCmd::STORE_TAG:
    case IrCmd::STORE_EXTRA:
    case IrCmd::STORE_POINTER:
    case IrCmd::STORE_DOUBLE:
    case IrCmd::STORE_INT:
    case IrCmd::STORE_INT64:
    case IrCmd::STORE_VECTOR:
    case IrCmd::STORE_TVALUE:
    case IrCmd::STORE_SPLIT_TVALUE:
        visitor.maybeDef(OP_A(inst)); // Argument can also be a pointer value
        break;
    case IrCmd::CMP_ANY:
        visitor.use(OP_A(inst));
        visitor.use(OP_B(inst));
        break;
    case IrCmd::CMP_TAG:
        visitor.maybeUse(OP_A(inst));
        break;
    case IrCmd::JUMP_IF_TRUTHY:
    case IrCmd::JUMP_IF_FALSY:
        visitor.use(OP_A(inst));
        break;
    case IrCmd::JUMP_EQ_TAG:
        visitor.maybeUse(OP_A(inst));
        break;
        // A <- B, C
    case IrCmd::DO_ARITH:
        visitor.maybeUse(OP_B(inst)); // Argument can also be a VmConst
        visitor.maybeUse(OP_C(inst)); // Argument can also be a VmConst

        visitor.def(OP_A(inst));
        break;
    case IrCmd::GET_TABLE:
        visitor.use(OP_B(inst));
        visitor.maybeUse(OP_C(inst)); // Argument can also be a VmConst

        visitor.def(OP_A(inst));
        break;
    case IrCmd::SET_TABLE:
        visitor.use(OP_A(inst));
        visitor.use(OP_B(inst));
        visitor.maybeUse(OP_C(inst)); // Argument can also be a VmConst
        break;
        // A <- B
    case IrCmd::DO_LEN:
        visitor.use(OP_B(inst));

        visitor.def(OP_A(inst));
        break;
    case IrCmd::GET_CACHED_IMPORT:
        visitor.def(OP_A(inst));
        break;
    case IrCmd::CONCAT:
        visitor.useRange(vmRegOp(OP_A(inst)), function.uintOp(OP_B(inst)));

        visitor.defRange(vmRegOp(OP_A(inst)), function.uintOp(OP_B(inst)));
        break;
    case IrCmd::GET_UPVALUE:
        break;
    case IrCmd::SET_UPVALUE:
        break;
    case IrCmd::INTERRUPT:
        break;
    case IrCmd::BARRIER_OBJ:
    case IrCmd::BARRIER_TABLE_FORWARD:
        visitor.maybeUse(OP_B(inst));
        break;
    case IrCmd::CLOSE_UPVALS:
        // Closing an upvalue should be counted as a register use (it copies the fresh register value)
        // But we lack the required information about the specific set of registers that are affected
        // Because we don't plan to optimize captured registers atm, we skip full dataflow analysis for them right now
        break;
    case IrCmd::CAPTURE:
        visitor.maybeUse(OP_A(inst));

        if (function.uintOp(OP_B(inst)) == 1)
            visitor.capture(vmRegOp(OP_A(inst)));
        break;
    case IrCmd::SETLIST:
        visitor.use(OP_B(inst));
        visitor.useRange(vmRegOp(OP_C(inst)), function.intOp(OP_D(inst)));
        break;
    case IrCmd::CALL:
        visitor.use(OP_A(inst));
        visitor.useRange(vmRegOp(OP_A(inst)) + 1, function.intOp(OP_B(inst)));

        visitor.defRange(vmRegOp(OP_A(inst)), function.intOp(OP_C(inst)));
        break;
    case IrCmd::RETURN:
        visitor.useRange(vmRegOp(OP_A(inst)), function.intOp(OP_B(inst)));
        break;

    case IrCmd::FASTCALL:
        visitor.use(OP_C(inst));

        if (FFlag::LuauCodegenFastcallInvokeRange)
        {
            visitor.defRange(vmRegOp(OP_B(inst)), function.intOp(OP_D(inst)));
        }
        else
        {
            if (int nresults = function.intOp(OP_D(inst)); nresults != -1)
                visitor.defRange(vmRegOp(OP_B(inst)), nresults);
        }
        break;
    case IrCmd::INVOKE_FASTCALL:
        if (int count = function.intOp(OP_F(inst)); count != -1)
        {
            // Only LOP_FASTCALL3 lowering is allowed to have third optional argument
            if (count >= 3 && OP_E(inst).kind == IrOpKind::Undef)
            {
                CODEGEN_ASSERT(OP_D(inst).kind == IrOpKind::VmReg && vmRegOp(OP_D(inst)) == vmRegOp(OP_C(inst)) + 1);

                visitor.useRange(vmRegOp(OP_C(inst)), count);
            }
            else
            {
                if (count >= 1)
                    visitor.use(OP_C(inst));

                if (count >= 2)
                    visitor.maybeUse(OP_D(inst)); // Argument can also be a VmConst

                if (count >= 3)
                    visitor.maybeUse(OP_E(inst)); // Argument can also be a VmConst
            }
        }
        else
        {
            visitor.useVarargs(vmRegOp(OP_C(inst)));
        }

        if (FFlag::LuauCodegenFastcallInvokeRange)
        {
            // While ADJUST_STACK_TO_REG would semantically define the result range, we need to define it immediately
            visitor.defRange(vmRegOp(OP_B(inst)), function.intOp(OP_G(inst)));
        }
        else
        {
            // Multiple return sequences (count == -1) are defined by ADJUST_STACK_TO_REG
            if (int count = function.intOp(OP_G(inst)); count != -1)
                visitor.defRange(vmRegOp(OP_B(inst)), count);
        }
        break;
    case IrCmd::FORGLOOP:
        // First register is not used by instruction, we check that it's still 'nil' with CHECK_TAG
        visitor.use(OP_A(inst), 1);
        visitor.use(OP_A(inst), 2);

        visitor.def(OP_A(inst), 2);
        visitor.defRange(vmRegOp(OP_A(inst)) + 3, function.intOp(OP_B(inst)));
        break;
    case IrCmd::FORGLOOP_FALLBACK:
        visitor.useRange(vmRegOp(OP_A(inst)), 3);

        visitor.def(OP_A(inst), 2);
        visitor.defRange(vmRegOp(OP_A(inst)) + 3, uint8_t(function.intOp(OP_B(inst)))); // ignore most significant bit
        break;
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
        visitor.use(OP_B(inst));
        break;
    case IrCmd::FALLBACK_GETGLOBAL:
        visitor.def(OP_B(inst));
        break;
    case IrCmd::FALLBACK_SETGLOBAL:
        visitor.use(OP_B(inst));
        break;
    case IrCmd::FALLBACK_GETTABLEKS:
        visitor.use(OP_C(inst));

        visitor.def(OP_B(inst));
        break;
    case IrCmd::FALLBACK_SETTABLEKS:
        visitor.use(OP_B(inst));
        visitor.use(OP_C(inst));
        break;
    case IrCmd::FALLBACK_NAMECALL:
        visitor.use(OP_C(inst));

        visitor.defRange(vmRegOp(OP_B(inst)), 2);
        break;
    case IrCmd::FALLBACK_PREPVARARGS:
        // No effect on explicitly referenced registers
        break;
    case IrCmd::FALLBACK_GETVARARGS:
        visitor.defRange(vmRegOp(OP_B(inst)), function.intOp(OP_C(inst)));
        break;
    case IrCmd::FALLBACK_DUPCLOSURE:
        visitor.def(OP_B(inst));
        break;
    case IrCmd::FALLBACK_FORGPREP:
        // This instruction doesn't always redefine Rn, Rn+1, Rn+2, so we have to mark it as implicit use
        visitor.useRange(vmRegOp(OP_B(inst)), 3);

        visitor.defRange(vmRegOp(OP_B(inst)), 3);
        break;
    case IrCmd::ADJUST_STACK_TO_REG:
        visitor.defRange(vmRegOp(OP_A(inst)), -1);
        break;
    case IrCmd::ADJUST_STACK_TO_TOP:
        // While this can be considered to be a vararg consumer, it is already handled in fastcall instructions
        break;
    case IrCmd::GET_TYPEOF:
        visitor.use(OP_A(inst));
        break;

    case IrCmd::FINDUPVAL:
        visitor.use(OP_A(inst));
        break;

    case IrCmd::MARK_USED:
        visitor.useRange(vmRegOp(OP_A(inst)), function.intOp(OP_B(inst)));
        break;
    case IrCmd::MARK_DEAD:
        // Does not affect VM def/use info
        break;

    default:
        // All instructions which reference registers have to be handled explicitly
        for (auto& op : inst.ops)
            CODEGEN_ASSERT(op.kind != IrOpKind::VmReg);
        break;
    }
}

template<typename T>
static void visitVmRegDefsUses(T& visitor, IrFunction& function, const IrBlock& block)
{
    for (uint32_t instIdx = block.start; instIdx <= block.finish; instIdx++)
    {
        IrInst& inst = function.instructions[instIdx];

        visitVmRegDefsUses(visitor, function, inst);
    }
}

} // namespace CodeGen
} // namespace Luau
