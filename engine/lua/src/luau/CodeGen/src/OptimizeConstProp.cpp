// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Luau/OptimizeConstProp.h"

#include "Luau/DenseHash.h"
#include "Luau/IrData.h"
#include "Luau/IrBuilder.h"
#include "Luau/IrUtils.h"

#include "lua.h"
#include "lobject.h"
#include "lstate.h"

#include <limits.h>
#include <math.h>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

LUAU_FASTINTVARIABLE(LuauCodeGenMinLinearBlockPath, 3)
LUAU_FASTINTVARIABLE(LuauCodeGenReuseSlotLimit, 64)
LUAU_FASTINTVARIABLE(LuauCodeGenReuseUdataTagLimit, 64)
LUAU_FASTINTVARIABLE(LuauCodeGenLiveSlotReuseLimit, 8)
LUAU_FASTFLAG(LuauCodegenBufNoDefTag)
LUAU_FASTFLAGVARIABLE(DebugLuauAbortingChecks)
LUAU_FASTFLAGVARIABLE(LuauCodegenSetBlockEntryState3)
LUAU_FASTFLAGVARIABLE(LuauCodegenBufferRangeMerge4)
LUAU_FASTFLAGVARIABLE(LuauCodegenLengthBaseInst)
LUAU_FASTFLAGVARIABLE(LuauCodegenUserdataAddressAlias)
LUAU_FASTFLAGVARIABLE(LuauCodegenPropagateTagsAcrossChains2)
LUAU_FASTFLAGVARIABLE(LuauCodegenRemoveDuplicateDoubleIntValues)
LUAU_FASTFLAGVARIABLE(LuauCodegenPreciseDupTableEffect)
LUAU_FASTFLAGVARIABLE(LuauCodegenBufferWriteEffects)
LUAU_FASTFLAGVARIABLE(LuauCodegenJumpCmpIntFoldFix)
LUAU_FASTFLAGVARIABLE(LuauCodegenLinearSetupEntryState3)

namespace Luau
{
namespace CodeGen
{

constexpr uint8_t kUpvalueEmptyKey = 0xff;

// Data we know about the register value
struct RegisterInfo
{
    uint8_t tag = 0xff;
    IrOp value;

    // Used to quickly invalidate links between SSA values and register memory
    // It's a bit imprecise where value and tag both always invalidate together
    uint32_t version = 0;

    bool knownNotReadonly = false;
    bool knownNoMetatable = false;
    int knownTableArraySize = -1;
};

// Load instructions are linked to target register to carry knowledge about the target
// We track a register version at the point of the load so it's easy to break the link when register is updated
struct RegisterLink
{
    uint8_t reg = 0;
    uint32_t version = 0;
};

// Reference to an instruction together with the position of that instruction in the current block chain and the last position of reuse
struct NumberedInstruction
{
    uint32_t instIdx = 0;
    uint32_t startPos = 0;
    uint32_t finishPos = 0;
};

struct BufferLoadStoreInfo
{
    IrCmd loadCmd = IrCmd::NOP;
    uint8_t accessSize = 0;
    uint8_t tag = LUA_TNIL;
    bool fromStore = false;

    IrOp address;
    IrOp value;

    int offset = 0;
};

struct ArrayValueEntry
{
    uint32_t pointer;
    IrOp offset;
    uint32_t value;
};

struct NodeSlotState
{
    uint32_t pointer;
    bool knownToNotBeNil;
};

static uint8_t tryGetTagForTypename(std::string_view name, bool forTypeof)
{
    if (name == "nil")
        return LUA_TNIL;

    if (name == "boolean")
        return LUA_TBOOLEAN;

    if (name == "number")
        return LUA_TNUMBER;

    if (name == "integer")
        return LUA_TINTEGER;

    // typeof(vector) can be changed by environment
    // TODO: support the environment option
    if (name == "vector" && !forTypeof)
        return LUA_TVECTOR;

    if (name == "string")
        return LUA_TSTRING;

    if (name == "table")
        return LUA_TTABLE;

    if (name == "function")
        return LUA_TFUNCTION;

    if (name == "thread")
        return LUA_TTHREAD;

    if (name == "buffer")
        return LUA_TBUFFER;

    return 0xff;
}

// Check if we can treat double as an integer in addition and subtraction
static bool safeIntegerConstant(double value)
{
    // Within 32 bits, note that we allow both max unsigned number as well as a negative counterpart
    // Doubles are actually ok within even larger bounds (but not exactly 2^53), but we use the function in 32 bit optimizations
    if (value < -4294967295.0 || value > 4294967295.0)
        return false;

    return double((long long)value) == value;
}

// Data we know about the current VM state
struct ConstPropState
{
    ConstPropState(IrBuilder& build, IrFunction& function)
        : build(build)
        , function(function)
        , valueMap({})
    {
    }

    uint8_t tryGetTag(IrOp op)
    {
        if (RegisterInfo* info = tryGetRegisterInfo(op))
        {
            if (info->tag != 0xff)
                return info->tag;
        }

        // SSA register might be associated by a tag through a CHECK_TAG
        if (op.kind == IrOpKind::Inst)
        {
            if (uint8_t* info = instTag.find(op.index))
                return *info;
        }

        return 0xff;
    }

    void updateTag(IrOp op, uint8_t tag)
    {
        if (RegisterInfo* info = tryGetRegisterInfo(op))
            info->tag = tag;
    }

    void saveTag(IrOp op, uint8_t tag)
    {
        if (RegisterInfo* info = tryGetRegisterInfo(op))
        {
            if (info->tag != tag)
            {
                info->tag = tag;
                info->version++;
            }
        }
    }

    IrOp tryGetValue(IrOp op)
    {
        if (RegisterInfo* info = tryGetRegisterInfo(op))
            return info->value;

        return IrOp{IrOpKind::None, 0u};
    }

    void saveValue(IrOp op, IrOp value)
    {
        CODEGEN_ASSERT(value.kind == IrOpKind::Constant);

        if (RegisterInfo* info = tryGetRegisterInfo(op))
        {
            if (info->value != value)
            {
                info->value = value;
                info->knownNotReadonly = false;
                info->knownNoMetatable = false;
                info->knownTableArraySize = -1;
                info->version++;
            }
        }
    }

    void invalidate(RegisterInfo& reg, bool invalidateTag, bool invalidateValue)
    {
        if (invalidateTag)
        {
            reg.tag = 0xff;
        }

        if (invalidateValue)
        {
            reg.value = {};
            reg.knownNotReadonly = false;
            reg.knownNoMetatable = false;
            reg.knownTableArraySize = -1;
        }

        reg.version++;
    }

    void invalidateTag(IrOp regOp)
    {
        // TODO: use maxstacksize from Proto
        maxReg = vmRegOp(regOp) > maxReg ? vmRegOp(regOp) : maxReg;
        invalidate(regs[vmRegOp(regOp)], /* invalidateTag */ true, /* invalidateValue */ false);
    }

    void invalidateValue(IrOp regOp)
    {
        // TODO: use maxstacksize from Proto
        maxReg = vmRegOp(regOp) > maxReg ? vmRegOp(regOp) : maxReg;
        invalidate(regs[vmRegOp(regOp)], /* invalidateTag */ false, /* invalidateValue */ true);
    }

    void invalidate(IrOp regOp)
    {
        // TODO: use maxstacksize from Proto
        maxReg = vmRegOp(regOp) > maxReg ? vmRegOp(regOp) : maxReg;
        invalidate(regs[vmRegOp(regOp)], /* invalidateTag */ true, /* invalidateValue */ true);
    }

    void invalidateRegistersFrom(int firstReg)
    {
        for (int i = firstReg; i <= maxReg; ++i)
            invalidate(regs[i], /* invalidateTag */ true, /* invalidateValue */ true);
    }

    void invalidateRegisterRange(int firstReg, int count)
    {
        if (count == -1)
        {
            invalidateRegistersFrom(firstReg);
        }
        else
        {
            for (int i = firstReg; i < firstReg + count && i <= maxReg; ++i)
                invalidate(regs[i], /* invalidateTag */ true, /* invalidateValue */ true);
        }
    }

    void invalidateCapturedRegisters()
    {
        for (int i = 0; i <= maxReg; ++i)
        {
            if (function.cfg.captured.regs.test(i))
                invalidate(regs[i], /* invalidateTag */ true, /* invalidateValue */ true);
        }
    }

    // Value propagation extends the live range of an SSA register
    // In some cases we can't propagate earlier values because we can't guarantee that we will be able to find a storage/restore location
    // As an example, when Luau call is performed, both volatile registers and stack slots might be overwritten
    void invalidateValuePropagation()
    {
        valueMap.clear();
        upvalueMap.clear();

        tryNumToIndexCache.clear();

        bufferLoadStoreInfo.clear();

        hashValueCache.clear();
        arrayValueCache.clear();

        // While other map clears already prevent instValue keys from matching again, this saves memory and map size
        instValue.clear();
    }

    // If table memory has changed, we can't reuse previously computed and validated table slot lookups
    // Same goes for table array elements as well
    void invalidateHeapTableData()
    {
        getSlotNodeCache.clear();

        checkSlotMatchCache.clear();

        getArrAddrCache.clear();
        checkArraySizeCache.clear();

        hashValueCache.clear();
        arrayValueCache.clear();
    }

    void invalidateHeapBufferData()
    {
        checkBufferLenCache.clear();

        bufferLoadStoreInfo.clear();
    }

    void invalidateUserdataData()
    {
        useradataTagCache.clear();
    }

    void invalidateHeap()
    {
        for (int i = 0; i <= maxReg; ++i)
            invalidateHeap(regs[i]);

        invalidateHeapTableData();

        // Buffer length checks are not invalidated since buffer size is immutable

        bufferLoadStoreInfo.clear();
    }

    void invalidateHeap(RegisterInfo& reg)
    {
        reg.knownNotReadonly = false;
        reg.knownNoMetatable = false;
        reg.knownTableArraySize = -1;
    }

    void invalidateUserCall()
    {
        invalidateHeap();
        invalidateCapturedRegisters();

        // We cannot guarantee right now that all live values can be rematerialized from non-stack memory locations
        // To prevent earlier values from being propagated to after operation which might call, we have to clear the maps
        // TODO: remove only the values that don't have a guaranteed restore location
        invalidateValuePropagation();

        upvalueMap.clear();

        inSafeEnv = false;
    }

    void invalidateTableArraySize()
    {
        for (int i = 0; i <= maxReg; ++i)
            invalidateTableArraySize(regs[i]);

        invalidateHeapTableData();
    }

    void invalidateTableArraySize(RegisterInfo& reg)
    {
        reg.knownTableArraySize = -1;
    }

    void createRegLink(uint32_t instIdx, IrOp regOp)
    {
        CODEGEN_ASSERT(!instLink.contains(instIdx));
        instLink[instIdx] = RegisterLink{uint8_t(vmRegOp(regOp)), regs[vmRegOp(regOp)].version};
    }

    RegisterInfo* tryGetRegisterInfo(IrOp op)
    {
        if (op.kind == IrOpKind::VmReg)
        {
            maxReg = vmRegOp(op) > maxReg ? vmRegOp(op) : maxReg;
            return &regs[vmRegOp(op)];
        }

        if (RegisterLink* link = tryGetRegLink(op))
        {
            maxReg = int(link->reg) > maxReg ? int(link->reg) : maxReg;
            return &regs[link->reg];
        }

        return nullptr;
    }

    RegisterLink* tryGetRegLink(IrOp instOp)
    {
        if (instOp.kind != IrOpKind::Inst)
            return nullptr;

        if (RegisterLink* link = instLink.find(instOp.index))
        {
            // Check that the target register hasn't changed the value
            if (link->version < regs[link->reg].version)
                return nullptr;

            return link;
        }

        return nullptr;
    }

    // Attach register version number to the register operand in a load instruction
    // This is used to allow instructions with register references to be compared for equality
    IrInst versionedVmRegLoad(IrCmd loadCmd, IrOp op)
    {
        CODEGEN_ASSERT(op.kind == IrOpKind::VmReg);
        uint32_t version = regs[vmRegOp(op)].version;
        CODEGEN_ASSERT(version <= 0xffffff);
        op.index = vmRegOp(op) | (version << 8);
        return IrInst{loadCmd, {op}};
    }

    // For instructions like LOAD_FLOAT which have an extra offset, we need to record that in the versioned instruction
    IrInst versionedVmRegLoad(IrCmd loadCmd, IrOp opA, IrOp opB)
    {
        IrInst inst = versionedVmRegLoad(loadCmd, opA);

        CODEGEN_ASSERT(inst.ops.size() == 1);
        inst.ops.push_back(opB);

        return inst;
    }

    uint32_t* getPreviousInstIndex(const IrInst& inst)
    {
        if (uint32_t* prevIdx = valueMap.find(inst))
        {
            IrInst& inst = function.instructions[*prevIdx];

            // Previous load might have been removed as unused
            if (inst.useCount != 0 || hasSideEffects(inst.cmd))
                return prevIdx;
        }

        return nullptr;
    }

    uint32_t* getPreviousVersionedLoadIndex(IrCmd cmd, IrOp vmReg)
    {
        CODEGEN_ASSERT(vmReg.kind == IrOpKind::VmReg);
        return getPreviousInstIndex(versionedVmRegLoad(cmd, vmReg));
    }

