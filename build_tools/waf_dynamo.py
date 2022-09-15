# Copyright 2020-2022 The Defold Foundation
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

import os, sys, subprocess, shutil, re, stat, glob, zipfile
from waflib.Configure import conf
from waflib import Utils, Build, Options, Task, Logs
from waflib.TaskGen import extension, feature, after, before, task_gen
from waflib.Logs import error
from waflib.Task import RUN_ME
from BuildUtility import BuildUtility, BuildUtilityException, create_build_utility
import sdk

if not 'DYNAMO_HOME' in os.environ:
    print ("You must define DYNAMO_HOME. Have you run './script/build.py shell' ?", file=sys.stderr)
    sys.exit(1)

def is_platform_private(platform):
    return platform in ['arm64-nx64']

for platform in ('nx64', 'ps4'):
    path = os.path.join(os.path.dirname(__file__), 'waf_dynamo_%s.py' % platform)
    if not os.path.exists(path):
        continue
    import imp
    sys.dont_write_bytecode = True
    imp.load_source('waf_dynamo_private', path)
    print("Imported %s from %s" % ('waf_dynamo_private', path))
    import waf_dynamo_private
    sys.dont_write_bytecode = False
    break

if 'waf_dynamo_private' not in sys.modules:
    class waf_dynamo_private(object):
        @classmethod
        def options(cls, opt):
            pass
        @classmethod
        def setup_tools(cls, ctx, build_util):
            pass
        @classmethod
        def setup_vars(cls, ctx, build_util):
            pass
        @classmethod
        def supports_feature(cls, platform, feature, data):
            return True
        @classmethod
        def transform_runnable_path(cls, platform, path):
            return path
    globals()['waf_dynamo_private'] = waf_dynamo_private

def platform_supports_feature(platform, feature, data):
    if feature == 'vulkan':
        return platform not in ['js-web', 'wasm-web', 'x86_64-ios', 'x86_64-linux']
    return waf_dynamo_private.supports_feature(platform, feature, data)

def platform_setup_tools(ctx, build_util):
    return waf_dynamo_private.setup_tools(ctx, build_util)

def platform_setup_vars(ctx, build_util):
    return waf_dynamo_private.setup_vars(ctx, build_util)

def transform_runnable_path(platform, path):
    return waf_dynamo_private.transform_runnable_path(platform, path)

# Note that some of these version numbers are also present in build.py (TODO: put in a waf_versions.py or similar)
# The goal is to put the sdk versions in sdk.py
SDK_ROOT=sdk.SDK_ROOT

ANDROID_ROOT=SDK_ROOT
ANDROID_BUILD_TOOLS_VERSION = '32.0.0'
ANDROID_NDK_API_VERSION='16' # Android 4.1
ANDROID_NDK_ROOT=os.path.join(SDK_ROOT,'android-ndk-r%s' % sdk.ANDROID_NDK_VERSION)
ANDROID_TARGET_API_LEVEL='31' # Android 12.0
ANDROID_MIN_API_LEVEL='16'
ANDROID_GCC_VERSION='4.9'
ANDROID_64_NDK_API_VERSION='21' # Android 5.0
EMSCRIPTEN_ROOT=os.environ.get('EMSCRIPTEN', '')

CLANG_VERSION='clang-13.0.0'

SDK_ROOT=sdk.SDK_ROOT
LINUX_TOOLCHAIN_ROOT=os.path.join(SDK_ROOT, 'linux')

# Workaround for a strange bug with the combination of ccache and clang
# Without CCACHE_CPP2 set breakpoint for source locations can't be set, e.g. b main.cpp:1234
os.environ['CCACHE_CPP2'] = 'yes'

