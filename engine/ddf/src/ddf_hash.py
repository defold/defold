#!/usr/bin/env python3

LICENSE="""
// Copyright 2020-2025 The Defold Foundation
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
"""


def mmix32(h, k, m, r):
    k = (k * m) & 0xFFFFFFFF
    k ^= (k >> r)
    k = (k * m) & 0xFFFFFFFF
    h = (h * m) & 0xFFFFFFFF
    h ^= k
    return h

def mmix64(h, k, m, r):
    k = (k * m) & 0xFFFFFFFFFFFFFFFF
    k ^= (k >> r)
    k = (k * m) & 0xFFFFFFFFFFFFFFFF
    h = (h * m) & 0xFFFFFFFFFFFFFFFF
    h ^= k
    return h

def dmHashBufferNoReverse32(key, seed: int = 0):
    if isinstance(key, str):
        key = key.encode("utf-8")
    m = 0x5bd1e995
    r = 24
    length = len(key)
    h = seed & 0xFFFFFFFF
    i = 0

    while i + 4 <= length:
        k = (
            key[i]
            | (key[i+1] << 8)
            | (key[i+2] << 16)
            | (key[i+3] << 24)
        ) & 0xFFFFFFFF

        h = mmix32(h, k, m, r)
        i += 4

    t = 0
    rem = length - i
    if rem >= 3:
        t ^= key[i+2] << 16
    if rem >= 2:
        t ^= key[i+1] << 8
    if rem >= 1:
        t ^= key[i]

    h = mmix32(h, t, m, r)
    h = mmix32(h, length, m, r)

    h ^= (h >> 13)
    h = (h * m) & 0xFFFFFFFF
    h ^= (h >> 15)

    return h


def dmHashBufferNoReverse64(key, seed: int = 0):
    if isinstance(key, str):
        key = key.encode("utf-8")
    m = 0xc6a4a7935bd1e995
    r = 47
    length = len(key)
    h = seed & 0xFFFFFFFFFFFFFFFF
    i = 0

    while i + 8 <= length:
        k = (
            key[i]
            | (key[i+1] << 8)
            | (key[i+2] << 16)
            | (key[i+3] << 24)
            | (key[i+4] << 32)
            | (key[i+5] << 40)
            | (key[i+6] << 48)
            | (key[i+7] << 56)
        ) & 0xFFFFFFFFFFFFFFFF

        h = mmix64(h, k, m, r)
        i += 8

    t = 0
    rem = length - i
    if rem >= 7:
        t ^= key[i+6] << 48
    if rem >= 6:
        t ^= key[i+5] << 40
    if rem >= 5:
        t ^= key[i+4] << 32
    if rem >= 4:
        t ^= key[i+3] << 24
    if rem >= 3:
        t ^= key[i+2] << 16
    if rem >= 2:
        t ^= key[i+1] << 8
    if rem >= 1:
        t ^= key[i]

    h = mmix64(h, t, m, r)
    h = mmix64(h, length, m, r)

    h ^= (h >> r)
    h = (h * m) & 0xFFFFFFFFFFFFFFFF
    h ^= (h >> r)

    return h
