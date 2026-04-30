// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "CodeGenX64.h"

#include "Luau/AssemblyBuilderX64.h"
#include "Luau/IrCallWrapperX64.h"
#include "Luau/UnwindBuilder.h"

#include "CodeGenContext.h"
#include "NativeState.h"
#include "EmitCommonX64.h"

#include "lstate.h"

LUAU_FASTFLAG(LuauCodegenFreeBlocks)
LUAU_FASTFLAGVARIABLE(LuauCodegenSuggestArgumentRegisterX64)

/* An overview of native environment stack setup that we are making in the entry function:
 * Each line is 8 bytes, stack grows downwards.
 *
 * | ... previous frames ...
 * | rdx home space | (unused)
 * | rcx home space | (unused)
 * | return address |
 * | ... saved non-volatile registers ... <-- rsp + kStackSizeFull
 * |   alignment    |
 * | xmm9 non-vol   |
 * | xmm9 cont.     |
 * | xmm8 non-vol   |
 * | xmm8 cont.     |
 * | xmm7 non-vol   |
 * | xmm7 cont.     |
 * | xmm6 non-vol   |
 * | xmm6 cont.     |
 * | spill slot 5   |
 * | spill slot 4   |
 * | spill slot 3   |
 * | spill slot 2   |
 * | spill slot 1   | <-- rsp + kStackOffsetToSpillSlots
 * | sTemporarySlot |
 * | sCode          |
 * | sClosure       | <-- rsp + kStackOffsetToLocals
 * | argument 6     | <-- rsp + 40
 * | argument 5     | <-- rsp + 32
 * | r9 home space  |
 * | r8 home space  |
 * | rdx home space |
 * | rcx home space | <-- rsp points here
 *
 * Arguments to our entry function are saved to home space only on Windows.
 * Space for arguments to function we call is always reserved, but used only on Windows.
 *
 * Right now we use a frame pointer, but because of a fixed layout we can omit it in the future
 */

