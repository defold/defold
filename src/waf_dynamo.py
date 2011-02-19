import os, sys, subprocess
import Build, Options, Utils, Task
from Configure import conf
from TaskGen import feature, after, before

def embed_build(task):
    symbol = task.inputs[0].name.upper().replace('.', '_')
    in_file = open(task.inputs[0].bldpath(task.env), 'rb')
    out_file = open(task.outputs[0].bldpath(task.env), 'wb')

    cpp_str = """
#include <stdint.h>
char %s[] =
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

def detect(conf):
    conf.find_program('valgrind', var='VALGRIND', mandatory = False)

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")

    conf.env['DYNAMO_HOME'] = dynamo_home

    dynamo_ext = os.path.join(dynamo_home, "ext")

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

    conf.env['PLATFORM'] = platform
    conf.env['BUILD_PLATFORM'] = build_platform

    if platform == "linux" or platform == "darwin":
        conf.env.append_value('CXXFLAGS', ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall', '-m32'])
        conf.env.append_value('LINKFLAGS', ['-m32'])
    elif platform == "arm6-darwin":
        ARM_DARWIN_ROOT='/Developer/Platforms/iPhoneOS.platform/Developer'
        conf.env['CC'] = '%s/usr/bin/gcc-4.2' % (ARM_DARWIN_ROOT)
        conf.env['CXX'] = '%s/usr/bin/g++-4.2' % (ARM_DARWIN_ROOT)
        conf.env['CPP'] = '%s/usr/bin/cpp-4.2' % (ARM_DARWIN_ROOT)
        conf.env['AR'] = '%s/usr/bin/ar' % (ARM_DARWIN_ROOT)
        conf.env['RANLIB'] = '%s/usr/bin/ranlib' % (ARM_DARWIN_ROOT)
        conf.env['LD'] = '%s/usr/bin/ld' % (ARM_DARWIN_ROOT)

        conf.env.append_value('CXXFLAGS', ['-g', '-D__STDC_LIMIT_MACROS', '-DDDF_EXPOSE_DESCRIPTORS', '-Wall', '-arch', 'armv6', '-isysroot', '%s/SDKs/iPhoneOS3.2.sdk' % (ARM_DARWIN_ROOT)])
        conf.env.append_value('LINKFLAGS', [ '-arch', 'armv6', '-isysroot', '%s/SDKs/iPhoneOS3.2.sdk' % (ARM_DARWIN_ROOT)])
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS']
        conf.env.append_value('LINKFLAGS', '/DEBUG')
        conf.env.append_value('LINKFLAGS', 'WS2_32.LIB')

    conf.env['CCFLAGS'] = conf.env['CXXFLAGS']

    conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include"))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include", platform))

    conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))
    conf.env.append_value('LIBPATH', os.path.join(dynamo_home, "lib"))

    if platform == "linux":
        conf.env['LIB_PLATFORM_THREAD'] = 'pthread'
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    elif 'darwin' in platform:
        conf.env['LIB_PLATFORM_THREAD'] = ''
        conf.env['LIB_PLATFORM_SOCKET'] = ''
    else:
        conf.env['LIB_PLATFORM_THREAD'] = ''
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
    opt.add_option('--eclipse', action='store_true', default=False, dest='eclipse', help='print eclipse friendly command-line')
    opt.add_option('--platform', default='', dest='platform', help='target platform, eg arm6-darwin')
    opt.add_option('--skip-tests', action='store_true', default=False, dest='skip_tests', help='skip unit tests')
