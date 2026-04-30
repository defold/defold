// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "CodeGenContext.h"

#include "CodeGenA64.h"
#include "CodeGenLower.h"
#include "CodeGenX64.h"

#include "Luau/CodeGenCommon.h"
#include "Luau/CodeBlockUnwind.h"
#include "Luau/UnwindBuilder.h"
#include "Luau/UnwindBuilderDwarf2.h"
#include "Luau/UnwindBuilderWin.h"

#include "lapi.h"

LUAU_FASTINTVARIABLE(LuauCodeGenBlockSize, 4 * 1024 * 1024)
LUAU_FASTINTVARIABLE(LuauCodeGenMaxTotalSize, 256 * 1024 * 1024)
LUAU_FASTFLAG(LuauCodegenFreeBlocks)

namespace Luau
{
namespace CodeGen
{

static const Instruction kCodeEntryInsn = LOP_NATIVECALL;

// From CodeGen.cpp
static void* gPerfLogContext = nullptr;
static PerfLogFn gPerfLogFn = nullptr;

unsigned int getCpuFeaturesA64();
unsigned int getCpuFeaturesX64();

void setPerfLog(void* context, PerfLogFn logFn)
{
    gPerfLogContext = context;
    gPerfLogFn = logFn;
}

static void logPerfFunction(Proto* p, uintptr_t addr, unsigned size)
{
    CODEGEN_ASSERT(p->source);

    const char* source = getstr(p->source);
    source = (source[0] == '=' || source[0] == '@') ? source + 1 : "[string]";

    char name[256];
    snprintf(name, sizeof(name), "<luau> %s:%d %s", source, p->linedefined, p->debugname ? getstr(p->debugname) : "");

    if (gPerfLogFn)
        gPerfLogFn(gPerfLogContext, addr, size, name);
}

static void logPerfFunctions(
    const std::vector<Proto*>& moduleProtos,
    const uint8_t* nativeModuleBaseAddress,
    const std::vector<NativeProtoExecDataPtr>& nativeProtos
)
{
    if (gPerfLogFn == nullptr)
        return;

    if (nativeProtos.size() > 0)
        gPerfLogFn(
            gPerfLogContext,
            uintptr_t(nativeModuleBaseAddress),
            unsigned(getNativeProtoExecDataHeader(nativeProtos[0].get()).entryOffsetOrAddress - nativeModuleBaseAddress),
            "<luau helpers>"
        );

    auto protoIt = moduleProtos.begin();

    for (const NativeProtoExecDataPtr& nativeProto : nativeProtos)
    {
        const NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeProto.get());

        while (protoIt != moduleProtos.end() && uint32_t((**protoIt).bytecodeid) != header.bytecodeId)
        {
            ++protoIt;
        }

        CODEGEN_ASSERT(protoIt != moduleProtos.end());

        logPerfFunction(*protoIt, uintptr_t(header.entryOffsetOrAddress), uint32_t(header.nativeCodeSize));
    }
}

// If Release is true, the native proto will be removed from the vector and
// ownership will be assigned to the Proto object (for use with the
// StandaloneCodeContext).  If Release is false, the native proto will not be
// removed from the vector (for use with the SharedCodeContext).
template<bool Release, typename NativeProtosVector>
[[nodiscard]] static uint32_t bindNativeProtos(const std::vector<Proto*>& moduleProtos, NativeProtosVector& nativeProtos)
{
    uint32_t protosBound = 0;

    auto protoIt = moduleProtos.begin();

    for (auto& nativeProto : nativeProtos)
    {
        const NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeProto.get());

        while (protoIt != moduleProtos.end() && uint32_t((**protoIt).bytecodeid) != header.bytecodeId)
        {
            ++protoIt;
        }

        CODEGEN_ASSERT(protoIt != moduleProtos.end());

        // The NativeProtoExecData is now owned by the VM and will be destroyed
        // via onDestroyFunction.
        Proto* proto = *protoIt;

        if constexpr (Release)
        {
            proto->execdata = nativeProto.release();
        }
        else
        {
            proto->execdata = nativeProto.get();
        }

        proto->exectarget = reinterpret_cast<uintptr_t>(header.entryOffsetOrAddress);
        proto->codeentry = &kCodeEntryInsn;

        ++protosBound;
    }

