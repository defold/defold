// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

struct Label;
struct ModuleHelpers;

namespace X64
{

class AssemblyBuilderX64;
struct IrRegAllocX64;

void emitInstCall(IrRegAllocX64& regs, AssemblyBuilderX64& build, ModuleHelpers& helpers, int ra, int nparams, int nresults);
void emitInstReturn(AssemblyBuilderX64& build, ModuleHelpers& helpers, int ra, int actualResults, bool functionVariadic);
void emitInstSetList(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int rb, int count, uint32_t index, int knownSize);
void emitInstForGLoop(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int aux, Label& loopRepeat);

} // namespace X64
} // namespace CodeGen
} // namespace Luau
