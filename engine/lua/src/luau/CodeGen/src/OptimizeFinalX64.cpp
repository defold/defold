// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/OptimizeFinalX64.h"

#include "Luau/IrUtils.h"

#include <utility>

namespace Luau
{
namespace CodeGen
{

// x64 assembly allows memory operands, but IR separates loads from uses
// To improve final x64 lowering, we try to 'inline' single-use register/constant loads into some of our instructions
// This pass might not be useful on different architectures
static void optimizeMemoryOperandsX64(IrFunction& function, IrBlock& block)
{
    CODEGEN_ASSERT(block.kind != IrBlockKind::Dead);

    for (uint32_t index = block.start; index <= block.finish; index++)
    {
        CODEGEN_ASSERT(index < function.instructions.size());
        IrInst& inst = function.instructions[index];

        switch (inst.cmd)
        {
        case IrCmd::CHECK_TAG:
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
            {
                IrInst& tag = function.instOp(OP_A(inst));

                if (tag.useCount == 1 && tag.cmd == IrCmd::LOAD_TAG && (OP_A(tag).kind == IrOpKind::VmReg || OP_A(tag).kind == IrOpKind::VmConst))
                    replace(function, OP_A(inst), OP_A(tag));
            }
            break;
        }
        case IrCmd::CHECK_TRUTHY:
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
            {
                IrInst& tag = function.instOp(OP_A(inst));

                if (tag.useCount == 1 && tag.cmd == IrCmd::LOAD_TAG && (OP_A(tag).kind == IrOpKind::VmReg || OP_A(tag).kind == IrOpKind::VmConst))
                    replace(function, OP_A(inst), OP_A(tag));
            }

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                IrInst& value = function.instOp(OP_B(inst));

                if (value.useCount == 1 && value.cmd == IrCmd::LOAD_INT)
                    replace(function, OP_B(inst), OP_A(value));
            }
            break;
        }
        case IrCmd::ADD_NUM:
        case IrCmd::SUB_NUM:
        case IrCmd::MUL_NUM:
        case IrCmd::DIV_NUM:
        case IrCmd::IDIV_NUM:
        case IrCmd::MOD_NUM:
        case IrCmd::MIN_NUM:
        case IrCmd::MAX_NUM:
        {
            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                IrInst& rhs = function.instOp(OP_B(inst));

                if (rhs.useCount == 1 && rhs.cmd == IrCmd::LOAD_DOUBLE && (OP_A(rhs).kind == IrOpKind::VmReg || OP_A(rhs).kind == IrOpKind::VmConst))
                    replace(function, OP_B(inst), OP_A(rhs));
            }
            break;
        }
        case IrCmd::JUMP_EQ_TAG:
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
            {
                IrInst& tagA = function.instOp(OP_A(inst));

                if (tagA.useCount == 1 && tagA.cmd == IrCmd::LOAD_TAG && (OP_A(tagA).kind == IrOpKind::VmReg || OP_A(tagA).kind == IrOpKind::VmConst))
                {
                    replace(function, OP_A(inst), OP_A(tagA));
                    break;
                }
            }

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                IrInst& tagB = function.instOp(OP_B(inst));

                if (tagB.useCount == 1 && tagB.cmd == IrCmd::LOAD_TAG && (OP_A(tagB).kind == IrOpKind::VmReg || OP_A(tagB).kind == IrOpKind::VmConst))
                {
                    std::swap(OP_A(inst), OP_B(inst));
                    replace(function, OP_A(inst), OP_A(tagB));
                }
            }
            break;
        }
        case IrCmd::JUMP_CMP_NUM:
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
            {
                IrInst& num = function.instOp(OP_A(inst));

                if (num.useCount == 1 && num.cmd == IrCmd::LOAD_DOUBLE)
                    replace(function, OP_A(inst), OP_A(num));
            }
            break;
        }
        case IrCmd::FLOOR_NUM:
        case IrCmd::CEIL_NUM:
        case IrCmd::ROUND_NUM:
        case IrCmd::SQRT_NUM:
        case IrCmd::ABS_NUM:
        {
            if (OP_A(inst).kind == IrOpKind::Inst)
            {
                IrInst& arg = function.instOp(OP_A(inst));

                if (arg.useCount == 1 && arg.cmd == IrCmd::LOAD_DOUBLE && (OP_A(arg).kind == IrOpKind::VmReg || OP_A(arg).kind == IrOpKind::VmConst))
                    replace(function, OP_A(inst), OP_A(arg));
            }
            break;
        }
        default:
            break;
        }
    }
}

void optimizeMemoryOperandsX64(IrFunction& function)
{
    for (IrBlock& block : function.blocks)
    {
        if (block.kind == IrBlockKind::Dead)
            continue;

        optimizeMemoryOperandsX64(function, block);
    }
}

} // namespace CodeGen
} // namespace Luau
