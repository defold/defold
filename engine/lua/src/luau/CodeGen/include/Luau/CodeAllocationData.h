// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

struct CodeAllocationData
{
    uint8_t* start = nullptr;
    size_t size = 0;
    uint8_t* codeStart = nullptr;

    // Allocation is page-aligned and can contain extra data
    uint8_t* allocationStart = nullptr;
    size_t allocationSize = 0;
};

} // namespace CodeGen
} // namespace Luau
