import sys, os
import Task, TaskGen
from TaskGen import extension
from waf_content import proto_compile_task

def configure(conf):
    conf.find_file('fontc.py', var='FONTC', mandatory = True)

def transform_font(msg):
    msg.Font = msg.Font.replace('.ttf', '.imagefontc')
    msg.Material = msg.Material.replace('.material', '.materialc')
    return msg

def transform_material(msg):
    msg.VertexProgram = msg.VertexProgram.replace('.vp', '.arbvp')
    msg.FragmentProgram = msg.FragmentProgram.replace('.fp', '.arbfp')
    return msg

proto_compile_task('font', 'render.render_ddf_pb2', 'render_ddf_pb2.FontDesc', '.font', '.fontc', transform_font)
proto_compile_task('material', 'render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc', '.material', '.materialc', transform_material)

#Task.simple_task_type('ttf', 'python ${FONTC} -s ${size} -o ${TGT} ${SRC}',
Task.simple_task_type('imagefont', '${JAVA} -classpath ${CLASSPATH} com.dynamo.render.Fontc ${SRC} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=False)

@extension('.font')
def font_file(self, node):
    classpath = [self.env['DYNAMO_HOME'] + '/ext/share/java/protobuf-java-2.3.0.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/ddf.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/render.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/fontc.jar']
    imagefont = self.create_task('imagefont')
    imagefont.env['CLASSPATH'] = os.pathsep.join(classpath)
    imagefont.set_inputs(node)
    obj_ext = '.imagefontc'
    out = node.change_ext(obj_ext)
    imagefont.set_outputs(out)
    font = self.create_task('font')
    font.set_inputs(node)
    obj_ext = '.fontc'
    out = node.change_ext(obj_ext)
    font.set_outputs(out)
