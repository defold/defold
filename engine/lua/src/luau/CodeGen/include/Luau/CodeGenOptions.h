// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include <string>

#include <stddef.h>
#include <stdint.h>

namespace Luau
{
namespace CodeGen
{

enum CodeGenFlags
{
    // Only run native codegen for modules that have been marked with --!native
    CodeGen_OnlyNativeModules = 1 << 0,
    // Run native codegen for functions that the compiler considers not profitable
    CodeGen_ColdFunctions = 1 << 1,
};

enum class CodeGenCounter : unsigned
{
    RegularBlockExecuted = 1,
    FallbackBlockExecuted = 2,
    VmExitTaken = 3,
};

using AllocationCallback = void(void* context, void* oldPointer, size_t oldSize, void* newPointer, size_t newSize);

struct IrBuilder;
struct IrOp;

using HostVectorOperationBytecodeType = uint8_t (*)(const char* member, size_t memberLength);
using HostVectorAccessHandler = bool (*)(IrBuilder& builder, const char* member, size_t memberLength, int resultReg, int sourceReg, int pcpos);
using HostVectorNamecallHandler =
    bool (*)(IrBuilder& builder, const char* member, size_t memberLength, int argResReg, int sourceReg, int params, int results, int pcpos);

enum class HostMetamethod
{
    Add,
    Sub,
    Mul,
    Div,
    Idiv,
    Mod,
    Pow,
    Minus,
    Equal,
    LessThan,
    LessEqual,
    Length,
    Concat,
};

using HostUserdataOperationBytecodeType = uint8_t (*)(uint8_t type, const char* member, size_t memberLength);
using HostUserdataMetamethodBytecodeType = uint8_t (*)(uint8_t lhsTy, uint8_t rhsTy, HostMetamethod method);
using HostUserdataAccessHandler =
    bool (*)(IrBuilder& builder, uint8_t type, const char* member, size_t memberLength, int resultReg, int sourceReg, int pcpos);
using HostUserdataMetamethodHandler =
    bool (*)(IrBuilder& builder, uint8_t lhsTy, uint8_t rhsTy, int resultReg, IrOp lhs, IrOp rhs, HostMetamethod method, int pcpos);
using HostUserdataNamecallHandler = bool (*)(
    IrBuilder& builder,
    uint8_t type,
    const char* member,
    size_t memberLength,
    int argResReg,
    int sourceReg,
    int params,
    int results,
    int pcpos
);

struct HostIrHooks
{
    // Suggest result type of a vector field access
    HostVectorOperationBytecodeType vectorAccessBytecodeType = nullptr;

    // Suggest result type of a vector function namecall
    HostVectorOperationBytecodeType vectorNamecallBytecodeType = nullptr;

    // Handle vector value field access
    // 'sourceReg' is guaranteed to be a vector
    // Guards should take a VM exit to 'pcpos'
    HostVectorAccessHandler vectorAccess = nullptr;

    // Handle namecall performed on a vector value
    // 'sourceReg' (self argument) is guaranteed to be a vector
    // All other arguments can be of any type
    // Guards should take a VM exit to 'pcpos'
    HostVectorNamecallHandler vectorNamecall = nullptr;

    // Suggest result type of a userdata field access
    HostUserdataOperationBytecodeType userdataAccessBytecodeType = nullptr;

    // Suggest result type of a metamethod call
    HostUserdataMetamethodBytecodeType userdataMetamethodBytecodeType = nullptr;

    // Suggest result type of a userdata namecall
    HostUserdataOperationBytecodeType userdataNamecallBytecodeType = nullptr;

    // Handle userdata value field access
    // 'sourceReg' is guaranteed to be a userdata, but tag has to be checked
    // Write to 'resultReg' might invalidate 'sourceReg'
    // Guards should take a VM exit to 'pcpos'
    HostUserdataAccessHandler userdataAccess = nullptr;

    // Handle metamethod operation on a userdata value
    // 'lhs' and 'rhs' operands can be VM registers of constants
    // Operand types have to be checked and userdata operand tags have to be checked
    // Write to 'resultReg' might invalidate source operands
    // Guards should take a VM exit to 'pcpos'
    HostUserdataMetamethodHandler userdataMetamethod = nullptr;

    // Handle namecall performed on a userdata value
    // 'sourceReg' (self argument) is guaranteed to be a userdata, but tag has to be checked
    // All other arguments can be of any type
    // Guards should take a VM exit to 'pcpos'
    HostUserdataNamecallHandler userdataNamecall = nullptr;
};

struct CompilationOptions
{
    unsigned int flags = 0;
    HostIrHooks hooks;

    // null-terminated array of userdata types names that might have custom lowering
    const char* const* userdataTypes = nullptr;

    bool recordCounters = false;
};

using AnnotatorFn = void (*)(void* context, std::string& result, int fid, int instpos);

// Output "#" before IR blocks and instructions
enum class IncludeIrPrefix
{
    No,
    Yes
};

// Output user count and last use information of blocks and instructions
enum class IncludeUseInfo
{
    No,
    Yes
};

// Output CFG informations like block predecessors, successors and etc
enum class IncludeCfgInfo
{
    No,
    Yes
};

// Output VM register live in/out information for blocks
enum class IncludeRegFlowInfo
{
    No,
    Yes
};

struct AssemblyOptions
{
    enum Target
    {
        Host,
        A64,
        A64_NoFeatures,
        X64_Windows,
        X64_SystemV,
    };

    Target target = Host;

    CompilationOptions compilationOptions;

    bool outputBinary = false;

    bool includeAssembly = false;
    bool includeIr = false;
    bool includeOutlinedCode = false;
    bool includeIrTypes = false;

    IncludeIrPrefix includeIrPrefix = IncludeIrPrefix::Yes;
    IncludeUseInfo includeUseInfo = IncludeUseInfo::Yes;
    IncludeCfgInfo includeCfgInfo = IncludeCfgInfo::Yes;
    IncludeRegFlowInfo includeRegFlowInfo = IncludeRegFlowInfo::Yes;

    // Optional annotator function can be provided to describe each instruction, it takes function id and sequential instruction id
    AnnotatorFn annotator = nullptr;
    void* annotatorContext = nullptr;
};

} // namespace CodeGen
} // namespace Luau
