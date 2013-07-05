import os, sys, subprocess, shutil, re, stat
import Build, Options, Utils, Task, Logs
from Configure import conf
from TaskGen import extension, taskgen, feature, after, before
from Logs import error
import cc, cxx

ANDROID_ROOT=os.path.join(os.environ['HOME'], 'android')
ANDROID_NDK_VERSION='8e'
ANDROID_NDK_API_VERSION='14'
ANDROID_API_VERSION='17'
ANDROID_GCC_VERSION='4.7'

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

IOS_TOOLCHAIN_ROOT='/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain'
ARM_DARWIN_ROOT='/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer'
IOS_SDK_VERSION="6.1"
# NOTE: Minimum iOS-version is also specified in Info.plist-files
# (MinimumOSVersion and perhaps DTPlatformVersion)
MIN_IOS_SDK_VERSION="4.3"

@feature('cc', 'cxx')
# We must apply this before the objc_hook below
# Otherwise will .mm-files not be compiled with -arch armv7 etc.
# I don't know if this is entirely correct
@before('apply_core')
def default_flags(self):

    platform = self.env['PLATFORM']
    build_platform = self.env['BUILD_PLATFORM']
    dynamo_home = self.env['DYNAMO_HOME']
    dynamo_ext = os.path.join(dynamo_home, "ext")

    if 'darwin' in platform:
        # OSX and iOS
        self.env.append_value('LINKFLAGS', ['-framework', 'Foundation'])

        if 'arm' in platform:
            # iOS
            self.env.append_value('LINKFLAGS', ['-framework', 'UIKit'])
        else:
            # OSX
            self.env.append_value('LINKFLAGS', ['-framework', 'AppKit'])

    if platform == "linux" or platform == "darwin" or platform == "x86_64-darwin":
        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall', '-fno-exceptions',])
            if platform == "darwin":
                self.env.append_value(f, ['-m32'])
            # We link by default to uuid on linux. libuuid is wrapped in dlib (at least currently)
        if platform == "darwin":
            self.env.append_value('LINKFLAGS', ['-m32'])
        if platform == "darwin" or platform == "x86_64-darwin":
            # OSX only
            self.env.append_value('LINKFLAGS', ['-framework', 'Carbon'])
        elif platform == "linux":
            # Linux only
            pass
    elif platform == "armv7-darwin":
        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-O2', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall', '-fno-exceptions', '-arch', 'armv7', '-isysroot', '%s/SDKs/iPhoneOS%s.sdk' % (ARM_DARWIN_ROOT, IOS_SDK_VERSION)])
        self.env.append_value('LINKFLAGS', [ '-arch', 'armv7', '-lobjc', '-isysroot', '%s/SDKs/iPhoneOS%s.sdk' % (ARM_DARWIN_ROOT, IOS_SDK_VERSION), '-dead_strip', '-miphoneos-version-min=%s' % MIN_IOS_SDK_VERSION])
    elif platform == 'armv7-android':

        sysroot='%s/android-ndk-r%s/platforms/android-%s/arch-arm' % (ANDROID_ROOT, ANDROID_NDK_VERSION, ANDROID_NDK_API_VERSION)
        stl="%s/android-ndk-r%s/sources/cxx-stl/gnu-libstdc++/%s/include" % (ANDROID_ROOT, ANDROID_NDK_VERSION, ANDROID_GCC_VERSION)
        stl_lib="%s/android-ndk-r%s/sources/cxx-stl/gnu-libstdc++/%s/libs/armeabi-v7a" % (ANDROID_ROOT, ANDROID_NDK_VERSION, ANDROID_GCC_VERSION)
        stl_arch="%s/include" % stl_lib

        for f in ['CCFLAGS', 'CXXFLAGS']:
            # NOTE: 
            # -mthumb and -funwind-tables removed from default flags
            # -fno-exceptions added
            self.env.append_value(f, ['-g', '-Os', '-gdwarf-2', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall',
                                      '-fpic', '-ffunction-sections', '-fstack-protector',
                                      '-D__ARM_ARCH_5__', '-D__ARM_ARCH_5T__', '-D__ARM_ARCH_5E__', '-D__ARM_ARCH_5TE__',
                                      '-Wno-psabi', '-march=armv7-a', '-mfloat-abi=softfp', '-mfpu=vfp',
                                      '-fomit-frame-pointer', '-fno-strict-aliasing', '-finline-limit=64', '-fno-exceptions',
                                      '-I%s/android-ndk-r%s/sources/android/native_app_glue' % (ANDROID_ROOT, ANDROID_NDK_VERSION),
                                      '-I%s/tmp/android-ndk-r%s/platforms/android-%s/arch-arm/usr/include' % (ANDROID_ROOT, ANDROID_NDK_VERSION, ANDROID_NDK_API_VERSION),
                                      '-I%s' % stl,
                                      '-I%s' % stl_arch,
                                      '--sysroot=%s' % sysroot,
                                      '-DANDROID', '-Wa,--noexecstack'])

        # TODO: Should be part of shared libraries
        # -Wl,-soname,libnative-activity.so -shared
        # -lgnustl_static -lsupc++
        self.env.append_value('LINKFLAGS', [
                '--sysroot=%s' % sysroot,
                '-Wl,--fix-cortex-a8', '-Wl,--no-undefined', '-Wl,-z,noexecstack', '-landroid',
                '-L%s' % stl_lib])
    else:
        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS'])
        self.env.append_value('LINKFLAGS', '/DEBUG')
        self.env.append_value('LINKFLAGS', ['shell32.lib', 'WS2_32.LIB'])

    if platform == build_platform:
        # Host libraries are installed to $PREFIX/lib
        self.env.append_value('LIBPATH', os.path.join(dynamo_home, "lib"))
    else:
        # Cross libraries are installed to $PREFIX/lib/PLATFORM
        self.env.append_value('LIBPATH', os.path.join(dynamo_home, "lib", platform))

    self.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    self.env.append_value('CPPPATH', os.path.join(dynamo_home, "include"))
    self.env.append_value('CPPPATH', os.path.join(dynamo_home, "include", platform))
    self.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))

