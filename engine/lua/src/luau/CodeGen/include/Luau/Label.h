// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

struct Label
{
    uint32_t id = 0;
    uint32_t location = ~0u;
};

} // namespace CodeGen
} // namespace Luau
