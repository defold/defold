import sys, os
import Task, TaskGen
from TaskGen import extension
from waf_content import proto_compile_task

def configure(conf):
    pass

def transform_material(task, msg):
    msg.vertex_program = msg.vertex_program.replace('.vp', '.vpc')
    msg.fragment_program = msg.fragment_program.replace('.fp', '.fpc')
    return msg

proto_compile_task('material', 'render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc', '.material', '.materialc', transform_material)

Task.simple_task_type('fontmap', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.font.Fontc ${SRC} ${CONTENT_ROOT} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
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
    out_tex = node.change_ext("_tex0.texturec")
    fontmap.set_outputs(out_tex)
