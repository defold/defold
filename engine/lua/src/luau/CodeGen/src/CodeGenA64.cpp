// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "CodeGenA64.h"

#include "Luau/AssemblyBuilderA64.h"
#include "Luau/UnwindBuilder.h"

#include "BitUtils.h"
#include "CodeGenContext.h"
#include "CodeGenUtils.h"
#include "NativeState.h"
#include "EmitCommonA64.h"

#include "lstate.h"

LUAU_DYNAMIC_FASTFLAG(AddReturnExectargetCheck)
LUAU_FASTFLAG(LuauCodegenFreeBlocks)

namespace Luau
{
namespace CodeGen
{
namespace A64
{

struct EntryLocations
{
    Label start;
    Label prologueEnd;
    Label epilogueStart;
};

static void emitExit(AssemblyBuilderA64& build, bool continueInVm)
{
    build.mov(x0, continueInVm);
    build.ldr(x1, mem(rNativeContext, offsetof(NativeContext, gateExit)));
    build.br(x1);
}

static void emitUpdatePcForExit(AssemblyBuilderA64& build)
{
    // x0 = pcpos * sizeof(Instruction)
    build.add(x0, rCode, x0);
    build.ldr(x1, mem(rState, offsetof(lua_State, ci)));
    build.str(x0, mem(x1, offsetof(CallInfo, savedpc)));
}

static void emitClearNativeFlag(AssemblyBuilderA64& build)
{
    build.ldr(x0, mem(rState, offsetof(lua_State, ci)));
    build.ldr(w1, mem(x0, offsetof(CallInfo, flags)));
    build.mov(w2, ~LUA_CALLINFO_NATIVE);
    build.and_(w1, w1, w2);
    build.str(w1, mem(x0, offsetof(CallInfo, flags)));
}

static void emitInterrupt(AssemblyBuilderA64& build)
{
    // x0 = pc offset
    // x1 = return address in native code

    Label skip;

    // Stash return address in rBase; we need to reload rBase anyway
    build.mov(rBase, x1);

    // Load interrupt handler; it may be nullptr in case the update raced with the check before we got here
    build.ldr(x2, mem(rState, offsetof(lua_State, global)));
    build.ldr(x2, mem(x2, offsetof(global_State, cb.interrupt)));
    build.cbz(x2, skip);

    // Update savedpc; required in case interrupt errors
    build.add(x0, rCode, x0);
    build.ldr(x1, mem(rState, offsetof(lua_State, ci)));
    build.str(x0, mem(x1, offsetof(CallInfo, savedpc)));

    // Call interrupt
    build.mov(x0, rState);
    build.mov(w1, -1);
    build.blr(x2);

    // Check if we need to exit
    build.ldrb(w0, mem(rState, offsetof(lua_State, status)));
    build.cbz(w0, skip);

    // L->ci->savedpc--
    // note: recomputing this avoids having to stash x0
    build.ldr(x1, mem(rState, offsetof(lua_State, ci)));
    build.ldr(x0, mem(x1, offsetof(CallInfo, savedpc)));
    build.sub(x0, x0, uint16_t(sizeof(Instruction)));
    build.str(x0, mem(x1, offsetof(CallInfo, savedpc)));

    emitExit(build, /* continueInVm */ false);

    build.setLabel(skip);

    // Return back to caller; rBase has stashed return address
    build.mov(x0, rBase);

    emitUpdateBase(build); // interrupt may have reallocated stack

    build.br(x0);
}

static void emitContinueCall(AssemblyBuilderA64& build, ModuleHelpers& helpers)
{
    // x0 = closure object to reentry (equal to clvalue(L->ci->func))

    // If the fallback yielded, we need to do this right away
    // note: it's slightly cheaper to check x0 LSB; a valid Closure pointer must be aligned to 8 bytes
    CODEGEN_ASSERT(CALL_FALLBACK_YIELD == 1);
    build.tbnz(x0, 0, helpers.exitNoContinueVm);

    // Need to update state of the current function before we jump away
    build.ldr(x1, mem(x0, offsetof(Closure, l.p))); // cl->l.p aka proto

    build.ldr(x2, mem(x1, offsetof(Proto, exectarget)));
    build.cbz(x2, helpers.exitContinueVm);

    build.mov(rClosure, x0);

    static_assert(offsetof(Proto, code) == offsetof(Proto, k) + sizeof(Proto::k));
    build.ldp(rConstants, rCode, mem(x1, offsetof(Proto, k))); // proto->k, proto->code

    build.br(x2);
}

void emitReturn(AssemblyBuilderA64& build, ModuleHelpers& helpers)
{
    // x1 = res
    // w2 = number of written values

    // x0 = ci
    build.ldr(x0, mem(rState, offsetof(lua_State, ci)));
    // w3 = ci->nresults
    build.ldr(w3, mem(x0, offsetof(CallInfo, nresults)));

    Label skipResultCopy;

    // Fill the rest of the expected results (nresults - written) with 'nil'
    build.cmp(w2, w3);
    build.b(ConditionA64::GreaterEqual, skipResultCopy);

    // TODO: cmp above could compute this and flags using subs
    build.sub(w2, w3, w2); // counter = nresults - written
    build.mov(w4, LUA_TNIL);

    Label repeatNilLoop = build.setLabel();
    build.str(w4, mem(x1, offsetof(TValue, tt)));
    build.add(x1, x1, uint16_t(sizeof(TValue)));
    build.sub(w2, w2, uint16_t(1));
    build.cbnz(w2, repeatNilLoop);

    build.setLabel(skipResultCopy);

    // x2 = cip = ci - 1
    build.sub(x2, x0, uint16_t(sizeof(CallInfo)));

    // res = cip->top when nresults >= 0
    Label skipFixedRetTop;
    build.tbnz(w3, 31, skipFixedRetTop);
    build.ldr(x1, mem(x2, offsetof(CallInfo, top))); // res = cip->top
    build.setLabel(skipFixedRetTop);

    // Update VM state (ci, base, top)
    build.str(x2, mem(rState, offsetof(lua_State, ci)));      // L->ci = cip
    build.ldr(rBase, mem(x2, offsetof(CallInfo, base)));      // sync base = L->base while we have a chance
    build.str(rBase, mem(rState, offsetof(lua_State, base))); // L->base = cip->base

    build.str(x1, mem(rState, offsetof(lua_State, top))); // L->top = res

    // Unlikely, but this might be the last return from VM
    build.ldr(w4, mem(x0, offsetof(CallInfo, flags)));
    build.tbnz(w4, countrz(uint32_t(LUA_CALLINFO_RETURN)), helpers.exitNoContinueVm);

    // Continue in interpreter if function has no native data
    build.ldr(w4, mem(x2, offsetof(CallInfo, flags)));
    build.tbz(w4, countrz(uint32_t(LUA_CALLINFO_NATIVE)), helpers.exitContinueVm);

    // Need to update state of the current function before we jump away
    build.ldr(rClosure, mem(x2, offsetof(CallInfo, func)));
    build.ldr(rClosure, mem(rClosure, offsetof(TValue, value.gc)));

    build.ldr(x1, mem(rClosure, offsetof(Closure, l.p))); // cl->l.p aka proto

    if (DFFlag::AddReturnExectargetCheck)
    {
        // Get new instruction location
        static_assert(offsetof(Proto, exectarget) == offsetof(Proto, execdata) + sizeof(Proto::execdata));
        build.ldp(x3, x4, mem(x1, offsetof(Proto, execdata)));
        build.cbz(x4, helpers.exitContinueVmClearNativeFlag);
    }

    static_assert(offsetof(Proto, code) == offsetof(Proto, k) + sizeof(Proto::k));
    build.ldp(rConstants, rCode, mem(x1, offsetof(Proto, k))); // proto->k, proto->code

    // Get instruction index from instruction pointer
    // To get instruction index from instruction pointer, we need to divide byte offset by 4
    // But we will actually need to scale instruction index by 4 back to byte offset later so it cancels out
    build.ldr(x2, mem(x2, offsetof(CallInfo, savedpc))); // cip->savedpc
    build.sub(x2, x2, rCode);

    if (!DFFlag::AddReturnExectargetCheck)
    {
        // Get new instruction location and jump to it
        static_assert(offsetof(Proto, exectarget) == offsetof(Proto, execdata) + sizeof(Proto::execdata));
        build.ldp(x3, x4, mem(x1, offsetof(Proto, execdata)));
    }
    build.ldr(w2, mem(x3, x2));
    build.add(x4, x4, x2);
    build.br(x4);
}

static EntryLocations buildEntryFunction(AssemblyBuilderA64& build, UnwindBuilder& unwind)
{
    EntryLocations locations;

    // Arguments: x0 = lua_State*, x1 = Proto*, x2 = native code pointer to jump to, x3 = NativeContext*

    locations.start = build.setLabel();

    // prologue
    build.sub(sp, sp, uint16_t(kStackSize));
    build.stp(x29, x30, mem(sp)); // fp, lr

    // stash non-volatile registers used for execution environment
    build.stp(x19, x20, mem(sp, 16));
    build.stp(x21, x22, mem(sp, 32));
    build.stp(x23, x24, mem(sp, 48));
    build.str(x25, mem(sp, 64));

    build.mov(x29, sp); // this is only necessary if we maintain frame pointers, which we do in the JIT for now

    locations.prologueEnd = build.setLabel();

    uint32_t prologueSize = build.getLabelOffset(locations.prologueEnd) - build.getLabelOffset(locations.start);

    // Setup native execution environment
    build.mov(rState, x0);
    build.mov(rNativeContext, x3);
    build.ldr(rGlobalState, mem(x0, offsetof(lua_State, global)));

    build.ldr(rBase, mem(x0, offsetof(lua_State, base))); // L->base

    static_assert(offsetof(Proto, code) == offsetof(Proto, k) + sizeof(Proto::k));
    build.ldp(rConstants, rCode, mem(x1, offsetof(Proto, k))); // proto->k, proto->code

    build.ldr(x9, mem(x0, offsetof(lua_State, ci)));          // L->ci
    build.ldr(x9, mem(x9, offsetof(CallInfo, func)));         // L->ci->func
    build.ldr(rClosure, mem(x9, offsetof(TValue, value.gc))); // L->ci->func->value.gc aka cl

    // Jump to the specified instruction; further control flow will be handled with custom ABI with register setup from EmitCommonA64.h
    build.br(x2);

    // Even though we jumped away, we will return here in the end
    locations.epilogueStart = build.setLabel();

    // Cleanup and exit
    build.ldr(x25, mem(sp, 64));
    build.ldp(x23, x24, mem(sp, 48));
    build.ldp(x21, x22, mem(sp, 32));
    build.ldp(x19, x20, mem(sp, 16));
    build.ldp(x29, x30, mem(sp)); // fp, lr
    build.add(sp, sp, uint16_t(kStackSize));

    build.ret();

    // Our entry function is special, it spans the whole remaining code area
    unwind.startFunction();
    unwind.prologueA64(prologueSize, kStackSize, {x29, x30, x19, x20, x21, x22, x23, x24, x25});
    unwind.finishFunction(build.getLabelOffset(locations.start), kFullBlockFunction);

    return locations;
}

bool initHeaderFunctions(BaseCodeGenContext& codeGenContext)
{
    AssemblyBuilderA64 build(/* logText= */ false);
    UnwindBuilder& unwind = *codeGenContext.unwindBuilder.get();

    unwind.startInfo(UnwindBuilder::A64);

    EntryLocations entryLocations = buildEntryFunction(build, unwind);

    build.finalize();

    unwind.finishInfo();

    CODEGEN_ASSERT(build.data.empty());

    uint8_t* codeStart = nullptr;

    if (FFlag::LuauCodegenFreeBlocks)
    {
        codeGenContext.gateAllocationData = codeGenContext.codeAllocator.allocate(
            build.data.data(),
            int(build.data.size()),
            reinterpret_cast<const uint8_t*>(build.code.data()),
            int(build.code.size() * sizeof(build.code[0]))
        );

        if (!codeGenContext.gateAllocationData.start)
            return false;

        codeStart = codeGenContext.gateAllocationData.codeStart;
    }
    else
    {
        if (!codeGenContext.codeAllocator.allocate_DEPRECATED(
                build.data.data(),
                int(build.data.size()),
                reinterpret_cast<const uint8_t*>(build.code.data()),
                int(build.code.size() * sizeof(build.code[0])),
                codeGenContext.gateData_DEPRECATED,
                codeGenContext.gateDataSize_DEPRECATED,
                codeStart
            ))
        {
            return false;
        }
    }

    // Set the offset at the beginning so that functions in new blocks will not overlay the locations
    // specified by the unwind information of the entry function
    unwind.setBeginOffset(build.getLabelOffset(entryLocations.prologueEnd));

    codeGenContext.context.gateEntry = codeStart + build.getLabelOffset(entryLocations.start);
    codeGenContext.context.gateExit = codeStart + build.getLabelOffset(entryLocations.epilogueStart);

    return true;
}

void assembleHelpers(AssemblyBuilderA64& build, ModuleHelpers& helpers)
{
    if (build.logText)
        build.logAppend("; updatePcAndContinueInVm\n");
    build.setLabel(helpers.updatePcAndContinueInVm);
    emitUpdatePcForExit(build);

    if (build.logText)
        build.logAppend("; exitContinueVmClearNativeFlag\n");
    build.setLabel(helpers.exitContinueVmClearNativeFlag);
    emitClearNativeFlag(build);

    if (build.logText)
        build.logAppend("; exitContinueVm\n");
    build.setLabel(helpers.exitContinueVm);
    emitExit(build, /* continueInVm */ true);

    if (build.logText)
        build.logAppend("; exitNoContinueVm\n");
    build.setLabel(helpers.exitNoContinueVm);
    emitExit(build, /* continueInVm */ false);

    if (build.logText)
        build.logAppend("; interrupt\n");
    build.setLabel(helpers.interrupt);
    emitInterrupt(build);

    if (build.logText)
        build.logAppend("; return\n");
    build.setLabel(helpers.return_);
    emitReturn(build, helpers);

    if (build.logText)
        build.logAppend("; continueCall\n");
    build.setLabel(helpers.continueCall);
    emitContinueCall(build, helpers);
}

} // namespace A64
} // namespace CodeGen
} // namespace Luau
