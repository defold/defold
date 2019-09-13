import os, sys, subprocess, shutil, re, stat, glob, zipfile
import Build, Options, Utils, Task, Logs
import Configure
from Configure import conf
from TaskGen import extension, taskgen, feature, after, before
from Logs import error
import cc, cxx
from Constants import RUN_ME
from BuildUtility import BuildUtility, BuildUtilityException, create_build_utility

if not 'DYNAMO_HOME' in os.environ:
    print >>sys.stderr, "You must define DYNAMO_HOME. Have you run './script/build.py shell' ?"
    sys.exit(1)

HOME=os.environ['USERPROFILE' if sys.platform == 'win32' else 'HOME']
# Note: ANDROID_ROOT is the root of the Android SDK, the NDK is put into the DYNAMO_HOME folder with install_ext.
#       It is also defined in the _strip_engine in build.py, so make sure these two paths are the same.
ANDROID_ROOT=os.path.join(HOME, 'android')
ANDROID_BUILD_TOOLS_VERSION = '23.0.2'
ANDROID_NDK_VERSION='20'
ANDROID_NDK_API_VERSION='16' # Android 4.1
ANDROID_NDK_API_VERSION_VULKAN='24'
ANDROID_NDK_ROOT=os.path.join(os.environ['DYNAMO_HOME'], 'ext', 'SDKs','android-ndk-r%s' % ANDROID_NDK_VERSION)
ANDROID_TARGET_API_LEVEL='28' # Android 9.0
ANDROID_MIN_API_LEVEL='14'
ANDROID_GCC_VERSION='4.9'
ANDROID_64_NDK_API_VERSION='21' # Android 5.0
EMSCRIPTEN_ROOT=os.environ.get('EMSCRIPTEN', '')

IOS_SDK_VERSION="12.1"
IOS_SIMULATOR_SDK_VERSION="12.1"
# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
MIN_IOS_SDK_VERSION="8.0"

OSX_SDK_VERSION="10.13"
MIN_OSX_SDK_VERSION="10.7"

XCODE_VERSION="10.1"

DARWIN_TOOLCHAIN_ROOT=os.path.join(os.environ['DYNAMO_HOME'], 'ext', 'SDKs','XcodeDefault%s.xctoolchain' % XCODE_VERSION)

# Workaround for a strange bug with the combination of ccache and clang
# Without CCACHE_CPP2 set breakpoint for source locations can't be set, e.g. b main.cpp:1234
os.environ['CCACHE_CPP2'] = 'yes'

def new_copy_task(name, input_ext, output_ext):
    def compile(task):
        with open(task.inputs[0].srcpath(task.env), 'rb') as in_f:
            with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
                out_f.write(in_f.read())

        return 0

    task = Task.task_type_from_func(name,
                                    func  = compile,
                                    color = 'PINK')

    @extension(input_ext)
    def copy_file(self, node):
        task = self.create_task(name)
        task.set_inputs(node)
        out = node.change_ext(output_ext)
        task.set_outputs(out)

def copy_file_task(bld, src, name=None):
    copy = 'copy /Y' if sys.platform == 'win32' else 'cp'
    parts = src.split('/')
    filename = parts[-1]
    src_path = src.replace('/', os.sep)
    return bld.new_task_gen(rule = '%s %s ${TGT}' % (copy, src_path),
                            target = filename,
                            name = name,
                            shell = True)

#   Extract api docs from source files and store the raw text in .apidoc
#   files per file and namespace for later collation into .json and .sdoc files.
def apidoc_extract_task(bld, src):
    import re
    def _strip_comment_stars(str):
        lines = str.split('\n')
        ret = []
        for line in lines:
            line = line.strip()
            if line.startswith('*'):
                line = line[1:]
                if line.startswith(' '):
                    line = line[1:]
            ret.append(line)
        return '\n'.join(ret)

    def _parse_comment(source):
        str = _strip_comment_stars(source)
        # The regexp means match all strings that:
        # * begins with line start, possible whitespace and an @
        # * followed by non-white-space (the tag)
        # * followed by possible spaces
        # * followed by every character that is not an @ or is an @ but not preceded by a new line (the value)
        lst = re.findall('^\s*@(\S+) *((?:[^@]|(?<!\n)@)*)', str, re.MULTILINE)
        is_document = False
        namespace = None
        for (tag, value) in lst:
            tag = tag.strip()
            value = value.strip()
            if tag == 'document':
                is_document = True
            elif tag == 'namespace':
                namespace = value
        return namespace, is_document

    def ns_elements(source):
        lst = re.findall('/(\*#.*?)\*/', source, re.DOTALL)
        elements = {}
        default_namespace = None
        for comment_str in lst:
            ns, is_doc = _parse_comment(comment_str)
            if ns and is_doc:
                default_namespace = ns
            if not ns:
                ns = default_namespace
            if ns not in elements:
                elements[ns] = []
            elements[ns].append('/' + comment_str + '*/')
        return elements

    import Node
    from itertools import chain
    from collections import defaultdict
    elements = {}
    def extract_docs(bld, src):
        ret = defaultdict(list)
        # Gather data
        for s in src:
            n = bld.path.find_resource(s)
            with open(n.abspath(), 'r') as in_f:
                source = in_f.read()
                for k,v in chain(elements.items(), ns_elements(source).items()):
                    if k == None:
                        print("Missing namespace definition in " + n.abspath())
                    ret[k] = ret[k] + v
        return ret

    def write_docs(task):
        # Write all namespace files
        for o in task.outputs:
            ns = o.file_base()
            with open(o.bldpath(task.env), 'w+') as out_f:
                out_f.write('\n'.join(elements[ns]))

    if not getattr(Options.options, 'skip_apidocs', False):
        elements = extract_docs(bld, src)
        target = []
        for ns in elements.keys():
            target.append(ns + '.apidoc')
        return bld.new_task_gen(rule=write_docs, name='apidoc_extract', source = src, target = target)


# Add single dmsdk file.
# * 'source' file is installed into 'target' directory
# * 'source' file is added to documentation pipeline
def dmsdk_add_file(bld, target, source):
    bld.install_files(target, source)
    apidoc_extract_task(bld, source)

# Add dmsdk files from 'source' recursively.
# * 'source' files are installed into 'target' folder, preserving the hierarchy (subfolders in 'source' is appended to the 'target' path).
# * 'source' files are added to documentation pipeline
def dmsdk_add_files(bld, target, source):
    bld_sdk_files = bld.path.find_dir(source).abspath()
    bld_path = bld.path.abspath()
    doc_files = []
    for root, dirs, files in os.walk(bld_sdk_files):
        for f in files:
            f = os.path.relpath(os.path.join(root, f), bld_path)
            doc_files.append(f)
            sdk_dir = os.path.dirname(os.path.relpath(f, source))
            bld.install_files(os.path.join(target, sdk_dir), f)
    apidoc_extract_task(bld, doc_files)

def getAndroidNDKArch(target_arch):
    return 'arm64' if 'arm64' == target_arch else 'arm'

def getAndroidArch(target_arch):
    return 'arm64-v8a' if 'arm64' == target_arch else 'armeabi-v7a'

def getAndroidBuildtoolName(target_arch):
    return 'aarch64-linux-android' if 'arm64' == target_arch else 'arm-linux-androideabi'

def getAndroidCompilerName(target_arch, api_version):
    if target_arch == 'arm64':
        return 'aarch64-linux-android%s-clang' % (api_version)
    else:
        return 'armv7a-linux-androideabi%s-clang' % (api_version)

def getAndroidNDKAPIVersion(target_arch):
    if Options.options.with_vulkan:
        return ANDROID_NDK_API_VERSION_VULKAN
    elif target_arch == 'arm64':
        return ANDROID_64_NDK_API_VERSION
    else:
        return ANDROID_NDK_API_VERSION

def getAndroidCompileFlags(target_arch):
    # NOTE compared to armv7-android:
    # -mthumb, -mfloat-abi, -mfpu are implicit on aarch64, removed from flags
    if 'arm64' == target_arch:
        return ['-D__aarch64__', '-DGOOGLE_PROTOBUF_NO_RTTI', '-march=armv8-a', '-fvisibility=hidden']
    # NOTE:
    # -fno-exceptions added
    else:
        return ['-D__ARM_ARCH_5__', '-D__ARM_ARCH_5T__', '-D__ARM_ARCH_5E__', '-D__ARM_ARCH_5TE__', '-DGOOGLE_PROTOBUF_NO_RTTI', '-march=armv7-a', '-mfloat-abi=softfp', '-mfpu=vfp', '-fvisibility=hidden']

def getAndroidLinkFlags(target_arch):
    if 'arm64' == target_arch:
        return ['-Wl,--no-undefined', '-Wl,-z,noexecstack', '-landroid', '-fpic', '-z', 'text']
    else:
        return ['-Wl,--fix-cortex-a8', '-Wl,--no-undefined', '-Wl,-z,noexecstack', '-landroid', '-fpic', '-z', 'text']


