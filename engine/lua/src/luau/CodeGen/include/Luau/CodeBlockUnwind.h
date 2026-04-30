// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

// context must be an UnwindBuilder
void* createBlockUnwindInfo(void* context, uint8_t* block, size_t blockSize, size_t& startOffset);
void destroyBlockUnwindInfo(void* context, void* unwindData);

bool isUnwindSupported();

} // namespace CodeGen
} // namespace Luau
