import ctypes, os, sys, platform

if sys.platform == "darwin":
    libname = "libdlib_shared.dylib"
    if platform.architecture()[0] == '64bit':
        libdir = "lib/x86_64-darwin"
    else:
        libdir = "lib/darwin"
elif sys.platform == "linux2":
    libname = "libdlib_shared.so"
    if platform.architecture()[0] == '64bit':
        libdir = "lib/x86_64-linux"
    else:
        libdir = "lib/linux"
elif sys.platform == "win32":
    libname = "dlib_shared.dll"
    libdir = "lib/win32"

dlib = None
try:
    # First try to load from the build directory
    # This is only used when running unit-tests. A bit budget but is works.
    dlib = ctypes.cdll.LoadLibrary(os.path.join('build/default/src', libname))
except:
    pass

if not dlib:
    # If not found load from default location in DYNAMO_HOME
    dlib = ctypes.cdll.LoadLibrary(os.path.join(os.environ['DYNAMO_HOME'], libdir, libname))

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


def dmHashBuffer32(buf):
    return dlib.dmHashBuffer32(buf, len(buf))

def dmHashBuffer64(buf):
    return dlib.dmHashBuffer64(buf, len(buf))

def dmLZ4MaxCompressedSize(uncompressed_size):
    mcs = ctypes.c_int()
    res = dlib.LZ4MaxCompressedSize(uncompressed_size, ctypes.byref(mcs))
    if res != 0:
        raise Exception('dlib.LZ4MaxCompressedSize failed! Error code: ' % res)
    return mcs.value

def dmLZ4CompressBuffer(buf, buf_len, max_out_len):
    outbuf = ctypes.create_string_buffer(max_out_len)
    outlen = ctypes.c_int()
    res = dlib.LZ4CompressBuffer(buf, buf_len, outbuf, ctypes.byref(outlen))
    if res != 0:
        raise Exception('dlib.LZ4CompressBuffer failed! Error code: ' % res)
    return ctypes.string_at(outbuf.raw, outlen.value)

def dmLZ4DecompressBuffer(buf, max_out_len):
    outbuf = ctypes.create_string_buffer(max_out_len)
    outlen = ctypes.c_int()
    res = dlib.LZ4DecompressBuffer(buf, len(buf), outbuf, max_out_len, ctypes.byref(outlen))
    if res != 0:
        raise Exception('dlib.LZ4DecompressBuffer failed! Error code: ' % res)
    return ctypes.string_at(outbuf.raw, outlen.value)

def dmEncryptXTeaCTR(buf, key):
    outbuf = ctypes.create_string_buffer(buf)
    res = dlib.EncryptXTeaCTR(outbuf, len(outbuf), key, len(key))
    if res != 0:
        raise Exception('dlib.EncryptXTeaCTR failed! Error code: ' % res)

    return ctypes.string_at(outbuf.raw, len(buf))

def dmDecryptXTeaCTR(buf, key):
    outbuf = ctypes.create_string_buffer(buf)
    res = dlib.DecryptXTeaCTR(outbuf, len(outbuf), key, len(key))
    if res != 0:
        raise Exception('dlib.DecryptXTeaCTR failed! Error code: ' % res)

    return ctypes.string_at(outbuf.raw, len(buf))
