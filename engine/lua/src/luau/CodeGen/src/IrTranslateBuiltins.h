// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

namespace Luau
{
namespace CodeGen
{

struct IrBuilder;
struct IrOp;

enum class BuiltinImplType
{
    None,
    UsesFallback, // Uses fallback for unsupported cases
    Full,         // Is either implemented in full, or exits to VM
};

struct BuiltinImplResult
{
    BuiltinImplType type;
    int actualResultCount;
};

BuiltinImplResult translateBuiltin(
    IrBuilder& build,
    int bfid,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nparams,
    int nresults,
    IrOp fallback,
    int pcpos
);

} // namespace CodeGen
} // namespace Luau