@feature('cprogram', 'cxxprogram', 'cstaticlib', 'cshlib')
@after('apply_obj_vars')
def android_link_flags(self):
    platform = self.env['PLATFORM']
    build_platform = self.env['BUILD_PLATFORM']
    if re.match('arm.*?android', platform):
        self.link_task.env.append_value('LINKFLAGS', ['-lgnustl_static', '-lm', '-llog', '-lc'])
        self.link_task.env.append_value('LINKFLAGS', '-Wl,--no-undefined -Wl,-z,noexecstack -Wl,-z,relro -Wl,-z,now'.split())

        if 'apk' in self.features:
            # NOTE: This is a hack We change cprogram -> cshlib
            # but it's probably to late. It works for the name though (libX.so and not X)
            self.link_task.env.append_value('LINKFLAGS', ['-shared'])

@feature('apk')
@before('apply_core')
def apply_apk_test(self):
    platform = self.env['PLATFORM']
    build_platform = self.env['BUILD_PLATFORM']
    if re.match('arm.*?android', platform):
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
        <string>TODO_PREFIX.%(executable)s</string>
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
        <key>LSRequiresIPhoneOS</key>
        <true/>
        <key>MinimumOSVersion</key>
        <string>4.3</string>
        <key>UIDeviceFamily</key>
        <array>
                <integer>1</integer>
                <integer>2</integer>
        </array>
        <key>UIStatusBarHidden</key>
        <true/>
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

    mobileprovision = task.env.MOBILE_PROVISION
    if not mobileprovision:
        mobileprovision = 'engine_profile.mobileprovision'
    mobileprovision_path = os.path.join(task.env['DYNAMO_HOME'], 'share', mobileprovision)
    shutil.copyfile(mobileprovision_path, os.path.join(signed_exe_dir, 'embedded.mobileprovision'))

    entitlements = 'engine_profile.xcent'
    entitlements_path = os.path.join(task.env['DYNAMO_HOME'], 'share', entitlements)
    resource_rules_plist_file = task.resource_rules_plist.bldpath(task.env)

    ret = bld.exec_command('CODESIGN_ALLOCATE=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate codesign -f -s "%s" --resource-rules=%s --entitlements %s %s' % (identity, resource_rules_plist_file, entitlements_path, signed_exe_dir))
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
    info_plist_file.write(INFO_PLIST % { 'executable' : task.exe_name })
    info_plist_file.close()

    resource_rules_plist_file = open(task.resource_rules_plist.bldpath(task.env), 'wb')
    resource_rules_plist_file.write(RESOURCE_RULES_PLIST)
    resource_rules_plist_file.close()

    return 0

Task.task_type_from_func('app_bundle',
                         func = app_bundle,
                         vars = ['SRC', 'DST'],
                         #color = 'RED',
                         after  = 'cxx_link cc_link static_link')

