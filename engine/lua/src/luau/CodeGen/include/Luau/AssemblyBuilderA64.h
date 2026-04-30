// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/RegisterA64.h"
#include "Luau/AddressA64.h"
#include "Luau/ConditionA64.h"
#include "Luau/Label.h"

#include <string>
#include <vector>

namespace Luau
{
namespace CodeGen
{
namespace A64
{

enum FeaturesA64
{
    Feature_JSCVT = 1 << 0,
    Feature_AdvSIMD = 1 << 1
};

class AssemblyBuilderA64
{
public:
    explicit AssemblyBuilderA64(bool logText, unsigned int features = 0);
    ~AssemblyBuilderA64();

    // Moves
    void mov(RegisterA64 dst, RegisterA64 src);
    void mov(RegisterA64 dst, int src); // macro

    // Moves of 32-bit immediates get decomposed into one or more of these
    void movz(RegisterA64 dst, uint16_t src, int shift = 0);
    void movn(RegisterA64 dst, uint16_t src, int shift = 0);
    void movk(RegisterA64 dst, uint16_t src, int shift = 0);

    // Arithmetics
    void add(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void add(RegisterA64 dst, RegisterA64 src1, uint16_t src2);
    void sub(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void sub(RegisterA64 dst, RegisterA64 src1, uint16_t src2);
    void neg(RegisterA64 dst, RegisterA64 src);
    void mul(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void msub(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, RegisterA64 src3);
    void sdiv(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void udiv(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    // predicate: dst is the result of an sdiv/udiv (quotient); src1 is the dividend, src2 is the divisor; dst != src1
    void rem(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);

    // Prevent implicit conversions from happening
    template<typename T>
    void add(RegisterA64 dst, RegisterA64 src1, T src2) = delete;
    template<typename T>
    void sub(RegisterA64 dst, RegisterA64 src1, T src2) = delete;

    // Comparisons
    // Note: some arithmetic instructions also have versions that update flags (ADDS etc) but we aren't using them atm
    void cmp(RegisterA64 src1, RegisterA64 src2);
    void cmp(RegisterA64 src1, uint16_t src2);

    template<typename T>
    void cmp(RegisterA64 src1, T src2) = delete; // Prevent implicit conversions from happening

    void ccmp(RegisterA64 src1, RegisterA64 src2, ConditionA64 cond, uint8_t nzcv);
    void ccmn(RegisterA64 src1, RegisterA64 src2, ConditionA64 cond, uint8_t nzcv);
    void ccmn(RegisterA64 src1, uint8_t src2, ConditionA64 cond, uint8_t nzcv);
    void cmn(RegisterA64 src1, uint16_t src2);

    void csel(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, ConditionA64 cond);
    void cset(RegisterA64 dst, ConditionA64 cond);

    // Bitwise
    void and_(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void orr(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void eor(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void bic(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void tst(RegisterA64 src1, RegisterA64 src2, int shift = 0);
    void mvn_(RegisterA64 dst, RegisterA64 src);

    // Bitwise with immediate
    // Note: immediate must have a single contiguous sequence of 1 bits set of length 1..31
    void and_(RegisterA64 dst, RegisterA64 src1, uint32_t src2);
    void orr(RegisterA64 dst, RegisterA64 src1, uint32_t src2);
    void eor(RegisterA64 dst, RegisterA64 src1, uint32_t src2);
    void tst(RegisterA64 src1, uint32_t src2);

    // Shifts
    void lsl(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void lsr(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void asr(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void ror(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void clz(RegisterA64 dst, RegisterA64 src);
    void rbit(RegisterA64 dst, RegisterA64 src);
    void rev(RegisterA64 dst, RegisterA64 src);

    // Shifts with immediates
    // Note: immediate value must be in [0, 31] or [0, 63] range based on register type
    void lsl(RegisterA64 dst, RegisterA64 src1, uint8_t src2);
    void lsr(RegisterA64 dst, RegisterA64 src1, uint8_t src2);
    void asr(RegisterA64 dst, RegisterA64 src1, uint8_t src2);
    void ror(RegisterA64 dst, RegisterA64 src1, uint8_t src2);

    // Bitfields
    void ubfiz(RegisterA64 dst, RegisterA64 src, uint8_t f, uint8_t w);
    void ubfx(RegisterA64 dst, RegisterA64 src, uint8_t f, uint8_t w);
    void sbfiz(RegisterA64 dst, RegisterA64 src, uint8_t f, uint8_t w);
    void sbfx(RegisterA64 dst, RegisterA64 src, uint8_t f, uint8_t w);

    // Load
    // Note: paired loads are currently omitted for simplicity
    void ldr(RegisterA64 dst, AddressA64 src);
    void ldrb(RegisterA64 dst, AddressA64 src);
    void ldrh(RegisterA64 dst, AddressA64 src);
    void ldrsb(RegisterA64 dst, AddressA64 src);
    void ldrsh(RegisterA64 dst, AddressA64 src);
    void ldrsw(RegisterA64 dst, AddressA64 src);
    void ldp(RegisterA64 dst1, RegisterA64 dst2, AddressA64 src);

    // Store
    void str(RegisterA64 src, AddressA64 dst);
    void strb(RegisterA64 src, AddressA64 dst);
    void strh(RegisterA64 src, AddressA64 dst);
    void stp(RegisterA64 src1, RegisterA64 src2, AddressA64 dst);

    // Control flow
    void b(Label& label);
    void bl(Label& label);
    void br(RegisterA64 src);
    void blr(RegisterA64 src);
    void ret();

    // Conditional control flow
    void b(ConditionA64 cond, Label& label);
    void cbz(RegisterA64 src, Label& label);
    void cbnz(RegisterA64 src, Label& label);
    void tbz(RegisterA64 src, uint8_t bit, Label& label);
    void tbnz(RegisterA64 src, uint8_t bit, Label& label);

    // Address of embedded data
    void adr(RegisterA64 dst, const void* ptr, size_t size);
    void adr(RegisterA64 dst, uint64_t value);
    void adr(RegisterA64 dst, float value);
    void adr(RegisterA64 dst, double value);

    template<typename T>
    void adr(RegisterA64 dst, T value) = delete; // Prevent implicit conversions from happening

    // Address of code (label)
    void adr(RegisterA64 dst, Label& label);

    // Floating-point scalar/vector moves
    // Note: constant must be compatible with immediate floating point moves (see isFmovSupportedFp64/isFmovSupportedFp32)
    void fmov(RegisterA64 dst, RegisterA64 src);
    void fmov(RegisterA64 dst, double src);
    void fmov(RegisterA64 dst, float src);

    template<typename T>
    void fmov(RegisterA64 dst, T src) = delete; // Prevent implicit conversions from happening

    // Floating-point scalar/vector math
    void fabs(RegisterA64 dst, RegisterA64 src);
    void fadd(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void fdiv(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void fmul(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void fneg(RegisterA64 dst, RegisterA64 src);
    void fsqrt(RegisterA64 dst, RegisterA64 src);
    void fsub(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void faddp(RegisterA64 dst, RegisterA64 src);
    void fmla(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);

    // Vector component manipulation
    void ins_4s(RegisterA64 dst, RegisterA64 src, uint8_t index);
    void ins_4s(RegisterA64 dst, uint8_t dstIndex, RegisterA64 src, uint8_t srcIndex);
    void dup_4s(RegisterA64 dst, RegisterA64 src, uint8_t index);
    void umov_4s(RegisterA64 dst, RegisterA64 src, uint8_t index);

    void fcmeq_4s(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void fcmgt_4s(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2);
    void bit(RegisterA64 dst, RegisterA64 src, RegisterA64 mask);
    void bif(RegisterA64 dst, RegisterA64 src, RegisterA64 mask);

    // Floating-point rounding and conversions
    void frinta(RegisterA64 dst, RegisterA64 src);
    void frintm(RegisterA64 dst, RegisterA64 src);
    void frintp(RegisterA64 dst, RegisterA64 src);
    void fcvt(RegisterA64 dst, RegisterA64 src);
    void fcvtzs(RegisterA64 dst, RegisterA64 src);
    void fcvtzu(RegisterA64 dst, RegisterA64 src);
    void scvtf(RegisterA64 dst, RegisterA64 src);
    void ucvtf(RegisterA64 dst, RegisterA64 src);

    // Floating-point conversion to integer using JS rules (wrap around 2^32) and set Z flag
    // note: this is part of ARM8.3 (JSCVT feature); support of this instruction needs to be checked at runtime
    void fjcvtzs(RegisterA64 dst, RegisterA64 src);

    // Floating-point comparisons
    void fcmp(RegisterA64 src1, RegisterA64 src2);
    void fcmpz(RegisterA64 src);
    void fcsel(RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, ConditionA64 cond);

    void udf();

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
        return label.location * 4;
    }

    void logAppend(const char* fmt, ...) LUAU_PRINTF_ATTR(2, 3);

    // Code size is measured in 'code' array units - uint8_t on x64 and uint32_t on arm64
    uint32_t getCodeSize() const;

    unsigned getInstructionCount() const;

    // Resulting data and code that need to be copied over one after the other
    // The *end* of 'data' has to be aligned to 16 bytes, this will also align 'code'
    std::vector<uint8_t> data;
    std::vector<uint32_t> code;

    std::string text;

    const bool logText = false;
    const unsigned int features = 0;

    // Maximum immediate argument to functions like add/sub/cmp
    static constexpr size_t kMaxImmediate = (1 << 12) - 1;

    // Check if immediate mode mask is supported for bitwise operations (and/or/xor)
    static bool isMaskSupported(uint32_t mask);

    // Check if fmov can be used to synthesize a constant
    static bool isFmovSupportedFp64(double value);
    static bool isFmovSupportedFp32(float value);

private:
    // Instruction archetypes
    void place0(const char* name, uint32_t word);
    void placeSR3(const char* name, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, uint8_t op, int shift = 0, int N = 0);
    void placeSR2(const char* name, RegisterA64 dst, RegisterA64 src, uint8_t op, uint8_t op2 = 0);
    void placeR3(const char* name, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, uint8_t op, uint8_t op2);
    void placeR1(const char* name, RegisterA64 dst, RegisterA64 src, uint32_t op);
    void placeI12(const char* name, RegisterA64 dst, RegisterA64 src1, int src2, uint8_t op);
    void placeI16(const char* name, RegisterA64 dst, int src, uint8_t op, int shift = 0);
    void placeA(const char* name, RegisterA64 dst, AddressA64 src, uint16_t opsize, int sizelog);
    void placeB(const char* name, Label& label, uint8_t op);
    void placeBC(const char* name, Label& label, uint8_t op, uint8_t cond);
    void placeBCR(const char* name, Label& label, uint8_t op, RegisterA64 cond);
    void placeBR(const char* name, RegisterA64 src, uint32_t op);
    void placeBTR(const char* name, Label& label, uint8_t op, RegisterA64 cond, uint8_t bit);
    void placeADR(const char* name, RegisterA64 src, uint8_t op);
    void placeADR(const char* name, RegisterA64 src, uint8_t op, Label& label);
    void placeP(const char* name, RegisterA64 dst1, RegisterA64 dst2, AddressA64 src, uint8_t op, uint8_t opc, int sizelog);
    void placeCS(const char* name, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, ConditionA64 cond, uint8_t op, uint8_t opc, int invert = 0);
    void placeFCMP(const char* name, RegisterA64 src1, RegisterA64 src2, uint8_t op, uint8_t opc);
    void placeFMOV(const char* name, RegisterA64 dst, double src, uint32_t op);
    void placeBM(const char* name, RegisterA64 dst, RegisterA64 src1, uint32_t src2, uint8_t op);
    void placeBFM(const char* name, RegisterA64 dst, RegisterA64 src1, int src2, uint8_t op, int immr, int imms);
    void placeER(const char* name, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, uint8_t op, int shift);
    void placeVR(const char* name, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, uint16_t op, uint8_t op2);

    void place(uint32_t word);

    struct Patch
    {
        enum Kind
        {
            Imm26,
            Imm19,
            Imm14,
        };

        Kind kind : 2;
        uint32_t label : 30;
        uint32_t location;
    };

    void patchLabel(Label& label, Patch::Kind kind);
    void patchOffset(uint32_t location, int value, Patch::Kind kind);

    void commit();
    LUAU_NOINLINE void extend();

    // Data
    size_t allocateData(size_t size, size_t align);

    // Logging of assembly in text form
    LUAU_NOINLINE void log(const char* opcode);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, int shift = 0);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, RegisterA64 src1, int src2);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, RegisterA64 src);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, int src, int shift = 0);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, double src);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, AddressA64 src);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst1, RegisterA64 dst2, AddressA64 src);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 src, Label label, int imm = -1);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 src);
    LUAU_NOINLINE void log(const char* opcode, Label label);
    LUAU_NOINLINE void log(const char* opcode, RegisterA64 dst, RegisterA64 src1, RegisterA64 src2, ConditionA64 cond);
    LUAU_NOINLINE void log(Label label);
    LUAU_NOINLINE void log(RegisterA64 reg);
    LUAU_NOINLINE void log(AddressA64 addr);

    uint32_t nextLabel = 1;
    std::vector<Patch> pendingLabels;
    std::vector<uint32_t> labelLocations;

    bool finalized = false;
    bool overflowed = false;

    size_t dataPos = 0;

    uint32_t* codePos = nullptr;
    uint32_t* codeEnd = nullptr;
};

} // namespace A64
} // namespace CodeGen
} // namespace Luau
