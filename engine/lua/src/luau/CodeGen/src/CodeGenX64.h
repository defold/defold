// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

namespace Luau
{
namespace CodeGen
{

class BaseCodeGenContext;
struct ModuleHelpers;

namespace X64
{

class AssemblyBuilderX64;

bool initHeaderFunctions(BaseCodeGenContext& codeGenContext);
void assembleHelpers(AssemblyBuilderX64& build, ModuleHelpers& helpers);

} // namespace X64
} // namespace CodeGen
} // namespace Luau