@taskgen
@after('apply_link')
@feature('cprogram')
def create_app_bundle(self):
    if not re.match('arm.*?darwin', self.env['PLATFORM']):
        return

    app_bundle_task = self.create_task('app_bundle', self.env)
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
        codesign.resource_rules_plist = resource_rules_plist
        codesign.set_inputs(self.link_task.outputs)
        codesign.set_outputs(signed_exe)

        codesign.exe = self.link_task.outputs[0]
        codesign.signed_exe = signed_exe

ANDROID_MANIFEST = """<?xml version="1.0" encoding="utf-8"?>
<!-- BEGIN_INCLUDE(manifest) -->
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
        package="com.defold.%(package)s"
        android:versionCode="1"
        android:versionName="1.0">

    <uses-sdk android:minSdkVersion="9" />
    <application android:label="%(app_name)s" android:hasCode="false" android:debuggable="true">
        <activity android:name="android.app.NativeActivity"
                android:label="%(app_name)s"
                android:configChanges="orientation|keyboardHidden">
            <meta-data android:name="android.app.lib_name"
                    android:value="%(lib_name)s" />
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
    <uses-permission android:name="android.permission.INTERNET" />

</manifest>
<!-- END_INCLUDE(manifest) -->
"""

ANDROID_STUB = """
extern void _glfwPreMain(struct android_app* state);

void android_main(struct android_app* state)
{
    // Make sure glue isn't stripped.
    app_dummy();
    _glfwPreMain(state);
}
"""

def android_package(task):
    bld = task.generator.bld

    manifest_file = open(task.manifest.bldpath(task.env), 'wb')
    manifest_file.write(ANDROID_MANIFEST % { 'package' : task.exe_name, 'app_name' : task.exe_name, 'lib_name' : task.exe_name })
    manifest_file.close()

    aapt = '%s/android-sdk/platform-tools/aapt' % (ANDROID_ROOT)
    android_jar = '%s/android-sdk/platforms/android-%s/android.jar' % (ANDROID_ROOT, ANDROID_API_VERSION)
    manifest = task.manifest.abspath(task.env)
    ap_ = task.ap_.abspath(task.env)
    native_lib = task.native_lib.abspath(task.env)
    gdbserver = task.gdbserver.abspath(task.env)
    shutil.copy(task.native_lib_in.abspath(task.env), native_lib)
    shutil.copy('%s/android-ndk-r%s/prebuilt/android-arm/gdbserver/gdbserver' % (ANDROID_ROOT, ANDROID_NDK_VERSION), gdbserver)

    ret = bld.exec_command('%s package --no-crunch -f --debug-mode -M %s -I %s -F %s' % (aapt, manifest, android_jar, ap_))
    if ret != 0:
        error('Error running aapt')
        return 1

    apkbuilder = '%s/android-sdk/tools/apkbuilder' % (ANDROID_ROOT)
    apk_unaligned = task.apk_unaligned.abspath(task.env)
    libs_dir = task.native_lib.parent.parent.abspath(task.env)
    ret = bld.exec_command('%s %s -v -z %s -nf %s -d' % (apkbuilder, apk_unaligned, ap_, libs_dir))
    if ret != 0:
        error('Error running apkbuilder')
        return 1

    apk = task.apk.abspath(task.env)
    zipalign = '%s/android-sdk/tools/zipalign' % (ANDROID_ROOT)
    ret = bld.exec_command('%s -f 4 %s %s' % (zipalign, apk_unaligned, apk))
    if ret != 0:
        error('Error running zipalign')
        return 1

    with open(task.android_mk.abspath(task.env), 'wb') as f:
        print >>f, 'APP_ABI := armeabi-v7a'

    with open(task.application_mk.abspath(task.env), 'wb') as f:
        print >>f, ''

    with open(task.gdb_setup.abspath(task.env), 'wb') as f:
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

    android_package_task = self.create_task('android_package', self.env)
    android_package_task.set_inputs(self.link_task.outputs)

    exe_name = self.name
    lib_name = self.link_task.outputs[0].name

    android_package_task.exe_name = exe_name

    manifest = self.path.exclusive_build_node("%s.android/AndroidManifest.xml" % exe_name)
    android_package_task.manifest = manifest

    native_lib = self.path.exclusive_build_node("%s.android/libs/armeabi-v7a/%s" % (exe_name, lib_name))
    android_package_task.native_lib = native_lib
    android_package_task.native_lib_in = self.link_task.outputs[0]

    gdbserver = self.path.exclusive_build_node("%s.android/libs/armeabi-v7a/gdbserver" % (exe_name))
    android_package_task.gdbserver = gdbserver

    ap_ = self.path.exclusive_build_node("%s.android/%s.ap_" % (exe_name, exe_name))
    android_package_task.ap_ = ap_

    apk_unaligned = self.path.exclusive_build_node("%s.android/%s-unaligned.apk" % (exe_name, exe_name))
    android_package_task.apk_unaligned = apk_unaligned

    apk = self.path.exclusive_build_node("%s.android/%s.apk" % (exe_name, exe_name))
    android_package_task.apk = apk

    # NOTE: These files are required for ndk-gdb
    android_package_task.android_mk = self.path.exclusive_build_node("%s.android/jni/Android.mk" % (exe_name))
    android_package_task.application_mk = self.path.exclusive_build_node("%s.android/jni/Application.mk" % (exe_name))
    android_package_task.gdb_setup = self.path.exclusive_build_node("%s.android/libs/armeabi-v7a/gdb.setup" % (exe_name))

    android_package_task.set_outputs([native_lib, manifest, ap_, apk_unaligned, apk, gdbserver,
                                      android_package_task.android_mk, android_package_task.application_mk, android_package_task.gdb_setup])

    self.android_package_task = android_package_task

