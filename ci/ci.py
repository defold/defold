#!/usr/bin/env python
# Copyright 2020-2026 The Defold Foundation
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

import sys
import subprocess
import platform
import os
import base64
import re
from argparse import ArgumentParser
from ci_helper import is_platform_supported, is_platform_private, is_repo_private

# The platforms we deploy our editor on
PLATFORMS_DESKTOP = ('x86_64-linux', 'x86_64-win32', 'x86_64-macos', 'arm64-macos')

SENSITIVE_OPTIONS = (
    '--github-token',
    '--token',
    '--gcloud-service-key',
    '--notarization-password',
)

REDACTED = '[REDACTED]'

def redact_sensitive_data(text):
    for option in SENSITIVE_OPTIONS:
        option_pattern = re.escape(option)
        text = re.sub(r'(%s(?:=|\s+))(?:"[^"]*"|\'[^\']*\'|\S+)' % option_pattern, r'\1%s' % REDACTED, text)

    if re.search(r'(^|\s)security\s', text):
        text = re.sub(r'((?:^|\s)-[Pkp]\s+)(?:"[^"]*"|\'[^\']*\'|\S+)', r'\1%s' % REDACTED, text)

    return text

def call(args, failonerror = True):
    print(redact_sensitive_data(args))
    process = subprocess.Popen(args, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, shell = True)

    output = ''
    while True:
        line = process.stdout.readline().decode()
        if line != '':
            redacted_line = redact_sensitive_data(line)
            output += line
            print(redacted_line.rstrip())
        else:
            break

    if process.wait() != 0 and failonerror:
        exit(1)

    return output


def aptget(package):
    call("sudo apt-get install -y --no-install-recommends " + package)

def aptfast(package):
    call("sudo apt-fast install -y --no-install-recommends " + package)

def choco(package):
    call("choco install " + package + " -y")


def mingwget(package):
    call("mingw-get install " + package)


def string_to_file(str, destfile):
    with open(destfile, "wb") as f:
        f.write(str.encode())

def b64decode_to_file(str, destfile):
    with open(destfile, "wb") as f:
        f.write(base64.decodebytes(str.encode()))

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
    b64decode_to_file(args.keychain_cert, cert_path)

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

def get_github_token():
    return os.environ.get('SERVICES_GITHUB_TOKEN', None)

