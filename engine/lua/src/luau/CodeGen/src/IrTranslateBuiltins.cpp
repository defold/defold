// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "IrTranslateBuiltins.h"

#include "Luau/Bytecode.h"
#include "Luau/IrBuilder.h"

#include "Luau/IrData.h"
#include "lstate.h"

#include <math.h>

LUAU_FASTFLAG(LuauCodegenBufferRangeMerge4)
LUAU_FASTFLAGVARIABLE(LuauCodegenBufNoDefTag)
LUAU_FASTFLAG(LuauCodegenInteger2)

// TODO: when nresults is less than our actual result count, we can skip computing/writing unused results

static const int kMinMaxUnrolledParams = 5;
static const int kBit32BinaryOpUnrolledParams = 5;

namespace Luau
{
namespace CodeGen
{

static bool isCompatibleConstant(IrBuilder& build, IrOp arg, IrConstKind expected)
{
    return arg.kind != IrOpKind::Constant || build.function.constOp(arg).kind == expected;
}

static void builtinCheckDouble(IrBuilder& build, IrOp arg, int pcpos)
{
    if (arg.kind == IrOpKind::Constant)
        CODEGEN_ASSERT(build.function.constOp(arg).kind == IrConstKind::Double);
    else
        build.loadAndCheckTag(arg, LUA_TNUMBER, build.vmExit(pcpos));
}

static IrOp builtinLoadDouble(IrBuilder& build, IrOp arg)
{
    if (arg.kind == IrOpKind::Constant)
        return arg;

    return build.inst(IrCmd::LOAD_DOUBLE, arg);
}

static void builtinCheckInt64(IrBuilder& build, IrOp arg, int pcpos)
{
    if (arg.kind == IrOpKind::Constant)
        CODEGEN_ASSERT(build.function.constOp(arg).kind == IrConstKind::Int64);
    else
        build.loadAndCheckTag(arg, LUA_TINTEGER, build.vmExit(pcpos));
}

static IrOp builtinLoadInt64(IrBuilder& build, IrOp arg)
{
    if (arg.kind == IrOpKind::Constant)
        return arg;

    return build.inst(IrCmd::LOAD_INT64, arg);
}

// Wrapper code for all builtins with a fixed signature and manual assembly lowering of the body

static BuiltinImplResult translateBuiltinNumberToNumberLibm(
    IrBuilder& build,
    LuauBuiltinFunction bfid,
    int nparams,
    int ra,
    int arg,
    int nresults,
    int pcpos
)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    IrOp va = builtinLoadDouble(build, build.vmReg(arg));

    IrOp res = build.inst(IrCmd::INVOKE_LIBM, build.constUint(bfid), va);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), res);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltin2NumberToNumberLibm(
    IrBuilder& build,
    LuauBuiltinFunction bfid,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    int nresults,
    int pcpos
)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp vb = builtinLoadDouble(build, args);

    if (bfid == LBF_MATH_LDEXP)
        vb = build.inst(IrCmd::NUM_TO_INT, vb);

    IrOp res = build.inst(IrCmd::INVOKE_LIBM, build.constUint(bfid), va, vb);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), res);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

// (number, ...) -> (number, number)
static BuiltinImplResult translateBuiltinNumberTo2Number(
    IrBuilder& build,
    LuauBuiltinFunction bfid,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    int nresults,
    int pcpos
)
{
    if (nparams < 1 || nresults > 2)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    build.inst(IrCmd::FASTCALL, build.constUint(bfid), build.vmReg(ra), build.vmReg(arg), build.constInt(nresults == 1 ? 1 : 2));

    return {BuiltinImplType::Full, 2};
}

static BuiltinImplResult translateBuiltinAssert(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults != 0)
        return {BuiltinImplType::None, -1};

    IrOp tag = build.inst(IrCmd::LOAD_TAG, build.vmReg(arg));

    // We don't know if it's really a boolean at this point, but we will only check this value if it is
    IrOp value = build.inst(IrCmd::LOAD_INT, build.vmReg(arg));

    build.inst(IrCmd::CHECK_TRUTHY, tag, value, build.vmExit(pcpos));

    return {BuiltinImplType::UsesFallback, 0};
}

static BuiltinImplResult translateBuiltinMathDegRad(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    const double rpd = (3.14159265358979323846 / 180.0);

    IrOp varg = builtinLoadDouble(build, build.vmReg(arg));
    IrOp value = build.inst(cmd, varg, build.constDouble(rpd));
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinMathLog(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    int libmId = LBF_MATH_LOG;
    std::optional<double> denom;

    if (nparams != 1)
    {
        std::optional<double> y = build.function.asDoubleOp(args);

        if (!y)
            return {BuiltinImplType::None, -1};

        if (*y == 2.0)
            libmId = LBF_IR_MATH_LOG2;
        else if (*y == 10.0)
            libmId = LBF_MATH_LOG10;
        else
            denom = log(*y);
    }

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));

    IrOp res = build.inst(IrCmd::INVOKE_LIBM, build.constUint(libmId), va);

    if (denom)
        res = build.inst(IrCmd::DIV_NUM, res, build.constDouble(*denom));

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), res);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinMathMinMax(
    IrBuilder& build,
    IrCmd cmd,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    if (nparams < 2 || nparams > kMinMaxUnrolledParams || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);

    if (nparams >= 3)
        builtinCheckDouble(build, arg3, pcpos);

    for (int i = 4; i <= nparams; ++i)
        builtinCheckDouble(build, build.vmReg(vmRegOp(args) + (i - 2)), pcpos);

    IrOp varg1 = builtinLoadDouble(build, build.vmReg(arg));
    IrOp varg2 = builtinLoadDouble(build, args);

    IrOp res = build.inst(cmd, varg2, varg1); // Swapped arguments are required for consistency with VM builtins

    if (nparams >= 3)
    {
        IrOp arg = builtinLoadDouble(build, arg3);
        res = build.inst(cmd, arg, res);
    }

    for (int i = 4; i <= nparams; ++i)
    {
        IrOp arg = builtinLoadDouble(build, build.vmReg(vmRegOp(args) + (i - 2)));
        res = build.inst(cmd, arg, res);
    }

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), res);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinMathClamp(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    IrOp fallback,
    int pcpos
)
{
    if (nparams < 3 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp block = build.block(IrBlockKind::Internal);

    CODEGEN_ASSERT(args.kind == IrOpKind::VmReg);

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);
    builtinCheckDouble(build, arg3, pcpos);

    IrOp min = builtinLoadDouble(build, args);
    IrOp max = builtinLoadDouble(build, arg3);

    build.inst(IrCmd::JUMP_CMP_NUM, min, max, build.cond(IrCondition::NotLessEqual), fallback, block);
    build.beginBlock(block);

    IrOp v = builtinLoadDouble(build, build.vmReg(arg));
    IrOp r = build.inst(IrCmd::MAX_NUM, min, v);
    IrOp clamped = build.inst(IrCmd::MIN_NUM, max, r);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), clamped);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::UsesFallback, 1};
}