def new_copy_task(name, input_ext, output_ext):
    def compile(task):
        with open(task.inputs[0].srcpath(), 'rb') as in_f:
            with open(task.outputs[0].abspath(), 'wb') as out_f:
                out_f.write(in_f.read())
        return 0

    task = Task.task_factory(name,
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
    return bld(rule = '%s %s ${TGT}' % (copy, src_path),
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

    import waflib.Node
    from itertools import chain
    from collections import defaultdict
    elements = {}
    def extract_docs(bld, src):
        ret = defaultdict(list)
        # Gather data
        for s in src:
            n = bld.path.find_resource(s)
            if not n:
                print("Couldn't find resource: %s" % s)
                continue
            with open(n.abspath(), encoding='utf8') as in_f:
                source = in_f.read()
                for k,v in chain(elements.items(), ns_elements(source).items()):
                    if k == None:
                        print("Missing namespace definition in " + n.abspath())
                    ret[k] = ret[k] + v
        return ret

    def write_docs(task):
        # Write all namespace files
        for o in task.outputs:
            ns = os.path.splitext(o.name)[0]
            with open(str(o.get_bld()), 'w+') as out_f:
                out_f.write('\n'.join(elements[ns]))

    if not getattr(Options.options, 'skip_apidocs', False):
        elements = extract_docs(bld, src)
        target = []
        for ns in elements.keys():
            if ns is not None:
                target.append(ns + '.apidoc')
        return bld(rule=write_docs, name='apidoc_extract', source = src, target = target)


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
    d = bld.path.find_dir(source)
    if d is None:
        print("Could not find source file/dir '%s' from dir '%s'" % (source,bld.path.abspath()))
        sys.exit(1)
    bld_sdk_files = d.abspath()
    bld_path = bld.path.abspath()
    doc_files = []
    for root, dirs, files in os.walk(bld_sdk_files):
        for f in files:
            if f.endswith('.DS_Store'):
                continue;
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
    if target_arch == 'arm64':
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

# from osx.py
def apply_framework(self):
    for x in self.to_list(self.env['FRAMEWORKPATH']):
        frameworkpath_st='-F%s'
        self.env.append_unique('CXXFLAGS',frameworkpath_st%x)
        self.env.append_unique('CFLAGS',frameworkpath_st%x)
        self.env.append_unique('LINKFLAGS',frameworkpath_st%x)
    for x in self.to_list(self.env['FRAMEWORK']):
        self.env.append_value('LINKFLAGS',['-framework',x])

feature('c','cxx')(apply_framework)
after('process_source')(apply_framework)

@feature('c', 'cxx')
# We must apply this before the objc_hook below
# Otherwise will .mm-files not be compiled with -arch armv7 etc.
# I don't know if this is entirely correct
@before('process_source')
def default_flags(self):
    build_util = create_build_utility(self.env)

    opt_level = Options.options.opt_level
    if opt_level == "2" and 'web' == build_util.get_target_os():
        opt_level = "3" # emscripten highest opt level
    elif opt_level == "0" and 'win' in build_util.get_target_os():
        opt_level = "d" # how to disable optimizations in windows

    # For nicer output (i.e. in CI logs), and still get some performance, let's default to -O1
    if (Options.options.with_asan or Options.options.with_ubsan or Options.options.with_tsan) and opt_level != '0':
        opt_level = 1

    FLAG_ST = '/%s' if 'win' == build_util.get_target_os() else '-%s'

    # Common for all platforms
    flags = []
    if Options.options.ndebug:
        flags += [self.env.DEFINES_ST % 'NDEBUG']

    for f in ['CFLAGS', 'CXXFLAGS', 'LINKFLAGS']:
        self.env.append_value(f, [FLAG_ST % ('O%s' % opt_level)])

    if Options.options.show_includes:
        if 'win' == build_util.get_target_os():
            flags += ['/showIncludes']
        else:
            flags += ['-H']

    for f in ['CFLAGS', 'CXXFLAGS']:
        self.env.append_value(f, flags)

    use_cl_exe = build_util.get_target_platform() in ['win32', 'x86_64-win32']

    if not use_cl_exe:
        self.env.append_value('CXXFLAGS', ['-std=c++11']) # Due to Basis library

    if os.environ.get('GITHUB_WORKFLOW', None) is not None:
       for f in ['CFLAGS', 'CXXFLAGS']:
           self.env.append_value(f, self.env.DEFINES_ST % "GITHUB_CI")
           self.env.append_value(f, self.env.DEFINES_ST % "JC_TEST_USE_COLORS=1")

    for f in ['CFLAGS', 'CXXFLAGS']:
        if '64' in build_util.get_target_architecture():
            self.env.append_value(f, self.env.DEFINES_ST % 'DM_PLATFORM_64BIT')
        else:
            self.env.append_value(f, self.env.DEFINES_ST % 'DM_PLATFORM_32BIT')

    if not hasattr(self, 'sdkinfo'):
        self.sdkinfo = sdk.get_sdk_info(SDK_ROOT, build_util.get_target_platform())

    libpath = build_util.get_library_path()

    # Create directory in order to avoid warning 'ld: warning: directory not found for option' before first install
    if not os.path.exists(libpath):
        os.makedirs(libpath)
    self.env.append_value('LIBPATH', libpath)

    self.env.append_value('INCLUDES', build_util.get_dynamo_ext('include'))
    self.env.append_value('INCLUDES', build_util.get_dynamo_home('sdk','include'))
    self.env.append_value('INCLUDES', build_util.get_dynamo_home('include'))
    self.env.append_value('INCLUDES', build_util.get_dynamo_home('include', build_util.get_target_platform()))
    self.env.append_value('INCLUDES', build_util.get_dynamo_ext('include', build_util.get_target_platform()))
    self.env.append_value('LIBPATH', build_util.get_dynamo_ext('lib', build_util.get_target_platform()))

    # Platform specific paths etc comes after the project specific stuff

    if build_util.get_target_os() in ('macos', 'ios'):
        self.env.append_value('LINKFLAGS', ['-weak_framework', 'Foundation'])
        if 'ios' == build_util.get_target_os():
            self.env.append_value('LINKFLAGS', ['-framework', 'UIKit', '-framework', 'SystemConfiguration', '-framework', 'AVFoundation'])
        else:
            self.env.append_value('LINKFLAGS', ['-framework', 'AppKit'])

    if "linux" == build_util.get_target_os():
        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-Werror=format', '-fno-exceptions','-fPIC', '-fvisibility=hidden'])

            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti'])

    elif "macos" == build_util.get_target_os():

        sys_root = '%s/MacOSX%s.sdk' % (build_util.get_dynamo_ext('SDKs'), sdk.VERSION_MACOSX)
        swift_dir = "%s/usr/lib/swift-%s/macosx" % (sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), sdk.SWIFT_VERSION)

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-Werror=format', '-fno-exceptions','-fPIC', '-fvisibility=hidden'])

            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti'])

            self.env.append_value(f, ['-stdlib=libc++', '-DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED', '-DGL_SILENCE_DEPRECATION'])
            self.env.append_value(f, '-mmacosx-version-min=%s' % sdk.VERSION_MACOSX_MIN)

            self.env.append_value(f, ['-isysroot', sys_root, '-nostdinc++', '-isystem', '%s/usr/include/c++/v1' % sys_root])
            if self.env['BUILD_PLATFORM'] in ('x86_64-linux', 'x86_64-macos', 'arm64-macos'):
                arch = build_util.get_target_architecture()
                self.env.append_value(f, ['-target', '%s-apple-darwin19' % arch])

        self.env.append_value('LINKFLAGS', ['-stdlib=libc++', '-isysroot', sys_root, '-mmacosx-version-min=%s' % sdk.VERSION_MACOSX_MIN, '-framework', 'Carbon','-flto'])
        self.env.append_value('LIBPATH', ['%s/usr/lib' % sys_root, '%s/usr/lib' % sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), '%s' % swift_dir])

        if 'linux' in self.env['BUILD_PLATFORM']:
            self.env.append_value('LINKFLAGS', ['-target', 'x86_64-apple-darwin19'])

    elif 'ios' == build_util.get_target_os() and build_util.get_target_architecture() in ('armv7', 'arm64', 'x86_64'):

        extra_ccflags = []
        extra_linkflags = []
        if 'linux' in self.env['BUILD_PLATFORM']:
            target_triplet='arm-apple-darwin19'
            extra_ccflags += ['-target', target_triplet]
            extra_linkflags += ['-target', target_triplet, '-L%s' % os.path.join(sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']),'usr/lib/clang/%s/lib/darwin' % self.sdkinfo['xcode-clang']['version']),
                                '-lclang_rt.ios', '-Wl,-force_load', '-Wl,%s' % os.path.join(sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), 'usr/lib/arc/libarclite_iphoneos.a')]
        else:
            extra_linkflags += ['-fobjc-link-runtime']

        #  NOTE: -lobjc was replaced with -fobjc-link-runtime in order to make facebook work with iOS 5 (dictionary subscription with [])
        sys_root = '%s/iPhoneOS%s.sdk' % (build_util.get_dynamo_ext('SDKs'), sdk.VERSION_IPHONEOS)
        swift_dir = "%s/usr/lib/swift-%s/iphoneos" % (sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), sdk.SWIFT_VERSION)
        if 'x86_64' == build_util.get_target_architecture():
            sys_root = '%s/iPhoneSimulator%s.sdk' % (build_util.get_dynamo_ext('SDKs'), sdk.VERSION_IPHONESIMULATOR)
            swift_dir = "%s/usr/lib/swift-%s/iphonesimulator" % (sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), sdk.SWIFT_VERSION)

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, extra_ccflags + ['-g', '-stdlib=libc++', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-fno-exceptions', '-fno-rtti', '-fvisibility=hidden',
                                            '-arch', build_util.get_target_architecture(), '-miphoneos-version-min=%s' % sdk.VERSION_IPHONEOS_MIN])

            self.env.append_value(f, ['-isysroot', sys_root, '-nostdinc++', '-isystem', '%s/usr/include/c++/v1' % sys_root])

            self.env.append_value(f, ['-DDM_PLATFORM_IOS'])
            if 'x86_64' == build_util.get_target_architecture():
                self.env.append_value(f, ['-DIOS_SIMULATOR'])

        self.env.append_value('LINKFLAGS', ['-arch', build_util.get_target_architecture(), '-stdlib=libc++', '-isysroot', sys_root, '-dead_strip', '-miphoneos-version-min=%s' % sdk.VERSION_IPHONEOS_MIN] + extra_linkflags)
        self.env.append_value('LIBPATH', ['%s/usr/lib' % sys_root, '%s/usr/lib' % sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), '%s' % swift_dir])

    elif 'android' == build_util.get_target_os():
        target_arch = build_util.get_target_architecture()

        bp_arch, bp_os = self.env['BUILD_PLATFORM'].split('-')
        sysroot='%s/toolchains/llvm/prebuilt/%s-%s/sysroot' % (ANDROID_NDK_ROOT, bp_os, bp_arch)

        for f in ['CFLAGS', 'CXXFLAGS']:
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

        emflags = ['DISABLE_EXCEPTION_CATCHING=1', 'AGGRESSIVE_VARIABLE_ELIMINATION=1', 'PRECISE_F32=2',
                   'EXTRA_EXPORTED_RUNTIME_METHODS=["stringToUTF8","ccall","stackTrace","UTF8ToString","callMain"]',
                   'ERROR_ON_UNDEFINED_SYMBOLS=1', 'INITIAL_MEMORY=33554432', 'LLD_REPORT_UNDEFINED', 'MAX_WEBGL_VERSION=2',
                   'GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0']

        if 'wasm' == build_util.get_target_architecture():
            emflags += ['WASM=1', 'IMPORTED_MEMORY=1', 'ALLOW_MEMORY_GROWTH=1']
        else:
            emflags += ['WASM=0', 'LEGACY_VM_SUPPORT=1']

        emflags = zip(['-s'] * len(emflags), emflags)
        emflags =[j for i in emflags for j in i]

        flags = []
        linkflags = []
        if int(opt_level) < 2:
            flags = ['-g4']
            linkflags = ['-g4']

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-Wall', '-fPIC', '-fno-exceptions', '-fno-rtti',
                                        '-DGL_ES_VERSION_2_0', '-DGOOGLE_PROTOBUF_NO_RTTI', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS'])
            self.env.append_value(f, emflags)
            self.env.append_value(f, flags)

        self.env.append_value('LINKFLAGS', ['-Wno-warn-absolute-paths', '--emit-symbol-map', '--memory-init-file', '0', '-lidbfs.js'])
        self.env.append_value('LINKFLAGS', emflags)
        self.env.append_value('LINKFLAGS', linkflags)

    elif build_util.get_target_platform() in ['win32', 'x86_64-win32']:
        for f in ['CFLAGS', 'CXXFLAGS']:
            # /Oy- = Disable frame pointer omission. Omitting frame pointers breaks crash report stack trace. /O2 implies /Oy.
            # 0x0600 = _WIN32_WINNT_VISTA
            self.env.append_value(f, ['/Oy-', '/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS', '/DWINVER=0x0600', '/D_WIN32_WINNT=0x0600', '/DNOMINMAX' '/D_CRT_SECURE_NO_WARNINGS', '/wd4996', '/wd4200', '/DUNICODE', '/D_UNICODE'])
        self.env.append_value('LINKFLAGS', '/DEBUG')
        self.env.append_value('LINKFLAGS', ['shell32.lib', 'WS2_32.LIB', 'Iphlpapi.LIB', 'AdvAPI32.Lib', 'Gdi32.lib'])
        self.env.append_unique('ARFLAGS', '/WX')

        # Make sure we prefix with lib*.lib on windows, since this is not done
        # by waf anymore and several extensions rely on them being named that way
        self.env.STLIB_ST         = 'lib%s.lib'
        self.env.cstlib_PATTERN   = 'lib%s.lib'
        self.env.cxxstlib_PATTERN = 'lib%s.lib'

    platform_setup_vars(self, build_util)

    if Options.options.with_iwyu and 'IWYU' in self.env:
        wrapper = build_util.get_dynamo_home('..', '..', 'scripts', 'iwyu-clang.sh')
        for f in ['CC', 'CXX']:
            self.env[f] = [wrapper, self.env[f][0]]


