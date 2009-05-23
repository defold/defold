import os, sys, subprocess
import Build, Options, Utils
from Configure import conf

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
    if not Options.commands['build']:
        return

    for t in  Build.bld.all_task_gen:
        if hasattr(t, 'uselib') and str(t.uselib).find("GTEST") != -1:
            output = t.path
            filename = os.path.join(output.abspath(t.env), t.target)
            if valgrind and sys.platform == 'linux2':
                filename = "valgrind --leak-check=full --error-exitcode=1 %s" % filename
            proc = subprocess.Popen(filename, shell = True)
            ret = proc.wait()
            if ret != 0:
                print("test failed %s" %(t.target) )
                sys.exit(ret)

def configure(conf):
    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        conf.fatal("DYNAMO_HOME not set")

    dynamo_ext = os.path.join(dynamo_home, "ext")

    if sys.platform == "darwin":
        platform = "darwin"
    elif sys.platform == "linux2":
        platform = "linux"
    elif sys.platform == "win32":
        platform = "win32"
    else:
        conf.fatal("Unable to determine platform")

    if platform == "linux" or platform == "darwin":
        conf.env['CXXFLAGS']='-g -D__STDC_LIMIT_MACROS -DDDF_EXPOSE_DESCRIPTORS'
    else:
        conf.env['CXXFLAGS']=['/Z7', '/MT', '/D__STDC_LIMIT_MACROS', '/DDDF_EXPOSE_DESCRIPTORS']
        conf.env.append_value('LINKFLAGS', '/DEBUG')

    conf.env.append_value('CPPPATH', os.path.join(dynamo_ext, "include"))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include"))
    conf.env.append_value('CPPPATH', os.path.join(dynamo_home, "include", platform))

    conf.env.append_value('LIBPATH', os.path.join(dynamo_ext, "lib", platform))
    conf.env.append_value('LIBPATH', os.path.join(dynamo_home, "lib"))

    