@feature('cc', 'cxx')
# We must apply this before the objc_hook below
# Otherwise will .mm-files not be compiled with -arch armv7 etc.
# I don't know if this is entirely correct
@before('apply_core')
def default_flags(self):
    global MIN_IOS_SDK_VERSION
    build_util = create_build_utility(self.env)

    opt_level = Options.options.opt_level
    if opt_level == "2" and 'web' == build_util.get_target_os():
        opt_level = "3" # emscripten highest opt level
    elif opt_level == "0" and 'win' in build_util.get_target_os():
        opt_level = "d" # how to disable optimizations in windows

    # For nicer better output (i.e. in CI logs), and still get some performance, let's default to -O1
    if Options.options.with_asan and opt_level != '0':
        opt_level = 1

    FLAG_ST = '/%s' if 'win' == build_util.get_target_os() else '-%s'

    # Common for all platforms
    flags = []
    flags += [FLAG_ST % ('O%s' % opt_level)]
    if Options.options.ndebug:
        flags += [self.env.CXXDEFINES_ST % 'NDEBUG']

    for f in ['CCFLAGS', 'CXXFLAGS']:
        self.env.append_value(f, flags)

    if 'osx' == build_util.get_target_os() or 'ios' == build_util.get_target_os():
        self.env.append_value('LINKFLAGS', ['-weak_framework', 'Foundation'])
        if 'ios' == build_util.get_target_os():
            self.env.append_value('LINKFLAGS', ['-framework', 'UIKit', '-framework', 'SystemConfiguration', '-framework', 'CoreTelephony'])
        else:
            self.env.append_value('LINKFLAGS', ['-framework', 'AppKit'])

    if "linux" == build_util.get_target_os() or "osx" == build_util.get_target_os():
        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-Werror=format', '-fno-exceptions','-fPIC', '-fvisibility=hidden'])

            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti'])

            # Without using '-ffloat-store', on 32bit Linux, there are floating point precison errors in
            # some tests after we switched to -02 optimisations. We should refine these tests so that they
            # don't rely on equal-compare floating point values, and/or verify that underlaying engine
            # code don't rely on floats being of a certain precision level. When this is done, we can
            # remove -ffloat-store. Jira Issue: DEF-1891
            if 'linux' == build_util.get_target_os() and 'x86' == build_util.get_target_architecture():
                self.env.append_value(f, ['-ffloat-store'])
            if 'osx' == build_util.get_target_os() and 'x86' == build_util.get_target_architecture():
                self.env.append_value(f, ['-m32'])
            if "osx" == build_util.get_target_os():
                # NOTE: Default libc++ changed from libstdc++ to libc++ on Maverick/iOS7.
                # Force libstdc++ for now
                self.env.append_value(f, ['-stdlib=libstdc++'])
                self.env.append_value(f, '-mmacosx-version-min=%s' % MIN_OSX_SDK_VERSION)
                self.env.append_value(f, ['-isysroot', '%s/MacOSX%s.sdk' % (build_util.get_dynamo_ext('SDKs'), OSX_SDK_VERSION)])
            # We link by default to uuid on linux. libuuid is wrapped in dlib (at least currently)
        if 'osx' == build_util.get_target_os() and 'x86' == build_util.get_target_architecture():
            self.env.append_value('LINKFLAGS', ['-m32'])
        if 'osx' == build_util.get_target_os():
            self.env.append_value('LINKFLAGS', ['-stdlib=libstdc++', '-isysroot', '%s/MacOSX%s.sdk' % (build_util.get_dynamo_ext('SDKs'), OSX_SDK_VERSION), '-mmacosx-version-min=%s' % MIN_OSX_SDK_VERSION,'-lSystem', '-framework', 'Carbon','-flto'])

    elif 'ios' == build_util.get_target_os() and build_util.get_target_architecture() in ('armv7', 'arm64', 'x86_64'):
        if Options.options.with_asan:
            MIN_IOS_SDK_VERSION="8.0" # embedded dylibs/frameworks are only supported on iOS 8.0 and later

        #  NOTE: -lobjc was replaced with -fobjc-link-runtime in order to make facebook work with iOS 5 (dictionary subscription with [])
        sys_root = '%s/iPhoneOS%s.sdk' % (build_util.get_dynamo_ext('SDKs'), IOS_SDK_VERSION)
        if 'x86_64' == build_util.get_target_architecture():
            sys_root = '%s/iPhoneSimulator%s.sdk' % (build_util.get_dynamo_ext('SDKs'), IOS_SIMULATOR_SDK_VERSION)

        for f in ['CCFLAGS', 'CXXFLAGS']:

            self.env.append_value(f, ['-g', '-stdlib=libc++', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-fno-exceptions', '-fno-rtti', '-fvisibility=hidden',
                                            '-arch', build_util.get_target_architecture(), '-miphoneos-version-min=%s' % MIN_IOS_SDK_VERSION,
                                            '-isysroot', sys_root])

            if 'x86_64' == build_util.get_target_architecture():
                self.env.append_value(f, ['-DIOS_SIMULATOR'])

        self.env.append_value('LINKFLAGS', [ '-arch', build_util.get_target_architecture(), '-stdlib=libc++', '-fobjc-link-runtime', '-isysroot', sys_root, '-dead_strip', '-miphoneos-version-min=%s' % MIN_IOS_SDK_VERSION])

    elif 'android' == build_util.get_target_os():
        target_arch = build_util.get_target_architecture()

        sysroot='%s/toolchains/llvm/prebuilt/%s-x86_64/sysroot' % (ANDROID_NDK_ROOT, self.env['BUILD_PLATFORM'])

        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-gdwarf-2', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall',
                                      '-fpic', '-ffunction-sections', '-fstack-protector',
                                      '-fomit-frame-pointer', '-fno-strict-aliasing', '-fno-exceptions', '-funwind-tables',
                                      '-I%s/sources/android/native_app_glue' % (ANDROID_NDK_ROOT),
                                      '-I%s/sources/android/cpufeatures' % (ANDROID_NDK_ROOT),
                                      '-isysroot=%s' % sysroot,
                                      '-DANDROID', '-Wa,--noexecstack'] + getAndroidCompileFlags(target_arch))
            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti'])

        # TODO: Should be part of shared libraries
        # -Wl,-soname,libnative-activity.so -shared
        # -lsupc++
        self.env.append_value('LINKFLAGS', [
                '-isysroot=%s' % sysroot,
                '-static-libstdc++'] + getAndroidLinkFlags(target_arch))
    elif 'web' == build_util.get_target_os():

        # Default to asmjs output
        wasm_enabled = 0
        legacy_vm_support = 1
        binaryen_method = "asmjs"
        if 'wasm' == build_util.get_target_architecture():
            wasm_enabled = 1
            legacy_vm_support = 0
            binaryen_method = "native-wasm"

        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-DGL_ES_VERSION_2_0', '-DGOOGLE_PROTOBUF_NO_RTTI', '-fno-exceptions', '-fno-rtti', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS',
                                      '-Wall', '-s', 'LEGACY_VM_SUPPORT=%d' % legacy_vm_support, '-s', 'WASM=%d' % wasm_enabled, '-s', 'BINARYEN_METHOD="%s"' % binaryen_method, '-s', 'BINARYEN_TRAP_MODE="clamp"', '-s', 'EXTRA_EXPORTED_RUNTIME_METHODS=["stringToUTF8","ccall"]', '-s', 'EXPORTED_FUNCTIONS=["_JSWriteDump","_main"]',
                                      '-I%s/system/lib/libcxxabi/include' % EMSCRIPTEN_ROOT]) # gtest uses cxxabi.h and for some reason, emscripten doesn't find it (https://github.com/kripken/emscripten/issues/3484)

        # NOTE: Disabled lto for when upgrading to 1.35.23, see https://github.com/kripken/emscripten/issues/3616
        self.env.append_value('LINKFLAGS', ['-O%s' % opt_level, '--emit-symbol-map', '--llvm-lto', '0', '-s', 'PRECISE_F32=2', '-s', 'AGGRESSIVE_VARIABLE_ELIMINATION=1', '-s', 'DISABLE_EXCEPTION_CATCHING=1', '-Wno-warn-absolute-paths', '-s', 'TOTAL_MEMORY=268435456', '--memory-init-file', '0', '-s', 'LEGACY_VM_SUPPORT=%d' % legacy_vm_support, '-s', 'WASM=%d' % wasm_enabled, '-s', 'BINARYEN_METHOD="%s"' % binaryen_method, '-s', 'BINARYEN_TRAP_MODE="clamp"', '-s', 'EXTRA_EXPORTED_RUNTIME_METHODS=["stringToUTF8","ccall"]', '-s', 'EXPORTED_FUNCTIONS=["_JSWriteDump","_main"]', '-s','ERROR_ON_UNDEFINED_SYMBOLS=1'])

    else: # *-win32
        for f in ['CCFLAGS', 'CXXFLAGS']:
            # /Oy- = Disable frame pointer omission. Omitting frame pointers breaks crash report stack trace. /O2 implies /Oy.
            # 0x0600 = _WIN32_WINNT_VISTA
            self.env.append_value(f, ['/Oy-', '/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS', '/DWINVER=0x0600', '/D_WIN32_WINNT=0x0600', '/D_CRT_SECURE_NO_WARNINGS', '/wd4996', '/wd4200'])
        self.env.append_value('LINKFLAGS', '/DEBUG')
        self.env.append_value('LINKFLAGS', ['shell32.lib', 'WS2_32.LIB', 'Iphlpapi.LIB', 'AdvAPI32.Lib'])
        self.env.append_unique('ARFLAGS', '/WX')

    libpath = build_util.get_library_path()

    # Create directory in order to avoid warning 'ld: warning: directory not found for option' before first install
    if not os.path.exists(libpath):
        os.makedirs(libpath)
    self.env.append_value('LIBPATH', libpath)

    self.env.append_value('CPPPATH', build_util.get_dynamo_ext('include'))
    self.env.append_value('CPPPATH', build_util.get_dynamo_home('sdk','include'))
    self.env.append_value('CPPPATH', build_util.get_dynamo_home('include'))
    self.env.append_value('CPPPATH', build_util.get_dynamo_home('include', build_util.get_target_platform()))
    self.env.append_value('CPPPATH', build_util.get_dynamo_ext('include', build_util.get_target_platform()))
    self.env.append_value('LIBPATH', build_util.get_dynamo_ext('lib', build_util.get_target_platform()))