# Used if you wish to be specific about certain default flags for a library (e.g. used for mbedtls library)
@feature('remove_flags')
@before('process_source')
@after('c')
@after('cxx')
def remove_flags_fn(self):
    lookup = getattr(self, 'remove_flags', [])
    for name, values in lookup.items():
        for flag, argcount in values:
            remove_flag(self.env[name], flag, argcount)

@feature('cprogram', 'cxxprogram', 'cstlib', 'cxxstlib', 'cshlib')
@before('process_source')
@after('c')
@after('cxx')
def web_exported_functions(self):

    if 'web' not in self.env.PLATFORM:
        return

    use_crash = hasattr(self, 'use') and 'CRASH' in self.use or self.name in ('crashext', 'crashext_null')

    for name in ('CFLAGS', 'CXXFLAGS', 'LINKFLAGS'):
        arr = self.env[name]

        for i, v in enumerate(arr):
            if v.startswith('EXPORTED_FUNCTIONS'):
                remove_flag_at_index(arr, i-1, 2) # "_main" is exported by default
                break

        if use_crash:
            self.env.append_value(name, ['-s', 'EXPORTED_FUNCTIONS=["_JSWriteDump","_dmExportedSymbols","_main"]'])


@feature('cprogram', 'cxxprogram', 'cstlib', 'cxxstlib', 'cshlib')
@after('apply_obj_vars')
def android_link_flags(self):
    build_util = create_build_utility(self.env)
    if 'android' == build_util.get_target_os():
        self.env.append_value('LINKFLAGS', ['-lm', '-llog', '-lc'])
        self.env.append_value('LINKFLAGS', '-Wl,--no-undefined -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now'.split())

        if 'apk' in self.features:
            # NOTE: This is a hack We change cprogram -> cshlib
            # but it's probably to late. It works for the name though (libX.so and not X)
            self.env.append_value('LINKFLAGS', ['-shared'])

@feature('cprogram', 'cxxprogram')
@before('process_source')
def osx_64_luajit(self):
    # Was previously needed for 64bit OSX, but removed when we updated luajit-2.1.0-beta3,
    # however it is still needed for 64bit iOS Simulator.
    if self.env['PLATFORM'] == 'x86_64-ios':
        self.env.append_value('LINKFLAGS', ['-pagezero_size', '10000', '-image_base', '100000000'])

@feature('skip_asan')
@before('process_source')
def asan_skip(self):
    self.skip_asan = True

@feature('skip_test')
@before('process_source')
def test_skip(self):
    self.skip_test = True

@feature('cprogram', 'cxxprogram', 'cstlib', 'cxxstlib', 'cshlib')
@before('process_source')
@after('asan_skip')
def asan_cxxflags(self):
    if getattr(self, 'skip_asan', False):
        return
    build_util = create_build_utility(self.env)
    if Options.options.with_asan and build_util.get_target_os() in ('macos','ios','android'):
        self.env.append_value('CXXFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope', '-DSANITIZE_ADDRESS'])
        self.env.append_value('CFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope', '-DSANITIZE_ADDRESS'])
        self.env.append_value('LINKFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope'])
    elif Options.options.with_ubsan and build_util.get_target_os() in ('macos','ios','android'):
        self.env.append_value('CXXFLAGS', ['-fsanitize=undefined'])
        self.env.append_value('CFLAGS', ['-fsanitize=undefined'])
        self.env.append_value('LINKFLAGS', ['-fsanitize=undefined'])
    elif Options.options.with_tsan and build_util.get_target_os() in ('macos','ios','android'):
        self.env.append_value('CXXFLAGS', ['-fsanitize=thread'])
        self.env.append_value('CFLAGS', ['-fsanitize=thread'])
        self.env.append_value('LINKFLAGS', ['-fsanitize=thread'])

@task_gen
@feature('cprogram', 'cxxprogram')
@after('apply_link')
def apply_unit_test(self):
    # Do not execute unit-tests tasks (compile and link) when --skip-build-tests is set
    if 'test' in self.features and getattr(Options.options, 'skip_build_tests', False):
            for t in self.tasks:
                t.hasrun = True

@feature('apk')
@before('process_source')
def apply_apk_test(self):
    build_util = create_build_utility(self.env)
    if 'android' == build_util.get_target_os():
        self.features.remove('cprogram')
        self.features.append('cshlib')

# Install all static libraries by default
@feature('cxxstlib', 'cstlib')
@after('default_cc')
@before('process_source')
def default_install_staticlib(self):
    self.install_path = self.env.LIBDIR

@feature('cshlib')
@after('default_cc')
@before('process_source')
def default_install_shlib(self):
    # Force installation dir to LIBDIR.
    # Default on windows is BINDIR
    self.install_path = self.env.LIBDIR

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

    exe_path = task.exe.abspath()
    signed_exe_path = task.signed_exe.abspath()
    shutil.copy(exe_path, signed_exe_path)

    signed_exe_dir = os.path.dirname(signed_exe_path)

    identity = task.env.IDENTITY
    if not identity:
        identity = 'Apple Development'

    mobileprovision = task.provision
    if not mobileprovision:
        mobileprovision = 'engine_profile.mobileprovision'
    mobileprovision_path = os.path.join(task.env['DYNAMO_HOME'], 'share', mobileprovision)
    shutil.copyfile(mobileprovision_path, os.path.join(signed_exe_dir, 'embedded.mobileprovision'))

    entitlements = task.entitlements
    if not entitlements:
        entitlements = 'engine_profile.xcent'
    entitlements_path = os.path.join(task.env['DYNAMO_HOME'], 'share', entitlements)
    resource_rules_plist_file = task.resource_rules_plist.abspath()

    if not hasattr(task.generator, 'sdkinfo'):
        task.generator.sdkinfo = sdk.get_sdk_info(SDK_ROOT, bld.env['PLATFORM'])

    ret = bld.exec_command('CODESIGN_ALLOCATE=%s/usr/bin/codesign_allocate codesign -f -s "%s" --resource-rules=%s --entitlements %s %s' % (sdk.get_toolchain_root(task.generator.sdkinfo, bld.env['PLATFORM']), identity, resource_rules_plist_file, entitlements_path, signed_exe_dir))
    if ret != 0:
        error('Error running codesign')
        return 1

    return 0

