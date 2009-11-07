import sys, os
import Task, TaskGen
from TaskGen import extension
from waf_gamesys import ProtoCTask

def configure(conf):
    conf.find_file('fontc.py', var='FONTC', mandatory = True)

ProtoCTask('font', 'dmRender.FontDesc', '${DYNAMO_HOME}/share/proto/render_ddf.proto', '.font_pb', '.font', include = '${DYNAMO_HOME}/share/proto')

Task.simple_task_type('ttf', 'python ${FONTC} -s ${size} -o ${TGT} ${SRC}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.ttf')
def ttf_file(self, node):
    obj_ext = '.imagefont'
    font = self.create_task('ttf')
    self.env['size'] = str(self.size)
    font.set_inputs(node)
    out = node.change_ext(obj_ext)
    font.set_outputs(out)
