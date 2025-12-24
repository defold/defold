# Copyright 2020-2025 The Defold Foundation
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

import os, sys, subprocess, shutil, re, socket, stat, glob, zipfile, tempfile, configparser
from waflib.Configure import conf
from waflib import Utils, Build, Options, Task, Logs
from waflib.TaskGen import extension, feature, after, before, task_gen
from waflib.Logs import error
from waflib.Task import RUN_ME
from BuildUtility import BuildUtility, BuildUtilityException, create_build_utility
from build_constants import TargetOS
import sdk

if not 'DYNAMO_HOME' in os.environ:
    print ("You must define DYNAMO_HOME. Have you run './script/build.py shell' ?", file=sys.stderr)
    sys.exit(1)

def import_lib(module_name, path):
    import importlib
    # Normally a finder would get you the loader and spec.
    loader = importlib.machinery.SourceFileLoader(module_name, path)
    spec = importlib.machinery.ModuleSpec(module_name, loader, origin=path)
    # Basically what import does when there is no loader.create_module().
    module = importlib.util.module_from_spec(spec)
    # Now is the time to put the module in sys.modules if you want.
    # How import initializes the module.
    loader.exec_module(module)

script_dir = os.path.dirname(__file__)
defold_root = os.path.abspath(os.path.join(script_dir, ".."))

# import the vendor specific build setup
path = os.path.join(script_dir, 'waf_dynamo_vendor.py')
if os.path.exists(path):
    sys.dont_write_bytecode = True
    import_lib('waf_dynamo_vendor', path)
    print("Imported %s from %s" % ('waf_dynamo_vendor', path))
    import waf_dynamo_vendor
    sys.dont_write_bytecode = False

if 'waf_dynamo_vendor' not in sys.modules:
    class waf_dynamo_vendor(object):
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
    globals()['waf_dynamo_vendor'] = waf_dynamo_vendor


def is_platform_private(platform):
    return platform in ['arm64-nx64', 'x86_64-ps4', 'x86_64-ps5']

def platform_supports_feature(platform, feature, data):
    if is_platform_private(platform):
        return waf_dynamo_vendor.supports_feature(platform, feature, data)
    if feature == 'vulkan' or feature == 'compute':
        return platform not in ['js-web', 'wasm-web', 'wasm_pthread-web', 'x86_64-ios']
    if feature == 'dx12':
        return platform in ['x86_64-win32']
    if feature == 'opengl_compute':
        return platform not in ['js-web', 'wasm-web', 'wasm_pthread-web', 'x86_64-ios', 'arm64-ios', 'arm64-macos', 'x86_64-macos']
    if feature == 'opengles':
        return platform in ['arm64-linux']
    if feature == 'webgpu':
        return platform in ['js-web', 'wasm-web', 'wasm_pthread-web']
    return waf_dynamo_vendor.supports_feature(platform, feature, data)

def platform_setup_tools(ctx, build_util):
    return waf_dynamo_vendor.setup_tools(ctx, build_util)

def platform_setup_vars(ctx, build_util):
    return waf_dynamo_vendor.setup_vars(ctx, build_util)

def transform_runnable_path(platform, path):
    return waf_dynamo_vendor.transform_runnable_path(platform, path)

def platform_glfw_version(platform):
    if platform in ['x86_64-macos', 'arm64-macos', 'x86_64-win32', 'win32', 'x86_64-linux', 'arm64-linux']:
        return 3
    return 2

def platform_get_glfw_lib(platform):
    if platform_glfw_version(platform) == 3:
        return 'glfw3'
    return 'dmglfw'

def platform_get_platform_lib(platform):
    if not (platform_supports_feature(platform, "opengl", None) or platform_supports_feature(platform, "vulkan", None)):
        return 'PLATFORM_NULL'

    if platform_glfw_version(platform) == 3:
        if Options.options.with_vulkan or platform in ('arm64-macos', 'x86_64-macos', 'arm64-nx64'):
            return 'PLATFORM_VULKAN'
    return 'PLATFORM'

def platform_graphics_libs_and_symbols(platform):
    graphics_libs = []
    graphics_lib_symbols = []

    use_opengl = False
    use_opengles = False
    use_vulkan = False

    if platform in ('arm64-macos', 'x86_64-macos', 'arm64-nx64'):
        use_opengl = Options.options.with_opengl
        use_vulkan = True
    elif platform in ('arm64-linux'):
        use_opengles = True
        use_vulkan = Options.options.with_vulkan
    else:
        use_opengl = True
        use_vulkan = Options.options.with_vulkan

    # We can only use one of these variants
    if use_opengles:
        graphics_libs += ['GRAPHICS_OPENGLES', 'DMGLFW', 'OPENGLES']
        graphics_lib_symbols += ['GraphicsAdapterOpenGLES']
    elif use_opengl:
        graphics_libs += ['GRAPHICS', 'DMGLFW', 'OPENGL']
        graphics_lib_symbols += ['GraphicsAdapterOpenGL']

    if use_vulkan:
        graphics_libs += ['GRAPHICS_VULKAN', 'DMGLFW', 'VULKAN']
        graphics_lib_symbols.append('GraphicsAdapterVulkan')

    if Options.options.with_dx12 and platform_supports_feature(platform, 'dx12', {}):
        graphics_libs += ['GRAPHICS_DX12', 'DX12']
        graphics_lib_symbols.append('GraphicsAdapterDX12')

    if Options.options.with_webgpu and platform_supports_feature(platform, 'webgpu', {}):
        graphics_libs += ['GRAPHICS_WEBGPU']
        graphics_lib_symbols.append('GraphicsAdapterWebGPU')

    if platform in ('arm64-nx64'):
        graphics_libs = ['GRAPHICS_VULKAN', 'DMGLFW', 'VULKAN']
        graphics_lib_symbols = ['GraphicsAdapterVulkan']

    if platform in ('x86_64-ps4'):
        graphics_libs = ['GRAPHICS']
        graphics_lib_symbols = ['GraphicsAdapterPS4']

    if platform in ('x86_64-ps5'):
        graphics_libs = ['GRAPHICS']
        graphics_lib_symbols = ['GraphicsAdapterPS5']

    return graphics_libs, graphics_lib_symbols

# Note that some of these version numbers are also present in build.py (TODO: put in a waf_versions.py or similar)
# The goal is to put the sdk versions in sdk.py
SDK_ROOT=sdk.SDK_ROOT

