import ctypes, os, sys

if sys.platform == "darwin":
    libname = "libdlib_shared.dylib"
    libdir = "lib"
elif sys.platform == "linux2":
    libname = "libdlib_shared.so"
    libdir = "lib"
elif sys.platform == "win32":
    libname = "dlib_shared.dll"
    libdir = "lib"

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

def dmHashBuffer32(buf):
    return dlib.dmHashBuffer32(buf, len(buf))

def dmHashBuffer64(buf):
    return dlib.dmHashBuffer64(buf, len(buf))