static BuiltinImplResult translateBuiltinVectorLerp(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    if (nparams < 3 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp arg1 = build.vmReg(arg);
    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));
    build.loadAndCheckTag(args, LUA_TVECTOR, build.vmExit(pcpos));
    builtinCheckDouble(build, arg3, pcpos);

    IrOp a = build.inst(IrCmd::LOAD_TVALUE, arg1);
    IrOp b = build.inst(IrCmd::LOAD_TVALUE, args);
    IrOp t = builtinLoadDouble(build, arg3);

    IrOp tvec = build.inst(IrCmd::FLOAT_TO_VEC, build.inst(IrCmd::NUM_TO_FLOAT, t));
    IrOp one = build.inst(IrCmd::FLOAT_TO_VEC, build.constDouble(1.0));
    IrOp diff = build.inst(IrCmd::SUB_VEC, b, a);

    IrOp res = build.inst(IrCmd::MULADD_VEC, diff, tvec, a);
    IrOp ret = build.inst(IrCmd::SELECT_VEC, res, b, tvec, one);
    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), build.inst(IrCmd::TAG_VECTOR, ret));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinMathLerp(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    IrOp fallback,
    int pcpos
)
{
    if (nparams < 3 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);
    builtinCheckDouble(build, arg3, pcpos);

    IrOp a = builtinLoadDouble(build, build.vmReg(arg));
    IrOp b = builtinLoadDouble(build, args);
    IrOp t = builtinLoadDouble(build, arg3);

    IrOp l = build.inst(IrCmd::MULADD_NUM, build.inst(IrCmd::SUB_NUM, b, a), t, a);
    IrOp r = build.inst(IrCmd::SELECT_NUM, l, b, t, build.constDouble(1.0)); // select on t==1.0

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), r);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinMathUnary(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    IrOp varg = builtinLoadDouble(build, build.vmReg(arg));
    IrOp result = build.inst(cmd, varg);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), result);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinMathIsNan(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    IrOp varg = builtinLoadDouble(build, build.vmReg(arg));

    IrOp result =
        build.inst(IrCmd::CMP_SPLIT_TVALUE, build.constTag(LUA_TNUMBER), build.constTag(LUA_TNUMBER), varg, varg, build.cond(IrCondition::NotEqual));

    build.inst(IrCmd::STORE_INT, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinType(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp tag = build.inst(IrCmd::LOAD_TAG, build.vmReg(arg));
    IrOp name = build.inst(IrCmd::GET_TYPE, tag);

    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra), name);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TSTRING));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinTypeof(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp name = build.inst(IrCmd::GET_TYPEOF, build.vmReg(arg));

    build.inst(IrCmd::STORE_POINTER, build.vmReg(ra), name);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TSTRING));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32MultiargOp(
    IrBuilder& build,
    IrCmd cmd,
    bool btest,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    if (nparams < 1 || nparams > kBit32BinaryOpUnrolledParams || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    if (nparams >= 2)
        builtinCheckDouble(build, args, pcpos);

    if (nparams >= 3)
        builtinCheckDouble(build, arg3, pcpos);

    for (int i = 4; i <= nparams; ++i)
        builtinCheckDouble(build, build.vmReg(vmRegOp(args) + (i - 2)), pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp res = build.inst(IrCmd::NUM_TO_UINT, va);

    if (nparams >= 2)
    {
        IrOp vb = builtinLoadDouble(build, args);
        IrOp arg = build.inst(IrCmd::NUM_TO_UINT, vb);

        res = build.inst(cmd, res, arg);
    }

    if (nparams >= 3)
    {
        IrOp vc = builtinLoadDouble(build, arg3);
        IrOp arg = build.inst(IrCmd::NUM_TO_UINT, vc);

        res = build.inst(cmd, res, arg);
    }

    for (int i = 4; i <= nparams; ++i)
    {
        IrOp vc = builtinLoadDouble(build, build.vmReg(vmRegOp(args) + (i - 2)));
        IrOp arg = build.inst(IrCmd::NUM_TO_UINT, vc);

        res = build.inst(cmd, res, arg);
    }

    if (btest)
    {
        IrOp value = build.inst(IrCmd::CMP_INT, res, build.constInt(0), build.cond(IrCondition::NotEqual));
        build.inst(IrCmd::STORE_INT, build.vmReg(ra), value);
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));
    }
    else
    {
        IrOp value = build.inst(IrCmd::UINT_TO_NUM, res);
        build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

        if (ra != arg)
            build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));
    }

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32Bnot(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    IrOp va = builtinLoadDouble(build, build.vmReg(arg));

    IrOp vaui = build.inst(IrCmd::NUM_TO_UINT, va);
    IrOp not_ = build.inst(IrCmd::BITNOT_UINT, vaui);
    IrOp value = build.inst(IrCmd::UINT_TO_NUM, not_);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32Shift(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp vb = builtinLoadDouble(build, args);

    IrOp vaui = build.inst(IrCmd::NUM_TO_UINT, va);

    IrOp vbi;

    if (std::optional<double> vbd = build.function.asDoubleOp(vb); vbd && *vbd >= INT_MIN && *vbd <= INT_MAX)
        vbi = build.constInt(int(*vbd));
    else
        vbi = build.inst(IrCmd::NUM_TO_INT, vb);

    bool knownGoodShift = unsigned(build.function.asIntOp(vbi).value_or(-1)) < 32u;

    if (!knownGoodShift)
    {
        // unsigned(s) < 32
        build.inst(IrCmd::CHECK_CMP_INT, vbi, build.constInt(32), build.cond(IrCondition::UnsignedLess), build.vmExit(pcpos));
    }

    IrOp shift = build.inst(cmd, vaui, vbi);

    IrOp value = build.inst(IrCmd::UINT_TO_NUM, shift);
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32Rotate(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp vb = builtinLoadDouble(build, args);

    IrOp vaui = build.inst(IrCmd::NUM_TO_UINT, va);
    IrOp vbi = build.inst(IrCmd::NUM_TO_INT, vb);

    IrOp shift = build.inst(cmd, vaui, vbi);

    IrOp value = build.inst(IrCmd::UINT_TO_NUM, shift);
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32Extract(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    if (nparams == 2 && args.kind == IrOpKind::Constant && unsigned(int(build.function.doubleOp(args))) >= 32)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp vb = builtinLoadDouble(build, args);

    IrOp n = build.inst(IrCmd::NUM_TO_UINT, va);

    IrOp value;
    if (nparams == 2)
    {
        if (vb.kind == IrOpKind::Constant)
        {
            int f = int(build.function.doubleOp(vb));
            CODEGEN_ASSERT(unsigned(f) < 32); // checked above

            value = n;

            if (f)
                value = build.inst(IrCmd::BITRSHIFT_UINT, value, build.constInt(f));

            if (f + 1 < 32)
                value = build.inst(IrCmd::BITAND_UINT, value, build.constInt(1));
        }
        else
        {
            IrOp f = build.inst(IrCmd::NUM_TO_INT, vb);

            // unsigned(f) < 32
            build.inst(IrCmd::CHECK_CMP_INT, f, build.constInt(32), build.cond(IrCondition::UnsignedLess), build.vmExit(pcpos));

            IrOp shift = build.inst(IrCmd::BITRSHIFT_UINT, n, f);
            value = build.inst(IrCmd::BITAND_UINT, shift, build.constInt(1));
        }
    }
    else
    {
        IrOp f = build.inst(IrCmd::NUM_TO_INT, vb);

        builtinCheckDouble(build, arg3, pcpos);
        IrOp vc = builtinLoadDouble(build, arg3);

        IrOp w = build.inst(IrCmd::NUM_TO_INT, vc);
        IrOp fw = build.inst(IrCmd::ADD_INT, f, w);

        // f >= 0 && w > 0 && f + w <= 32
        build.inst(IrCmd::CHECK_CMP_INT, f, build.constInt(0), build.cond(IrCondition::GreaterEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT, w, build.constInt(0), build.cond(IrCondition::Greater), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT, fw, build.constInt(32), build.cond(IrCondition::LessEqual), build.vmExit(pcpos));

        IrOp shift = build.inst(IrCmd::BITLSHIFT_UINT, build.constInt(0xfffffffe), build.inst(IrCmd::SUB_INT, w, build.constInt(1)));
        IrOp m = build.inst(IrCmd::BITNOT_UINT, shift);

        IrOp nf = build.inst(IrCmd::BITRSHIFT_UINT, n, f);
        value = build.inst(IrCmd::BITAND_UINT, nf, m);
    }

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), build.inst(IrCmd::UINT_TO_NUM, value));

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32ExtractK(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp n = build.inst(IrCmd::NUM_TO_UINT, va);

    double a2 = build.function.doubleOp(args);
    int fw = int(a2);

    int f = fw & 31;
    int w1 = fw >> 5;

    uint32_t m = ~(0xfffffffeu << w1);

    IrOp result = n;

    if (f)
        result = build.inst(IrCmd::BITRSHIFT_UINT, result, build.constInt(f));

    if ((f + w1 + 1) < 32)
        result = build.inst(IrCmd::BITAND_UINT, result, build.constInt(m));

    IrOp value = build.inst(IrCmd::UINT_TO_NUM, result);
    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32Unary(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    IrOp va = builtinLoadDouble(build, build.vmReg(arg));

    IrOp vaui = build.inst(IrCmd::NUM_TO_UINT, va);

    IrOp bin = build.inst(cmd, vaui);

    IrOp value = build.inst(IrCmd::UINT_TO_NUM, bin);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), value);

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBit32Replace(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    if (nparams < 3 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckDouble(build, build.vmReg(arg), pcpos);
    builtinCheckDouble(build, args, pcpos);
    builtinCheckDouble(build, arg3, pcpos);

    IrOp va = builtinLoadDouble(build, build.vmReg(arg));
    IrOp vb = builtinLoadDouble(build, args);
    IrOp vc = builtinLoadDouble(build, arg3);

    IrOp n = build.inst(IrCmd::NUM_TO_UINT, va);
    IrOp v = build.inst(IrCmd::NUM_TO_UINT, vb);
    IrOp f = build.inst(IrCmd::NUM_TO_INT, vc);

    IrOp value;
    if (nparams == 3)
    {
        // unsigned(f) < 32
        build.inst(IrCmd::CHECK_CMP_INT, f, build.constInt(32), build.cond(IrCondition::UnsignedLess), build.vmExit(pcpos));

        IrOp m = build.constInt(1);
        IrOp shift = build.inst(IrCmd::BITLSHIFT_UINT, m, f);
        IrOp not_ = build.inst(IrCmd::BITNOT_UINT, shift);
        IrOp lhs = build.inst(IrCmd::BITAND_UINT, n, not_);

        IrOp vm = build.inst(IrCmd::BITAND_UINT, v, m);
        IrOp rhs = build.inst(IrCmd::BITLSHIFT_UINT, vm, f);

        value = build.inst(IrCmd::BITOR_UINT, lhs, rhs);
    }
    else
    {
        builtinCheckDouble(build, build.vmReg(vmRegOp(args) + 2), pcpos);
        IrOp vd = builtinLoadDouble(build, build.vmReg(vmRegOp(args) + 2));

        IrOp w = build.inst(IrCmd::NUM_TO_INT, vd);
        IrOp fw = build.inst(IrCmd::ADD_INT, f, w);

        // f >= 0 && w > 0 && f + w <= 32
        build.inst(IrCmd::CHECK_CMP_INT, f, build.constInt(0), build.cond(IrCondition::GreaterEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT, w, build.constInt(0), build.cond(IrCondition::Greater), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT, fw, build.constInt(32), build.cond(IrCondition::LessEqual), build.vmExit(pcpos));

        IrOp shift1 = build.inst(IrCmd::BITLSHIFT_UINT, build.constInt(0xfffffffe), build.inst(IrCmd::SUB_INT, w, build.constInt(1)));
        IrOp m = build.inst(IrCmd::BITNOT_UINT, shift1);

        IrOp shift2 = build.inst(IrCmd::BITLSHIFT_UINT, m, f);
        IrOp not_ = build.inst(IrCmd::BITNOT_UINT, shift2);
        IrOp lhs = build.inst(IrCmd::BITAND_UINT, n, not_);

        IrOp vm = build.inst(IrCmd::BITAND_UINT, v, m);
        IrOp rhs = build.inst(IrCmd::BITLSHIFT_UINT, vm, f);

        value = build.inst(IrCmd::BITOR_UINT, lhs, rhs);
    }

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), build.inst(IrCmd::UINT_TO_NUM, value));

    if (ra != arg)
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVector(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    CODEGEN_ASSERT(LUA_VECTOR_SIZE == 3);

    if (nparams == 2)
    {
        builtinCheckDouble(build, build.vmReg(arg), pcpos);
        builtinCheckDouble(build, args, pcpos);

        IrOp x = builtinLoadDouble(build, build.vmReg(arg));
        IrOp y = builtinLoadDouble(build, args);

        IrOp xf = build.inst(IrCmd::NUM_TO_FLOAT, x);
        IrOp yf = build.inst(IrCmd::NUM_TO_FLOAT, y);

        build.inst(IrCmd::STORE_VECTOR, build.vmReg(ra), xf, yf, build.constDouble(0.0));
    }
    else
    {
        builtinCheckDouble(build, build.vmReg(arg), pcpos);
        builtinCheckDouble(build, args, pcpos);
        builtinCheckDouble(build, arg3, pcpos);

        IrOp x = builtinLoadDouble(build, build.vmReg(arg));
        IrOp y = builtinLoadDouble(build, args);
        IrOp z = builtinLoadDouble(build, arg3);

        IrOp xf = build.inst(IrCmd::NUM_TO_FLOAT, x);
        IrOp yf = build.inst(IrCmd::NUM_TO_FLOAT, y);
        IrOp zf = build.inst(IrCmd::NUM_TO_FLOAT, z);

        build.inst(IrCmd::STORE_VECTOR, build.vmReg(ra), xf, yf, zf);
    }

    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TVECTOR));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinTableInsert(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams != 2 || nresults > 0)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TTABLE, build.vmExit(pcpos));

    IrOp table = build.inst(IrCmd::LOAD_POINTER, build.vmReg(arg));
    build.inst(IrCmd::CHECK_READONLY, table, build.vmExit(pcpos));

    IrOp pos = build.inst(IrCmd::ADD_INT, build.inst(IrCmd::TABLE_LEN, table), build.constInt(1));

    IrOp setnum = build.inst(IrCmd::TABLE_SETNUM, table, pos);

    if (args.kind == IrOpKind::Constant)
    {
        CODEGEN_ASSERT(build.function.constOp(args).kind == IrConstKind::Double);

        // No barrier necessary since numbers aren't collectable
        build.inst(IrCmd::STORE_DOUBLE, setnum, args);
        build.inst(IrCmd::STORE_TAG, setnum, build.constTag(LUA_TNUMBER));
    }
    else
    {
        IrOp va = build.inst(IrCmd::LOAD_TVALUE, args);
        build.inst(IrCmd::STORE_TVALUE, setnum, va);

        // Compiler only generates FASTCALL*K for source-level constants, so dynamic imports are not affected
        CODEGEN_ASSERT(build.function.proto);
        IrOp argstag = args.kind == IrOpKind::VmConst ? build.constTag(build.function.proto->k[vmConstOp(args)].tt) : build.undef();

        build.inst(IrCmd::BARRIER_TABLE_FORWARD, table, args, argstag);
    }

    return {BuiltinImplType::Full, 0};
}

static BuiltinImplResult translateBuiltinStringLen(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(build.vmReg(arg), LUA_TSTRING, build.vmExit(pcpos));

    IrOp ts = build.inst(IrCmd::LOAD_POINTER, build.vmReg(arg));

    IrOp len = build.inst(IrCmd::STRING_LEN, ts);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), build.inst(IrCmd::INT_TO_NUM, len));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static void translateBufferArgsAndCheckBounds(
    IrBuilder& build,
    int nparams,
    int arg,
    IrOp args,
    IrOp arg3,
    int size,
    int pcpos,
    IrOp& buf,
    IrOp& intIndex
)
{
    build.loadAndCheckTag(build.vmReg(arg), LUA_TBUFFER, build.vmExit(pcpos));
    builtinCheckDouble(build, args, pcpos);

    if (nparams == 3)
        builtinCheckDouble(build, arg3, pcpos);

    buf = build.inst(IrCmd::LOAD_POINTER, build.vmReg(arg));

    IrOp numIndex = builtinLoadDouble(build, args);
    intIndex = build.inst(IrCmd::NUM_TO_INT, numIndex);

    if (FFlag::LuauCodegenBufferRangeMerge4)
        build.inst(IrCmd::CHECK_BUFFER_LEN, buf, intIndex, build.constInt(0), build.constInt(size), build.undef(), build.vmExit(pcpos));
    else
        build.inst(IrCmd::CHECK_BUFFER_LEN, buf, intIndex, build.constInt(size), build.vmExit(pcpos));
}

