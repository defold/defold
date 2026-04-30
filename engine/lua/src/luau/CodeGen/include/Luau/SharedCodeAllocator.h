// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/CodeAllocationData.h"
#include "Luau/CodeGen.h"
#include "Luau/Common.h"
#include "Luau/NativeProtoExecData.h"

#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace Luau
{
namespace CodeGen
{

// SharedCodeAllocator is a native executable code allocator that provides
// shared ownership of the native code.  Code is allocated on a per-module
// basis.  Each module is uniquely identifiable via an id, which may be a hash
// or other unique value.  Each module may contain multiple natively compiled
// functions (protos).
//
// The module is the unit of shared ownership (i.e., it is where the reference
// count is maintained).


struct CodeAllocator;
class NativeModule;
class NativeModuleRef;
class SharedCodeAllocator;


// A NativeModule represents a single natively-compiled module (script).  It is
// the unit of shared ownership and is thus where the reference count is
// maintained.  It owns a set of NativeProtos, with associated native exec data,
// and the allocated native data and code.
class NativeModule
{
public:
    NativeModule(
        SharedCodeAllocator* allocator,
        const std::optional<ModuleId>& moduleId,
        const uint8_t* moduleBaseAddress,
        std::vector<NativeProtoExecDataPtr> nativeProtos
    ) noexcept;
    NativeModule(
        SharedCodeAllocator* allocator,
        const std::optional<ModuleId>& moduleId,
        CodeAllocationData codeAllocationData,
        std::vector<NativeProtoExecDataPtr> nativeProtos
    ) noexcept;

    NativeModule(const NativeModule&) = delete;
    NativeModule(NativeModule&&) = delete;
    NativeModule& operator=(const NativeModule&) = delete;
    NativeModule& operator=(NativeModule&&) = delete;

    // The NativeModule must not be destroyed if there are any outstanding
    // references.  It should thus only be destroyed by a call to release()
    // that releases the last reference.
    ~NativeModule() noexcept;

    size_t addRef() const noexcept;
    size_t addRefs(size_t count) const noexcept;
    size_t release() const noexcept;
    [[nodiscard]] size_t getRefcount() const noexcept;

    [[nodiscard]] const std::optional<ModuleId>& getModuleId() const noexcept;

    // Gets the base address of the executable native code for the module.
    [[nodiscard]] const uint8_t* getModuleBaseAddress() const noexcept;

    // Gets the information about code allocation for this module.
    [[nodiscard]] CodeAllocationData getCodeAllocationData() const noexcept;

    // Attempts to find the NativeProto with the given bytecode id.  If no
    // NativeProto for that bytecode id exists, a null pointer is returned.
    [[nodiscard]] const uint32_t* tryGetNativeProto(uint32_t bytecodeId) const noexcept;

    [[nodiscard]] const std::vector<NativeProtoExecDataPtr>& getNativeProtos() const noexcept;

private:
    mutable std::atomic<size_t> refcount = 0;

    SharedCodeAllocator* allocator = nullptr;
    std::optional<ModuleId> moduleId = {};
    const uint8_t* moduleBaseAddress_DEPRECATED = nullptr;
    CodeAllocationData codeAllocationData;

    std::vector<NativeProtoExecDataPtr> nativeProtos = {};
};

// A NativeModuleRef is an owning reference to a NativeModule.  (Note:  We do
// not use shared_ptr, to avoid complex state management in the Luau GC Proto
// object.)
class NativeModuleRef
{
public:
    NativeModuleRef() noexcept = default;
    NativeModuleRef(const NativeModule* nativeModule) noexcept;

    NativeModuleRef(const NativeModuleRef& other) noexcept;
    NativeModuleRef(NativeModuleRef&& other) noexcept;
    NativeModuleRef& operator=(NativeModuleRef other) noexcept;

    ~NativeModuleRef() noexcept;

    void reset() noexcept;
    void swap(NativeModuleRef& other) noexcept;

    [[nodiscard]] bool empty() const noexcept;
    explicit operator bool() const noexcept;

    [[nodiscard]] const NativeModule* get() const noexcept;
    [[nodiscard]] const NativeModule* operator->() const noexcept;
    [[nodiscard]] const NativeModule& operator*() const noexcept;

private:
    const NativeModule* nativeModule = nullptr;
};

class SharedCodeAllocator
{
public:
    SharedCodeAllocator(CodeAllocator* codeAllocator) noexcept;

    SharedCodeAllocator(const SharedCodeAllocator&) = delete;
    SharedCodeAllocator(SharedCodeAllocator&&) = delete;
    SharedCodeAllocator& operator=(const SharedCodeAllocator&) = delete;
    SharedCodeAllocator& operator=(SharedCodeAllocator&&) = delete;

    ~SharedCodeAllocator() noexcept;

    // If we have a NativeModule for the given ModuleId, an owning reference to
    // it is returned.  Otherwise, an empty NativeModuleRef is returned.
    [[nodiscard]] NativeModuleRef tryGetNativeModule(const ModuleId& moduleId) const noexcept;

    // If we have a NativeModule for the given ModuleId, an owning reference to
    // it is returned.  Otherwise, a new NativeModule is created for that ModuleId
    // using the provided NativeProtos, data, and code (space is allocated for the
    // data and code such that it can be executed).  Like std::map::insert, the
    // bool result is true if a new module was created; false if an existing
    // module is being returned.
    std::pair<NativeModuleRef, bool> getOrInsertNativeModule(
        const ModuleId& moduleId,
        std::vector<NativeProtoExecDataPtr> nativeProtos,
        const uint8_t* data,
        size_t dataSize,
        const uint8_t* code,
        size_t codeSize
    );

    NativeModuleRef insertAnonymousNativeModule(
        std::vector<NativeProtoExecDataPtr> nativeProtos,
        const uint8_t* data,
        size_t dataSize,
        const uint8_t* code,
        size_t codeSize
    );

    // If a NativeModule exists for the given ModuleId and that NativeModule
    // is no longer referenced, the NativeModule is destroyed.  This should
    // usually only be called by NativeModule::release() when the reference
    // count becomes zero
    void eraseNativeModuleIfUnreferenced(const NativeModule& nativeModule);

private:
    struct ModuleIdHash
    {
        [[nodiscard]] size_t operator()(const ModuleId& moduleId) const noexcept;
    };

    [[nodiscard]] NativeModuleRef tryGetNativeModuleWithLockHeld(const ModuleId& moduleId) const noexcept;

    mutable std::mutex mutex;

    std::unordered_map<ModuleId, std::unique_ptr<NativeModule>, ModuleIdHash, std::equal_to<>> identifiedModules;

    std::atomic<size_t> anonymousModuleCount = 0;

    CodeAllocator* codeAllocator = nullptr;
};

} // namespace CodeGen
} // namespace Luau
