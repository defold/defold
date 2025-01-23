# Copyright 2020-2025 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
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
import waflib.Task, waflib.TaskGen
from waflib.TaskGen import extension
from waf_content import proto_compile_task

def configure(conf):
    pass

waflib.Task.task_factory('material', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.pipeline.MaterialBuilder ${SRC} ${TGT} ${SHADER_NAME} ${CONTENT_ROOT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=False)

waflib.Task.task_factory('material_shaderbuilder', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.pipeline.ShaderProgramBuilder ${FP} ${VP} ${TGT} ${PLATFORM}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=False)

@extension('.material')
def material_file(self, node):
    import google.protobuf.text_format
    import render.material_ddf_pb2
    import dlib

    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar']
    material = self.create_task('material')

    msg = render.material_ddf_pb2.MaterialDesc()
    with open(node.srcpath(), 'rb') as in_f:
        google.protobuf.text_format.Merge(in_f.read(), msg)

    shader_name = None

    # When building builtin content for the engine, we need a fixed name for the output shader
    # I guess this can be done in some magic waf way, but we'll do it this way for now
    if hasattr(material.generator, "remap_material_output"):
        shader_name = material.generator.remap_material_output(msg)

    if shader_name == None:
        shader_hash = dlib.dmHashBuffer64(msg.vertex_program + msg.fragment_program)
        shader_name = 'shader_%d%s' % (shader_hash, '.spc')

    material.env['CLASSPATH']    = os.pathsep.join(classpath)
    material.env['CONTENT_ROOT'] = material.generator.content_root
    material.env['SHADER_NAME']  = shader_name

    material.set_inputs(node)
    material_node = node.change_ext('.materialc')
    material.set_outputs(material_node)

    shader = self.create_task('material_shaderbuilder')
    shader.env['CLASSPATH'] = os.pathsep.join(classpath)
    shader.env['FP'] = material.generator.content_root + msg.fragment_program
    shader.env['VP'] = material.generator.content_root + msg.vertex_program

    shader.set_inputs(material_node)

    shader_node = node.parent.get_bld().make_node(shader_name)
    shader.set_outputs(shader_node)

waflib.Task.task_factory('fontmap', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.font.Fontc ${SRC} ${CONTENT_ROOT} ${TGT}',
                         color='PINK',
                         after='proto_gen_py',
                         before='c cxx',
                         shell=False)

@extension('.font')
def font_file(self, node):
    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar']
    fontmap = self.create_task('fontmap')

    fontmap.env['CLASSPATH'] = os.pathsep.join(classpath)
    fontmap.env['CONTENT_ROOT'] = fontmap.generator.content_root
    fontmap.set_inputs(node)
    obj_ext = '.fontc'
    out = node.change_ext(obj_ext)
    fontmap.set_outputs(out)
