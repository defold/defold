// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Common.h"
#include "Luau/DenseHash.h"
#include "Luau/Label.h"
#include "Luau/ConditionX64.h"
#include "Luau/OperandX64.h"
#include "Luau/RegisterX64.h"

#include <string>
#include <vector>

namespace Luau
{
namespace CodeGen
{
namespace X64
{

enum FeaturesX64
{
    Feature_FMA3 = 1 << 0,
    Feature_AVX = 1 << 1
};

enum class RoundingModeX64
{
    RoundToNearestEven = 0b00,
    RoundToNegativeInfinity = 0b01,
    RoundToPositiveInfinity = 0b10,
    RoundToZero = 0b11,
};

enum class AlignmentDataX64
{
    Nop,
    Int3,
    Ud2, // int3 will be used as a fall-back if it doesn't fit
};

enum class ABIX64
{
    Windows,
    SystemV,
};

class AssemblyBuilderX64
{
public:
    explicit AssemblyBuilderX64(bool logText, ABIX64 abi, unsigned int features = 0);
    explicit AssemblyBuilderX64(bool logText, unsigned int features = 0);
    ~AssemblyBuilderX64();

    // Base two operand instructions with 9 opcode selection
    void add(OperandX64 lhs, OperandX64 rhs);
    void sub(OperandX64 lhs, OperandX64 rhs);
    void cmp(OperandX64 lhs, OperandX64 rhs);
    void and_(OperandX64 lhs, OperandX64 rhs);
    void or_(OperandX64 lhs, OperandX64 rhs);
    void xor_(OperandX64 lhs, OperandX64 rhs);

    // Binary shift instructions with special rhs handling
    void sal(OperandX64 lhs, OperandX64 rhs);
    void sar(OperandX64 lhs, OperandX64 rhs);
    void shl(OperandX64 lhs, OperandX64 rhs);
    void shr(OperandX64 lhs, OperandX64 rhs);
    void rol(OperandX64 lhs, OperandX64 rhs);
    void ror(OperandX64 lhs, OperandX64 rhs);

    // Two operand mov instruction has additional specialized encodings
    void mov(OperandX64 lhs, OperandX64 rhs);
    void mov64(RegisterX64 lhs, int64_t imm);
    void movsx(RegisterX64 lhs, OperandX64 rhs);
    void movzx(RegisterX64 lhs, OperandX64 rhs);

    // Base one operand instruction with 2 opcode selection
    void div(OperandX64 op);
    void idiv(OperandX64 op);
    void mul(OperandX64 op);
    void imul(OperandX64 op);
    void neg(OperandX64 op);
    void not_(OperandX64 op);
    void dec(OperandX64 op);
    void inc(OperandX64 op);

    // Additional forms of imul
    void imul(OperandX64 lhs, OperandX64 rhs);
    void imul(OperandX64 dst, OperandX64 lhs, int32_t rhs);

    void test(OperandX64 lhs, OperandX64 rhs);
    void lea(OperandX64 lhs, OperandX64 rhs);
    void setcc(ConditionX64 cond, OperandX64 op);
    void cmov(ConditionX64 cond, RegisterX64 lhs, OperandX64 rhs);

    void push(OperandX64 op);
    void pop(OperandX64 op);
    void ret();

    // Control flow
    void jcc(ConditionX64 cond, Label& label);
    void jmp(Label& label);
    void jmp(OperandX64 op);

    void call(Label& label);
    void call(OperandX64 op);

    void lea(RegisterX64 lhs, Label& label);

    void int3();
    void ud2();

    void cqo();
    void cdq();

    void bsr(RegisterX64 dst, OperandX64 src);
    void bsf(RegisterX64 dst, OperandX64 src);
    void bswap(RegisterX64 dst);

    // Code alignment
    void nop(uint32_t length = 1);
    void align(uint32_t alignment, AlignmentDataX64 data = AlignmentDataX64::Nop);

