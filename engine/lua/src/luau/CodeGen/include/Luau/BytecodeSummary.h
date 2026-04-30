// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeGenCommon.h"
#include "Luau/Bytecode.h"

#include <string>
#include <vector>

#include <stdint.h>

struct lua_State;
struct Proto;

namespace Luau
{
namespace CodeGen
{

class FunctionBytecodeSummary
{
public:
    FunctionBytecodeSummary(std::string source, std::string name, const int line, unsigned nestingLimit);

    const std::string& getSource() const
    {
        return source;
    }

    const std::string& getName() const
    {
        return name;
    }

    int getLine() const
    {
        return line;
    }

    const unsigned getNestingLimit() const
    {
        return nestingLimit;
    }

    const unsigned getOpLimit() const
    {
        return LOP__COUNT;
    }

    void incCount(unsigned nesting, uint8_t op)
    {
        CODEGEN_ASSERT(nesting <= getNestingLimit());
        CODEGEN_ASSERT(op < getOpLimit());
        ++counts[nesting][op];
    }

    unsigned getCount(unsigned nesting, uint8_t op) const
    {
        CODEGEN_ASSERT(nesting <= getNestingLimit());
        CODEGEN_ASSERT(op < getOpLimit());
        return counts[nesting][op];
    }

    const std::vector<unsigned>& getCounts(unsigned nesting) const
    {
        CODEGEN_ASSERT(nesting <= getNestingLimit());
        return counts[nesting];
    }

    static FunctionBytecodeSummary fromProto(Proto* proto, unsigned nestingLimit);

private:
    std::string source;
    std::string name;
    int line;
    unsigned nestingLimit;
    std::vector<std::vector<unsigned>> counts;
};

std::vector<FunctionBytecodeSummary> summarizeBytecode(lua_State* L, int idx, unsigned nestingLimit);

} // namespace CodeGen
} // namespace Luau
