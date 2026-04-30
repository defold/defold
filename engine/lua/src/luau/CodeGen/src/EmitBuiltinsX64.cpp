// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "EmitBuiltinsX64.h"

#include "Luau/Bytecode.h"

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrCallWrapperX64.h"
#include "Luau/IrRegAllocX64.h"

#include "EmitCommonX64.h"
#include "NativeState.h"

#include "lstate.h"

namespace Luau
{
namespace CodeGen
{
namespace X64
{

static void emitBuiltinMathFrexp(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int arg, int nresults)
{
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::xmmword, luauRegValue(arg));
    callWrap.addArgument(SizeX64::qword, sTemporarySlot);
    callWrap.call(qword[rNativeContext + offsetof(NativeContext, libm_frexp)]);

    build.vmovsd(luauRegValue(ra), xmm0);
    build.mov(luauRegTag(ra), LUA_TNUMBER);

    if (nresults > 1)
    {
        build.vcvtsi2sd(xmm0, xmm0, dword[sTemporarySlot + 0]);
        build.vmovsd(luauRegValue(ra + 1), xmm0);
        build.mov(luauRegTag(ra + 1), LUA_TNUMBER);
    }
}

static void emitBuiltinMathModf(IrRegAllocX64& regs, AssemblyBuilderX64& build, int ra, int arg, int nresults)
{
    IrCallWrapperX64 callWrap(regs, build);
    callWrap.addArgument(SizeX64::xmmword, luauRegValue(arg));
    callWrap.addArgument(SizeX64::qword, sTemporarySlot);
    callWrap.call(qword[rNativeContext + offsetof(NativeContext, libm_modf)]);

    build.vmovsd(xmm1, qword[sTemporarySlot + 0]);
    build.vmovsd(luauRegValue(ra), xmm1);
    build.mov(luauRegTag(ra), LUA_TNUMBER);

    if (nresults > 1)
    {
        build.vmovsd(luauRegValue(ra + 1), xmm0);
        build.mov(luauRegTag(ra + 1), LUA_TNUMBER);
    }
}

void emitBuiltin(IrRegAllocX64& regs, AssemblyBuilderX64& build, int bfid, int ra, int arg, int nresults)
{
    switch (bfid)
    {
    case LBF_MATH_FREXP:
        CODEGEN_ASSERT(nresults == 1 || nresults == 2);
        return emitBuiltinMathFrexp(regs, build, ra, arg, nresults);
    case LBF_MATH_MODF:
        CODEGEN_ASSERT(nresults == 1 || nresults == 2);
        return emitBuiltinMathModf(regs, build, ra, arg, nresults);
    default:
        CODEGEN_ASSERT(!"Missing x64 lowering");
    }
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
