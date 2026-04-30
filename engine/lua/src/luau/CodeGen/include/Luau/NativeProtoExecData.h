// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <memory>
#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

// The NativeProtoExecData is constant metadata associated with a NativeProto.
// We generally refer to the NativeProtoExecData via a pointer to the instruction
// offsets array because this makes the logic in the entry gate simpler.

class NativeModule;

struct NativeProtoExecDataHeader
{
    // The NativeModule that owns this NativeProto.  This is initialized
    // when the NativeProto is bound to the NativeModule via assignToModule().
    NativeModule* nativeModule = nullptr;

    // We store the native code offset until the code is allocated in executable
    // pages, after which point we store the actual address.
    const uint8_t* entryOffsetOrAddress = nullptr;

    // The bytecode id of the proto
    uint32_t bytecodeId = 0;

    // The number of bytecode instructions in the proto.  This is the number of
    // elements in the instruction offsets array following this header.
    uint32_t bytecodeInstructionCount = 0;

    // The number of extra uin32_t elements of custom data after the bytecode offsets
    uint32_t extraDataCount = 0;

    // The size of the native code for this NativeProto, in bytes.
    size_t nativeCodeSize = 0;
};

// Make sure that the instruction offsets array following the header will be
// correctly aligned:
static_assert(sizeof(NativeProtoExecDataHeader) % sizeof(uint32_t) == 0);

struct NativeProtoExecDataDeleter
{
    void operator()(const uint32_t* instructionOffsets) const noexcept;
};

using NativeProtoExecDataPtr = std::unique_ptr<uint32_t[], NativeProtoExecDataDeleter>;

[[nodiscard]] NativeProtoExecDataPtr createNativeProtoExecData(uint32_t bytecodeInstructionCount, uint32_t extraDataCount);
void destroyNativeProtoExecData(const uint32_t* instructionOffsets) noexcept;

[[nodiscard]] NativeProtoExecDataHeader& getNativeProtoExecDataHeader(uint32_t* instructionOffsets) noexcept;
[[nodiscard]] const NativeProtoExecDataHeader& getNativeProtoExecDataHeader(const uint32_t* instructionOffsets) noexcept;

} // namespace CodeGen
} // namespace Luau