# Used if you wish to be specific about certain default flags for a library (e.g. used for mbedtls library)
@feature('remove_flags')
@before('apply_core')
@after('cc')
@after('cxx')
def remove_flags_fn(self):
    lookup = getattr(self, 'remove_flags', [])
    for name, values in lookup.iteritems():
        for flag, argcount in values:
            remove_flag(self.env[name], flag, argcount)

@feature('cprogram', 'cxxprogram', 'cstaticlib', 'cshlib')
@after('apply_obj_vars')
def android_link_flags(self):
    build_util = create_build_utility(self.env)
    if 'android' == build_util.get_target_os():
        self.link_task.env.append_value('LINKFLAGS', ['-lm', '-llog', '-lc'])
        self.link_task.env.append_value('LINKFLAGS', '-Wl,--no-undefined -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now'.split())

        if 'apk' in self.features:
            # NOTE: This is a hack We change cprogram -> cshlib
            # but it's probably to late. It works for the name though (libX.so and not X)
            self.link_task.env.append_value('LINKFLAGS', ['-shared'])

@feature('cprogram', 'cxxprogram')
@before('apply_core')
def osx_64_luajit(self):
    # Was previously needed for 64bit OSX, but removed when we updated luajit-2.1.0-beta3,
    # however it is still needed for 64bit iOS Simulator.
    if self.env['PLATFORM'] == 'x86_64-ios':
        self.env.append_value('LINKFLAGS', ['-pagezero_size', '10000', '-image_base', '100000000'])

@feature('skip_asan')
@before('apply_core')
def asan_skip(self):
    self.skip_asan = True

@feature('cprogram', 'cxxprogram', 'cstaticlib', 'cshlib')
@before('apply_core')
@after('skip_asan')
def asan_cxxflags(self):
    if getattr(self, 'skip_asan', False):
        return
    build_util = create_build_utility(self.env)
    if Options.options.with_asan and build_util.get_target_os() in ('osx','ios'):
        self.env.append_value('CXXFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope', '-DSANITIZE_ADDRESS'])
        self.env.append_value('CCFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope', '-DSANITIZE_ADDRESS'])
        self.env.append_value('LINKFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope'])

@taskgen
@feature('cprogram', 'cxxprogram')
@after('apply_link')
def apply_unit_test(self):
    # Do not execute unit-tests tasks (compile and link) when --skip-build-tests is set
    if 'test' in self.features and getattr(Options.options, 'skip_build_tests', False):
            for t in self.tasks:
                t.hasrun = True

@feature('apk')
@before('apply_core')
def apply_apk_test(self):
    build_util = create_build_utility(self.env)
    if 'android' == build_util.get_target_os():
        self.features.remove('cprogram')
        self.features.append('cshlib')

# Install all static libraries by default
@feature('cstaticlib')
@after('default_cc')
@before('apply_core')
def default_install_staticlib(self):
    self.default_install_path = self.env.LIBDIR

@feature('cshlib')
@after('default_cc')
@before('apply_core')
def default_install_shlib(self):
    # Force installation dir to LIBDIR.
    # Default on windows is BINDIR
    self.default_install_path = self.env.LIBDIR

# objective-c++ support
if sys.platform == "darwin":
    EXT_OBJCXX = ['.mm']
    @extension(EXT_OBJCXX)
    def objc_hook(self, node):
        tsk = cxx.cxx_hook(self, node)
        tsk.env.append_unique('CXXFLAGS', tsk.env['GCC-OBJCXX'])
        tsk.env.append_unique('LINKFLAGS', tsk.env['GCC-OBJCLINK'])

# objective-c support
if sys.platform == "darwin":
    EXT_OBJC = ['.m']
    @extension(EXT_OBJC)
    def objc_hook(self, node):
        tsk = cc.c_hook(self, node)
        tsk.env.append_unique('CXXFLAGS', tsk.env['GCC-OBJCC'])
        tsk.env.append_unique('LINKFLAGS', tsk.env['GCC-OBJCLINK'])

# iPhone bundle and signing support
RESOURCE_RULES_PLIST = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>rules</key>
        <dict>
                <key>.*</key>
                <true/>
                <key>Info.plist</key>
                <dict>
                        <key>omit</key>
                        <true/>
                        <key>weight</key>
                        <real>10</real>
                </dict>
                <key>ResourceRules.plist</key>
                <dict>
                        <key>omit</key>
                        <true/>
                        <key>weight</key>
                        <real>100</real>
                </dict>
        </dict>
</dict>
</plist>
"""

# The following is removed info.plist
# <key>NSMainNibFile</key>
# <string>MainWindow</string>
# We manage our own setup. At least for now

# NOTE: fb355198514515820 is HelloFBSample appid and for testing only
# This INFO_PLIST is only used for development
INFO_PLIST = """<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>BuildMachineOSBuild</key>
        <string>10K540</string>
        <key>CFBundleDevelopmentRegion</key>
        <string>en</string>
        <key>CFBundleDisplayName</key>
        <string>%(executable)s</string>
        <key>CFBundleExecutable</key>
        <string>%(executable)s</string>
        <key>CFBundleIdentifier</key>
        <string>%(bundleid)s</string>
        <key>CFBundleInfoDictionaryVersion</key>
        <string>6.0</string>
        <key>CFBundleName</key>
        <string>%(executable)s</string>
        <key>CFBundlePackageType</key>
        <string>APPL</string>
        <key>CFBundleResourceSpecification</key>
        <string>ResourceRules.plist</string>
        <key>CFBundleShortVersionString</key>
        <string>1.0</string>
        <key>CFBundleSignature</key>
        <string>????</string>
        <key>CFBundleSupportedPlatforms</key>
        <array>
                <string>iPhoneOS</string>
        </array>
        <key>CFBundleVersion</key>
        <string>1.0</string>
        <key>DTCompiler</key>
        <string>com.apple.compilers.llvmgcc42</string>
        <key>DTPlatformBuild</key>
        <string>8H7</string>
        <key>DTPlatformName</key>
        <string>iphoneos</string>
        <key>DTPlatformVersion</key>
        <string>4.3</string>
        <key>DTSDKBuild</key>
        <string>8H7</string>
        <key>DTSDKName</key>
        <string>iphoneos4.3</string>
        <key>DTXcode</key>
        <string>0402</string>
        <key>DTXcodeBuild</key>
        <string>4A2002a</string>
        <key>UIFileSharingEnabled</key>
        <true/>
        <key>LSRequiresIPhoneOS</key>
        <true/>
        <key>MinimumOSVersion</key>
        <string>5.0</string>
        <key>UIDeviceFamily</key>
        <array>
                <integer>1</integer>
                <integer>2</integer>
        </array>
        <key>UIStatusBarHidden</key>
        <true/>
        <key>UIViewControllerBasedStatusBarAppearance</key>
        <false/>
        <key>UISupportedInterfaceOrientations</key>
        <array>
                <string>UIInterfaceOrientationPortrait</string>
                <string>UIInterfaceOrientationPortraitUpsideDown</string>
                <string>UIInterfaceOrientationLandscapeLeft</string>
                <string>UIInterfaceOrientationLandscapeRight</string>
        </array>
        <key>UISupportedInterfaceOrientations~ipad</key>
        <array>
                <string>UIInterfaceOrientationPortrait</string>
                <string>UIInterfaceOrientationPortraitUpsideDown</string>
                <string>UIInterfaceOrientationLandscapeLeft</string>
                <string>UIInterfaceOrientationLandscapeRight</string>
        </array>

        <key>CFBundleURLTypes</key>
        <array>
                <dict>
                        <key>CFBundleURLSchemes</key>
                        <array>
                                <string>fb596203607098569</string>
                        </array>
                </dict>
        </array>