    std::pair<IrCmd, uint32_t> getPreviousVersionedLoadForTag(uint8_t tag, IrOp vmReg)
    {
        if (!function.cfg.captured.regs.test(vmRegOp(vmReg)))
        {
            if (tag == LUA_TBOOLEAN)
            {
                if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_INT, vmReg))
                    return std::make_pair(IrCmd::LOAD_INT, *prevIdx);
            }
            else if (tag == LUA_TNUMBER)
            {
                if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_DOUBLE, vmReg))
                    return std::make_pair(IrCmd::LOAD_DOUBLE, *prevIdx);
            }
            else if (tag == LUA_TINTEGER)
            {
                if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_INT64, vmReg))
                    return std::make_pair(IrCmd::LOAD_INT64, *prevIdx);
            }
            else if (tag == LUA_TVECTOR)
            {
                if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_FLOAT, vmReg))
                    return std::make_pair(IrCmd::LOAD_FLOAT, *prevIdx);
            }
            else if (isGCO(tag))
            {
                if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_POINTER, vmReg))
                    return std::make_pair(IrCmd::LOAD_POINTER, *prevIdx);
            }
        }

        return std::make_pair(IrCmd::NOP, kInvalidInstIdx);
    }

    // Find existing value of the instruction that is exactly the same, or record current on for future lookups
    void substituteOrRecord(IrInst& inst, uint32_t instIdx)
    {
        if (uint32_t* prevIdx = getPreviousInstIndex(inst))
        {
            substitute(function, inst, IrOp{IrOpKind::Inst, *prevIdx});
            return;
        }

        valueMap[inst] = instIdx;
    }

    // VM register load can be replaced by a previous load of the same version of the register
    // If there is no previous load, we record the current one for future lookups
    bool substituteOrRecordVmRegLoad(IrInst& loadInst)
    {
        CODEGEN_ASSERT(OP_A(loadInst).kind == IrOpKind::VmReg);

        // To avoid captured register invalidation tracking in lowering later, values from loads from captured registers are not propagated
        // This prevents the case where load value location is linked to memory in case of a spill and is then clobbered in a user call
        if (function.cfg.captured.regs.test(vmRegOp(OP_A(loadInst))))
            return false;

        IrInst versionedLoad = loadInst.cmd == IrCmd::LOAD_FLOAT ? versionedVmRegLoad(loadInst.cmd, OP_A(loadInst), OPT_OP_B(loadInst))
                                                                 : versionedVmRegLoad(loadInst.cmd, OP_A(loadInst));

        // Check if there is a value that already has this version of the register
        if (uint32_t* prevIdx = getPreviousInstIndex(versionedLoad))
        {
            // Previous value might not be linked to a register yet
            // For example, it could be a NEW_TABLE stored into a register and we might need to track guards made with this value
            if (!instLink.contains(*prevIdx))
                createRegLink(*prevIdx, OP_A(loadInst));

            // Substitute load instruction with the previous value
            substitute(function, loadInst, IrOp{IrOpKind::Inst, *prevIdx});
            return true;
        }

        uint32_t instIdx = function.getInstIndex(loadInst);

        // Record load of this register version for future substitution
        valueMap[versionedLoad] = instIdx;

        createRegLink(instIdx, OP_A(loadInst));
        return false;
    }

    // VM register loads can use the value that was stored in the same Vm register earlier
    void forwardVmRegStoreToLoad(IrInst& storeInst, IrCmd loadCmd)
    {
        CODEGEN_ASSERT(OP_A(storeInst).kind == IrOpKind::VmReg);
        CODEGEN_ASSERT(OP_B(storeInst).kind == IrOpKind::Inst);

        // To avoid captured register invalidation tracking in lowering later, values from stores into captured registers are not propagated
        // This prevents the case where store creates an alternative value location in case of a spill and is then clobbered in a user call
        if (function.cfg.captured.regs.test(vmRegOp(OP_A(storeInst))))
            return;

        // Future loads of this register version can use the value we stored
        valueMap[versionedVmRegLoad(loadCmd, OP_A(storeInst))] = OP_B(storeInst).index;
    }

    // When loading from a VM register, there might be an instruction that has loaded the whole TValue
    // That TValue might have a CHECK_TAG data associated with it
    bool substituteTagLoadWithTValueData(IrBuilder& build, IrInst& loadInst)
    {
        CODEGEN_ASSERT(OP_A(loadInst).kind == IrOpKind::VmReg);

        if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_TVALUE, OP_A(loadInst)))
        {
            if (uint8_t* tag = instTag.find(*prevIdx); tag && *tag != 0xff)
            {
                substitute(function, loadInst, build.constTag(*tag));
                return true;
            }
        }

        return false;
    }

    // When loading from a VM register, there might be an instruction that has loaded the whole TValue
    // That TValue might have a value data associated with it, if not we will record it here
    bool substituteOrRecordValueLoadWithTValueData(IrBuilder& build, IrInst& loadInst)
    {
        CODEGEN_ASSERT(OP_A(loadInst).kind == IrOpKind::VmReg);

        if (uint32_t* prevIdx = getPreviousVersionedLoadIndex(IrCmd::LOAD_TVALUE, OP_A(loadInst)))
        {
            if (IrOp* valueOp = instValue.find(*prevIdx))
            {
                if (IrInst* value = function.asInstOp(*valueOp))
                {
                    if (value->useCount != 0 && value->cmd == loadInst.cmd)
                    {
                        substitute(function, loadInst, IrOp{IrOpKind::Inst, valueOp->index});
                        return true;
                    }

                    if (value->useCount != 0 && getCmdValueKind(value->cmd) == getCmdValueKind(loadInst.cmd))
                    {
                        substitute(function, loadInst, IrOp{IrOpKind::Inst, valueOp->index});
                        return true;
                    }
                }
                else if (valueOp->kind == IrOpKind::Constant)
                {
                    if (getConstValueKind(function.constOp(*valueOp)) == getCmdValueKind(loadInst.cmd))
                    {
                        substitute(function, loadInst, *valueOp);
                        return true;
                    }
                }
            }
            else
            {
                // Current instruction is now the holder of the value in the TValue
                instValue[*prevIdx] = IrOp{IrOpKind::Inst, function.getInstIndex(loadInst)};
            }
        }

        return false;
    }

    IrInst versionedVmUpvalueLoad(IrInst& loadInst)
    {
        IrOp op = OP_A(loadInst);
        CODEGEN_ASSERT(op.kind == IrOpKind::VmUpvalue);
        uint32_t version = regs[vmUpvalueOp(op)].version;
        CODEGEN_ASSERT(version <= 0xffffff);
        op.index = vmUpvalueOp(OP_A(loadInst)) | (version << 8);
        return IrInst{loadInst.cmd, {op}};
    }

    bool substituteOrRecordVmUpvalueLoad(IrInst& loadInst)
    {
        CODEGEN_ASSERT(OP_A(loadInst).kind == IrOpKind::VmUpvalue);

        if (uint32_t* prevIdx = upvalueMap.find(vmUpvalueOp(OP_A(loadInst))))
        {
            if (*prevIdx != kInvalidInstIdx)
            {
                // Substitute load instruction with the previous value
                substitute(function, loadInst, IrOp{IrOpKind::Inst, *prevIdx});
                return true;
            }
        }

        uint32_t instIdx = function.getInstIndex(loadInst);

        // Record load of this upvalue for future substitution
        upvalueMap[vmUpvalueOp(OP_A(loadInst))] = instIdx;
        return false;
    }

    void forwardVmUpvalueStoreToLoad(IrInst& storeInst)
    {
        // Future loads of this upvalue version can use the value we stored
        upvalueMap[vmUpvalueOp(OP_A(storeInst))] = OP_B(storeInst).index;
    }

    // For a LOAD_FLOAT operation, it is possible to find an operand from a previous STORE_VECTOR instruction
    std::optional<IrOp> findSubstituteComponentLoadFromStoreVector(IrBuilder& build, IrOp vmReg, int offset)
    {
        IrInst versionedLoad = versionedVmRegLoad(IrCmd::LOAD_FLOAT, vmReg);

        // Check if there is a value that already has this version of the register
        if (uint32_t* prevIdx = getPreviousInstIndex(versionedLoad))
        {
            IrInst& store = function.instructions[*prevIdx];
            CODEGEN_ASSERT(store.cmd == IrCmd::STORE_VECTOR);

            IrOp argOp;

            if (offset == 0)
                argOp = OP_B(store);
            else if (offset == 4)
                argOp = OP_C(store);
            else if (offset == 8)
                argOp = OP_D(store);

            if (IrInst* arg = function.asInstOp(argOp))
            {
                // Argument can only be re-used if it contains the value of the same precision
                if (arg->cmd == IrCmd::LOAD_FLOAT || arg->cmd == IrCmd::BUFFER_READF32 || arg->cmd == IrCmd::NUM_TO_FLOAT ||
                    arg->cmd == IrCmd::UINT_TO_FLOAT)
                    return argOp;
            }
            else if (argOp.kind == IrOpKind::Constant)
            {
                // Constant can be used if we bring it down to correct precision
                return build.constDouble(float(function.doubleOp(argOp)));
            }
        }

        return {};
    }

    // When we extract a chain of buffer access offsets, we want to keep the values small enough to fit in arm64 12 bit immediates
    // Of course the combined offset can be larger, but it is validated later
    bool isValidIntegerForImmediate(int i)
    {
        return i >= -4095 && i <= 4095;
    }

    // For double offset, we must also ensure it's a round integer
    bool isValidDoubleForImmediate(double d)
    {
        return d >= -4095.0 && d <= 4095 && double(int(d)) == d;
    }

    struct BufferAccessBase
    {
        IrOp op;
        int scale = 1;
        int offset = 0;
    };

    // Passing through a chain of +/-/*, find the root operand and the combined integer offset
    BufferAccessBase getOffsetBase(IrOp value)
    {
        BufferAccessBase base{value, 1, 0};

        while (true)
        {
            if (base.op.kind != IrOpKind::Inst)
                break;

            IrInst& inst = function.instOp(base.op);

            std::optional<double> lhsNum = function.asDoubleOp(OPT_OP_A(inst));
            std::optional<double> rhsNum = function.asDoubleOp(OPT_OP_B(inst));
            std::optional<int> lhsInt = function.asIntOp(OPT_OP_A(inst));
            std::optional<int> rhsInt = function.asIntOp(OPT_OP_B(inst));

            if (inst.cmd == IrCmd::ADD_NUM && lhsNum && isValidDoubleForImmediate(*lhsNum))
            {
                base.offset += int(*lhsNum) * base.scale;
                base.op = OP_B(inst);
            }
            else if (inst.cmd == IrCmd::ADD_NUM && rhsNum && isValidDoubleForImmediate(*rhsNum))
            {
                base.offset += int(*rhsNum) * base.scale;
                base.op = OP_A(inst);
            }
            else if (inst.cmd == IrCmd::SUB_NUM && rhsNum && isValidDoubleForImmediate(*rhsNum))
            {
                base.offset -= int(*rhsNum) * base.scale;
                base.op = OP_A(inst);
            }
            else if (inst.cmd == IrCmd::MUL_NUM && lhsNum && isValidDoubleForImmediate(*lhsNum))
            {
                base.scale *= int(*lhsNum);
                base.op = OP_B(inst);
            }
            else if (inst.cmd == IrCmd::MUL_NUM && rhsNum && isValidDoubleForImmediate(*rhsNum))
            {
                base.scale *= int(*rhsNum);
                base.op = OP_A(inst);
            }
            else if (inst.cmd == IrCmd::ADD_INT && lhsInt && isValidIntegerForImmediate(*lhsInt))
            {
                base.offset += *lhsInt * base.scale;
                base.op = OP_B(inst);
            }
            else if (inst.cmd == IrCmd::ADD_INT && rhsInt && isValidIntegerForImmediate(*rhsInt))
            {
                base.offset += *rhsInt * base.scale;
                base.op = OP_A(inst);
            }
            else if (inst.cmd == IrCmd::SUB_INT && rhsInt && isValidIntegerForImmediate(*rhsInt))
            {
                base.offset -= *rhsInt * base.scale;
                base.op = OP_A(inst);
            }
            else if (inst.cmd == IrCmd::TRUNCATE_UINT)
            {
                // Ok to pass through TRUNCATE_UINT since ADD_INT/SUB_INT will also establish a truncated value
                base.op = OP_A(inst);
            }
            else
            {
                break;
            }

            // Do not proceed deeper if we have accumulated a number outside [-4095; 4095]
            // Note that it's ok that the current numbers might be outside that range as downstream checks will validate them
            if (!isValidIntegerForImmediate(base.offset) || !isValidIntegerForImmediate(base.scale))
                break;
        }

        return base;
    }

    // Update current offset computation to be based on previous CHECK_BUFFER_LEN base and update min/max range of that check
    bool tryMergeAndKillBufferLengthCheck(IrBuilder& build, IrBlock& block, IrInst& currCheck, IrInst& prevCheck, int extraOffset)
    {
        int prevMinOffset = function.intOp(OP_C(prevCheck));
        int prevMaxOffset = function.intOp(OP_D(prevCheck));

        int currMinOffset = function.intOp(OP_C(currCheck)) + extraOffset;
        int currMaxOffset = function.intOp(OP_D(currCheck)) + extraOffset;

        int newMinOffset = prevMinOffset < currMinOffset ? prevMinOffset : currMinOffset;
        int newMaxOffset = prevMaxOffset > currMaxOffset ? prevMaxOffset : currMaxOffset;

        // If the total access size grows too big, we will not merge it together to avoid immediate operand overflows
        if (newMaxOffset - newMinOffset > 4095)
            return false;

        // If the minimal offset grows to big, we will also abort the merge to avoid immediate operand overflows
        if (newMinOffset < -4095 || newMinOffset > 4095)
            return false;

        if (newMinOffset != prevMinOffset)
            replace(function, OP_C(prevCheck), build.constInt(newMinOffset));

        if (newMaxOffset != prevMaxOffset)
            replace(function, OP_D(prevCheck), build.constInt(newMaxOffset));

        kill(function, currCheck);
        return true;
    }

    // If the offsets are dynamic, but use the same base, we can extend the access size around that base pointer
    bool tryMergeBufferRangeCheck(IrBuilder& build, IrBlock& block, IrInst& inst, IrInst& prev)
    {
        // Can't merge checks between different buffers
        if (OP_A(inst) != OP_A(prev))
            return false;

        IrInst* currIndex = function.asInstOp(OP_B(inst));
        IrInst* prevIndex = function.asInstOp(OP_B(prev));

        if (!currIndex || !prevIndex)
            return false;

        // When both come from a conversion from a double
        if (currIndex->cmd == IrCmd::NUM_TO_INT && prevIndex->cmd == IrCmd::NUM_TO_INT)
        {
            BufferAccessBase offsetBaseCurr = getOffsetBase(OP_A(currIndex));
            BufferAccessBase offsetBasePrev = getOffsetBase(OP_A(prevIndex));

            // If they both are based on the same register (not a constant) with different constant offsets, merge checks
            if (offsetBaseCurr.op == offsetBasePrev.op && offsetBaseCurr.scale == offsetBasePrev.scale &&
                (!FFlag::LuauCodegenLengthBaseInst || offsetBaseCurr.op.kind != IrOpKind::Constant))
            {
                // Difference between base offsets
                int extraOffset = offsetBaseCurr.offset - offsetBasePrev.offset;

                // We want to update our current integer source to be based on the original base plus an extra offset
                if (extraOffset != 0)
                {
                    // But we can only update our source if it was defined after previous source
                    if (OP_B(prev).index >= OP_B(inst).index)
                        return false;

                    // We can replace the way we get our offset from double addition to integer addition
                    replace(function, block, OP_B(inst).index, IrInst{IrCmd::ADD_INT, {OP_B(prev), build.constInt(extraOffset)}});
                }

                // If the way we got the index is from a regular int(d) conversion, we replace it with a checked conversion
                if (OP_E(prev).kind == IrOpKind::Undef)
                    replace(function, OP_E(prev), OP_A(prevIndex));

                return tryMergeAndKillBufferLengthCheck(build, block, inst, prev, extraOffset);
            }
        }
        // Or maybe both are already integers from the same base
        else if (getCmdValueKind(currIndex->cmd) == IrValueKind::Int && getCmdValueKind(prevIndex->cmd) == IrValueKind::Int)
        {
            BufferAccessBase offsetBaseCurr = getOffsetBase(OP_B(inst));
            BufferAccessBase offsetBasePrev = getOffsetBase(OP_B(prev));

            // If they both are based on the same register with different constant offsets, merge checks
            if (offsetBaseCurr.op == offsetBasePrev.op && offsetBaseCurr.scale == offsetBasePrev.scale)
            {
                // Difference between base offsets
                int extraOffset = offsetBaseCurr.offset - offsetBasePrev.offset;

                return tryMergeAndKillBufferLengthCheck(build, block, inst, prev, extraOffset);
            }
        }

        return false;
    }

    void substituteOrRecordBufferLoad(IrBlock& block, uint32_t instIdx, IrInst& loadInst, uint8_t accessSize)
    {
        // Only constant offsets are supported
        if (OP_B(loadInst).kind != IrOpKind::Constant)
            return;

        int offset = function.intOp(OP_B(loadInst));
        uint8_t tag = !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_C(loadInst) ? LUA_TBUFFER : function.tagOp(OP_C(loadInst));

        // Find if we have data for this kind of load
        for (BufferLoadStoreInfo& info : bufferLoadStoreInfo)
        {
            if (info.address == OP_A(loadInst) && info.offset == offset && info.tag == tag)
            {
                // Values from stores can have a different, but compatible types and will convert on read
                if (info.fromStore)
                {
                    switch (loadInst.cmd)
                    {
                    case IrCmd::BUFFER_READI8:
                        if (info.loadCmd == IrCmd::BUFFER_READI8)
                        {
                            if (info.value.kind == IrOpKind::Inst)
                            {
                                replace(function, block, instIdx, IrInst{IrCmd::SEXTI8_INT, {info.value}});
                                substituteOrRecord(loadInst, instIdx);
                            }
                            else
                            {
                                substitute(function, loadInst, info.value);
                            }
                            return;
                        }
                        break;
                    case IrCmd::BUFFER_READU8:
                        if (info.loadCmd == IrCmd::BUFFER_READI8)
                        {
                            if (info.value.kind == IrOpKind::Inst)
                            {
                                replace(function, block, instIdx, IrInst{IrCmd::BITAND_UINT, {info.value, build.constInt(0xff)}});
                                substituteOrRecord(loadInst, instIdx);
                            }
                            else
                            {
                                substitute(function, loadInst, build.constInt(uint8_t(function.intOp(info.value))));
                            }

                            return;
                        }
                        break;
                    case IrCmd::BUFFER_READI16:
                        if (info.loadCmd == IrCmd::BUFFER_READI16)
                        {
                            if (info.value.kind == IrOpKind::Inst)
                            {
                                replace(function, block, instIdx, IrInst{IrCmd::SEXTI16_INT, {info.value}});
                                substituteOrRecord(loadInst, instIdx);
                            }
                            else
                            {
                                substitute(function, loadInst, info.value);
                            }
                            return;
                        }
                        break;
                    case IrCmd::BUFFER_READU16:
                        if (info.loadCmd == IrCmd::BUFFER_READI16)
                        {
                            if (info.value.kind == IrOpKind::Inst)
                            {
                                replace(function, block, instIdx, IrInst{IrCmd::BITAND_UINT, {info.value, build.constInt(0xffff)}});
                                substituteOrRecord(loadInst, instIdx);
                            }
                            else
                            {
                                substitute(function, loadInst, build.constInt(uint16_t(function.intOp(info.value))));
                            }

                            return;
                        }
                        break;
                    case IrCmd::BUFFER_READI32:
                        if (info.loadCmd == IrCmd::BUFFER_READI32)
                        {
                            if (IrInst* src = function.asInstOp(info.value); src && producesDirtyHighRegisterBits(src->cmd))
                            {
                                replace(function, block, instIdx, IrInst{IrCmd::TRUNCATE_UINT, {info.value}});
                                substituteOrRecord(loadInst, instIdx);
                            }
                            else
                            {
                                substitute(function, loadInst, info.value);
                            }
                            return;
                        }
                        break;
                    case IrCmd::BUFFER_READF32:
                        if (info.loadCmd == IrCmd::BUFFER_READF32)
                        {
                            substitute(function, loadInst, info.value);
                            return;
                        }
                        break;
                    case IrCmd::BUFFER_READF64:
                        if (info.loadCmd == IrCmd::BUFFER_READF64)
                        {
                            substitute(function, loadInst, info.value);
                            return;
                        }
                        break;
                    default:
                        CODEGEN_ASSERT(!"unknown load instruction");
                    }
                }
                else if (info.loadCmd == loadInst.cmd) // Values from loads match exactly
                {
                    substitute(function, loadInst, info.value);
                    return;
                }
            }
        }

        // Record this load for future reuse
        BufferLoadStoreInfo info;

        info.loadCmd = loadInst.cmd;
        info.accessSize = accessSize;
        info.tag = tag;
        info.fromStore = false;

        info.address = OP_A(loadInst);
        info.value = IrOp{IrOpKind::Inst, function.getInstIndex(loadInst)};

        info.offset = offset;

        bufferLoadStoreInfo.push_back(info);
    }

    void forwardBufferStoreToLoad(IrInst& storeInst, IrCmd loadCmd, uint8_t accessSize)
    {
        uint8_t tag = !FFlag::LuauCodegenBufNoDefTag && !HAS_OP_D(storeInst) ? LUA_TBUFFER : function.tagOp(OP_D(storeInst));

        // Writing at unknown offset removes everything in the same kind of memory (buffer/userdata)
        // For userdata, we could check where the pointer is coming from, but we don't have an example of such usage
        if (OP_B(storeInst).kind != IrOpKind::Constant)
        {
            for (size_t i = 0; i < bufferLoadStoreInfo.size();)
            {
                BufferLoadStoreInfo& info = bufferLoadStoreInfo[i];

                if (info.tag == tag)
                {
                    bufferLoadStoreInfo[i] = bufferLoadStoreInfo.back();
                    bufferLoadStoreInfo.pop_back();
                }
                else
                {
                    i++;
                }
            }

            return;
        }

        int offset = function.intOp(OP_B(storeInst));

        // Write at a constant offset invalidates that range in every object unless we know the pointers are unrelated
        for (size_t i = 0; i < bufferLoadStoreInfo.size();)
        {
            BufferLoadStoreInfo& info = bufferLoadStoreInfo[i];

            bool intersectingRange = offset + accessSize - 1 >= info.offset && offset <= info.offset + info.accessSize - 1;

            if (intersectingRange && info.tag == tag)
            {
                const IrInst& currPtr = function.instOp(OP_A(storeInst));
                const IrInst& infoPtr = function.instOp(info.address);

                // Pointers from separate allocations cannot be the same
                if (currPtr.cmd == IrCmd::NEW_USERDATA && infoPtr.cmd == IrCmd::NEW_USERDATA &&
                    (!FFlag::LuauCodegenUserdataAddressAlias || OP_A(storeInst) != info.address))
                {
                    i++;
                    continue;
                }

                bufferLoadStoreInfo[i] = bufferLoadStoreInfo.back();
                bufferLoadStoreInfo.pop_back();
            }
            else
            {
                i++;
            }
        }

        IrOp value = OP_C(storeInst);

        // Store of smaller type will truncate data
        // Dynamic values are handled in 'substituteOrRecordBufferLoad'
        if (OP_C(storeInst).kind == IrOpKind::Constant)
        {
            if (loadCmd == IrCmd::BUFFER_READI8)
                value = build.constInt(int8_t(function.intOp(OP_C(storeInst))));
            else if (loadCmd == IrCmd::BUFFER_READI16)
                value = build.constInt(int16_t(function.intOp(OP_C(storeInst))));
            else if (loadCmd == IrCmd::BUFFER_READF32)
                value = build.constDouble(float(function.doubleOp(OP_C(storeInst))));
        }

        // Record this store value for future reuse
        BufferLoadStoreInfo info;

        info.loadCmd = loadCmd;
        info.accessSize = accessSize;
        info.tag = tag;
        info.fromStore = true;

        info.address = OP_A(storeInst);
        info.value = value;

        info.offset = offset;

        bufferLoadStoreInfo.push_back(info);
    }

    // Loads from array can either have a dynamic index, constant index in GET_ARR_ADDR or constant offset in the load/store
    IrOp getCombinedArrayLoadOffsetOp(IrInst& arrayAddrInst, IrOp loadOffsetOp)
    {
        CODEGEN_ASSERT(arrayAddrInst.cmd == IrCmd::GET_ARR_ADDR);

        int loadOffset = function.asIntOp(loadOffsetOp).value_or(0);

        if (OP_B(arrayAddrInst).kind == IrOpKind::Constant)
        {
            int arrayAddrOffset = function.intOp(OP_B(arrayAddrInst)) * sizeof(TValue);

            if (arrayAddrOffset != 0 || loadOffset == 0)
            {
                // Only one of the offsets can be a constant non-zero
                CODEGEN_ASSERT(loadOffset == 0);
                return build.constInt(arrayAddrOffset);
            }
        }
        else
        {
            CODEGEN_ASSERT(OP_B(arrayAddrInst).kind == IrOpKind::Inst);

            // Cannot apply load offset to a dynamic array address
            CODEGEN_ASSERT(loadOffset == 0);
            return OP_B(arrayAddrInst);
        }

        CODEGEN_ASSERT(loadOffsetOp.kind == IrOpKind::Constant);
        return loadOffsetOp;
    }

    void invalidateTableStoreLocation(IrInst targetAddr, IrOp writeOffsetOp, uint8_t tag)
    {
        if (targetAddr.cmd == IrCmd::GET_SLOT_NODE_ADDR)
        {
            CODEGEN_ASSERT(function.intOp(writeOffsetOp) == 0);

            // hashValueCache contains data about loads that have successfully completed CHECK_SLOT_MATCH check
            // This means that a value for a key 'A' has been found at the expected location in the table
            // This means that writing to a key 'B' which also did a CHECK_SLOT_MATCH check cannot affect 'A' in any table
            // Writes through other means like SET_TABLE are marked as 'user call' and invalidate all table heap knowledge
            for (auto& [pointerIdx, loadedValueIdx] : hashValueCache)
            {
                IrInst& address = function.instructions[pointerIdx];

                if (OP_C(address) == OP_C(targetAddr))
                    loadedValueIdx = kInvalidInstIdx;
            }

            // If we don't know what tag was written or it is nil, on the next access we will need to recheck that it's not nil
            if (tag == kUnknownTag || tag == LUA_TNIL)
            {
                for (auto& el : checkSlotMatchCache)
                {
                    IrInst& check = function.instructions[el.pointer];
                    IrInst& slotAddr = function.instOp(OP_A(check));

                    if (OP_C(slotAddr) == OP_C(targetAddr))
                        el.knownToNotBeNil = false;
                }
            }
        }
        else if (targetAddr.cmd == IrCmd::GET_ARR_ADDR)
        {
            IrOp offsetOp = getCombinedArrayLoadOffsetOp(targetAddr, writeOffsetOp);

            std::optional<int> optOffset = function.asIntOp(offsetOp);

            if (!optOffset)
            {
                // This store is at unknown position, clear all data
                arrayValueCache.clear();
            }
            else
            {
                for (size_t i = 0; i < arrayValueCache.size();)
                {
                    auto& entry = arrayValueCache[i];

                    // Clear cached results at unknown positions and matching known position
                    if (entry.offset.kind != IrOpKind::Constant || function.intOp(entry.offset) == *optOffset)
                    {
                        entry = arrayValueCache.back();
                        arrayValueCache.pop_back();
                    }
                    else
                    {
                        i++;
                    }
                }
            }
        }
        else if (targetAddr.cmd == IrCmd::TABLE_SETNUM)
        {
            // Double-check that TABLE_SETNUM invalidated the table array data
            CODEGEN_ASSERT(arrayValueCache.empty());
        }
        else
        {
            CODEGEN_ASSERT(targetAddr.cmd == IrCmd::GET_CLOSURE_UPVAL_ADDR);
        }
    }

    // Record that a future load from this GET_SLOT_NODE_ADDR/GET_ARR_ADDR can find values in this store instruction
    void forwardTableStoreToLoad(IrInst& targetAddr, IrOp writeOffsetOp, uint32_t instIdx)
    {
        if (targetAddr.cmd == IrCmd::GET_SLOT_NODE_ADDR)
        {
            CODEGEN_ASSERT(function.intOp(writeOffsetOp) == 0);

            hashValueCache[function.getInstIndex(targetAddr)] = instIdx;
        }
        else if (targetAddr.cmd == IrCmd::GET_ARR_ADDR)
        {
            IrOp offsetOp = getCombinedArrayLoadOffsetOp(targetAddr, writeOffsetOp);

            arrayValueCache.push_back({function.getInstIndex(targetAddr), offsetOp, instIdx});
        }
        else
        {
            CODEGEN_ASSERT(targetAddr.cmd == IrCmd::TABLE_SETNUM || targetAddr.cmd == IrCmd::GET_CLOSURE_UPVAL_ADDR);
        }
    }

    // Used to compute the pressure of the cached value 'set' on the spill registers
    // We want to find out the maximum live range intersection count between the cached value at 'slot' and current instruction
    // Note that this pressure is approximate, as some values that might have been live at one point could have been marked dead later
    int getMaxInternalOverlap(std::vector<NumberedInstruction>& set, size_t slot)
    {
        // Start with one live range for the slot we want to reuse
        int curr = 1;

        // For any slots where lifetime began before the slot of interest, mark as live if lifetime end is still active
        // This saves us from processing slots [0; slot] in the range sweep later, which requires sorting the lifetime end points
        for (size_t i = 0; i < slot; i++)
        {
            if (set[i].finishPos >= set[slot].startPos)
                curr++;
        }

        int max = curr;

        // Collect lifetime end points and sort them
        rangeEndTemp.clear();

        for (size_t i = slot + 1; i < set.size(); i++)
            rangeEndTemp.push_back(set[i].finishPos);

        std::sort(rangeEndTemp.begin(), rangeEndTemp.end());

        // Go over the lifetime begin/end ranges that we store as separate array and walk based on the smallest of values
        for (size_t i1 = slot + 1, i2 = 0; i1 < set.size() && i2 < rangeEndTemp.size();)
        {
            if (rangeEndTemp[i2] == set[i1].startPos)
            {
                i1++;
                i2++;
            }
            else if (rangeEndTemp[i2] < set[i1].startPos)
            {
                CODEGEN_ASSERT(curr > 0);

                curr--;
                i2++;
            }
            else
            {
                curr++;
                i1++;

                if (curr > max)
                    max = curr;
            }
        }

        // We might have unprocessed lifetime end entries, but we will never have unprocessed lifetime start entries
        // Not that lifetime end entries can only decrease the current value and do not affect the end result (maximum)
        return max;
    }

    void clear()
    {
        for (int i = 0; i <= maxReg; ++i)
            regs[i] = RegisterInfo();

        maxReg = 0;
        instPos = 0u;

        inSafeEnv = false;
        checkedGc = false;

        instLink.clear();

        instTag.clear();
        instValue.clear();

        invalidateValuePropagation();
        invalidateHeapTableData();
        invalidateHeapBufferData();
        invalidateUserdataData();
    }

    IrBuilder& build;
    IrFunction& function;

    std::array<RegisterInfo, 256> regs;

    // For range/full invalidations, we only want to visit a limited number of data that we have recorded
    int maxReg = 0;

    // Number of the instruction being processed
    uint32_t instPos = 0;

    bool inSafeEnv = false;
    bool checkedGc = false;

    // Stores which register does the instruction value correspond to (and at which version of the register)
    DenseHashMap<uint32_t, RegisterLink> instLink{kInvalidInstIdx};

    // Stored the tag of a TValue stored in an instruction and will never change
    DenseHashMap<uint32_t, uint8_t> instTag{kInvalidInstIdx};
    DenseHashMap<uint32_t, IrOp> instValue{kInvalidInstIdx};

    DenseHashMap<IrInst, uint32_t, IrInstHash, IrInstEq> valueMap;

    // For upvalue load-store optimizations, we just keep track of the last known value of the upvalue
    DenseHashMap<uint8_t, uint32_t> upvalueMap{kUpvalueEmptyKey};

    // For load-store optimizations of table elements, separate maps for hash and array parts as writes to one do not affect the other
    DenseHashMap<uint32_t, uint32_t> hashValueCache{kInvalidInstIdx};
    std::vector<ArrayValueEntry> arrayValueCache;

    // Some instruction re-uses can't be stored in valueMap because of extra requirements
    std::vector<uint32_t> tryNumToIndexCache; // Fallback block argument might be different

    // Heap changes might affect table state
    std::vector<NumberedInstruction> getSlotNodeCache; // Additionally, pcpos argument might be different
    std::vector<NodeSlotState> checkSlotMatchCache;    // Additionally, fallback block argument might be different

    std::vector<uint32_t> getArrAddrCache;
    std::vector<uint32_t> checkArraySizeCache; // Additionally, fallback block argument might be different

    std::vector<uint32_t> checkBufferLenCache; // Additionally, fallback block argument might be different

    // Userdata tag cache can point to both NEW_USERDATA and CHECK_USERDATA_TAG instructions
    std::vector<uint32_t> useradataTagCache; // Additionally, fallback block argument might be different

    std::vector<BufferLoadStoreInfo> bufferLoadStoreInfo;

    std::vector<uint32_t> rangeEndTemp;
};

