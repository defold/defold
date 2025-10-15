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
import run
import sdk

DOTNET_VERSION=9

def sortversions(versions):
    """Return a list of semantic version strings sorted in ascending order."""

    if not versions:
        return []

    def _version_key(value):
        parts = []
        for token in re.split(r"(\\d+)|([^.]+)", value):
            if not token or token == '.':
                continue
            # trick to make integer vs string comparisons, by making them a separate tuple
            # this means strings (e.g. "1-rc", will be sorted before actual integers)
            # this is ok as we're really only using the major version in this
            if token.isdigit():
                parts.append((1, int(token)))
            else:
                parts.append((0, token.lower()))
        return parts

    return sorted(versions, key=_version_key)


def _find_best_version_from_folder(base_folder, major_version):

    print("MAWE _find_best_version_from_folder:", base_folder)

    if not base_folder or not os.path.isdir(base_folder):
        print(f"MAWE not is dir {base_folder}")
        return None

    candidates = []
    for entry in os.listdir(base_folder):
        print("MAWE entry:", entry)
        entry_path = os.path.join(base_folder, entry)
        if not os.path.isdir(entry_path):
            continue
        candidates.append(entry)

    if not candidates:
        print(f"MAWE no candidates")
        return None

    print("MAWE ***********************************************")
    print("MAWE UNSORTED")
    for x in candidates:
        print("MAWE: ", x)
    print("MAWE ***********************************************")

    candidates = sortversions(candidates)
    candidates.reverse()

    print("MAWE ***********************************************")
    print("MAWE SORTED")
    for x in candidates:
        print("MAWE: ", x)
    print("MAWE ***********************************************")

    for version in candidates:
        tokens = version.split('.')
        print("MAWE KEY:", tokens)
        major = tokens[0]
        try:
            major = int(major)
        except:
            continue

        if major > major_version:
            continue

        return version

    return None


# For properties, see https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-publish
Task.task_factory('csproj_stlib', '${DOTNET} publish -c ${MODE} -o ${BUILD_DIR} -v q --artifacts-path ${ARTIFACTS_PATH} -r ${RUNTIME} -p:PublishAot=true -p:NativeLib=Static -p:PublishTrimmed=true -p:IlcDehydrate=false ${SRC[0].abspath()}',
                      color='BROWN',
                      after='cxxstlib')

@feature('cs_stlib')
@before('process_source')
def compile_csharp_lib(self):
    if not self.env['DOTNET']:
        print("Skipping %s, as C# is not supported on this platform", self.name)
        return

    project = self.path.find_resource(self.project)
    if not project:
        self.bld.fatal("Couldn't find project '%s'" % self.project)

    csnodes = project.parent.ant_glob('**/*.cs', quiet=False)
    if not csnodes:
        self.bld.fatal("No source files found in project '%s'" % self.project)


    libname = os.path.basename(self.project)
    if libname.startswith('lib'):
        libname = libname[3:]

    libname_no_suffix = os.path.splitext(libname)[0]

    platform = self.env.PLATFORM
    if platform in ['armv7-android']:
        Logs.info("Platform '%s' does not yet support C# building" % platform)
        return

    build_util = create_build_utility(self.env)
    if build_util.get_target_os() in ['win32']:
        prefix = ''
        suffix = '.lib'
    else:
        prefix = 'lib'
        suffix = '.a'

    def defold_platform_to_dotnet(target_os, target_arch):
        # https://learn.microsoft.com/en-us/dotnet/core/rid-catalog
        os_to_dotnet_os = {
            'macos': 'osx',
            'win32': 'win',
        }
        target_os = os_to_dotnet_os.get(target_os, target_os) # translate the name, or use the original one

        arch_to_dotnet_arch = {
            'x86_64': 'x64',
            'arm64': 'arm64',
            'armv7': 'arm',
        }
        target_arch = arch_to_dotnet_arch.get(target_arch, target_arch) # translate the name, or use the original one

        if target_os and target_arch:
            return f'{target_os}-{target_arch}'
        return None

    arch = defold_platform_to_dotnet(build_util.get_target_os(), build_util.get_target_architecture())

    tsk = self.create_task('csproj_stlib')
    tsk.inputs.append(project)
    tsk.inputs.extend(csnodes)

    # build/cs/osx-arm64/publish/libdlib_cs.a
    build_dir = self.path.get_bld().make_node(f'cs')
    target = build_dir.make_node(f'{prefix}{libname_no_suffix}{suffix}')
    tsk.outputs.append(target)

    tsk.env['RUNTIME'] = arch
    tsk.env['MODE'] = 'Release' # Debug
    tsk.env['BUILD_DIR'] = build_dir.abspath()
    tsk.env['ARTIFACTS_PATH'] = build_dir.abspath()

    lib_path = os.path.join(self.env["PREFIX"], 'lib', self.env['PLATFORM'])
    inst_to=getattr(self,'install_path', lib_path)
    if inst_to:
        install_task = self.add_install_files(install_to=inst_to, install_from=tsk.outputs)

def _get_dotnet_version():
    result = run.shell_command('dotnet --info')
    lines = result.splitlines()
    lines = [x.strip() for x in lines]

    i = lines.index('Host:')
    version = lines[i+1].strip().split()[1]

    for l in lines:
        if l.startswith('Base Path:'):
            sdk_dir = l.split('Base Path:')[1].strip()
            while sdk_dir.endswith('/'):
                sdk_dir = sdk_dir[:-1]
            sdk_dir = os.path.dirname(sdk_dir)
            break
    return version, sdk_dir