Task.task_factory('codesign',
                         func = codesign,
                         #color = 'RED',
                         after  = 'app_bundle')

def app_bundle(task):
    task.info_plist.parent.mkdir()

    info_plist_file = open(task.info_plist.abspath(), 'w')
    bundleid = 'com.defold.%s' % task.exe_name
    if task.bundleid:
        bundleid = task.bundleid
    info_plist_file.write(INFO_PLIST % { 'executable' : task.exe_name, 'bundleid' : bundleid })
    info_plist_file.close()

    resource_rules_plist_file = open(task.resource_rules_plist.abspath(), 'w')
    resource_rules_plist_file.write(RESOURCE_RULES_PLIST)
    resource_rules_plist_file.close()

    return 0

def create_export_symbols(task):
    with open(task.outputs[0].abspath(), 'w') as out_f:
        for name in Utils.to_list(task.exported_symbols):
            print ('extern "C" void %s();' % name, file=out_f)
        print ('extern "C" void dmExportedSymbols() {', file=out_f)
        for name in Utils.to_list(task.exported_symbols):
            print ("    %s();" % name, file=out_f)
        print ("}", file=out_f)

    return 0

task = Task.task_factory('create_export_symbols',
                         func  = create_export_symbols,
                         color = 'PINK',
                         before  = 'c cxx')

task.runnable_status = lambda self: RUN_ME

@task_gen
@feature('cprogram', 'cxxprogram')
@before('process_source')
def export_symbols(self):
    # Force inclusion of symbol, e.g. from static libraries
    # We used to use -Wl,-exported_symbol on Darwin but changed
    # to a more general solution by generating a .cpp-file with
    # a function which references the symbols in question
    Utils.def_attrs(self, exported_symbols=[])
    if not self.exported_symbols:
        return

    exported_symbols = self.path.find_or_declare('__exported_symbols_%d.cpp' % self.idx)

    task = self.create_task('create_export_symbols')
    task.exported_symbols = self.exported_symbols
    task.set_outputs([exported_symbols])

    # Add exported symbols as a dependancy to this task
    if type(self.source) == str:
        sources = []
        for x in self.source.split(' '):
            sources.append(self.path.make_node(x))
        self.source = sources
    self.source.append(exported_symbols)

Task.task_factory('app_bundle',
                         func = app_bundle,
                         vars = ['SRC', 'DST'],
                         #color = 'RED',
                         after  = 'link_task stlink_task')


def _strip_executable(bld, platform, target_arch, path):
    """ Strips the debug symbols from an executable """
    if platform not in ['x86_64-linux','x86_64-macos','arm64-macos','arm64-ios','armv7-android','arm64-android']:
        return 0 # return ok, path is still unstripped

    strip = "strip"
    if 'android' in platform:
        HOME = os.environ['USERPROFILE' if sys.platform == 'win32' else 'HOME']
        ANDROID_HOST = 'linux' if sys.platform == 'linux' else 'darwin'
        build_tool = getAndroidBuildtoolName(target_arch)
        strip = "%s/toolchains/%s-%s/prebuilt/%s-x86_64/bin/%s-strip" % (ANDROID_NDK_ROOT, build_tool, ANDROID_GCC_VERSION, ANDROID_HOST, build_tool)

    return bld.exec_command("%s %s" % (strip, path))

AUTHENTICODE_CERTIFICATE="Midasplayer Technology AB"

def authenticode_certificate_installed(task):
    if Options.options.skip_codesign:
        return 0
    ret = task.exec_command('powershell "Get-ChildItem cert: -Recurse | Where-Object {$_.FriendlyName -Like """%s*"""} | Measure | Foreach-Object { exit $_.Count }"' % AUTHENTICODE_CERTIFICATE, stdout=True, stderr=True)
    return ret > 0

def authenticode_sign(task):
    if Options.options.skip_codesign:
        return
    exe_file = task.inputs[0].abspath()
    exe_file_to_sign = task.inputs[0].change_ext('_to_sign.exe').abspath()
    exe_file_signed = task.outputs[0].abspath()

    ret = task.exec_command('copy /Y %s %s' % (exe_file, exe_file_to_sign), stdout=True, stderr=True)
    if ret != 0:
        error("Unable to copy file before signing")
        return 1

    ret = task.exec_command('"%s" sign /sm /n "%s" /fd sha256 /tr http://timestamp.comodoca.com /td sha256 /d defold /du https://www.defold.com /v %s' % (task.env['SIGNTOOL'], AUTHENTICODE_CERTIFICATE, exe_file_to_sign), stdout=True, stderr=True)
    if ret != 0:
        error("Unable to sign executable")
        return 1

    ret = task.exec_command('move /Y %s %s' % (exe_file_to_sign, exe_file_signed), stdout=True, stderr=True)
    if ret != 0:
        error("Unable to rename file after signing")
        return 1

    return 0

Task.task_factory('authenticode_sign',
                     func = authenticode_sign,
                     after = 'link_task stlink_task')

@task_gen
@feature('authenticode')
def authenticode(self):
    exe_file = self.link_task.outputs[0].abspath(self.env)
    sign_task = self.create_task('authenticode_sign')
    sign_task.set_inputs(self.link_task.outputs)
    sign_task.set_outputs([self.link_task.outputs[0].change_ext('_signed.exe')])

@task_gen
@after('apply_link')
@feature('cprogram', 'cxxprogram')
def create_app_bundle(self):
    if not re.match('arm.*?darwin', self.env['PLATFORM']):
        return

    Utils.def_attrs(self, bundleid = None, provision = None, entitlements = None)

    app_bundle_task = self.create_task('app_bundle')
    app_bundle_task.bundleid = self.bundleid
    app_bundle_task.set_inputs(self.link_task.outputs)

    exe_name = self.link_task.outputs[0].name
    app_bundle_task.exe_name = exe_name

    info_plist = self.path.get_bld().make_node("%s.app/Info.plist" % exe_name)
    app_bundle_task.info_plist = info_plist

    resource_rules_plist = self.path.get_bld().make_node("%s.app/ResourceRules.plist" % exe_name)
    app_bundle_task.resource_rules_plist = resource_rules_plist

    app_bundle_task.set_outputs([info_plist, resource_rules_plist])

    self.app_bundle_task = app_bundle_task

    if not Options.options.skip_codesign and not self.env["CODESIGN_UNSUPPORTED"]:
        signed_exe = self.path.get_bld().make_node("%s.app/%s" % (exe_name, exe_name))

        codesign = self.create_task('codesign')
        codesign.provision = self.provision
        codesign.entitlements = self.entitlements
        codesign.resource_rules_plist = resource_rules_plist
        codesign.set_inputs(self.link_task.outputs)
        codesign.set_outputs(signed_exe)

        codesign.exe = self.link_task.outputs[0]
        codesign.signed_exe = signed_exe


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

    d8 = '%s/android-sdk/build-tools/%s/d8' % (ANDROID_ROOT, ANDROID_BUILD_TOOLS_VERSION)
    dynamo_home = task.env['DYNAMO_HOME']
    android_jar = '%s/ext/share/java/android.jar' % (dynamo_home)

    dex_dir = os.path.dirname(task.classes_dex.abspath())
    root = os.path.normpath(dex_dir)
    libs = os.path.join(root, 'libs')
    bin = os.path.join(root, 'bin')
    bin_cls = os.path.join(bin, 'classes')
    dx_libs = os.path.join(bin, 'dexedLibs')
    gen = os.path.join(root, 'gen')
    jni_dir = os.path.join(root, 'jni')
    native_lib = task.native_lib.get_bld().abspath()
    native_lib_dir = task.native_lib.get_bld().parent.abspath()

    bld.exec_command('mkdir -p %s' % (libs))
    bld.exec_command('mkdir -p %s' % (bin))
    bld.exec_command('mkdir -p %s' % (bin_cls))
    bld.exec_command('mkdir -p %s' % (dx_libs))
    bld.exec_command('mkdir -p %s' % (gen))
    bld.exec_command('mkdir -p %s' % (native_lib_dir))
    bld.exec_command('mkdir -p %s' % (jni_dir))

    shutil.copy(task.native_lib_in.get_bld().abspath(), native_lib)

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
        ret = bld.exec_command('%s --output %s %s' % (d8, dex_dir, ' '.join(dex_input)))
        if ret != 0:
            error('Error running d8')
            return 1

    # strip the executable
    path = task.native_lib.abspath()
    ret = _strip_executable(bld, task.env.PLATFORM, build_util.get_target_architecture(), path)
    if ret != 0:
        error('Error stripping file %s' % path)
        return 1

    with open(task.android_mk.abspath(), 'w') as f:
        print ('APP_ABI := %s' % getAndroidArch(build_util.get_target_architecture()), file=f)

    with open(task.application_mk.abspath(), 'w') as f:
        print ('', file=f)

    with open(task.gdb_setup.abspath(), 'w') as f:
        if 'arm64' == build_util.get_target_architecture():
            print ('set solib-search-path ./libs/arm64-v8a:./obj/local/arm64-v8a/', file=f)
        else:
            print ('set solib-search-path ./libs/armeabi-v7a:./obj/local/armeabi-v7a/', file=f)

    return 0