static void handleBuiltinEffects(ConstPropState& state, LuauBuiltinFunction bfid, uint32_t firstReturnReg, int nresults)
{
    // Switch over all values is used to force new items to be handled
    switch (bfid)
    {
    case LBF_NONE:
    case LBF_ASSERT:
    case LBF_MATH_ABS:
    case LBF_MATH_ACOS:
    case LBF_MATH_ASIN:
    case LBF_MATH_ATAN2:
    case LBF_MATH_ATAN:
    case LBF_MATH_CEIL:
    case LBF_MATH_COSH:
    case LBF_MATH_COS:
    case LBF_MATH_DEG:
    case LBF_MATH_EXP:
    case LBF_MATH_FLOOR:
    case LBF_MATH_FMOD:
    case LBF_MATH_FREXP:
    case LBF_MATH_LDEXP:
    case LBF_MATH_LOG10:
    case LBF_MATH_LOG:
    case LBF_MATH_MAX:
    case LBF_MATH_MIN:
    case LBF_MATH_MODF:
    case LBF_MATH_POW:
    case LBF_MATH_RAD:
    case LBF_MATH_SINH:
    case LBF_MATH_SIN:
    case LBF_MATH_SQRT:
    case LBF_MATH_TANH:
    case LBF_MATH_TAN:
    case LBF_BIT32_ARSHIFT:
    case LBF_BIT32_BAND:
    case LBF_BIT32_BNOT:
    case LBF_BIT32_BOR:
    case LBF_BIT32_BXOR:
    case LBF_BIT32_BTEST:
    case LBF_BIT32_EXTRACT:
    case LBF_BIT32_LROTATE:
    case LBF_BIT32_LSHIFT:
    case LBF_BIT32_REPLACE:
    case LBF_BIT32_RROTATE:
    case LBF_BIT32_RSHIFT:
    case LBF_TYPE:
    case LBF_STRING_BYTE:
    case LBF_STRING_CHAR:
    case LBF_STRING_LEN:
    case LBF_TYPEOF:
    case LBF_STRING_SUB:
    case LBF_MATH_CLAMP:
    case LBF_MATH_SIGN:
    case LBF_MATH_ROUND:
    case LBF_RAWGET:
    case LBF_RAWEQUAL:
    case LBF_TABLE_UNPACK:
    case LBF_VECTOR:
    case LBF_BIT32_COUNTLZ:
    case LBF_BIT32_COUNTRZ:
    case LBF_SELECT_VARARG:
    case LBF_RAWLEN:
    case LBF_BIT32_EXTRACTK:
    case LBF_GETMETATABLE:
    case LBF_TONUMBER:
    case LBF_TOSTRING:
    case LBF_BIT32_BYTESWAP:
    case LBF_BUFFER_READI8:
    case LBF_BUFFER_READU8:
    case LBF_BUFFER_READI16:
    case LBF_BUFFER_READU16:
    case LBF_BUFFER_READI32:
    case LBF_BUFFER_READU32:
    case LBF_BUFFER_READF32:
    case LBF_BUFFER_READF64:
    case LBF_BUFFER_READINTEGER:
    case LBF_VECTOR_MAGNITUDE:
    case LBF_VECTOR_NORMALIZE:
    case LBF_VECTOR_CROSS:
    case LBF_VECTOR_DOT:
    case LBF_VECTOR_FLOOR:
    case LBF_VECTOR_CEIL:
    case LBF_VECTOR_ABS:
    case LBF_VECTOR_SIGN:
    case LBF_VECTOR_CLAMP:
    case LBF_VECTOR_MIN:
    case LBF_VECTOR_MAX:
    case LBF_VECTOR_LERP:
    case LBF_MATH_LERP:
    case LBF_MATH_ISNAN:
    case LBF_MATH_ISINF:
    case LBF_MATH_ISFINITE:
    case LBF_INTEGER_ADD:
    case LBF_INTEGER_MUL:
    case LBF_INTEGER_IDIV:
    case LBF_INTEGER_LT:
    case LBF_INTEGER_CREATE:
    case LBF_INTEGER_MOD:
    case LBF_INTEGER_SUB:
    case LBF_INTEGER_LE:
    case LBF_INTEGER_GT:
    case LBF_INTEGER_GE:
    case LBF_INTEGER_ULT:
    case LBF_INTEGER_ULE:
    case LBF_INTEGER_UGT:
    case LBF_INTEGER_UGE:
    case LBF_INTEGER_DIV:
    case LBF_INTEGER_NEG:
    case LBF_INTEGER_BSWAP:
    case LBF_INTEGER_MIN:
    case LBF_INTEGER_MAX:
    case LBF_INTEGER_REM:
    case LBF_INTEGER_UDIV:
    case LBF_INTEGER_UREM:
    case LBF_INTEGER_BAND:
    case LBF_INTEGER_BOR:
    case LBF_INTEGER_BNOT:
    case LBF_INTEGER_BXOR:
    case LBF_INTEGER_BTEST:
    case LBF_INTEGER_COUNTRZ:
    case LBF_INTEGER_COUNTLZ:
    case LBF_INTEGER_LSHIFT:
    case LBF_INTEGER_RSHIFT:
    case LBF_INTEGER_ARSHIFT:
    case LBF_INTEGER_LROTATE:
    case LBF_INTEGER_RROTATE:
    case LBF_INTEGER_CLAMP:
    case LBF_INTEGER_EXTRACT:
    case LBF_INTEGER_TONUMBER:
        break;
    case LBF_BUFFER_WRITEU8:
    case LBF_BUFFER_WRITEU16:
    case LBF_BUFFER_WRITEU32:
    case LBF_BUFFER_WRITEF32:
    case LBF_BUFFER_WRITEF64:
    case LBF_BUFFER_WRITEINTEGER:
        if (FFlag::LuauCodegenBufferWriteEffects)
            state.invalidateHeapBufferData();
        break;
    case LBF_TABLE_INSERT:
        state.invalidateHeap();
        return; // table.insert does not modify result registers.
    case LBF_RAWSET:
        state.invalidateHeap();
        break;
    case LBF_SETMETATABLE:
        state.invalidateHeap(); // TODO: only knownNoMetatable is affected and we might know which one
        break;
    }

    // TODO: classify further using switch above, some fastcalls only modify the value, not the tag
    // TODO: fastcalls are different from calls and it might be possible to not invalidate all register starting from return
    state.invalidateRegistersFrom(firstReturnReg);
}

