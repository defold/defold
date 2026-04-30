// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/BytecodeSummary.h"

#include "Luau/IrUtils.h"

#include "CodeGenLower.h"

#include "lua.h"
#include "lapi.h"
#include "lobject.h"
#include "lstate.h"

namespace Luau
{
namespace CodeGen
{

FunctionBytecodeSummary::FunctionBytecodeSummary(std::string source, std::string name, const int line, unsigned nestingLimit)
    : source(std::move(source))
    , name(std::move(name))
    , line(line)
    , nestingLimit(nestingLimit)
{
    counts.reserve(nestingLimit);
    for (unsigned i = 0; i < 1 + nestingLimit; ++i)
    {
        counts.push_back(std::vector<unsigned>(getOpLimit(), 0));
    }
}

FunctionBytecodeSummary FunctionBytecodeSummary::fromProto(Proto* proto, unsigned nestingLimit)
{
    const char* source = getstr(proto->source);
    source = (source[0] == '=' || source[0] == '@') ? source + 1 : "[string]";

    const char* name = proto->debugname ? getstr(proto->debugname) : "";

    int line = proto->linedefined;

    FunctionBytecodeSummary summary(source, name, line, nestingLimit);

    for (int i = 0; i < proto->sizecode;)
    {
        Instruction insn = proto->code[i];
        uint8_t op = LUAU_INSN_OP(insn);
        summary.incCount(0, op);
        i += getOpLength(LuauOpcode(op));
    }

    return summary;
}

std::vector<FunctionBytecodeSummary> summarizeBytecode(lua_State* L, int idx, unsigned nestingLimit)
{
    CODEGEN_ASSERT(lua_isLfunction(L, idx));
    const TValue* func = luaA_toobject(L, idx);

    Proto* root = clvalue(func)->l.p;

    std::vector<Proto*> protos;
    gatherFunctions(protos, root, CodeGen_ColdFunctions, root->flags & LPF_NATIVE_FUNCTION);

    std::vector<FunctionBytecodeSummary> summaries;
    summaries.reserve(protos.size());

    for (Proto* proto : protos)
    {
        if (proto)
            summaries.push_back(FunctionBytecodeSummary::fromProto(proto, nestingLimit));
    }

    return summaries;
}

} // namespace CodeGen
} // namespace Luau
