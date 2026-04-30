// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/SharedCodeAllocator.h"

#include "Luau/CodeAllocator.h"
#include "Luau/CodeGenCommon.h"

#include <algorithm>
#include <string_view>
#include <utility>

LUAU_FASTFLAG(LuauCodegenFreeBlocks)

namespace Luau
{
namespace CodeGen
{

struct NativeProtoBytecodeIdEqual
{
    [[nodiscard]] bool operator()(const NativeProtoExecDataPtr& left, const NativeProtoExecDataPtr& right) const noexcept
    {
        return getNativeProtoExecDataHeader(left.get()).bytecodeId == getNativeProtoExecDataHeader(right.get()).bytecodeId;
    }
};

struct NativeProtoBytecodeIdLess
{
    [[nodiscard]] bool operator()(const NativeProtoExecDataPtr& left, const NativeProtoExecDataPtr& right) const noexcept
    {
        return getNativeProtoExecDataHeader(left.get()).bytecodeId < getNativeProtoExecDataHeader(right.get()).bytecodeId;
    }

    [[nodiscard]] bool operator()(const NativeProtoExecDataPtr& left, uint32_t right) const noexcept
    {
        return getNativeProtoExecDataHeader(left.get()).bytecodeId < right;
    }

    [[nodiscard]] bool operator()(uint32_t left, const NativeProtoExecDataPtr& right) const noexcept
    {
        return left < getNativeProtoExecDataHeader(right.get()).bytecodeId;
    }
};

NativeModule::NativeModule(
    SharedCodeAllocator* allocator,
    const std::optional<ModuleId>& moduleId,
    const uint8_t* moduleBaseAddress,
    std::vector<NativeProtoExecDataPtr> nativeProtos
) noexcept
    : allocator{allocator}
    , moduleId{moduleId}
    , moduleBaseAddress_DEPRECATED{moduleBaseAddress}
    , nativeProtos{std::move(nativeProtos)}
{
    CODEGEN_ASSERT(!FFlag::LuauCodegenFreeBlocks);
    CODEGEN_ASSERT(allocator != nullptr);
    CODEGEN_ASSERT(moduleBaseAddress_DEPRECATED != nullptr);

    // Bind all of the NativeProtos to this module:
    for (const NativeProtoExecDataPtr& nativeProto : this->nativeProtos)
    {
        NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeProto.get());
        header.nativeModule = this;
        header.entryOffsetOrAddress = moduleBaseAddress_DEPRECATED + reinterpret_cast<uintptr_t>(header.entryOffsetOrAddress);
    }

    std::sort(this->nativeProtos.begin(), this->nativeProtos.end(), NativeProtoBytecodeIdLess{});

