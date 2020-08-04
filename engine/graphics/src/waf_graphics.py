# Copyright 2020 The Defold Foundation
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
# 
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
# 
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

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