def install_linux(args):
    # # we use apt-fast to speed up apt-get downloads
    # # https://github.com/ilikenwf/apt-fast
    # call("sudo add-apt-repository ppa:apt-fast/stable")
    call("sudo apt-get update", failonerror=False)
    # call("echo debconf apt-fast/maxdownloads string 16 | sudo debconf-set-selections")
    # call("echo debconf apt-fast/dlflag boolean true | sudo debconf-set-selections")
    # call("echo debconf apt-fast/aptmanager string apt-get | sudo debconf-set-selections")
    # call("sudo apt-get install -y apt-fast aria2")

    call("sudo apt-get install -y software-properties-common")

    call("update-alternatives --display clang")
    call("update-alternatives --display clang++")

    # Legacy ncurses 5 libraries needed when building wasm-web.
    # Ubuntu 24.04/Noble runners no longer provide these package names in apt.
    if platform.machine() in ('aarch64', 'arm64'):
        ncurses_url = "http://ports.ubuntu.com/ubuntu-ports/pool/universe/n/ncurses"
        libtinfo_deb = "libtinfo5_6.3-2_arm64.deb"
        libncurses_deb = "libncurses5_6.3-2_arm64.deb"
    else:
        ncurses_url = "http://security.ubuntu.com/ubuntu/pool/universe/n/ncurses"
        libtinfo_deb = "libtinfo5_6.3-2ubuntu0.2_amd64.deb"
        libncurses_deb = "libncurses5_6.3-2ubuntu0.2_amd64.deb"

    call(f"wget {ncurses_url}/{libtinfo_deb} {ncurses_url}/{libncurses_deb}")
    call(f"sudo apt install -y ./{libtinfo_deb} ./{libncurses_deb}")

    clang_priority = 200 # GA runner has clang at prio 100, so let's add a higher prio
    clang_version = 17
    clang_path = "/usr/bin"
    clang_exe = f"/usr/bin/clang-{clang_version}" # installed on the recent GA runners

    # On older ubuntu 20 clang-16 isn't available
    # Also note that this is before the install_sdk step
    # if we had to install it ourselves, let's use the correct path
    if not os.path.exists(clang_exe):
        print(f"{clang_exe} not found. Installing LLVM + CLANG {clang_version} ...")

        call(f"wget https://apt.llvm.org/llvm.sh")
        call(f"chmod +x ./llvm.sh")
        call(f"sudo ./llvm.sh {clang_version}")
        call(f"rm ./llvm.sh")

        clang_path = f"/usr/lib/llvm-{clang_version}/bin"

        # Add and select the correct version
        call(f"sudo update-alternatives --install /usr/bin/clang clang {clang_path}/clang-{clang_version} {clang_priority}")
        call(f"sudo update-alternatives --install /usr/bin/clang++ clang++ {clang_path}/clang++ {clang_priority}")
        call(f"sudo update-alternatives --install /usr/bin/clang-cpp clang-cpp {clang_path}/clang-cpp {clang_priority}")
        call(f"sudo update-alternatives --install /usr/bin/llvm-ar llvm-ar {clang_path}/llvm-ar {clang_priority}")

    else:
        print(f"{clang_exe} found. Selecting LLVM + CLANG {clang_version} ...")
        # Add and select the correct version
        call(f"sudo update-alternatives --install /usr/bin/clang clang {clang_path}/clang-{clang_version} {clang_priority}")
        call(f"sudo update-alternatives --install /usr/bin/clang++ clang++ {clang_path}/clang++-{clang_version} {clang_priority}")
        call(f"sudo update-alternatives --install /usr/bin/clang-cpp clang-cpp {clang_path}/clang-cpp-{clang_version} {clang_priority}")
        call(f"sudo update-alternatives --install /usr/bin/llvm-ar llvm-ar {clang_path}/llvm-ar-{clang_version} {clang_priority}")

    call("update-alternatives --display clang")
    call("update-alternatives --display clang++")
    call("update-alternatives --display clang-cpp")
    call("update-alternatives --display llvm-ar")

    packages = [
        "autoconf",
        "automake",
        "build-essential",
        "freeglut3-dev",
        "libssl-dev",
        "libtool",
        "libxi-dev",
        "libx11-xcb-dev",
        "libxrandr-dev",
        "libopenal-dev",
        "libgl1-mesa-dev",
        "libglw1-mesa-dev",
        "openssl",
        "tofrodos",
        "tree",
        "valgrind",
        "uuid-dev",
        "xvfb"
    ]
    aptget(" ".join(packages))


def install_macos(args):
    if args.keychain_cert:
        setup_keychain(args)

def install(args):
    # installed tools: https://github.com/actions/virtual-environments/blob/main/images/linux/Ubuntu2404-Readme.md
    system = platform.system()
    print("Installing dependencies for system '%s' " % (system))
    if system == "Linux":
        install_linux(args)

    elif system == "Darwin":
        install_macos(args)

def create_gcloud_options(gcloud_service_key):
    gcloud_certfile = None
    gcloud_keyfile = None
    if gcloud_service_key:
        gcloud_certfile = os.path.join("ci", "gcloud_certfile.cer")
        gcloud_keyfile = os.path.join("ci", "gcloud_keyfile.json")
        b64decode_to_file(gcloud_service_key, gcloud_keyfile)

    opts = []
    # windows EV Code Signing with key in Google Cloud KMS
    if gcloud_keyfile and gcloud_certfile:
        opts.append("--gcloud-location=europe-west3")
        opts.append("--gcloud-keyname=ev-windows-key")
        opts.append("--gcloud-keyringname=ev-key-ring")
        opts.append("--gcloud-projectid=defold-editor")

        gcloud_keyfile = os.path.abspath(gcloud_keyfile)
        if not os.path.exists(gcloud_keyfile):
            print("Google Cloud key file not found:", gcloud_keyfile)
            sys.exit(1)

        print("Using Google Cloud key file", gcloud_keyfile)
        opts.append('--gcloud-keyfile=%s' % gcloud_keyfile)

        gcloud_certfile = os.path.abspath(gcloud_certfile)
        if not os.path.exists(gcloud_certfile):
            print("Google Cloud certificate not found:", gcloud_certfile)
            sys.exit(1)

        print("Using Google Cloud certificate ", gcloud_certfile)
        opts.append('--gcloud-certfile=%s' % gcloud_certfile)
    return opts

