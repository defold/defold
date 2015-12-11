import os
import Task
from TaskGen import extension, feature, before
from Configure import conf
import Utils

Task.simple_task_type('sol', '${JAVA} -jar ${SOL_COMPILER} --source-path `dirname ${SRC}` ${SOL_OPTIONS} `basename ${SRC}` -o ${TGT[0].abspath(env)}',
                      color='YELLOW',
                      shell=True)

@extension('.sol')
def sol_hook(self,node):
    task = self.create_task('sol')
    obj_ext='.o'
    task.inputs = [node]
    task.outputs = [node.change_ext(obj_ext)]
    self.compiled_tasks.append(task)

def detect(conf):
    conf.check_tool('java')
    conf.find_file('solc.jar', var='SOL_COMPILER', mandatory = True, path_list = [os.path.abspath(conf.env["DYNAMO_HOME"] + "/ext/share/java")])
    conf.env['STATICLIB_SOL_RUNTIME'] = 'sol-runtime'
    conf.env['STATICLIB_SOL'] = 'sol'

def set_options(opt):
    pass

@feature('sol')
@before('apply_core')
def apply_sol(self):
    env = self.env
    app = env.append_unique

    includes = []
    if hasattr(self, "sol_includes"):
        includes.extend(Utils.to_list(self.sol_includes))

    include_flags = ''
    for i in includes:
        if os.path.isabs(i):
            include_flags = include_flags + ' --source-path ' + p
        else:
            p_bld = self.path.find_dir(i).bldpath(self.env)
            p_src = self.path.find_dir(i).srcpath(self.env)
            include_flags = include_flags + ' --source-path ' + p_bld + ' --source-path ' + p_src

    Utils.def_attrs(self, sol_options = "--no-emit-transitive --emit obj -O3 --platform " + self.env['PLATFORM'] + " " + include_flags + " --source-path " + self.env["DYNAMO_HOME"] + "/lib/sol")
    self.env['SOL_OPTIONS'] = self.sol_options
