import ctypes, os, sys

if sys.platform == "darwin":
    libname = "libdlib_shared.dylib"
elif sys.platform == "linux2":
    libname = "libdlib_shared.so"
elif sys.platform == "win32":
    libname = "libdlib_shared.dll"

dlib = ctypes.cdll.LoadLibrary(os.path.join(os.environ['DYNAMO_HOME'], "lib", libname))

dlib.HashBuffer32.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
dlib.HashBuffer32.restype = ctypes.c_uint32

dlib.HashBuffer64.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
dlib.HashBuffer64.restype = ctypes.c_uint64

def HashBuffer32(buf):
    return dlib.HashBuffer32(buf, len(buf)) 

def HashBuffer64(buf):
    return dlib.HashBuffer64(buf, len(buf))
 