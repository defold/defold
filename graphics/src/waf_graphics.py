import sys, os
import Task, TaskGen
from TaskGen import extension

def configure(conf):
    conf.find_file('texc.py', var='TEXC', mandatory = True)
    conf.find_file('glslvc', var='GLSLVC', mandatory = True)
    conf.find_file('glslfc', var='GLSLFC', mandatory = True)

Task.simple_task_type('texture', 'python ${TEXC} ${SRC} -o ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.png .jpg')
def png_file(self, node):
    texture = self.create_task('texture')
    texture.set_inputs(node)
    out = node.change_ext('.texturec')
    texture.set_outputs(out)

Task.simple_task_type('vertexprogram', '${GLSLVC} ${SRC} ${TGT}',
                      color='PINK',
                      shell=True)

@extension('.vp')
def vp_file(self, node):
    obj_ext = '.vpc'
    program = self.create_task('vertexprogram')
    program.set_inputs(node)
    out = node.change_ext(obj_ext)
    program.set_outputs(out)

Task.simple_task_type('fragmentprogram', '${GLSLFC} ${SRC} ${TGT}',
                      color='PINK',
                      shell=True)

@extension('.fp')
def fp_file(self, node):
    obj_ext = '.fpc'
    program = self.create_task('fragmentprogram')
    program.set_inputs(node)
    out = node.change_ext(obj_ext)
    program.set_outputs(out)