    return protosBound;
}

BaseCodeGenContext::BaseCodeGenContext(size_t blockSize, size_t maxTotalSize, AllocationCallback* allocationCallback, void* allocationCallbackContext)
    : codeAllocator{blockSize, maxTotalSize, allocationCallback, allocationCallbackContext}
{
    CODEGEN_ASSERT(isSupported());

#if defined(_WIN32)
    unwindBuilder = std::make_unique<UnwindBuilderWin>();
#else
    unwindBuilder = std::make_unique<UnwindBuilderDwarf2>();
#endif

    codeAllocator.context = unwindBuilder.get();
    codeAllocator.createBlockUnwindInfo = createBlockUnwindInfo;
    codeAllocator.destroyBlockUnwindInfo = destroyBlockUnwindInfo;

    initFunctions(context);
}

BaseCodeGenContext::~BaseCodeGenContext()
{
    if (FFlag::LuauCodegenFreeBlocks)
        codeAllocator.deallocate(gateAllocationData);
}

[[nodiscard]] bool BaseCodeGenContext::initHeaderFunctions()
{
#if defined(CODEGEN_TARGET_X64)
    if (!X64::initHeaderFunctions(*this))
        return false;
#elif defined(CODEGEN_TARGET_A64)
    if (!A64::initHeaderFunctions(*this))
        return false;
#endif

    if (gPerfLogFn)
        gPerfLogFn(gPerfLogContext, uintptr_t(context.gateEntry), 4096, "<luau gate>");

    return true;
}


StandaloneCodeGenContext::StandaloneCodeGenContext(
    size_t blockSize,
    size_t maxTotalSize,
    AllocationCallback* allocationCallback,
    void* allocationCallbackContext
)
    : BaseCodeGenContext{blockSize, maxTotalSize, allocationCallback, allocationCallbackContext}
    , sharedAllocator{&codeAllocator}
{
}

[[nodiscard]] std::optional<ModuleBindResult> StandaloneCodeGenContext::tryBindExistingModule(const ModuleId&, const std::vector<Proto*>&)
{
    // The StandaloneCodeGenContext does not support sharing of native code
    return {};
}

[[nodiscard]] ModuleBindResult StandaloneCodeGenContext::bindModule(
    const std::optional<ModuleId>&,
    const std::vector<Proto*>& moduleProtos,
    std::vector<NativeProtoExecDataPtr> nativeProtos,
    const uint8_t* data,
    size_t dataSize,
    const uint8_t* code,
    size_t codeSize
)
{
    if (FFlag::LuauCodegenFreeBlocks)
    {
        NativeModuleRef moduleRef = sharedAllocator.insertAnonymousNativeModule(std::move(nativeProtos), data, dataSize, code, codeSize);

        // If we did not get a NativeModule back, allocation failed:
        if (moduleRef.empty())
            return {CodeGenCompilationResult::AllocationFailed};

        logPerfFunctions(moduleProtos, moduleRef->getModuleBaseAddress(), moduleRef->getNativeProtos());

        // Bind the native protos and acquire an owning reference for each:
        const uint32_t protosBound = bindNativeProtos<false>(moduleProtos, moduleRef->getNativeProtos());
        moduleRef->addRefs(protosBound);

        return {CodeGenCompilationResult::Success, protosBound};
    }
    else
    {
        uint8_t* nativeData = nullptr;
        size_t sizeNativeData = 0;
        uint8_t* codeStart = nullptr;
        if (!codeAllocator.allocate_DEPRECATED(data, int(dataSize), code, int(codeSize), nativeData, sizeNativeData, codeStart))
        {
            return {CodeGenCompilationResult::AllocationFailed};
        }

        // Relocate the entry offsets to their final executable addresses:
        for (const NativeProtoExecDataPtr& nativeProto : nativeProtos)
        {
            NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeProto.get());

            header.entryOffsetOrAddress = codeStart + reinterpret_cast<uintptr_t>(header.entryOffsetOrAddress);
        }

        logPerfFunctions(moduleProtos, codeStart, nativeProtos);

        const uint32_t protosBound = bindNativeProtos<true>(moduleProtos, nativeProtos);

        return {CodeGenCompilationResult::Success, protosBound};
    }
}

