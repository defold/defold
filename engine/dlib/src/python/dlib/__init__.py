# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import ctypes, os, sys, platform

# NOTE: The output here is parsed later on, so don't print invalid code!

if platform.architecture()[0] == '32bit':
    raise Exception("32 bit hosts are not supported!")

machine = platform.machine() # x86_64 or arm64
bindir = None
if sys.platform == "darwin":
    libname = "libdlib_shared.dylib"
    libdir = "lib/%s-macos" % machine
elif sys.platform in ("linux", "linux2"): # support both python3 and python2
    libname = "libdlib_shared.so"
    if machine == 'aarch64':
        libdir = "lib/arm64-linux"
    else:
        libdir = "lib/x86_64-linux"
elif sys.platform == "win32":
    libname = "dlib_shared.dll"
    libdir = "lib/x86_64-win32"
    bindir = "bin/x86_64-win32"

dlib = None
load_error = None
override_path = os.environ.get('DM_DLIB_SHARED_LIBRARY')
if override_path:
    try:
        dlib = ctypes.cdll.LoadLibrary(override_path)
    except OSError as e:
        load_error = e

try:
    # First try to load from the build directory
    # This is only used when running unit-tests. A bit budget but is works.
    if not dlib:
        dlib = ctypes.cdll.LoadLibrary(os.path.join('build/default/src', libname))
except OSError as e:
    load_error = e

if not dlib:
    # If not found load from default location in DYNAMO_HOME
    dynamo_home = os.environ.get('DYNAMO_HOME')
    candidates = []
    if dynamo_home:
        candidates = [os.path.join(dynamo_home, libdir, libname)]
        if bindir:
            candidates.append(os.path.join(dynamo_home, bindir, libname))

    for candidate in candidates:
        try:
            dlib = ctypes.cdll.LoadLibrary(candidate)
            break
        except OSError as e:
            load_error = e

if dlib:
    dlib.dmHashBuffer32.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
    dlib.dmHashBuffer32.restype = ctypes.c_uint32

    dlib.dmHashBuffer64.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
    dlib.dmHashBuffer64.restype = ctypes.c_uint64

    # DM_DLLEXPORT int _MaxCompressedSize(int uncompressed_size, int* max_compressed_size)
    dlib.LZ4MaxCompressedSize.argtypes = [ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
    dlib.LZ4MaxCompressedSize.restype = ctypes.c_int

    # DM_DLLEXPORT int _CompressBuffer(const void* buffer, uint32_t buffer_size, void* compressed_buffer, int* compressed_size)
    dlib.LZ4CompressBuffer.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    dlib.LZ4CompressBuffer.restype = ctypes.c_int

    # DM_DLLEXPORT int _DecompressBuffer(const void* buffer, uint32_t buffer_size, void* decompressed_buffer, uint32_t max_output, int* decompressed_size)
    dlib.LZ4DecompressBuffer.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_int)]
    dlib.LZ4DecompressBuffer.restype = ctypes.c_int

    # int EncryptXTeaCTR(uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    dlib.EncryptXTeaCTR.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
    dlib.EncryptXTeaCTR.restype = ctypes.c_int

    # int DecryptXTeaCTR(uint8_t* data, uint32_t datalen, const uint8_t* key, uint32_t keylen)
    dlib.DecryptXTeaCTR.argtypes = [ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32]
    dlib.DecryptXTeaCTR.restype = ctypes.c_int


def _to_bytes(buf):
    if isinstance(buf, bytes):
        return buf
    if isinstance(buf, bytearray):
        return bytes(buf)
    return buf.encode('ascii')

def _byte_value(data, index):
    value = data[index]
    if isinstance(value, int):
        return value
    return ord(value)

def _mmix32(h, k):
    m = 0x5bd1e995
    r = 24
    mask = 0xffffffff
    k = (k * m) & mask
    k ^= k >> r
    k = (k * m) & mask
    h = (h * m) & mask
    h ^= k
    return h & mask

def _mmix64(h, k):
    m = 0xc6a4a7935bd1e995
    r = 47
    mask = 0xffffffffffffffff
    k = (k * m) & mask
    k ^= k >> r
    k = (k * m) & mask
    h = (h * m) & mask
    h ^= k
    return h & mask

def _hash_buffer32(buf):
    data = _to_bytes(buf)
    data_len = len(data)
    i = 0
    remaining = data_len
    h = 0

    while remaining >= 4:
        k = (_byte_value(data, i) |
             (_byte_value(data, i + 1) << 8) |
             (_byte_value(data, i + 2) << 16) |
             (_byte_value(data, i + 3) << 24))
        h = _mmix32(h, k)
        i += 4
        remaining -= 4

    t = 0
    if remaining >= 3:
        t ^= _byte_value(data, i + 2) << 16
    if remaining >= 2:
        t ^= _byte_value(data, i + 1) << 8
    if remaining >= 1:
        t ^= _byte_value(data, i)

    h = _mmix32(h, t)
    h = _mmix32(h, data_len)

    h ^= h >> 13
    h = (h * 0x5bd1e995) & 0xffffffff
    h ^= h >> 15
    return h & 0xffffffff

