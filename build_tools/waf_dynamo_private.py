import sys, os, shutil, time
import Options
import Task, Node, Utils
from TaskGen import extension, taskgen, feature, after, before
import waf_dynamo

# we don't want to look for the msvc compiler when cross compiling (it takes 10+ seconds!)
def _dont_look_for_msvc():
    if 'msvc' in Options.options.check_c_compiler:
        Options.options.check_c_compiler = 'gcc'
    if 'msvc' in Options.options.check_cxx_compiler:
        Options.options.check_cxx_compiler = 'g++'

def _set_ccflags(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CCFLAGS', flag)
        conf.env.append_unique('CXXFLAGS', flag)

def _set_cxxflags(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CXXFLAGS', flag)

def _set_linkflags(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('LINKFLAGS', flag)

def _set_libpath(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('LIBPATH', flag)

def _set_includes(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CPPPATH', flag)

def _set_defines(conf, flags):
    for flag in flags.split():
        conf.env.append_unique('CCDEFINES', flag)
        conf.env.append_unique('CXXDEFINES', flag)




#*******************************************************************************************************
# NINTENDO SWITCH -->
#*******************************************************************************************************

def setup_tools_nx(conf, build_util):

    # until we have the package on S3
    NINTENDO_SDK_ROOT = os.environ['NINTENDO_SDK_ROOT']

    # If the path isn't formatted ok, the gxx:init_cxx()->ccroot.get_cc_version will throw a very silent exception
    #  error: could not determine the compiler version ['C:NintendoSDKNintendoSDK/Compilers/NX/nx/aarch64/bin/clang++', '-dM', '-E', '-']
    NINTENDO_SDK_ROOT = NINTENDO_SDK_ROOT.replace('\\', '/')
    os.environ['NINTENDO_SDK_ROOT'] = NINTENDO_SDK_ROOT
    conf.env['NINTENDO_SDK_ROOT'] = NINTENDO_SDK_ROOT

    bin_folder          = "%s/Compilers/NX/nx/aarch64/bin" % NINTENDO_SDK_ROOT
    os.environ['CC']    = "%s/clang" % bin_folder
    os.environ['CXX']   = "%s/clang++" % bin_folder
    conf.env['CC']      = '%s/clang' % bin_folder
    conf.env['CXX']     = '%s/clang++' % bin_folder
    conf.env['LINK_CXX']= '%s/clang++' % bin_folder
    conf.env['CPP']     = '%s/clang -E' % bin_folder
    conf.env['AR']      = '%s/llvm-ar' % bin_folder
    conf.env['RANLIB']  = '%s/llvm-ranlib' % bin_folder
    conf.env['LD']      = '%s/lld' % bin_folder

    commandline_folder          = "%s/Tools/CommandLineTools" % NINTENDO_SDK_ROOT
    conf.env['AUTHORINGTOOL']   = '%s/AuthoringTool/AuthoringTool.exe' % commandline_folder
    conf.env['MAKEMETA']        = '%s/MakeMeta/MakeMeta.exe' % commandline_folder
    conf.env['MAKENSO']         = '%s/MakeNso/MakeNso.exe' % commandline_folder

    conf.env['TEST_LAUNCH_PATTERN'] = 'RunOnTarget.exe --pattern-failure-exit "tests FAILED" --working-directory . %s %s' # program + args

def get_sdk_root():
    return os.environ['NINTENDO_SDK_ROOT']

def get_lib_paths(buildtype, buildtarget):
    sdk_root = get_sdk_root()
    paths = []
    if buildtype in ['Debug', 'Develop']:
        paths.append("%s/Libraries/%s/Debug" % (sdk_root, buildtarget))
        paths.append("%s/Libraries/%s/Develop" % (sdk_root, buildtarget))
    else:
        paths.append("%s/Libraries/%s/Release" % (sdk_root, buildtarget))
    return paths

def setup_vars_nx(conf, build_util):

    # until we have the package on S3
    NINTENDO_SDK_ROOT = get_sdk_root()

    BUILDTARGET = 'NX-NXFP2-a64'
    BUILDTYPE = "Debug"

    opt_level = Options.options.opt_level
    # For nicer better output (i.e. in CI logs), and still get some performance, let's default to -O1
    if Options.options.with_asan and opt_level != '0':
        opt_level = 1

    CCFLAGS ="-g -O%s" % opt_level
    CCFLAGS+=" -mcpu=cortex-a57+fp+simd+crypto+crc -fno-common -fno-short-enums -ffunction-sections -fdata-sections -fPIC -fdiagnostics-format=msvc"
    CXXFLAGS="-fno-rtti -std=gnu++14 "

    _set_ccflags(conf, CCFLAGS)
    _set_cxxflags(conf, CXXFLAGS)

    DEFINES ="DM_NO_SYSTEM_FUNCTION LUA_NO_SYSTEM LUA_NO_TMPFILE LUA_NO_TMPNAM"
    DEFINES+=" DM_NO_IPV6"
    DEFINES+=" GOOGLE_PROTOBUF_NO_RTTI DDF_EXPOSE_DESCRIPTORS"
    DEFINES+=" JC_TEST_NO_DEATH_TEST JC_TEST_USE_COLORS=1"
    DEFINES+=" NN_SDK_BUILD_%s" % BUILDTYPE.upper()
    _set_defines(conf, DEFINES)

    LINKFLAGS ="-g -O%s" % opt_level
    LINKFLAGS+=" -nostartfiles -Wl,--gc-sections -Wl,--build-id=sha1 -Wl,-init=_init -Wl,-fini=_fini -Wl,-pie -Wl,-z,combreloc"
    LINKFLAGS+=" -Wl,-z,relro -Wl,--enable-new-dtags -Wl,-u,malloc -Wl,-u,calloc -Wl,-u,realloc -Wl,-u,aligned_alloc -Wl,-u,free"
    LINKFLAGS+=" -fdiagnostics-format=msvc"
    LINKFLAGS+=" -Wl,-T C:/Nintendo/SDK/NintendoSDK/Resources/SpecFiles/Application.aarch64.lp64.ldscript"
    LINKFLAGS+=" -Wl,--start-group"
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/rocrt.o" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/nnApplication.o" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_gll.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_gfx.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/libnn_mii_draw.a" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" -Wl,--end-group"
    LINKFLAGS+=" -Wl,--start-group"
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nss" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" -Wl,--end-group"
    LINKFLAGS+=" %s/Libraries/NX-NXFP2-a64/Develop/crtend.o" % NINTENDO_SDK_ROOT
    LINKFLAGS+=" -fuse-ld=lld.exe"
    _set_linkflags(conf, LINKFLAGS)

    LIBPATHS = ' '.join(get_lib_paths(BUILDTYPE, BUILDTARGET))
    _set_libpath(conf, LIBPATHS)

    CPPPATH ="%s/Include" % (NINTENDO_SDK_ROOT,)
    CPPPATH+=" %s/Common/Configs/Targets/%s/Include" % (NINTENDO_SDK_ROOT, BUILDTARGET)
    _set_includes(conf, CPPPATH)

    conf.env.program_PATTERN = '%s.nsp'
    conf.env.bundle_PATTERN = '%s.nspd'

    # Until we support cares
    conf.env['STATICLIB_CARES'] = []
    conf.env['SHLIB_VULKAN']    = ['vulkan', 'opengl']


NX64_MANIFEST="""<?xml version="1.0"?>
<!-- From C:/Nintendo/SDK/NintendoSDK/Resources/SpecFilesApplication.aarch64.lp64.nmeta -->

<NintendoSdkMeta>
    <Core>
        <Name>Application</Name>
        <ApplicationId>0x01004b9000490000</ApplicationId>
        <Is64BitInstruction>True</Is64BitInstruction>
        <ProcessAddressSpace>AddressSpace64Bit</ProcessAddressSpace>
    </Core>

    <Application>
        <Title>
            <Language>AmericanEnglish</Language>
            <Name>%(app_name)s</Name>
            <Publisher>Defold</Publisher>
        </Title>
        <Icon>
            <Language>AmericanEnglish</Language>
            <IconPath>%(app_icon)s</IconPath>
        </Icon>
        <DisplayVersion>1.0.0</DisplayVersion>
        <SupportedLanguage>AmericanEnglish</SupportedLanguage>
        <Rating>
            <Organization>ESRB</Organization>
            <Age>0</Age>
        </Rating>
        <LogoType>LicensedByNintendo</LogoType>
        <ReleaseVersion>0</ReleaseVersion>

        <UserAccountSaveDataSize>0x0000000000100000</UserAccountSaveDataSize>
        <UserAccountSaveDataJournalSize>0x0000000000040000</UserAccountSaveDataJournalSize>
        <CacheStorageSize>0x0000000003F00000</CacheStorageSize>
        <CacheStorageJournalSize>0x0000000000100000</CacheStorageJournalSize>
    </Application>
</NintendoSdkMeta>
"""

# Creates a set of directories (modified from wafadmin/Tools/osx.py)
# returns the last DIR node in the supplied path
def create_bundle_dirs(self, path, dir):
    bld=self.bld
    for token in path.split('/'):
        subdir=dir.get_dir(token)
        if not subdir:
            subdir=dir.__class__(token, dir, Node.DIR)
        bld.rescan(subdir)
        dir=subdir
    return dir

@feature('cprogram', 'cxxprogram')
@before('apply_core')
@after('default_flags')
def switch_modify_linkflags_fn(self):
    if self.env['PLATFORM'] in ['arm64-nx64']:
        waf_dynamo.remove_flag(self.env['LINKFLAGS'], '-Wl,--enable-auto-import', 0)

Task.simple_task_type('switch_nso', '${MAKENSO} ${SRC} ${TGT}', color='YELLOW', shell=True, after='cxx_link cc_link')

Task.simple_task_type('switch_meta', '${MAKEMETA} --desc ${SWITCH_DESC} --meta ${SWITCH_META} -o ${TGT} -d DefaultIs64BitInstruction=True -d DefaultProcessAddressSpace=AddressSpace64Bit',
                                     color='YELLOW', shell=True, after='switch_nso')


def switch_make_bundle(self):
    shutil.copy2(self.input_main.abspath(self.env), self.bundle_main.abspath(self.env))
    shutil.copy2(self.input_npdm.abspath(self.env), self.bundle_npdm.abspath(self.env))
    shutil.copy2(self.input_rtld, self.bundle_rtld.abspath(self.env))
    shutil.copy2(self.input_sdk, self.bundle_sdk.abspath(self.env))

    dir = self.bundle_rtld.parent
    for i, path in enumerate(self.nx_shared_libs):
        dst = self.nx_shared_library_outputs[i]
        print "copy2", path, "->", dst.abspath(self.env)
        shutil.copy2(path, dst.abspath(self.env))

Task.task_type_from_func('switch_make_bundle', func = switch_make_bundle, color = 'blue', after  = 'switch_meta')

Task.simple_task_type('switch_nspd', '${AUTHORINGTOOL} createnspd -o ${NSPD_DIR} --desc ${SWITCH_DESC} --meta ${SWITCH_META} --type Application --program ${CODE_DIR} ${DATA_DIR} --utf8',
                    color='YELLOW', shell=True, after='switch_make_bundle')

Task.simple_task_type('switch_nsp', '${AUTHORINGTOOL} creatensp -o ${TGT} --desc ${SWITCH_DESC} --meta ${SWITCH_META} --type Application --program ${CODE_DIR} ${DATA_DIR} --utf8',
                    color='YELLOW', shell=True, after='switch_nspd')

@feature('cprogram', 'cxxprogram')
@after('apply_link')
def switch_make_app(self):
    for task in self.tasks:
        if task.name in ['cxx_link', 'cc_link']:
            break

    NINTENDO_SDK_ROOT = get_sdk_root()
    BUILDTARGET = 'NX-NXFP2-a64'
    BUILDTYPE = "Debug"

    lib_paths = get_lib_paths(BUILDTYPE, BUILDTARGET)
    nss_files = []
    shared_libs = []
    for name in self.uselib.split():
        key = 'SHLIB_%s' % name
        shlibs = getattr(self.env, key, [])
        for name in shlibs:
            for libpath in lib_paths:
                path = os.path.join(libpath, '%s.nso' % name)
                if os.path.exists(path):
                    nss_files.append(os.path.join(libpath, '%s.nss' % name))
                    shared_libs.append(path)
                    break

    index = self.env['LINKFLAGS'].index('-Wl,--end-group')
    for path in reversed(nss_files):
        self.env['LINKFLAGS'].insert(index, path)

    descfile = os.path.join(self.env.NINTENDO_SDK_ROOT, 'Resources/SpecFiles/Application.desc')
    app_icon = os.path.join(self.env.NINTENDO_SDK_ROOT, 'Resources/SpecFiles/NintendoSDK_Application.bmp')

    nss = task.outputs[0]
    bundle_parent = nss.parent

    exe_name = os.path.splitext(nss.name)[0]

    nso = nss.change_ext('.nso')
    nsotask = self.create_task('switch_nso')
    nsotask.set_inputs(nss)
    nsotask.set_outputs(nso)

    manifest = bundle_parent.exclusive_build_node("%s.nmeta" % exe_name)

    generated_folder = manifest.parent
    if not os.path.exists(generated_folder.abspath(task.env)):
        os.makedirs(generated_folder.abspath(task.env))

    with open(manifest.abspath(task.env), 'wb') as f:
        f.write(NX64_MANIFEST % { 'app_name' : exe_name, 'app_icon' : app_icon })
        self.bld.node_sigs[self.env.variant()][manifest.id] = Utils.h_file(manifest.abspath(task.env))

    npdm = nss.change_ext('.npdm')
    metatask = self.create_task('switch_meta')
    metatask.set_inputs(nso)
    metatask.set_outputs(npdm)
    metatask.env['SWITCH_META'] = manifest.abspath(task.env)
    metatask.env['SWITCH_DESC'] = descfile

    nspd_dir = '%s.nspd' % exe_name
    # temporary folders for code/data that will be copied into the signed bundle
    code_dir = '%s.code' % exe_name
    data_dir = '%s.data' % exe_name


    bundle_npsd = create_bundle_dirs(self, nspd_dir, bundle_parent)

    bundle_nsp = bundle_parent.exclusive_build_node('%s.nsp' % exe_name)

    bundle_main = bundle_parent.exclusive_build_node(code_dir+'/main')
    bundle_npdm = bundle_parent.exclusive_build_node(code_dir+'/main.npdm')
    bundle_rtld = bundle_parent.exclusive_build_node(code_dir+'/rtld')
    bundle_sdk  = bundle_parent.exclusive_build_node(code_dir+'/sdk')

    shared_library_outputs = []
    for i, path in enumerate(shared_libs):
        lib = bundle_parent.exclusive_build_node(code_dir+'/subsdk%d' % i)
        shared_library_outputs.append(lib)

    bundletask = self.create_task('switch_make_bundle')
    bundletask.set_inputs([nso, npdm])
    bundletask.set_outputs([bundle_main, bundle_npdm, bundle_rtld, bundle_sdk] + shared_library_outputs)
    # explicit variables makes it easier to maintain/read
    bundletask.bundle_main = bundle_main
    bundletask.bundle_npdm = bundle_npdm
    bundletask.bundle_rtld = bundle_rtld
    bundletask.bundle_sdk = bundle_sdk
    bundletask.input_main = nso
    bundletask.input_npdm = npdm
    bundletask.input_rtld = os.path.join(self.env.NINTENDO_SDK_ROOT, 'Libraries/NX-NXFP2-a64/Develop/nnrtld.nso')
    bundletask.input_sdk = os.path.join(self.env.NINTENDO_SDK_ROOT, 'Libraries/NX-NXFP2-a64/Develop/nnSdkEn.nso')
    bundletask.nx_shared_libs = shared_libs
    bundletask.nx_shared_library_outputs = shared_library_outputs

    # TODO: Add some mechanic to copy app data to the bundle (e.g. icon)
    authorize_control_dir   = create_bundle_dirs(self, nspd_dir+'/control0.ncd/data', bundle_parent)
    authorize_control       = authorize_control_dir.find_or_declare(['control.nacp'])
    authorize_data_dir      = bundle_parent.exclusive_build_node(data_dir).abspath(task.env)
    if not os.path.exists(authorize_data_dir):
        os.makedirs(authorize_data_dir)

    with open(os.path.join(authorize_data_dir, 'dummy'), 'wb') as f:
        f.write("created to avoid an empty folder which trips up the bundler")

    self.bld.rescan(bundle_main.parent)

    nspdtask = self.create_task('switch_nspd')
    nspdtask.set_inputs([bundle_main, bundle_npdm])
    nspdtask.set_outputs([authorize_control])
    nspdtask.env['SWITCH_META'] = manifest.abspath(task.env)
    nspdtask.env['SWITCH_DESC'] = descfile
    nspdtask.env['NSPD_DIR'] = bundle_npsd.abspath(task.env)
    nspdtask.env['CODE_DIR'] = bundle_main.parent.abspath(task.env)
    nspdtask.env['DATA_DIR'] = authorize_data_dir

    # TODO: Make this variable drive by the app creation task/generator
    ## check if the directory is empty
    #if not os.listdir(nsptask.env['DATA_DIR']):
    #    nsptask.env['DATA_DIR'] = None

    authorize_data_dir  = bundle_parent.exclusive_build_node(data_dir).abspath(task.env)
    if not os.path.exists(authorize_data_dir):
        os.makedirs(authorize_data_dir)

    nsptask = self.create_task('switch_nsp')
    nsptask.set_inputs([bundle_main, bundle_npdm])
    nsptask.set_outputs([bundle_nsp])
    nsptask.env['SWITCH_META'] = manifest.abspath(task.env)
    nsptask.env['SWITCH_DESC'] = descfile
    nsptask.env['CODE_DIR'] = bundle_npsd.exclusive_build_node('program0.ncd/code').abspath(task.env)
    nsptask.env['DATA_DIR'] = bundle_npsd.exclusive_build_node('program0.ncd/data').abspath(task.env)

    task.bundle_output = bundle_nsp.abspath(task.env)



def supports_feature_nx(platform, feature, data):
    # until we've added an implementation of dns.cpp
    if feature in [ 'test_dns',
                    'test_httpclient',
                    'test_configfile',
                    'test_connection_pool',
                    'test_httpserver',
                    'test_ssdp',
                    'test_httpcache',
                    'test_ssdp_internals',
                    'luasocket']:
        return False

    # until we've added an implementation of memprofile.cpp
    if feature in ['test_memprofile']:
        return False
    # until we've figured out a way to test it host<->switch
    if feature in ['test_webserver']:
        return False

    if feature in ['opengl', 'luajit', 'bullet3d']:
        return False
    return True

#*******************************************************************************************************
# <-- NINTENDO SWITCH
#*******************************************************************************************************

# setup any extra options
def set_options(opt):
    pass

# setup the CC et al
def setup_tools(conf, build_util):
    if build_util.get_target_platform() in ['arm64-nx64']:
        _dont_look_for_msvc()
        setup_tools_nx(conf, build_util)

# Modify any variables after the cc+cxx compiler tools have been loaded
def setup_vars(task_gen, build_util):
    if build_util.get_target_platform() in ['arm64-nx64']:
        setup_vars_nx(task_gen, build_util)


def is_platform_private(platform):
    return platform in ['arm64-nx64']


def supports_feature(platform, feature, data):
    """ Can ask if the platform supports this feature.
    Returns True by defalt.
    @param platform
    @param feature string
    @param data table
    """
    if platform in ['arm64-nx64']:
        return supports_feature_nx(platform, feature, data)
    return True
