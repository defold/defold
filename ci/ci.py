#!/usr/bin/env python

import sys
import subprocess
import platform
import os
import base64
from argparse import ArgumentParser

def call(args, failonerror = True):
    print(args)
    process = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, shell = True)

    output = ''
    while True:
        line = process.stdout.readline()
        if line != '':
            output += line
            print(line.rstrip())
        else:
            break

    if process.wait() != 0 and failonerror:
        exit(1)

    return output


def platform_from_host():
    system = platform.system()
    if system == "Linux":
        return "x86_64-linux"
    elif system == "Darwin":
        return "x86_64-darwin"
    else:
        return "x86_64-win32"

def aptget(package):
    call("sudo apt-get install -y --no-install-recommends " + package)

def aptfast(package):
    call("sudo apt-fast install -y --no-install-recommends " + package)

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
        # we use apt-fast to speed up apt-get downloads
        # https://github.com/ilikenwf/apt-fast
        call("sudo add-apt-repository ppa:apt-fast/stable")
        call("sudo apt-get update", failonerror=False)
        call("echo debconf apt-fast/maxdownloads string 16 | sudo debconf-set-selections")
        call("echo debconf apt-fast/dlflag boolean true | sudo debconf-set-selections")
        call("echo debconf apt-fast/aptmanager string apt-get | sudo debconf-set-selections")
        call("sudo apt-get install -y apt-fast aria2")

        call("sudo apt-get install -y software-properties-common")
        packages = [
            "gcc-5",
            "g++-5",
            "libssl-dev",
            "openssl",
            "libtool",
            "autoconf",
            "automake",
            "build-essential",
            "uuid-dev",
            "libxi-dev",
            "libopenal-dev",
            "libgl1-mesa-dev",
            "libglw1-mesa-dev",
            "freeglut3-dev",
            "tofrodos",
            "tree",
            "valgrind",
            "lib32z1",
            "xvfb"
        ]
        aptfast(" ".join(packages))
    elif system == "Darwin":
        if args.keychain_cert:
            setup_keychain(args)


def build_engine(platform, with_valgrind = False, with_asan = False, with_vanilla_lua = False, skip_tests = False, skip_codesign = True, skip_docs = False, skip_builtins = False, archive = False):
    args = 'python scripts/build.py distclean install_ext'.split()
    opts = []
    waf_opts = []

    opts.append('--platform=%s' % platform)

    if platform == 'js-web' or platform == 'wasm-web':
        args.append('install_ems')

    args.append('build_engine')

    if archive:
        args.append('archive_engine')

    if skip_codesign:
        opts.append('--skip-codesign')
    if skip_docs:
        opts.append('--skip-docs')
    if skip_builtins:
        opts.append('--skip-builtins')
    if skip_tests:
        opts.append('--skip-tests')
        waf_opts.append('--skip-build-tests')

    if with_valgrind:
        waf_opts.append('--with-valgrind')
    if with_asan:
        waf_opts.append('--with-asan')
    if with_vanilla_lua:
        waf_opts.append('--use-vanilla-lua')

    cmd = ' '.join(args + opts)

    # Add arguments to waf after a double-dash
    if waf_opts:
        cmd += ' -- ' + ' '.join(waf_opts)

    call(cmd)


def build_editor(branch = None, channel = None, engine_artifacts = None):
    opts = []

    if engine_artifacts:
        opts.append('--engine-artifacts=%s' % engine_artifacts)

    if branch:
        opts.append("--branch=%s" % branch)

    if channel:
        opts.append('--channel=%s' % channel)

    opts_string = ' '.join(opts)
    call('python scripts/build.py distclean install_ext build_editor2 --platform=%s %s' % (platform_from_host(), opts_string))
    for platform in ['x86_64-darwin', 'x86_64-linux', 'x86_64-win32']:
        call('python scripts/build.py bundle_editor2 --platform=%s %s' % (platform, opts_string))
        # call('python scripts/build.py bundle_editor2 archive_editor2 --platform=%s %s' % (platform, opts_string))


def notarize_editor(branch = None, channel = None, release = False, engine_artifacts = None, notarization_username = None, notarization_password = None, notarization_itc_provider = None):
    if not notarization_username or not notarization_password:
        print("No notarization username or password")
        exit(1)

    # args = 'python scripts/build.py download_editor2 notarize_editor2 archive_editor2'.split()
    args = 'python scripts/build.py notarize_editor2'.split()
    opts = []

    if release:
        args.append("release")

    if engine_artifacts:
        opts.append('--engine-artifacts=%s' % engine_artifacts)

    if branch:
        opts.append("--branch=%s" % branch)

    if channel:
        opts.append("--channel=%s" % channel)

    opts.append('--platform=x86_64-darwin')

    opts.append('--notarization-username=%s' % notarization_username)
    opts.append('--notarization-password=%s' % notarization_password)

    if notarization_itc_provider:
        opts.append('--notarization-itc-provider=%s' % notarization_itc_provider)

    cmd = ' '.join(args + opts)
    call(cmd)


