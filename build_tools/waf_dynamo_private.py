import sys, os
import Options

# we don't want to look for the msvc compiler when cross compiling (it takes 10+ seconds!)
def _dont_look_for_msvc():
    if 'msvc' in Options.options.check_c_compiler:
        Options.options.check_c_compiler = 'gcc'
    if 'msvc' in Options.options.check_cxx_compiler:
        Options.options.check_cxx_compiler = 'g++'

def _set_ccflags(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CCFLAGS', flag)
        conf.env.append_unique('CXXFLAGS', flag)

def _set_cxxflags(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CXXFLAGS', flag)

def _set_linkflags(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('LINKFLAGS', flag)

def _set_libpath(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('LIBPATH', flag)

def _set_includes(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CPPPATH', flag)

def _set_defines(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CCDEFINES', flag)
        conf.env.append_unique('CXXDEFINES', flag)

#*******************************************************************************************************
# NINTENDO SWITCH -->
#*******************************************************************************************************

def setup_tools_nx(conf, build_util):

    # until we have the package on S3
    NINTENDO_SDK_ROOT = os.environ['NINTENDO_SDK_ROOT']

    # If the path isn't formatted ok, the gxx:init_cxx()->ccroot.get_cc_version will throw a very silent exception
    #  error: could not determine the compiler version ['C:NintendoSDKNintendoSDK/Compilers/NX/nx/aarch64/bin/clang++', '-dM', '-E', '-']
    NINTENDO_SDK_ROOT = NINTENDO_SDK_ROOT.replace('\\', '/')
    os.environ['NINTENDO_SDK_ROOT'] = NINTENDO_SDK_ROOT

    bin_folder          = "%s/Compilers/NX/nx/aarch64/bin" % NINTENDO_SDK_ROOT
    os.environ['CC']    = "%s/clang" % bin_folder
    os.environ['CXX']   = "%s/clang++" % bin_folder
    conf.env['CC']      = '%s/clang' % bin_folder
    conf.env['CXX']     = '%s/clang++' % bin_folder
    conf.env['LINK_CXX']= '%s/clang++' % bin_folder
    conf.env['CPP']     = '%s/clang -E' % bin_folder
    conf.env['AR']      = '%s/llvm-ar' % bin_folder
    conf.env['RANLIB']  = '%s/llvm-ranlib' % bin_folder
    conf.env['LD']      = '%s/lld' % bin_folder

    #CHECK_C_COMPILER

    commandline_folder          = "%s/Tools/CommandLineTools" % NINTENDO_SDK_ROOT
    conf.env['AUTHORINGTOOL']   = '%s/AuthoringTool/AuthoringTool.exe' % commandline_folder
    conf.env['MAKEMETA']        = '%s/MakeMeta/MakeMeta.exe' % commandline_folder
    conf.env['MAKENSO']         = '%s/MakeNso/MakeNso.exe' % commandline_folder

def setup_vars_nx(conf, build_util):
    
    #print "MAWE", "setup_vars_nx"

    # until we have the package on S3
    NINTENDO_SDK_ROOT = os.environ['NINTENDO_SDK_ROOT']

    BUILDTARGET = 'NX-NXFP2-a64'
    BUILDTYPE = "Debug"

    CCFLAGS="-mcpu=cortex-a57+fp+simd+crypto+crc -fno-common -fno-short-enums -ffunction-sections -fdata-sections -fPIC -fdiagnostics-format=msvc"
    CXXFLAGS="-fno-rtti -std=gnu++14 "

    _set_ccflags(conf, CCFLAGS)
    _set_cxxflags(conf, CXXFLAGS)

    DEFINES = ""
    DEFINES+= "NN_SDK_BUILD_%s" % BUILDTYPE.upper()
    _set_defines(conf, DEFINES)

    LINKFLAGS ="-nostartfiles -Wl,--gc-sections -Wl,--build-id=sha1 -Wl,-init=_init -Wl,-fini=_fini -Wl,-pie -Wl,-z,combreloc"
    LINKFLAGS+=" -Wl,-z,relro -Wl,--enable-new-dtags -Wl,-u,malloc -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,aligned_alloc -Wl,-u,free"
    LINKFLAGS+=" -fdiagnostics-format=msvc -Wl,-T C:/Nintendo/SDK/NintendoSDK/Resources/SpecFiles/Application.aarch64.lp64.ldscript"
    LINKFLAGS+=" -Wl,--start-group "
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/rocrt.o" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/nnApplication.o" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_init_memory.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_gll.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_gfx.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_mii_draw.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" -Wl,--end-group"
    LINKFLAGS+=" -Wl,--start-group"
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nss" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" -Wl,--end-group"
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/crtend.o" % NINTENDO_SDK_ROOT
    _set_linkflags(conf, LINKFLAGS)

    LIBPATHS ="%s/Libraries/%s/%s" % (NINTENDO_SDK_ROOT, BUILDTARGET, BUILDTYPE)
    _set_libpath(conf, LIBPATHS)

    CPPPATH ="%s/Common/Configs/Targets/%s/Include" % (NINTENDO_SDK_ROOT, BUILDTARGET)
    CPPPATH+=" %s/Include" % (NINTENDO_SDK_ROOT,)
    _set_includes(conf, CPPPATH)

    #print "MAWE", "setup_vars_nx"
    #print conf.env



#*******************************************************************************************************
# <-- NINTENDO SWITCH
#*******************************************************************************************************

# setup any extra options
def set_options(opt):
    pass

# setup the CC et al
def setup_tools(conf, build_util):
    print "MAWE", "setup_tools"

    if build_util.get_target_platform() in ['arm64-nx64']:
        _dont_look_for_msvc()
        setup_tools_nx(conf, build_util)

# Modify any variables after the cc+cxx compiler tools have been loaded
def setup_vars(task_gen, build_util):
    #print "MAWE", "setup_vars", task_gen.name

    if build_util.get_target_platform() in ['arm64-nx64']:
        setup_vars_nx(task_gen, build_util)


def is_platform_private(platform):
    return platform in ['arm64-nx64']