static BuiltinImplResult translateBuiltinBufferRead(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos,
    IrCmd readCmd,
    int size,
    IrCmd convCmd
)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp buf, intIndex;
    translateBufferArgsAndCheckBounds(build, nparams, arg, args, arg3, size, pcpos, buf, intIndex);

    IrOp result =
        FFlag::LuauCodegenBufNoDefTag ? build.inst(readCmd, buf, intIndex, build.constTag(LUA_TBUFFER)) : build.inst(readCmd, buf, intIndex);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), convCmd == IrCmd::NOP ? result : build.inst(convCmd, result));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinBufferWrite(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos,
    IrCmd writeCmd,
    int size,
    IrCmd convCmd
)
{
    if (nparams < 3 || nresults > 0)
        return {BuiltinImplType::None, -1};

    IrOp buf, intIndex;
    translateBufferArgsAndCheckBounds(build, nparams, arg, args, arg3, size, pcpos, buf, intIndex);

    IrOp numValue = builtinLoadDouble(build, arg3);

    if (FFlag::LuauCodegenBufNoDefTag)
        build.inst(writeCmd, buf, intIndex, convCmd == IrCmd::NOP ? numValue : build.inst(convCmd, numValue), build.constTag(LUA_TBUFFER));
    else
        build.inst(writeCmd, buf, intIndex, convCmd == IrCmd::NOP ? numValue : build.inst(convCmd, numValue));

    return {BuiltinImplType::Full, 0};
}