Task.task_factory('android_package',
                         func = android_package,
                         vars = ['SRC', 'DST'],
                         after  = 'link_task stlink_task')

@task_gen
@after('apply_link')
@feature('apk')
def create_android_package(self):
    if not re.match('arm.*?android', self.env['PLATFORM']):
        return
    Utils.def_attrs(self, android_package = None)

    android_package_task = self.create_task('android_package')
    android_package_task.set_inputs(self.link_task.outputs)
    android_package_task.android_package = self.android_package

    Utils.def_attrs(self, extra_packages = "")
    android_package_task.extra_packages = Utils.to_list(self.extra_packages)

    Utils.def_attrs(self, jars=[])
    android_package_task.jars = Utils.to_list(self.jars)

    exe_name = self.name
    lib_name = self.link_task.outputs[0].name

    android_package_task.exe_name = exe_name

    try:
        build_util = create_build_utility(android_package_task.env)
    except BuildUtilityException as ex:
        android_package_task.fatal(ex.msg)

    if 'arm64' == build_util.get_target_architecture():
        native_lib = self.path.get_bld().make_node("%s.android/libs/arm64-v8a/%s" % (exe_name, lib_name))
    else:
        native_lib = self.path.get_bld().make_node("%s.android/libs/armeabi-v7a/%s" % (exe_name, lib_name))
    android_package_task.native_lib = native_lib
    android_package_task.native_lib_in = self.link_task.outputs[0]
    android_package_task.classes_dex = self.path.get_bld().make_node("%s.android/classes.dex" % (exe_name))

    # NOTE: These files are required for ndk-gdb
    android_package_task.android_mk = self.path.get_bld().make_node("%s.android/jni/Android.mk" % (exe_name))
    android_package_task.application_mk = self.path.get_bld().make_node("%s.android/jni/Application.mk" % (exe_name))
    if 'arm64' == build_util.get_target_architecture():
        android_package_task.gdb_setup = self.path.get_bld().make_node("%s.android/libs/arm64-v8a/gdb.setup" % (exe_name))
    else:
        android_package_task.gdb_setup = self.path.get_bld().make_node("%s.android/libs/armeabi-v7a/gdb.setup" % (exe_name))

    android_package_task.set_outputs([native_lib,
                                      android_package_task.android_mk, android_package_task.application_mk, android_package_task.gdb_setup])

    self.android_package_task = android_package_task

def copy_stub(task):
    with open(task.outputs[0].abspath(), 'w') as out_f:
        out_f.write(ANDROID_STUB)

    return 0

task = Task.task_factory('copy_stub',
                                func  = copy_stub,
                                color = 'PINK',
                                before  = 'c cxx')

task.runnable_status = lambda self: RUN_ME

@task_gen
@before('process_source')
@feature('apk')
def create_copy_glue(self):
    if not re.match('arm.*?android', self.env['PLATFORM']):
        return

    stub = self.path.get_bld().find_or_declare('android_stub.c')
    self.source.append(stub)
    task = self.create_task('copy_stub')
    task.set_outputs([stub])

def embed_build(task):
    symbol = task.inputs[0].name.upper().replace('.', '_').replace('-', '_').replace('@', 'at')
    in_file = open(task.inputs[0].abspath(), 'rb').read()
    cpp_out_file = open(task.outputs[0].abspath(), 'w')
    h_out_file = open(task.outputs[1].abspath(), 'w')

    cpp_str = """
#include <stdint.h>
#include "dlib/align.h"
unsigned char DM_ALIGNED(16) %s[] =
"""
    cpp_out_file.write(cpp_str % (symbol))
    cpp_out_file.write('{\n    ')
    data = in_file
    data_is_binary = type(data) == bytes

    tmp = ''
    for i,x in enumerate(data):
        if data_is_binary:
            tmp += hex(x) + ', '
        else:
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

    if data_is_binary:
        m.update(data)
    else:
        m.update(data.encode('utf-8'))

    task.generator.bld.node_sigs[task.inputs[0]] = m.digest()

    return 0

Task.task_factory('dex', '${DX} --dex --output ${TGT} ${SRC}',
                      color='YELLOW',
                      after='jar_files',
                      shell=True)

@task_gen
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

Task.task_factory('embed_file',
                  func = embed_build,
                  vars = ['SRC', 'DST'],
                  color = 'RED',
                  before  = 'c cxx')

@feature('embed')
@before('process_source')
def embed_file(self):
    Utils.def_attrs(self, embed_source=[])
    embed_out_nodes = []

    for name in Utils.to_list(self.embed_source):
        Logs.info("Embedding '%s' ..." % name)
        node = self.path.find_resource(name)

        if node == None:
            Logs.info("File %s was not found in %s" % (name, self.path.abspath()))

        cc_out = node.parent.find_or_declare([node.name + '.embed.cpp'])
        h_out = node.parent.find_or_declare([node.name + '.embed.h'])

        task = self.create_task('embed_file', node, [cc_out, h_out])
        embed_out_nodes.append(cc_out)

    # some sources are added as nodes and some are not
    # so we have to make sure we only have nodes in the source list
    source_nodes = []
    for x in Utils.to_list(self.source):
        if type(x) == str:
            source_nodes.append(self.path.find_node(x))
        else:
            source_nodes.append(x)

    # Add dependency on generated embed source files to the task gen
    self.source = source_nodes + embed_out_nodes

def do_find_file(file_name, path_list):
    for directory in Utils.to_list(path_list):
        found_file_name = os.path.join(directory,file_name)
        if os.path.exists(found_file_name):
            return found_file_name

@conf
def find_file(self, file_name, path_list = [], var = None, mandatory = False):
    if not path_list: path_list = os.environ['PATH'].split(os.pathsep)
    ret = do_find_file(file_name, path_list)

    # JG: Maybe fix this. Not sure it's the correct conversion to 'check_message'
    self.start_msg('Checking for file %s' % (file_name))
    self.end_msg(ret)
    if var: self.env[var] = ret

    if not ret and mandatory:
        self.fatal('The file %s could not be found' % file_name)

    return ret