    // AVX
    void vaddpd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vaddps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vaddsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vaddss(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vsubsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vsubss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vsubps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmulsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmulss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmulps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vdivsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vdivss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vdivps(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vandps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vandpd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vandnpd(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vxorps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vxorpd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vorps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vorpd(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vucomisd(OperandX64 src1, OperandX64 src2);
    void vucomiss(OperandX64 src1, OperandX64 src2);

    void vcvttsd2si(OperandX64 dst, OperandX64 src);
    void vcvtsi2sd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vcvtsi2ss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vcvtsd2ss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vcvtss2sd(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vroundsd(OperandX64 dst, OperandX64 src1, OperandX64 src2, RoundingModeX64 roundingMode); // inexact
    void vroundss(OperandX64 dst, OperandX64 src1, OperandX64 src2, RoundingModeX64 roundingMode); // inexact
    void vroundps(OperandX64 dst, OperandX64 src, RoundingModeX64 roundingMode);                   // inexact

    void vsqrtpd(OperandX64 dst, OperandX64 src);
    void vsqrtps(OperandX64 dst, OperandX64 src);
    void vsqrtsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vsqrtss(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vmovsd(OperandX64 dst, OperandX64 src);
    void vmovsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmovss(OperandX64 dst, OperandX64 src);
    void vmovss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmovapd(OperandX64 dst, OperandX64 src);
    void vmovaps(OperandX64 dst, OperandX64 src);
    void vmovupd(OperandX64 dst, OperandX64 src);
    void vmovups(OperandX64 dst, OperandX64 src);
    void vmovq(OperandX64 lhs, OperandX64 rhs);

    void vmaxps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmaxsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vmaxss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vminps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vminsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vminss(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vcmpeqsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vcmpltsd(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vcmpltss(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vcmpeqps(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    void vblendvps(RegisterX64 dst, RegisterX64 src1, OperandX64 src2, RegisterX64 mask);
    void vblendvpd(RegisterX64 dst, RegisterX64 src1, OperandX64 src2, RegisterX64 mask);

    void vpshufps(RegisterX64 dst, RegisterX64 src1, OperandX64 src2, uint8_t shuffle);
    void vpinsrd(RegisterX64 dst, RegisterX64 src1, OperandX64 src2, uint8_t offset);
    void vpextrd(RegisterX64 dst, RegisterX64 src, uint8_t offset);

    void vdpps(OperandX64 dst, OperandX64 src1, OperandX64 src2, uint8_t mask);
    void vfmadd213ps(OperandX64 dst, OperandX64 src1, OperandX64 src2);
    void vfmadd213pd(OperandX64 dst, OperandX64 src1, OperandX64 src2);

    // Run final checks
    bool finalize();

    // Places a label at current location and returns it
    Label setLabel();

    // Assigns label position to the current location
    void setLabel(Label& label);

    // Extracts code offset (in bytes) from label
    uint32_t getLabelOffset(const Label& label)
    {
        CODEGEN_ASSERT(label.location != ~0u);
        return label.location;
    }

    // Constant allocation (uses rip-relative addressing)
    OperandX64 i32(int32_t value);
    OperandX64 i64(int64_t value);
    OperandX64 f32(float value);
    OperandX64 f64(double value);
    OperandX64 u32x4(uint32_t x, uint32_t y, uint32_t z, uint32_t w);
    OperandX64 f32x4(float x, float y, float z, float w);
    OperandX64 f64x2(double x, double y);
    OperandX64 bytes(const void* ptr, size_t size, size_t align = 8);

    void logAppend(const char* fmt, ...) LUAU_PRINTF_ATTR(2, 3);

    // Code size is measured in 'code' array units - uint8_t on x64 and uint32_t on arm64
    uint32_t getCodeSize() const;

    unsigned getInstructionCount() const;

    // Resulting data and code that need to be copied over one after the other
    // The *end* of 'data' has to be aligned to 16 bytes, this will also align 'code'
    std::vector<uint8_t> data;
    std::vector<uint8_t> code;

    std::string text;

    const bool logText = false;

    const ABIX64 abi;

    const unsigned int features = 0;

private:
    // Instruction archetypes
    void placeBinary(
        const char* name,
        OperandX64 lhs,
        OperandX64 rhs,
        uint8_t codeimm8,
        uint8_t codeimm,
        uint8_t codeimmImm8,
        uint8_t code8rev,
        uint8_t coderev,
        uint8_t code8,
        uint8_t code,
        uint8_t opreg
    );
    void placeBinaryRegMemAndImm(OperandX64 lhs, OperandX64 rhs, uint8_t code8, uint8_t code, uint8_t codeImm8, uint8_t opreg);
    void placeBinaryRegAndRegMem(OperandX64 lhs, OperandX64 rhs, uint8_t code8, uint8_t code);
    void placeBinaryRegMemAndReg(OperandX64 lhs, OperandX64 rhs, uint8_t code8, uint8_t code);

    void placeUnaryModRegMem(const char* name, OperandX64 op, uint8_t code8, uint8_t code, uint8_t opreg);

    void placeShift(const char* name, OperandX64 lhs, OperandX64 rhs, uint8_t opreg);

    void placeJcc(const char* name, Label& label, uint8_t cc);

    void placeAvx(const char* name, OperandX64 dst, OperandX64 src, uint8_t code, bool setW, uint8_t mode, uint8_t prefix);
    void placeAvx(const char* name, OperandX64 dst, OperandX64 src, uint8_t code, uint8_t coderev, bool setW, uint8_t mode, uint8_t prefix);
    void placeAvx(const char* name, OperandX64 dst, OperandX64 src1, OperandX64 src2, uint8_t code, bool setW, uint8_t mode, uint8_t prefix);
    void placeAvx(
        const char* name,
        OperandX64 dst,
        OperandX64 src1,
        OperandX64 src2,
        uint8_t imm8,
        uint8_t code,
        bool setW,
        uint8_t mode,
        uint8_t prefix
    );

    // Instruction components
    void placeRegAndModRegMem(OperandX64 lhs, OperandX64 rhs, int32_t extraCodeBytes = 0);
    void placeModRegMem(OperandX64 rhs, uint8_t regop, int32_t extraCodeBytes = 0);
    void placeRex(RegisterX64 op);
    void placeRex(OperandX64 op);
    void placeRexNoW(OperandX64 op);
    void placeRex(RegisterX64 lhs, OperandX64 rhs);
    void placeVex(OperandX64 dst, OperandX64 src1, OperandX64 src2, bool setW, uint8_t mode, uint8_t prefix);
    void placeImm8Or32(int32_t imm);
    void placeImm8(int32_t imm);
    void placeImm16(int16_t imm);
    void placeImm32(int32_t imm);
    void placeImm64(int64_t imm);
    void placeLabel(Label& label);
    void place(uint8_t byte);

    void commit();
    LUAU_NOINLINE void extend();

    // Data
    size_t allocateData(size_t size, size_t align);

    // Logging of assembly in text form (Intel asm with VS disassembly formatting)
    LUAU_NOINLINE void log(const char* opcode);
    LUAU_NOINLINE void log(const char* opcode, OperandX64 op);
    LUAU_NOINLINE void log(const char* opcode, OperandX64 op1, OperandX64 op2);
    LUAU_NOINLINE void log(const char* opcode, OperandX64 op1, OperandX64 op2, OperandX64 op3);
    LUAU_NOINLINE void log(const char* opcode, OperandX64 op1, OperandX64 op2, OperandX64 op3, OperandX64 op4);
    LUAU_NOINLINE void log(Label label);
    LUAU_NOINLINE void log(const char* opcode, Label label);
    LUAU_NOINLINE void log(const char* opcode, RegisterX64 reg, Label label);
    void log(OperandX64 op);

    const char* getSizeName(SizeX64 size) const;
    const char* getRegisterName(RegisterX64 reg) const;

    uint32_t nextLabel = 1;
    std::vector<Label> pendingLabels;
    std::vector<uint32_t> labelLocations;

    DenseHashMap<uint32_t, int32_t> constCache32;
    DenseHashMap<uint64_t, int32_t> constCache64;

    bool finalized = false;

    size_t dataPos = 0;

    uint8_t* codePos = nullptr;
    uint8_t* codeEnd = nullptr;

    unsigned instructionCount = 0;
};

} // namespace X64
} // namespace CodeGen
} // namespace Luau
