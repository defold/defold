// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Bytecode.h"
#include "Luau/Common.h"
#include "Luau/DenseHash.h"
#include "Luau/IrData.h"

#include <vector>

struct Proto;
typedef uint32_t Instruction;

namespace Luau
{
namespace CodeGen
{

struct HostIrHooks;

struct IrBuilder
{
    IrBuilder(const HostIrHooks& hostHooks);

    void buildFunctionIr(Proto* proto);

    void rebuildBytecodeBasicBlocks(Proto* proto);
    void translateInst(LuauOpcode op, const Instruction* pc, int i);
    void handleFastcallFallback(IrOp fallbackOrUndef, const Instruction* pc, int i);

    bool isInternalBlock(IrOp block);
    void beginBlock(IrOp block);

    void loadAndCheckTag(IrOp loc, uint8_t tag, IrOp fallback);
    void checkSafeEnv(int pcpos);

    // Clones all instructions into the current block
    // Source block that is cloned cannot use values coming in from a predecessor
    void clone(std::vector<uint32_t> sourceIdxs, bool removeCurrentTerminator);

    IrOp undef();

    IrOp constInt(int value);
    IrOp constInt64(int64_t value);
    IrOp constUint(unsigned value);
    IrOp constImport(unsigned value);
    IrOp constDouble(double value);
    IrOp constTag(uint8_t value);
    IrOp constAny(IrConst constant, uint64_t asCommonKey);

    IrOp cond(IrCondition cond);

    IrOp inst(IrCmd cmd, const IrOps& ops);
    IrOp inst(IrCmd cmd, std::initializer_list<IrOp> ops);
    IrOp inst(IrCmd cmd);
    IrOp inst(IrCmd cmd, IrOp a);
    IrOp inst(IrCmd cmd, IrOp a, IrOp b);
    IrOp inst(IrCmd cmd, IrOp a, IrOp b, IrOp c);
    IrOp inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d);
    IrOp inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d, IrOp e);
    IrOp inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d, IrOp e, IrOp f);
    IrOp inst(IrCmd cmd, IrOp a, IrOp b, IrOp c, IrOp d, IrOp e, IrOp f, IrOp g);

    IrOp block(IrBlockKind kind); // Requested kind can be ignored if we are in an outlined sequence
    IrOp blockAtInst(uint32_t index);
    IrOp fallbackBlock(uint32_t pcpos);

    IrOp vmReg(uint8_t index);
    IrOp vmConst(uint32_t index);
    IrOp vmUpvalue(uint8_t index);

    IrOp vmExit(uint32_t pcpos);

    const HostIrHooks& hostHooks;

    bool inTerminatedBlock = false;

    bool interruptRequested = false;

    bool activeFastcallFallback = false;
    IrOp fastcallFallbackReturn;

    // Force builder to skip source commands
    int cmdSkipTarget = -1;

    IrFunction function;

    uint32_t activeBlockIdx = ~0u;

    std::vector<uint32_t> instIndexToBlock; // Block index at the bytecode instruction

    struct LoopInfo
    {
        IrOp step;
        int startpc = 0;
    };

    std::vector<LoopInfo> numericLoopStack;

    // Similar to BytecodeBuilder, duplicate constants are removed used the same method
    struct ConstantKey
    {
        IrConstKind kind;
        // Note: this stores value* from IrConst; when kind is Double, this stores the same bits as double does but in uint64_t.
        uint64_t value;

        bool operator==(const ConstantKey& key) const
        {
            return kind == key.kind && value == key.value;
        }
    };

    struct ConstantKeyHash
    {
        size_t operator()(const ConstantKey& key) const
        {
            // finalizer from MurmurHash64B
            const uint32_t m = 0x5bd1e995;

            uint32_t h1 = uint32_t(key.value);
            uint32_t h2 = uint32_t(key.value >> 32) ^ (int(key.kind) * m);

            h1 ^= h2 >> 18;
            h1 *= m;
            h2 ^= h1 >> 22;
            h2 *= m;
            h1 ^= h2 >> 17;
            h1 *= m;
            h2 ^= h1 >> 19;
            h2 *= m;

            // ... truncated to 32-bit output (normally hash is equal to (uint64_t(h1) << 32) | h2, but we only really need the lower 32-bit half)
            return size_t(h2);
        }
    };

    DenseHashMap<ConstantKey, uint32_t, ConstantKeyHash> constantMap;
};

} // namespace CodeGen
} // namespace Luau