def run_tests(ctx, valgrind = False, configfile = None):
    if ctx == None or getattr(Options.options, 'skip_tests', False):
        return

    # TODO: Add something similar to this
    # http://code.google.com/p/v8/source/browse/trunk/tools/run-valgrind.py
    # to find leaks and set error code

    if not ctx.env['VALGRIND']:
        valgrind = False

    if not getattr(Options.options, 'with_valgrind', False):
        valgrind = False

    if 'web' in ctx.env.PLATFORM and not ctx.env['NODEJS']:
        Logs.info('Not running tests. node.js not found')
        return

    for t in ctx.get_all_task_gen():
        if 'test' in str(t.features) and t.name.startswith('test_') and ('cprogram' in t.features or 'cxxprogram' in t.features):
            if getattr(t, 'skip_test', False):
                continue

            env = dict(os.environ)
            merged_table = t.env.get_merged_dict()
            keys=list(merged_table.keys())
            for key in keys:
                v = merged_table[key]
                if isinstance(v, str):
                    env[key] = v

            output = t.path
            launch_pattern = '%s %s'
            if 'TEST_LAUNCH_PATTERN' in t.env:
                launch_pattern = t.env.TEST_LAUNCH_PATTERN

            for task in t.tasks:
                if task in ['link_task']:
                    break

            program = transform_runnable_path(ctx.env.PLATFORM, task.outputs[0].abspath())

            cmd = launch_pattern % (program, configfile if configfile else '')
            if 'web' in ctx.env.PLATFORM: # should be moved to TEST_LAUNCH_ARGS
                cmd = '%s %s' % (ctx.env['NODEJS'], cmd)
            # disable shortly during beta release, due to issue with jctest + test_gui
            valgrind = False
            if valgrind:
                dynamo_home = os.getenv('DYNAMO_HOME')
                cmd = "valgrind -q --leak-check=full --suppressions=%s/share/valgrind-python.supp --suppressions=%s/share/valgrind-libasound.supp --suppressions=%s/share/valgrind-libdlib.supp --suppressions=%s/ext/share/luajit/lj.supp --error-exitcode=1 %s" % (dynamo_home, dynamo_home, dynamo_home, dynamo_home, cmd)
            proc = subprocess.Popen(cmd, shell = True, env = env)
            ret = proc.wait()
            if ret != 0:
                print("test failed %s" %(t.target) )
                sys.exit(ret)

@feature('cprogram', 'cxxprogram', 'cstlib', 'cxxstlib', 'cshlib')
@after('apply_obj_vars')
def linux_link_flags(self):
    platform = self.env['PLATFORM']
    if re.match('.*?linux', platform):
        self.env.append_value('LINKFLAGS', ['-lpthread', '-lm', '-ldl'])

@feature('cprogram', 'cxxprogram')
@after('apply_obj_vars')
def js_web_link_flags(self):
    platform = self.env['PLATFORM']
    if 'web' in platform and 'test' in self.features:
        pre_js = os.path.join(self.env['DYNAMO_HOME'], 'share', "js-web-pre.js")
        self.env.append_value('LINKFLAGS', ['--pre-js', pre_js])

@task_gen
@before('process_source')
@feature('test')
def test_flags(self):
    self.install_path = None # the tests shouldn't be installed
    # When building tests for the web, we disable emission of emscripten js.mem init files,
    # as the assumption when these are loaded is that the cwd will contain these items.
    if 'web' in self.env['PLATFORM']:
        for f in ['CFLAGS', 'CXXFLAGS', 'LINKFLAGS']:
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
            self.env.append_value('LINKFLAGS', ['--js-library', js])

Task.task_factory('dSYM', '${DSYMUTIL} -o ${TGT} ${SRC}',
                      color='YELLOW',
                      after='link_task',
                      shell=True)

Task.task_factory('DSYMZIP', '${ZIP} -r ${TGT} ${SRC}',
                      color='BROWN',
                      after='dSYM') # Not sure how I could ensure this task running after the dSYM task /MAWE

@feature('extract_symbols')
@after('cprogram', 'cxxprogram')
def extract_symbols(self):
    platform = self.env['PLATFORM']
    if not ('macos' in platform or 'ios' in platform):
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

def remove_flag_at_index(arr, index, count):
    for i in range(count):
        del arr[index]

def remove_flag(arr, flag, nargs):
    while flag in arr:
        index = arr.index(flag)
        remove_flag_at_index(arr, index, nargs+1)

