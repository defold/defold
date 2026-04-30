// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/IrData.h"
#include "Luau/CodeGenOptions.h"

#include <string>
#include <vector>

struct Proto;

namespace Luau
{
namespace CodeGen
{

struct CfgInfo;

const char* getCmdName(IrCmd cmd);
const char* getBlockKindName(IrBlockKind kind);

struct IrToStringContext
{
    std::string& result;
    const std::vector<IrBlock>& blocks;
    const std::vector<IrConst>& constants;
    const CfgInfo& cfg;
    Proto* proto = nullptr;
};

void toString(IrToStringContext& ctx, const IrInst& inst, uint32_t index);
void toString(IrToStringContext& ctx, const IrBlock& block, uint32_t index); // Block title
void toString(IrToStringContext& ctx, IrOp op);

void toString(std::string& result, Proto* proto, IrConst constant);

const char* getBytecodeTypeName(uint8_t type, const char* const* userdataTypes);

void toString(std::string& result, const BytecodeTypes& bcTypes, const char* const* userdataTypes);

void toStringDetailed(IrToStringContext& ctx, const IrBlock& block, uint32_t blockIdx, IrInst& inst, uint32_t instIdx, IncludeUseInfo includeUseInfo);
void toStringDetailed(
    IrToStringContext& ctx,
    const IrBlock& block,
    uint32_t blockIdx,
    IncludeUseInfo includeUseInfo,
    IncludeCfgInfo includeCfgInfo,
    IncludeRegFlowInfo includeRegFlowInfo
);

std::string toString(IrFunction& function, IncludeUseInfo includeUseInfo);

std::string dump(IrFunction& function);

std::string toDot(const IrFunction& function, bool includeInst);
std::string toDotCfg(const IrFunction& function);
std::string toDotDjGraph(const IrFunction& function);

std::string dumpDot(const IrFunction& function, bool includeInst);

} // namespace CodeGen
} // namespace Luau