def build_engine(channel, platform, args):

    install_sdk = 'install_sdk'
    # for some platforms, we use the locally installed platform sdk
    if platform in ('x86_64-macos',
                    'arm64-macos',
                    'arm64-ios',
                    'x86_64-ios',
                    'wasm-web',
                    'wasm_pthread-web',
                    'arm64-linux',
                    'x86_64-linux',
                    'armv7-android',
                    'arm64-android'):
        install_sdk = ''

    cmd_args = ('"%s" scripts/build.py distclean %s install_ext check_sdk' % (sys.executable, install_sdk)).split()

    cmd_opts = []
    waf_opts = []

    cmd_opts.append('--platform=%s' % platform)
    # ccache isn't needed on CI
    cmd_opts.append('--disable-ccache')
    if args.verbose:
        cmd_opts.append('--verbose')

    cmd_args.append('build_engine')

    if channel:
        cmd_opts.append('--channel=%s' % channel)

    if args.archive:
        cmd_args.append('archive_engine')

    if args.codesign:
        cmd_opts.append('--codesign')
    if args.skip_docs:
        cmd_opts.append('--skip-docs')
    if args.skip_builtins:
        cmd_opts.append('--skip-builtins')
    if args.skip_tests:
        cmd_opts.append('--skip-tests')
    if args.skip_build_tests:
        waf_opts.append('--skip-build-tests')
    if args.codesign and args.gcloud_service_key:
        cmd_opts.extend(create_gcloud_options(args.gcloud_service_key))

    if args.with_valgrind:
        waf_opts.append('--with-valgrind')
    if args.with_asan:
        waf_opts.append('--with-asan')
    if args.with_ubsan:
        waf_opts.append('--with-ubsan')
    if args.with_tsan:
        waf_opts.append('--with-tsan')
    if args.with_vanilla_lua:
        waf_opts.append('--use-vanilla-lua')

    if platform == 'x86_64-linux':
        cmd_args.append('build_sdk_headers') # gather headers after a successful build

    cmd = ' '.join(cmd_args + cmd_opts)

    # Add arguments to waf after a double-dash
    if waf_opts:
        cmd += ' -- ' + ' '.join(waf_opts)

    call(cmd)

def build_editor2(channel, platform, args):
    if not platform in PLATFORMS_DESKTOP:
        raise Exception("Unsupported platform for editor build: %s" % platform)

    cmd_args = ('"%s" scripts/build.py distclean install_ext build_editor2' % sys.executable).split()
    cmd_opts = []
    cmd_opts.append('--channel=%s' % channel)
    cmd_opts.append('--platform=%s' % platform)

    if args.engine_artifacts:
        cmd_opts.append('--engine-artifacts=%s' % args.engine_artifacts)
    if args.codesign and args.notarization_username:
        cmd_opts.append('--notarization-username="%s"' % args.notarization_username)
    if args.codesign and args.notarization_password:
        cmd_opts.append('--notarization-password="%s"' % args.notarization_password)
    if args.codesign and args.notarization_itc_provider:
        cmd_opts.append('--notarization-itc-provider="%s"' % args.notarization_itc_provider)
    if args.codesign and args.gcloud_service_key:
        cmd_opts.extend(create_gcloud_options(args.gcloud_service_key))
    if args.skip_tests:
        cmd_opts.append('--skip-tests')
    if args.codesign:
        cmd_opts.append('--codesign')

    cmd = ' '.join(cmd_args + cmd_opts)
    call(cmd)