def detect(conf):
    conf.find_program('valgrind', var='VALGRIND', mandatory = False)
    conf.find_program('ccache', var='CCACHE', mandatory = False)

    if Options.options.with_iwyu:
        conf.find_program('include-what-you-use', var='IWYU', mandatory = False)

    build_platform = sdk.get_host_platform()

    platform = None
    if getattr(Options.options, 'platform', None):
        platform=getattr(Options.options, 'platform')

    if not platform:
        platform = build_platform

    if platform == "win32":
        bob_build_platform = "x86_64-win32"
    elif platform == "linux":
        bob_build_platform = "x86_64-linux"
    elif platform == "darwin":
        bob_build_platform = "x86_64-macos"
    else:
        bob_build_platform = platform

    conf.env['BOB_BUILD_PLATFORM'] = bob_build_platform # used in waf_gamesys.py TODO: Remove the need for BOB_BUILD_PLATFORM (weird names)
    conf.env['PLATFORM'] = platform
    conf.env['BUILD_PLATFORM'] = build_platform

    if platform in ('js-web', 'wasm-web') and not conf.env['NODEJS']:
        conf.find_program('node', var='NODEJS', mandatory = False)

    try:
        build_util = create_build_utility(conf.env)
    except BuildUtilityException as ex:
        conf.fatal(ex.msg)

    dynamo_home = build_util.get_dynamo_home()
    conf.env['DYNAMO_HOME'] = dynamo_home

    sdkinfo = sdk.get_sdk_info(SDK_ROOT, build_util.get_target_platform())
    sdkinfo_host = sdk.get_sdk_info(SDK_ROOT, build_platform)

    if 'linux' in build_platform and build_util.get_target_platform() in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        conf.env['TESTS_UNSUPPORTED'] = True
        print ("Tests disabled (%s cannot run on %s)" % (build_util.get_target_platform(), build_platform))

        conf.env['CODESIGN_UNSUPPORTED'] = True
        print ("Codesign disabled", Options.options.skip_codesign)

    # Vulkan support
    if Options.options.with_vulkan and build_util.get_target_platform() in ('x86_64-ios','js-web','wasm-web'):
        conf.fatal('Vulkan is unsupported on %s' % build_util.get_target_platform())

    if 'win32' in platform:
        includes = sdkinfo['includes']['path']
        libdirs = sdkinfo['lib_paths']['path']
        bindirs = sdkinfo['bin_paths']['path']

        if platform == 'x86_64-win32':
            conf.env['MSVC_INSTALLED_VERSIONS'] = [('msvc 14.0',[('x64', ('amd64', (bindirs, includes, libdirs)))])]
        else:
            conf.env['MSVC_INSTALLED_VERSIONS'] = [('msvc 14.0',[('x86', ('x86', (bindirs, includes, libdirs)))])]

        if not Options.options.skip_codesign:
            conf.find_program('signtool', var='SIGNTOOL', mandatory = True, path_list = bindirs)

    if build_util.get_target_os() in ('macos', 'ios'):
        path_list = None
        if 'linux' in build_platform:
            path_list=[os.path.join(sdk.get_toolchain_root(sdkinfo_host, build_platform),'bin')]
        else:
            path_list=[os.path.join(sdk.get_toolchain_root(sdkinfo, build_util.get_target_platform()),'usr','bin')]
        conf.find_program('dsymutil', var='DSYMUTIL', mandatory = True, path_list=path_list) # or possibly llvm-dsymutil
        conf.find_program('zip', var='ZIP', mandatory = True)

    if 'macos' == build_util.get_target_os():
        # Force gcc without llvm on darwin.
        # We got strange bugs with http cache with gcc-llvm...
        os.environ['CC'] = 'clang'
        os.environ['CXX'] = 'clang++'

        llvm_prefix = ''
        bin_dir = '%s/usr/bin' % (sdk.get_toolchain_root(sdkinfo, build_util.get_target_platform()))
        if 'linux' in build_platform:
            llvm_prefix = 'llvm-'
            bin_dir = os.path.join(sdk.get_toolchain_root(sdkinfo_host, build_platform),'bin')

        conf.env['CC']      = '%s/clang' % bin_dir
        conf.env['CXX']     = '%s/clang++' % bin_dir
        conf.env['LINK_CC'] = '%s/clang' % bin_dir
        conf.env['LINK_CXX']= '%s/clang++' % bin_dir
        conf.env['CPP']     = '%s/clang -E' % bin_dir
        conf.env['AR']      = '%s/%sar' % (bin_dir, llvm_prefix)
        conf.env['RANLIB']  = '%s/%sranlib' % (bin_dir, llvm_prefix)

    elif 'ios' == build_util.get_target_os() and build_util.get_target_architecture() in ('armv7','arm64','x86_64'):

        # NOTE: If we are to use clang for OSX-builds the wrapper script must be qualifed, e.g. clang-ios.sh or similar
        if 'linux' in build_platform:
            bin_dir=os.path.join(sdk.get_toolchain_root(sdkinfo_host, build_platform),'bin')

            conf.env['CC']      = '%s/clang' % bin_dir
            conf.env['CXX']     = '%s/clang++' % bin_dir
            conf.env['LINK_CC'] = '%s/clang' % bin_dir
            conf.env['LINK_CXX']= '%s/clang++' % bin_dir
            conf.env['CPP']     = '%s/clang -E' % bin_dir
            conf.env['AR']      = '%s/llvm-ar' % bin_dir
            conf.env['RANLIB']  = '%s/llvm-ranlib' % bin_dir

        else:
            bin_dir = '%s/usr/bin' % (sdk.get_toolchain_root(sdkinfo, build_util.get_target_platform()))

            conf.env['CC']      = '%s/clang' % bin_dir
            conf.env['CXX']     = '%s/clang++' % bin_dir
            conf.env['LINK_CC'] = '%s/clang' % bin_dir
            conf.env['LINK_CXX']= '%s/clang++' % bin_dir
            conf.env['CPP']     = '%s/clang -E' % bin_dir
            conf.env['AR']      = '%s/ar' % bin_dir
            conf.env['RANLIB']  = '%s/ranlib' % bin_dir
            conf.env['LD']      = '%s/ld' % bin_dir

        conf.env['GCC-OBJCXX'] = '-xobjective-c++'
        conf.env['GCC-OBJCLINK'] = '-lobjc'


    elif 'android' == build_util.get_target_os() and build_util.get_target_architecture() in ('armv7', 'arm64'):
        # TODO: No windows support yet (unknown path to compiler when wrote this)
        bp_arch, bp_os = build_platform.split('-')
        if bp_os == 'macos':
            bp_os = 'darwin' # the toolset is still called darwin
        target_arch = build_util.get_target_architecture()
        tool_name   = getAndroidBuildtoolName(target_arch)
        api_version = getAndroidNDKAPIVersion(target_arch)
        clang_name  = getAndroidCompilerName(target_arch, api_version)
        bintools    = '%s/toolchains/llvm/prebuilt/%s-%s/bin' % (ANDROID_NDK_ROOT, bp_os, bp_arch)

        conf.env['CC']       = '%s/%s' % (bintools, clang_name)
        conf.env['CXX']      = '%s/%s++' % (bintools, clang_name)
        conf.env['LINK_CXX'] = '%s/%s++' % (bintools, clang_name)
        conf.env['CPP']      = '%s/%s -E' % (bintools, clang_name)
        conf.env['AR']       = '%s/%s-ar' % (bintools, tool_name)
        conf.env['RANLIB']   = '%s/%s-ranlib' % (bintools, tool_name)
        conf.env['LD']       = '%s/%s-ld' % (bintools, tool_name)
        conf.env['DX']       = '%s/android-sdk/build-tools/%s/dx' % (ANDROID_ROOT, ANDROID_BUILD_TOOLS_VERSION)

    elif 'linux' == build_util.get_target_os():
        bin_dir=os.path.join(sdk.get_toolchain_root(sdkinfo, build_util.get_target_platform()),'bin')

        conf.find_program('clang', var='CLANG', mandatory = False, path_list=[bin_dir])

        if conf.env.CLANG:
            conf.env['CC']      = '%s/clang' % bin_dir
            conf.env['CXX']     = '%s/clang++' % bin_dir
            conf.env['CPP']     = '%s/clang -E' % bin_dir
            conf.env['LINK_CC'] = '%s/clang' % bin_dir
            conf.env['LINK_CXX']= '%s/clang++' % bin_dir
            conf.env['AR']      = '%s/llvm-ar' % bin_dir
            conf.env['RANLIB']  = '%s/llvm-ranlib' % bin_dir

        else:
            # Fallback to default compiler
            conf.env.CXX = "clang++"
            conf.env.CC = "clang"
            conf.env.CPP = "clang -E"

    platform_setup_tools(conf, build_util)

    # jg: this whole thing is a 'dirty hack' to be able to pick up our own SDKs
    if 'win32' in platform:
        includes = sdkinfo['includes']['path']
        libdirs = sdkinfo['lib_paths']['path']
        bindirs = sdkinfo['bin_paths']['path']

        bindirs.append(build_util.get_binary_path())
        bindirs.append(build_util.get_dynamo_ext('bin', build_util.get_target_platform()))

        # The JDK dir doesn't get added since we use no_autodetect
        bindirs.append(os.path.join(os.getenv('JAVA_HOME'), 'bin'))

        # there's no lib prefix anymore so we need to set our our lib dir first so we don't
        # pick up the wrong hid.lib from the windows sdk
        libdirs.insert(0, build_util.get_dynamo_home('lib', build_util.get_target_platform()))

        conf.env['PATH']     = bindirs + sys.path + conf.env['PATH']
        conf.env['INCLUDES'] = includes
        conf.env['LIBPATH']  = libdirs
        conf.load('msvc', funs='no_autodetect')

        if not Options.options.skip_codesign:
            conf.find_program('signtool', var='SIGNTOOL', mandatory = True, path_list = bindirs)
    else:
        conf.load('compiler_c')
        conf.load('compiler_cxx')

    # Since we're using an old waf version, we remove unused arguments
    remove_flag(conf.env['shlib_CFLAGS'], '-compatibility_version', 1)
    remove_flag(conf.env['shlib_CFLAGS'], '-current_version', 1)
    remove_flag(conf.env['shlib_CXXFLAGS'], '-compatibility_version', 1)
    remove_flag(conf.env['shlib_CXXFLAGS'], '-current_version', 1)

    # NOTE: We override after check_tool. Otherwise waf gets confused and CXX_NAME etc are missing..
    if platform in ('js-web', 'wasm-web'):
        bin = os.environ.get('EMSCRIPTEN')
        if None == bin:
            conf.fatal('EMSCRIPTEN environment variable does not exist')
        conf.env['EMSCRIPTEN'] = bin
        conf.env['CC'] = '%s/emcc' % (bin)
        conf.env['CXX'] = '%s/em++' % (bin)
        conf.env['LINK_CC'] = '%s/emcc' % (bin)
        conf.env['LINK_CXX'] = '%s/em++' % (bin)
        conf.env['CPP'] = '%s/em++' % (bin)
        conf.env['AR'] = '%s/emar' % (bin)
        conf.env['RANLIB'] = '%s/emranlib' % (bin)
        conf.env['LD'] = '%s/emcc' % (bin)
        conf.env['cprogram_PATTERN']='%s.js'
        conf.env['cxxprogram_PATTERN']='%s.js'

        # Unknown argument: -Bstatic, -Bdynamic
        conf.env['STLIB_MARKER']=''
        conf.env['SHLIB_MARKER']=''

    if platform in ('x86_64-linux','armv7-android','arm64-android'): # Currently the only platform exhibiting the behavior
        conf.env['STLIB_MARKER'] = ['-Wl,-start-group', '-Wl,-Bstatic']
        conf.env['SHLIB_MARKER'] = ['-Wl,-end-group', '-Wl,-Bdynamic']

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
                    conf.env[t] = [conf.env.CCACHE[0]] + c
                else:
                    conf.env[t] = [conf.env.CCACHE[0], c]
        else:
            Logs.info('ccache disabled')

    conf.env.BINDIR = Utils.subst_vars('${PREFIX}/bin/%s' % build_util.get_target_platform(), conf.env)
    conf.env.LIBDIR = Utils.subst_vars('${PREFIX}/lib/%s' % build_util.get_target_platform(), conf.env)

    if platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        conf.load('waf_objectivec')

        # Unknown argument: -Bstatic, -Bdynamic
        conf.env['STLIB_MARKER']=''
        conf.env['SHLIB_MARKER']=''

    if re.match('.*?linux', platform):
        conf.env['LIB_PLATFORM_SOCKET'] = ''
        conf.env['LIB_DL'] = 'dl'
    elif platform in ('x86_64-macos', 'arm64-macos', 'arm64-ios', 'x86_64-ios'):
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif 'android' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif platform == 'win32':
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32 Iphlpapi AdvAPI32'.split()
    else:
        conf.env['LIB_PLATFORM_SOCKET'] = ''

    use_vanilla = getattr(Options.options, 'use_vanilla_lua', False)
    if build_util.get_target_os() == 'web' or not platform_supports_feature(build_util.get_target_platform(), 'luajit', {}):
        use_vanilla = True

    if use_vanilla:
        conf.env['STLIB_LUA'] = 'lua'
    else:
        conf.env['STLIB_LUA'] = 'luajit-5.1'

    conf.env['STLIB_TESTMAIN'] = ['testmain'] # we'll use this for all internal tests/tools

    if platform not in ('x86_64-macos',):
        conf.env['STLIB_UNWIND'] = 'unwind'

    if platform in ('x86_64-macos','arm64-macos'):
        conf.env['FRAMEWORK_OPENGL'] = ['OpenGL', 'AGL']
    elif platform in ('armv7-android', 'arm64-android'):
        conf.env['LIB_OPENGL'] = ['EGL', 'GLESv1_CM', 'GLESv2']
    elif platform in ('win32', 'x86_64-win32'):
        conf.env['LINKFLAGS_OPENGL'] = ['opengl32.lib', 'glu32.lib']
    elif platform in ('x86_64-linux',):
        conf.env['LIB_OPENGL'] = ['GL', 'GLU']

    if platform in ('x86_64-macos','arm64-macos'):
        conf.env['FRAMEWORK_OPENAL'] = ['OpenAL']
    elif platform in ('arm64-ios', 'x86_64-ios'):
        conf.env['FRAMEWORK_OPENAL'] = ['OpenAL', 'AudioToolbox']
    elif platform in ('armv7-android', 'arm64-android'):
        conf.env['LIB_OPENAL'] = ['OpenSLES']
    elif platform in ('win32', 'x86_64-win32'):
        conf.env['LIB_OPENAL'] = ['OpenAL32']
    elif platform in ('x86_64-linux',):
        conf.env['LIB_OPENAL'] = ['openal']

    conf.env['STLIB_DLIB'] = ['dlib', 'mbedtls', 'zip']
    conf.env['STLIB_DDF'] = 'ddf'
    conf.env['STLIB_CRASH'] = 'crashext'
    conf.env['STLIB_CRASH_NULL'] = 'crashext_null'
    conf.env['STLIB_PROFILE'] = ['profile', 'remotery']
    conf.env['STLIB_PROFILE_NULL'] = ['profile_null', 'remotery_null']
    conf.env['DEFINES_PROFILE_NULL'] = ['DM_PROFILE_NULL']
    conf.env['STLIB_PROFILE_NULL_NOASAN'] = ['profile_null_noasan', 'remotery_null_noasan']

    if ('record' not in Options.options.disable_features):
        conf.env['STLIB_RECORD'] = 'record_null'
    else:
        if platform in ('x86_64-linux', 'x86_64-win32', 'x86_64-macos', 'arm64-macos'):
            conf.env['STLIB_RECORD'] = 'record'
            conf.env['LINKFLAGS_RECORD'] = ['vpx.lib']
        else:
            Logs.info("record disabled")
            conf.env['STLIB_RECORD'] = 'record_null'
    conf.env['STLIB_RECORD_NULL'] = 'record_null'

    conf.env['STLIB_GRAPHICS']          = ['graphics', 'graphics_transcoder_basisu', 'basis_transcoder']
    conf.env['STLIB_GRAPHICS_VULKAN']   = ['graphics_vulkan', 'graphics_transcoder_basisu', 'basis_transcoder']
    conf.env['STLIB_GRAPHICS_NULL']     = ['graphics_null', 'graphics_transcoder_null']

    conf.env['STLIB_DMGLFW'] = 'dmglfw'

    if platform in ('x86_64-macos','arm64-macos'):
        vulkan_validation = os.environ.get('DM_VULKAN_VALIDATION',None)
        conf.env['STLIB_VULKAN'] = vulkan_validation and 'vulkan' or 'MoltenVK'
        conf.env['FRAMEWORK_VULKAN'] = ['Metal', 'IOSurface', 'QuartzCore']
        conf.env['FRAMEWORK_DMGLFW'] = ['QuartzCore']
    elif platform in ('arm64-ios','x86_64-ios'):
        conf.env['STLIB_VULKAN'] = 'MoltenVK'
        conf.env['FRAMEWORK_VULKAN'] = 'Metal'
        conf.env['FRAMEWORK_DMGLFW'] = ['QuartzCore', 'OpenGLES', 'CoreVideo', 'CoreGraphics']
    elif platform in ('x86_64-linux',):
        conf.env['SHLIB_VULKAN'] = ['vulkan', 'X11-xcb']
    elif platform in ('armv7-android','arm64-android'):
        conf.env['SHLIB_VULKAN'] = ['vulkan']
    elif platform in ('x86_64-win32','win32'):
        conf.env['LINKFLAGS_VULKAN'] = 'vulkan-1.lib' # because it doesn't have the "lib" prefix

    if platform in ('x86_64-macos','arm64-macos',):
        conf.env['FRAMEWORK_TESTAPP'] = ['AppKit', 'Cocoa', 'IOKit', 'Carbon', 'CoreVideo']
    elif platform in ('armv7-android', 'arm64-android'):
        pass
        #conf.env['STLIB_TESTAPP'] += ['android']
    elif platform in ('x86_64-linux',):
        conf.env['LIB_TESTAPP'] += ['Xext', 'X11', 'Xi', 'pthread']
    elif platform in ('win32', 'x86_64-win32'):
        conf.env['LINKFLAGS_TESTAPP'] = ['user32.lib', 'shell32.lib']

    conf.env['STLIB_DMGLFW'] = 'dmglfw'

    if platform in ('x86_64-win32','win32'):
        conf.env['LINKFLAGS_PLATFORM'] = ['user32.lib', 'shell32.lib', 'xinput9_1_0.lib', 'openal32.lib', 'dbghelp.lib', 'xinput9_1_0.lib']

