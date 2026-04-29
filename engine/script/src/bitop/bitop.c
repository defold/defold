/*
** Lua BitOp -- a bit operations library for Lua 5.1/5.2.
** http://bitop.luajit.org/
**
** Copyright (C) 2008-2012 Mike Pall. All rights reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining
** a copy of this software and associated documentation files (the
** "Software"), to deal in the Software without restriction, including
** without limitation the rights to use, copy, modify, merge, publish,
** distribute, sublicense, and/or sell copies of the Software, and to
** permit persons to whom the Software is furnished to do so, subject to
** the following conditions:
**
** The above copyright notice and this permission notice shall be
** included in all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
** EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
** MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
** IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
** CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
** SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**
** [ MIT license: http://www.opensource.org/licenses/mit-license.php ]
*/

#include "bitop.h"
#include <dmsdk/dlua/dlua.h>

#ifdef _MSC_VER
/* MSVC is stuck in the last century and doesn't have C99's stdint.h. */
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

typedef int32_t SBits;
typedef uint32_t UBits;

typedef union {
    dlua_Number n;
    uint64_t b;
} BitNum;

/* Convert argument to bit type. */
static UBits barg(dlua_State *L, int idx)
{
    BitNum bn;
    UBits b;
    bn.n = dlua_tonumber(L, idx);
    bn.n += 6755399441055744.0;    /* 2^52+2^51 */
#ifdef SWAPPED_DOUBLE
    b = (UBits)(bn.b >> 32);
#else
    b = (UBits)bn.b;
#endif
    if (b == 0 && !dlua_isnumber(L, idx)) {
        dluaL_typerror(L, idx, "number");
    }
    return b;
}

/* Return bit type. */
#define BRET(b)    dlua_pushnumber(L, (dlua_Number)(SBits)(b)); return 1;

static int bit_tobit(dlua_State *L) { BRET(barg(L, 1)) }
static int bit_bnot(dlua_State *L) { BRET(~barg(L, 1)) }

#define BIT_OP(func, opr) \
    static int func(dlua_State *L) { int i; UBits b = barg(L, 1); \
        for (i = dlua_gettop(L); i > 1; i--) b opr barg(L, i); BRET(b) }
BIT_OP(bit_band, &=)
BIT_OP(bit_bor, |=)
BIT_OP(bit_bxor, ^=)

#define bshl(b, n)    (b << n)
#define bshr(b, n)    (b >> n)
#define bsar(b, n)    ((SBits)b >> n)
#define brol(b, n)    ((b << n) | (b >> (32-n)))
#define bror(b, n)    ((b << (32-n)) | (b >> n))
#define BIT_SH(func, fn) \
    static int func(dlua_State *L) { \
        UBits b = barg(L, 1); UBits n = barg(L, 2) & 31; BRET(fn(b, n)) }
BIT_SH(bit_lshift, bshl)
BIT_SH(bit_rshift, bshr)
BIT_SH(bit_arshift, bsar)
BIT_SH(bit_rol, brol)
BIT_SH(bit_ror, bror)

static int bit_bswap(dlua_State *L)
{
    UBits b = barg(L, 1);
    b = (b >> 24) | ((b >> 8) & 0xff00) | ((b & 0xff00) << 8) | (b << 24);
    BRET(b)
}

static int bit_tohex(dlua_State *L)
{
    UBits b = barg(L, 1);
    SBits n = dlua_isnone(L, 2) ? 8 : (SBits)barg(L, 2);
    const char *hexdigits = "0123456789abcdef";
    char buf[8];
    int i;
    if (n < 0) { n = -n; hexdigits = "0123456789ABCDEF"; }
    if (n > 8) n = 8;
    for (i = (int)n; --i >= 0; ) { buf[i] = hexdigits[b & 15]; b >>= 4; }
    dlua_pushlstring(L, buf, (size_t)n);
    return 1;
}

static const struct dluaL_Reg bit_funcs[] = {
    { "tobit",	bit_tobit },
    { "bnot",	bit_bnot },
    { "band",	bit_band },
    { "bor",	bit_bor },
    { "bxor",	bit_bxor },
    { "lshift",	bit_lshift },
    { "rshift",	bit_rshift },
    { "arshift",	bit_arshift },
    { "rol",	bit_rol },
    { "ror",	bit_ror },
    { "bswap",	bit_bswap },
    { "tohex",	bit_tohex },
    { NULL, NULL }
};

/* Signed right-shifts are implementation-defined per C89/C99.
** But the de facto standard are arithmetic right-shifts on two's
** complement CPUs. This behaviour is required here, so test for it.
*/
#define BAD_SAR		(bsar(-8, 2) != (SBits)-2)

DLUA_API int luaopen_bit(dlua_State *L)
{
    UBits b;
    dlua_pushnumber(L, (dlua_Number)1437217655L);
    b = barg(L, -1);
    if (b != (UBits)1437217655L || BAD_SAR) {    /* Perform a simple self-test. */
        const char *msg = "compiled with incompatible luaconf.h";
#ifdef _WIN32
        if (b == (UBits)1610612736L)
            msg = "use D3DCREATE_FPU_PRESERVE with DirectX";
#endif
        if (b == (UBits)1127743488L)
            msg = "not compiled with SWAPPED_DOUBLE";
        if (BAD_SAR)
            msg = "arithmetic right-shift broken";
        dluaL_error(L, "bit library self-test failed (%s)", msg);
    }
    dluaL_register(L, "bit", bit_funcs);
    return 1;
}