</dict>
</plist>
"""
def codesign(task):
    bld = task.generator.bld

    exe_path = task.exe.bldpath(task.env)
    signed_exe_path = task.signed_exe.bldpath(task.env)
    shutil.copy(exe_path, signed_exe_path)

    signed_exe_dir = os.path.dirname(signed_exe_path)

    identity = task.env.IDENTITY
    if not identity:
        identity = 'iPhone Developer'

    mobileprovision = task.provision
    if not mobileprovision:
        mobileprovision = 'engine_profile.mobileprovision'
    mobileprovision_path = os.path.join(task.env['DYNAMO_HOME'], 'share', mobileprovision)
    shutil.copyfile(mobileprovision_path, os.path.join(signed_exe_dir, 'embedded.mobileprovision'))

    entitlements = task.entitlements
    if not entitlements:
        entitlements = 'engine_profile.xcent'
    entitlements_path = os.path.join(task.env['DYNAMO_HOME'], 'share', entitlements)
    resource_rules_plist_file = task.resource_rules_plist.bldpath(task.env)

    ret = bld.exec_command('CODESIGN_ALLOCATE=%s/usr/bin/codesign_allocate codesign -f -s "%s" --resource-rules=%s --entitlements %s %s' % (DARWIN_TOOLCHAIN_ROOT, identity, resource_rules_plist_file, entitlements_path, signed_exe_dir))
    if ret != 0:
        error('Error running codesign')
        return 1

    return 0

Task.task_type_from_func('codesign',
                         func = codesign,
                         #color = 'RED',
                         after  = 'app_bundle')

def app_bundle(task):
    info_plist_file = open(task.info_plist.bldpath(task.env), 'wb')
    bundleid = 'com.defold.%s' % task.exe_name
    if task.bundleid:
        bundleid = task.bundleid
    info_plist_file.write(INFO_PLIST % { 'executable' : task.exe_name, 'bundleid' : bundleid })
    info_plist_file.close()

    resource_rules_plist_file = open(task.resource_rules_plist.bldpath(task.env), 'wb')
    resource_rules_plist_file.write(RESOURCE_RULES_PLIST)
    resource_rules_plist_file.close()

    return 0

def create_export_symbols(task):
    with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
        for name in Utils.to_list(task.exported_symbols):
            print >>out_f, 'extern "C" void %s();' % name
        print >>out_f, "void dmExportedSymbols() {"
        for name in Utils.to_list(task.exported_symbols):
            print >>out_f, "    %s();" % name
        print >>out_f, "}"

    return 0

task = Task.task_type_from_func('create_export_symbols',
                                func  = create_export_symbols,
                                color = 'PINK',
                                before  = 'cc cxx')

task.runnable_status = lambda self: RUN_ME

@taskgen
@feature('cprogram')
@before('apply_core')
def export_symbols(self):
    # Force inclusion of symbol, e.g. from static libraries
    # We used to use -Wl,-exported_symbol on Darwin but changed
    # to a more general solution by generating a .cpp-file with
    # a function which references the symbols in question
    Utils.def_attrs(self, exported_symbols=[])
    if not self.exported_symbols:
        return

    exported_symbols = self.path.find_or_declare('__exported_symbols_%d.cpp' % self.idx)
    self.allnodes.append(exported_symbols)

    task = self.create_task('create_export_symbols')
    task.exported_symbols = self.exported_symbols
    task.set_outputs([exported_symbols])

Task.task_type_from_func('app_bundle',
                         func = app_bundle,
                         vars = ['SRC', 'DST'],
                         #color = 'RED',
                         after  = 'cxx_link cc_link static_link')


def _strip_executable(bld, platform, target_arch, path):
    """ Strips the debug symbols from an executable """
    if platform not in ['x86_64-linux','x86_64-darwin','armv7-darwin','arm64-darwin','armv7-android','arm64-android']:
        return 0 # return ok, path is still unstripped

    strip = "strip"
    if 'android' in platform:
        HOME = os.environ['USERPROFILE' if sys.platform == 'win32' else 'HOME']
        ANDROID_HOST = 'linux' if sys.platform == 'linux2' else 'darwin'
        build_tool = getAndroidBuildtoolName(target_arch)
        strip = "%s/toolchains/%s-%s/prebuilt/%s-x86_64/bin/%s-strip" % (ANDROID_NDK_ROOT, build_tool, ANDROID_GCC_VERSION, ANDROID_HOST, build_tool)

    return bld.exec_command("%s %s" % (strip, path))

AUTHENTICODE_CERTIFICATE="Midasplayer Technology AB"

def authenticode_certificate_installed(task):
    if Options.options.skip_codesign:
        return 0
    ret = task.exec_command('powershell "Get-ChildItem cert: -Recurse | Where-Object {$_.FriendlyName -Like """%s*"""} | Measure | Foreach-Object { exit $_.Count }"' % AUTHENTICODE_CERTIFICATE, log=True)
    return ret > 0

def authenticode_sign(task):
    if Options.options.skip_codesign:
        return
    exe_file = task.inputs[0].abspath(task.env)
    exe_file_to_sign = task.inputs[0].change_ext('_to_sign.exe').abspath(task.env)
    exe_file_signed = task.outputs[0].abspath(task.env)

    ret = task.exec_command('copy /Y %s %s' % (exe_file, exe_file_to_sign), log=True)
    if ret != 0:
        error("Unable to copy file before signing")
        return 1

    ret = task.exec_command('"%s" sign /sm /n "%s" /fd sha256 /tr http://timestamp.comodoca.com /td sha256 /d defold /du https://www.defold.com /v %s' % (task.env['SIGNTOOL'], AUTHENTICODE_CERTIFICATE, exe_file_to_sign), log=True)
    if ret != 0:
        error("Unable to sign executable")
        return 1

    ret = task.exec_command('move /Y %s %s' % (exe_file_to_sign, exe_file_signed), log=True)
    if ret != 0:
        error("Unable to rename file after signing")
        return 1

    return 0

Task.task_type_from_func('authenticode_sign',
                         func = authenticode_sign,
                         after = 'cxx_link cc_link static_link msvc_manifest')

@taskgen
@feature('authenticode')
def authenticode(self):
    exe_file = self.link_task.outputs[0].abspath(self.env)
    sign_task = self.create_task('authenticode_sign', self.env)
    sign_task.set_inputs(self.link_task.outputs)
    sign_task.set_outputs([self.link_task.outputs[0].change_ext('_signed.exe')])

@taskgen
@after('apply_link')
@feature('cprogram')
def create_app_bundle(self):
    if not re.match('arm.*?darwin', self.env['PLATFORM']):
        return
    Utils.def_attrs(self, bundleid = None, provision = None, entitlements = None)

    app_bundle_task = self.create_task('app_bundle', self.env)
    app_bundle_task.bundleid = self.bundleid
    app_bundle_task.set_inputs(self.link_task.outputs)

    exe_name = self.link_task.outputs[0].name
    app_bundle_task.exe_name = exe_name

    info_plist = self.path.exclusive_build_node("%s.app/Info.plist" % exe_name)
    app_bundle_task.info_plist = info_plist

    resource_rules_plist = self.path.exclusive_build_node("%s.app/ResourceRules.plist" % exe_name)
    app_bundle_task.resource_rules_plist = resource_rules_plist

    app_bundle_task.set_outputs([info_plist, resource_rules_plist])

    self.app_bundle_task = app_bundle_task

    if not Options.options.skip_codesign:
        signed_exe = self.path.exclusive_build_node("%s.app/%s" % (exe_name, exe_name))

        codesign = self.create_task('codesign', self.env)
        codesign.provision = self.provision
        codesign.entitlements = self.entitlements
        codesign.resource_rules_plist = resource_rules_plist
        codesign.set_inputs(self.link_task.outputs)
        codesign.set_outputs(signed_exe)

        codesign.exe = self.link_task.outputs[0]
        codesign.signed_exe = signed_exe

# TODO: We should support a custom AndroidManifest.xml-file in waf
# Or at least a decent templating system, e.g. mustache
ANDROID_MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<!-- BEGIN_INCLUDE(manifest) -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="%(package)s"
        android:versionCode="1"
        android:versionName="1.0"
        android:installLocation="auto">

    <uses-feature android:required="true" android:glEsVersion="0x00020000" />
    <uses-sdk android:minSdkVersion="%(min_api_level)s" android:targetSdkVersion="%(target_api_level)s" />
    <application
        android:label="%(app_name)s"
        android:hasCode="true"
        android:name="android.support.multidex.MultiDexApplication">

        <meta-data android:name="android.max_aspect" android:value="2.1" />
        <meta-data android:name="android.notch_support" android:value="true"/>

        <activity android:name="com.dynamo.android.DefoldActivity"
                android:label="%(app_name)s"
                android:configChanges="orientation|screenSize|keyboardHidden"
                android:theme="@android:style/Theme.NoTitleBar.Fullscreen"
                android:launchMode="singleTask">
            <meta-data android:name="android.app.lib_name"
                    android:value="%(lib_name)s" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
        <activity android:name="com.dynamo.android.DispatcherActivity" android:theme="@android:style/Theme.Translucent.NoTitleBar" />
        <activity android:name="com.defold.iap.IapGooglePlayActivity"
          android:theme="@android:style/Theme.Translucent.NoTitleBar"
          android:configChanges="keyboard|keyboardHidden|screenLayout|screenSize|orientation"
          android:label="IAP">
        </activity>

        <!-- For IAC Invocations -->
        <activity android:name="com.defold.iac.IACActivity"
            android:theme="@android:style/Theme.Translucent.NoTitleBar"
            android:launchMode="singleTask"
            android:configChanges="keyboardHidden|orientation|screenSize">
            <intent-filter>
               <action android:name="android.intent.action.VIEW" />
               <category android:name="android.intent.category.DEFAULT" />
               <category android:name="android.intent.category.BROWSABLE" />
               <data android:scheme="%(package)s" />
            </intent-filter>
        </activity>

        <!-- For Amazon IAP -->
        <receiver android:name="com.amazon.device.iap.ResponseReceiver" >
            <intent-filter>
                <action android:name="com.amazon.inapp.purchasing.NOTIFY" android:permission="com.amazon.inapp.purchasing.Permission.NOTIFY" />
            </intent-filter>
        </receiver>

    </application>
    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="com.android.vending.BILLING" />
    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />

</manifest>
<!-- END_INCLUDE(manifest) -->
"""

ANDROID_STUB = """
struct android_app;

extern void _glfwPreMain(struct android_app* state);
extern void app_dummy();

void android_main(struct android_app* state)
{
    // Make sure glue isn't stripped.
    app_dummy();
    _glfwPreMain(state);
}
"""

