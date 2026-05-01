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

#ifndef DMSDK_ENDIAN_HPP
#define DMSDK_ENDIAN_HPP

#include <dmsdk/dlib/endian.h>

/*# Endian C++ API documentation
 * C++ overloads for endian conversion functions.
 *
 * @document
 * @name Endian
 * @namespace dmEndian
 * @language C++
 */

namespace dmEndian
{
    /*# swap bytes in a 16-bit value
     * @name dmEndian::ByteSwap
     * @param x [type:uint16_t] Value to byte swap
     * @return value [type:uint16_t] Byte-swapped value
     */
    static inline uint16_t ByteSwap(uint16_t x)
    {
        return EndianSwap16(x);
    }

    /*# swap bytes in a 32-bit value
     * @name dmEndian::ByteSwap
     * @param x [type:uint32_t] Value to byte swap
     * @return value [type:uint32_t] Byte-swapped value
     */
    static inline uint32_t ByteSwap(uint32_t x)
    {
        return EndianSwap32(x);
    }

    /*# swap bytes in a 64-bit value
     * @name dmEndian::ByteSwap
     * @param x [type:uint64_t] Value to byte swap
     * @return value [type:uint64_t] Byte-swapped value
     */
    static inline uint64_t ByteSwap(uint64_t x)
    {
        return EndianSwap64(x);
    }

    /*# convert a 16-bit host value to network byte order
     * @name dmEndian::ToNetwork
     * @param x [type:uint16_t] Host-order value
     * @return value [type:uint16_t] Network-order value
     */
    static inline uint16_t ToNetwork(uint16_t x)
    {
        return EndianToNetwork16(x);
    }

    /*# convert a 16-bit network value to host byte order
     * @name dmEndian::ToHost
     * @param x [type:uint16_t] Network-order value
     * @return value [type:uint16_t] Host-order value
     */
    static inline uint16_t ToHost(uint16_t x)
    {
        return EndianToHost16(x);
    }

    /*# convert a 32-bit host value to network byte order
     * @name dmEndian::ToNetwork
     * @param x [type:uint32_t] Host-order value
     * @return value [type:uint32_t] Network-order value
     */
    static inline uint32_t ToNetwork(uint32_t x)
    {
        return EndianToNetwork32(x);
    }

    /*# convert a 32-bit network value to host byte order
     * @name dmEndian::ToHost
     * @param x [type:uint32_t] Network-order value
     * @return value [type:uint32_t] Host-order value
     */
    static inline uint32_t ToHost(uint32_t x)
    {
        return EndianToHost32(x);
    }

    /*# convert a 64-bit host value to network byte order
     * @name dmEndian::ToNetwork
     * @param x [type:uint64_t] Host-order value
     * @return value [type:uint64_t] Network-order value
     */
    static inline uint64_t ToNetwork(uint64_t x)
    {
        return EndianToNetwork64(x);
    }

    /*# convert a 64-bit network value to host byte order
     * @name dmEndian::ToHost
     * @param x [type:uint64_t] Network-order value
     * @return value [type:uint64_t] Host-order value
     */
    static inline uint64_t ToHost(uint64_t x)
    {
        return EndianToHost64(x);
    }
}

#endif // DMSDK_ENDIAN_HPP
