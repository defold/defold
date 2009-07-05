import ctypes, os, sys

if sys.platform == "darwin":
    libname = "libdlib_shared.dylib"
elif sys.platform == "linux2":
    libname = "libdlib_shared.so"
elif sys.platform == "win32":
    libname = "dlib_shared.dll"

dlib = ctypes.cdll.LoadLibrary(os.path.join(os.environ['DYNAMO_HOME'], "lib", libname))

dlib.dmHashBuffer32.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
dlib.dmHashBuffer32.restype = ctypes.c_uint32

dlib.dmHashBuffer64.argtypes = [ctypes.c_char_p, ctypes.c_uint32]
dlib.dmHashBuffer64.restype = ctypes.c_uint64

def dmHashBuffer32(buf):
    return dlib.dmHashBuffer32(buf, len(buf)) 

def dmHashBuffer64(buf):
    return dlib.dmHashBuffer64(buf, len(buf))
 