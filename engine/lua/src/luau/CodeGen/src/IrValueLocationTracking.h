// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/IrData.h"

#include <array>

namespace Luau
{
namespace CodeGen
{

struct IrValueLocationTracking
{
    IrValueLocationTracking(IrFunction& function);

    void setRestoreCallback(void* context, void (*callback)(void* context, IrInst& inst));

    bool canBeRematerialized(IrCmd cmd);
    bool canRematerializeArguments(IrInst& inst);

    void beforeInstLowering(IrInst& inst);
    void afterInstLowering(IrInst& inst, uint32_t instIdx);

    void recordRestoreOp(uint32_t instIdx, IrOp location);
    void invalidateRestoreOp(IrOp location, bool skipValueInvalidation);
    void invalidateRestoreVmRegs(int start, int count);

    IrFunction& function;

    std::array<uint32_t, 256> vmRegValue;

    // For range/full invalidations, we only want to visit a limited number of data that we have recorded
    int maxReg = 0;

    void* restoreCallbackCtx = nullptr;
    void (*restoreCallback)(void* context, IrInst& inst) = nullptr;
};

} // namespace CodeGen
} // namespace Luau