static void constPropInInst(ConstPropState& state, IrBuilder& build, IrFunction& function, IrBlock& block, IrInst& inst, uint32_t index)
{
    state.instPos++;

    switch (inst.cmd)
    {
    case IrCmd::LOAD_TAG:
        if (uint8_t tag = state.tryGetTag(OP_A(inst)); tag != 0xff)
        {
            substitute(function, inst, build.constTag(tag));
        }
        else if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (state.substituteTagLoadWithTValueData(build, inst))
                break;

            state.substituteOrRecordVmRegLoad(inst);
        }
        break;
    case IrCmd::LOAD_POINTER:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (state.substituteOrRecordValueLoadWithTValueData(build, inst))
                break;

            state.substituteOrRecordVmRegLoad(inst);
        }
        break;
    case IrCmd::LOAD_DOUBLE:
    {
        IrOp value = state.tryGetValue(OP_A(inst));

        if (function.asDoubleOp(value))
        {
            substitute(function, inst, value);
        }
        else if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (state.substituteOrRecordValueLoadWithTValueData(build, inst))
                break;

            state.substituteOrRecordVmRegLoad(inst);
        }
        break;
    }
    case IrCmd::LOAD_INT:
    {
        IrOp value = state.tryGetValue(OP_A(inst));

        if (function.asIntOp(value))
        {
            substitute(function, inst, value);
        }
        else if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (state.substituteOrRecordValueLoadWithTValueData(build, inst))
                break;

            state.substituteOrRecordVmRegLoad(inst);
        }
        break;
    }
    case IrCmd::LOAD_INT64:
    {
        IrOp value = state.tryGetValue(OP_A(inst));

        if (function.asInt64Op(value))
        {
            substitute(function, inst, value);
        }
        else if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (state.substituteOrRecordValueLoadWithTValueData(build, inst))
                break;

            state.substituteOrRecordVmRegLoad(inst);
        }
        break;
    }
    case IrCmd::LOAD_FLOAT:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (std::optional<IrOp> subst = state.findSubstituteComponentLoadFromStoreVector(build, OP_A(inst), function.intOp(OP_B(inst))))
            {
                substitute(function, inst, *subst);
                break;
            }

            // There could be a whole TValue that we can extract a field from
            if (uint32_t* prevIdxPtr = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_TVALUE, OP_A(inst)))
            {
                uint32_t prevIdx = *prevIdxPtr;
                IrInst& prev = function.instructions[prevIdx];

                // Unpack the STORE_TVALUE of a TAG_VECTOR value
                if (prev.cmd == IrCmd::TAG_VECTOR)
                {
                    if (function.asInstOp(OP_A(prev)))
                        prevIdx = OP_A(prev).index;
                }

                IrInst& value = function.instructions[prevIdx];

                unsigned byteOffset = unsigned(function.intOp(OP_B(inst)));
                CODEGEN_ASSERT(byteOffset % 4 == 0);

                unsigned component = byteOffset / 4;
                CODEGEN_ASSERT(component <= 3);

                // If we are extracting from a constant, we can substitute with it
                if (value.cmd == IrCmd::LOAD_TVALUE && OP_A(value).kind == IrOpKind::VmConst && function.proto)
                {
                    TValue* tv = &function.proto->k[vmConstOp(OP_A(value))];

                    if (ttisvector(tv))
                    {
                        const float* v = vvalue(tv);
                        substitute(function, inst, build.constDouble(v[component]));
                        break;
                    }
                }
                else if (value.cmd == IrCmd::LOAD_TVALUE && OP_A(value).kind == IrOpKind::VmReg)
                {
                    // We were able to match "LOAD_FLOAT Rx" to a previous "STORE_TVALUE Rx, %n" where "%n = LOAD_TVALUE Ry"
                    // But we still have to check that %n represents the current version of Ry
                    if (state.tryGetRegLink(IrOp{IrOpKind::Inst, prevIdx}) != nullptr)
                    {
                        if (std::optional<IrOp> subst =
                                state.findSubstituteComponentLoadFromStoreVector(build, OP_A(value), function.intOp(OP_B(inst))))
                        {
                            substitute(function, inst, *subst);
                            break;
                        }
                    }
                }

                replace(function, block, index, IrInst{IrCmd::EXTRACT_VEC, {IrOp{IrOpKind::Inst, prevIdx}, build.constInt(component)}});

                state.substituteOrRecord(inst, index);
                break;
            }

            state.substituteOrRecordVmRegLoad(inst);
        }
        break;
    case IrCmd::LOAD_TVALUE:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (!state.substituteOrRecordVmRegLoad(inst) && !HAS_OP_C(inst))
            {
                // Provide information about what kind of tag is being loaded, this helps dead store elimination later
                if (uint8_t tag = state.tryGetTag(OP_A(inst)); tag != 0xff)
                    replace(function, block, index, IrInst{IrCmd::LOAD_TVALUE, {OP_A(inst), build.constInt(0), build.constTag(tag)}});
            }
        }
        else if (IrInst* source = function.asInstOp(OP_A(inst)))
        {
            if (source->cmd == IrCmd::GET_SLOT_NODE_ADDR)
            {
                uint32_t* prevIdx = state.hashValueCache.find(OP_A(inst).index);

                if (prevIdx && *prevIdx != kInvalidInstIdx)
                {
                    IrInst& prev = function.instructions[*prevIdx];

                    if (prev.cmd == IrCmd::LOAD_TVALUE)
                    {
                        if (prev.useCount != 0)
                            substitute(function, inst, IrOp{IrOpKind::Inst, *prevIdx});
                    }
                    else if (prev.cmd == IrCmd::STORE_SPLIT_TVALUE)
                    {
                        state.instTag[index] = function.tagOp(OP_B(prev));
                        state.instValue[index] = OP_C(prev);
                    }

                    break;
                }

                state.hashValueCache[OP_A(inst).index] = index;
            }
            else if (source->cmd == IrCmd::GET_ARR_ADDR)
            {
                IrOp offsetOp = state.getCombinedArrayLoadOffsetOp(*source, OPT_OP_B(inst));

                auto it = std::find_if(
                    state.arrayValueCache.begin(),
                    state.arrayValueCache.end(),
                    [&](const ArrayValueEntry& el)
                    {
                        return el.pointer == OP_A(inst).index && el.offset == offsetOp;
                    }
                );

                if (it != state.arrayValueCache.end() && it->value != kInvalidInstIdx)
                {
                    IrInst& prev = function.instructions[it->value];

                    if (prev.cmd == IrCmd::LOAD_TVALUE)
                    {
                        if (prev.useCount != 0)
                            substitute(function, inst, IrOp{IrOpKind::Inst, it->value});
                    }
                    else if (prev.cmd == IrCmd::STORE_SPLIT_TVALUE)
                    {
                        state.instTag[index] = function.tagOp(OP_B(prev));
                        state.instValue[index] = OP_C(prev);
                    }

                    break;
                }

                state.arrayValueCache.push_back({OP_A(inst).index, offsetOp, index});
            }
        }
        break;
    case IrCmd::STORE_TAG:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            const IrOp source = OP_A(inst);

            IrCmd activeLoadCmd = IrCmd::NOP;
            uint32_t activeLoadValue = kInvalidInstIdx;

            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                uint8_t value = function.tagOp(OP_B(inst));

                // STORE_TAG usually follows a store of the value, but it also bumps the version of the whole register
                // To be able to propagate STORE_*** into LOAD_***, we find active LOAD_*** value and recreate it with updated version
                // Register in this optimization cannot be captured to avoid complications in lowering (IrValueLocationTracking doesn't model it)
                std::tie(activeLoadCmd, activeLoadValue) = state.getPreviousVersionedLoadForTag(value, source);

                if (state.tryGetTag(source) == value)
                {
                    kill(function, inst);
                }
                else
                {
                    state.saveTag(source, value);

                    // Storing 'nil' implicitly kills the known value in the register
                    // This is required for dead store elimination to correctly establish tag+value pairs as it treats 'nil' write as a full TValue
                    // store
                    if (value == LUA_TNIL)
                        state.invalidateValue(source);
                }
            }
            else
            {
                state.invalidateTag(source);
            }

            // Future LOAD_*** instructions can re-use previous register version load
            if (activeLoadValue != kInvalidInstIdx)
                state.valueMap[state.versionedVmRegLoad(activeLoadCmd, source)] = activeLoadValue;
        }
        break;
    case IrCmd::STORE_EXTRA:
        break;
    case IrCmd::STORE_POINTER:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (FFlag::LuauCodegenRemoveDuplicateDoubleIntValues && OP_B(inst).kind == IrOpKind::Inst)
            {
                if (uint32_t* prevIdx = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_POINTER, OP_A(inst)))
                {
                    if (*prevIdx == OP_B(inst).index)
                    {
                        kill(function, inst);
                        break;
                    }
                }
            }

            state.invalidateValue(OP_A(inst));

            if (OP_B(inst).kind == IrOpKind::Inst)
            {
                state.forwardVmRegStoreToLoad(inst, IrCmd::LOAD_POINTER);

                if (IrInst* instOp = function.asInstOp(OP_B(inst)); instOp && instOp->cmd == IrCmd::NEW_TABLE)
                {
                    if (RegisterInfo* info = state.tryGetRegisterInfo(OP_A(inst)))
                    {
                        info->knownNotReadonly = true;
                        info->knownNoMetatable = true;
                        info->knownTableArraySize = function.uintOp(OP_A(instOp));
                    }
                }
            }
        }
        break;
    case IrCmd::STORE_DOUBLE:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                if (state.tryGetValue(OP_A(inst)) == OP_B(inst))
                    kill(function, inst);
                else
                    state.saveValue(OP_A(inst), OP_B(inst));
            }
            else
            {
                if (FFlag::LuauCodegenRemoveDuplicateDoubleIntValues)
                {
                    if (uint32_t* prevIdx = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_DOUBLE, OP_A(inst)))
                    {
                        if (*prevIdx == OP_B(inst).index)
                        {
                            kill(function, inst);
                            break;
                        }
                    }
                }

                state.invalidateValue(OP_A(inst));
                state.forwardVmRegStoreToLoad(inst, IrCmd::LOAD_DOUBLE);
            }
        }
        break;
    case IrCmd::STORE_INT:
        // Integer store doesn't access high register bits
        if (IrInst* src = function.asInstOp(OP_B(inst)); src && src->cmd == IrCmd::TRUNCATE_UINT)
            replace(function, OP_B(inst), OP_A(src));

        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                if (state.tryGetValue(OP_A(inst)) == OP_B(inst))
                    kill(function, inst);
                else
                    state.saveValue(OP_A(inst), OP_B(inst));
            }
            else
            {
                if (FFlag::LuauCodegenRemoveDuplicateDoubleIntValues)
                {
                    if (uint32_t* prevIdx = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_INT, OP_A(inst)))
                    {
                        if (*prevIdx == OP_B(inst).index)
                        {
                            kill(function, inst);
                            break;
                        }
                    }
                }

                state.invalidateValue(OP_A(inst));
                state.forwardVmRegStoreToLoad(inst, IrCmd::LOAD_INT);
            }
        }
        break;
    case IrCmd::STORE_INT64:
        // Unlike STORE_INT (32-bit), do NOT remove TRUNCATE_UINT here:
        // a 64-bit store uses all bits, so truncation is meaningful.

        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            if (OP_B(inst).kind == IrOpKind::Constant)
            {
                if (state.tryGetValue(OP_A(inst)) == OP_B(inst))
                    kill(function, inst);
                else
                    state.saveValue(OP_A(inst), OP_B(inst));
            }
            else
            {
                if (FFlag::LuauCodegenRemoveDuplicateDoubleIntValues)
                {
                    if (uint32_t* prevIdx = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_INT64, OP_A(inst)))
                    {
                        if (*prevIdx == OP_B(inst).index)
                        {
                            kill(function, inst);
                            break;
                        }
                    }
                }

                state.invalidateValue(OP_A(inst));
                state.forwardVmRegStoreToLoad(inst, IrCmd::LOAD_INT64);
            }
        }
        break;
    case IrCmd::STORE_VECTOR:
        state.invalidateValue(OP_A(inst));

        // To avoid captured register invalidation tracking in lowering later, values from loads from captured registers are not propagated
        if (!function.cfg.captured.regs.test(vmRegOp(OP_A(inst))))
        {
            // This is different from how other stores use 'forwardVmRegStoreToLoad'
            // Instead of mapping a store to a load directly, we map a LOAD_FLOAT without a specific offset to the the store instruction itself
            // LOAD_FLOAT will have special path to look up this store and apply additional checks to make sure the argument reuse is valid
            // One of the restrictions is that STORE_VECTOR converts double to float, so reusing the source is only possible if it comes from a float
            // The register versioning rules will stay the same and follow the correct invalidation
            state.valueMap[state.versionedVmRegLoad(IrCmd::LOAD_FLOAT, OP_A(inst))] = index;
        }
        break;
    case IrCmd::STORE_TVALUE:
        if (OP_A(inst).kind == IrOpKind::VmReg || OP_A(inst).kind == IrOpKind::Inst)
        {
            if (OP_A(inst).kind == IrOpKind::VmReg)
            {
                if (OP_B(inst).kind == IrOpKind::Inst)
                {
                    if (uint32_t* prevIdx = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_TVALUE, OP_A(inst)))
                    {
                        if (*prevIdx == OP_B(inst).index)
                        {
                            kill(function, inst);
                            break;
                        }
                    }
                }

                state.invalidate(OP_A(inst));
            }

            uint8_t tag = state.tryGetTag(OP_B(inst));

            // We know the tag of some instructions that result in TValue
            if (tag == 0xff)
                tag = tryGetOperandTag(function, OP_B(inst)).value_or(kUnknownTag);

            IrOp value = state.tryGetValue(OP_B(inst));

            if (OP_A(inst).kind == IrOpKind::VmReg)
            {
                if (tag != 0xff)
                    state.saveTag(OP_A(inst), tag);

                if (value.kind != IrOpKind::None)
                    state.saveValue(OP_A(inst), value);
            }

            IrCmd activeLoadCmd = IrCmd::NOP;
            uint32_t activeLoadValue = kInvalidInstIdx;

            // If we know the tag, we can try extracting the value from a register used by LOAD_TVALUE
            // To do that, we have to ensure that the register link of the source value is still valid
            if (tag != 0xff && value.kind == IrOpKind::None && state.tryGetRegLink(OP_B(inst)) != nullptr)
            {
                if (IrInst* arg = function.asInstOp(OP_B(inst)); arg && arg->cmd == IrCmd::LOAD_TVALUE && OP_A(arg).kind == IrOpKind::VmReg)
                {
                    std::tie(activeLoadCmd, activeLoadValue) = state.getPreviousVersionedLoadForTag(tag, OP_A(arg));

                    if (activeLoadValue != kInvalidInstIdx)
                        value = IrOp{IrOpKind::Inst, activeLoadValue};
                }
            }

            if (IrInst* target = function.asInstOp(OP_A(inst)))
                state.invalidateTableStoreLocation(*target, OPT_OP_C(inst), tag);

            // If we have constant tag and value, replace TValue store with tag/value pair store
            bool canSplitTvalueStore = false;

            if (tag == LUA_TBOOLEAN &&
                (value.kind == IrOpKind::Inst || (value.kind == IrOpKind::Constant && function.constOp(value).kind == IrConstKind::Int)))
                canSplitTvalueStore = true;
            else if (tag == LUA_TNUMBER &&
                     (value.kind == IrOpKind::Inst || (value.kind == IrOpKind::Constant && function.constOp(value).kind == IrConstKind::Double)))
                canSplitTvalueStore = true;
            else if (tag == LUA_TINTEGER &&
                     (value.kind == IrOpKind::Inst || (value.kind == IrOpKind::Constant && function.constOp(value).kind == IrConstKind::Int64)))
                canSplitTvalueStore = true;
            else if (tag != 0xff && isGCO(tag) && value.kind == IrOpKind::Inst)
                canSplitTvalueStore = true;

            if (canSplitTvalueStore)
            {
                if (HAS_OP_C(inst))
                    replace(function, block, index, {IrCmd::STORE_SPLIT_TVALUE, {OP_A(inst), build.constTag(tag), value, OP_C(inst)}});
                else
                    replace(function, block, index, {IrCmd::STORE_SPLIT_TVALUE, {OP_A(inst), build.constTag(tag), value}});

                // Value can be propagated to future loads of the same register
                if (OP_A(inst).kind == IrOpKind::VmReg && activeLoadValue != kInvalidInstIdx)
                    state.valueMap[state.versionedVmRegLoad(activeLoadCmd, OP_A(inst))] = activeLoadValue;

                if (IrInst* target = function.asInstOp(OP_A(inst)))
                    state.forwardTableStoreToLoad(*target, OPT_OP_D(inst), index);
            }
            else if (OP_A(inst).kind == IrOpKind::VmReg)
            {
                state.forwardVmRegStoreToLoad(inst, IrCmd::LOAD_TVALUE);
            }
        }
        break;
    case IrCmd::STORE_SPLIT_TVALUE:
        if (OP_A(inst).kind == IrOpKind::VmReg)
        {
            state.invalidate(OP_A(inst));

            state.saveTag(OP_A(inst), function.tagOp(OP_B(inst)));

            if (OP_C(inst).kind == IrOpKind::Constant)
                state.saveValue(OP_A(inst), OP_C(inst));
        }
        else
        {
            if (IrInst* target = function.asInstOp(OP_A(inst)))
            {
                state.invalidateTableStoreLocation(*target, OPT_OP_D(inst), function.tagOp(OP_B(inst)));

                state.forwardTableStoreToLoad(*target, OPT_OP_D(inst), index);
            }
        }
        break;
    case IrCmd::JUMP_IF_TRUTHY:
        if (uint8_t tag = state.tryGetTag(OP_A(inst)); tag != 0xff)
        {
            if (tag == LUA_TNIL)
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
            else if (tag != LUA_TBOOLEAN)
                replace(function, block, index, {IrCmd::JUMP, {OP_B(inst)}});
        }
        break;
    case IrCmd::JUMP_IF_FALSY:
        if (uint8_t tag = state.tryGetTag(OP_A(inst)); tag != 0xff)
        {
            if (tag == LUA_TNIL)
                replace(function, block, index, {IrCmd::JUMP, {OP_B(inst)}});
            else if (tag != LUA_TBOOLEAN)
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
        }
        break;
    case IrCmd::JUMP_EQ_TAG:
    {
        uint8_t tagA = OP_A(inst).kind == IrOpKind::Constant ? function.tagOp(OP_A(inst)) : state.tryGetTag(OP_A(inst));
        uint8_t tagB = OP_B(inst).kind == IrOpKind::Constant ? function.tagOp(OP_B(inst)) : state.tryGetTag(OP_B(inst));

        if (tagA != 0xff && tagB != 0xff)
        {
            if (tagA == tagB)
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
        }
        else if (OP_A(inst) == OP_B(inst))
        {
            replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
        }
        break;
    }
    case IrCmd::JUMP_CMP_INT:
    {
        std::optional<int> valueA = function.asIntOp(OP_A(inst).kind == IrOpKind::Constant ? OP_A(inst) : state.tryGetValue(OP_A(inst)));
        std::optional<int> valueB = function.asIntOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst)));

        if (FFlag::LuauCodegenJumpCmpIntFoldFix && valueA && valueB)
        {
            if (compare(*valueA, *valueB, conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
        }
        else if (valueA && valueB)
        {
            if (compare(*valueA, *valueB, conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
        }
        break;
    }
    case IrCmd::JUMP_CMP_NUM:
    {
        std::optional<double> valueA = function.asDoubleOp(OP_A(inst).kind == IrOpKind::Constant ? OP_A(inst) : state.tryGetValue(OP_A(inst)));
        std::optional<double> valueB = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst)));

        if (valueA && valueB)
        {
            if (compare(*valueA, *valueB, conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
        }
        break;
    }
    case IrCmd::JUMP_CMP_FLOAT:
    {
        std::optional<double> valueA = function.asDoubleOp(OP_A(inst).kind == IrOpKind::Constant ? OP_A(inst) : state.tryGetValue(OP_A(inst)));
        std::optional<double> valueB = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst)));

        if (valueA && valueB)
        {
            if (compare(float(*valueA), float(*valueB), conditionOp(OP_C(inst))))
                replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            else
                replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
        }
        break;
    }
    case IrCmd::JUMP_FORN_LOOP_COND:
    {
        std::optional<double> step = function.asDoubleOp(OP_C(inst).kind == IrOpKind::Constant ? OP_C(inst) : state.tryGetValue(OP_C(inst)));

        if (!step)
            break;

        std::optional<double> idx = function.asDoubleOp(OP_A(inst).kind == IrOpKind::Constant ? OP_A(inst) : state.tryGetValue(OP_A(inst)));
        std::optional<double> limit = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst)));

        if (*step > 0)
        {
            if (idx && limit)
            {
                if (compare(*idx, *limit, IrCondition::NotLessEqual))
                    replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
                else
                    replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            }
            else
            {
                replace(
                    function,
                    block,
                    index,
                    IrInst{IrCmd::JUMP_CMP_NUM, {OP_A(inst), OP_B(inst), build.cond(IrCondition::NotLessEqual), OP_E(inst), OP_D(inst)}}
                );
            }
        }
        else
        {
            if (idx && limit)
            {
                if (compare(*limit, *idx, IrCondition::NotLessEqual))
                    replace(function, block, index, {IrCmd::JUMP, {OP_E(inst)}});
                else
                    replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
            }
            else
            {
                replace(
                    function,
                    block,
                    index,
                    IrInst{IrCmd::JUMP_CMP_NUM, {OP_B(inst), OP_A(inst), build.cond(IrCondition::NotLessEqual), OP_E(inst), OP_D(inst)}}
                );
            }
        }
        break;
    }
    case IrCmd::GET_UPVALUE:
        state.substituteOrRecordVmUpvalueLoad(inst);
        break;
    case IrCmd::SET_UPVALUE:
        state.forwardVmUpvalueStoreToLoad(inst);

        if (uint8_t tag = state.tryGetTag(OP_B(inst)); tag != 0xff)
        {
            replace(function, OP_C(inst), build.constTag(tag));
        }
        break;
    case IrCmd::CHECK_TAG:
    {
        uint8_t b = function.tagOp(OP_B(inst));
        uint8_t tag = state.tryGetTag(OP_A(inst));

        if (tag == 0xff)
        {
            if (IrOp value = state.tryGetValue(OP_A(inst)); value.kind == IrOpKind::Constant)
            {
                if (function.constOp(value).kind == IrConstKind::Double)
                    tag = LUA_TNUMBER;
                else if (function.constOp(value).kind == IrConstKind::Int64)
                    tag = LUA_TINTEGER;
            }
        }

        if (tag != 0xff)
        {
            if (tag == b)
            {
                if (FFlag::DebugLuauAbortingChecks)
                    replace(function, OP_C(inst), build.undef());
                else
                    kill(function, inst);
            }
            else
            {
                replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}}); // Shows a conflict in assumptions on this path
            }
        }
        else
        {
            IrInst& lhs = function.instOp(OP_A(inst));

            // If we are loading a tag from a register which has previously been loaded as a full TValue
            // We can associate a known tag with that instruction going forward
            if (lhs.cmd == IrCmd::LOAD_TAG && OP_A(lhs).kind == IrOpKind::VmReg)
            {
                if (uint32_t* prevIdx = state.getPreviousVersionedLoadIndex(IrCmd::LOAD_TVALUE, OP_A(lhs)))
                    state.instTag[*prevIdx] = b;
            }

            state.updateTag(OP_A(inst), b); // We can assume the tag value going forward
        }
        break;
    }
    case IrCmd::CHECK_TRUTHY:
        // It is possible to check if current tag in state is truthy or not, but this case almost never comes up
        break;
    case IrCmd::CHECK_READONLY:
        if (RegisterInfo* info = state.tryGetRegisterInfo(OP_A(inst)))
        {
            if (info->knownNotReadonly)
            {
                if (FFlag::DebugLuauAbortingChecks)
                    replace(function, OP_B(inst), build.undef());
                else
                    kill(function, inst);
            }
            else
            {
                info->knownNotReadonly = true;
            }
        }
        break;
    case IrCmd::CHECK_NO_METATABLE:
        if (RegisterInfo* info = state.tryGetRegisterInfo(OP_A(inst)))
        {
            if (info->knownNoMetatable)
            {
                if (FFlag::DebugLuauAbortingChecks)
                    replace(function, OP_B(inst), build.undef());
                else
                    kill(function, inst);
            }
            else
            {
                info->knownNoMetatable = true;
            }
        }
        break;
    case IrCmd::CHECK_SAFE_ENV:
        if (state.inSafeEnv)
        {
            if (FFlag::DebugLuauAbortingChecks)
                replace(function, OP_A(inst), build.undef());
            else
                kill(function, inst);
        }
        else
        {
            state.inSafeEnv = true;
        }
        break;
    case IrCmd::CHECK_BUFFER_LEN:
    {
        std::optional<int> bufferOffset = function.asIntOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst)));

        if (FFlag::LuauCodegenBufferRangeMerge4)
        {
            int minOffset = function.intOp(OP_C(inst));
            int maxOffset = function.intOp(OP_D(inst));

            CODEGEN_ASSERT(minOffset < maxOffset);
            int accessSize = maxOffset - minOffset;
            CODEGEN_ASSERT(accessSize > 0);

            if (bufferOffset)
            {
                // Negative offsets and offsets overflowing signed integer will jump to fallback, no need to keep the check
                if (*bufferOffset < 0 || unsigned(*bufferOffset) + unsigned(accessSize) >= unsigned(INT_MAX))
                {
                    replace(function, block, index, {IrCmd::JUMP, {OP_F(inst)}});
                    break;
                }
            }

            for (uint32_t prevIdx : state.checkBufferLenCache)
            {
                IrInst& prev = function.instructions[prevIdx];

                // Exactly the same access removes the instruction
                if (OP_A(prev) == OP_A(inst) && OP_B(prev) == OP_B(inst) && OP_C(prev) == OP_C(inst) && OP_D(prev) == OP_D(inst))
                {
                    if (FFlag::DebugLuauAbortingChecks)
                        replace(function, OP_F(inst), build.undef());
                    else
                        kill(function, inst);
                    return; // Break out from both the loop and the switch
                }

                // Constant offset access at different locations might be merged
                if (OP_A(prev) == OP_A(inst) && OP_B(inst).kind == IrOpKind::Constant && OP_B(prev).kind == IrOpKind::Constant)
                {
                    int currBound = function.intOp(OP_B(inst));
                    int prevBound = function.intOp(OP_B(prev));

                    // Negative and overflowing constant offsets should already be replaced with unconditional jumps to a fallback
                    CODEGEN_ASSERT(currBound >= 0);
                    CODEGEN_ASSERT(prevBound >= 0);

                    // Rebase current check to the same base offset
                    int extraOffset = currBound - prevBound;

                    if (state.tryMergeAndKillBufferLengthCheck(build, block, inst, prev, extraOffset))
                        return; // Break out from both the loop and the switch

                    continue;
                }

                if (state.tryMergeBufferRangeCheck(build, block, inst, prev))
                    return; // Break out from both the loop and the switch
            }
        }
        else
        {
            int accessSize = function.intOp(OP_C(inst));
            CODEGEN_ASSERT(accessSize > 0);

            if (bufferOffset)
            {
                // Negative offsets and offsets overflowing signed integer will jump to fallback, no need to keep the check
                if (*bufferOffset < 0 || unsigned(*bufferOffset) + unsigned(accessSize) >= unsigned(INT_MAX))
                {
                    replace(function, block, index, {IrCmd::JUMP, {OP_D(inst)}});
                    break;
                }
            }

            for (uint32_t prevIdx : state.checkBufferLenCache)
            {
                IrInst& prev = function.instructions[prevIdx];

                if (OP_A(prev) != OP_A(inst) || OP_C(prev) != OP_C(inst))
                    continue;

                if (OP_B(prev) == OP_B(inst))
                {
                    if (FFlag::DebugLuauAbortingChecks)
                        replace(function, OP_D(inst), build.undef());
                    else
                        kill(function, inst);
                    return; // Break out from both the loop and the switch
                }
                else if (OP_B(inst).kind == IrOpKind::Constant && OP_B(prev).kind == IrOpKind::Constant)
                {
                    // If arguments are different constants, we can check if a larger bound was already tested or if the previous bound can be raised
                    int currBound = function.intOp(OP_B(inst));
                    int prevBound = function.intOp(OP_B(prev));

                    // Negative and overflowing constant offsets should already be replaced with unconditional jumps to a fallback
                    CODEGEN_ASSERT(currBound >= 0);
                    CODEGEN_ASSERT(prevBound >= 0);

                    if (unsigned(currBound) >= unsigned(prevBound))
                        replace(function, OP_B(prev), OP_B(inst));

                    if (FFlag::DebugLuauAbortingChecks)
                        replace(function, OP_D(inst), build.undef());
                    else
                        kill(function, inst);

                    return; // Break out from both the loop and the switch
                }
            }
        }

        if (int(state.checkBufferLenCache.size()) < FInt::LuauCodeGenReuseSlotLimit)
            state.checkBufferLenCache.push_back(index);
        break;
    }
    case IrCmd::CHECK_USERDATA_TAG:
    {
        for (uint32_t prevIdx : state.useradataTagCache)
        {
            IrInst& prev = function.instructions[prevIdx];

            if (prev.cmd == IrCmd::CHECK_USERDATA_TAG)
            {
                if (OP_A(prev) != OP_A(inst) || OP_B(prev) != OP_B(inst))
                    continue;
            }
            else if (prev.cmd == IrCmd::NEW_USERDATA)
            {
                if (OP_A(inst).kind != IrOpKind::Inst || prevIdx != OP_A(inst).index || OP_B(prev) != OP_B(inst))
                    continue;
            }

            if (FFlag::DebugLuauAbortingChecks)
                replace(function, OP_C(inst), build.undef());
            else
                kill(function, inst);

            return; // Break out from both the loop and the switch
        }

        if (int(state.useradataTagCache.size()) < FInt::LuauCodeGenReuseUdataTagLimit)
            state.useradataTagCache.push_back(index);
        break;
    }
    case IrCmd::CHECK_CMP_NUM:
    case IrCmd::CHECK_CMP_INT:
    case IrCmd::CHECK_CMP_INT64:
        break;
    case IrCmd::BUFFER_READI8:
        state.substituteOrRecordBufferLoad(block, index, inst, 1);
        break;
    case IrCmd::BUFFER_READU8:
        state.substituteOrRecordBufferLoad(block, index, inst, 1);
        break;
    case IrCmd::BUFFER_WRITEI8:
        if (IrInst* src = function.asInstOp(OP_C(inst)))
        {
            std::optional<int> intSrcB = function.asIntOp(OPT_OP_B(*src));

            if (src->cmd == IrCmd::SEXTI8_INT)
                replace(function, OP_C(inst), OP_A(src));
            else if (src->cmd == IrCmd::BITAND_UINT && intSrcB && *intSrcB == 0xff)
                replace(function, OP_C(inst), OP_A(src));
        }

        state.forwardBufferStoreToLoad(inst, IrCmd::BUFFER_READI8, 1);
        break;
    case IrCmd::BUFFER_READI16:
        state.substituteOrRecordBufferLoad(block, index, inst, 2);
        break;
    case IrCmd::BUFFER_READU16:
        state.substituteOrRecordBufferLoad(block, index, inst, 2);
        break;
    case IrCmd::BUFFER_WRITEI16:
        if (IrInst* src = function.asInstOp(OP_C(inst)))
        {
            std::optional<int> intSrcB = function.asIntOp(OPT_OP_B(*src));

            if (src->cmd == IrCmd::SEXTI16_INT)
                replace(function, OP_C(inst), OP_A(src));
            else if (src->cmd == IrCmd::BITAND_UINT && intSrcB && *intSrcB == 0xffff)
                replace(function, OP_C(inst), OP_A(src));
        }

        state.forwardBufferStoreToLoad(inst, IrCmd::BUFFER_READI16, 2);
        break;
    case IrCmd::BUFFER_READI32:
        state.substituteOrRecordBufferLoad(block, index, inst, 4);
        break;
    case IrCmd::BUFFER_WRITEI32:
        if (IrInst* src = function.asInstOp(OP_C(inst)))
        {
            if (src->cmd == IrCmd::TRUNCATE_UINT)
                replace(function, OP_C(inst), OP_A(src));
        }

        state.forwardBufferStoreToLoad(inst, IrCmd::BUFFER_READI32, 4);
        break;
    case IrCmd::BUFFER_READF32:
        state.substituteOrRecordBufferLoad(block, index, inst, 4);
        break;
    case IrCmd::BUFFER_WRITEF32:
        state.forwardBufferStoreToLoad(inst, IrCmd::BUFFER_READF32, 4);
        break;
    case IrCmd::BUFFER_READF64:
        state.substituteOrRecordBufferLoad(block, index, inst, 8);
        break;
    case IrCmd::BUFFER_WRITEF64:
        state.forwardBufferStoreToLoad(inst, IrCmd::BUFFER_READF64, 8);
        break;
    case IrCmd::CHECK_GC:
        // It is enough to perform a GC check once in a block
        if (state.checkedGc)
        {
            kill(function, inst);
        }
        else
        {
            state.checkedGc = true;

            // GC assist might modify table data (hash part)
            state.invalidateHeapTableData();
        }
        break;
    case IrCmd::BARRIER_OBJ:
    case IrCmd::BARRIER_TABLE_FORWARD:
        if (OP_B(inst).kind == IrOpKind::VmReg)
        {
            if (uint8_t tag = state.tryGetTag(OP_B(inst)); tag != 0xff)
            {
                // If the written object is not collectable, barrier is not required
                if (!isGCO(tag))
                    kill(function, inst);
                else
                    replace(function, OP_C(inst), build.constTag(tag));
            }
        }
        break;

    case IrCmd::FASTCALL:
    {
        LuauBuiltinFunction bfid = LuauBuiltinFunction(function.uintOp(OP_A(inst)));
        int firstReturnReg = vmRegOp(OP_B(inst));
        int nresults = function.intOp(OP_D(inst));

        // TODO: FASTCALL is more restrictive than INVOKE_FASTCALL; we should either determine the exact semantics, or rework it
        handleBuiltinEffects(state, bfid, firstReturnReg, nresults);

        switch (bfid)
        {
        case LBF_MATH_MODF:
        case LBF_MATH_FREXP:
            state.updateTag(IrOp{IrOpKind::VmReg, uint8_t(firstReturnReg)}, LUA_TNUMBER);

            if (nresults > 1)
                state.updateTag(IrOp{IrOpKind::VmReg, uint8_t(firstReturnReg + 1)}, LUA_TNUMBER);
            break;
        default:
            break;
        }
        break;
    }
    case IrCmd::INVOKE_FASTCALL:
        handleBuiltinEffects(state, LuauBuiltinFunction(function.uintOp(OP_A(inst))), vmRegOp(OP_B(inst)), function.intOp(OP_G(inst)));
        break;

        // These instructions don't have an effect on register/memory state we are tracking
    case IrCmd::NOP:
        break;
    case IrCmd::LOAD_ENV:
        break;
    case IrCmd::GET_ARR_ADDR:
        for (uint32_t prevIdx : state.getArrAddrCache)
        {
            IrInst& prev = function.instructions[prevIdx];

            if (OP_A(prev) == OP_A(inst) && OP_B(prev) == OP_B(inst))
            {
                substitute(function, inst, IrOp{IrOpKind::Inst, prevIdx});
                return; // Break out from both the loop and the switch
            }
        }

        if (int(state.getArrAddrCache.size()) < FInt::LuauCodeGenReuseSlotLimit)
            state.getArrAddrCache.push_back(index);
        break;
    case IrCmd::GET_SLOT_NODE_ADDR:
        for (size_t i = 0; i < state.getSlotNodeCache.size(); i++)
        {
            auto&& [prevIdx, num, lastNum] = state.getSlotNodeCache[i];

            IrInst& prev = function.instructions[prevIdx];

            if (OP_A(prev) == OP_A(inst) && OP_C(prev) == OP_C(inst))
            {
                // Check if this reuse will increase the overall register pressure over the limit
                int limit = FInt::LuauCodeGenLiveSlotReuseLimit;

                if (int(state.getSlotNodeCache.size()) > limit && state.getMaxInternalOverlap(state.getSlotNodeCache, i) > limit)
                    return;

                // Update live range of the value from the optimization standpoint
                lastNum = state.instPos;

                substitute(function, inst, IrOp{IrOpKind::Inst, prevIdx});
                return; // Break out from both the loop and the switch
            }
        }

        if (int(state.getSlotNodeCache.size()) < FInt::LuauCodeGenReuseSlotLimit)
            state.getSlotNodeCache.push_back({index, state.instPos, state.instPos});
        break;
    case IrCmd::GET_HASH_NODE_ADDR:
    case IrCmd::GET_CLOSURE_UPVAL_ADDR:
        break;
    case IrCmd::ADD_INT64:
    case IrCmd::SUB_INT64:
    case IrCmd::MUL_INT64:
    case IrCmd::DIV_INT64:
    case IrCmd::IDIV_INT64:
    case IrCmd::CHECK_DIV_INT64:
    case IrCmd::UDIV_INT64:
    case IrCmd::REM_INT64:
    case IrCmd::UREM_INT64:
    case IrCmd::MOD_INT64:
    case IrCmd::ADD_INT:
    case IrCmd::SUB_INT:
    case IrCmd::SEXTI8_INT:
    case IrCmd::SEXTI16_INT:
        state.substituteOrRecord(inst, index);
        break;
    case IrCmd::ADD_NUM:
    case IrCmd::SUB_NUM:
        if (std::optional<double> k = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst))))
        {
            // a + 0.0 and a - (-0.0) can't be folded since the behavior is different for negative zero
            // however, a - 0.0 and a + (-0.0) can be folded into a
            if (*k == 0.0 && bool(signbit(*k)) == (inst.cmd == IrCmd::ADD_NUM))
                substitute(function, inst, OP_A(inst));
            else
                state.substituteOrRecord(inst, index);
        }
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::MUL_NUM:
        if (std::optional<double> k = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst))))
        {
            if (*k == 1.0) // a * 1.0 = a
                substitute(function, inst, OP_A(inst));
            else if (*k == 2.0) // a * 2.0 = a + a
                replace(function, block, index, {IrCmd::ADD_NUM, {OP_A(inst), OP_A(inst)}});
            else if (*k == -1.0) // a * -1.0 = -a
                replace(function, block, index, {IrCmd::UNM_NUM, {OP_A(inst)}});
            else
                state.substituteOrRecord(inst, index);
        }
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::DIV_NUM:
        if (std::optional<double> k = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst))))
        {
            if (*k == 1.0) // a / 1.0 = a
                substitute(function, inst, OP_A(inst));
            else if (*k == -1.0) // a / -1.0 = -a
                replace(function, block, index, {IrCmd::UNM_NUM, {OP_A(inst)}});
            else if (int exp = 0; frexp(*k, &exp) == 0.5 && exp >= -1000 && exp <= 1000) // a / 2^k = a * 2^-k
                replace(function, block, index, {IrCmd::MUL_NUM, {OP_A(inst), build.constDouble(1.0 / *k)}});
            else
                state.substituteOrRecord(inst, index);
        }
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::IDIV_NUM:
    case IrCmd::MULADD_NUM:
    case IrCmd::MOD_NUM:
    case IrCmd::MIN_NUM:
    case IrCmd::MAX_NUM:
    case IrCmd::UNM_NUM:
    case IrCmd::FLOOR_NUM:
    case IrCmd::CEIL_NUM:
    case IrCmd::ROUND_NUM:
    case IrCmd::SQRT_NUM:
    case IrCmd::ABS_NUM:
    case IrCmd::SIGN_NUM:
    case IrCmd::SELECT_NUM:
    case IrCmd::SELECT_INT64:
    case IrCmd::SELECT_VEC:
    case IrCmd::MULADD_VEC:
    case IrCmd::EXTRACT_VEC:
    case IrCmd::NOT_ANY:
        state.substituteOrRecord(inst, index);
        break;
    case IrCmd::ADD_FLOAT:
    case IrCmd::SUB_FLOAT:
        if (std::optional<double> k = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst))))
        {
            // a + 0.0 and a - (-0.0) can't be folded since the behavior is different for negative zero
            // however, a - 0.0 and a + (-0.0) can be folded into a
            if (float(*k) == 0.0 && bool(signbit(float(*k))) == (inst.cmd == IrCmd::ADD_FLOAT))
                substitute(function, inst, OP_A(inst));
            else
                state.substituteOrRecord(inst, index);
        }
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::MUL_FLOAT:
        if (std::optional<double> k = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst))))
        {
            if (float(*k) == 1.0f) // a * 1.0 = a
                substitute(function, inst, OP_A(inst));
            else if (float(*k) == 2.0f) // a * 2.0 = a + a
                replace(function, block, index, {IrCmd::ADD_FLOAT, {OP_A(inst), OP_A(inst)}});
            else if (float(*k) == -1.0f) // a * -1.0 = -a
                replace(function, block, index, {IrCmd::UNM_FLOAT, {OP_A(inst)}});
            else
                state.substituteOrRecord(inst, index);
        }
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::DIV_FLOAT:
        if (std::optional<double> k = function.asDoubleOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst))))
        {
            if (float(*k) == 1.0) // a / 1.0 = a
                substitute(function, inst, OP_A(inst));
            else if (float(*k) == -1.0) // a / -1.0 = -a
                replace(function, block, index, {IrCmd::UNM_FLOAT, {OP_A(inst)}});
            else if (int exp = 0; frexpf(float(*k), &exp) == 0.5f && exp >= -1000 && exp <= 1000) // a / 2^k = a * 2^-k
                replace(function, block, index, {IrCmd::MUL_FLOAT, {OP_A(inst), build.constDouble(1.0f / float(*k))}});
            else
                state.substituteOrRecord(inst, index);
        }
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::MIN_FLOAT:
    case IrCmd::MAX_FLOAT:
    case IrCmd::UNM_FLOAT:
    case IrCmd::FLOOR_FLOAT:
    case IrCmd::CEIL_FLOAT:
    case IrCmd::SQRT_FLOAT:
    case IrCmd::ABS_FLOAT:
    case IrCmd::SIGN_FLOAT:
        state.substituteOrRecord(inst, index);
        break;
    case IrCmd::SELECT_IF_TRUTHY:
        if (uint8_t tag = state.tryGetTag(OP_A(inst)); tag != 0xff)
        {
            if (tag == LUA_TNIL)
                substitute(function, inst, OP_C(inst));
            else if (tag != LUA_TBOOLEAN)
                substitute(function, inst, OP_B(inst));
        }
        break;
    case IrCmd::CMP_INT:
    case IrCmd::CMP_INT64:
        break;
    case IrCmd::CMP_ANY:
        state.invalidateUserCall();
        break;
    case IrCmd::CMP_TAG:
        break;
    case IrCmd::CMP_SPLIT_TVALUE:
        if (function.proto)
        {
            uint8_t tagA = OP_A(inst).kind == IrOpKind::Constant ? function.tagOp(OP_A(inst)) : state.tryGetTag(OP_A(inst));
            uint8_t tagB = OP_B(inst).kind == IrOpKind::Constant ? function.tagOp(OP_B(inst)) : state.tryGetTag(OP_B(inst));

            // Try to find pattern like type(x) == 'tagname' or typeof(x) == 'tagname'
            if (tagA == LUA_TSTRING && tagB == LUA_TSTRING && OP_C(inst).kind == IrOpKind::Inst && OP_D(inst).kind == IrOpKind::Inst)
            {
                IrInst& lhs = function.instOp(OP_C(inst));
                IrInst& rhs = function.instOp(OP_D(inst));

                if (rhs.cmd == IrCmd::LOAD_POINTER && OP_A(rhs).kind == IrOpKind::VmConst)
                {
                    TValue name = function.proto->k[vmConstOp(OP_A(rhs))];
                    CODEGEN_ASSERT(name.tt == LUA_TSTRING);
                    std::string_view nameStr{svalue(&name), tsvalue(&name)->len};

                    if (int tag = tryGetTagForTypename(nameStr, lhs.cmd == IrCmd::GET_TYPEOF); tag != 0xff)
                    {
                        if (lhs.cmd == IrCmd::GET_TYPE)
                        {
                            replace(function, block, index, {IrCmd::CMP_TAG, {OP_A(lhs), build.constTag(tag), OP_E(inst)}});
                            foldConstants(build, function, block, index);
                        }
                        else if (lhs.cmd == IrCmd::GET_TYPEOF)
                        {
                            replace(function, block, index, {IrCmd::CMP_TAG, {OP_A(lhs), build.constTag(tag), OP_E(inst)}});
                            foldConstants(build, function, block, index);
                        }
                    }
                }
            }
        }
        break;
    case IrCmd::JUMP:
        break;
    case IrCmd::JUMP_EQ_POINTER:
        if (function.proto)
        {
            // Try to find pattern like type(x) == 'tagname' or typeof(x) == 'tagname'
            if (OP_A(inst).kind == IrOpKind::Inst && OP_B(inst).kind == IrOpKind::Inst)
            {
                IrInst& lhs = function.instOp(OP_A(inst));
                IrInst& rhs = function.instOp(OP_B(inst));

                if (rhs.cmd == IrCmd::LOAD_POINTER && OP_A(rhs).kind == IrOpKind::VmConst)
                {
                    TValue name = function.proto->k[vmConstOp(OP_A(rhs))];
                    CODEGEN_ASSERT(name.tt == LUA_TSTRING);
                    std::string_view nameStr{svalue(&name), tsvalue(&name)->len};

                    if (int tag = tryGetTagForTypename(nameStr, lhs.cmd == IrCmd::GET_TYPEOF); tag != 0xff)
                    {
                        if (lhs.cmd == IrCmd::GET_TYPE)
                        {
                            replace(function, block, index, {IrCmd::JUMP_EQ_TAG, {OP_A(lhs), build.constTag(tag), OP_C(inst), OP_D(inst)}});
                            foldConstants(build, function, block, index);
                        }
                        else if (lhs.cmd == IrCmd::GET_TYPEOF)
                        {
                            replace(function, block, index, {IrCmd::JUMP_EQ_TAG, {OP_A(lhs), build.constTag(tag), OP_C(inst), OP_D(inst)}});
                            foldConstants(build, function, block, index);
                        }
                    }
                }
            }
        }
        break;
    case IrCmd::JUMP_SLOT_MATCH:
    case IrCmd::TABLE_LEN:
        break;
    case IrCmd::TABLE_SETNUM:
        state.invalidateTableArraySize();
        break;
    case IrCmd::STRING_LEN:
    case IrCmd::NEW_TABLE:
    case IrCmd::DUP_TABLE:
        break;
    case IrCmd::TRY_NUM_TO_INDEX:
        for (uint32_t prevIdx : state.tryNumToIndexCache)
        {
            IrInst& prev = function.instructions[prevIdx];

            if (OP_A(prev) == OP_A(inst))
            {
                substitute(function, inst, IrOp{IrOpKind::Inst, prevIdx});
                return; // Break out from both the loop and the switch
            }
        }

        if (int(state.tryNumToIndexCache.size()) < FInt::LuauCodeGenReuseSlotLimit)
            state.tryNumToIndexCache.push_back(index);
        break;
    case IrCmd::TRY_CALL_FASTGETTM:
        break;
    case IrCmd::NEW_USERDATA:
        if (int(state.useradataTagCache.size()) < FInt::LuauCodeGenReuseUdataTagLimit)
            state.useradataTagCache.push_back(index);
        break;
    case IrCmd::INT64_TO_NUM:
    case IrCmd::INT_TO_NUM:
        state.substituteOrRecord(inst, index);
        break;
    case IrCmd::UINT_TO_NUM:
    case IrCmd::UINT_TO_FLOAT:
        // UINT_TO_***(TRUNCATE_UINT(NUM_TO_UINT(x)) => UINT_TO_***(NUM_TO_UINT(x)) since instruction handles truncation of NUM_TO_UINT result
        if (IrInst* src = function.asInstOp(OP_A(inst)); src && src->cmd == IrCmd::TRUNCATE_UINT)
        {
            if (IrInst* srcOfSrc = function.asInstOp(OP_A(src)); srcOfSrc && srcOfSrc->cmd == IrCmd::NUM_TO_UINT)
                replace(function, OP_A(inst), OP_A(src));
        }

        state.substituteOrRecord(inst, index);
        break;
    case IrCmd::NUM_TO_INT:
    {
        IrInst* src = function.asInstOp(OP_A(inst));

        if (src && src->cmd == IrCmd::INT_TO_NUM)
        {
            substitute(function, inst, OP_A(src));
            break;
        }

        if (FFlag::LuauCodegenBufferRangeMerge4 && src && src->cmd == IrCmd::ADD_NUM)
        {
            if (std::optional<double> arg = function.asDoubleOp(OP_B(src)); arg && *arg == 0.0)
            {
                replace(function, OP_A(inst), OP_A(src));
                state.substituteOrRecord(inst, index);
                break;
            }

            if (std::optional<double> arg = function.asDoubleOp(OP_A(src)); arg && *arg == 0.0)
            {
                replace(function, OP_A(inst), OP_B(src));
                state.substituteOrRecord(inst, index);
                break;
            }
        }

        // INT and UINT are stored in the same way and can be reinterpreted (constants are not and are handled in foldConstants)
        if (src && src->cmd == IrCmd::UINT_TO_NUM && OP_A(src).kind != IrOpKind::Constant)
        {
            if (IrInst* srcOfSrc = function.asInstOp(OP_A(src)); srcOfSrc && producesDirtyHighRegisterBits(srcOfSrc->cmd))
                replace(function, block, index, IrInst{IrCmd::TRUNCATE_UINT, {OP_A(src)}});
            else
                substitute(function, inst, OP_A(src));
            break;
        }

        state.substituteOrRecord(inst, index);
        break;
    }
    case IrCmd::NUM_TO_INT64:
    {
        IrInst* src = function.asInstOp(OP_A(inst));

        if (src && src->cmd == IrCmd::INT64_TO_NUM)
        {
            substitute(function, inst, OP_A(src));
            break;
        }

        if (FFlag::LuauCodegenBufferRangeMerge4 && src && src->cmd == IrCmd::ADD_NUM)
        {
            if (std::optional<double> arg = function.asDoubleOp(OP_B(src)); arg && *arg == 0.0)
            {
                replace(function, OP_A(inst), OP_A(src));
                state.substituteOrRecord(inst, index);
                break;
            }

            if (std::optional<double> arg = function.asDoubleOp(OP_A(src)); arg && *arg == 0.0)
            {
                replace(function, OP_A(inst), OP_B(src));
                state.substituteOrRecord(inst, index);
                break;
            }
        }

        state.substituteOrRecord(inst, index);
        break;
    }
    case IrCmd::NUM_TO_UINT:
    {
        IrInst* src = function.asInstOp(OP_A(inst));

        if (src && src->cmd == IrCmd::UINT_TO_NUM)
        {
            if (IrInst* srcOfSrc = function.asInstOp(OP_A(src)); srcOfSrc && producesDirtyHighRegisterBits(srcOfSrc->cmd))
                replace(function, block, index, IrInst{IrCmd::TRUNCATE_UINT, {OP_A(src)}});
            else
                substitute(function, inst, OP_A(src));
            break;
        }

        // INT and UINT are stored in the same way and can be reinterpreted (constants are not and are handled in foldConstants)
        if (src && src->cmd == IrCmd::INT_TO_NUM && OP_A(src).kind != IrOpKind::Constant)
        {
            substitute(function, inst, OP_A(src));
            break;
        }

        if (src && src->cmd == IrCmd::ADD_NUM)
        {
            IrInst* addSrc1 = function.asInstOp(OP_A(src));
            std::optional<double> addNum1 = function.asDoubleOp(OP_A(src));
            IrInst* addSrc2 = function.asInstOp(OP_B(src));
            std::optional<double> addNum2 = function.asDoubleOp(OP_B(src));

            if (addSrc1 && addSrc1->cmd == IrCmd::UINT_TO_NUM && addSrc2 && addSrc2->cmd == IrCmd::UINT_TO_NUM)
            {
                // If we are converting an addition of two sources that were initially and UINT, we can instead add our value as UINT
                replace(function, block, index, {IrCmd::ADD_INT, {OP_A(addSrc1), OP_A(addSrc2)}});
                break;
            }
            else if (addNum1 && safeIntegerConstant(*addNum1) && addSrc2 && addSrc2->cmd == IrCmd::UINT_TO_NUM)
            {
                // If we are converting an addition of two sources that were initially and UINT, we can instead add our value as UINT
                replace(function, block, index, {IrCmd::ADD_INT, {build.constInt(unsigned((long long)*addNum1)), OP_A(addSrc2)}});
                break;
            }
            else if (addSrc1 && addSrc1->cmd == IrCmd::UINT_TO_NUM && addNum2 && safeIntegerConstant(*addNum2))
            {
                // If we are converting an addition of two sources that were initially and UINT, we can instead add our value as UINT
                replace(function, block, index, {IrCmd::ADD_INT, {OP_A(addSrc1), build.constInt(unsigned((long long)*addNum2))}});
                break;
            }
        }
        else if (src && src->cmd == IrCmd::SUB_NUM)
        {
            IrInst* addSrc1 = function.asInstOp(OP_A(src));
            std::optional<double> addNum1 = function.asDoubleOp(OP_A(src));
            IrInst* addSrc2 = function.asInstOp(OP_B(src));
            std::optional<double> addNum2 = function.asDoubleOp(OP_B(src));

            if (addSrc1 && addSrc1->cmd == IrCmd::UINT_TO_NUM && addSrc2 && addSrc2->cmd == IrCmd::UINT_TO_NUM)
            {
                // If we are converting an addition of two sources that were initially and UINT, we can instead add our value as UINT
                replace(function, block, index, {IrCmd::SUB_INT, {OP_A(addSrc1), OP_A(addSrc2)}});
                break;
            }
            else if (addNum1 && safeIntegerConstant(*addNum1) && addSrc2 && addSrc2->cmd == IrCmd::UINT_TO_NUM)
            {
                // If we are converting an addition of two sources that were initially and UINT, we can instead add our value as UINT
                replace(function, block, index, {IrCmd::SUB_INT, {build.constInt(unsigned((long long)*addNum1)), OP_A(addSrc2)}});
                break;
            }
            else if (addSrc1 && addSrc1->cmd == IrCmd::UINT_TO_NUM && addNum2 && safeIntegerConstant(*addNum2))
            {
                // If we are converting an addition of two sources that were initially and UINT, we can instead add our value as UINT
                replace(function, block, index, {IrCmd::SUB_INT, {OP_A(addSrc1), build.constInt(unsigned((long long)*addNum2))}});
                break;
            }
        }

        state.substituteOrRecord(inst, index);
        break;
    }
    case IrCmd::TRUNCATE_UINT:
        // Truncation can be skipped if the source does not produce dirty high register bits: TRUNCATE_UINT(uint32) => uint32
        if (IrInst* src = function.asInstOp(OP_A(inst)); src && !producesDirtyHighRegisterBits(src->cmd))
            substitute(function, inst, OP_A(inst));
        else
            state.substituteOrRecord(inst, index);
        break;
    case IrCmd::FLOAT_TO_NUM:
        // double->float->double conversion cannot be skipped as it affects value precision
        state.substituteOrRecord(inst, index);
        break;
    case IrCmd::NUM_TO_FLOAT:
        if (IrInst* src = function.asInstOp(OP_A(inst)))
        {
            if (src->cmd == IrCmd::FLOAT_TO_NUM)
                substitute(function, inst, OP_A(src)); // Skip float->double->float conversion: NUM_TO_FLOAT(FLOAT_TO_NUM(value)) => value
            else if (src->cmd == IrCmd::UINT_TO_NUM)
                replace(function, block, index, IrInst{IrCmd::UINT_TO_FLOAT, {OP_A(src)}});
            else
                state.substituteOrRecord(inst, index);
        }
        else
        {
            state.substituteOrRecord(inst, index);
        }
        break;
    case IrCmd::CHECK_ARRAY_SIZE:
    {
        std::optional<int> arrayIndex = function.asIntOp(OP_B(inst).kind == IrOpKind::Constant ? OP_B(inst) : state.tryGetValue(OP_B(inst)));

        // Negative offsets will jump to fallback, no need to keep the check
        if (arrayIndex && *arrayIndex < 0)
        {
            replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
            break;
        }

        if (RegisterInfo* info = state.tryGetRegisterInfo(OP_A(inst)); info && arrayIndex)
        {
            if (info->knownTableArraySize >= 0)
            {
                if (unsigned(*arrayIndex) < unsigned(info->knownTableArraySize))
                {
                    if (FFlag::DebugLuauAbortingChecks)
                        replace(function, OP_C(inst), build.undef());
                    else
                        kill(function, inst);
                }
                else
                {
                    replace(function, block, index, {IrCmd::JUMP, {OP_C(inst)}});
                }

                break;
            }
        }

        for (uint32_t prevIdx : state.checkArraySizeCache)
        {
            IrInst& prev = function.instructions[prevIdx];

            if (OP_A(prev) != OP_A(inst))
                continue;

            bool sameBoundary = OP_B(prev) == OP_B(inst);

            // If arguments are different, in case they are both constant, we can check if a larger bound was already tested
            if (!sameBoundary && OP_B(inst).kind == IrOpKind::Constant && OP_B(prev).kind == IrOpKind::Constant &&
                unsigned(function.intOp(OP_B(inst))) < unsigned(function.intOp(OP_B(prev))))
                sameBoundary = true;

            if (sameBoundary)
            {
                if (FFlag::DebugLuauAbortingChecks)
                    replace(function, OP_C(inst), build.undef());
                else
                    kill(function, inst);
                return; // Break out from both the loop and the switch
            }

            // TODO: it should be possible to update previous check with a higher bound if current and previous checks are against a constant
        }

        if (int(state.checkArraySizeCache.size()) < FInt::LuauCodeGenReuseSlotLimit)
            state.checkArraySizeCache.push_back(index);
        break;
    }
    case IrCmd::CHECK_SLOT_MATCH:
        for (auto& el : state.checkSlotMatchCache)
        {
            IrInst& prev = function.instructions[el.pointer];

            if (OP_A(prev) == OP_A(inst) && OP_B(prev) == OP_B(inst))
            {
                if (uint8_t* info = state.instTag.find(OP_A(inst).index))
                {
                    if (*info != LUA_TNIL)
                        el.knownToNotBeNil = true;
                }

                if (el.knownToNotBeNil)
                    kill(function, inst);
                else
                    replace(function, block, index, {IrCmd::CHECK_NODE_VALUE, {OP_A(inst), OP_C(inst)}}); // Only a check for 'nil' value is left

                el.knownToNotBeNil = true;
                return; // Break out from both the loop and the switch
            }
        }

        if (int(state.checkSlotMatchCache.size()) < FInt::LuauCodeGenReuseSlotLimit)
            state.checkSlotMatchCache.push_back({index, true});
        break;

    case IrCmd::ADD_VEC:
    case IrCmd::SUB_VEC:
    case IrCmd::MUL_VEC:
    case IrCmd::DIV_VEC:
    case IrCmd::IDIV_VEC:
    case IrCmd::DOT_VEC:
    case IrCmd::MIN_VEC:
    case IrCmd::MAX_VEC:
        if (IrInst* a = function.asInstOp(OP_A(inst)); a && a->cmd == IrCmd::TAG_VECTOR)
            replace(function, OP_A(inst), OP_A(a));

        if (IrInst* b = function.asInstOp(OP_B(inst)); b && b->cmd == IrCmd::TAG_VECTOR)
            replace(function, OP_B(inst), OP_A(b));

        state.substituteOrRecord(inst, index);
        break;

    case IrCmd::UNM_VEC:
    case IrCmd::FLOOR_VEC:
    case IrCmd::CEIL_VEC:
    case IrCmd::ABS_VEC:
        if (IrInst* a = function.asInstOp(OP_A(inst)); a && a->cmd == IrCmd::TAG_VECTOR)
            replace(function, OP_A(inst), OP_A(a));

        state.substituteOrRecord(inst, index);
        break;

    case IrCmd::FLOAT_TO_VEC:
    case IrCmd::TAG_VECTOR:
        state.substituteOrRecord(inst, index);
        break;

    case IrCmd::INVOKE_LIBM:
        state.substituteOrRecord(inst, index);
        break;

    case IrCmd::CHECK_NODE_NO_NEXT:
    case IrCmd::CHECK_NODE_VALUE:
    case IrCmd::BARRIER_TABLE_BACK:
    case IrCmd::RETURN:
    case IrCmd::COVERAGE:
    case IrCmd::SET_SAVEDPC:  // TODO: we may be able to remove some updates to PC
    case IrCmd::CLOSE_UPVALS: // Doesn't change memory that we track
    case IrCmd::CAPTURE:
    case IrCmd::SUBSTITUTE:
    case IrCmd::MARK_USED:
    case IrCmd::MARK_DEAD:
    case IrCmd::ADJUST_STACK_TO_REG: // Changes stack top, but not the values
    case IrCmd::ADJUST_STACK_TO_TOP: // Changes stack top, but not the values
    case IrCmd::CHECK_FASTCALL_RES:  // Changes stack top, but not the values
    case IrCmd::BITAND_INT64:
    case IrCmd::BITXOR_INT64:
    case IrCmd::BITOR_INT64:
    case IrCmd::BITNOT_INT64:
    case IrCmd::BITLSHIFT_INT64:
    case IrCmd::BITRSHIFT_INT64:
    case IrCmd::BITARSHIFT_INT64:
    case IrCmd::BITLROTATE_INT64:
    case IrCmd::BITRROTATE_INT64:
    case IrCmd::BITCOUNTLZ_INT64:
    case IrCmd::BITCOUNTRZ_INT64:
    case IrCmd::BYTESWAP_INT64:
    case IrCmd::BITAND_UINT:
    case IrCmd::BITXOR_UINT:
    case IrCmd::BITOR_UINT:
    case IrCmd::BITNOT_UINT:
    case IrCmd::BITLSHIFT_UINT:
    case IrCmd::BITRSHIFT_UINT:
    case IrCmd::BITARSHIFT_UINT:
    case IrCmd::BITRROTATE_UINT:
    case IrCmd::BITLROTATE_UINT:
    case IrCmd::BITCOUNTLZ_UINT:
    case IrCmd::BITCOUNTRZ_UINT:
    case IrCmd::BYTESWAP_UINT:
    case IrCmd::GET_TYPE:
    case IrCmd::GET_TYPEOF:
    case IrCmd::FINDUPVAL:
        break;

    case IrCmd::DO_ARITH:
        state.invalidate(OP_A(inst));
        state.invalidateUserCall();
        break;
    case IrCmd::DO_LEN:
        state.invalidate(OP_A(inst));
        state.invalidateUserCall(); // TODO: if argument is a string, there will be no user call

        state.saveTag(OP_A(inst), LUA_TNUMBER);
        break;
    case IrCmd::GET_TABLE:
        state.invalidate(OP_A(inst));
        state.invalidateUserCall();
        break;
    case IrCmd::SET_TABLE:
        state.invalidateUserCall();
        break;
    case IrCmd::GET_CACHED_IMPORT:
        state.invalidate(OP_A(inst));

        // Outside of safe environment, environment traversal for an import can execute custom code
        if (!state.inSafeEnv)
            state.invalidateUserCall();
        else
            state.invalidateValuePropagation();
        break;
    case IrCmd::CONCAT:
        state.invalidateRegisterRange(vmRegOp(OP_A(inst)), function.uintOp(OP_B(inst)));
        state.invalidateUserCall(); // TODO: if only strings and numbers are concatenated, there will be no user calls
        break;
    case IrCmd::INTERRUPT:
        // While interrupt can observe state and yield/error, interrupt handlers must never change state
        break;
    case IrCmd::SETLIST:
        if (RegisterInfo* info = state.tryGetRegisterInfo(OP_B(inst)); info && info->knownTableArraySize >= 0)
            replace(function, OP_F(inst), build.constUint(info->knownTableArraySize));

        // TODO: this can be relaxed when x64 emitInstSetList becomes aware of register allocator
        state.invalidateValuePropagation();
        state.invalidateHeapTableData();
        state.invalidateHeapBufferData();
        break;
    case IrCmd::CALL:
        state.invalidateRegistersFrom(vmRegOp(OP_A(inst)));
        state.invalidateUserCall();
        break;
    case IrCmd::FORGLOOP:
        state.invalidateRegistersFrom(vmRegOp(OP_A(inst)) + 2); // Rn and Rn+1 are not modified

        // TODO: this can be relaxed when x64 emitInstForGLoop becomes aware of register allocator
        state.invalidateValuePropagation();
        state.invalidateHeapTableData();
        state.invalidateHeapBufferData();
        break;
    case IrCmd::FORGLOOP_FALLBACK:
        state.invalidateRegistersFrom(vmRegOp(OP_A(inst)) + 2); // Rn and Rn+1 are not modified
        state.invalidateUserCall();
        break;
    case IrCmd::FORGPREP_XNEXT_FALLBACK:
        // This fallback only conditionally throws an exception
        break;

        // Full fallback instructions
    case IrCmd::FALLBACK_GETGLOBAL:
        state.invalidate(OP_B(inst));
        state.invalidateUserCall();
        break;
    case IrCmd::FALLBACK_SETGLOBAL:
        state.invalidateUserCall();
        break;
    case IrCmd::FALLBACK_GETTABLEKS:
        state.invalidate(OP_B(inst));
        state.invalidateUserCall();
        break;
    case IrCmd::FALLBACK_SETTABLEKS:
        state.invalidateUserCall();
        break;
    case IrCmd::FALLBACK_NAMECALL:
        state.invalidate(IrOp{OP_B(inst).kind, vmRegOp(OP_B(inst)) + 0u});
        state.invalidate(IrOp{OP_B(inst).kind, vmRegOp(OP_B(inst)) + 1u});
        state.invalidateUserCall();
        break;
    case IrCmd::FALLBACK_PREPVARARGS:
        break;
    case IrCmd::FALLBACK_GETVARARGS:
        state.invalidateRegisterRange(vmRegOp(OP_B(inst)), function.intOp(OP_C(inst)));
        break;
    case IrCmd::NEWCLOSURE:
        break;
    case IrCmd::FALLBACK_DUPCLOSURE:
        state.invalidate(OP_B(inst));

        if (FFlag::LuauCodegenPreciseDupTableEffect)
        {
            // GC assist inside DUPCLOSURE might modify table data (hash part)
            state.invalidateHeapTableData();
        }
        break;
    case IrCmd::FALLBACK_FORGPREP:
        state.invalidate(IrOp{OP_B(inst).kind, vmRegOp(OP_B(inst)) + 0u});
        state.invalidate(IrOp{OP_B(inst).kind, vmRegOp(OP_B(inst)) + 1u});
        state.invalidate(IrOp{OP_B(inst).kind, vmRegOp(OP_B(inst)) + 2u});
        state.invalidateUserCall();
        break;
    }
}