static BuiltinImplResult translateBuiltinVectorMagnitude(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 1 || nresults > 1 || arg1.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp a = build.inst(IrCmd::LOAD_TVALUE, arg1, build.constInt(0));

    IrOp sum = build.inst(IrCmd::DOT_VEC, a, a);
    IrOp mag = build.inst(IrCmd::SQRT_FLOAT, sum);

    mag = build.inst(IrCmd::FLOAT_TO_NUM, mag);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), mag);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVectorNormalize(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 1 || nresults > 1 || arg1.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp a = build.inst(IrCmd::LOAD_TVALUE, arg1, build.constInt(0));
    IrOp sum = build.inst(IrCmd::DOT_VEC, a, a);

    IrOp mag = build.inst(IrCmd::SQRT_FLOAT, sum);
    IrOp inv = build.inst(IrCmd::DIV_FLOAT, build.constDouble(1.0f), mag);
    IrOp invvec = build.inst(IrCmd::FLOAT_TO_VEC, inv);

    IrOp result = build.inst(IrCmd::MUL_VEC, a, invvec);

    result = build.inst(IrCmd::TAG_VECTOR, result);

    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), result);

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVectorCross(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 2 || nresults > 1 || arg1.kind == IrOpKind::Constant || args.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));
    build.loadAndCheckTag(args, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp x1 = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(0));
    IrOp x2 = build.inst(IrCmd::LOAD_FLOAT, args, build.constInt(0));

    IrOp y1 = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(4));
    IrOp y2 = build.inst(IrCmd::LOAD_FLOAT, args, build.constInt(4));

    IrOp z1 = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(8));
    IrOp z2 = build.inst(IrCmd::LOAD_FLOAT, args, build.constInt(8));

    IrOp y1z2 = build.inst(IrCmd::MUL_FLOAT, y1, z2);
    IrOp z1y2 = build.inst(IrCmd::MUL_FLOAT, z1, y2);
    IrOp xr = build.inst(IrCmd::SUB_FLOAT, y1z2, z1y2);

    IrOp z1x2 = build.inst(IrCmd::MUL_FLOAT, z1, x2);
    IrOp x1z2 = build.inst(IrCmd::MUL_FLOAT, x1, z2);
    IrOp yr = build.inst(IrCmd::SUB_FLOAT, z1x2, x1z2);

    IrOp x1y2 = build.inst(IrCmd::MUL_FLOAT, x1, y2);
    IrOp y1x2 = build.inst(IrCmd::MUL_FLOAT, y1, x2);
    IrOp zr = build.inst(IrCmd::SUB_FLOAT, x1y2, y1x2);

    build.inst(IrCmd::STORE_VECTOR, build.vmReg(ra), xr, yr, zr);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TVECTOR));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVectorDot(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 2 || nresults > 1 || arg1.kind == IrOpKind::Constant || args.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));
    build.loadAndCheckTag(args, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp a = build.inst(IrCmd::LOAD_TVALUE, arg1, build.constInt(0));
    IrOp b = build.inst(IrCmd::LOAD_TVALUE, args, build.constInt(0));

    IrOp sum = build.inst(IrCmd::DOT_VEC, a, b);

    sum = build.inst(IrCmd::FLOAT_TO_NUM, sum);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), sum);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVectorMap1x4(
    IrBuilder& build,
    IrCmd cmd,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 1 || nresults > 1 || arg1.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp value = build.inst(IrCmd::LOAD_TVALUE, arg1);
    IrOp ret = build.inst(cmd, value);

    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), build.inst(IrCmd::TAG_VECTOR, ret));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVectorMap1(
    IrBuilder& build,
    IrCmd cmd,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 1 || nresults > 1 || arg1.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp x1 = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(0));
    IrOp y1 = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(4));
    IrOp z1 = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(8));

    IrOp xr = build.inst(cmd, x1);
    IrOp yr = build.inst(cmd, y1);
    IrOp zr = build.inst(cmd, z1);

    build.inst(IrCmd::STORE_VECTOR, build.vmReg(ra), xr, yr, zr);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TVECTOR));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinVectorClamp(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    IrOp fallback,
    int pcpos
)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 3 || nresults > 1 || arg1.kind == IrOpKind::Constant || args.kind == IrOpKind::Constant || arg3.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));
    build.loadAndCheckTag(args, LUA_TVECTOR, build.vmExit(pcpos));
    build.loadAndCheckTag(arg3, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp block1 = build.block(IrBlockKind::Internal);
    IrOp block2 = build.block(IrBlockKind::Internal);
    IrOp block3 = build.block(IrBlockKind::Internal);

    IrOp x = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(0));
    IrOp xmin = build.inst(IrCmd::LOAD_FLOAT, args, build.constInt(0));
    IrOp xmax = build.inst(IrCmd::LOAD_FLOAT, arg3, build.constInt(0));

    build.inst(IrCmd::JUMP_CMP_FLOAT, xmin, xmax, build.cond(IrCondition::NotLessEqual), fallback, block1);

    build.beginBlock(block1);

    IrOp y = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(4));
    IrOp ymin = build.inst(IrCmd::LOAD_FLOAT, args, build.constInt(4));
    IrOp ymax = build.inst(IrCmd::LOAD_FLOAT, arg3, build.constInt(4));

    build.inst(IrCmd::JUMP_CMP_FLOAT, ymin, ymax, build.cond(IrCondition::NotLessEqual), fallback, block2);

    build.beginBlock(block2);

    IrOp z = build.inst(IrCmd::LOAD_FLOAT, arg1, build.constInt(8));
    IrOp zmin = build.inst(IrCmd::LOAD_FLOAT, args, build.constInt(8));
    IrOp zmax = build.inst(IrCmd::LOAD_FLOAT, arg3, build.constInt(8));

    build.inst(IrCmd::JUMP_CMP_FLOAT, zmin, zmax, build.cond(IrCondition::NotLessEqual), fallback, block3);

    build.beginBlock(block3);

    IrOp xtemp = build.inst(IrCmd::MAX_FLOAT, xmin, x);
    IrOp xclamped = build.inst(IrCmd::MIN_FLOAT, xmax, xtemp);

    IrOp ytemp = build.inst(IrCmd::MAX_FLOAT, ymin, y);
    IrOp yclamped = build.inst(IrCmd::MIN_FLOAT, ymax, ytemp);

    IrOp ztemp = build.inst(IrCmd::MAX_FLOAT, zmin, z);
    IrOp zclamped = build.inst(IrCmd::MIN_FLOAT, zmax, ztemp);

    build.inst(IrCmd::STORE_VECTOR, build.vmReg(ra), xclamped, yclamped, zclamped);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TVECTOR));

    return {BuiltinImplType::UsesFallback, 1};
}

