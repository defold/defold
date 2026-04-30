// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/SharedCodeAllocator.h"

#include "NativeState.h"

#include <memory>
#include <optional>
#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

// The "code-gen context" maintains the native code-gen state.  There are two
// implementations.  The StandaloneCodeGenContext is a VM-specific context type.
// It is the "simple" implementation that can be used when native code-gen is
// used with a single Luau VM.  The SharedCodeGenContext supports use from
// multiple Luau VMs concurrently, and allows for sharing of executable native
// code and related metadata.

struct ModuleBindResult
{
    CodeGenCompilationResult compilationResult = {};

    uint32_t functionsBound = 0;
};

class BaseCodeGenContext
{
public:
    BaseCodeGenContext(size_t blockSize, size_t maxTotalSize, AllocationCallback* allocationCallback, void* allocationCallbackContext);
    virtual ~BaseCodeGenContext();

    [[nodiscard]] bool initHeaderFunctions();

    [[nodiscard]] virtual std::optional<ModuleBindResult> tryBindExistingModule(
        const ModuleId& moduleId,
        const std::vector<Proto*>& moduleProtos
    ) = 0;

    [[nodiscard]] virtual ModuleBindResult bindModule(
        const std::optional<ModuleId>& moduleId,
        const std::vector<Proto*>& moduleProtos,
        std::vector<NativeProtoExecDataPtr> nativeExecDatas,
        const uint8_t* data,
        size_t dataSize,
        const uint8_t* code,
        size_t codeSize
    ) = 0;

    virtual void onCloseState() noexcept = 0;
    virtual void onDestroyFunction(void* execdata) noexcept = 0;

    CodeAllocator codeAllocator;
    std::unique_ptr<UnwindBuilder> unwindBuilder;

    uint8_t* gateData_DEPRECATED = nullptr;
    size_t gateDataSize_DEPRECATED = 0;
    CodeAllocationData gateAllocationData;

    void* userdataRemappingContext = nullptr;
    UserdataRemapperCallback* userdataRemapper = nullptr;

    NativeContext context;
};

class StandaloneCodeGenContext final : public BaseCodeGenContext
{
public:
    StandaloneCodeGenContext(size_t blockSize, size_t maxTotalSize, AllocationCallback* allocationCallback, void* allocationCallbackContext);

    [[nodiscard]] std::optional<ModuleBindResult> tryBindExistingModule(const ModuleId& moduleId, const std::vector<Proto*>& moduleProtos) override;

    [[nodiscard]] ModuleBindResult bindModule(
        const std::optional<ModuleId>& moduleId,
        const std::vector<Proto*>& moduleProtos,
        std::vector<NativeProtoExecDataPtr> nativeExecDatas,
        const uint8_t* data,
        size_t dataSize,
        const uint8_t* code,
        size_t codeSize
    ) override;

    void onCloseState() noexcept override;
    void onDestroyFunction(void* execdata) noexcept override;

private:
    SharedCodeAllocator sharedAllocator;
};

class SharedCodeGenContext final : public BaseCodeGenContext
{
public:
    SharedCodeGenContext(size_t blockSize, size_t maxTotalSize, AllocationCallback* allocationCallback, void* allocationCallbackContext);

    [[nodiscard]] std::optional<ModuleBindResult> tryBindExistingModule(const ModuleId& moduleId, const std::vector<Proto*>& moduleProtos) override;

    [[nodiscard]] ModuleBindResult bindModule(
        const std::optional<ModuleId>& moduleId,
        const std::vector<Proto*>& moduleProtos,
        std::vector<NativeProtoExecDataPtr> nativeExecDatas,
        const uint8_t* data,
        size_t dataSize,
        const uint8_t* code,
        size_t codeSize
    ) override;

    void onCloseState() noexcept override;
    void onDestroyFunction(void* execdata) noexcept override;

private:
    SharedCodeAllocator sharedAllocator;
};

} // namespace CodeGen
} // namespace Luau