def test_editor(channel, platform, args):
    if not platform in PLATFORMS_DESKTOP:
        raise Exception("Unsupported platform for editor tests: %s" % platform)

    cmd_args = ('"%s" scripts/build.py distclean install_ext test_editor2' % sys.executable).split()
    cmd_opts = []
    cmd_opts.append('--channel=%s' % channel)
    cmd_opts.append('--platform=%s' % platform)

    if args.engine_artifacts:
        cmd_opts.append('--engine-artifacts=%s' % args.engine_artifacts)

    cmd = ' '.join(cmd_args + cmd_opts)
    call(cmd)

def archive_editor2(channel, platform, args):
    if platform is None:
        platforms = PLATFORMS_DESKTOP
    else:
        platforms = [platform]

    if args.skip_install_ext:
        cmd_args = ('"%s" scripts/build.py archive_editor2' % sys.executable).split()
    else:
        cmd_args = ('"%s" scripts/build.py install_ext archive_editor2' % sys.executable).split()

    for platform in platforms:
        cmd_opts = []
        cmd_opts.append("--channel=%s" % channel)
        cmd_opts.append('--platform=%s' % platform)

        if args.engine_artifacts:
            cmd_opts.append('--engine-artifacts=%s' % args.engine_artifacts)

        cmd = ' '.join(cmd_args + cmd_opts)
        call(cmd)

def distclean():
    call('"%s" scripts/build.py distclean' % sys.executable)


def install_ext(platform = None):
    cmd_args = ('"%s" scripts/build.py install_ext' % sys.executable).split()
    cmd_opts = []
    if platform:
        cmd_opts.append('--platform=%s' % platform)

    cmd = ' '.join(cmd_args + cmd_opts)
    call(cmd)


def build_bob(channel, branch, args):
    cmd_args = ('"%s" scripts/build.py install_ext sync_archive build_bob archive_bob' % sys.executable).split()
    cmd_opts = []
    cmd_opts.append("--channel=%s" % channel)
    if args.skip_tests:
        cmd_opts.append("--skip-tests")

    cmd = ' '.join(cmd_args + cmd_opts)
    call(cmd)

def test_bob(channel):
    call('"%s" scripts/build.py install_ext --channel=%s' % (sys.executable, channel))
    call('"%s" scripts/build.py test_bob --channel=%s' % (sys.executable, channel))


def release(channel, platform=None):
    cmd_args = ('"%s" scripts/build.py install_release_dependencies release' % sys.executable).split()
    cmd_opts = []
    cmd_opts.append("--channel=%s" % channel)
    if platform:
        cmd_opts.append("--platform=%s" % platform)

    token = get_github_token()
    if token:
        cmd_opts.append("--github-token=%s" % token)

    cmd = ' '.join(cmd_args + cmd_opts)
    call(cmd)

# Channels that generate and ship editor release notes.
RELEASE_NOTES_CHANNELS = ("alpha", "beta", "stable")

# DEV-ONLY (issue-7186 validation): let the feature branch exercise the full
# notes pipeline on a disposable channel. Delete this whole statement before
# merging to dev.
RELEASE_NOTES_CHANNELS = RELEASE_NOTES_CHANNELS + ("release-notes-view",)

# Channels where missing notes fail the release. Alpha builds continuously off an
# in-progress board, so it ships notes best-effort rather than blocking a build.
MANDATORY_RELEASE_NOTES_CHANNELS = ("beta", "stable")

def gen_release_notes(channel):
    if channel not in RELEASE_NOTES_CHANNELS:
        print("Channel '%s' does not ship release notes - skipping" % channel)
        return

    version = open("VERSION").read().strip()
    notes_md = os.path.join("releasenotes", "%s.md" % version)
    notes_json = os.path.join("releasenotes", "%s.json" % version)

    # Manually-authored notes win: if a file is already on disk, use it as-is and
    # don't hit the API or overwrite it.
    if os.path.exists(notes_md):
        if not os.path.exists(notes_json):
            raise Exception("%s already exists, but matching %s is missing" % (notes_md, notes_json))
        print("%s already exists - using manually-authored notes as-is" % notes_md)
        return

    # Run the generator. It exits non-zero if it errors or can't confirm a fix is
    # on the required branch(es). --use-github-compare makes that branch check use
    # the GitHub API, needed because CI clones are shallow.
    mandatory = channel in MANDATORY_RELEASE_NOTES_CHANNELS
    call('"%s" scripts/releasenotes_github_projectv2.py --version %s --channel %s --token %s --use-github-compare generate' % (
        sys.executable, version, channel, get_github_token()),
        failonerror = mandatory)

    if mandatory:
        if not os.path.exists(notes_md):
            raise Exception("No release notes produced for %s on channel '%s'" % (version, channel))
        if not os.path.exists(notes_json):
            raise Exception("No release notes JSON produced for %s on channel '%s'" % (version, channel))
    elif not os.path.exists(notes_md):
        print("::warning::No release notes generated for %s on '%s' - shipping without them" % (version, channel))