void StandaloneCodeGenContext::onCloseState() noexcept
{
    // The StandaloneCodeGenContext is owned by the one VM that owns it, so when
    // that VM is destroyed, we destroy *this as well:
    delete this;
}

void StandaloneCodeGenContext::onDestroyFunction(void* execdata) noexcept
{
    if (FFlag::LuauCodegenFreeBlocks)
        getNativeProtoExecDataHeader(static_cast<const uint32_t*>(execdata)).nativeModule->release();
    else
        destroyNativeProtoExecData(static_cast<uint32_t*>(execdata));
}


SharedCodeGenContext::SharedCodeGenContext(
    size_t blockSize,
    size_t maxTotalSize,
    AllocationCallback* allocationCallback,
    void* allocationCallbackContext
)
    : BaseCodeGenContext{blockSize, maxTotalSize, allocationCallback, allocationCallbackContext}
    , sharedAllocator{&codeAllocator}
{
}

[[nodiscard]] std::optional<ModuleBindResult> SharedCodeGenContext::tryBindExistingModule(
    const ModuleId& moduleId,
    const std::vector<Proto*>& moduleProtos
)
{
    NativeModuleRef nativeModule = sharedAllocator.tryGetNativeModule(moduleId);
    if (nativeModule.empty())
    {
        return {};
    }

    // Bind the native protos and acquire an owning reference for each:
    const uint32_t protosBound = bindNativeProtos<false>(moduleProtos, nativeModule->getNativeProtos());
    nativeModule->addRefs(protosBound);

    return {{CodeGenCompilationResult::Success, protosBound}};
}

[[nodiscard]] ModuleBindResult SharedCodeGenContext::bindModule(
    const std::optional<ModuleId>& moduleId,
    const std::vector<Proto*>& moduleProtos,
    std::vector<NativeProtoExecDataPtr> nativeProtos,
    const uint8_t* data,
    size_t dataSize,
    const uint8_t* code,
    size_t codeSize
)
{
    const std::pair<NativeModuleRef, bool> insertionResult = [&]() -> std::pair<NativeModuleRef, bool>
    {
        if (moduleId.has_value())
        {
            return sharedAllocator.getOrInsertNativeModule(*moduleId, std::move(nativeProtos), data, dataSize, code, codeSize);
        }
        else
        {
            return {sharedAllocator.insertAnonymousNativeModule(std::move(nativeProtos), data, dataSize, code, codeSize), true};
        }
    }();

    // If we did not get a NativeModule back, allocation failed:
    if (insertionResult.first.empty())
        return {CodeGenCompilationResult::AllocationFailed};

    // If we allocated a new module, log the function code ranges for perf:
    if (insertionResult.second)
        logPerfFunctions(moduleProtos, insertionResult.first->getModuleBaseAddress(), insertionResult.first->getNativeProtos());

    // Bind the native protos and acquire an owning reference for each:
    const uint32_t protosBound = bindNativeProtos<false>(moduleProtos, insertionResult.first->getNativeProtos());
    insertionResult.first->addRefs(protosBound);

    return {CodeGenCompilationResult::Success, protosBound};
}

void SharedCodeGenContext::onCloseState() noexcept
{
    // The lifetime of the SharedCodeGenContext is managed separately from the
    // VMs that use it.  When a VM is destroyed, we don't need to do anything
    // here.
}

void SharedCodeGenContext::onDestroyFunction(void* execdata) noexcept
{
    getNativeProtoExecDataHeader(static_cast<const uint32_t*>(execdata)).nativeModule->release();
}


[[nodiscard]] UniqueSharedCodeGenContext createSharedCodeGenContext()
{
    return createSharedCodeGenContext(size_t(FInt::LuauCodeGenBlockSize), size_t(FInt::LuauCodeGenMaxTotalSize), nullptr, nullptr);
}