def archive_editor(branch = None, channel = None, release = False, engine_artifacts = None):
    # args = 'python scripts/build.py download_editor2 notarize_editor2 archive_editor2'.split()
    args = 'python scripts/build.py archive_editor2'.split()
    opts = []

    if engine_artifacts:
        opts.append('--engine-artifacts=%s' % engine_artifacts)

    if release:
        args.append("release")

    if branch:
        opts.append("--branch=%s" % branch)

    if channel:
        opts.append("--channel=%s" % channel)

    cmd = ' '.join(args + opts)
    call(cmd)


def build_bob(branch = None, channel = None, release = False):
    args = "python scripts/build.py distclean install_ext sync_archive build_bob archive_bob".split()
    opts = []

    if release:
        args.append("release")

    if branch:
        opts.append("--branch=%s" % branch)

    if channel:
        opts.append("--channel=%s" % channel)

    cmd = ' '.join(args + opts)
    call(cmd)


def build_sdk():
    call('python scripts/build.py build_sdk')


def smoke_test():
    call('python scripts/build.py distclean install_ext smoke_test')

def get_branch():
    branch = call("git rev-parse --abbrev-ref HEAD")
    if branch == "HEAD":
        branch = call("git rev-parse HEAD")
    return branch

def main(argv):
    parser = ArgumentParser()
    parser.add_argument('commands', nargs="+", help="The command to execute (engine, build-editor, notarize-editor, archive-editor, bob, sdk, install, smoke)")
    parser.add_argument("--platform", dest="platform", help="Platform to build for (when building the engine)")
    parser.add_argument("--with-asan", dest="with_asan", action='store_true', help="")
    parser.add_argument("--with-valgrind", dest="with_valgrind", action='store_true', help="")
    parser.add_argument("--with-vanilla-lua", dest="with_vanilla_lua", action='store_true', help="")
    parser.add_argument("--archive", dest="archive", action='store_true', help="")
    parser.add_argument("--skip-tests", dest="skip_tests", action='store_true', help="")
    parser.add_argument("--skip-builtins", dest="skip_builtins", action='store_true', help="")
    parser.add_argument("--skip-docs", dest="skip_docs", action='store_true', help="")
    parser.add_argument("--engine-artifacts", dest="engine_artifacts", help="")
    parser.add_argument("--keychain-cert", dest="keychain_cert", help="Base 64 encoded certificate to import to macOS keychain")
    parser.add_argument("--keychain-cert-pass", dest="keychain_cert_pass", help="Password for the certificate to import to macOS keychain")
    parser.add_argument('--notarization-username', dest='notarization_username', help="Username to use when sending the editor for notarization")
    parser.add_argument('--notarization-password', dest='notarization_password', help="Password to use when sending the editor for notarization")
    parser.add_argument('--notarization-itc-provider', dest='notarization_itc_provider', help="Optional iTunes Connect provider to use when sending the editor for notarization")

    args = parser.parse_args()

    platform = args.platform
    # https://stackoverflow.com/a/55276236/1266551
    branch = get_branch()

    # configure build flags based on the branch
    if branch == "master":
        release_channel = "stable"
        editor_channel = "stable"
        release_bob = False
        release_editor = True
        engine_artifacts = args.engine_artifacts or "archived"
    elif branch == "beta":
        release_channel = "beta"
        editor_channel = "beta"
        release_bob = False
        release_editor = True
        engine_artifacts = args.engine_artifacts or "archived"
    elif branch == "dev":
        release_channel = "alpha"
        editor_channel = "alpha"
        release_bob = True
        release_editor = True
        engine_artifacts = args.engine_artifacts or "archived"
    elif branch == "editor-dev":
        release_channel = "alpha"
        editor_channel = "editor-alpha"
        release_bob = False
        release_editor = True
        engine_artifacts = args.engine_artifacts
    elif branch and branch.startswith("DEFEDIT-"):
        release_channel = None
        editor_channel = None
        release_bob = False
        release_editor = False
        engine_artifacts = args.engine_artifacts or "archived-stable"
    else: # engine dev branch
        release_channel = None
        editor_channel = None
        release_bob = False
        release_editor = False
        engine_artifacts = args.engine_artifacts or "archived"

    print("Using branch={} release_channel={} editor_channel={} engine_artifacts={}".format(branch, release_channel, editor_channel, engine_artifacts))

    # execute commands
    for command in args.commands:
        if command == "engine":
            if not platform:
                raise Exception("No --platform specified.")
            build_engine(
                platform,
                with_valgrind = args.with_valgrind or (branch in [ "master", "beta" ]),
                with_asan = args.with_asan,
                with_vanilla_lua = args.with_vanilla_lua,
                archive = args.archive,
                skip_tests = args.skip_tests,
                skip_builtins = args.skip_builtins,
                skip_docs = args.skip_docs)
        elif command == "build-editor":
            build_editor(
                branch = branch,
                channel = editor_channel,
                engine_artifacts = engine_artifacts)
        elif command == "notarize-editor":
            notarize_editor(
                branch = branch,
                channel = editor_channel,
                engine_artifacts = engine_artifacts,
                notarization_username = args.notarization_username,
                notarization_password = args.notarization_password,
                notarization_itc_provider = args.notarization_itc_provider)
        elif command == "archive-editor":
            archive_editor(
                branch = branch,
                channel = editor_channel,
                engine_artifacts = engine_artifacts,
                release = release_editor)
        elif command == "bob":
            build_bob(branch = branch, channel = release_channel, release = release_bob)
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
