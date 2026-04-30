// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "NativeState.h"

#include "Luau/UnwindBuilder.h"

#include "CodeGenUtils.h"

#include "lbuiltins.h"
#include "lgc.h"
#include "ltable.h"
#include "lfunc.h"
#include "lvm.h"

#include <math.h>
#include <string.h>

namespace Luau
{
namespace CodeGen
{

void initFunctions(NativeContext& context)
{
    static_assert(sizeof(context.luauF_table) == sizeof(luauF_table), "fastcall tables are not of the same length");
    memcpy(context.luauF_table, luauF_table, sizeof(luauF_table));

    context.luaV_lessthan = luaV_lessthan;
    context.luaV_lessequal = luaV_lessequal;
    context.luaV_equalval = luaV_equalval;

    context.luaV_doarithadd = luaV_doarithimpl<TM_ADD>;
    context.luaV_doarithsub = luaV_doarithimpl<TM_SUB>;
    context.luaV_doarithmul = luaV_doarithimpl<TM_MUL>;
    context.luaV_doarithdiv = luaV_doarithimpl<TM_DIV>;
    context.luaV_doarithidiv = luaV_doarithimpl<TM_IDIV>;
    context.luaV_doarithmod = luaV_doarithimpl<TM_MOD>;
    context.luaV_doarithpow = luaV_doarithimpl<TM_POW>;
    context.luaV_doarithunm = luaV_doarithimpl<TM_UNM>;

    context.luaV_dolen = luaV_dolen;
    context.luaV_gettable = luaV_gettable;
    context.luaV_settable = luaV_settable;
    context.luaV_concat = luaV_concat;

    context.luaH_getn = luaH_getn;
    context.luaH_new = luaH_new;
    context.luaH_clone = luaH_clone;
    context.luaH_resizearray = luaH_resizearray;
    context.luaH_setnum = luaH_setnum;

    context.luaC_barriertable = luaC_barriertable;
    context.luaC_barrierf = luaC_barrierf;
    context.luaC_barrierback = luaC_barrierback;
    context.luaC_step = luaC_step;

    context.luaF_close = luaF_close;
    context.luaF_findupval = luaF_findupval;
    context.luaF_newLclosure = luaF_newLclosure;

    context.luaT_gettm = luaT_gettm;
    context.luaT_objtypenamestr = luaT_objtypenamestr;

    context.libm_exp = exp;
    context.libm_pow = pow;
    context.libm_fmod = fmod;
    context.libm_log = log;
    context.libm_log2 = log2;
    context.libm_log10 = log10;
    context.libm_ldexp = ldexp;
    context.libm_round = round;
    context.libm_frexp = frexp;
    context.libm_modf = modf;

    context.libm_asin = asin;
    context.libm_sin = sin;
    context.libm_sinh = sinh;
    context.libm_acos = acos;
    context.libm_cos = cos;
    context.libm_cosh = cosh;
    context.libm_atan = atan;
    context.libm_atan2 = atan2;
    context.libm_tan = tan;
    context.libm_tanh = tanh;

    context.forgLoopTableIter = forgLoopTableIter;
    context.forgLoopNodeIter = forgLoopNodeIter;
    context.forgLoopNonTableFallback = forgLoopNonTableFallback;
    context.forgPrepXnextFallback = forgPrepXnextFallback;
    context.callProlog = callProlog;
    context.callEpilogC = callEpilogC;
    context.newUserdata = newUserdata;
    context.getImport = getImport;

    context.callFallback = callFallback;

    context.executeGETGLOBAL = executeGETGLOBAL;
    context.executeSETGLOBAL = executeSETGLOBAL;
    context.executeGETTABLEKS = executeGETTABLEKS;
    context.executeSETTABLEKS = executeSETTABLEKS;

    context.executeNAMECALL = executeNAMECALL;
    context.executeFORGPREP = executeFORGPREP;
    context.executeGETVARARGSMultRet = executeGETVARARGSMultRet;
    context.executeGETVARARGSConst = executeGETVARARGSConst;
    context.executeDUPCLOSURE = executeDUPCLOSURE;
    context.executePREPVARARGS = executePREPVARARGS;
    context.executeSETLIST = executeSETLIST;
}

} // namespace CodeGen
} // namespace Luau