def configure(conf):
    detect(conf)

old = Build.BuildContext.exec_command
def exec_command(self, cmd, **kw):
    return old(self, cmd, **kw)

Build.BuildContext.exec_command = exec_command

def options(opt):
    opt.load('compiler_c')
    opt.load('compiler_cxx')

    opt.add_option('--platform', default='', dest='platform', help='target platform, eg arm64-ios')
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
    opt.add_option('--with-ubsan', action='store_true', default=False, dest='with_ubsan', help='Enables undefined behavior sanitizer')
    opt.add_option('--with-tsan', action='store_true', default=False, dest='with_tsan', help='Enables thread sanitizer')
    opt.add_option('--with-iwyu', action='store_true', default=False, dest='with_iwyu', help='Enables include-what-you-use tool (if installed)')
    opt.add_option('--show-includes', action='store_true', default=False, dest='show_includes', help='Outputs the tree of includes')
    opt.add_option('--static-analyze', action='store_true', default=False, dest='static_analyze', help='Enables static code analyzer')
    opt.add_option('--with-valgrind', action='store_true', default=False, dest='with_valgrind', help='Enables usage of valgrind')
    opt.add_option('--with-vulkan', action='store_true', default=False, dest='with_vulkan', help='Enables Vulkan as graphics backend')
