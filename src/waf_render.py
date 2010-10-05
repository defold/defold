import sys, os
import Task, TaskGen
from TaskGen import extension
from waf_content import proto_compile_task

def configure(conf):
    conf.find_file('fontc.py', var='FONTC', mandatory = True)

def transform_font(msg):
    msg.Font = msg.Font.replace('.ttf', '.imagefontc')
    return msg

proto_compile_task('font', 'render.render_ddf_pb2', 'render_ddf_pb2.FontDesc', '.font', '.fontc', transform_font)

#Task.simple_task_type('ttf', 'python ${FONTC} -s ${size} -o ${TGT} ${SRC}',
Task.simple_task_type('ttf', '${JAVA} -classpath ${CLASSPATH} com.dynamo.render.Fontc ${SRC} ${size} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=False)

@extension('.ttf')
def ttf_file(self, node):

    classpath = [self.env['DYNAMO_HOME'] + '/ext/share/java/protobuf-java-2.3.0.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/ddf.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/render.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/fontc.jar']
    obj_ext = '.imagefontc'
    font = self.create_task('ttf')
    font.env['CLASSPATH'] = os.pathsep.join(classpath)
    self.env['size'] = str(self.size)
    font.set_inputs(node)
    out_node = node.parent.find_or_declare(self.imagefont)
    font.set_outputs(out_node)