[[nodiscard]] UniqueSharedCodeGenContext createSharedCodeGenContext(AllocationCallback* allocationCallback, void* allocationCallbackContext)
{
    return createSharedCodeGenContext(
        size_t(FInt::LuauCodeGenBlockSize), size_t(FInt::LuauCodeGenMaxTotalSize), allocationCallback, allocationCallbackContext
    );
}

[[nodiscard]] UniqueSharedCodeGenContext createSharedCodeGenContext(
    size_t blockSize,
    size_t maxTotalSize,
    AllocationCallback* allocationCallback,
    void* allocationCallbackContext
)
{
    UniqueSharedCodeGenContext codeGenContext{new SharedCodeGenContext{blockSize, maxTotalSize, nullptr, nullptr}};

    if (!codeGenContext->initHeaderFunctions())
        return {};

    return codeGenContext;
}

void destroySharedCodeGenContext(const SharedCodeGenContext* codeGenContext) noexcept
{
    delete codeGenContext;
}

void SharedCodeGenContextDeleter::operator()(const SharedCodeGenContext* codeGenContext) const noexcept
{
    destroySharedCodeGenContext(codeGenContext);
}


[[nodiscard]] static BaseCodeGenContext* getCodeGenContext(lua_State* L) noexcept
{
    return static_cast<BaseCodeGenContext*>(L->global->ecb.context);
}

static void onCloseState(lua_State* L)
{
    getCodeGenContext(L)->onCloseState();
    L->global->ecb = lua_ExecutionCallbacks{};
}

static void onDestroyFunction(lua_State* L, Proto* proto)
{
    getCodeGenContext(L)->onDestroyFunction(proto->execdata);
    proto->execdata = nullptr;
    proto->exectarget = 0;
    proto->codeentry = proto->code;
}

static int onEnter(lua_State* L, Proto* proto)
{
    BaseCodeGenContext* codeGenContext = getCodeGenContext(L);

    CODEGEN_ASSERT(proto->execdata);
    CODEGEN_ASSERT(L->ci->savedpc >= proto->code && L->ci->savedpc < proto->code + proto->sizecode);

    uintptr_t target = proto->exectarget + static_cast<uint32_t*>(proto->execdata)[L->ci->savedpc - proto->code];

    // Returns 1 to finish the function in the VM
    return GateFn(codeGenContext->context.gateEntry)(L, proto, target, &codeGenContext->context);
}

static int onEnterDisabled(lua_State* L, Proto* proto)
{
    // If the function wasn't entered natively, it cannot be resumed natively later
    L->ci->flags &= ~LUA_CALLINFO_NATIVE;

    return 1;
}

// Defined in CodeGen.cpp
void onDisable(lua_State* L, Proto* proto);

static size_t getMemorySize(lua_State* L, Proto* proto)
{
    const NativeProtoExecDataHeader& execDataHeader = getNativeProtoExecDataHeader(static_cast<const uint32_t*>(proto->execdata));

    const size_t execDataSize = sizeof(NativeProtoExecDataHeader) + execDataHeader.bytecodeInstructionCount * sizeof(Instruction);

    // While execDataSize is exactly the size of the allocation we made and hold for 'execdata' field, the code size is approximate
    // This is because code+data page is shared and owned by all Proto from a single module and each one can keep the whole region alive
    // So individual Proto being freed by GC will not reflect memory use by native code correctly
    return execDataSize + execDataHeader.nativeCodeSize;
}

static char* getCounterData(lua_State* L, Proto* proto, size_t* count)
{
    CODEGEN_ASSERT(count != nullptr);

    const NativeProtoExecDataHeader& execDataHeader = getNativeProtoExecDataHeader(static_cast<const uint32_t*>(proto->execdata));

    *count = execDataHeader.extraDataCount / 4;
    return reinterpret_cast<char*>(static_cast<uint32_t*>(proto->execdata) + proto->sizecode);
}