static void setupBlockEntryState(IrBuilder& build, IrFunction& function, IrBlock& block, ConstPropState& state)
{
    // State starts with knowledge of entry registers, unless it's a block which establishes that knowledge
    if ((block.flags & kBlockFlagEntryArgCheck) != 0)
        return;

    const BytecodeTypeInfo& typeInfo = build.function.bcOriginalTypeInfo;

    for (size_t i = 0; i < typeInfo.argumentTypes.size(); i++)
    {
        uint8_t et = typeInfo.argumentTypes[i];
        uint8_t tag = et & ~LBC_TYPE_OPTIONAL_BIT;

        if (tag == LBC_TYPE_ANY || (et & LBC_TYPE_OPTIONAL_BIT) != 0)
            continue;

        if (function.cfg.written.regs[i])
            continue;

        if (function.cfg.written.varargSeq && i >= function.cfg.written.varargStart)
            continue;

        if (function.cfg.captured.regs[i])
            continue;

        if (std::optional<uint8_t> vmTag = tryGetLuauTagForBcType(tag, /* ignoreOptionalPart */ true))
            state.updateTag(build.vmReg(uint8_t(i)), *vmTag);
    }

    if (FFlag::LuauCodegenPropagateTagsAcrossChains2)
    {
        propagateTagsFromPredecessors(
            function,
            block,
            [&](size_t i)
            {
                return state.regs[i].tag;
            },
            [&](size_t i, uint8_t tag)
            {
                state.updateTag(build.vmReg(uint8_t(i)), tag);
            }
        );
    }
}

