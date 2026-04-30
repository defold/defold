// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/CodeBlockUnwind.h"

#include "Luau/CodeAllocator.h"
#include "Luau/CodeGenCommon.h"
#include "Luau/UnwindBuilder.h"

#include <string.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(CODEGEN_TARGET_X64)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#elif (defined(__linux__) || defined(__APPLE__)) && (defined(CODEGEN_TARGET_X64) || defined(CODEGEN_TARGET_A64))

// __register_frame and __deregister_frame are defined in libgcc or libc++
// (depending on how it's built). We want to declare them as weak symbols
// so that if they're provided by a shared library, we'll use them, and if
// not, we'll disable some c++ exception handling support. However, if they're
// declared as weak and the definitions are linked in a static library
// that's not linked with whole-archive, then the symbols will technically be defined here,
// and the linker won't look for the strong ones in the library.
#ifndef LUAU_ENABLE_REGISTER_FRAME
#define REGISTER_FRAME_WEAK __attribute__((weak))
#else
#define REGISTER_FRAME_WEAK
#endif

extern "C" void __register_frame(const void*) REGISTER_FRAME_WEAK;
extern "C" void __deregister_frame(const void*) REGISTER_FRAME_WEAK;

extern "C" void __unw_add_dynamic_fde() __attribute__((weak));
#endif

#if defined(__APPLE__) && defined(CODEGEN_TARGET_A64)
#include <sys/sysctl.h>
#include <mach-o/loader.h>
#include <dlfcn.h>

struct unw_dynamic_unwind_sections_t
{
    uintptr_t dso_base;
    uintptr_t dwarf_section;
    size_t dwarf_section_length;
    uintptr_t compact_unwind_section;
    size_t compact_unwind_section_length;
};

typedef int (*unw_add_find_dynamic_unwind_sections_t)(int (*)(uintptr_t addr, unw_dynamic_unwind_sections_t* info));
#endif

namespace Luau
{
namespace CodeGen
{

#if defined(__APPLE__) && defined(CODEGEN_TARGET_A64)
static int findDynamicUnwindSections(uintptr_t addr, unw_dynamic_unwind_sections_t* info)
{
    // Define a minimal mach header for JIT'd code.
    static const mach_header_64 kFakeMachHeader = {
        MH_MAGIC_64,
        CPU_TYPE_ARM64,
        CPU_SUBTYPE_ARM64_ALL,
        MH_DYLIB,
    };

    info->dso_base = (uintptr_t)&kFakeMachHeader;
    info->dwarf_section = 0;
    info->dwarf_section_length = 0;
    info->compact_unwind_section = 0;
    info->compact_unwind_section_length = 0;
    return 1;
}
#endif

#if (defined(__linux__) || defined(__APPLE__)) && (defined(CODEGEN_TARGET_X64) || defined(CODEGEN_TARGET_A64))
static void visitFdeEntries(char* pos, void (*cb)(const void*))
{
    // When using glibc++ unwinder, we need to call __register_frame/__deregister_frame on the entire .eh_frame data
    // When using libc++ unwinder (libunwind), each FDE has to be handled separately
    // libc++ unwinder is the macOS unwinder, but on Linux the unwinder depends on the library the executable is linked with
    // __unw_add_dynamic_fde is specific to libc++ unwinder, as such we determine the library based on its existence
    if (__unw_add_dynamic_fde == nullptr)
        return cb(pos);

    for (;;)
    {
        unsigned partLength;
        memcpy(&partLength, pos, sizeof(partLength));

        if (partLength == 0) // Zero-length section signals completion
            break;

        unsigned partId;
        memcpy(&partId, pos + 4, sizeof(partId));

        if (partId != 0) // Skip CIE part
            cb(pos);     // CIE is found using an offset in FDE

        pos += partLength + 4;
    }
}
#endif

void* createBlockUnwindInfo(void* context, uint8_t* block, size_t blockSize, size_t& beginOffset)
{
    UnwindBuilder* unwind = (UnwindBuilder*)context;

    // All unwinding related data is placed together at the start of the block
    size_t unwindSize = unwind->getUnwindInfoSize(blockSize);
    unwindSize = (unwindSize + (kCodeAlignment - 1)) & ~(kCodeAlignment - 1); // Match code allocator alignment
    CODEGEN_ASSERT(blockSize >= unwindSize);

    char* unwindData = (char*)block;
    [[maybe_unused]] size_t functionCount = unwind->finalize(unwindData, unwindSize, block, blockSize);

#if defined(_WIN32) && defined(CODEGEN_TARGET_X64)

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM)
    if (!RtlAddFunctionTable((RUNTIME_FUNCTION*)block, uint32_t(functionCount), uintptr_t(block)))
    {
        CODEGEN_ASSERT(!"Failed to allocate function table");
        return nullptr;
    }
#endif

#elif (defined(__linux__) || defined(__APPLE__)) && (defined(CODEGEN_TARGET_X64) || defined(CODEGEN_TARGET_A64))
    if (!&__register_frame)
        return nullptr;

    visitFdeEntries(unwindData, __register_frame);
#endif

#if defined(__APPLE__) && defined(CODEGEN_TARGET_A64)
    // Starting from macOS 14, we need to register unwind section callback to state that our ABI doesn't require pointer authentication
    // This might conflict with other JITs that do the same; unfortunately this is the best we can do for now.
    static unw_add_find_dynamic_unwind_sections_t unw_add_find_dynamic_unwind_sections =
        unw_add_find_dynamic_unwind_sections_t(dlsym(RTLD_DEFAULT, "__unw_add_find_dynamic_unwind_sections"));
    static int regonce = unw_add_find_dynamic_unwind_sections ? unw_add_find_dynamic_unwind_sections(findDynamicUnwindSections) : 0;
    CODEGEN_ASSERT(regonce == 0);
#endif

    beginOffset = unwindSize + unwind->getBeginOffset();
    return block;
}

void destroyBlockUnwindInfo(void* context, void* unwindData)
{
#if defined(_WIN32) && defined(CODEGEN_TARGET_X64)

#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_SYSTEM)
    if (!RtlDeleteFunctionTable((RUNTIME_FUNCTION*)unwindData))
        CODEGEN_ASSERT(!"Failed to deallocate function table");
#endif

#elif (defined(__linux__) || defined(__APPLE__)) && (defined(CODEGEN_TARGET_X64) || defined(CODEGEN_TARGET_A64))
    if (!&__deregister_frame)
    {
        CODEGEN_ASSERT(!"Cannot deregister unwind information");
        return;
    }

    visitFdeEntries((char*)unwindData, __deregister_frame);
#endif
}

bool isUnwindSupported()
{
#if defined(_WIN32) && defined(CODEGEN_TARGET_X64)
    return true;
#elif defined(__APPLE__) && defined(CODEGEN_TARGET_A64)
    char ver[256];
    size_t verLength = sizeof(ver);
    // libunwind on macOS 12 and earlier (which maps to osrelease 21) assumes JIT frames use pointer authentication without a way to override that
    return sysctlbyname("kern.osrelease", ver, &verLength, NULL, 0) == 0 && atoi(ver) >= 22;
#elif (defined(__linux__) || defined(__APPLE__)) && (defined(CODEGEN_TARGET_X64) || defined(CODEGEN_TARGET_A64))
    return true;
#else
    return false;
#endif
}

} // namespace CodeGen
} // namespace Luau
