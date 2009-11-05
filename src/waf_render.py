import sys, os
import Task, TaskGen
from TaskGen import extension

def configure(conf):
    conf.find_file('fontc.py', var='FONTC', mandatory = True)

Task.simple_task_type('ttf', 'python ${FONTC} -s ${size} --vp ${vertex_program} --fp ${fragment_program} -o ${TGT} ${SRC}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.ttf')
def ttf_file(self, node):
    obj_ext = '.font'
    font = self.create_task('ttf')
    self.env['size'] = str(self.size)
    self.env['vertex_program'] = str(self.vertex_program)
    self.env['fragment_program'] = str(self.fragment_program)
    font.set_inputs(node)
    out = node.change_ext(obj_ext)
    font.set_outputs(out)