def _get_dotnet_aot_base_folder(nuget_path, dotnet_platform):
    return f"{nuget_path}/microsoft.netcore.app.runtime.nativeaot.{dotnet_platform}"

def _get_dotnet_aot_base_with_version(nuget_path, dotnet_platform, dotnet_version):
    return f"{nuget_path}/microsoft.netcore.app.runtime.nativeaot.{dotnet_platform}/{dotnet_version}/runtimes/{dotnet_platform}/native"

def _get_dotnet_nuget_path():
    result = run.shell_command("dotnet nuget locals global-packages -l")
    if result is not None and 'global-packages:' in result:
        nuget_path = result.split('global-packages:')[1].strip()

    if not nuget_path:
        if build_util.get_target_os() in ('win32'):
            nuget_path = os.path.expanduser('%%userprofile%%/.nuget/packages')
        else:
            nuget_path = os.path.expanduser('~/.nuget/packages')
    return nuget_path

def _skip_dotnet(self):
    conf.env.DOTNET_VERSION = None
    conf.env.DOTNET_AOT_VERSION = None
    conf.env.DOTNET_SDK = None
    print("*************************************************************")
    print("Dotnet configuration does not work. Skipping!")
    print("*************************************************************")


def configure(conf):
    platform = getattr(Options.options, 'platform', sdk.get_host_platform())
    if platform == '':
        platform = sdk.get_host_platform()

    if platform not in ['x86_64-macos', 'arm64-macos']:
        return # not currently supported

    path = conf.find_program('dotnet', var='DOTNET', mandatory = False)
    if not path:
        print("DotNet was not found. Building C# tests will be disabled. See README_SETUP.md for info on how to install DotNet.")
        return

    conf.env.DOTNET_VERSION, conf.env.DOTNET_SDK = _get_dotnet_version()

    build_util = create_build_utility(conf.env)

    dotnet_arch ='arm64'
    if 'x86_64' in platform:
        dotnet_arch = 'x64'

    platform_to_cs = {
        'macos': 'osx',
        'win32': 'win'
    }

    dotnet_os = platform_to_cs.get(build_util.get_target_os(), build_util.get_target_os())
    dotnet_platform = '%s-%s' % (dotnet_os, dotnet_arch)

    nuget_path = _get_dotnet_nuget_path()
    conf.env.NUGET_PACKAGES = nuget_path
    if not os.path.exists(conf.env.NUGET_PACKAGES):
        print(f"'NUGET_PACKAGES' not found. Using NUGET_PACKAGES={nuget_path}")
        os.makedirs(conf.env.NUGET_PACKAGES)
        if not os.path.exists(conf.env.NUGET_PACKAGES):
            conf.fatal("Couldn't find C# nuget packages: '%s'" % conf.env.NUGET_PACKAGES)

    print(f"Nuget path: {nuget_path}")
    for x in os.listdir(nuget_path):
        print("MAWE x", x, os.path.isdir(os.path.join(nuget_path, x)))

    print("Setting DOTNET_SDK", conf.env.DOTNET_SDK)
    print("Setting DOTNET_VERSION", conf.env.DOTNET_VERSION)

    aot_base = _get_dotnet_aot_base_with_version(nuget_path, dotnet_platform, conf.env.DOTNET_VERSION)
    # Note, we can't check it here, in case this is a clean install, and we haven't downloaded any packages with nuget yet

    if build_util.get_target_os() in ('win32'):
        pass
    else:
        # Since there are dynamic libraries with the same name, ld will choose them by default.
        # only way to override that behavior is to explicitly specify the paths to the static libraries
        if build_util.get_target_os() in ('macos'):
            conf.env['LINKFLAGS_CSHARP'] = [
                f'{aot_base}/libbootstrapperdll.o',
                f'{aot_base}/libRuntime.WorkstationGC.a',
                f'{aot_base}/libeventpipe-enabled.a',
                f'{aot_base}/libstandalonegc-enabled.a',
                f'{aot_base}/libstdc++compat.a',
                f'{aot_base}/libSystem.Native.a',
                f'{aot_base}/libSystem.IO.Compression.Native.a',
                f'{aot_base}/libSystem.Globalization.Native.a']

            if platform == 'x86_64-macos':
                conf.env.append_unique('LINKFLAGS_CSHARP', f'{aot_base}/libRuntime.VxsortDisabled.a')

        elif build_util.get_target_os() in ('ios'):
            conf.env['LINKFLAGS_CSHARP'] = [
                f'{aot_base}/libbootstrapperdll.o',
                f'{aot_base}/libRuntime.WorkstationGC.a',
                f'{aot_base}/libeventpipe-disabled.a',
                f'{aot_base}/libstandalonegc-disabled.a',
                f'{aot_base}/libstdc++compat.a',
                f'{aot_base}/libSystem.Native.a',
                f'{aot_base}/libSystem.Globalization.Native.a',
                f'{aot_base}/libSystem.IO.Compression.Native.a',
                f'{aot_base}/libSystem.Net.Security.Native.a',
                f'{aot_base}/libSystem.Security.Cryptography.Native.Apple.a',
                f'{aot_base}/licucore.a']

        if build_util.get_target_os() in ('macos'):
            conf.env['FRAMEWORK_CSHARP'] = ["OpenAL","OpenGL","QuartzCore"]