static void initializeExecutionCallbacks(lua_State* L, BaseCodeGenContext* codeGenContext) noexcept
{
    CODEGEN_ASSERT(codeGenContext != nullptr);

    lua_ExecutionCallbacks* ecb = &L->global->ecb;

    ecb->context = codeGenContext;
    ecb->close = onCloseState;
    ecb->destroy = onDestroyFunction;
    ecb->enter = onEnter;
    ecb->disable = onDisable;
    ecb->getmemorysize = getMemorySize;
    ecb->getcounterdata = getCounterData;
}

void create(lua_State* L)
{
    return create(L, size_t(FInt::LuauCodeGenBlockSize), size_t(FInt::LuauCodeGenMaxTotalSize), nullptr, nullptr);
}

void create(lua_State* L, AllocationCallback* allocationCallback, void* allocationCallbackContext)
{
    return create(L, size_t(FInt::LuauCodeGenBlockSize), size_t(FInt::LuauCodeGenMaxTotalSize), allocationCallback, allocationCallbackContext);
}

void create(lua_State* L, size_t blockSize, size_t maxTotalSize, AllocationCallback* allocationCallback, void* allocationCallbackContext)
{
    std::unique_ptr<StandaloneCodeGenContext> codeGenContext =
        std::make_unique<StandaloneCodeGenContext>(blockSize, maxTotalSize, allocationCallback, allocationCallbackContext);

    if (!codeGenContext->initHeaderFunctions())
        return;

    initializeExecutionCallbacks(L, codeGenContext.release());
}

void create(lua_State* L, SharedCodeGenContext* codeGenContext)
{
    initializeExecutionCallbacks(L, codeGenContext);
}

[[nodiscard]] static NativeProtoExecDataPtr createNativeProtoExecData(Proto* proto, const IrBuilder& ir)
{
    uint32_t extraDataCount = uint32_t(ir.function.extraNativeData.size());

    NativeProtoExecDataPtr nativeExecData = createNativeProtoExecData(proto->sizecode, extraDataCount);

    uint32_t instTarget = ir.function.entryLocation;
    uint32_t unassignedOffset = ir.function.endLocation - instTarget;

    for (int i = 0; i < proto->sizecode; ++i)
    {
        const BytecodeMapping& bcMapping = ir.function.bcMapping[i];

        CODEGEN_ASSERT(bcMapping.asmLocation >= instTarget);

        if (bcMapping.asmLocation != ~0u)
            nativeExecData[i] = bcMapping.asmLocation - instTarget;
        else
            nativeExecData[i] = unassignedOffset;
    }

    // After the instruction offsets, custom native data is placed
    for (uint32_t i = 0; i < extraDataCount; i++)
        nativeExecData[proto->sizecode + i] = ir.function.extraNativeData[i];

    // Set first instruction offset to 0 so that entering this function still
    // executes any generated entry code.
    nativeExecData[0] = 0;

    NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeExecData.get());
    header.entryOffsetOrAddress = reinterpret_cast<const uint8_t*>(static_cast<uintptr_t>(instTarget));
    header.bytecodeId = uint32_t(proto->bytecodeid);
    header.bytecodeInstructionCount = proto->sizecode;
    header.extraDataCount = extraDataCount;

    return nativeExecData;
}

template<typename AssemblyBuilder>
[[nodiscard]] static NativeProtoExecDataPtr createNativeFunction(
    AssemblyBuilder& build,
    ModuleHelpers& helpers,
    Proto* proto,
    uint32_t& totalIrInstCount,
    const CompilationOptions& options,
    CodeGenCompilationResult& result
)
{
    IrBuilder ir(options.hooks);
    ir.buildFunctionIr(proto);

    unsigned instCount = unsigned(ir.function.instructions.size());

    if (totalIrInstCount + instCount >= unsigned(FInt::CodegenHeuristicsInstructionLimit.value))
    {
        result = CodeGenCompilationResult::CodeGenOverflowInstructionLimit;
        return {};
    }

    totalIrInstCount += instCount;

    AssemblyOptions assemblyOptions;
    assemblyOptions.compilationOptions = options;

    if (!lowerFunction(ir, build, helpers, proto, assemblyOptions, /* stats */ nullptr, result))
    {
        return {};
    }

    return createNativeProtoExecData(proto, ir);
}