static BuiltinImplResult translateBuiltinVectorMinMax(
    IrBuilder& build,
    IrCmd cmd,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    IrOp arg1 = build.vmReg(arg);

    if (nparams != 2 || nresults > 1 || arg1.kind == IrOpKind::Constant || args.kind == IrOpKind::Constant)
        return {BuiltinImplType::None, -1};

    build.loadAndCheckTag(arg1, LUA_TVECTOR, build.vmExit(pcpos));
    build.loadAndCheckTag(args, LUA_TVECTOR, build.vmExit(pcpos));

    IrOp value1 = build.inst(IrCmd::LOAD_TVALUE, arg1);
    IrOp value2 = build.inst(IrCmd::LOAD_TVALUE, args);

    IrOp ret = build.inst(cmd, value2, value1); // Swapped arguments are required for consistency with VM builtins

    build.inst(IrCmd::STORE_TVALUE, build.vmReg(ra), build.inst(IrCmd::TAG_VECTOR, ret));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Create(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg, // arg contains the LUA_TNUMBER representing the int64 to create
    int nresults,
    int pcpos
)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp argReg = build.vmReg(arg);
    builtinCheckDouble(build, argReg, pcpos);

    IrOp argValue = builtinLoadDouble(build, build.vmReg(arg));

    IrOp integerValue = build.inst(IrCmd::NUM_TO_INT64, argValue);
    // roundtrip check: ((double)l) == x
    IrOp backToDouble = build.inst(IrCmd::INT64_TO_NUM, integerValue);

    build.inst(IrCmd::CHECK_CMP_NUM, backToDouble, argValue, build.cond(IrCondition::Equal), build.vmExit(pcpos));

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), integerValue);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64ToNumber(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg, // arg contains the int64 to cast to number
    int nresults,
    int pcpos
)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    IrOp argReg = build.vmReg(arg);
    builtinCheckInt64(build, argReg, pcpos);
    IrOp argValue = builtinLoadInt64(build, argReg);

    build.inst(IrCmd::STORE_DOUBLE, build.vmReg(ra), build.inst(IrCmd::INT64_TO_NUM, argValue));
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TNUMBER));

    return {BuiltinImplType::Full, 1};
}

enum class Int64Binary
{
    Add,
    Sub,
    Mul,
    Div,
    Idiv,
    Udiv,
    Rem,
    Urem,
    Mod,
};