def copy_glue(task):
    with open(task.glue_file, 'rb') as in_f:
        with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
            out_f.write(in_f.read())

    with open(task.outputs[1].bldpath(task.env), 'wb') as out_f:
        out_f.write(ANDROID_STUB)

    return 0

task = Task.task_type_from_func('copy_glue',
                                func  = copy_glue,
                                color = 'PINK',
                                before  = 'cc cxx')

from Constants import RUN_ME

task.runnable_status = lambda self: RUN_ME

@taskgen
@before('apply_core')
@feature('apk')
def create_copy_glue(self):
    if not re.match('arm.*?android', self.env['PLATFORM']):
        return

    glue = self.path.find_or_declare('android_native_app_glue.c')
    self.allnodes.append(glue)
    stub = self.path.find_or_declare('androd_stub.c')
    self.allnodes.append(stub)

    task = self.create_task('copy_glue')
    task.glue_file = '%s/android-ndk-r%s/sources/android/native_app_glue/android_native_app_glue.c' % (ANDROID_ROOT, ANDROID_NDK_VERSION)
    task.set_outputs([glue, stub])

def embed_build(task):
    symbol = task.inputs[0].name.upper().replace('.', '_').replace('-', '_')
    in_file = open(task.inputs[0].bldpath(task.env), 'rb')
    out_file = open(task.outputs[0].bldpath(task.env), 'wb')

    cpp_str = """
#include <stdint.h>
unsigned char %s[] =
"""
    out_file.write(cpp_str % (symbol))
    out_file.write('{\n    ')

    data = in_file.read()
    for i,x in enumerate(data):
        out_file.write('0x%X, ' % ord(x))
        if i > 0 and i % 4 == 0:
            out_file.write('\n    ')
    out_file.write('\n};\n')
    out_file.write('uint32_t %s_SIZE = sizeof(%s);\n' % (symbol, symbol))

    out_file.close()

    m = Utils.md5()
    m.update(data)

    task.generator.bld.node_sigs[task.inputs[0].variant(task.env)][task.inputs[0].id] = m.digest()
    return 0

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
        node = self.path.find_resource(name)
        cc_out = node.parent.find_or_declare([node.name + '.embed.cpp'])

        task = self.create_task('embed_file')
        task.set_inputs(node)
        task.set_outputs(cc_out)
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

def run_gtests(valgrind = False):
    if not Options.commands['build'] or getattr(Options.options, 'skip_tests', False):
        return

# TODO: Add something similar to this
# http://code.google.com/p/v8/source/browse/trunk/tools/run-valgrind.py
# to find leaks and set error code

    if not Build.bld.env['VALGRIND']:
        valgrind = False

    for t in  Build.bld.all_task_gen:
        if hasattr(t, 'uselib') and str(t.uselib).find("GTEST") != -1:
            output = t.path
            filename = os.path.join(output.abspath(t.env), t.target)
            if valgrind:
                dynamo_home = os.getenv('DYNAMO_HOME')
                filename = "valgrind -q --leak-check=full --suppressions=%s/share/valgrind-python.supp --suppressions=%s/share/valgrind-libasound.supp --suppressions=%s/share/valgrind-libdlib.supp --error-exitcode=1 %s" % (dynamo_home, dynamo_home, dynamo_home, filename)
            proc = subprocess.Popen(filename, shell = True)
            ret = proc.wait()
            if ret != 0:
                print("test failed %s" %(t.target) )
                sys.exit(ret)

