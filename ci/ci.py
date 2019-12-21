#!/usr/bin/env python

import sys
import subprocess
import platform
import os
import base64
from argparse import ArgumentParser

def call(args):
    print(args)
    ret = os.system(args)
    if ret != 0:
        exit(1)


def aptget(package):
    call("sudo apt-get install --no-install-recommends " + package)


def choco(package):
    call("choco install " + package + " -y")


def mingwget(package):
    call("mingw-get install " + package)


def setup_keychain(args):
    print("Setting up keychain")
    keychain_pass = "foobar"
    keychain_name = "defold.keychain"

    # create new keychain
    print("Creating keychain")
    # call("security delete-keychain {}".format(keychain_name))
    call("security create-keychain -p {} {}".format(keychain_pass, keychain_name))

    # set the new keychain as the default keychain
    print("Setting keychain as default")
    call("security default-keychain -s {}".format(keychain_name))

    # unlock the keychain
    print("Unlock keychain")
    call("security unlock-keychain -p {} {}".format(keychain_pass, keychain_name))

    # decode and import cert to keychain
    print("Decoding certificate")
    cert_path = os.path.join("ci", "cert.p12")
    cert_pass = args.keychain_cert_pass
    with open(cert_path, "wb") as file:
        file.write(base64.decodestring(args.keychain_cert))
    print("Importing certificate")
    # -A = allow access to the keychain without warning (https://stackoverflow.com/a/19550453)
    call("security import {} -k {} -P {} -A".format(cert_path, keychain_name, cert_pass))
    os.remove(cert_path)

    # required since macOS Sierra https://stackoverflow.com/a/40039594
    call("security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k {} {}".format(keychain_pass, keychain_name))
    # prevent the keychain from auto-locking
    call("security set-keychain-settings {}".format(keychain_name))

    # add the keychain to the keychain search list
    call("security list-keychains -d user -s {}".format(keychain_name))

    print("Done with keychain setup")


def install(args):
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
        aptget("lib32z1") # aapt: error while loading shared libraries: libz.so.1: cannot open shared object file: No such file or directory
    elif system == "Darwin":
        if args.keychain_cert:
            setup_keychain(args)


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
        args.append('--engine-artifacts=' + engine_artifacts)

    if channel:
        args.append('--channel=' + channel)

    cmd = ' '.join(args)
    call(cmd)


def build_bob(branch = None, channel = None, release = False):
    args = "python scripts/build.py distclean install_ext sync_archive build_bob archive_bob".split()

    if release:
        args.append("release")

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
    parser.add_argument("--keychain-cert", dest="keychain_cert", help="Base 64 encoded certificate to import to macOS keychain")
    parser.add_argument("--keychain-cert-pass", dest="keychain_cert_pass", help="Password for the certificate to import to macOS keychain")
    args = parser.parse_args()

    platform = args.platform
    branch = args.branch
    print("args.branch is {}".format(args.branch))
    if branch:
        branch = branch.replace("refs/heads/", "")

    # configure channel and release based on branch
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

    print("Using branch={} channel={} release={}".format(branch, channel, release))

    # execute commands
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
            elif branch.startswith("DEFEDIT-"):
                build_editor(release = False, engine_artifacts = "archived-stable")
            else:
                # Assume this is a branch for an engine related issue (DEF-xyz or Issue-xyz). Naming can vary though.
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
            install(args)
        else:
            print("Unknown command {0}".format(command))


if __name__ == "__main__":
    main(sys.argv[1:])
