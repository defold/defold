#!/usr/bin/env python

import os
import sys
import subprocess
from argparse import ArgumentParser

def call(args):
    print(args.joins(" "))
    subprocess.call(args, stdin=None, stdout=None, stderr=None, shell=False)


def install(platform):
    if platform == 'linux' or platform == 'linux-64':
        call("sudo apt-get install libxi-dev freeglut3-dev libglu1-mesa-dev libgl1-mesa-dev libxext-dev x11proto-xext-dev mesa-common-dev libxt-dev libx11-dev libcurl4-openssl-dev uuid-dev python-setuptools build-essential libopenal-dev rpm git curl autoconf libtool automake cmake tofrodos valgrind tree silversearcher-ag".split(" "))


def make_engine_builder(platform, skip_tests = True, with_valgrind = False, with_asan = False, with_vanilla_lua = False, skip_codesign = True, skip_docs = True, skip_builtins = True, branch = None, archive = False, org = 'defold', name = None):
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
    elif platform == 'linux':
        args.append('--platform=linux')
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

    if name is None:
        name = 'engine-%s' % (platform)

    call(cmd.split(" "))
    # return make_builder(name, slaves, cmd, branch = branch, org = org)


def main(argv):
    parser = ArgumentParser()
    parser.add_argument('commands', nargs="+", help="The command to execute")
    parser.add_argument("--platform", dest="platform", help="Platform to build for")
    args = parser.parse_args()

    if not args.platform:
        raise Exception("No --platform specified.")


    for command in args.commands:
        if command == "build":
            make_engine_builder(args.platform)
        elif command == "install":
            install(args.platform)
        else:
            print("Unknown command {0}".format(command))


if __name__ == "__main__":
    main(sys.argv[1:])
