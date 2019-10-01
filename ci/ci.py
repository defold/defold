#!/usr/bin/env python

import sys
import subprocess
import platform
import os
from argparse import ArgumentParser

def call(args):
    args.replace("--release", "")
    print(args)
    ret = os.system(args)
    if ret != 0:
        exit(1)
    # subprocess.check_call(args, shell=True)
    # subprocess.call(args, stdin=None, stdout=None, stderr=None, shell=True)


def aptget(package):
    call("sudo apt-get install --no-install-recommends " + package)


def choco(package):
    call("choco install " + package + " -y")


def mingwget(package):
    call("mingw-get install " + package)


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
        aptget("tofrodos")
        aptget("tree")
        aptget("silversearcher-ag")
        aptget("valgrind")



def build_engine(platform, with_valgrind = False, with_asan = False, with_vanilla_lua = False, skip_tests = False, skip_codesign = True, skip_docs = False, skip_builtins = False, archive = False):
    args = 'python scripts/build.py distclean install_ext'.split()
    sub_args = []

    args.append('--platform=' + platform)

    if platform == 'js-web' or platform == 'wasm-web':
        args.append('install_ems')

    args.append('build_engine')

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

    if branch:
        args.append("--branch=" + branch)

    if channel:
        args.append("--channel=" + channel)

    cmd = ' '.join(args)
    call(cmd)


def build_sdk():
    call('python scripts/build.py build_sdk')


def smoke_test():
    call('python scripts/build.py distclean install_ext smoke_test')


def main(argv):
    parser = ArgumentParser()
    parser.add_argument('commands', nargs="+", help="The command to execute")
    parser.add_argument("--platform", dest="platform", help="Platform to build for (when building the engine)")
    parser.add_argument("--branch", dest="branch", help="The branch to build for (when building engine and editor)")
    parser.add_argument("--with-asan", dest="with_asan", action='store_true', help="")
    parser.add_argument("--with-valgrind", dest="with_valgrind", action='store_true', help="")
    parser.add_argument("--with-vanilla-lua", dest="with_vanilla_lua", action='store_true', help="")
    parser.add_argument("--archive", dest="archive", action='store_true', help="")
    parser.add_argument("--skip-tests", dest="skip_tests", action='store_true', help="")
    parser.add_argument("--skip-builtins", dest="skip_builtins", action='store_true', help="")
    parser.add_argument("--skip-docs", dest="skip_docs", action='store_true', help="")
    args = parser.parse_args()

    platform = args.platform
    branch = args.branch
    if branch:
        branch = branch.replace("refs/heads/", "")
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


    print("S3 ACCESS KEY: %s" % (os.getenv('S3_ACCESS_KEY')))
    print("Platform: %s Branch: %s Channel: %s" % (platform, branch, channel))
    print("FOOO BOOO MOOO")
    exit(0)

    for command in args.commands:
        if command == "engine":
            if not platform:
                raise Exception("No --platform specified.")
            with_valgrind = args.with_valgrind or (branch in [ "master", "beta" ])
            build_engine(platform, with_valgrind = with_valgrind, with_asan = args.with_asan, with_vanilla_lua = args.with_vanilla_lua, archive = args.archive, skip_tests = args.skip_tests, skip_builtins = args.skip_builtins, skip_docs = args.skip_docs)
        elif command == "editor":
            if branch == "master" or branch == "beta" or branch == "dev":
                build_editor(channel = channel, release = True, engine_artifacts = "archived")
            elif branch == "editor-dev":
                build_editor(channel = "editor-alpha", release = True)
            else:
                build_editor(release = False, engine_artifacts = "archived")
                build_editor(release = False)
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