EMSCRIPTEN_ROOT=os.environ.get('EMSCRIPTEN', '')

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
#   files per file for later collation into .json and .sdoc files.
def apidoc_extract_task(bld, src):
    import re
    from collections import defaultdict
    all_docs = {}

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
        comment = {
            "is_document": False,
            "namespace": None,
            "path": None
        }
        for (tag, value) in lst:
            tag = tag.strip()
            value = value.strip()
            if tag == 'document':
                comment["is_document"] = True
            else:
                comment[tag] = value
        return comment

    def _parse_source(source_path):
        resource = bld.path.find_resource(source_path)
        if not resource:
            sys.exit("Couldn't find resource: %s" % resource)
            return

        elements = {}
        resource_path = resource.abspath()
        resource_file = os.path.basename(resource_path)
        relative_path = resource_path.replace(defold_root, "")[1:]

        with open(resource_path, encoding='utf8') as in_f:
            source = in_f.read()
            lst = re.findall('/(\*#.*?)\*/', source, re.DOTALL)
            default_namespace = None
            for comment_str in lst:
                comment = _parse_comment(comment_str)

                namespace = comment.get("namespace")
                if comment["is_document"]:
                    comment_path = comment.get("path")
                    if not comment_path:
                        print("Missing @path in '%s', adding '%s'" % (resource_path, relative_path))
                        comment_str = comment_str + ("* @path %s\n" % relative_path)
                    else:
                        # there really shouldn't be any files with hardcoded paths anymore
                        # but let's keep this here for some time in case we introduce a hardcoded
                        # path somewhere again
                        print("Replacing @path in '%s' with '%s'" % (resource_path, relative_path))
                        comment_str = comment_str.replace("@path " + comment_path, "@path " + relative_path)

                    comment_file = comment.get("file")
                    if not comment_file:
                        print("Missing @file in '%s', adding '%s'" % (resource_path, resource_file))
                        comment_str = comment_str + ("* @file %s\n" % resource_file)
                    elif comment_file != resource_file:
                        # there shouldn't be any of these, but let's keep it here anyway
                        print("Replacing @file in '%s' with '%s'" % (resource_path, resource_file))
                        comment_str = comment_str.replace("@file " + comment_file, "@file " + resource_file)

                    comment_language = comment.get("language")
                    if not comment_language:
                        print("Missing @language in %s, assuming C++" % (resource_path))
                        comment_str = comment_str + "* @language C++\n"

                    if namespace:
                        default_namespace = namespace

                if not namespace:
                    namespace = default_namespace
                    comment["namespace"] = default_namespace

                if namespace:
                    if namespace not in elements:
                        elements[namespace] = []
                    elements[namespace].append('/' + comment_str + '*/')
                else:
                    if resource_path not in elements:
                        elements[resource_path] = []
                    elements[resource_path].append('/' + comment_str + '*/')

        return elements

    def extract_docs(bld, src):
        docs = defaultdict(list)
        # Gather data
        for s in src:
            elements = _parse_source(s)
            for k,v in elements.items():
                # turn path into key which will later be used as the
                # build target filename
                key = "-".join(os.path.normpath(s).split(os.sep))
                key = key.replace("..-", "")
                docs[key] = docs[key] + v
        all_docs.update(docs)
        return docs

    def write_docs(task):
        for o in task.outputs:
            name = os.path.splitext(o.name)[0] # remove .apidoc
            docs = all_docs[name]
            with open(str(o.get_bld()), 'w+', encoding='utf-8') as out_f:
                out_f.write('\n'.join(docs))

    if not getattr(Options.options, 'skip_apidocs', False):
        docs = extract_docs(bld, src)
        target = []
        for key in docs.keys():
            target.append(key + '.apidoc')
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

def getAndroidCompilerName(target_arch, api_version):
    if target_arch == 'arm64':
        return 'aarch64-linux-android%s-clang' % (api_version)
    else:
        return 'armv7a-linux-androideabi%s-clang' % (api_version)

def getAndroidNDKAPIVersion(target_arch):
    if target_arch == 'arm64':
        return sdk.ANDROID_64_NDK_API_VERSION
    else:
        return sdk.ANDROID_NDK_API_VERSION

def getAndroidCompileFlags(target_arch):
    # NOTE compared to armv7-android:
    # -mthumb, -mfloat-abi, -mfpu are implicit on aarch64, removed from flags
    if 'arm64' == target_arch:
        return ['-D__aarch64__', '-DGOOGLE_PROTOBUF_NO_RTTI', '-march=armv8-a', '-fvisibility=hidden']
    # NOTE:
    # -fno-exceptions added
    else:
        return ['-D__ARM_ARCH_5__', '-D__ARM_ARCH_5T__', '-D__ARM_ARCH_5E__', '-D__ARM_ARCH_5TE__', '-DGOOGLE_PROTOBUF_NO_RTTI', '-march=armv7-a', '-mfloat-abi=softfp', '-fvisibility=hidden']