    // We should not have two NativeProtos for the same bytecode id:
    CODEGEN_ASSERT(
        std::adjacent_find(this->nativeProtos.begin(), this->nativeProtos.end(), NativeProtoBytecodeIdEqual{}) == this->nativeProtos.end()
    );
}

NativeModule::NativeModule(
    SharedCodeAllocator* allocator,
    const std::optional<ModuleId>& moduleId,
    CodeAllocationData codeAllocationData,
    std::vector<NativeProtoExecDataPtr> nativeProtos
) noexcept
    : allocator{allocator}
    , moduleId{moduleId}
    , codeAllocationData{codeAllocationData}
    , nativeProtos{std::move(nativeProtos)}
{
    CODEGEN_ASSERT(FFlag::LuauCodegenFreeBlocks);
    CODEGEN_ASSERT(allocator != nullptr);
    CODEGEN_ASSERT(codeAllocationData.start != nullptr);

    // Bind all of the NativeProtos to this module:
    for (const NativeProtoExecDataPtr& nativeProto : this->nativeProtos)
    {
        NativeProtoExecDataHeader& header = getNativeProtoExecDataHeader(nativeProto.get());
        header.nativeModule = this;
        header.entryOffsetOrAddress = codeAllocationData.codeStart + reinterpret_cast<uintptr_t>(header.entryOffsetOrAddress);
    }

    std::sort(this->nativeProtos.begin(), this->nativeProtos.end(), NativeProtoBytecodeIdLess{});

    // We should not have two NativeProtos for the same bytecode id:
    CODEGEN_ASSERT(
        std::adjacent_find(this->nativeProtos.begin(), this->nativeProtos.end(), NativeProtoBytecodeIdEqual{}) == this->nativeProtos.end()
    );
}

NativeModule::~NativeModule() noexcept
{
    CODEGEN_ASSERT(refcount == 0);
}

size_t NativeModule::addRef() const noexcept
{
    return refcount.fetch_add(1) + 1;
}

size_t NativeModule::addRefs(size_t count) const noexcept
{
    return refcount.fetch_add(count) + count;
}

size_t NativeModule::release() const noexcept
{
    size_t newRefcount = refcount.fetch_sub(1) - 1;
    if (newRefcount != 0)
        return newRefcount;

    allocator->eraseNativeModuleIfUnreferenced(*this);

    // NOTE:  *this may have been destroyed by the prior call, and must not be
    // accessed after this point.
    return 0;
}

[[nodiscard]] size_t NativeModule::getRefcount() const noexcept
{
    return refcount;
}

[[nodiscard]] const std::optional<ModuleId>& NativeModule::getModuleId() const noexcept
{
    return moduleId;
}

[[nodiscard]] const uint8_t* NativeModule::getModuleBaseAddress() const noexcept
{
    return FFlag::LuauCodegenFreeBlocks ? codeAllocationData.codeStart : moduleBaseAddress_DEPRECATED;
}

[[nodiscard]] CodeAllocationData NativeModule::getCodeAllocationData() const noexcept
{
    CODEGEN_ASSERT(FFlag::LuauCodegenFreeBlocks);

    return codeAllocationData;
}

[[nodiscard]] const uint32_t* NativeModule::tryGetNativeProto(uint32_t bytecodeId) const noexcept
{
    const auto range = std::equal_range(nativeProtos.begin(), nativeProtos.end(), bytecodeId, NativeProtoBytecodeIdLess{});
    if (range.first == range.second)
        return nullptr;

    CODEGEN_ASSERT(std::next(range.first) == range.second);

    return range.first->get();
}

[[nodiscard]] const std::vector<NativeProtoExecDataPtr>& NativeModule::getNativeProtos() const noexcept
{
    return nativeProtos;
}


NativeModuleRef::NativeModuleRef(const NativeModule* nativeModule) noexcept
    : nativeModule{nativeModule}
{
    if (nativeModule != nullptr)
        nativeModule->addRef();
}

NativeModuleRef::NativeModuleRef(const NativeModuleRef& other) noexcept
    : nativeModule{other.nativeModule}
{
    if (nativeModule != nullptr)
        nativeModule->addRef();
}

NativeModuleRef::NativeModuleRef(NativeModuleRef&& other) noexcept
    : nativeModule{std::exchange(other.nativeModule, nullptr)}
{
}

NativeModuleRef& NativeModuleRef::operator=(NativeModuleRef other) noexcept
{
    swap(other);

    return *this;
}

NativeModuleRef::~NativeModuleRef() noexcept
{
    reset();
}

void NativeModuleRef::reset() noexcept
{
    if (nativeModule == nullptr)
        return;

    nativeModule->release();
    nativeModule = nullptr;
}

void NativeModuleRef::swap(NativeModuleRef& other) noexcept
{
    std::swap(nativeModule, other.nativeModule);
}

[[nodiscard]] bool NativeModuleRef::empty() const noexcept
{
    return nativeModule == nullptr;
}

NativeModuleRef::operator bool() const noexcept
{
    return nativeModule != nullptr;
}

[[nodiscard]] const NativeModule* NativeModuleRef::get() const noexcept
{
    return nativeModule;
}

[[nodiscard]] const NativeModule* NativeModuleRef::operator->() const noexcept
{
    return nativeModule;
}

[[nodiscard]] const NativeModule& NativeModuleRef::operator*() const noexcept
{
    return *nativeModule;
}


SharedCodeAllocator::SharedCodeAllocator(CodeAllocator* codeAllocator) noexcept
    : codeAllocator{codeAllocator}
{
}

SharedCodeAllocator::~SharedCodeAllocator() noexcept
{
    // The allocator should not be destroyed until all outstanding references
    // have been released and all allocated modules have been destroyed.
    CODEGEN_ASSERT(identifiedModules.empty());
    CODEGEN_ASSERT(anonymousModuleCount == 0);
}

[[nodiscard]] NativeModuleRef SharedCodeAllocator::tryGetNativeModule(const ModuleId& moduleId) const noexcept
{
    std::unique_lock lock{mutex};

    return tryGetNativeModuleWithLockHeld(moduleId);
}

std::pair<NativeModuleRef, bool> SharedCodeAllocator::getOrInsertNativeModule(
    const ModuleId& moduleId,
    std::vector<NativeProtoExecDataPtr> nativeProtos,
    const uint8_t* data,
    size_t dataSize,
    const uint8_t* code,
    size_t codeSize
)
{
    std::unique_lock lock{mutex};

    if (NativeModuleRef existingModule = tryGetNativeModuleWithLockHeld(moduleId))
        return {std::move(existingModule), false};

    if (FFlag::LuauCodegenFreeBlocks)
    {
        CodeAllocationData result = codeAllocator->allocate(data, int(dataSize), code, int(codeSize));

        if (!result.start)
            return {};

        std::unique_ptr<NativeModule>& nativeModule = identifiedModules[moduleId];
        nativeModule = std::make_unique<NativeModule>(this, moduleId, result, std::move(nativeProtos));

        return {NativeModuleRef{nativeModule.get()}, true};
    }
    else
    {
        uint8_t* nativeData = nullptr;
        size_t sizeNativeData = 0;
        uint8_t* codeStart = nullptr;
        if (!codeAllocator->allocate_DEPRECATED(data, int(dataSize), code, int(codeSize), nativeData, sizeNativeData, codeStart))
        {
            return {};
        }

        std::unique_ptr<NativeModule>& nativeModule = identifiedModules[moduleId];
        nativeModule = std::make_unique<NativeModule>(this, moduleId, codeStart, std::move(nativeProtos));

        return {NativeModuleRef{nativeModule.get()}, true};
    }
}

NativeModuleRef SharedCodeAllocator::insertAnonymousNativeModule(
    std::vector<NativeProtoExecDataPtr> nativeProtos,
    const uint8_t* data,
    size_t dataSize,
    const uint8_t* code,
    size_t codeSize
)
{
    std::unique_lock lock{mutex};

    if (FFlag::LuauCodegenFreeBlocks)
    {
        CodeAllocationData result = codeAllocator->allocate(data, int(dataSize), code, int(codeSize));

        if (!result.start)
            return {};

        NativeModuleRef nativeModuleRef{new NativeModule{this, std::nullopt, result, std::move(nativeProtos)}};
        ++anonymousModuleCount;

        return nativeModuleRef;
    }
    else
    {
        uint8_t* nativeData = nullptr;
        size_t sizeNativeData = 0;
        uint8_t* codeStart = nullptr;
        if (!codeAllocator->allocate_DEPRECATED(data, int(dataSize), code, int(codeSize), nativeData, sizeNativeData, codeStart))
        {
            return {};
        }

        NativeModuleRef nativeModuleRef{new NativeModule{this, std::nullopt, codeStart, std::move(nativeProtos)}};
        ++anonymousModuleCount;

        return nativeModuleRef;
    }
}

void SharedCodeAllocator::eraseNativeModuleIfUnreferenced(const NativeModule& nativeModule)
{
    std::unique_lock lock{mutex};

    // It is possible that someone acquired a reference to the module between
    // the time that we called this function and the time that we acquired the
    // lock.  If so, that's okay.
    if (nativeModule.getRefcount() != 0)
        return;

    if (FFlag::LuauCodegenFreeBlocks)
        codeAllocator->deallocate(nativeModule.getCodeAllocationData());

    if (const std::optional<ModuleId>& moduleId = nativeModule.getModuleId())
    {
        const auto it = identifiedModules.find(*moduleId);
        CODEGEN_ASSERT(it != identifiedModules.end());

        identifiedModules.erase(it);
    }
    else
    {
        CODEGEN_ASSERT(anonymousModuleCount.fetch_sub(1) != 0);
        delete &nativeModule;
    }
}

[[nodiscard]] NativeModuleRef SharedCodeAllocator::tryGetNativeModuleWithLockHeld(const ModuleId& moduleId) const noexcept
{
    const auto it = identifiedModules.find(moduleId);
    if (it == identifiedModules.end())
        return NativeModuleRef{};

    return NativeModuleRef{it->second.get()};
}

[[nodiscard]] size_t SharedCodeAllocator::ModuleIdHash::operator()(const ModuleId& moduleId) const noexcept
{
    return std::hash<std::string_view>{}(std::string_view{reinterpret_cast<const char*>(moduleId.data()), moduleId.size()});
}

} // namespace CodeGen
} // namespace Luau
