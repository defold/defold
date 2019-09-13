#!/usr/bin/env python

import sys
import subprocess
import platform
from argparse import ArgumentParser

def call(args):
    args.replace("--release", "")
    print(args)
    subprocess.check_call(args, shell=True)
    # subprocess.call(args, stdin=None, stdout=None, stderr=None, shell=True)


def aptget(package):
    call("sudo apt-get install --no-install-recommends " + package)


def choco(package):
    call("choco install " + package)


def install():
    system = platform.system()
    print("Installing dependencies for system '%s' " % (system))
    if system == "Linux":
        call("sudo apt-get update")
        call("sudo apt-get install -y software-properties-common")
        aptget("gcc-5")
        aptget("g++-5")
        aptget("libssl-dev")
        aptget("openssl")
        aptget("libtool")
        aptget("autoconf")
        aptget("automake")
        aptget("build-essential")
        aptget("uuid-dev")
        aptget("libxi-dev")
        aptget("libopenal-dev")
        aptget("libgl1-mesa-dev")
        aptget("libglw1-mesa-dev")
        aptget("freeglut3-dev")
        call("update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 10")
        call("update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 20")
        call("update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 10")
        call("update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 20")
        call("update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30")
        call("update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30")
        call("update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30")
        call("update-alternatives --set c++ /usr/bin/g++")
        aptget("clang-6.0")
        call("update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-6.0 1000")
        call("update-alternatives --install /usr/bin/clang clang /usr/bin/clang-6.0 1000")
        call("update-alternatives --config clang")
        call("update-alternatives --config clang++")
        aptget("tofrodos")
        aptget("cmake")
        aptget("curl")
        aptget("tree")
        aptget("silversearcher-ag")
        aptget("valgrind")
    elif system == "Windows":
        choco("visualstudio2017-workload-vctools")



def build_engine(platform, skip_tests = True, with_valgrind = False, with_asan = False, with_vanilla_lua = False, skip_codesign = True, skip_docs = True, skip_builtins = True, archive = False):
    args = 'python scripts/build.py distclean install_ext'.split()
    sub_args = []

    if platform == 'ios':
        args.append('--platform=armv7-darwin')
    elif platform == 'ios-64':
        args.append('--platform=arm64-darwin')
    elif platform == 'ios-simulator':
        args.append('--platform=x86_64-ios')
    elif platform == 'darwin-64':
        args.append('--platform=x86_64-darwin')
    elif platform == 'android':
        args.append('--platform=armv7-android')
    elif platform == 'android-64':
        args.append('--platform=arm64-android')
    elif platform == 'js-web':
        args.append('install_ems')
        args.append('--platform=js-web')
    elif platform == 'wasm-web':
        args.append('install_ems')
        args.append('--platform=wasm-web')
    elif platform == 'win32':
        args.append('--platform=win32')
    elif platform == 'win32-64':
        args.append('--platform=x86_64-win32')
    elif platform == 'linux-64':
        args.append('--platform=x86_64-linux')
    else:
        target_platform = ''

    args.append('build_engine')
    args.append('--no-colors')

    if archive:
        args.append('archive_engine')
    if skip_codesign:
        args.append('--skip-codesign')
    if skip_docs:
        args.append('--skip-docs')
    if skip_builtins:
        args.append('--skip-builtins')

    if skip_tests:
        args.append('--skip-tests')
        sub_args.append('--skip-build-tests')

    if with_valgrind:
        sub_args.append('--with-valgrind')

    if with_asan:
        sub_args.append('--with-asan')

    if with_vanilla_lua:
        sub_args.append('--use-vanilla-lua')

    cmd = ' '.join(args)

    # Add arguments to underlying processes after a double-dash
    if sub_args:
        cmd += ' -- ' + ' '.join(sub_args)

    call(cmd)


def build_editor(channel = None, release = False, engine_artifacts = None):
    args = 'python scripts/build.py distclean install_ext build_editor2 archive_editor2'.split()

    if release:
        args.append('release_editor2')

    # for install_ext
    args.append('--platform=x86_64-darwin')

    args.append('--no-colors')

    # Specifies from where to get the engine SHA1.
    # From build.py:
    #     'What engine version to bundle the Editor with (auto, dynamo-home, archived, archived-stable or a SHA1)'
    if engine_artifacts:
        args.append('--engine_artifacts=' + engine_artifacts)

    if channel:
        args.append('--channel=' + channel)

    cmd = ' '.join(args)
    call(cmd)


def build_bob(branch = None, channel = None, release = False):
    args = "python scripts/build.py distclean install_ext sync_archive build_bob archive_bob".split()

    if release:
        args.append("release")

    args.append("--")
    args.append("--no-colors")

    if branch:
        args.append("--branch=" + branch)

    if channel:
        args.append("--channel=" + channel)

    cmd = ' '.join(args)
    call(cmd)


def build_sdk():
    call('python scripts/build.py build_sdk --no-colors')


def smoke_test():
    call('python scripts/build.py distclean install_ext smoke_test --no-colors')


def main(argv):
    parser = ArgumentParser()
    parser.add_argument('commands', nargs="+", help="The command to execute")
    parser.add_argument("--platform", dest="platform", help="Platform to build for (when building the engine)")
    parser.add_argument("--branch", dest="branch", help="The branch to build for (when building engine and editor)")
    parser.add_argument("--with-asan", dest="with_asan", action='store_true', help="")
    parser.add_argument("--with-valgrind", dest="with_valgrind", action='store_true', help="")
    parser.add_argument("--with-vanilla-lua", dest="with_vanilla_lua", action='store_true', help="")
    args = parser.parse_args()

    platform = args.platform
    branch = args.branch.replace("refs/heads/", "")
    if branch == "master":
        channel = "stable"
        release = False
    elif branch == "beta":
        channel = "beta"
        release = False
    elif branch == "dev":
        channel = "alpha"
        release = True
    elif branch == "editor-dev":
        channel = "alpha"
        release = False
    else:
        channel = None
        release = False

    print("Platform: %s Branch: %s Channel: %s" % (platform, branch, channel))

    for command in args.commands:
        if command == "engine":
            if not platform:
                raise Exception("No --platform specified.")
            with_valgrind = args.with_valgrind or (branch in [ "master", "beta" ])
            build_engine(platform, with_valgrind = with_valgrind, with_asan = args.with_asan, with_vanilla_lua = args.with_vanilla_lua)
        elif command == "editor":
            if branch == "master" or branch == "beta" or branch == "dev":
                build_editor(channel = channel, release = True, engine_artifacts = "archived")
            elif branch == "editor-dev":
                build_editor(channel = "editor-alpha", release = True)
            else:
                build_editor(release = False, engine_artifacts = "archived")
        elif command == "bob":
            if branch == "master" or branch == "beta" or branch == "dev":
                build_bob(branch = branch, channel = channel, release = release)
            else:
                build_bob()
        elif command == "sdk":
            build_sdk()
        elif command == "smoke":
            smoke_test()
        elif command == "install":
            install()
        else:
            print("Unknown command {0}".format(command))


if __name__ == "__main__":
    main(sys.argv[1:])
