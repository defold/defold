// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/CodeGen.h"

#include "CodeGenLower.h"

#include "Luau/CodeGenCommon.h"
#include "Luau/CodeAllocator.h"
#include "Luau/CodeBlockUnwind.h"
#include "Luau/IrBuilder.h"

#include "Luau/UnwindBuilder.h"
#include "Luau/UnwindBuilderDwarf2.h"
#include "Luau/UnwindBuilderWin.h"

#include "Luau/AssemblyBuilderA64.h"
#include "Luau/AssemblyBuilderX64.h"

#include "CodeGenContext.h"
#include "NativeState.h"

#include "CodeGenA64.h"
#include "CodeGenX64.h"

#include "lapi.h"
#include "lmem.h"

#include <memory>
#include <optional>

#if defined(CODEGEN_TARGET_X64)
#ifdef _MSC_VER
#include <intrin.h> // __cpuid
#else
#include <cpuid.h> // __cpuid
#endif
#endif

#if defined(CODEGEN_TARGET_A64)
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif

LUAU_FASTFLAGVARIABLE(DebugCodegenOptSize)
LUAU_FASTFLAGVARIABLE(DebugCodegenSkipNumbering)

// Per-module IR instruction count limit
LUAU_FASTINTVARIABLE(CodegenHeuristicsInstructionLimit, 1'048'576) // 1 M

// Per-function IR block limit
// Current value is based on some member variables being limited to 16 bits
// Because block check is made before optimization passes and optimization can generate new blocks, limit is lowered 2x
// The limit will probably be adjusted in the future to avoid performance issues with analysis that's more complex than O(n)
LUAU_FASTINTVARIABLE(CodegenHeuristicsBlockLimit, 32'768) // 32 K

// Per-function IR instruction limit
// Current value is based on some member variables being limited to 16 bits
LUAU_FASTINTVARIABLE(CodegenHeuristicsBlockInstructionLimit, 65'536) // 64 K

LUAU_FASTFLAGVARIABLE(LuauCodegenInteger2)

namespace Luau
{
namespace CodeGen
{

std::string toString(const CodeGenCompilationResult& result)
{
    switch (result)
    {
    case CodeGenCompilationResult::Success:
        return "Success";
    case CodeGenCompilationResult::NothingToCompile:
        return "NothingToCompile";
    case CodeGenCompilationResult::NotNativeModule:
        return "NotNativeModule";
    case CodeGenCompilationResult::CodeGenNotInitialized:
        return "CodeGenNotInitialized";
    case CodeGenCompilationResult::CodeGenOverflowInstructionLimit:
        return "CodeGenOverflowInstructionLimit";
    case CodeGenCompilationResult::CodeGenOverflowBlockLimit:
        return "CodeGenOverflowBlockLimit";
    case CodeGenCompilationResult::CodeGenOverflowBlockInstructionLimit:
        return "CodeGenOverflowBlockInstructionLimit";
    case CodeGenCompilationResult::CodeGenAssemblerFinalizationFailure:
        return "CodeGenAssemblerFinalizationFailure";
    case CodeGenCompilationResult::CodeGenLoweringFailure:
        return "CodeGenLoweringFailure";
    case CodeGenCompilationResult::AllocationFailed:
        return "AllocationFailed";
    case CodeGenCompilationResult::Count:
        return "Count";
    }

    CODEGEN_ASSERT(false);
    return "";
}

void onDisable(lua_State* L, Proto* proto)
{
    // do nothing if proto already uses bytecode
    if (proto->codeentry == proto->code)
        return;

    // ensure that VM does not call native code for this proto
    proto->codeentry = proto->code;

    // prevent native code from entering proto with breakpoints
    proto->exectarget = 0;

    // walk all thread call stacks and clear the LUA_CALLINFO_NATIVE flag from any
    // entries pointing to the current proto that has native code enabled.
    luaM_visitgco(
        L,
        proto,
        [](void* context, lua_Page* page, GCObject* gco)
        {
            Proto* proto = (Proto*)context;

            if (gco->gch.tt != LUA_TTHREAD)
                return false;

            lua_State* th = gco2th(gco);

            for (CallInfo* ci = th->ci; ci > th->base_ci; ci--)
            {
                if (isLua(ci))
                {
                    Proto* p = clvalue(ci->func)->l.p;

                    if (p == proto)
                    {
                        ci->flags &= ~LUA_CALLINFO_NATIVE;
                    }
                }
            }

            return false;
        }
    );
}

#if defined(CODEGEN_TARGET_A64)
unsigned int getCpuFeaturesA64()
{
    unsigned int result = 0;

#ifdef __APPLE__
    int jscvt = 0;
    size_t jscvtLen = sizeof(jscvt);
    if (sysctlbyname("hw.optional.arm.FEAT_JSCVT", &jscvt, &jscvtLen, nullptr, 0) == 0 && jscvt == 1)
        result |= A64::Feature_JSCVT;

    int advSIMD = 0;
    size_t advSIMDLen = sizeof(advSIMD);
    if (sysctlbyname("hw.optional.arm.AdvSIMD", &advSIMD, &advSIMDLen, nullptr, 0) == 0 && advSIMD == 1)
        result |= A64::Feature_AdvSIMD;
#endif

    return result;
}
#else
unsigned int getCpuFeaturesX64()
{
    unsigned int result = 0;

    int cpuinfo[4] = {0, 0, 0, 0};
#if defined(CODEGEN_TARGET_X64)
#ifdef _MSC_VER
    __cpuid(cpuinfo, 1);
#else
    __cpuid(1, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
#endif
#endif

    if ((cpuinfo[2] & 0x00001000) != 0)
        result |= X64::Feature_FMA3;

    if ((cpuinfo[2] & 0x10000000) != 0)
        result |= X64::Feature_AVX;

    return result;
}
#endif

bool isSupported()
{
    if (LUA_EXTRA_SIZE != 1)
        return false;

    if (sizeof(TValue) != 16)
        return false;

    if (sizeof(LuaNode) != 32)
        return false;

    // Windows CRT uses stack unwinding in longjmp so we have to use unwind data; on other platforms, it's only necessary for C++ EH.
#if defined(_WIN32)
    if (!isUnwindSupported())
        return false;
#else
    if (!LUA_USE_LONGJMP && !isUnwindSupported())
        return false;
#endif

#if defined(CODEGEN_TARGET_X64)
    int cpuinfo[4] = {};
#ifdef _MSC_VER
    __cpuid(cpuinfo, 1);
#else
    __cpuid(1, cpuinfo[0], cpuinfo[1], cpuinfo[2], cpuinfo[3]);
#endif

    // We require AVX1 support for VEX encoded XMM operations
    // We also require SSE4.1 support for ROUNDSD but the AVX check below covers it
    // https://en.wikipedia.org/wiki/CPUID#EAX=1:_Processor_Info_and_Feature_Bits
    if ((cpuinfo[2] & (1 << 28)) == 0)
        return false;

    return true;
#elif defined(CODEGEN_TARGET_A64)
    return true;
#else
    return false;
#endif
}

} // namespace CodeGen
} // namespace Luau
