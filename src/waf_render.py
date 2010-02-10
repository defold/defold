import sys, os
import Task, TaskGen
from TaskGen import extension
from waf_gamesys import ProtoCTask

def configure(conf):
    conf.find_file('fontc.py', var='FONTC', mandatory = True)

ProtoCTask('font', 'dmRender.FontDesc', '${DYNAMO_HOME}/share/proto/render_ddf.proto', '.font_pb', '.font', include = '${DYNAMO_HOME}/share/proto')

#Task.simple_task_type('ttf', 'python ${FONTC} -s ${size} -o ${TGT} ${SRC}',
Task.simple_task_type('ttf', '${JAVA} -classpath ${CLASSPATH} com.dynamo.render.Fontc ${SRC} ${size} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.ttf')
def ttf_file(self, node):

    classpath = [self.env['DYNAMO_HOME'] + '/ext/share/java/protobuf-java-2.0.3.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/ddf.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/render.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/fontc.jar']
    obj_ext = '.imagefont'
    font = self.create_task('ttf')
    font.env['CLASSPATH'] = os.pathsep.join(classpath)
    self.env['size'] = str(self.size)
    font.set_inputs(node)
    out_node = node.parent.find_or_declare(self.imagefont)
    font.set_outputs(out_node)
