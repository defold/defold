// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

namespace Luau
{
namespace CodeGen
{

struct Label;
struct IrOp;

namespace X64
{

class AssemblyBuilderX64;
struct OperandX64;
struct IrRegAllocX64;

void emitBuiltin(IrRegAllocX64& regs, AssemblyBuilderX64& build, int bfid, int ra, int arg, int nresults);

} // namespace X64
} // namespace CodeGen
} // namespace Luau
