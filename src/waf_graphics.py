import sys, os
import Task, TaskGen
from TaskGen import extension

def configure(conf):
    conf.find_file('ddsc.py', var='DDSC', mandatory = True)

# TODO: Target filename i .fixmejpg.
# Waf doesn't seems to like when the source and target have identical extensions...
if sys.platform == "win32":
    NULL = ">NUL"
else:
    NULL = ">/dev/null"

Task.simple_task_type('dds', 'nvcompress -bc3 -fast ${SRC} ${TGT} %s' % NULL,
                      color='PINK', 
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.jpg')
def jpg_file(self, node):
    obj_ext = '.dds'
    dds = self.create_task('dds')
    dds.set_inputs(node)
    out = node.change_ext(obj_ext)
    dds.set_outputs(out)
    self.allnodes.append(out)

@extension('.png')
def png_file(self, node):
    obj_ext = '.dds'
    dds = self.create_task('dds')
    dds.set_inputs(node)
    out = node.change_ext(obj_ext)
    dds.set_outputs(out)
    self.allnodes.append(out)

Task.simple_task_type('texture', 'python ${DDSC} ${SRC} -o ${TGT} %s' % NULL,
                      color='PINK', 
                      after='proto_gen_py dds',
                      before='cc cxx',
                      shell=True)

@extension('.dds')
def dds_file(self, node):
    obj_ext = '.texturec'
    texture = self.create_task('texture')
    texture.set_inputs(node)
    out = node.change_ext(obj_ext)
    texture.set_outputs(out)
    # Hackish... configure?
    #self.env["DDSC"] = os.path.join(os.environ["DYNAMO_HOME"], "bin", "ddsc.py")

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