def build_sdk(channel, platform=None):
    cmd_args = ('"%s" scripts/build.py install_release_dependencies build_sdk' % sys.executable).split()
    cmd_opts = []
    cmd_opts.append("--channel=%s" % channel)
    if platform:
        cmd_opts.append("--platform=%s" % platform)

    cmd = ' '.join(cmd_args + cmd_opts)
    call(cmd)


def smoke_test():
    call('"%s" scripts/build.py distclean install_ext smoke_test' % sys.executable)



def get_branch():
    # Repository dispatch runs use this payload-derived ref for checkout.
    branch = os.environ.get('BUILD_BRANCH', '')
    if branch:
        return branch

    # The name of the head branch. Only set for pull request events.
    branch = os.environ.get('GITHUB_HEAD_REF', '')
    if branch == '':
        # The branch or tag name that triggered the workflow run.
        branch = os.environ.get('GITHUB_REF_NAME', '')

    if branch == '':
        # https://stackoverflow.com/a/55276236/1266551
        branch = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"], text=True).strip()
        if branch == "HEAD":
            branch = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()

    return branch

def release_settings_for_branch(branch):
    if branch == "master":
        return "stable", True
    if branch == "beta":
        return "beta", True
    if branch == "dev":
        return "alpha", True
    # DEV-ONLY (issue-7186 validation): release this branch to a disposable
    # custom channel so the full pipeline runs without touching production
    # (no tags / no GitHub release / no production channel). Remove before
    # merging to dev.
    if branch == "issue-7186-release-notes-view":
        return "release-notes-view", True
    return "dev", False

def should_release_branch(branch):
    return release_settings_for_branch(branch)[1]

def release_notes_required_for_branch(branch):
    channel = release_settings_for_branch(branch)[0]
    return channel in RELEASE_NOTES_CHANNELS

def get_pull_request_target_branch():
    # The name of the base (or target) branch. Only set for pull request events.
    return os.environ.get('GITHUB_BASE_REF', '')