def getAndroidLinkFlags(target_arch):
    common_flags = ['-Wl,--no-undefined', '-Wl,-z,noexecstack', '-landroid', '-fpic', '-z', 'text']
    if target_arch == 'arm64':
        return common_flags + ['-Wl,-z,max-page-size=16384']
    else:
        return ['-Wl,--fix-cortex-a8'] + common_flags

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
    target_os = build_util.get_target_os()
    target_arch = build_util.get_target_architecture()

    opt_level = Options.options.opt_level
    if opt_level == "2" and TargetOS.WEB == target_os:
        opt_level = "3" # emscripten highest opt level
    elif opt_level == "0" and TargetOS.WINDOWS in target_os:
        opt_level = "d" # how to disable optimizations in windows

    # For nicer output (i.e. in CI logs), and still get some performance, let's default to -O1
    if (Options.options.with_asan or Options.options.with_ubsan or Options.options.with_tsan) and opt_level != '0':
        opt_level = 1

    FLAG_ST = '/%s' if TargetOS.WINDOWS == target_os else '-%s'

    # Common for all platforms
    flags = []
    if target_os not in (TargetOS.WINDOWS, TargetOS.XBONE):
        flags += ["-Werror=return-type"]

        # if we're building on CI, we want to setup special debug paths, as it makes it easier to use with extension builds
        if os.environ.get('GITHUB_WORKFLOW', None) is not None:
            build_script_path = self.path.abspath()
            parts = build_script_path.split(os.sep)
            index = next((i for i, part in enumerate(parts) if part == "engine"), -1)
            if index != -1 and (index + 1) < len(parts):
                flags += ["-fdebug-compilation-dir=engine/{}".format(parts[index + 1])]
            flags += ["-fdebug-prefix-map=../src=src", "-fdebug-prefix-map=../../../tmp/dynamo_home=../../defoldsdk"]

    if Options.options.ndebug:
        flags += [self.env.DEFINES_ST % 'NDEBUG']

    for f in ['CFLAGS', 'CXXFLAGS', 'LINKFLAGS']:
        self.env.append_value(f, [FLAG_ST % ('O%s' % opt_level)])

    if Options.options.show_includes:
        if TargetOS.WINDOWS == target_os:
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
        if '64' in target_arch:
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

    self.env.append_value('INCLUDES', build_util.get_dynamo_home('sdk','include'))
    self.env.append_value('INCLUDES', build_util.get_dynamo_home('include', build_util.get_target_platform()))
    self.env.append_value('INCLUDES', build_util.get_dynamo_home('include'))
    self.env.append_value('INCLUDES', build_util.get_dynamo_ext('include', build_util.get_target_platform()))
    self.env.append_value('INCLUDES', build_util.get_dynamo_ext('include'))
    self.env.append_value('LIBPATH', build_util.get_dynamo_ext('lib', build_util.get_target_platform()))

    # For 32-bit Windows, search both legacy 'win32' and tuple 'x86-win32' folders
    # TODO: Remove the redundant lib folders ("win32") once we've moved fully to CMake
    if build_util.get_target_platform() == 'win32':
        alias = 'x86-win32' # used by the new/CMake code path
        # Includes under DYNAMO_HOME and DYNAMO_HOME/ext
        self.env.append_value('INCLUDES', build_util.get_dynamo_home('include', alias))
        self.env.append_value('INCLUDES', build_util.get_dynamo_ext('include', alias))
        # Lib paths under DYNAMO_HOME and DYNAMO_HOME/ext
        self.env.append_value('LIBPATH', build_util.get_dynamo_home('lib', alias))
        self.env.append_value('LIBPATH', build_util.get_dynamo_ext('lib', alias))

    # Platform specific paths etc comes after the project specific stuff

    if target_os in (TargetOS.MACOS, TargetOS.IOS):
        self.env.append_value('LINKFLAGS', ['-weak_framework', 'Foundation'])
        if TargetOS.IOS == target_os:
            self.env.append_value('LINKFLAGS', ['-framework', 'UIKit', '-framework', 'SystemConfiguration', '-framework', 'AVFoundation'])
        else:
            self.env.append_value('LINKFLAGS', ['-framework', 'AppKit'])

    if TargetOS.LINUX == target_os:
        clang_arch = 'x86_64-unknown-linux-gnu'
        if build_util.get_target_platform() == 'arm64-linux':
            clang_arch = 'aarch64-unknown-linux-gnu'

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, [f'--target={clang_arch}', '-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-Werror=format', '-fno-exceptions','-fPIC', '-fvisibility=hidden'])

            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti'])

        self.env.append_value('LINKFLAGS', [f'--target={clang_arch}', '-fuse-ld=lld'])

    elif TargetOS.MACOS == target_os:
        sys_root = self.sdkinfo[build_util.get_target_platform()]['path']
        swift_dir = "%s/usr/lib/swift-%s/macosx" % (sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), sdk.SWIFT_VERSION)

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-Werror=format', '-fPIC', '-fvisibility=hidden'])
            self.env.append_value(f, ['-DDM_PLATFORM_MACOS'])

            self.env.append_value(f, ['-DGL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED', '-DGL_SILENCE_DEPRECATION'])
            self.env.append_value(f, '-mmacosx-version-min=%s' % sdk.VERSION_MACOSX_MIN)

            self.env.append_value(f, ['-isysroot', sys_root])
            self.env.append_value(f, ['-target', '%s-apple-darwin19' % target_arch])

            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti', '-stdlib=libc++', '-fno-exceptions', '-nostdinc++'])
                self.env.append_value(f, ['-isystem', '%s/usr/include/c++/v1' % sys_root])

        self.env.append_value('LINKFLAGS', ['-stdlib=libc++', '-isysroot', sys_root, '-mmacosx-version-min=%s' % sdk.VERSION_MACOSX_MIN, '-framework', 'Carbon','-flto'])
        self.env.append_value('LINKFLAGS', ['-target', '%s-apple-darwin19' % target_arch])
        self.env.append_value('LIBPATH', ['%s/usr/lib' % sys_root, '%s/usr/lib' % sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), '%s' % swift_dir])

        if 'linux' in self.env['BUILD_PLATFORM']:
            self.env.append_value('LINKFLAGS', ['-target', '%s-apple-darwin19' % target_arch])

    elif TargetOS.IOS == target_os and target_arch in ('armv7', 'arm64', 'x86_64'):
        extra_ccflags = []
        extra_linkflags = []
        if 'linux' in self.env['BUILD_PLATFORM']:
            target_triplet='arm-apple-darwin19'
            extra_ccflags += ['-target', target_triplet]
            extra_linkflags += ['-target', target_triplet, '-L%s' % os.path.join(sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']),'usr/lib/clang/%s/lib/darwin' % self.sdkinfo['xcode-clang']['version']),
                                '-lclang_rt.ios', '-Wl,-force_load', '-Wl,%s' % os.path.join(sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), 'usr/lib/arc/libarclite_iphoneos.a')]
        else:
            #  NOTE: -lobjc was replaced with -fobjc-link-runtime in order to make facebook work with iOS 5 (dictionary subscription with [])
            extra_linkflags += ['-fobjc-link-runtime']

        sys_root = self.sdkinfo[build_util.get_target_platform()]['path']
        swift_dir = "%s/usr/lib/swift-%s/iphoneos" % (sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), sdk.SWIFT_VERSION)
        if 'x86_64' == target_arch:
            swift_dir = "%s/usr/lib/swift-%s/iphonesimulator" % (sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), sdk.SWIFT_VERSION)

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, extra_ccflags + ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DGOOGLE_PROTOBUF_NO_RTTI', '-Wall', '-fvisibility=hidden',
                                            '-arch', target_arch, '-miphoneos-version-min=%s' % sdk.VERSION_IPHONEOS_MIN])
            self.env.append_value(f, ['-isysroot', sys_root])

            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-exceptions', '-fno-rtti', '-stdlib=libc++', '-nostdinc++'])
                self.env.append_value(f, ['-isystem', '%s/usr/include/c++/v1' % sys_root])

            self.env.append_value(f, ['-DDM_PLATFORM_IOS'])
            if 'x86_64' == target_arch:
                self.env.append_value(f, ['-DIOS_SIMULATOR'])

        self.env.append_value('LINKFLAGS', ['-arch', target_arch, '-stdlib=libc++', '-isysroot', sys_root, '-dead_strip', '-miphoneos-version-min=%s' % sdk.VERSION_IPHONEOS_MIN] + extra_linkflags)
        self.env.append_value('LIBPATH', ['%s/usr/lib' % sys_root, '%s/usr/lib' % sdk.get_toolchain_root(self.sdkinfo, self.env['PLATFORM']), '%s' % swift_dir])

    elif TargetOS.ANDROID == target_os:
        bp_arch, bp_os = self.env['BUILD_PLATFORM'].split('-')
        # NDK doesn't support arm64 yet
        if bp_arch == 'arm64':
            bp_arch = 'x86_64';
        if bp_os == 'macos':
            bp_os = 'darwin'
        sysroot='%s/toolchains/llvm/prebuilt/%s-%s/sysroot' % (self.sdkinfo['ndk'], bp_os, bp_arch)

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-gdwarf-2', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall',
                                      '-fpic', '-ffunction-sections', '-fstack-protector',
                                      '-fomit-frame-pointer', '-fno-strict-aliasing', '-fno-exceptions', '-funwind-tables',
                                      '-I%s/sources/android/native_app_glue' % (self.sdkinfo['ndk']),
                                      '-I%s/sources/android/cpufeatures' % (self.sdkinfo['ndk']),
                                      '-isysroot=%s' % sysroot,
                                      '-DANDROID', '-Wa,--noexecstack'] + getAndroidCompileFlags(target_arch))
            if f == 'CXXFLAGS':
                self.env.append_value(f, ['-fno-rtti'])

        # TODO: Should be part of shared libraries
        # -Wl,-soname,libnative-activity.so -shared
        # -lsupc++
        self.env.append_value('LINKFLAGS', [
                '-isysroot=%s' % sysroot,
                '-static-libstdc++',
                '-Wl,--build-id=uuid'] + getAndroidLinkFlags(target_arch))
    elif TargetOS.WEB == target_os:

        emflags_compile = ['DISABLE_EXCEPTION_CATCHING=1']

        emflags_compile = zip(['-s'] * len(emflags_compile), emflags_compile)
        emflags_compile =[j for i in emflags_compile for j in i]

        emflags_link = [
            'DISABLE_EXCEPTION_CATCHING=1',
            'ALLOW_UNIMPLEMENTED_SYSCALLS=0',
            'EXPORTED_RUNTIME_METHODS=["ccall","UTF8ToString","callMain","HEAPU8","stringToNewUTF8"]',
            'EXPORTED_FUNCTIONS=_main,_malloc,_free',
            'ERROR_ON_UNDEFINED_SYMBOLS=1',
            'INITIAL_MEMORY=33554432',
            'MAX_WEBGL_VERSION=2',
            'GL_SUPPORT_AUTOMATIC_ENABLE_EXTENSIONS=0',
            'IMPORTED_MEMORY=1',
            'STACK_SIZE=5MB']

        with_pthread = False
        if build_util.get_target_platform() == 'wasm_pthread-web':
            with_pthread = True

        if with_pthread:
            emflags_link += [
                'MIN_FIREFOX_VERSION=79',
                'MIN_SAFARI_VERSION=150000',
                'MIN_CHROME_VERSION=75']
        else:
            emflags_link += [
                'MIN_FIREFOX_VERSION=40',
                'MIN_SAFARI_VERSION=101000',
                'MIN_CHROME_VERSION=45']

        flags = []
        linkflags = []

        if Options.options.with_webgpu and platform_supports_feature(build_util.get_target_platform(), 'webgpu', {}):
            if 'wagyu' in Options.options.enable_features:
                # When building the executable locally, targeting wagyu, we need to link with the stubs
                wagyu_port = '%s/ext/wagyu-port/webgpu-port.py:wagyu=true' % (os.environ['DYNAMO_HOME'])
                linkflags += ['--use-port=%s' % wagyu_port]
            else:
                emflags_link += ['USE_WEBGPU', 'GL_WORKAROUND_SAFARI_GETCONTEXT_BUG=0']
            emflags_link += ['ASYNCIFY', 'WASM_BIGINT=1']
            if int(opt_level) >= 3:
                emflags_link += ['ASYNCIFY_ADVISE', 'ASYNCIFY_IGNORE_INDIRECT', 'ASYNCIFY_ADD=["main", "dmEngineCreate(int, char**)"]' ]

        if with_pthread:
            # sound needs this to startup its thread with no deadlock
            emflags_link += ['PTHREAD_POOL_SIZE=1']

        if 'wasm' == target_arch:
            emflags_link += ['WASM=1', 'ALLOW_MEMORY_GROWTH=1']
            if int(opt_level) < 2:
                flags += ['-gseparate-dwarf', '-gsource-map']
                linkflags += ['-gseparate-dwarf', '-gsource-map']
        else:
            emflags_link += ['WASM=0', 'LEGACY_VM_SUPPORT=1']

        emflags_link = zip(['-s'] * len(emflags_link), emflags_link)
        emflags_link =[j for i in emflags_link for j in i]

        if os.environ.get('EMCFLAGS', None) is not None:
            flags += os.environ.get("EMCFLAGS", "").split(' ')

        if os.environ.get('EMLINKFLAGS', None) is not None:
            linkflags += os.environ.get("EMLINKFLAGS", "").split(' ')

        flags += ['-O%s' % opt_level]
        linkflags += ['-O%s' % opt_level]

        if with_pthread:
            flags += ['-pthread']
            linkflags += ['-pthread']
        else:
            self.env.append_value('DEFINES', ['DM_NO_THREAD_SUPPORT'])

        self.env['DM_HOSTFS']           = '/node_vfs/'
        self.env.append_value('DEFINES', ['JC_TEST_NO_DEATH_TEST', 'PTHREADS_DEBUG'])
        # This disables a few tests in test_httpclient (no real investigation done)
        self.env.append_value('DEFINES', ['DM_TEST_DLIB_HTTPCLIENT_NO_HOST_SERVER'])

        for f in ['CFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-Wall', '-fPIC', '-fno-exceptions', '-fno-rtti', '-Wno-nontrivial-memcall',
                                      '-DGL_ES_VERSION_2_0', '-DGOOGLE_PROTOBUF_NO_RTTI', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-DDM_NO_SYSTEM_FUNCTION'])
            self.env.append_value(f, emflags_compile)
            self.env.append_value(f, flags)

        self.env.append_value('LINKFLAGS', ['-Wno-warn-absolute-paths', '--emit-symbol-map', '-lidbfs.js'])
        self.env.append_value('LINKFLAGS', emflags_link)
        self.env.append_value('LINKFLAGS', linkflags)

    elif build_util.get_target_platform() in ['win32', 'x86_64-win32']:
        for f in ['CFLAGS', 'CXXFLAGS']:
            # /Oy- = Disable frame pointer omission. Omitting frame pointers breaks crash report stack trace. /O2 implies /Oy.
            # 0x0600 = _WIN32_WINNT_VISTA
            self.env.append_value(f, ['/Oy-', '/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS',
                                        '/DWINVER=0x0600', '/D_WIN32_WINNT=0x0600', '/DNOMINMAX',
                                        '/D_CRT_SECURE_NO_WARNINGS', '/wd4996', '/wd4200', '/DUNICODE', '/D_UNICODE'])

        self.env.append_value('LINKFLAGS', '/DEBUG')
        self.env.append_value('LINKFLAGS', ['shell32.lib', 'WS2_32.LIB', 'Iphlpapi.LIB', 'AdvAPI32.Lib', 'Gdi32.lib'])
        self.env.append_unique('ARFLAGS', '/WX')

        # Make sure we prefix with lib*.lib on windows, since this is not done
        # by waf anymore and several extensions rely on them being named that way
        self.env.STLIB_ST         = 'lib%s.lib'
        self.env.cstlib_PATTERN   = 'lib%s.lib'
        self.env.cxxstlib_PATTERN = 'lib%s.lib'

    platform_setup_vars(self, build_util)

    hostfs = ''
    if 'DM_HOSTFS' in self.env:
        hostfs = self.env['DM_HOSTFS']
    self.env.append_unique('DEFINES', 'DM_HOSTFS=\"%s\"' % hostfs)

    if 'IWYU' in self.env: # enabled during configure step
        wrapper = build_util.get_dynamo_home('..', '..', 'scripts', 'iwyu-clang.sh')
        for f in ['CC', 'CXX']:
            self.env[f] = [wrapper, self.env[f][0]]


# Used if you wish to be specific about certain default flags for a library (e.g. used for mbedtls library)
@feature('remove_flags')
@before('process_source')
@after('c')
@after('cxx')
def remove_flags_fn(self):
    lookup = getattr(self, 'remove_flags', {})
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
        if use_crash and name in 'LINKFLAGS':
            for i, v in enumerate(arr):
                if v.startswith('EXPORTED_FUNCTIONS'):
                    arr[i] = v + ",_JSWriteDump,_dmExportedSymbols"
                    break

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
    if Options.options.with_asan:
        if build_util.get_target_os() in ('linux','macos','ios','android','ps4','ps5'):
            self.env.append_value('CXXFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope', '-DDM_SANITIZE_ADDRESS'])
            self.env.append_value('CFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope', '-DDM_SANITIZE_ADDRESS'])
            self.env.append_value('LINKFLAGS', ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope'])
        elif build_util.get_target_os() in ('win',):
            self.env.append_value('CXXFLAGS', ['/fsanitize=address', '-D_DISABLE_VECTOR_ANNOTATION', '-DDM_SANITIZE_ADDRESS'])
            self.env.append_value('CFLAGS', ['/fsanitize=address', '-D_DISABLE_VECTOR_ANNOTATION', '-DDM_SANITIZE_ADDRESS'])
            # not a linker option
    elif Options.options.with_ubsan and build_util.get_target_os() in ('linux','macos','ios','android','ps4','ps5','nx64'):
        self.env.append_value('CXXFLAGS', ['-fsanitize=undefined', '-DDM_SANITIZE_UNDEFINED'])
        self.env.append_value('CFLAGS', ['-fsanitize=undefined', '-DDM_SANITIZE_UNDEFINED'])
        self.env.append_value('LINKFLAGS', ['-fsanitize=undefined'])
    elif Options.options.with_tsan and build_util.get_target_os() in ('linux','macos','ios','android','ps4','ps5'):
        self.env.append_value('CXXFLAGS', ['-fsanitize=thread', '-DDM_SANITIZE_THREAD'])
        self.env.append_value('CFLAGS', ['-fsanitize=thread', '-DDM_SANITIZE_THREAD'])
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
    if platform not in ['x86_64-linux','arm64-linux','x86_64-macos','arm64-macos','arm64-ios','armv7-android','arm64-android']:
        return 0 # return ok, path is still unstripped

    sdkinfo = sdk.get_sdk_info(SDK_ROOT, bld.env.PLATFORM)
    strip = "strip"
    if 'android' in platform:
        host_names = {
            'win32': 'windows',
            'darwin': 'darwin',
            'linux': 'linux',
        }
        home_names = {
            'win32': 'USERPROFILE',
            'darwin': 'HOME',
            'linux': 'HOME',
        }
        HOME = os.environ[home_names.get(sys.platform)]
        ANDROID_HOST = host_names.get(sys.platform)
        strip = "%s/toolchains/llvm/prebuilt/%s-x86_64/bin/llvm-strip" % (sdkinfo['ndk'], ANDROID_HOST)

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

    sdk_info = sdk.get_sdk_info(SDK_ROOT, build_util.get_target_platform())
    android_jar = sdk_info['jar'] # android.jar from the sdk

    dynamo_home = task.env['DYNAMO_HOME']

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
        proguardjar = '%s/android-sdk/tools/proguard/lib/proguard.jar' % sdkinfo['path']
        dex_input = ['%s/share/java/classes.jar' % dynamo_home]

        ret = bld.exec_command('%s -jar %s -include %s -libraryjars %s -injars %s -outjar %s' % (task.env['JAVA'][0], proguardjar, proguardtxt, android_jar, ':'.join(dx_jars), dex_input[0]))
        if ret != 0:
            error('Error running proguard')
            return 1
    else:
        dex_input = dx_jars

    if dex_input:
        ret = bld.exec_command('%s --output %s %s' % (task.env.D8[0], dex_dir, ' '.join(dex_input)))
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

Task.task_factory('dex', '${D8} --dex --output ${TGT} ${SRC}',
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

def create_test_server_config(ctx, port=None, ip=None, config_name=None):
    local_ip = ip
    if local_ip is None:
        hostname = socket.gethostname()
        local_ip = socket.gethostbyname(hostname)

    config = configparser.RawConfigParser()
    config.add_section("server")
    config.set("server", "ip", local_ip)
    config.set("server", "socket", port)

    if config_name is None:
        config_name = tempfile.mktemp(".cfg", "unittest_")
    configfilepath = os.path.basename(config_name)
    with open(configfilepath, 'w') as f:
        config.write(f)
        print("Wrote test config file: %s" % configfilepath)
        return configfilepath
    return None

def _should_run_test_taskgen(ctx, taskgen):
    if not 'test' in taskgen.features:
        return False

    if not taskgen.name.startswith('test_'):
        return False

    if getattr(taskgen, 'skip_test', False):
        return False

    supported_features = ['cprogram', 'cxxprogram', 'jar']
    found_feature = False
    for feature in supported_features:
        if feature in taskgen.features:
            found_feature = True
            break
    if not found_feature:
        return False

    if ctx.targets:
        if not taskgen.name in ctx.targets:
            return False

    if not platform_supports_feature(ctx.env.PLATFORM, taskgen.name, True):
        print("Skipping %s test for platform %s" % (ctx.env.PLATFORM, taskgen.name))
        return False

    return True


def run_tests(ctx, valgrind = False, configfile = None):
    if ctx == None or ctx.env == None or getattr(Options.options, 'skip_tests', False):
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
        if not _should_run_test_taskgen(ctx, t):
            continue

        if not t.tasks:
            print("No runnable task found in generator %s" % t.name)
            continue

        task = None
        task_type = None
        for _task in t.tasks:
            for attr in ['link_task', 'jar_task']:
                if _task == getattr(t, attr, None):
                    task = _task
                    task_type = attr
                    break

        # Create the environment for the task
        env = dict(os.environ)
        merged_table = t.env.get_merged_dict()
        keys=list(merged_table.keys())
        for key in keys:
            v = merged_table[key]
            if isinstance(v, str):
                env[key] = v

        launch_pattern = '%s %s'
        if task_type == 'jar_task':
            # java -cp <classpath> <main-class>
            mainclass = getattr(t, 'mainclass', '')
            classpath = Utils.to_list(getattr(t, 'classpath', []))
            java_library_paths = Utils.to_list(getattr(t, 'java_library_paths', []))
            jar_path = task.outputs[0].abspath()
            jar_dir = os.path.dirname(jar_path)
            java_library_paths.append(jar_dir)
            classpath.append(jar_path)
            debug_flags = ''
            #debug_flags = '-Xcheck:jni'
            #debug_flags = '-Xcheck:jni -Xlog:library=info -verbose:class'
            launch_pattern = f'java {debug_flags} -Djava.library.path={os.pathsep.join(java_library_paths)} -Djni.library.path={os.pathsep.join(java_library_paths)} -cp {os.pathsep.join(classpath)} {mainclass} -verbose:class'
            print("launch_pattern:", launch_pattern)

        if 'TEST_LAUNCH_PATTERN' in t.env:
            launch_pattern = t.env.TEST_LAUNCH_PATTERN

        if task is None:
            print("Skipping", t.name)
            continue

        program = transform_runnable_path(ctx.env.PLATFORM, task.outputs[0].abspath())

        if task_type == 'jar_task':
            cmd = launch_pattern
        else:
            cmd = launch_pattern % (program, configfile if configfile else '')

            if 'web' in ctx.env.PLATFORM: # should be moved to TEST_LAUNCH_ARGS
                cmd = '%s %s' % (ctx.env['NODEJS'][0], cmd)

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
        self.env.append_value('LINKFLAGS', ['--pre-js', pre_js, '-lnodefs.js'])

@task_gen
@after('apply_obj_vars')
@feature('test')
def test_no_install(self):
    if 'test_install' in self.features:
        return # the user wanted it installed
    self.install_path = None # the tests shouldn't normally be installed

@task_gen
@before('process_source')
@feature('test')
def test_flags(self):
    # When building tests for the web, we disable emission of emscripten js.mem init files,
    # as the assumption when these are loaded is that the cwd will contain these items.
    if 'web' in self.env['PLATFORM']:
        for f in ['CFLAGS', 'CXXFLAGS', 'LINKFLAGS']:
            if '-gsource-map' in self.env[f]:
                self.env[f].remove('-gsource-map')

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
@after('cprogram', 'cxxprogram', 'cshlib', 'cxxshlib')
@after('apply_link')
def extract_symbols(self):
    platform = self.env['PLATFORM']
    if not ('macos' in platform or 'ios' in platform):
        return

    link_output = self.link_task.outputs[0]
    dsym = link_output.change_ext('.dSYM')
    dsymtask = self.create_task('dSYM')
    dsymtask.set_inputs(link_output)
    dsymtask.set_outputs(dsym)

    archive = link_output.change_ext('.dSYM.zip')
    ziptask = self.create_task('DSYMZIP')
    ziptask.always_run = True
    ziptask.set_inputs(dsymtask.outputs[0])
    ziptask.set_outputs(archive)

def remove_flag_at_index(arr, index, count):
    for i in range(count):
        del arr[index]

def remove_flag(arr, flag, nargs):
    while flag in arr:
        index = arr.index(flag)
        remove_flag_at_index(arr, index, nargs+1)

def detect(conf):
    if Options.options.with_valgrind:
        conf.find_program('valgrind', var='VALGRIND', mandatory = False)

    if not Options.options.disable_ccache:
        conf.find_program('ccache', var='CCACHE', mandatory = False)

    if Options.options.with_iwyu:
        conf.find_program('include-what-you-use', var='IWYU', mandatory = False)

    host_platform = sdk.get_host_platform()

    platform = getattr(Options.options, 'platform', None)

    if not platform:
        platform = host_platform

    conf.env['PLATFORM'] = platform
    conf.env['BUILD_PLATFORM'] = host_platform

    try:
        build_util = create_build_utility(conf.env)
    except BuildUtilityException as ex:
        conf.fatal(ex.msg)

    if platform in ('x86_64-linux', 'arm64-linux', 'x86_64-win32', 'x86_64-macos', 'arm64-macos'):
        conf.env['IS_TARGET_DESKTOP'] = 'true'

    if host_platform in ('x86_64-linux', 'arm64-linux', 'x86_64-win32', 'x86_64-macos', 'arm64-macos'):
        conf.env['IS_HOST_DESKTOP'] = 'true'

    bindirs = [build_util.get_dynamo_ext('bin', host_platform)]
    conf.find_program('glslang', var='GLSLANG', mandatory = True, path_list = bindirs)

    target_os = build_util.get_target_os()
    conf.env['TARGET_OS'] = target_os

    dynamo_home = build_util.get_dynamo_home()
    conf.env['DYNAMO_HOME'] = dynamo_home

    # these may be the same if we're building the host tools
    sdkinfo = sdk.get_sdk_info(SDK_ROOT, build_util.get_target_platform())
    sdkinfo_host = sdk.get_sdk_info(SDK_ROOT, host_platform)

    if 'linux' in host_platform and target_os in (TargetOS.MACOS, TargetOS.IOS):
        conf.env['TESTS_UNSUPPORTED'] = True
        print ("Tests disabled (%s cannot run on %s)" % (build_util.get_target_platform(), host_platform))

        conf.env['CODESIGN_UNSUPPORTED'] = True
        print ("Codesign disabled", Options.options.skip_codesign)

    # Vulkan support
    if Options.options.with_vulkan and build_util.get_target_platform() in ('arm64-linux', 'x86_64-ios', 'js-web', 'wasm-web', 'wasm_pthread-web'):
        conf.fatal('Vulkan is unsupported on %s' % build_util.get_target_platform())

    if target_os == TargetOS.WINDOWS:
        includes = sdkinfo['includes']['path']
        libdirs = sdkinfo['lib_paths']['path']
        bindirs = sdkinfo['bin_paths']['path']

        if platform == 'x86_64-win32':
            conf.env['MSVC_INSTALLED_VERSIONS'] = [('msvc 14.0',[('x64', ('amd64', (bindirs, includes, libdirs)))])]
        else:
            conf.env['MSVC_INSTALLED_VERSIONS'] = [('msvc 14.0',[('x86', ('x86', (bindirs, includes, libdirs)))])]

        if not Options.options.skip_codesign:
            conf.find_program('signtool', var='SIGNTOOL', mandatory = True, path_list = bindirs)

    if target_os in (TargetOS.MACOS, TargetOS.IOS):
        path_list = None
        if 'linux' in host_platform:
            path_list=[os.path.join(sdk.get_toolchain_root(sdkinfo_host, host_platform),'bin')]
        else:
            path_list=[os.path.join(sdk.get_toolchain_root(sdkinfo, build_util.get_target_platform()),'usr','bin')]
        conf.find_program('dsymutil', var='DSYMUTIL', mandatory = True, path_list=path_list) # or possibly llvm-dsymutil
        conf.find_program('zip', var='ZIP', mandatory = True)

    if TargetOS.MACOS == target_os:
        # Force gcc without llvm on darwin.
        # We got strange bugs with http cache with gcc-llvm...
        os.environ['CC'] = 'clang'
        os.environ['CXX'] = 'clang++'

        llvm_prefix = ''
        bin_dir = '%s/usr/bin' % (sdk.get_toolchain_root(sdkinfo, build_util.get_target_platform()))
        if 'linux' in host_platform:
            llvm_prefix = 'llvm-'
            bin_dir = os.path.join(sdk.get_toolchain_root(sdkinfo_host, host_platform),'bin')

        conf.env['CC']      = '%s/clang' % bin_dir
        conf.env['CXX']     = '%s/clang++' % bin_dir
        conf.env['LINK_CC'] = '%s/clang' % bin_dir
        conf.env['LINK_CXX']= '%s/clang++' % bin_dir
        conf.env['CPP']     = '%s/clang -E' % bin_dir
        conf.env['AR']      = '%s/%sar' % (bin_dir, llvm_prefix)
        conf.env['RANLIB']  = '%s/%sranlib' % (bin_dir, llvm_prefix)

    elif TargetOS.IOS == target_os and build_util.get_target_architecture() in ('armv7','arm64','x86_64'):

        # NOTE: If we are to use clang for OSX-builds the wrapper script must be qualifed, e.g. clang-ios.sh or similar
        if 'linux' in host_platform:
            bin_dir=os.path.join(sdk.get_toolchain_root(sdkinfo_host, host_platform),'bin')

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
            conf.env['LD']      = '%s/lld' % bin_dir

        conf.env['GCC-OBJCXX'] = '-xobjective-c++'
        conf.env['GCC-OBJCLINK'] = '-lobjc'


    elif TargetOS.ANDROID == target_os and build_util.get_target_architecture() in ('armv7', 'arm64'):
        # TODO: No windows support yet (unknown path to compiler when wrote this)
        bp_arch, bp_os = host_platform.split('-')
        exe_suffix = ''
        cmd_suffix = ''
        if bp_os == 'macos':
            bp_os = 'darwin'
        elif bp_os == 'win32':
            bp_os = 'windows'
            cmd_suffix = '.cmd'
            exe_suffix = '.exe'
        target_arch = build_util.get_target_architecture()
        api_version = sdkinfo['api']
        clang_name  = sdkinfo['clangname']
        bintools    = sdkinfo['bintools']
        bintools    = os.path.normpath(bintools)
        sep         = os.path.sep

        if not os.path.exists(bintools):
            conf.fatal("Path does not exist: %s" % bintools)

        conf.env['ANDROID_JAR'] = sdkinfo['jar']

        conf.env['CC']       = f'{bintools}{sep}{clang_name}{cmd_suffix}'
        conf.env['CXX']      = f'{bintools}{sep}{clang_name}++{cmd_suffix}'
        conf.env['LINK_CXX'] = f'{bintools}{sep}{clang_name}++{cmd_suffix}'
        conf.env['CPP']      = f'{bintools}{sep}{clang_name}{cmd_suffix} -E'

        conf.env['AR']       = f'{bintools}{sep}llvm-ar{exe_suffix}'
        conf.env['RANLIB']   = f'{bintools}{sep}llvm-ranlib{exe_suffix}'
        conf.env['LD']       = f'{bintools}{sep}lld{exe_suffix}'

        # Make sure the the compiler_c/compiler_cxx uses the correct path
        conf.environ['PATH'] = os.path.normpath(bintools) + os.pathsep + conf.environ['PATH']

        conf.find_program('d8', var='D8', mandatory = True, path_list=[sdkinfo['build_tools']])

    elif TargetOS.LINUX == target_os:
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
            # llvm-ar or ar are found when loading compiler_c/compiler_cxx

    platform_setup_tools(conf, build_util)

    # jg: this whole thing is a 'dirty hack' to be able to pick up our own SDKs
    if TargetOS.WINDOWS == target_os:
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
        if build_util.get_target_platform() == 'win32':
            libdirs.insert(1, build_util.get_dynamo_home('lib', 'x86-win32'))

        conf.env['PATH']     = bindirs + sys.path + conf.env['PATH']
        conf.env['INCLUDES'] = includes
        conf.env['LIBPATH']  = libdirs
        conf.load('msvc', funs='no_autodetect')

        if not Options.options.skip_codesign:
            conf.find_program('signtool', var='SIGNTOOL', mandatory = True, path_list = bindirs)
    else:
        conf.options.check_c_compiler = 'clang gcc'
        conf.options.check_cxx_compiler = 'clang++ g++'
        conf.load('compiler_c')
        conf.load('compiler_cxx')

    # Since we're using an old waf version, we remove unused arguments
    remove_flag(conf.env['shlib_CFLAGS'], '-compatibility_version', 1)
    remove_flag(conf.env['shlib_CFLAGS'], '-current_version', 1)
    remove_flag(conf.env['shlib_CXXFLAGS'], '-compatibility_version', 1)
    remove_flag(conf.env['shlib_CXXFLAGS'], '-current_version', 1)

    # Needed for api generation
    if conf.env.IS_HOST_DESKTOP:
        if not 'CLANG' in conf.env:
            conf.find_program('clang',   var='CLANG', mandatory = True)
            os.environ['CLANG'] = conf.env.CLANG[0]
        if not 'CLANGPP' in conf.env:
            conf.find_program('clang++', var='CLANGPP', mandatory = True)
            os.environ['CLANGPP'] = conf.env.CLANGPP[0]

    # NOTE: We override after check_tool. Otherwise waf gets confused and CXX_NAME etc are missing..
    if target_os == TargetOS.WEB:
        emsdk = sdkinfo['emsdk']['path']

        if emsdk is None:
            conf.fatal('EMSDK environment variable not found.')

        if not conf.env['NODEJS']:
            emsdk_node = sdkinfo['emsdk']['node']
            path_list = None
            if emsdk_node is not None and os.path.exists(emsdk_node):
                path_list = [os.path.dirname(emsdk_node)]
            conf.find_program('node', var='NODEJS', mandatory = False, path_list = path_list)

        bin_dir = os.path.join(emsdk, 'upstream', 'emscripten')

        conf.env['EMSCRIPTEN'] = bin_dir
        conf.env['EMSDK'] = emsdk # let's use the actual install dir if we wish to use other tools
        conf.env['EM_CACHE'] = sdkinfo['emsdk']['cache']
        conf.env['EM_CONFIG'] = sdkinfo['emsdk']['config']

        conf.env['CC'] = f'{bin_dir}/emcc'
        conf.env['CXX'] = f'{bin_dir}/em++'
        conf.env['LINK_CC'] = f'{bin_dir}/emcc'
        conf.env['LINK_CXX'] = f'{bin_dir}/em++'
        conf.env['CPP'] = f'{bin_dir}/em++'
        conf.env['AR'] = f'{bin_dir}/emar'
        conf.env['RANLIB'] = f'{bin_dir}/emranlib'
        conf.env['LD'] = f'{bin_dir}/emcc'
        conf.env['cprogram_PATTERN']='%s.js'
        conf.env['cxxprogram_PATTERN']='%s.js'

        # Unknown argument: -Bstatic, -Bdynamic
        conf.env['STLIB_MARKER']=''
        conf.env['SHLIB_MARKER']=''

    if platform in ('x86_64-linux','arm64-linux','armv7-android','arm64-android'): # Currently the only platform exhibiting the behavior
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

    if conf.env['CCACHE'] and TargetOS.WINDOWS != target_os:
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

    if target_os in (TargetOS.MACOS, TargetOS.IOS):
        conf.load('waf_objectivec')

        # Unknown argument: -Bstatic, -Bdynamic
        conf.env['STLIB_MARKER']=''
        conf.env['SHLIB_MARKER']=''

    if TargetOS.LINUX == target_os:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
        conf.env['LIB_DL'] = 'dl'
    elif target_os in (TargetOS.MACOS, TargetOS.IOS):
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif TargetOS.ANDROID == target_os:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif TargetOS.WINDOWS == target_os:
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32 Iphlpapi AdvAPI32'.split()
    else:
        conf.env['LIB_PLATFORM_SOCKET'] = ''

    use_vanilla = getattr(Options.options, 'use_vanilla_lua', False)
    if target_os == TargetOS.WEB or not platform_supports_feature(build_util.get_target_platform(), 'luajit', {}):
        use_vanilla = True

    if use_vanilla:
        conf.env['STLIB_LUA'] = 'lua'
    else:
        conf.env['STLIB_LUA'] = 'luajit-5.1'

    conf.env['STLIB_TESTMAIN'] = ['testmain'] # we'll use this for all internal tests/tools

    if target_os != TargetOS.MACOS:
        conf.env['STLIB_UNWIND'] = 'unwind'

    if TargetOS.MACOS == target_os:
        conf.env['FRAMEWORK_OPENGL'] = ['OpenGL']
    elif TargetOS.ANDROID == target_os:
        conf.env['LIB_OPENGL'] = ['EGL', 'GLESv1_CM', 'GLESv2']
    elif TargetOS.WINDOWS == target_os:
        conf.env['LINKFLAGS_OPENGL'] = ['opengl32.lib', 'glu32.lib']
    elif 'linux' == target_os:
        conf.env['LIB_OPENGL'] = ['GL', 'GLU']
        conf.env['LIB_OPENGLES'] = ['EGL', 'GLESv1_CM', 'GLESv2']

    if TargetOS.MACOS == target_os:
        conf.env['FRAMEWORK_OPENAL'] = ['OpenAL']
        conf.env['FRAMEWORK_SOUND'] = ['AVFoundation']
    elif TargetOS.IOS == target_os:
        conf.env['FRAMEWORK_OPENAL'] = ['OpenAL', 'AudioToolbox']
        conf.env['FRAMEWORK_SOUND'] = ['AVFoundation']
    elif TargetOS.ANDROID == target_os:
        conf.env['LIB_OPENAL'] = ['OpenSLES']
    elif TargetOS.LINUX == target_os:
        conf.env['LIB_OPENAL'] = ['openal']

    conf.env['STLIB_DLIB'] = ['dlib', 'image', 'mbedtls', 'zip']
    conf.env['STLIB_DDF'] = 'ddf'
    conf.env['STLIB_CRASH'] = 'crashext'
    conf.env['STLIB_CRASH_NULL'] = 'crashext_null'

    conf.env['STLIB_PROFILE'] = ['profile']
    conf.env['STLIB_PROFILE_NULL'] = ['profile_null']
    conf.env['STLIB_PROFILER_BASIC'] = ['profiler_basic']
    conf.env['STLIB_PROFILER_REMOTERY'] = ['profiler_remotery']
    conf.env['STLIB_PROFILER_NULL'] = ['profiler_null']

    conf.env['DEFINES_PROFILE_NULL'] = ['DM_PROFILE_NULL']
    conf.env['STLIB_PROFILE_NULL_NOASAN'] = ['profile_null_noasan']

    if ('record' not in Options.options.disable_features):
        conf.env['STLIB_RECORD'] = 'record_null'
    else:
        if platform in ('x86_64-linux', 'arm64-linux', 'x86_64-win32', 'x86_64-macos', 'arm64-macos'):
            conf.env['STLIB_RECORD'] = 'record'
            conf.env['LINKFLAGS_RECORD'] = ['vpx.lib']
        else:
            Logs.info("record disabled")
            conf.env['STLIB_RECORD'] = 'record_null'
    conf.env['STLIB_RECORD_NULL'] = 'record_null'

    conf.env['STLIB_GRAPHICS']          = ['graphics', 'graphics_transcoder_basisu', 'basis_transcoder']
    conf.env['STLIB_GRAPHICS_OPENGLES'] = ['graphics_opengles', 'graphics_transcoder_basisu', 'basis_transcoder']
    conf.env['STLIB_GRAPHICS_VULKAN']   = ['graphics_vulkan', 'graphics_transcoder_basisu', 'basis_transcoder']
    conf.env['STLIB_GRAPHICS_DX12']     = ['graphics_dx12', 'graphics_transcoder_basisu', 'basis_transcoder']
    if 'wagyu' in Options.options.enable_features:
        conf.env['STLIB_GRAPHICS_WEBGPU']   = ['graphics_webgpu_wagyu', 'graphics_transcoder_basisu', 'basis_transcoder']
    else:
        conf.env['STLIB_GRAPHICS_WEBGPU']   = ['graphics_webgpu', 'graphics_transcoder_basisu', 'basis_transcoder']
    conf.env['STLIB_GRAPHICS_NULL']     = ['graphics_null', 'graphics_transcoder_null']

    conf.env['STLIB_FONT']            = ['font']
    conf.env['STLIB_FONT_LAYOUT']     = ['font_skribidi', 'harfbuzz', 'sheenbidi', 'unibreak', 'skribidi']

    if platform_glfw_version(platform) == 3:
        conf.env['STLIB_DMGLFW'] = 'glfw3'
    else:
        conf.env['STLIB_DMGLFW'] = 'dmglfw'

    # ***********************************************************
    # Vulkan
    if TargetOS.MACOS == target_os:
        conf.env['STLIB_VULKAN'] = Options.options.with_vulkan_validation and 'vulkan' or 'MoltenVK'
        conf.env['FRAMEWORK_VULKAN'] = ['Metal', 'IOSurface', 'QuartzCore']
        conf.env['FRAMEWORK_DMGLFW'] = ['QuartzCore']
    elif TargetOS.IOS == target_os:
        conf.env['STLIB_VULKAN'] = 'MoltenVK'
        conf.env['FRAMEWORK_VULKAN'] = ['Metal', 'IOSurface']
        conf.env['FRAMEWORK_DMGLFW'] = ['QuartzCore', 'OpenGLES', 'CoreVideo', 'CoreGraphics']
    elif TargetOS.LINUX == target_os:
        conf.env['LIB_VULKAN'] = ['vulkan', 'X11-xcb']
        # currently we only have the validation
        if Options.options.with_vulkan_validation and platform == 'arm64-linux':
            conf.env.append_value('LIB_VULKAN', ['VkLayer_khronos_validation'])

    elif TargetOS.ANDROID == target_os:
        conf.env['SHLIB_VULKAN'] = ['vulkan']
    elif TargetOS.WINDOWS == target_os:
        conf.env['LINKFLAGS_VULKAN'] = 'vulkan-1.lib' # because it doesn't have the "lib" prefix

    if TargetOS.MACOS == target_os:
        conf.env['FRAMEWORK_TESTAPP'] = ['AppKit', 'Cocoa', 'IOKit', 'Carbon', 'CoreVideo']
        conf.env['FRAMEWORK_APP'] = ['AppKit', 'Cocoa', 'IOKit', 'Carbon', 'CoreVideo']
    elif TargetOS.ANDROID == target_os:
        pass
        #conf.env['STLIB_TESTAPP'] += ['android']
    elif TargetOS.LINUX == target_os:
        conf.env['LIB_TESTAPP'] += ['Xext', 'X11', 'Xi', 'pthread']
        conf.env['LIB_APP'] += ['Xext', 'X11', 'Xi', 'pthread']
    elif TargetOS.WINDOWS == target_os:
        conf.env['LINKFLAGS_TESTAPP'] = ['user32.lib', 'shell32.lib']

    if TargetOS.WINDOWS == target_os:
        conf.env['LINKFLAGS_SOUND']     = ['ole32.lib'] # cocreateinstance in device_wasapi.cpp
        conf.env['LINKFLAGS_DINPUT']    = ['dinput8.lib', 'dxguid.lib', 'xinput9_1_0.lib']
        conf.env['LINKFLAGS_APP']       = ['user32.lib', 'shell32.lib', 'dbghelp.lib'] + conf.env['LINKFLAGS_DINPUT']
        conf.env['LINKFLAGS_DX12']      = ['D3D12.lib', 'DXGI.lib', 'D3Dcompiler.lib']


    if conf.env.PLATFORM in ['win32', 'x86_64-win32']:
        conf.env['LINKFLAGS_HID']               = ['hid.lib']
        conf.env['LINKFLAGS_HID_NULL']          = ['hid_null.lib']
        conf.env['LINKFLAGS_INPUT']             = ['input.lib']
        conf.env['LINKFLAGS_PLATFORM']          = ['platform.lib']
        conf.env['LINKFLAGS_PLATFORM_VULKAN']   = ['platform_vulkan.lib']
        conf.env['LINKFLAGS_PLATFORM_NULL']     = ['platform_null.lib']
    else:
        conf.env['STLIB_HID']               = ['hid']
        conf.env['STLIB_HID_NULL']          = ['hid_null']
        conf.env['STLIB_INPUT']             = ['input']
        conf.env['STLIB_PLATFORM']          = ['platform']
        conf.env['STLIB_PLATFORM_VULKAN']   = ['platform_vulkan']
        conf.env['STLIB_PLATFORM_NULL']     = ['platform_null']

    conf.env['STLIB_EXTENSION'] = 'extension'
    conf.env['STLIB_SCRIPT'] = 'script'

    if conf.env.IS_TARGET_DESKTOP:

        conf.env['STLIB_JNI'] = 'jni'
        conf.env['STLIB_JNI_NOASAN'] = 'jni_noasan'

        if 'JAVA_HOME' in os.environ:
            host = 'win32'
            if 'linux' in sys.platform:
                host = 'linux'
            elif 'darwin' in sys.platform:
                host = 'darwin'

            conf.env['INCLUDES_JDK'] = [os.path.join(os.environ['JAVA_HOME'], 'include'), os.path.join(os.environ['JAVA_HOME'], 'include', host)]
            conf.env['LIBPATH_JDK'] = os.path.join(os.environ['JAVA_HOME'], 'lib')
            conf.env['DEFINES_JDK'] = ['DM_HAS_JDK']

            # if the jdk doesn't have the jni.h
            jni_path = os.path.join(conf.env['INCLUDES_JDK'][0], 'jni.h')
            if not os.path.exists(jni_path):
                Logs.error("JAVA_HOME=%s" % os.environ['JAVA_HOME'])
                Logs.error("Failed to find jni.h at %s" % jni_path)
                sys.exit(1)

    conf.load('waf_csharp')

    if Options.options.generate_compile_commands:
        conf.load('clang_compilation_database')


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
    opt.add_option('--generate-compile-commands', action="store_true", default=False, dest='generate_compile_commands', help='generate (appending mode) compile_commands.json')
    opt.add_option('--use-vanilla-lua', action="store_true", default=False, dest='use_vanilla_lua', help='use luajit')
    opt.add_option('--opt-level', default="2", dest='opt_level', help='optimization level')
    opt.add_option('--ndebug', action='store_true', default=False, help='Defines NDEBUG for the engine')
    opt.add_option('--with-asan', action='store_true', default=False, dest='with_asan', help='Enables address sanitizer')
    opt.add_option('--with-ubsan', action='store_true', default=False, dest='with_ubsan', help='Enables undefined behavior sanitizer')
    opt.add_option('--with-tsan', action='store_true', default=False, dest='with_tsan', help='Enables thread sanitizer')
    opt.add_option('--with-iwyu', action='store_true', default=False, dest='with_iwyu', help='Enables include-what-you-use tool (if installed)')
    opt.add_option('--show-includes', action='store_true', default=False, dest='show_includes', help='Outputs the tree of includes')
    opt.add_option('--static-analyze', action='store_true', default=False, dest='static_analyze', help='Enables static code analyzer')
    opt.add_option('--with-valgrind', action='store_true', default=False, dest='with_valgrind', help='Enables usage of valgrind')
    opt.add_option('--with-openal', action='store_true', default=False, dest='with_openal', help='Enables OpenAL as the sound backend')
    opt.add_option('--with-opengl', action='store_true', default=False, dest='with_opengl', help='Enables OpenGL as the graphics backend')
    opt.add_option('--with-vulkan', action='store_true', default=False, dest='with_vulkan', help='Enables Vulkan as graphics backend')
    opt.add_option('--with-vulkan-validation', action='store_true', default=False, dest='with_vulkan_validation', help='Enables Vulkan validation layers (on osx and ios)')
    opt.add_option('--with-dx12', action='store_true', default=False, dest='with_dx12', help='Enables DX12 as a graphics backend')
    opt.add_option('--with-opus', action='store_true', default=False, dest='with_opus', help='Enable Opus audio codec support in runtime')
    opt.add_option('--with-webgpu', action='store_true', default=False, dest='with_webgpu', help='Enables WebGPU as graphics backend')

    # Currently supported features: physics
    opt.add_option('--disable-feature', action='append', default=[], dest='disable_features', help='disable feature, --disable-feature=foo')

    # Currently supported features: physics, simd (html5)
    opt.add_option('--enable-feature', action='append', default=[], dest='enable_features', help='enable feature, --disable-feature=foo')