static void saveBlockExitState(IrFunction& function, const IrBlock& block, ConstPropState& state)
{
    CODEGEN_ASSERT(FFlag::LuauCodegenPropagateTagsAcrossChains2);

    std::vector<uint8_t> tags;
    tags.reserve(state.maxReg + 1);

    for (int i = 0; i <= state.maxReg; ++i)
        tags.emplace_back(state.regs[i].tag);

    uint32_t blockIdx = function.getBlockIndex(block);
    function.blockExitTags[blockIdx] = std::move(tags);
}

static void constPropInBlock(IrBuilder& build, IrBlock& block, ConstPropState& state)
{
    IrFunction& function = build.function;

    // Block might establish a safe environment right at the start
    if ((block.flags & kBlockFlagSafeEnvCheck) != 0)
        state.inSafeEnv = true;

    for (uint32_t index = block.start; index <= block.finish; index++)
    {
        CODEGEN_ASSERT(index < function.instructions.size());
        IrInst& inst = function.instructions[index];

        applySubstitutions(function, inst);

        foldConstants(build, function, block, index);

        constPropInInst(state, build, function, block, inst, index);

        // Optimizations might have killed the current block
        if (block.kind == IrBlockKind::Dead)
            break;
    }
}

static void constPropInBlockChain(IrBuilder& build, std::vector<uint8_t>& visited, IrBlock* block, ConstPropState& state)
{
    IrFunction& function = build.function;

    state.clear();

    if (FFlag::LuauCodegenSetBlockEntryState3)
        setupBlockEntryState(build, function, *block, state);

    const uint32_t startSortkey = block->sortkey;
    uint32_t chainPos = 0;

    IrBlock* lastBlock = nullptr;

    while (block)
    {
        uint32_t blockIdx = function.getBlockIndex(*block);
        CODEGEN_ASSERT(!visited[blockIdx]);
        visited[blockIdx] = true;

        // If we are still in safe env, block doesn't need to re-establish it
        if (state.inSafeEnv && (block->flags & kBlockFlagSafeEnvCheck) != 0)
            block->flags &= ~kBlockFlagSafeEnvCheck;

        constPropInBlock(build, *block, state);

        // Optimizations might have killed the current block
        if (block->kind == IrBlockKind::Dead)
            break;

        // Blocks in a chain are guaranteed to follow each other
        // We force that by giving all blocks the same sorting key, but consecutive chain keys
        block->sortkey = startSortkey;
        block->chainkey = chainPos++;

        IrInst& termInst = function.instructions[block->finish];

        IrBlock* nextBlock = nullptr;

        // Unconditional jump into a block with a single user (current block) allows us to continue optimization
        // with the information we have gathered so far (unless we have already visited that block earlier)
        if (termInst.cmd == IrCmd::JUMP && OP_A(termInst).kind == IrOpKind::Block)
        {
            IrBlock& target = function.blockOp(OP_A(termInst));
            uint32_t targetIdx = function.getBlockIndex(target);

            if (target.useCount == 1 && !visited[targetIdx] && target.kind != IrBlockKind::Fallback)
            {
                if (getLiveOutValueCount(function, target) != 0)
                    break;

                // Make sure block ordering guarantee is checked at lowering time
                block->expectedNextBlock = function.getBlockIndex(target);

                nextBlock = &target;
            }
        }

        if (FFlag::LuauCodegenPropagateTagsAcrossChains2)
            lastBlock = block;
        block = nextBlock;
    }

    if (FFlag::LuauCodegenPropagateTagsAcrossChains2 && lastBlock)
        saveBlockExitState(function, *lastBlock, state);
}