def android_package(task):
    # TODO: This is a complete mess and should be split in several smaller tasks!

    bld = task.generator.bld

    package = 'com.defold.%s' % task.exe_name
    if task.android_package:
        package = task.android_package

    try:
        build_util = create_build_utility(task.env)
    except BuildUtilityException as ex:
        task.fatal(ex.msg)
    manifest_file = open(task.manifest.bldpath(task.env), 'wb')
    manifest_file.write(ANDROID_MANIFEST % { 'package' : package, 'app_name' : task.exe_name, 'lib_name' : task.exe_name, 'min_api_level' : ANDROID_MIN_API_LEVEL, 'target_api_level' : ANDROID_TARGET_API_LEVEL })
    manifest_file.close()

    aapt = '%s/android-sdk/build-tools/%s/aapt' % (ANDROID_ROOT, ANDROID_BUILD_TOOLS_VERSION)
    dx = '%s/android-sdk/build-tools/%s/dx' % (ANDROID_ROOT, ANDROID_BUILD_TOOLS_VERSION)
    dynamo_home = task.env['DYNAMO_HOME']
    android_jar = '%s/ext/share/java/android.jar' % (dynamo_home)

    manifest = task.manifest.abspath(task.env)
    dme_and = os.path.normpath(os.path.join(os.path.dirname(task.manifest.abspath(task.env)), '..', '..'))

    libs = os.path.join(dme_and, 'libs')
    bin = os.path.join(dme_and, 'bin')
    bin_cls = os.path.join(bin, 'classes')
    dx_libs = os.path.join(bin, 'dexedLibs')
    gen = os.path.join(dme_and, 'gen')
    ap_ = task.ap_.abspath(task.env)
    native_lib = task.native_lib.abspath(task.env)

    bld.exec_command('mkdir -p %s' % (libs))
    bld.exec_command('mkdir -p %s' % (bin))
    bld.exec_command('mkdir -p %s' % (bin_cls))
    bld.exec_command('mkdir -p %s' % (dx_libs))
    bld.exec_command('mkdir -p %s' % (gen))
    shutil.copy(task.native_lib_in.abspath(task.env), native_lib)

    if task.extra_packages:
        extra_packages_cmd = '--extra-packages %s' % task.extra_packages[0]
    else:
        extra_packages_cmd = ''

    ret = bld.exec_command('%s package -f %s -m --debug-mode --auto-add-overlay -M %s -I %s -F %s' % (aapt, extra_packages_cmd, manifest, android_jar, ap_))
    if ret != 0:
        error('Error running aapt')
        return 1

    dx_jars = []
    for jar in task.jars:
        dx_jars.append(jar)

    if getattr(task, 'proguard', None):
        proguardtxt = task.proguard[0]
        proguardjar = '%s/android-sdk/tools/proguard/lib/proguard.jar' % ANDROID_ROOT
        dex_input = ['%s/share/java/classes.jar' % dynamo_home]

        ret = bld.exec_command('%s -jar %s -include %s -libraryjars %s -injars %s -outjar %s' % (task.env['JAVA'][0], proguardjar, proguardtxt, android_jar, ':'.join(dx_jars), dex_input[0]))
        if ret != 0:
            error('Error running proguard')
            return 1
    else:
        dex_input = dx_jars
    
    if dex_input:
        ret = bld.exec_command('%s --dex --output %s %s' % (dx, task.classes_dex.abspath(task.env), ' '.join(dex_input)))
        if ret != 0:
            error('Error running dx')
            return 1
    
    if os.path.exists(task.classes_dex.abspath(task.env)):
        with zipfile.ZipFile(ap_, 'a', zipfile.ZIP_DEFLATED) as zip:
            zip.write(task.classes_dex.abspath(task.env), 'classes.dex')

    apk_unaligned = task.apk_unaligned.abspath(task.env)
    libs_dir = task.native_lib.parent.parent.abspath(task.env)

    # strip the executable
    path = task.native_lib.abspath(task.env)
    ret = _strip_executable(bld, task.env.PLATFORM, build_util.get_target_architecture(), path)
    if ret != 0:
        error('Error stripping file %s' % path)
        return 1

    # add library files
    with zipfile.ZipFile(ap_, 'a', zipfile.ZIP_DEFLATED) as zip:
        for root, dirs, files in os.walk(libs_dir):
            for f in files:
                full_path = os.path.join(root, f)
                relative_path = os.path.relpath(full_path, libs_dir)
                if relative_path.startswith('armeabi-v7a'):
                    relative_path = os.path.join('lib', relative_path)
                if relative_path.startswith('arm64-v8a'):
                    relative_path = os.path.join('lib', relative_path)
                zip.write(full_path, relative_path)

    shutil.copy(ap_, apk_unaligned)

    apk = task.apk.abspath(task.env)

    zipalign = '%s/android-sdk/build-tools/%s/zipalign' % (ANDROID_ROOT, ANDROID_BUILD_TOOLS_VERSION)
    ret = bld.exec_command('%s -f 4 %s %s' % (zipalign, apk_unaligned, ap_))
    if ret != 0:
        error('Error running zipalign')
        return 1

    apkc = '%s/../../com.dynamo.cr/com.dynamo.cr.bob/libexec/x86_64-%s/apkc' % (os.environ['DYNAMO_HOME'], task.env.BUILD_PLATFORM)
    if not os.path.exists(apkc):
        error("file doesn't exist: %s" % apkc)
        return 1

    ret = bld.exec_command('%s --in="%s" --out="%s"' % (apkc, ap_, apk))
    if ret != 0:
        error('Error running apkc')
        return 1

    with open(task.android_mk.abspath(task.env), 'wb') as f:
        print >>f, 'APP_ABI := %s' % getAndroidArch(build_util.get_target_architecture())

    with open(task.application_mk.abspath(task.env), 'wb') as f:
        print >>f, ''

    with open(task.gdb_setup.abspath(task.env), 'wb') as f:
        if 'arm64' == build_util.get_target_architecture():
            print >>f, 'set solib-search-path ./libs/arm64-v8a:./obj/local/arm64-v8a/'
        else:
            print >>f, 'set solib-search-path ./libs/armeabi-v7a:./obj/local/armeabi-v7a/'

    return 0

Task.task_type_from_func('android_package',
                         func = android_package,
                         vars = ['SRC', 'DST'],
                         after  = 'cxx_link cc_link static_link')

@taskgen
@after('apply_link')
@feature('apk')
def create_android_package(self):
    if not re.match('arm.*?android', self.env['PLATFORM']):
        return
    Utils.def_attrs(self, android_package = None)

    android_package_task = self.create_task('android_package', self.env)
    android_package_task.set_inputs(self.link_task.outputs)
    android_package_task.android_package = self.android_package

    Utils.def_attrs(self, extra_packages = "")
    android_package_task.extra_packages = Utils.to_list(self.extra_packages)

    Utils.def_attrs(self, jars=[])
    android_package_task.jars = Utils.to_list(self.jars)

    exe_name = self.name
    lib_name = self.link_task.outputs[0].name

    android_package_task.exe_name = exe_name

    manifest = self.path.exclusive_build_node("%s.android/AndroidManifest.xml" % exe_name)
    android_package_task.manifest = manifest

    try:
        build_util = create_build_utility(android_package_task.env)
    except BuildUtilityException as ex:
        android_package_task.fatal(ex.msg)

    if 'arm64' == build_util.get_target_architecture():
        native_lib = self.path.exclusive_build_node("%s.android/libs/arm64-v8a/%s" % (exe_name, lib_name))
    else:
        native_lib = self.path.exclusive_build_node("%s.android/libs/armeabi-v7a/%s" % (exe_name, lib_name))
    android_package_task.native_lib = native_lib
    android_package_task.native_lib_in = self.link_task.outputs[0]

    ap_ = self.path.exclusive_build_node("%s.android/%s.ap_" % (exe_name, exe_name))
    android_package_task.ap_ = ap_

    apk_unaligned = self.path.exclusive_build_node("%s.android/%s-unaligned.apk" % (exe_name, exe_name))
    android_package_task.apk_unaligned = apk_unaligned

    apk = self.path.exclusive_build_node("%s.android/%s.apk" % (exe_name, exe_name))
    android_package_task.apk = apk

    android_package_task.classes_dex = self.path.exclusive_build_node("%s.android/classes.dex" % (exe_name))

    # NOTE: These files are required for ndk-gdb
    android_package_task.android_mk = self.path.exclusive_build_node("%s.android/jni/Android.mk" % (exe_name))
    android_package_task.application_mk = self.path.exclusive_build_node("%s.android/jni/Application.mk" % (exe_name))
    if 'arm64' == build_util.get_target_architecture():
        android_package_task.gdb_setup = self.path.exclusive_build_node("%s.android/libs/arm64-v8a/gdb.setup" % (exe_name))
    else:
        android_package_task.gdb_setup = self.path.exclusive_build_node("%s.android/libs/armeabi-v7a/gdb.setup" % (exe_name))

    android_package_task.set_outputs([native_lib, manifest, ap_, apk_unaligned, apk,
                                      android_package_task.android_mk, android_package_task.application_mk, android_package_task.gdb_setup])

    self.android_package_task = android_package_task

def copy_stub(task):
    with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
        out_f.write(ANDROID_STUB)

    return 0

task = Task.task_type_from_func('copy_stub',
                                func  = copy_stub,
                                color = 'PINK',
                                before  = 'cc cxx')

task.runnable_status = lambda self: RUN_ME

