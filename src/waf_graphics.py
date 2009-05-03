import sys, os
import Task, TaskGen
from TaskGen import extension

Task.simple_task_type('vertexprogram', 'cgc -quiet -profile arbvp1 -o ${TGT} ${SRC} ',
                      color='PINK', 
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.vp')
def vp_file(self, node):
    obj_ext = '.arbvp'
    program = self.create_task('vertexprogram')
    program.set_inputs(node)
    out = node.change_ext(obj_ext)
    program.set_outputs(out)

Task.simple_task_type('fragmentprogram', 'cgc -quiet -profile arbfp1 -o ${TGT} ${SRC} ',
                      color='PINK', 
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.fp')
def fp_file(self, node):
    obj_ext = '.arbfp'
    program = self.create_task('fragmentprogram')
    program.set_inputs(node)
    out = node.change_ext(obj_ext)
    program.set_outputs(out)
