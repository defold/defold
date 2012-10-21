import os, sys, subprocess, shutil, re
import Build, Options, Utils, Task
from Configure import conf
from TaskGen import extension, taskgen, feature, after, before
from Logs import error
import cc, cxx

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

ARM_DARWIN_ROOT='/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer'
IOS_SDK_VERSION="5.1"
MIN_IOS_SDK_VERSION=IOS_SDK_VERSION

@feature('cc', 'cxx')
# We must apply this before the objc_hook below
# Otherwise will .mm-files not be compiled with -arch armv6 etc.
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

    if platform == "linux" or platform == "darwin" or platform == "x86_64-darwin":
        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall'])
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
    elif platform == "armv6-darwin":
        for f in ['CCFLAGS', 'CXXFLAGS']:
            self.env.append_value(f, ['-g', '-O2', '-gdwarf-2', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall', '-arch', 'armv6', '-isysroot', '%s/SDKs/iPhoneOS%s.sdk' % (ARM_DARWIN_ROOT, IOS_SDK_VERSION)])
        self.env.append_value('LINKFLAGS', [ '-arch', 'armv6', '-lobjc', '-isysroot', '%s/SDKs/iPhoneOS%s.sdk' % (ARM_DARWIN_ROOT, IOS_SDK_VERSION), '-dead_strip', '-miphoneos-version-min=%s' % MIN_IOS_SDK_VERSION])
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
        identity = 'iPhone Developer: Christian MURRAY (QXZXCL5J5G)'

    mobileprovision = task.env.MOBILE_PROVISION
    if not mobileprovision:
        mobileprovision = 'engine_profile.mobileprovision'
    mobileprovision_path = os.path.join(task.env['DYNAMO_HOME'], 'share', mobileprovision)
    shutil.copyfile(mobileprovision_path, os.path.join(signed_exe_dir, 'embedded.mobileprovision'))

    entitlements = '/Users/chmu/Library/Developer/Xcode/DerivedData/test_iphone2-dsbdefmnlgdwdlchxwoxthhbgnwc/Build/Intermediates/test_iphone2.build/Debug-iphoneos/test_iphone2.build/test_iphone2.xcent'
    resource_rules_plist_file = task.resource_rules_plist.bldpath(task.env)

    ret = bld.exec_command('CODESIGN_ALLOCATE=/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate codesign -f -s "%s" --resource-rules=%s --entitlements %s %s' % (identity, resource_rules_plist_file, entitlements, signed_exe_dir))
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
def link_flags(self):
    platform = self.env['PLATFORM']
    if platform == 'linux':
        self.link_task.env.append_value('LINKFLAGS', ['-lpthread', '-lm'])

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

    if platform == "armv6-darwin":
        conf.env['GCC-OBJCXX'] = '-xobjective-c++'
        conf.env['GCC-OBJCLINK'] = '-lobjc'
        conf.env['CC'] = '%s/usr/bin/llvm-gcc-4.2' % (ARM_DARWIN_ROOT)
        conf.env['CXX'] = '%s/usr/bin/llvm-g++-4.2' % (ARM_DARWIN_ROOT)
        conf.env['LINK_CXX'] = '%s/usr/bin/llvm-g++-4.2' % (ARM_DARWIN_ROOT)
        conf.env['CPP'] = '%s/usr/bin/cpp-4.2' % (ARM_DARWIN_ROOT)
        conf.env['AR'] = '%s/usr/bin/ar' % (ARM_DARWIN_ROOT)
        conf.env['RANLIB'] = '%s/usr/bin/ranlib' % (ARM_DARWIN_ROOT)
        conf.env['LD'] = '%s/usr/bin/ld' % (ARM_DARWIN_ROOT)

    conf.check_tool('compiler_cc')
    conf.check_tool('compiler_cxx')

    if conf.env['CCACHE'] and not 'win32' == platform:
        # Prepend gcc/g++ with CCACHE
        for t in ['CC', 'CXX']:
            c = conf.env[t]
            if type(c) == list:
                conf.env[t] = [conf.env.CCACHE] + c
            else:
                conf.env[t] = [conf.env.CCACHE, c]

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
    opt.add_option('--platform', default='', dest='platform', help='target platform, eg armv6-darwin')
    opt.add_option('--skip-tests', action='store_true', default=False, dest='skip_tests', help='skip unit tests')
    opt.add_option('--skip-codesign', action="store_true", default=False, dest='skip_codesign', help='skip code signing')