namespace Luau
{
namespace CodeGen
{
namespace X64
{

struct EntryLocations
{
    Label start;
    Label prologueEnd;
    Label epilogueStart;
};

static EntryLocations buildEntryFunction(AssemblyBuilderX64& build, UnwindBuilder& unwind)
{
    EntryLocations locations;

    build.align(kFunctionAlignment, X64::AlignmentDataX64::Ud2);

    locations.start = build.setLabel();
    unwind.startFunction();

    RegisterX64 rArg1{};
    RegisterX64 rArg2{};
    RegisterX64 rArg3{};
    RegisterX64 rArg4{};
    if (FFlag::LuauCodegenSuggestArgumentRegisterX64)
    {
        rArg1 = IrCallWrapperX64::suggestArgumentRegister<0>(SizeX64::qword, build);
        rArg2 = IrCallWrapperX64::suggestArgumentRegister<1>(SizeX64::qword, build);
        rArg3 = IrCallWrapperX64::suggestArgumentRegister<2>(SizeX64::qword, build);
        rArg4 = IrCallWrapperX64::suggestArgumentRegister<3>(SizeX64::qword, build);
    }
    else
    {
        rArg1 = (build.abi == ABIX64::Windows) ? rcx : rdi;
        rArg2 = (build.abi == ABIX64::Windows) ? rdx : rsi;
        rArg3 = (build.abi == ABIX64::Windows) ? r8 : rdx;
        rArg4 = (build.abi == ABIX64::Windows) ? r9 : rcx;
    }

    // Save common non-volatile registers
    if (build.abi == ABIX64::SystemV)
    {
        // We need to use a standard rbp-based frame setup for debuggers to work with JIT code
        build.push(rbp);
        build.mov(rbp, rsp);
    }

    build.push(rbx);
    build.push(r12);
    build.push(r13);
    build.push(r14);
    build.push(r15);

    if (build.abi == ABIX64::Windows)
    {
        // Save non-volatile registers that are specific to Windows x64 ABI
        build.push(rdi);
        build.push(rsi);

        // On Windows, rbp is available as a general-purpose non-volatile register and this might be freed up
        build.push(rbp);
    }

    // Allocate stack space
    uint8_t usableXmmRegCount = getXmmRegisterCount(build.abi);
    unsigned xmmStorageSize = getNonVolXmmStorageSize(build.abi, usableXmmRegCount);
    unsigned fullStackSize = getFullStackSize(build.abi, usableXmmRegCount);

    build.sub(rsp, fullStackSize);

    OperandX64 xmmStorageOffset = rsp + (fullStackSize - (kStackAlign + xmmStorageSize));

    // On Windows, we have to save non-volatile xmm registers
    std::vector<RegisterX64> savedXmmRegs;

    if (build.abi == ABIX64::Windows)
    {
        if (usableXmmRegCount > kWindowsFirstNonVolXmmReg)
            savedXmmRegs.reserve(usableXmmRegCount - kWindowsFirstNonVolXmmReg);

        for (uint8_t i = kWindowsFirstNonVolXmmReg, offset = 0; i < usableXmmRegCount; i++, offset += 16)
        {
            RegisterX64 xmmReg = RegisterX64{SizeX64::xmmword, i};
            build.vmovaps(xmmword[xmmStorageOffset + offset], xmmReg);
            savedXmmRegs.push_back(xmmReg);
        }
    }

    locations.prologueEnd = build.setLabel();

    uint32_t prologueSize = build.getLabelOffset(locations.prologueEnd) - build.getLabelOffset(locations.start);

    if (build.abi == ABIX64::SystemV)
        unwind.prologueX64(prologueSize, fullStackSize, /* setupFrame= */ true, {rbx, r12, r13, r14, r15}, {});
    else if (build.abi == ABIX64::Windows)
        unwind.prologueX64(prologueSize, fullStackSize, /* setupFrame= */ false, {rbx, r12, r13, r14, r15, rdi, rsi, rbp}, savedXmmRegs);

    // Setup native execution environment
    build.mov(rState, rArg1);
    build.mov(rNativeContext, rArg4);
    build.mov(rBase, qword[rState + offsetof(lua_State, base)]); // L->base
    build.mov(rax, qword[rState + offsetof(lua_State, ci)]);     // L->ci
    build.mov(rax, qword[rax + offsetof(CallInfo, func)]);       // L->ci->func
    build.mov(rax, qword[rax + offsetof(TValue, value.gc)]);     // L->ci->func->value.gc aka cl
    build.mov(sClosure, rax);
    build.mov(rConstants, qword[rArg2 + offsetof(Proto, k)]); // proto->k
    build.mov(rax, qword[rArg2 + offsetof(Proto, code)]);     // proto->code
    build.mov(sCode, rax);

    // Jump to the specified instruction; further control flow will be handled with custom ABI with register setup from EmitCommonX64.h
    build.jmp(rArg3);

    // Even though we jumped away, we will return here in the end
    locations.epilogueStart = build.setLabel();

    // Epilogue and exit
    if (build.abi == ABIX64::Windows)
    {
        // xmm registers are restored before the official epilogue that has to start with 'add rsp/lea rsp'
        for (uint8_t i = kWindowsFirstNonVolXmmReg, offset = 0; i < usableXmmRegCount; i++, offset += 16)
            build.vmovaps(RegisterX64{SizeX64::xmmword, i}, xmmword[xmmStorageOffset + offset]);
    }

    build.add(rsp, fullStackSize);

    if (build.abi == ABIX64::Windows)
    {
        build.pop(rbp);
        build.pop(rsi);
        build.pop(rdi);
    }

    build.pop(r15);
    build.pop(r14);
    build.pop(r13);
    build.pop(r12);
    build.pop(rbx);

    if (build.abi == ABIX64::SystemV)
        build.pop(rbp);

    build.ret();

    // Our entry function is special, it spans the whole remaining code area
    unwind.finishFunction(build.getLabelOffset(locations.start), kFullBlockFunction);

    return locations;
}

bool initHeaderFunctions(BaseCodeGenContext& codeGenContext)
{
    AssemblyBuilderX64 build(/* logText= */ false);
    UnwindBuilder& unwind = *codeGenContext.unwindBuilder.get();

    unwind.startInfo(UnwindBuilder::X64);

    EntryLocations entryLocations = buildEntryFunction(build, unwind);

    build.finalize();

    unwind.finishInfo();

    CODEGEN_ASSERT(build.data.empty());

    uint8_t* codeStart = nullptr;

    if (FFlag::LuauCodegenFreeBlocks)
    {
        codeGenContext.gateAllocationData =
            codeGenContext.codeAllocator.allocate(build.data.data(), int(build.data.size()), build.code.data(), int(build.code.size()));

        if (!codeGenContext.gateAllocationData.start)
            return false;

        codeStart = codeGenContext.gateAllocationData.codeStart;
    }
    else
    {
        if (!codeGenContext.codeAllocator.allocate_DEPRECATED(
                build.data.data(),
                int(build.data.size()),
                build.code.data(),
                int(build.code.size()),
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

void assembleHelpers(X64::AssemblyBuilderX64& build, ModuleHelpers& helpers)
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
}

} // namespace X64
} // namespace CodeGen
} // namespace Luau