// Note that blocks in the collected path are marked as visited
static std::vector<uint32_t> collectDirectBlockJumpPath(IrFunction& function, std::vector<uint8_t>& visited, IrBlock* block)
{
    // Path shouldn't be built starting with a block that has 'live out' values.
    // One theoretical way to get it is if we had 'block' jumping unconditionally into a successor that uses values from 'block'
    // * if the successor has only one use, the starting conditions of 'tryCreateLinearBlock' would prevent this
    // * if the successor has multiple uses, it can't have such 'live in' values without phi nodes that we don't have yet
    // Another possibility is to have two paths from 'block' into the target through two intermediate blocks
    // Usually that would mean that we would have a conditional jump at the end of 'block'
    // But using check guards and fallback blocks it becomes a possible setup
    // We avoid this by making sure fallbacks rejoin the other immediate successor of 'block'
    CODEGEN_ASSERT(getLiveOutValueCount(function, *block) == 0);

    std::vector<uint32_t> path;

    while (block)
    {
        IrInst& termInst = function.instructions[block->finish];
        IrBlock* nextBlock = nullptr;

        // A chain is made from internal blocks that were not a part of bytecode CFG
        if (termInst.cmd == IrCmd::JUMP && OP_A(termInst).kind == IrOpKind::Block)
        {
            IrBlock& target = function.blockOp(OP_A(termInst));
            uint32_t targetIdx = function.getBlockIndex(target);

            if (!visited[targetIdx] && target.kind == IrBlockKind::Internal)
            {
                // Additional restriction is that to join a block, it cannot produce values that are used in other blocks
                // And it also can't use values produced in other blocks
                auto [liveIns, liveOuts] = getLiveInOutValueCount(function, target, true);

                if (liveIns == 0 && liveOuts == 0)
                {
                    visited[targetIdx] = true;
                    path.push_back(targetIdx);

                    nextBlock = &target;

                    for (;;)
                    {
                        if (IrBlock* nextInChain = tryGetNextBlockInChain(function, *nextBlock))
                        {
                            uint32_t nextInChainIdx = function.getBlockIndex(*nextInChain);

                            visited[nextInChainIdx] = true;
                            path.push_back(nextInChainIdx);

                            nextBlock = nextInChain;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
        }

        block = nextBlock;
    }

    return path;
}

static void tryCreateLinearBlock(IrBuilder& build, std::vector<uint8_t>& visited, IrBlock& startingBlock, ConstPropState& state)
{
    IrFunction& function = build.function;

    uint32_t blockIdx = function.getBlockIndex(startingBlock);
    CODEGEN_ASSERT(!visited[blockIdx]);
    visited[blockIdx] = true;

    uint32_t termInstIdx = startingBlock.finish;
    IrInst& termInst = function.instructions[termInstIdx];

    // Block has to end with an unconditional jump
    if (termInst.cmd != IrCmd::JUMP)
        return;

    // And it can't be jump to a VM exit or undef
    if (OP_A(termInst).kind != IrOpKind::Block)
        return;

    // And it has to jump to a block with more than one user
    // If there's only one use, it should already be optimized by constPropInBlockChain
    if (function.blockOp(OP_A(termInst)).useCount == 1)
        return;

    uint32_t targetBlockIdx = OP_A(termInst).index;

    // Check if this path is worth it (and it will also mark path blocks as visited)
    std::vector<uint32_t> path = collectDirectBlockJumpPath(function, visited, &startingBlock);

    // If path is too small, we are not going to linearize it
    if (int(path.size()) < FInt::LuauCodeGenMinLinearBlockPath)
        return;

    // Initialize state with the knowledge of our current block
    state.clear();

    if (FFlag::LuauCodegenSetBlockEntryState3 && FFlag::LuauCodegenLinearSetupEntryState3)
        setupBlockEntryState(build, function, startingBlock, state);

    constPropInBlock(build, startingBlock, state);

    if (FFlag::LuauCodegenLinearSetupEntryState3)
    {
        // Verify that target hasn't changed
        if (startingBlock.finish != termInstIdx || OP_A(function.instructions[termInstIdx]).index != targetBlockIdx)
        {
            // If the block changed, it means original constant propagation pass did not reach a fixed point
            return;
        }

        // Check that the start of the linear block path is still held by multiple predecessors
        // We will be replacing blocks later and if use count is 1 it will kill the chain before linearization is complete
        if (function.blocks[targetBlockIdx].useCount == 1)
            return;
    }
    else
    {
        // Verify that target hasn't changed
        if (OP_A(function.instructions[startingBlock.finish]).index != targetBlockIdx)
        {
            CODEGEN_ASSERT(!"Running same optimization pass on the linear chain head block changed the jump target");
            return;
        }
    }

    // Note: using startingBlock after this line is unsafe as the reference may be reallocated by build.block() below
    const uint32_t startingSortKey = startingBlock.sortkey;
    const uint32_t startingChainKey = startingBlock.chainkey;

    // Create new linearized block into which we are going to redirect starting block jump
    IrOp newBlock = build.block(IrBlockKind::Linearized);
    visited.push_back(false);

    build.beginBlock(newBlock);

    // By default, blocks are ordered according to start instruction; we alter sort order to make sure linearized block is placed right after the
    // starting block
    function.blocks[newBlock.index].sortkey = startingSortKey;
    function.blocks[newBlock.index].chainkey = startingChainKey + 1;

    // Make sure block ordering guarantee is checked at lowering time
    function.blocks[blockIdx].expectedNextBlock = newBlock.index;

    replace(function, OP_A(termInst), newBlock);

    // Clone the collected path into our fresh block
    build.clone(path, /* removeCurrentTerminator */ true);

    // If all live in/out data is defined aside from the new block, generate it
    // Note that liveness information is not strictly correct after optimization passes and may need to be recomputed before next passes
    // The information generated here is consistent with current state that could be outdated, but still useful in IR inspection
    if (function.cfg.in.size() == newBlock.index)
    {
        CODEGEN_ASSERT(function.cfg.in.size() == function.cfg.out.size());
        CODEGEN_ASSERT(function.cfg.in.size() == function.cfg.def.size());

        // Live in is the same as the input of the original first block
        function.cfg.in.push_back(function.cfg.in[path.front()]);

        // Live out is the same as the result of the original last block
        function.cfg.out.push_back(function.cfg.out[path.back()]);

        // Defs are tricky, registers are joined together, but variadic sequences can be consumed inside the block
        function.cfg.def.push_back({});
        RegisterSet& def = function.cfg.def.back();

        for (uint32_t pathBlockIdx : path)
        {
            const RegisterSet& pathDef = function.cfg.def[pathBlockIdx];

            def.regs |= pathDef.regs;

            // Taking only the last defined variadic sequence if it's not consumed before before the end
            if (pathDef.varargSeq && function.cfg.out.back().varargSeq)
            {
                def.varargSeq = true;
                def.varargStart = pathDef.varargStart;
            }
        }

        // Update predecessors
        function.cfg.predecessorsOffsets.push_back(uint32_t(function.cfg.predecessors.size()));
        function.cfg.predecessors.push_back(blockIdx);

        // Updating successors will require visiting the instructions again and we don't have a current use for linearized block successor list
    }

    // Optimize our linear block
    IrBlock& linearBlock = function.blockOp(newBlock);
    constPropInBlock(build, linearBlock, state);
}

void constPropInBlockChains(IrBuilder& build)
{
    IrFunction& function = build.function;

    ConstPropState state{build, function};

    std::vector<uint8_t> visited(function.blocks.size(), false);

    if (FFlag::LuauCodegenPropagateTagsAcrossChains2)
        function.blockExitTags.resize(function.blocks.size());

    for (IrBlock& block : function.blocks)
    {
        if (block.kind == IrBlockKind::Fallback || block.kind == IrBlockKind::Dead)
            continue;

        if (visited[function.getBlockIndex(block)])
            continue;

        constPropInBlockChain(build, visited, &block, state);
    }
}

void createLinearBlocks(IrBuilder& build)
{
    // Go through internal block chains and outline them into a single new block.
    // Outlining will be able to linearize the execution, even if there was a jump to a block with multiple users,
    // new 'block' will only be reachable from a single one and all gathered information can be preserved.
    IrFunction& function = build.function;

    ConstPropState state{build, function};

    std::vector<uint8_t> visited(function.blocks.size(), false);

    // This loop can create new 'linear' blocks, so index-based loop has to be used (and it intentionally won't reach those new blocks)
    size_t originalBlockCount = function.blocks.size();
    for (size_t i = 0; i < originalBlockCount; i++)
    {
        IrBlock& block = function.blocks[i];

        if (block.kind == IrBlockKind::Fallback || block.kind == IrBlockKind::Dead)
            continue;

        if (visited[function.getBlockIndex(block)])
            continue;

        tryCreateLinearBlock(build, visited, block, state);
    }
}

} // namespace CodeGen
} // namespace Luau