def _hash_buffer64(buf):
    data = _to_bytes(buf)
    data_len = len(data)
    i = 0
    remaining = data_len
    h = 0

    while remaining >= 8:
        k = (_byte_value(data, i) |
             (_byte_value(data, i + 1) << 8) |
             (_byte_value(data, i + 2) << 16) |
             (_byte_value(data, i + 3) << 24) |
             (_byte_value(data, i + 4) << 32) |
             (_byte_value(data, i + 5) << 40) |
             (_byte_value(data, i + 6) << 48) |
             (_byte_value(data, i + 7) << 56))
        h = _mmix64(h, k)
        i += 8
        remaining -= 8

    t = 0
    if remaining >= 7:
        t ^= _byte_value(data, i + 6) << 48
    if remaining >= 6:
        t ^= _byte_value(data, i + 5) << 40
    if remaining >= 5:
        t ^= _byte_value(data, i + 4) << 32
    if remaining >= 4:
        t ^= _byte_value(data, i + 3) << 24
    if remaining >= 3:
        t ^= _byte_value(data, i + 2) << 16
    if remaining >= 2:
        t ^= _byte_value(data, i + 1) << 8
    if remaining >= 1:
        t ^= _byte_value(data, i)

    h = _mmix64(h, t)
    h = _mmix64(h, data_len)

    h ^= h >> 47
    h = (h * 0xc6a4a7935bd1e995) & 0xffffffffffffffff
    h ^= h >> 47
    return h & 0xffffffffffffffff

def _require_dlib(function_name):
    if not dlib:
        raise OSError("Native dlib shared library is required for %s but could not be loaded: %s" % (function_name, load_error))

def dmHashBuffer32(buf):
    data = _to_bytes(buf)
    if dlib:
        return dlib.dmHashBuffer32(data, len(data))
    return _hash_buffer32(data)

def dmHashBuffer64(buf):
    data = _to_bytes(buf)
    if dlib:
        return dlib.dmHashBuffer64(data, len(data))
    return _hash_buffer64(data)

def dmLZ4MaxCompressedSize(uncompressed_size):
    _require_dlib('dmLZ4MaxCompressedSize')
    mcs = ctypes.c_int()
    res = dlib.LZ4MaxCompressedSize(uncompressed_size, ctypes.byref(mcs))
    if res != 0:
        raise Exception('dlib.LZ4MaxCompressedSize failed! Error code: ' % res)
    return mcs.value

def dmLZ4CompressBuffer(buf, buf_len, max_out_len):
    _require_dlib('dmLZ4CompressBuffer')
    outbuf = ctypes.create_string_buffer(max_out_len)
    outlen = ctypes.c_int()
    res = dlib.LZ4CompressBuffer(buf, buf_len, outbuf, ctypes.byref(outlen))
    if res != 0:
        raise Exception('dlib.LZ4CompressBuffer failed! Error code: ' % res)
    return ctypes.string_at(outbuf.raw, outlen.value)

def dmLZ4DecompressBuffer(buf, max_out_len):
    _require_dlib('dmLZ4DecompressBuffer')
    outbuf = ctypes.create_string_buffer(max_out_len)
    outlen = ctypes.c_int()
    res = dlib.LZ4DecompressBuffer(buf, len(buf), outbuf, max_out_len, ctypes.byref(outlen))
    if res != 0:
        raise Exception('dlib.LZ4DecompressBuffer failed! Error code: ' % res)
    return ctypes.string_at(outbuf.raw, outlen.value)

def dmEncryptXTeaCTR(buf, key):
    _require_dlib('dmEncryptXTeaCTR')
    outbuf = ctypes.create_string_buffer(buf)
    res = dlib.EncryptXTeaCTR(outbuf, len(outbuf), key, len(key))
    if res != 0:
        raise Exception('dlib.EncryptXTeaCTR failed! Error code: ' % res)

    return ctypes.string_at(outbuf.raw, len(buf))

def dmDecryptXTeaCTR(buf, key):
    _require_dlib('dmDecryptXTeaCTR')
    outbuf = ctypes.create_string_buffer(buf)
    res = dlib.DecryptXTeaCTR(outbuf, len(outbuf), key, len(key))
    if res != 0:
        raise Exception('dlib.DecryptXTeaCTR failed! Error code: ' % res)

    return ctypes.string_at(outbuf.raw, len(buf))