def main(argv):
    parser = ArgumentParser()
    parser.add_argument('commands', nargs="+", help="The command to execute (engine, build-editor, test-editor, archive-editor, gen-release-notes, bob, test-bob, sdk, install, smoke, should-release, requires-release-notes, should-build-platform)")
    parser.add_argument("--platform", dest="platform", help="Platform to build for (when building the engine)")
    parser.add_argument("--with-asan", dest="with_asan", action='store_true', help="")
    parser.add_argument("--with-ubsan", dest="with_ubsan", action='store_true', help="")
    parser.add_argument("--with-tsan", dest="with_tsan", action='store_true', help="")
    parser.add_argument("--with-valgrind", dest="with_valgrind", action='store_true', help="")
    parser.add_argument("--with-vanilla-lua", dest="with_vanilla_lua", action='store_true', help="")
    parser.add_argument("--archive", dest="archive", action='store_true', help="Archive engine artifacts to S3")
    parser.add_argument("--skip-tests", dest="skip_tests", action='store_true', help="")
    parser.add_argument("--skip-build-tests", dest="skip_build_tests", action='store_true', help="")
    parser.add_argument("--skip-builtins", dest="skip_builtins", action='store_true', help="")
    parser.add_argument("--skip-docs", dest="skip_docs", action='store_true', help="")
    parser.add_argument("--codesign", dest="codesign", action='store_true', help="Enable code signing")
    parser.add_argument("--verbose", dest="verbose", action='store_true', help="Enable verbose build output")
    parser.add_argument("--engine-artifacts", dest="engine_artifacts", default="archived", help="Engine artifacts to include when building the editor")
    parser.add_argument("--channel", dest="channel", help="Override the release channel derived from the branch")
    parser.add_argument("--skip-install-ext", dest="skip_install_ext", action='store_true', help="Skip install_ext before archive-editor")
    parser.add_argument("--keychain-cert", dest="keychain_cert", help="Base 64 encoded certificate to import to macOS keychain")
    parser.add_argument("--keychain-cert-pass", dest="keychain_cert_pass", help="Password for the certificate to import to macOS keychain")
    parser.add_argument("--gcloud-service-key", dest="gcloud_service_key", help="String containing Google Cloud service account key")
    parser.add_argument('--notarization-username', dest='notarization_username', help="Username to use when sending the editor for notarization")
    parser.add_argument('--notarization-password', dest='notarization_password', help="Password to use when sending the editor for notarization")
    parser.add_argument('--notarization-itc-provider', dest='notarization_itc_provider', help="Optional iTunes Connect provider to use when sending the editor for notarization")
    parser.add_argument('--github-token', dest='github_token', help='GitHub authentication token when releasing to GitHub')
    parser.add_argument('--github-target-repo', dest='github_target_repo', help='GitHub target repo when releasing artefacts')
    parser.add_argument('--github-sha1', dest='github_sha1', help='A specific sha1 to use in github operations')

    args = parser.parse_args()

    platform = args.platform

    if args.commands == ["should-build-platform"]:
        print("true" if platform and is_platform_supported(platform) else "false")
        return

    if platform and not is_platform_supported(platform):
        print("Platform {} is private and the repo '{}' cannot build for this platform. Skipping".format(platform, os.environ.get('GITHUB_REPOSITORY', '')))
        return;

    # saving lots of CI minutes and waiting by not building the editor, which we don't use
    if is_repo_private():
        repo = os.environ.get('GITHUB_REPOSITORY', 'defold')
        for command in args.commands:
            if 'editor' in command or 'bob' in command:
                print("The repo {} is private. We've disabled building the editor and bob. Skipping".format(repo))
                return

        if platform and not is_platform_private(platform):
            if platform not in ['x86_64-win32', 'x86_64-linux']:
                print("The repo {} is private. We've disabled building the platform {}. Skipping".format(repo, platform))
                return

    branch = get_branch()

    if args.commands == ["should-release"]:
        print("true" if should_release_branch(branch) else "false")
        return

    if args.commands == ["requires-release-notes"]:
        print("true" if release_notes_required_for_branch(branch) else "false")
        return

    channel, make_release = release_settings_for_branch(branch)
    if args.channel:
        channel = args.channel

    print(f"Using branch={branch} channel={channel} engine_artifacts={args.engine_artifacts}")

    # execute commands
    for command in args.commands:
        if command == "engine":
            if not platform:
                raise Exception("No --platform specified.")
            build_engine(channel, platform, args)
        elif command == "build-editor":
            if not platform:
                raise Exception("No --platform specified.")
            build_editor2(channel, platform, args)
        elif command == "test-editor":
            if not platform:
                raise Exception("No --platform specified.")
            test_editor(channel, platform, args)
        elif command == "archive-editor":
            archive_editor2(channel, platform, args)
        elif command == "gen-release-notes":
            gen_release_notes(channel)
        elif command == "bob":
            build_bob(channel, branch, args)
        elif command == "test-bob":
            test_bob(channel)
        elif command == "sdk":
            build_sdk(channel, platform)
        elif command == "smoke":
            smoke_test()
        elif command == "install":
            install(args)
        elif command == "install_ext":
            install_ext(platform = platform)
        elif command == "distclean":
            distclean()
        elif command == "release":
            if make_release:
                release(channel, platform)
            else:
                print("Branch '%s' is not configured for automatic release from CI" % branch)
        else:
            print("Unknown command {0}".format(command))


if __name__ == "__main__":
    main(sys.argv[1:])