[[nodiscard]] static CompilationResult compileInternal(
    const std::optional<ModuleId>& moduleId,
    lua_State* L,
    int idx,
    const CompilationOptions& options,
    CompilationStats* stats
)
{
    CODEGEN_ASSERT(lua_isLfunction(L, idx));
    const TValue* func = luaA_toobject(L, idx);

    Proto* root = clvalue(func)->l.p;

    if ((options.flags & CodeGen_OnlyNativeModules) != 0 && (root->flags & LPF_NATIVE_MODULE) == 0 && (root->flags & LPF_NATIVE_FUNCTION) == 0)
        return CompilationResult{CodeGenCompilationResult::NotNativeModule};

    BaseCodeGenContext* codeGenContext = getCodeGenContext(L);
    if (codeGenContext == nullptr)
        return CompilationResult{CodeGenCompilationResult::CodeGenNotInitialized};

    std::vector<Proto*> protos;
    gatherFunctions(protos, root, options.flags, root->flags & LPF_NATIVE_FUNCTION);

    // Skip protos that have been compiled during previous invocations of CodeGen::compile
    protos.erase(
        std::remove_if(
            protos.begin(),
            protos.end(),
            [](Proto* p)
            {
                return p == nullptr || p->execdata != nullptr;
            }
        ),
        protos.end()
    );

    if (protos.empty())
        return CompilationResult{CodeGenCompilationResult::NothingToCompile};

    if (stats != nullptr)
        stats->functionsTotal = uint32_t(protos.size());

    if (moduleId.has_value())
    {
        if (std::optional<ModuleBindResult> existingModuleBindResult = codeGenContext->tryBindExistingModule(*moduleId, protos))
        {
            if (stats != nullptr)
                stats->functionsBound = existingModuleBindResult->functionsBound;

            return CompilationResult{existingModuleBindResult->compilationResult};
        }
    }

#if defined(CODEGEN_TARGET_A64)
    static unsigned int cpuFeatures = getCpuFeaturesA64();
    A64::AssemblyBuilderA64 build(/* logText= */ false, cpuFeatures);
#else
    static unsigned int cpuFeatures = getCpuFeaturesX64();
    X64::AssemblyBuilderX64 build(/* logText= */ false, cpuFeatures);
#endif

    ModuleHelpers helpers;
#if defined(CODEGEN_TARGET_A64)
    A64::assembleHelpers(build, helpers);
#else
    X64::assembleHelpers(build, helpers);
#endif

    CompilationResult compilationResult;

    std::vector<NativeProtoExecDataPtr> nativeProtos;
    nativeProtos.reserve(protos.size());

    uint32_t totalIrInstCount = 0;

    for (size_t i = 0; i != protos.size(); ++i)
    {
        CodeGenCompilationResult protoResult = CodeGenCompilationResult::Success;

        NativeProtoExecDataPtr nativeExecData = createNativeFunction(build, helpers, protos[i], totalIrInstCount, options, protoResult);
        if (nativeExecData != nullptr)
        {
            nativeProtos.push_back(std::move(nativeExecData));
        }
        else
        {
            compilationResult.protoFailures.push_back(
                {protoResult, protos[i]->debugname ? getstr(protos[i]->debugname) : "", protos[i]->linedefined}
            );
        }
    }

    // Very large modules might result in overflowing a jump offset; in this
    // case we currently abandon the entire module
    if (!build.finalize())
    {
        compilationResult.result = CodeGenCompilationResult::CodeGenAssemblerFinalizationFailure;
        return compilationResult;
    }

    // If no functions were assembled, we don't need to allocate/copy executable pages for helpers
    if (nativeProtos.empty())
        return compilationResult;

    if (stats != nullptr)
    {
        for (const NativeProtoExecDataPtr& nativeExecData : nativeProtos)
        {
            NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeExecData.get());

            stats->bytecodeSizeBytes += header.bytecodeInstructionCount * sizeof(Instruction);

            // Account for the native -> bytecode instruction offsets mapping:
            stats->nativeMetadataSizeBytes += header.bytecodeInstructionCount * sizeof(uint32_t);
        }

        stats->functionsCompiled += uint32_t(nativeProtos.size());
        stats->nativeCodeSizeBytes += build.code.size() * sizeof(build.code[0]);
        stats->nativeDataSizeBytes += build.data.size();
    }

    for (size_t i = 0; i < nativeProtos.size(); ++i)
    {
        NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeProtos[i].get());

        uint32_t begin = uint32_t(reinterpret_cast<uintptr_t>(header.entryOffsetOrAddress));
        uint32_t end = i + 1 < nativeProtos.size() ? uint32_t(uintptr_t(getNativeProtoExecDataHeader(nativeProtos[i + 1].get()).entryOffsetOrAddress))
                                                   : uint32_t(build.code.size() * sizeof(build.code[0]));

        CODEGEN_ASSERT(begin < end);

        header.nativeCodeSize = end - begin;
    }

    const ModuleBindResult bindResult = codeGenContext->bindModule(
        moduleId,
        protos,
        std::move(nativeProtos),
        reinterpret_cast<const uint8_t*>(build.data.data()),
        build.data.size(),
        reinterpret_cast<const uint8_t*>(build.code.data()),
        build.code.size() * sizeof(build.code[0])
    );

    if (stats != nullptr)
        stats->functionsBound = bindResult.functionsBound;

    if (bindResult.compilationResult != CodeGenCompilationResult::Success)
        compilationResult.result = bindResult.compilationResult;

    return compilationResult;
}

