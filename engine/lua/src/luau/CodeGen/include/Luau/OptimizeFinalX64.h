// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/IrData.h"

namespace Luau
{
namespace CodeGen
{

void optimizeMemoryOperandsX64(IrFunction& function);

} // namespace CodeGen
} // namespace Luau
