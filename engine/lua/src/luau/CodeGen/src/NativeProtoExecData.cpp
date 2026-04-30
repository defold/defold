// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/NativeProtoExecData.h"

#include "Luau/Common.h"

#include <new>

namespace Luau
{
namespace CodeGen
{

[[nodiscard]] static size_t computeNativeExecDataSize(uint32_t bytecodeInstructionCount, uint32_t extraDataCount) noexcept
{
    return sizeof(NativeProtoExecDataHeader) + (bytecodeInstructionCount * sizeof(uint32_t)) + (extraDataCount * sizeof(uint32_t));
}

void NativeProtoExecDataDeleter::operator()(const uint32_t* instructionOffsets) const noexcept
{
    destroyNativeProtoExecData(instructionOffsets);
}

[[nodiscard]] NativeProtoExecDataPtr createNativeProtoExecData(uint32_t bytecodeInstructionCount, uint32_t extraDataCount)
{
    std::unique_ptr<uint8_t[]> bytes = std::make_unique<uint8_t[]>(computeNativeExecDataSize(bytecodeInstructionCount, extraDataCount));
    new (static_cast<void*>(bytes.get())) NativeProtoExecDataHeader{};
    return NativeProtoExecDataPtr{reinterpret_cast<uint32_t*>(bytes.release() + sizeof(NativeProtoExecDataHeader))};
}

void destroyNativeProtoExecData(const uint32_t* instructionOffsets) noexcept
{
    const NativeProtoExecDataHeader* header = &getNativeProtoExecDataHeader(instructionOffsets);
    header->~NativeProtoExecDataHeader();
    delete[] reinterpret_cast<const uint8_t*>(header);
}

[[nodiscard]] NativeProtoExecDataHeader& getNativeProtoExecDataHeader(uint32_t* instructionOffsets) noexcept
{
    return *reinterpret_cast<NativeProtoExecDataHeader*>(reinterpret_cast<uint8_t*>(instructionOffsets) - sizeof(NativeProtoExecDataHeader));
}

[[nodiscard]] const NativeProtoExecDataHeader& getNativeProtoExecDataHeader(const uint32_t* instructionOffsets) noexcept
{
    return *reinterpret_cast<const NativeProtoExecDataHeader*>(
        reinterpret_cast<const uint8_t*>(instructionOffsets) - sizeof(NativeProtoExecDataHeader)
    );
}

} // namespace CodeGen
} // namespace Luau
