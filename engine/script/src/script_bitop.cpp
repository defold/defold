// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include "script.h"
#include "script_private.h"

extern "C"
{
#include "bitop/bitop.h"
}

/*# Bitwise operations API documentation
 *
 * [Lua BitOp](http://bitop.luajit.org/api.html) is a C extension module for Lua 5.1/5.2 which adds bitwise operations on numbers.
 *
 * Lua BitOp is Copyright &copy; 2008-2012 Mike Pall.
 * Lua BitOp is free software, released under the MIT license (same license as the Lua core).
 *
 * Lua BitOp is compatible with the built-in bitwise operations in LuaJIT 2.0 and is used
 * on platforms where Defold runs without LuaJIT.
 *
 * For clarity the examples assume the definition of a helper function `printx()`.
 * This prints its argument as an unsigned 32 bit hexadecimal number on all platforms:
 *
 * ```lua
 * function printx(x)
 *   print("0x"..bit.tohex(x))
 * end
 * ```
 *
 * @document
 * @name BitOp
 * @namespace bit
 * @language Lua
 */

/*# normalize number to the numeric range for bit operations
 *
 * Normalizes a number to the numeric range for bit operations and returns it. This function is usually not needed since all bit operations already normalize all of their input arguments.
 *
 * @name bit.tobit
 * @param x [type:number] number to normalize
 * @return y [type:number] normalized number
 * @examples
 *
 * ```lua
 * print(0xffffffff)                --> 4294967295 (*)
 * print(bit.tobit(0xffffffff))     --> -1
 * printx(bit.tobit(0xffffffff))    --> 0xffffffff
 * print(bit.tobit(0xffffffff + 1)) --> 0
 * print(bit.tobit(2^40 + 1234))    --> 1234
 * ```
 *
 * (*) See the treatment of hex literals for an explanation why the printed numbers in the first two lines differ (if your Lua installation uses a double number type).
 */

/*# convert number to a hex string
 *
* Converts its first argument to a hex string. The number of hex digits is given by the absolute value of the optional second argument. Positive numbers between 1 and 8 generate lowercase hex digits. Negative numbers generate uppercase hex digits. Only the least-significant 4*|n| bits are used. The default is to generate 8 lowercase hex digits.
 *
 * @name bit.tohex
 * @param x [type:number] number to convert
 * @param n [type:number] number of hex digits to return
 * @return s [type:string] hexadecimal string
 * @examples
 *
 * ```lua
 * print(bit.tohex(1))              --> 00000001
 * print(bit.tohex(-1))             --> ffffffff
 * print(bit.tohex(0xffffffff))     --> ffffffff
 * print(bit.tohex(-1, -8))         --> FFFFFFFF
 * print(bit.tohex(0x21, 4))        --> 0021
 * print(bit.tohex(0x87654321, 4))  --> 4321
 * ```
 */


/*# bitwise not
 *
 * Returns the bitwise not of its argument.
 *
 * @name bit.bnot
 * @param x [type:number] number
 * @return y [type:number] bitwise not of number x
 * @examples
 *
 * ```lua
 * print(bit.bnot(0))            --> -1
 * printx(bit.bnot(0))           --> 0xffffffff
 * print(bit.bnot(-1))           --> 0
 * print(bit.bnot(0xffffffff))   --> 0
 * printx(bit.bnot(0x12345678))  --> 0xedcba987
 * ```
 *
 */

/*# bitwise or
 *
 * Returns the bitwise or of all of its arguments. Note that more than two arguments are allowed.
 *
 * @name bit.bor
 * @param x1 [type:number] number
 * @param [x2...] [type:number] number(s)
 * @return y [type:number] bitwise or of the provided arguments
 * @examples
 *
 * ```lua
 * print(bit.bor(1, 2, 4, 8))                --> 15
 * ```
 *
 */

/*# bitwise and
 *
 * Returns the bitwise and of all of its arguments. Note that more than two arguments are allowed.
 *
 * @name bit.band
 * @param x1 [type:number] number
 * @param [x2...] [type:number] number(s)
 * @return y [type:number] bitwise and of the provided arguments
 * @examples
 *
 * ```lua
 * printx(bit.band(0x12345678, 0xff))        --> 0x00000078
 * ```
 *
 */