CompilationResult compile(const ModuleId& moduleId, lua_State* L, int idx, const CompilationOptions& options, CompilationStats* stats)
{
    return compileInternal(moduleId, L, idx, options, stats);
}

CompilationResult compile(lua_State* L, int idx, const CompilationOptions& options, CompilationStats* stats)
{
    return compileInternal({}, L, idx, options, stats);
}

CompilationResult compile(lua_State* L, int idx, unsigned int flags, CompilationStats* stats)
{
    return compileInternal({}, L, idx, CompilationOptions{flags}, stats);
}

CompilationResult compile(const ModuleId& moduleId, lua_State* L, int idx, unsigned int flags, CompilationStats* stats)
{
    return compileInternal(moduleId, L, idx, CompilationOptions{flags}, stats);
}

[[nodiscard]] bool isNativeExecutionEnabled(lua_State* L)
{
    return getCodeGenContext(L) != nullptr && L->global->ecb.enter == onEnter;
}

void setNativeExecutionEnabled(lua_State* L, bool enabled)
{
    if (getCodeGenContext(L) != nullptr)
        L->global->ecb.enter = enabled ? onEnter : onEnterDisabled;
}

void disableNativeExecutionForFunction(lua_State* L, const int level) noexcept
{
    CODEGEN_ASSERT(unsigned(level) < unsigned(L->ci - L->base_ci));

    const CallInfo* ci = L->ci - level;
    const TValue* o = ci->func;
    CODEGEN_ASSERT(ttisfunction(o));

    Proto* proto = clvalue(o)->l.p;
    CODEGEN_ASSERT(proto);

    CODEGEN_ASSERT(proto->codeentry != proto->code);
    onDestroyFunction(L, proto);
}

static uint8_t userdataRemapperWrap(lua_State* L, const char* str, size_t len)
{
    if (BaseCodeGenContext* codegenCtx = getCodeGenContext(L))
    {
        uint8_t index = codegenCtx->userdataRemapper(codegenCtx->userdataRemappingContext, str, len);

        if (index < (LBC_TYPE_TAGGED_USERDATA_END - LBC_TYPE_TAGGED_USERDATA_BASE))
            return LBC_TYPE_TAGGED_USERDATA_BASE + index;
    }

    return LBC_TYPE_USERDATA;
}

void setUserdataRemapper(lua_State* L, void* context, UserdataRemapperCallback cb)
{
    if (BaseCodeGenContext* codegenCtx = getCodeGenContext(L))
    {
        codegenCtx->userdataRemappingContext = context;
        codegenCtx->userdataRemapper = cb;

        L->global->ecb.gettypemapping = cb ? userdataRemapperWrap : nullptr;
    }
}

} // namespace CodeGen
} // namespace Luau