@feature('cprogram', 'cxxprogram', 'cstaticlib', 'cshlib')
@after('apply_obj_vars')
def linux_link_flags(self):
    platform = self.env['PLATFORM']
    if platform == 'linux':
        self.link_task.env.append_value('LINKFLAGS', ['-lpthread', '-lm'])

def create_clang_wrapper(conf, exe):
    clang_wrapper_path = os.path.join(conf.env['DYNAMO_HOME'], 'bin', '%s-wrapper.sh' % exe)

    s = '#!/bin/sh\n'
    s += "%s $@\n" % os.path.join(IOS_TOOLCHAIN_ROOT, 'usr/bin/%s' % exe)
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

def detect(conf):
    conf.find_program('valgrind', var='VALGRIND', mandatory = False)
    conf.find_program('ccache', var='CCACHE', mandatory = False)

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")

    conf.env['DYNAMO_HOME'] = dynamo_home

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

    if platform == 'darwin' or platform == 'x86_64-darwin':
        # Force gcc without llvm on darwin.
        # We got strange bugs with http cache with gcc-llvm...
        os.environ['CC'] = 'gcc-4.2'
        os.environ['CXX'] = 'g++-4.2'

    conf.env['PLATFORM'] = platform
    conf.env['BUILD_PLATFORM'] = build_platform

    if platform == "armv7-darwin":
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
        conf.env['LINK_CXX'] = '%s/usr/bin/clang++' % (IOS_TOOLCHAIN_ROOT)
        conf.env['CPP'] = '%s/usr/bin/clang -E' % (IOS_TOOLCHAIN_ROOT)
        conf.env['AR'] = '%s/usr/bin/ar' % (IOS_TOOLCHAIN_ROOT)
        conf.env['RANLIB'] = '%s/usr/bin/ranlib' % (IOS_TOOLCHAIN_ROOT)
        conf.env['LD'] = '%s/usr/bin/ld' % (IOS_TOOLCHAIN_ROOT)
    elif platform == "armv7-android":
        # TODO: No windows support yet (unknown path to compiler when wrote this)
        if build_platform == 'linux':
            arch = 'x86'
        else:
            arch = 'x86_64'

        bin='%s/android-ndk-r%s/toolchains/arm-linux-androideabi-%s/prebuilt/%s-%s/bin' % (ANDROID_ROOT, ANDROID_NDK_VERSION, ANDROID_GCC_VERSION, build_platform, arch)
        conf.env['CC'] = '%s/arm-linux-androideabi-gcc' % (bin)
        conf.env['CXX'] = '%s/arm-linux-androideabi-g++' % (bin)
        conf.env['LINK_CXX'] = '%s/arm-linux-androideabi-g++' % (bin)
        conf.env['CPP'] = '%s/arm-linux-androideabi-cpp' % (bin)
        conf.env['AR'] = '%s/arm-linux-androideabi-ar' % (bin)
        conf.env['RANLIB'] = '%s/arm-linux-androideabi-ranlib' % (bin)
        conf.env['LD'] = '%s/arm-linux-androideabi-ld' % (bin)

    conf.check_tool('compiler_cc')
    conf.check_tool('compiler_cxx')

    if conf.env['CCACHE'] and not 'win32' == platform:
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

    if platform == build_platform:
        # Host libraries are installed to $PREFIX/lib
        conf.env.BINDIR = Utils.subst_vars('${PREFIX}/bin', conf.env)
    else:
        # Cross libraries are installed to $PREFIX/lib/PLATFORM
        conf.env.BINDIR = Utils.subst_vars('${PREFIX}/bin/%s' % platform, conf.env)
        conf.env.LIBDIR = Utils.subst_vars('${PREFIX}/lib/%s' % platform, conf.env)

    if platform == "linux":
        conf.env['LIB_PLATFORM_SOCKET'] = ''
        conf.env['LIB_DL'] = 'dl'
        conf.env['LIB_UUID'] = 'uuid'
    elif 'darwin' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif 'android' in platform:
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    else:
        conf.env['LIB_PLATFORM_SOCKET'] = 'WS2_32'

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
    opt.add_option('--skip-tests', action='store_true', default=False, dest='skip_tests', help='skip unit tests')
    opt.add_option('--skip-codesign', action="store_true", default=False, dest='skip_codesign', help='skip code signing')
    opt.add_option('--disable-ccache', action="store_true", default=False, dest='disable_ccache', help='force disable of ccache')