@taskgen
@before('apply_core')
@feature('apk')
def create_copy_glue(self):
    if not re.match('arm.*?android', self.env['PLATFORM']):
        return

    stub = self.path.find_or_declare('android_stub.c')
    self.allnodes.append(stub)

    task = self.create_task('copy_stub')
    task.set_outputs([stub])

def embed_build(task):
    symbol = task.inputs[0].name.upper().replace('.', '_').replace('-', '_').replace('@', 'at')
    in_file = open(task.inputs[0].bldpath(task.env), 'rb')
    cpp_out_file = open(task.outputs[0].bldpath(task.env), 'wb')
    h_out_file = open(task.outputs[1].bldpath(task.env), 'wb')

    cpp_str = """
#include <stdint.h>
#include "dlib/align.h"
unsigned char DM_ALIGNED(16) %s[] =
"""
    cpp_out_file.write(cpp_str % (symbol))
    cpp_out_file.write('{\n    ')

    data = in_file.read()

    tmp = ''
    for i,x in enumerate(data):
        tmp += hex(ord(x)) + ', '
        if i > 0 and i % 4 == 0:
            tmp += '\n    '
    cpp_out_file.write(tmp)

    cpp_out_file.write('\n};\n')
    cpp_out_file.write('uint32_t %s_SIZE = sizeof(%s);\n' % (symbol, symbol))

    h_out_file.write('extern unsigned char %s[];\n' % (symbol))
    h_out_file.write('extern uint32_t %s_SIZE;\n' % (symbol))

    cpp_out_file.close()
    h_out_file.close()

    m = Utils.md5()
    m.update(data)

    task.generator.bld.node_sigs[task.inputs[0].variant(task.env)][task.inputs[0].id] = m.digest()
    return 0

Task.simple_task_type('dex', '${DX} --dex --output ${TGT} ${SRC}',
                      color='YELLOW',
                      after='jar_create',
                      shell=True)

@taskgen
@after('apply_java')
@feature('dex')
def apply_dex(self):
    if not re.match('arm.*?android', self.env['PLATFORM']):
        return

    jar = self.path.find_or_declare(self.destfile)
    dex = jar.change_ext('.dex')
    task = self.create_task('dex')
    task.set_inputs(jar)
    task.set_outputs(dex)

Task.task_type_from_func('embed_file',
                         func = embed_build,
                         vars = ['SRC', 'DST'],
                         color = 'RED',
                         before  = 'cc cxx')

@feature('embed')
@before('apply_core')
def embed_file(self):
    Utils.def_attrs(self, embed_source=[])
    for name in Utils.to_list(self.embed_source):
        Logs.info("Embedding '%s' ..." % name)
        node = self.path.find_resource(name)
        cc_out = node.parent.find_or_declare([node.name + '.embed.cpp'])
        h_out = node.parent.find_or_declare([node.name + '.embed.h'])

        task = self.create_task('embed_file')
        task.set_inputs(node)
        task.set_outputs([cc_out, h_out])
        self.allnodes.append(cc_out)

def do_find_file(file_name, path_list):
    for directory in Utils.to_list(path_list):
        found_file_name = os.path.join(directory,file_name)
        if os.path.exists(found_file_name):
            return found_file_name

@conf
def find_file(self, file_name, path_list = [], var = None, mandatory = False):
    if not path_list: path_list = os.environ['PATH'].split(os.pathsep)
    ret = do_find_file(file_name, path_list)
    self.check_message('file', file_name, ret, ret)
    if var: self.env[var] = ret

    if not ret and mandatory:
        self.fatal('The file %s could not be found' % file_name)

    return ret

def run_tests(valgrind = False, configfile = None):
    if not Options.commands['build'] or getattr(Options.options, 'skip_tests', False):
        return

    # TODO: Add something similar to this
    # http://code.google.com/p/v8/source/browse/trunk/tools/run-valgrind.py
    # to find leaks and set error code

    if not Build.bld.env['VALGRIND']:
        valgrind = False

    if not getattr(Options.options, 'with_valgrind', False):
        valgrind = False

    if 'web' in Build.bld.env.PLATFORM and not Build.bld.env['NODEJS']:
        Logs.info('Not running tests. node.js not found')
        return

    for t in Build.bld.all_task_gen:
        if 'test' in str(t.features) and t.name.startswith('test_') and ('cprogram' in t.features or 'cxxprogram' in t.features):
            output = t.path
            cmd = "%s %s" % (os.path.join(output.abspath(t.env), Build.bld.env.program_PATTERN % t.target), configfile)
            if 'web' in Build.bld.env.PLATFORM:
                cmd = '%s %s' % (Build.bld.env['NODEJS'], cmd)
            # disable shortly during beta release, due to issue with jctest + test_gui
            valgrind = False
            if valgrind:
                dynamo_home = os.getenv('DYNAMO_HOME')
                cmd = "valgrind -q --leak-check=full --suppressions=%s/share/valgrind-python.supp --suppressions=%s/share/valgrind-libasound.supp --suppressions=%s/share/valgrind-libdlib.supp --suppressions=%s/ext/share/luajit/lj.supp --error-exitcode=1 %s" % (dynamo_home, dynamo_home, dynamo_home, dynamo_home, cmd)
            proc = subprocess.Popen(cmd, shell = True)
            ret = proc.wait()
            if ret != 0:
                print("test failed %s" %(t.target) )
                sys.exit(ret)

@feature('cprogram', 'cxxprogram', 'cstaticlib', 'cshlib')
@after('apply_obj_vars')
def linux_link_flags(self):
    platform = self.env['PLATFORM']
    if re.match('.*?linux', platform):
        self.link_task.env.append_value('LINKFLAGS', ['-lpthread', '-lm', '-ldl'])

@feature('cprogram', 'cxxprogram')
@after('apply_obj_vars')
def js_web_link_flags(self):
    platform = self.env['PLATFORM']
    if 'web' in platform:
        pre_js = os.path.join(self.env['DYNAMO_HOME'], 'share', "js-web-pre.js")
        self.link_task.env.append_value('LINKFLAGS', ['--pre-js', pre_js])

@taskgen
@before('apply_core')
@feature('test')
def test_flags(self):
# When building tests for the web, we disable emission of emscripten js.mem init files,
# as the assumption when these are loaded is that the cwd will contain these items.
    if 'web' in self.env['PLATFORM']:
        for f in ['CCFLAGS', 'CXXFLAGS', 'LINKFLAGS']:
            self.env.append_value(f, ['--memory-init-file', '0'])

@feature('cprogram', 'cxxprogram')
@after('apply_obj_vars')
def js_web_web_link_flags(self):
    platform = self.env['PLATFORM']
    if 'web' in platform:
        lib_dirs = None
        if 'JS_LIB_PATHS' in self.env:
            lib_dirs = self.env['JS_LIB_PATHS']
        else:
            lib_dirs = {}
        libs = getattr(self, 'web_libs', [])
        jsLibHome = os.path.join(self.env['DYNAMO_HOME'], 'lib', platform, 'js')
        for lib in libs:
            js = ''
            if lib in lib_dirs:
                js = os.path.join(lib_dirs[lib], lib)
            else:
                js = os.path.join(jsLibHome, lib)
            self.link_task.env.append_value('LINKFLAGS', ['--js-library', js])

Task.simple_task_type('dSYM', '${DSYMUTIL} -o ${TGT} ${SRC}',
                      color='YELLOW',
                      after='cxx_link',
                      shell=True)

Task.simple_task_type('DSYMZIP', '${ZIP} -r ${TGT} ${SRC}',
                      color='BROWN',
                      after='dSYM') # Not sure how I could ensure this task running after the dSYM task /MAWE

@feature('extract_symbols')
@after('cprogram')
def extract_symbols(self):
    platform = self.env['PLATFORM']
    if not 'darwin' in platform:
        return

    engine = self.path.find_or_declare(self.target)
    dsym = engine.change_ext('.dSYM')
    dsymtask = self.create_task('dSYM')
    dsymtask.set_inputs(engine)
    dsymtask.set_outputs(dsym)

    archive = engine.change_ext('.dSYM.zip')
    ziptask = self.create_task('DSYMZIP')
    ziptask.set_inputs(dsymtask.outputs[0])
    ziptask.set_outputs(archive)
    ziptask.install_path = self.install_path