/*# bitwise xor
 *
 * Returns the bitwise xor of all of its arguments. Note that more than two arguments are allowed.
 *
 * @name bit.bxor
 * @param x1 [type:number] number
 * @param [x2...] [type:number] number(s)
 * @return y [type:number] bitwise xor of the provided arguments
 * @examples
 *
 * ```lua
 * printx(bit.bxor(0xa5a5f0f0, 0xaa55ff00))  --> 0x0ff00ff0
 * ```
 *
 */

/*# bitwise logical left-shift
 *
 * Returns the bitwise logical left-shift of its first argument by the number of bits given by the second argument.
 * Logical shifts treat the first argument as an unsigned number and shift in 0-bits.
 * Only the lower 5 bits of the shift count are used (reduces to the range [0..31]).
 *
 * @name bit.lshift
 * @param x [type:number] number
 * @param n [type:number] number of bits
 * @return y [type:number] bitwise logical left-shifted number
 * @examples
 *
 * ```lua
 * print(bit.lshift(1, 0))              --> 1
 * print(bit.lshift(1, 8))              --> 256
 * print(bit.lshift(1, 40))             --> 256
 * printx(bit.lshift(0x87654321, 12))   --> 0x54321000
 * ```
 *
 */

/*# bitwise logical right-shift
 *
 * Returns the bitwise logical right-shift of its first argument by the number of bits given by the second argument.
 * Logical shifts treat the first argument as an unsigned number and shift in 0-bits.
 * Only the lower 5 bits of the shift count are used (reduces to the range [0..31]).
 *
 * @name bit.rshift
 * @param x [type:number] number
 * @param n [type:number] number of bits
 * @return y [type:number] bitwise logical right-shifted number
 * @examples
 *
 * ```lua
 * print(bit.rshift(256, 8))            --> 1
 * print(bit.rshift(-256, 8))           --> 16777215
 * printx(bit.rshift(0x87654321, 12))   --> 0x00087654
 * ```
 *
 */

/*# bitwise arithmetic right-shift
 *
 * Returns the bitwise arithmetic right-shift of its first argument by the number of bits given by the second argument.
 * Arithmetic right-shift treats the most-significant bit as a sign bit and replicates it.
 * Only the lower 5 bits of the shift count are used (reduces to the range [0..31]).
 *
 * @name bit.arshift
 * @param x [type:number] number
 * @param n [type:number] number of bits
 * @return y [type:number] bitwise arithmetic right-shifted number
 * @examples
 *
 * ```lua
 * print(bit.arshift(256, 8))           --> 1
 * print(bit.arshift(-256, 8))          --> -1
 * printx(bit.arshift(0x87654321, 12))  --> 0xfff87654
 * ```
 *
 */

/*# bitwise left rotation
 *
 * Returns the bitwise left rotation of its first argument by the number of bits given by the second argument. Bits shifted out on one side are shifted back in on the other side.
 * Only the lower 5 bits of the rotate count are used (reduces to the range [0..31]).
 *
 * @name bit.rol
 * @param x [type:number] number
 * @param n [type:number] number of bits
 * @return y [type:number] bitwise left-rotated number
 * @examples
 *
 * ```lua
 * printx(bit.rol(0x12345678, 12))   --> 0x45678123
 * ```
 *
 */

/*# bitwise right rotation
 *
 * Returns the bitwise right rotation of its first argument by the number of bits given by the second argument. Bits shifted out on one side are shifted back in on the other side.
 * Only the lower 5 bits of the rotate count are used (reduces to the range [0..31]).
 *
 * @name bit.ror
 * @param x [type:number] number
 * @param n [type:number] number of bits
 * @return y [type:number] bitwise right-rotated number
 * @examples
 *
 * ```lua
 * printx(bit.ror(0x12345678, 12))   --> 0x67812345
 * ```
 *
 */

/*# bitwise swap
 *
 * Swaps the bytes of its argument and returns it. This can be used to convert little-endian 32 bit numbers to big-endian 32 bit numbers or vice versa.
 *
 * @name bit.bswap
 * @param x [type:number] number
 * @return y [type:number] bitwise swapped number
 * @examples
 *
 * ```lua
 * printx(bit.bswap(0x12345678)) --> 0x78563412
 * printx(bit.bswap(0x78563412)) --> 0x12345678
 * ```
 *
 */

namespace dmScript
{
    void InitializeBitop(lua_State* L)
    {
        int top = lua_gettop(L);

        luaopen_bit(L);

        int stack = lua_gettop(L);

        // Above call leaves a table and a number on the stack which will not
        // be needed for anything.
        lua_pop(L, stack - top);
    }
}
