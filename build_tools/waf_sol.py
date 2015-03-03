import os
import Task
from TaskGen import extension, feature, before
from Configure import conf
import Utils

Task.simple_task_type('sol', '${JAVA} -jar ${SOL_COMPILER} ${SOL_OPTIONS} ${SRC} -o ${TGT[0].abspath(env)}',
                      color='YELLOW',
                      shell=True)

@extension('.sol')
def sol_hook(self,node):
    task = self.create_task('sol')
    #obj_ext='_%d.o'%self.idx
    obj_ext='.o'
    task.inputs = [node]
    task.outputs = [node.change_ext(obj_ext)]
    self.compiled_tasks.append(task)

def detect(conf):
    conf.check_tool('java')
    # TODO: Hardcoded path
    conf.find_file('compiler.jar', var='SOL_COMPILER', mandatory = True, path_list = [os.path.abspath("../../../sol/compiler/build/libs/")])
    # TODO: Hardcoded path
    conf.env.append_value('LIBPATH', os.path.abspath("../../../sol/build/default"))
    # TODO: Hardcoded path
    conf.env.append_value('LIBPATH', os.path.abspath("../../../sol/compiler/build/default"))
    conf.env['STATICLIB_SOL_RUNTIME'] = 'sol-runtime'
    conf.env['STATICLIB_SOL'] = 'sol'

def set_options(opt):
    pass

@feature('sol')
@before('apply_core')
def apply_sol(self):
    Utils.def_attrs(self, sol_options = "--verbose --keep-tmp --no-emit-transitive -O3")
    self.env['SOL_OPTIONS'] = self.sol_options