def create_clang_wrapper(conf, exe):
    clang_wrapper_path = os.path.join(conf.env['DYNAMO_HOME'], 'bin', '%s-wrapper.sh' % exe)

    s = '#!/bin/sh\n'
    # NOTE:  -Qunused-arguments to make clang happy (clang: warning: argument unused during compilation)
    s += "%s -Qunused-arguments $@\n" % os.path.join(DARWIN_TOOLCHAIN_ROOT, 'usr/bin/%s' % exe)
    if os.path.exists(clang_wrapper_path):
        # Keep existing script if equal
        # The cache in ccache consistency control relies on the timestamp
        with open(clang_wrapper_path, 'rb') as f:
            if f.read() == s:
                return clang_wrapper_path

    with open(clang_wrapper_path, 'wb') as f:
        f.write(s)
    os.chmod(clang_wrapper_path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
    return clang_wrapper_path

# Capture vcvarsall.bat environment variables for MSVC binaries, INCLUDE and LIB dir
# Adapted from msvc.py in waf 1.5.9
def _capture_vcvars_env(conf,target,vcvars):
    batfile=os.path.join(conf.blddir,'waf-print-msvc.bat')
    f=open(batfile,'w')
    f.write("""@echo off
set INCLUDE=
set LIB=
call "%s" %s
echo PATH=%%PATH%%
echo INCLUDE=%%INCLUDE%%
echo LIB=%%LIB%%
"""%(vcvars,target))
    f.close()
    sout=Utils.cmd_output(['cmd','/E:on','/V:on','/C',batfile])
    lines=sout.splitlines()
    for line in lines[0:]:
        if line.startswith('PATH='):
            path=line[5:]
            paths=[p for p in path.split(';') if "MinGW" not in p]
            MSVC_PATH=paths
        elif line.startswith('INCLUDE='):
            MSVC_INCDIR=[i for i in line[8:].split(';')if i]
        elif line.startswith('LIB='):
            MSVC_LIBDIR=[i for i in line[4:].split(';')if i]
    return(MSVC_PATH,MSVC_INCDIR,MSVC_LIBDIR)

# Given path to msvc installation, capture target platform settings
def _get_msvc_target_support(conf, product_dir, version):
    targets=[]
    vcvarsall_path = os.path.join(product_dir, 'VC', 'vcvarsall.bat')

    if os.path.isfile(vcvarsall_path):
        if os.path.isfile(os.path.join(product_dir, 'Common7', 'Tools', 'vsvars32.bat')):
            try:
                targets.append(('x86', ('x86', _capture_vcvars_env(conf, 'x86', vcvarsall_path))))
            except Configure.ConfigurationError:
                pass
        if os.path.isfile(os.path.join(product_dir, 'VC', 'bin', 'amd64', 'vcvars64.bat')):
            try:
                targets.append(('x64', ('amd64', _capture_vcvars_env(conf, 'x64', vcvarsall_path))))
            except:
                pass

    return targets

def _find_msvc_installs():
    import _winreg
    version_pattern = re.compile('^..?\...?')
    installs = []
    for vcver in ['VCExpress', 'VisualStudio']:
        try:
            all_versions = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\Wow6432node\\Microsoft\\' + vcver)
        except WindowsError:
            try:
                all_versions = _winreg.OpenKey(_winreg.HKEY_LOCAL_MACHINE, 'SOFTWARE\\Microsoft\\' + vcver)
            except WindowsError:
                continue
        index = 0
        while 1:
            try:
                version = _winreg.EnumKey(all_versions, index)
            except WindowsError:
                break
            index = index + 1
            if not version_pattern.match(version):
                continue
            try:
                msvc_version = _winreg.OpenKey(all_versions, version + "\\Setup\\VS")
                path, type = _winreg.QueryValueEx(msvc_version, 'ProductDir')
                path = str(path)
                installs.append((version, path))
            except WindowsError:
                continue
    return list(set(installs))

def get_msvc_version(conf, platform):
    dynamo_home = conf.env['DYNAMO_HOME']
    msvcdir = os.path.join(dynamo_home, 'ext', 'SDKs', 'Win32', 'MicrosoftVisualStudio14.0')
    windowskitsdir = os.path.join(dynamo_home, 'ext', 'SDKs', 'Win32', 'WindowsKits')

    if not os.path.exists(msvcdir):
        msvcdir = os.path.normpath(os.path.join(os.environ['VS140COMNTOOLS'], '..', '..'))
        windowskitsdir = os.path.join(os.environ['ProgramFiles(x86)'], 'Windows Kits')

    target_map = {'win32': 'x86',
                   'x86_64-win32': 'x64'}
    platform_map = {'win32': '',
                    'x86_64-win32': 'amd64'}

    msvc_path = (os.path.join(msvcdir,'VC','bin', platform_map[platform]),
                os.path.join(windowskitsdir,'8.1','bin',target_map[platform]))

    # Since the programs(Windows!) can update, we do this dynamically
    ucrt_dirs = [ x for x in os.listdir(os.path.join(windowskitsdir,'10','Include'))]
    ucrt_dirs = [ x for x in ucrt_dirs if x.startswith('10.0')]
    ucrt_dirs.sort(key=lambda x: int((x.split('.'))[2]))
    ucrt_version = ucrt_dirs[-1]
    if not ucrt_version.startswith('10.0'):
        conf.fatal("Unable to determine ucrt version: '%s'" % ucrt_version)

    includes = [os.path.join(msvcdir,'VC','include'),
                os.path.join(msvcdir,'VC','atlmfc','include'),
                os.path.join(windowskitsdir,'10','Include',ucrt_version,'ucrt'),
                os.path.join(windowskitsdir,'8.1','Include','winrt'),
                os.path.join(windowskitsdir,'8.1','Include','um'),
                os.path.join(windowskitsdir,'8.1','Include','shared')]
    libdirs = [ os.path.join(msvcdir,'VC','lib','amd64'),
                os.path.join(msvcdir,'VC','atlmfc','lib','amd64'),
                os.path.join(windowskitsdir,'10','Lib',ucrt_version,'ucrt','x64'),
                os.path.join(windowskitsdir,'8.1','Lib','winv6.3','um','x64')]
    if platform == 'win32':
        libdirs = [os.path.join(msvcdir,'VC','lib'),
                    os.path.join(msvcdir,'VC','atlmfc','lib'),
                    os.path.join(windowskitsdir,'10','Lib',ucrt_version,'ucrt','x86'),
                    os.path.join(windowskitsdir,'8.1','Lib','winv6.3','um','x86')]

    return msvc_path, includes, libdirs

def remove_flag(arr, flag, nargs):
    if not flag in arr:
        return
    index = arr.index(flag)
    del arr[index]
    for i in range(nargs):
        del arr[index]

def detect(conf):
    conf.find_program('valgrind', var='VALGRIND', mandatory = False)
    conf.find_program('ccache', var='CCACHE', mandatory = False)
    conf.find_program('nodejs', var='NODEJS', mandatory = False)
    if not conf.env['NODEJS']:
        conf.find_program('node', var='NODEJS', mandatory = False)

    platform = None
    if getattr(Options.options, 'platform', None):
        platform=getattr(Options.options, 'platform')

    if sys.platform == "darwin":
        build_platform = "darwin"
    elif sys.platform == "linux2":
        build_platform = "linux"
    elif sys.platform == "win32":
        build_platform = "win32"
    else:
        conf.fatal("Unable to determine host platform")

    if not platform:
        platform = build_platform

    if platform == "win32":
        bob_build_platform = "x86-win32"
    elif platform == "linux":
        bob_build_platform = "x86-linux"
    elif platform == "darwin":
        bob_build_platform = "x86-darwin"
    else:
        bob_build_platform = platform

    conf.env['BOB_BUILD_PLATFORM'] = bob_build_platform
    conf.env['PLATFORM'] = platform
    conf.env['BUILD_PLATFORM'] = build_platform

    try:
        build_util = create_build_utility(conf.env)
    except BuildUtilityException as ex:
        conf.fatal(ex.msg)

    dynamo_home = build_util.get_dynamo_home()
    conf.env['DYNAMO_HOME'] = dynamo_home

    # Vulkan support
    if Options.options.with_vulkan and build_util.get_target_platform() in ('armv7-darwin','x86_64-ios','js-web','wasm-web'):
        conf.fatal('Vulkan is unsupported on %s' % build_util.get_target_platform())

    if 'win32' in platform:
        if platform == 'x86_64-win32':
            conf.env['MSVC_INSTALLED_VERSIONS'] = [('msvc 14.0',[('x64', ('amd64', get_msvc_version(conf, 'x86_64-win32')))])]
        else:
            conf.env['MSVC_INSTALLED_VERSIONS'] = [('msvc 14.0',[('x86', ('x86', get_msvc_version(conf, 'win32')))])]

        target_map = {'win32': 'x86', 'x86_64-win32': 'x64'}
        windowskitsdir = os.path.join(dynamo_home, 'ext', 'SDKs', 'Win32', 'WindowsKits')
        if not os.path.exists(windowskitsdir):
            windowskitsdir = os.path.join(os.environ['ProgramFiles(x86)'], 'Windows Kits')
        conf.find_program('signtool', var='SIGNTOOL', mandatory = True, path_list = [os.path.join(windowskitsdir, '8.1','bin', target_map[platform] )])

    if  build_util.get_target_os() in ('osx', 'ios'):
        conf.find_program('dsymutil', var='DSYMUTIL', mandatory = True) # or possibly llvm-dsymutil
        conf.find_program('zip', var='ZIP', mandatory = True)

    if 'osx' == build_util.get_target_os():
        # Force gcc without llvm on darwin.
        # We got strange bugs with http cache with gcc-llvm...
        os.environ['CC'] = 'clang'
        os.environ['CXX'] = 'clang++'

        conf.env['CC']      = '%s/usr/bin/clang' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['CXX']     = '%s/usr/bin/clang++' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['LINK_CXX']= '%s/usr/bin/clang++' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['CPP']     = '%s/usr/bin/clang -E' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['AR']      = '%s/usr/bin/ar' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['RANLIB']  = '%s/usr/bin/ranlib' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['LD']      = '%s/usr/bin/ld' % (DARWIN_TOOLCHAIN_ROOT)

    elif 'ios' == build_util.get_target_os() and build_util.get_target_architecture() in ('armv7','arm64','x86_64'):
        # Wrap clang in a bash-script due to a bug in clang related to cwd
        # waf change directory from ROOT to ROOT/build when building.
        # clang "thinks" however that cwd is ROOT instead of ROOT/build
        # This bug is at least prevalent in "Apple clang version 3.0 (tags/Apple/clang-211.12) (based on LLVM 3.0svn)"

        # NOTE: If we are to use clang for OSX-builds the wrapper script must be qualifed, e.g. clang-ios.sh or similar
        clang_wrapper = create_clang_wrapper(conf, 'clang')
        clangxx_wrapper = create_clang_wrapper(conf, 'clang++')

        conf.env['GCC-OBJCXX'] = '-xobjective-c++'
        conf.env['GCC-OBJCLINK'] = '-lobjc'
        conf.env['CC'] = clang_wrapper
        conf.env['CXX'] = clangxx_wrapper
        conf.env['LINK_CXX'] = '%s/usr/bin/clang++' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['CPP'] = '%s/usr/bin/clang -E' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['AR'] = '%s/usr/bin/ar' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['RANLIB'] = '%s/usr/bin/ranlib' % (DARWIN_TOOLCHAIN_ROOT)
        conf.env['LD'] = '%s/usr/bin/ld' % (DARWIN_TOOLCHAIN_ROOT)
    elif 'android' == build_util.get_target_os() and build_util.get_target_architecture() in ('armv7', 'arm64'):
        # TODO: No windows support yet (unknown path to compiler when wrote this)
        arch        = 'x86_64'
        target_arch = build_util.get_target_architecture()
        tool_name   = getAndroidBuildtoolName(target_arch)
        api_version = getAndroidNDKAPIVersion(target_arch)
        clang_name  = getAndroidCompilerName(target_arch, api_version)
        bintools    = '%s/toolchains/llvm/prebuilt/%s-%s/bin' % (ANDROID_NDK_ROOT, build_platform, arch)

        conf.env['CC']       = '%s/%s' % (bintools, clang_name)
        conf.env['CXX']      = '%s/%s++' % (bintools, clang_name)
        conf.env['LINK_CXX'] = '%s/%s++' % (bintools, clang_name)
        conf.env['CPP']      = '%s/%s -E' % (bintools, clang_name)
        conf.env['AR']       = '%s/%s-ar' % (bintools, tool_name)
        conf.env['RANLIB']   = '%s/%s-ranlib' % (bintools, tool_name)
        conf.env['LD']       = '%s/%s-ld' % (bintools, tool_name)
        conf.env['DX']       = '%s/android-sdk/build-tools/%s/dx' % (ANDROID_ROOT, ANDROID_BUILD_TOOLS_VERSION)
    elif 'linux' == build_util.get_target_os():
        conf.find_program('gcc-5', var='GCC5', mandatory = False)
        if conf.env.GCC5 and "gcc-5" in conf.env.GCC5:
            conf.env.CXX = "g++-5"
            conf.env.CC = "gcc-5"
            conf.env.CPP = "cpp-5"
            conf.env.AR = "gcc-ar-5"
            conf.env.RANLIB = "gcc-ranlib-5"
        else:
            conf.env.CXX = "g++"
            conf.env.CC = "gcc"
            conf.env.CPP = "cpp"

    conf.check_tool('compiler_cc')
    conf.check_tool('compiler_cxx')

    # Since we're using an old waf version, we remove unused arguments
    remove_flag(conf.env['shlib_CCFLAGS'], '-compatibility_version', 1)
    remove_flag(conf.env['shlib_CCFLAGS'], '-current_version', 1)
    remove_flag(conf.env['shlib_CXXFLAGS'], '-compatibility_version', 1)
    remove_flag(conf.env['shlib_CXXFLAGS'], '-current_version', 1)

    # NOTE: We override after check_tool. Otherwise waf gets confused and CXX_NAME etc are missing..
    if 'web' == build_util.get_target_os() and ('js' == build_util.get_target_architecture() or 'wasm' == build_util.get_target_architecture()):
        bin = os.environ.get('EMSCRIPTEN')
        if None == bin:
            conf.fatal('EMSCRIPTEN environment variable does not exist')
        conf.env['EMSCRIPTEN'] = bin
        conf.env['CC'] = '%s/emcc' % (bin)
        conf.env['CXX'] = '%s/em++' % (bin)
        conf.env['LINK_CXX'] = '%s/em++' % (bin)
        conf.env['CPP'] = '%s/em++' % (bin)
        conf.env['AR'] = '%s/emar' % (bin)
        conf.env['RANLIB'] = '%s/emranlib' % (bin)
        conf.env['LD'] = '%s/emcc' % (bin)
        conf.env['program_PATTERN']='%s.js'

    if Options.options.static_analyze:
        conf.find_program('scan-build', var='SCANBUILD', mandatory = True, path_list=['/usr/local/opt/llvm/bin'])
        output_dir = os.path.normpath(os.path.join(os.environ['DYNAMO_HOME'], '..', '..', 'static_analyze'))
        for t in ['CC', 'CXX']:
            c = conf.env[t]
            if type(c) == list:
                conf.env[t] = [conf.env.SCANBUILD, '-k','-o',output_dir] + c
            else:
                conf.env[t] = [conf.env.SCANBUILD, '-k','-o',output_dir, c]

    if conf.env['CCACHE'] and not 'win' == build_util.get_target_os():
        if not Options.options.disable_ccache:
            # Prepend gcc/g++ with CCACHE
            for t in ['CC', 'CXX']:
                c = conf.env[t]
                if type(c) == list:
                    conf.env[t] = [conf.env.CCACHE] + c
                else:
                    conf.env[t] = [conf.env.CCACHE, c]
        else:
            Logs.info('ccache disabled')

    conf.env.BINDIR = Utils.subst_vars('${PREFIX}/bin/%s' % build_util.get_target_platform(), conf.env)
    conf.env.LIBDIR = Utils.subst_vars('${PREFIX}/lib/%s' % build_util.get_target_platform(), conf.env)

    if re.match('.*?linux', platform):
        conf.env['LIB_PLATFORM_SOCKET'] = ''
        conf.env['LIB_DL'] = 'dl'
        conf.env['LIB_UUID'] = 'uuid'
    elif 'darwin' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif 'android' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif platform == 'win32':
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32 Iphlpapi AdvAPI32'.split()
    else:
        conf.env['LIB_PLATFORM_SOCKET'] = ''

    use_vanilla = getattr(Options.options, 'use_vanilla_lua', False)
    if build_util.get_target_os() == 'web':
        use_vanilla = True

    conf.env['LUA_BYTECODE_ENABLE_32'] = 'no'
    conf.env['LUA_BYTECODE_ENABLE_64'] = 'no'
    if use_vanilla:
        conf.env['STATICLIB_LUA'] = 'lua'
    else:
        conf.env['STATICLIB_LUA'] = 'luajit-5.1'
        if build_util.get_target_platform() == 'x86_64-linux' or build_util.get_target_platform() == 'x86_64-win32' or build_util.get_target_platform() == 'x86_64-darwin' or build_util.get_target_platform() == 'arm64-android' or build_util.get_target_platform() == 'arm64-darwin':
            conf.env['LUA_BYTECODE_ENABLE_64'] = 'yes'
        else:
            conf.env['LUA_BYTECODE_ENABLE_32'] = 'yes'

    conf.env['STATICLIB_CARES'] = []
    if platform != 'web':
        conf.env['STATICLIB_CARES'].append('cares')
    if platform in ('armv7-darwin','arm64-darwin','x86_64-ios'):
        conf.env['STATICLIB_CARES'].append('resolv')

def configure(conf):
    detect(conf)

old = Build.BuildContext.exec_command
def exec_command(self, cmd, **kw):
    if getattr(Options.options, 'eclipse', False):
        if isinstance(cmd, list):
            print >>sys.stderr, ' '.join(cmd)
        else:
            print >>sys.stderr, cmd
    return old(self, cmd, **kw)

Build.BuildContext.exec_command = exec_command

def set_options(opt):
    opt.tool_options('compiler_cc')
    opt.tool_options('compiler_cxx')

    opt.add_option('--eclipse', action='store_true', default=False, dest='eclipse', help='print eclipse friendly command-line')
    opt.add_option('--platform', default='', dest='platform', help='target platform, eg armv7-darwin')
    opt.add_option('--skip-tests', action='store_true', default=False, dest='skip_tests', help='skip running unit tests')
    opt.add_option('--skip-build-tests', action='store_true', default=False, dest='skip_build_tests', help='skip building unit tests')
    opt.add_option('--skip-codesign', action="store_true", default=False, dest='skip_codesign', help='skip code signing')
    opt.add_option('--skip-apidocs', action='store_true', default=False, dest='skip_apidocs', help='skip extraction and generation of API docs.')
    opt.add_option('--disable-ccache', action="store_true", default=False, dest='disable_ccache', help='force disable of ccache')
    opt.add_option('--use-vanilla-lua', action="store_true", default=False, dest='use_vanilla_lua', help='use luajit')
    opt.add_option('--disable-feature', action='append', default=[], dest='disable_features', help='disable feature, --disable-feature=foo')
    opt.add_option('--opt-level', default="2", dest='opt_level', help='optimization level')
    opt.add_option('--ndebug', action='store_true', default=False, help='Defines NDEBUG for the engine')
    opt.add_option('--with-asan', action='store_true', default=False, dest='with_asan', help='Enables address sanitizer')
    opt.add_option('--static-analyze', action='store_true', default=False, dest='static_analyze', help='Enables static code analyzer')
    opt.add_option('--with-valgrind', action='store_true', default=False, dest='with_valgrind', help='Enables usage of valgrind')
    opt.add_option('--with-vulkan', action='store_true', default=False, dest='with_vulkan', help='Enables Vulkan as graphics backend')