static BuiltinImplResult translateBuiltinInt64Binary(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    int nresults,
    int pcpos,
    Int64Binary op
)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp vb = builtinLoadInt64(build, args);

    IrOp binOp;
    switch (op)
    {
    case Int64Binary::Add:
        binOp = build.inst(IrCmd::ADD_INT64, va, vb);
        break;

    case Int64Binary::Sub:
        binOp = build.inst(IrCmd::SUB_INT64, va, vb);
        break;
    case Int64Binary::Mul:
        binOp = build.inst(IrCmd::MUL_INT64, va, vb);
        break;
    case Int64Binary::Div:
        build.inst(IrCmd::CHECK_DIV_INT64, va, vb, build.vmExit(pcpos));
        binOp = build.inst(IrCmd::DIV_INT64, va, vb);
        break;
    case Int64Binary::Idiv:
        build.inst(IrCmd::CHECK_DIV_INT64, va, vb, build.vmExit(pcpos));
        binOp = build.inst(IrCmd::IDIV_INT64, va, vb);
        break;
    case Int64Binary::Udiv:
        build.inst(IrCmd::CHECK_CMP_INT64, vb, build.constInt64(0), build.cond(IrCondition::NotEqual), build.vmExit(pcpos));
        binOp = build.inst(IrCmd::UDIV_INT64, va, vb);
        break;
    case Int64Binary::Rem:
        build.inst(IrCmd::CHECK_CMP_INT64, vb, build.constInt64(0), build.cond(IrCondition::NotEqual), build.vmExit(pcpos));
        // ARM64 sdiv wraps, producing rem=0.
        binOp = build.inst(IrCmd::REM_INT64, va, vb);
        break;
    case Int64Binary::Urem:
        build.inst(IrCmd::CHECK_CMP_INT64, vb, build.constInt64(0), build.cond(IrCondition::NotEqual), build.vmExit(pcpos));
        binOp = build.inst(IrCmd::UREM_INT64, va, vb);
        break;
    case Int64Binary::Mod:
        build.inst(IrCmd::CHECK_CMP_INT64, vb, build.constInt64(0), build.cond(IrCondition::NotEqual), build.vmExit(pcpos));
        // ARM64 sdiv wraps, producing mod=0.
        binOp = build.inst(IrCmd::MOD_INT64, va, vb);
        break;
    default:
        CODEGEN_ASSERT(!"Unhandled Int64Binary kind");
    }

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), binOp);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64MinMax(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos, bool min)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp vb = builtinLoadInt64(build, args);

    IrOp cond = min ? build.cond(IrCondition::LessEqual) : build.cond(IrCondition::Greater);

    // vb < va ? vb : va
    IrOp selectOp = build.inst(IrCmd::SELECT_INT64, va, vb, vb, va, cond);
    for (int i = 3; i <= nparams; ++i)
    {
        builtinCheckInt64(build, build.vmReg(vmRegOp(args) + (i - 2)), pcpos);

        IrOp vc = builtinLoadInt64(build, build.vmReg(vmRegOp(args) + (i - 2)));

        selectOp = build.inst(IrCmd::SELECT_INT64, vc, selectOp, selectOp, vc, cond);
    }
    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), selectOp);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Neg(IrBuilder& build, int nparams, int ra, int arg, int nresults, int pcpos)
{
    if (nparams != 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp result = build.inst(IrCmd::SUB_INT64, build.constInt64(0), va);

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

// TODO: tune this
static const int kInt64BinaryOpUnrolledParams = 5;

static BuiltinImplResult translateBuiltinInt64MultiargOp(
    IrBuilder& build,
    IrCmd cmd,
    bool btest,
    int64_t identity,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nresults,
    int pcpos
)
{
    if (nparams > kInt64BinaryOpUnrolledParams || nresults > 1)
        return {BuiltinImplType::None, -1};

    if (nparams == 0)
    {
        if (btest)
        {
            // btest() with no args: identity is -1 (all bits), -1 != 0 -> true (1)
            build.inst(IrCmd::STORE_INT, build.vmReg(ra), build.constInt(1));
            build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));
        }
        else
        {
            build.inst(IrCmd::STORE_INT64, build.vmReg(ra), build.constInt64(identity));
            build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));
        }
        return {BuiltinImplType::Full, 1};
    }

    builtinCheckInt64(build, build.vmReg(arg), pcpos);

    if (nparams >= 2)
        builtinCheckInt64(build, args, pcpos);

    if (nparams >= 3)
        builtinCheckInt64(build, arg3, pcpos);

    for (int i = 4; i <= nparams; ++i)
        builtinCheckInt64(build, build.vmReg(vmRegOp(args) + (i - 2)), pcpos);

    IrOp res = builtinLoadInt64(build, build.vmReg(arg));

    if (nparams >= 2)
    {
        IrOp vb = builtinLoadInt64(build, args);
        res = build.inst(cmd, res, vb);
    }

    if (nparams >= 3)
    {
        IrOp vc = builtinLoadInt64(build, arg3);
        res = build.inst(cmd, res, vc);
    }

    for (int i = 4; i <= nparams; ++i)
    {
        IrOp vc = builtinLoadInt64(build, build.vmReg(vmRegOp(args) + (i - 2)));
        res = build.inst(cmd, res, vc);
    }

    if (btest)
    {
        IrOp result = build.inst(IrCmd::CMP_INT64, res, build.constInt64(0), build.cond(IrCondition::NotEqual));
        build.inst(IrCmd::STORE_INT, build.vmReg(ra), result);
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));
    }
    else
    {
        build.inst(IrCmd::STORE_INT64, build.vmReg(ra), res);
        build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));
    }

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Extract(IrBuilder& build, int nparams, int ra, int arg, IrOp args, IrOp arg3, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);

    IrOp n = builtinLoadInt64(build, build.vmReg(arg));
    IrOp f = builtinLoadInt64(build, args);

    IrOp value;
    if (nparams == 2)
    {
        // extract(n, f): extract single bit at position f
        // f >= 0 && f <= 63
        build.inst(IrCmd::CHECK_CMP_INT64, f, build.constInt64(0), build.cond(IrCondition::GreaterEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT64, f, build.constInt64(63), build.cond(IrCondition::LessEqual), build.vmExit(pcpos));

        IrOp shifted = build.inst(IrCmd::BITRSHIFT_INT64, n, f);
        value = build.inst(IrCmd::BITAND_INT64, shifted, build.constInt64(1));
    }
    else
    {
        // extract(n, f, w): extract w bits starting at position f
        builtinCheckInt64(build, arg3, pcpos);
        IrOp w = builtinLoadInt64(build, arg3);
        IrOp fw = build.inst(IrCmd::ADD_INT64, f, w);

        // f >= 0 && f <= 63 && w >= 1 && f + w <= 64
        build.inst(IrCmd::CHECK_CMP_INT64, f, build.constInt64(0), build.cond(IrCondition::GreaterEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT64, f, build.constInt64(63), build.cond(IrCondition::LessEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT64, w, build.constInt64(1), build.cond(IrCondition::GreaterEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT64, w, build.constInt64(64), build.cond(IrCondition::LessEqual), build.vmExit(pcpos));
        build.inst(IrCmd::CHECK_CMP_INT64, fw, build.constInt64(64), build.cond(IrCondition::LessEqual), build.vmExit(pcpos));

        // mask = 0xFFFFFFFFFFFFFFFF >> (64 - w)
        IrOp shiftAmount = build.inst(IrCmd::SUB_INT64, build.constInt64(64), w);
        IrOp mask = build.inst(IrCmd::BITRSHIFT_INT64, build.constInt64(-1), shiftAmount);

        IrOp shifted = build.inst(IrCmd::BITRSHIFT_INT64, n, f);
        value = build.inst(IrCmd::BITAND_INT64, shifted, mask);
    }

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), value);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Rotate(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp vb = builtinLoadInt64(build, args);

    IrOp result = build.inst(cmd, va, vb);

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Unary(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp result = build.inst(cmd, va);

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Shift(IrBuilder& build, IrCmd cmd, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp vb = builtinLoadInt64(build, args);

    IrOp result = build.inst(cmd, va, vb);

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Bnot(IrBuilder& build, int nparams, int ra, int arg, int nresults, int pcpos)
{
    if (nparams < 1 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp result = build.inst(IrCmd::BITNOT_INT64, va);

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Compare(
    IrBuilder& build,
    int nparams,
    int ra,
    int arg,
    IrOp args,
    int nresults,
    int pcpos,
    IrCondition cond
)
{
    if (nparams < 2 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);

    IrOp va = builtinLoadInt64(build, build.vmReg(arg));
    IrOp vb = builtinLoadInt64(build, args);

    IrOp result = build.inst(IrCmd::CMP_INT64, va, vb, build.cond(cond));
    build.inst(IrCmd::STORE_INT, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TBOOLEAN));

    return {BuiltinImplType::Full, 1};
}

static BuiltinImplResult translateBuiltinInt64Clamp(IrBuilder& build, int nparams, int ra, int arg, IrOp args, int nresults, int pcpos)
{
    if (nparams < 3 || nresults > 1)
        return {BuiltinImplType::None, -1};

    builtinCheckInt64(build, build.vmReg(arg), pcpos);
    builtinCheckInt64(build, args, pcpos);
    builtinCheckInt64(build, build.vmReg(vmRegOp(args) + 1), pcpos);

    IrOp val = builtinLoadInt64(build, build.vmReg(arg));
    IrOp mi = builtinLoadInt64(build, args);
    IrOp mx = builtinLoadInt64(build, build.vmReg(vmRegOp(args) + 1));

    // guard: min <= max
    build.inst(IrCmd::CHECK_CMP_INT64, mi, mx, build.cond(IrCondition::LessEqual), build.vmExit(pcpos));

    // clamp: if val < min, use min; then if result > max, use max
    IrOp clamped = build.inst(IrCmd::SELECT_INT64, val, mi, val, mi, build.cond(IrCondition::Less));
    IrOp result = build.inst(IrCmd::SELECT_INT64, clamped, mx, clamped, mx, build.cond(IrCondition::Greater));

    build.inst(IrCmd::STORE_INT64, build.vmReg(ra), result);
    build.inst(IrCmd::STORE_TAG, build.vmReg(ra), build.constTag(LUA_TINTEGER));

    return {BuiltinImplType::Full, 1};
}

BuiltinImplResult translateBuiltin(
    IrBuilder& build,
    int bfid,
    int ra,
    int arg,
    IrOp args,
    IrOp arg3,
    int nparams,
    int nresults,
    IrOp fallback,
    int pcpos
)
{
    // Builtins are not allowed to handle variadic arguments
    if (nparams == LUA_MULTRET)
        return {BuiltinImplType::None, -1};

    if (FFlag::LuauCodegenInteger2 && (args.kind == IrOpKind::Constant || arg3.kind == IrOpKind::Constant))
    {
        switch (bfid)
        {
        case LBF_MATH_MIN:
        case LBF_MATH_MAX:
        case LBF_MATH_POW:
        case LBF_MATH_FMOD:
        case LBF_MATH_ATAN2:
        case LBF_MATH_LDEXP:
        case LBF_MATH_LERP:
        case LBF_MATH_CLAMP:
        case LBF_BIT32_BAND:
        case LBF_BIT32_BOR:
        case LBF_BIT32_BXOR:
        case LBF_BIT32_BTEST:
        case LBF_BIT32_LSHIFT:
        case LBF_BIT32_RSHIFT:
        case LBF_BIT32_ARSHIFT:
        case LBF_BIT32_LROTATE:
        case LBF_BIT32_RROTATE:
        case LBF_BIT32_EXTRACT:
        case LBF_BIT32_EXTRACTK:
        case LBF_BIT32_REPLACE:
        case LBF_VECTOR:
        case LBF_TABLE_INSERT:
        case LBF_BUFFER_READI8:
        case LBF_BUFFER_READU8:
        case LBF_BUFFER_WRITEU8:
        case LBF_BUFFER_READI16:
        case LBF_BUFFER_READU16:
        case LBF_BUFFER_WRITEU16:
        case LBF_BUFFER_READI32:
        case LBF_BUFFER_READU32:
        case LBF_BUFFER_WRITEU32:
        case LBF_BUFFER_READF32:
        case LBF_BUFFER_WRITEF32:
        case LBF_BUFFER_READF64:
        case LBF_BUFFER_WRITEF64:
            if (!isCompatibleConstant(build, args, IrConstKind::Double))
                return {BuiltinImplType::None, -1};

            if (!isCompatibleConstant(build, arg3, IrConstKind::Double))
                return {BuiltinImplType::None, -1};

            break;

        case LBF_INTEGER_ADD:
        case LBF_INTEGER_SUB:
        case LBF_INTEGER_MUL:
        case LBF_INTEGER_DIV:
        case LBF_INTEGER_IDIV:
        case LBF_INTEGER_UDIV:
        case LBF_INTEGER_REM:
        case LBF_INTEGER_UREM:
        case LBF_INTEGER_MOD:
        case LBF_INTEGER_MIN:
        case LBF_INTEGER_MAX:
        case LBF_INTEGER_CLAMP:
        case LBF_INTEGER_LT:
        case LBF_INTEGER_LE:
        case LBF_INTEGER_GT:
        case LBF_INTEGER_GE:
        case LBF_INTEGER_ULT:
        case LBF_INTEGER_ULE:
        case LBF_INTEGER_UGT:
        case LBF_INTEGER_UGE:
        case LBF_INTEGER_BAND:
        case LBF_INTEGER_BOR:
        case LBF_INTEGER_BXOR:
        case LBF_INTEGER_BNOT:
        case LBF_INTEGER_BTEST:
        case LBF_INTEGER_LSHIFT:
        case LBF_INTEGER_RSHIFT:
        case LBF_INTEGER_ARSHIFT:
        case LBF_INTEGER_LROTATE:
        case LBF_INTEGER_RROTATE:
        case LBF_INTEGER_EXTRACT:
            if (!isCompatibleConstant(build, args, IrConstKind::Int64))
                return {BuiltinImplType::None, -1};

            if (!isCompatibleConstant(build, arg3, IrConstKind::Int64))
                return {BuiltinImplType::None, -1};

            break;
        }
    }

    switch (bfid)
    {
    case LBF_ASSERT:
        return translateBuiltinAssert(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_MATH_DEG:
        return translateBuiltinMathDegRad(build, IrCmd::DIV_NUM, nparams, ra, arg, args, nresults, pcpos);
    case LBF_MATH_RAD:
        return translateBuiltinMathDegRad(build, IrCmd::MUL_NUM, nparams, ra, arg, args, nresults, pcpos);
    case LBF_MATH_LOG:
        return translateBuiltinMathLog(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_MATH_MIN:
        return translateBuiltinMathMinMax(build, IrCmd::MIN_NUM, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_MATH_MAX:
        return translateBuiltinMathMinMax(build, IrCmd::MAX_NUM, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_MATH_CLAMP:
        return translateBuiltinMathClamp(build, nparams, ra, arg, args, arg3, nresults, fallback, pcpos);
    case LBF_MATH_FLOOR:
        return translateBuiltinMathUnary(build, IrCmd::FLOOR_NUM, nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_CEIL:
        return translateBuiltinMathUnary(build, IrCmd::CEIL_NUM, nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_SQRT:
        return translateBuiltinMathUnary(build, IrCmd::SQRT_NUM, nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_ABS:
        return translateBuiltinMathUnary(build, IrCmd::ABS_NUM, nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_ROUND:
        return translateBuiltinMathUnary(build, IrCmd::ROUND_NUM, nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_EXP:
    case LBF_MATH_ASIN:
    case LBF_MATH_SIN:
    case LBF_MATH_SINH:
    case LBF_MATH_ACOS:
    case LBF_MATH_COS:
    case LBF_MATH_COSH:
    case LBF_MATH_ATAN:
    case LBF_MATH_TAN:
    case LBF_MATH_TANH:
    case LBF_MATH_LOG10:
        return translateBuiltinNumberToNumberLibm(build, LuauBuiltinFunction(bfid), nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_SIGN:
        return translateBuiltinMathUnary(build, IrCmd::SIGN_NUM, nparams, ra, arg, nresults, pcpos);
    case LBF_MATH_POW:
    case LBF_MATH_FMOD:
    case LBF_MATH_ATAN2:
    case LBF_MATH_LDEXP:
        return translateBuiltin2NumberToNumberLibm(build, LuauBuiltinFunction(bfid), nparams, ra, arg, args, nresults, pcpos);
    case LBF_MATH_FREXP:
    case LBF_MATH_MODF:
        return translateBuiltinNumberTo2Number(build, LuauBuiltinFunction(bfid), nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_BAND:
        return translateBuiltinBit32MultiargOp(build, IrCmd::BITAND_UINT, /* btest= */ false, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_BIT32_BOR:
        return translateBuiltinBit32MultiargOp(build, IrCmd::BITOR_UINT, /* btest= */ false, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_BIT32_BXOR:
        return translateBuiltinBit32MultiargOp(build, IrCmd::BITXOR_UINT, /* btest= */ false, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_BIT32_BTEST:
        return translateBuiltinBit32MultiargOp(build, IrCmd::BITAND_UINT, /* btest= */ true, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_BIT32_BNOT:
        return translateBuiltinBit32Bnot(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_LSHIFT:
        return translateBuiltinBit32Shift(build, IrCmd::BITLSHIFT_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_RSHIFT:
        return translateBuiltinBit32Shift(build, IrCmd::BITRSHIFT_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_ARSHIFT:
        return translateBuiltinBit32Shift(build, IrCmd::BITARSHIFT_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_LROTATE:
        return translateBuiltinBit32Rotate(build, IrCmd::BITLROTATE_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_RROTATE:
        return translateBuiltinBit32Rotate(build, IrCmd::BITRROTATE_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_EXTRACT:
        return translateBuiltinBit32Extract(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_BIT32_EXTRACTK:
        return translateBuiltinBit32ExtractK(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_COUNTLZ:
        return translateBuiltinBit32Unary(build, IrCmd::BITCOUNTLZ_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_COUNTRZ:
        return translateBuiltinBit32Unary(build, IrCmd::BITCOUNTRZ_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_REPLACE:
        return translateBuiltinBit32Replace(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_TYPE:
        return translateBuiltinType(build, nparams, ra, arg, args, nresults);
    case LBF_TYPEOF:
        return translateBuiltinTypeof(build, nparams, ra, arg, args, nresults);
    case LBF_VECTOR:
        return translateBuiltinVector(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_TABLE_INSERT:
        return translateBuiltinTableInsert(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_STRING_LEN:
        return translateBuiltinStringLen(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BIT32_BYTESWAP:
        return translateBuiltinBit32Unary(build, IrCmd::BYTESWAP_UINT, nparams, ra, arg, args, nresults, pcpos);
    case LBF_BUFFER_READI8:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READI8, 1, IrCmd::INT_TO_NUM);
    case LBF_BUFFER_READU8:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READU8, 1, IrCmd::INT_TO_NUM);
    case LBF_BUFFER_WRITEU8:
        return translateBuiltinBufferWrite(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_WRITEI8, 1, IrCmd::NUM_TO_UINT);
    case LBF_BUFFER_READI16:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READI16, 2, IrCmd::INT_TO_NUM);
    case LBF_BUFFER_READU16:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READU16, 2, IrCmd::INT_TO_NUM);
    case LBF_BUFFER_WRITEU16:
        return translateBuiltinBufferWrite(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_WRITEI16, 2, IrCmd::NUM_TO_UINT);
    case LBF_BUFFER_READI32:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READI32, 4, IrCmd::INT_TO_NUM);
    case LBF_BUFFER_READU32:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READI32, 4, IrCmd::UINT_TO_NUM);
    case LBF_BUFFER_WRITEU32:
        return translateBuiltinBufferWrite(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_WRITEI32, 4, IrCmd::NUM_TO_UINT);
    case LBF_BUFFER_READF32:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READF32, 4, IrCmd::FLOAT_TO_NUM);
    case LBF_BUFFER_WRITEF32:
        return translateBuiltinBufferWrite(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_WRITEF32, 4, IrCmd::NUM_TO_FLOAT);
    case LBF_BUFFER_READF64:
        return translateBuiltinBufferRead(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_READF64, 8, IrCmd::NOP);
    case LBF_BUFFER_WRITEF64:
        return translateBuiltinBufferWrite(build, nparams, ra, arg, args, arg3, nresults, pcpos, IrCmd::BUFFER_WRITEF64, 8, IrCmd::NOP);
    case LBF_VECTOR_MAGNITUDE:
        return translateBuiltinVectorMagnitude(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_NORMALIZE:
        return translateBuiltinVectorNormalize(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_CROSS:
        return translateBuiltinVectorCross(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_DOT:
        return translateBuiltinVectorDot(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_FLOOR:
        return translateBuiltinVectorMap1x4(build, IrCmd::FLOOR_VEC, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_CEIL:
        return translateBuiltinVectorMap1x4(build, IrCmd::CEIL_VEC, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_ABS:
        return translateBuiltinVectorMap1x4(build, IrCmd::ABS_VEC, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_SIGN:
        return translateBuiltinVectorMap1(build, IrCmd::SIGN_FLOAT, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_CLAMP:
        return translateBuiltinVectorClamp(build, nparams, ra, arg, args, arg3, nresults, fallback, pcpos);
    case LBF_VECTOR_MIN:
        return translateBuiltinVectorMinMax(build, IrCmd::MIN_VEC, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_MAX:
        return translateBuiltinVectorMinMax(build, IrCmd::MAX_VEC, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_VECTOR_LERP:
        return translateBuiltinVectorLerp(build, nparams, ra, arg, args, arg3, nresults, pcpos);
    case LBF_MATH_LERP:
        return translateBuiltinMathLerp(build, nparams, ra, arg, args, arg3, nresults, fallback, pcpos);
    case LBF_MATH_ISNAN:
        return translateBuiltinMathIsNan(build, nparams, ra, arg, args, nresults, pcpos);
    case LBF_INTEGER_CREATE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Create(build, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_TONUMBER:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64ToNumber(build, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_ADD:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Add);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_SUB:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Sub);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_MUL:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Mul);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_DIV:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Div);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_IDIV:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Idiv);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_UDIV:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Udiv);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_REM:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Rem);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_UREM:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Urem);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_MOD:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Binary(build, nparams, ra, arg, args, nresults, pcpos, Int64Binary::Mod);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_MIN:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64MinMax(build, nparams, ra, arg, args, nresults, pcpos, true);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_MAX:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64MinMax(build, nparams, ra, arg, args, nresults, pcpos, false);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_NEG:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Neg(build, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_CLAMP:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Clamp(build, nparams, ra, arg, args, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_LT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::Less);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_LE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::LessEqual);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_GT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::Greater);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_GE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::GreaterEqual);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_ULT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::UnsignedLess);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_ULE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::UnsignedLessEqual);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_UGT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::UnsignedGreater);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_UGE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Compare(build, nparams, ra, arg, args, nresults, pcpos, IrCondition::UnsignedGreaterEqual);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_BAND:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64MultiargOp(build, IrCmd::BITAND_INT64, false, int64_t(-1), nparams, ra, arg, args, arg3, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_BOR:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64MultiargOp(build, IrCmd::BITOR_INT64, false, int64_t(0), nparams, ra, arg, args, arg3, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_BXOR:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64MultiargOp(build, IrCmd::BITXOR_INT64, false, int64_t(0), nparams, ra, arg, args, arg3, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_BNOT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Bnot(build, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_BTEST:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64MultiargOp(build, IrCmd::BITAND_INT64, true, int64_t(-1), nparams, ra, arg, args, arg3, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_LSHIFT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Shift(build, IrCmd::BITLSHIFT_INT64, nparams, ra, arg, args, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_RSHIFT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Shift(build, IrCmd::BITRSHIFT_INT64, nparams, ra, arg, args, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_ARSHIFT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Shift(build, IrCmd::BITARSHIFT_INT64, nparams, ra, arg, args, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_LROTATE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Rotate(build, IrCmd::BITLROTATE_INT64, nparams, ra, arg, args, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_RROTATE:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Rotate(build, IrCmd::BITRROTATE_INT64, nparams, ra, arg, args, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_COUNTLZ:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Unary(build, IrCmd::BITCOUNTLZ_INT64, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_COUNTRZ:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Unary(build, IrCmd::BITCOUNTRZ_INT64, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_BSWAP:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Unary(build, IrCmd::BYTESWAP_INT64, nparams, ra, arg, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    case LBF_INTEGER_EXTRACT:
        if (FFlag::LuauCodegenInteger2)
            return translateBuiltinInt64Extract(build, nparams, ra, arg, args, arg3, nresults, pcpos);
        return {BuiltinImplType::None, -1};
    default:
        return {BuiltinImplType::None, -1};
    }
}

} // namespace CodeGen
} // namespace Luau
