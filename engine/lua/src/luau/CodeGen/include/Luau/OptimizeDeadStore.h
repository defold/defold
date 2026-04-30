// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/IrData.h"

namespace Luau
{
namespace CodeGen
{

struct IrBuilder;

void markDeadStoresInBlockChains(IrBuilder& build);

} // namespace CodeGen
} // namespace Luau
