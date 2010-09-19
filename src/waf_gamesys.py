import Task, TaskGen, Utils, re, os
from TaskGen import extension
import waf_ddf, waf_content

def ProtoCTask(name, message_type, proto_file, input_ext, output_ext, append_to_all = False, include = '../proto'):
    task = Task.simple_task_type(name, 'protoc --encode=%s -I ${DYNAMO_HOME}/share/proto -I %s -I ${DYNAMO_HOME}/ext/include %s < ${SRC} > ${TGT}' % (message_type, include, proto_file),
                                 color='PINK',
                                 after='proto_gen_py',
                                 before='cc cxx',
                                 shell=True)

    def scan(self):
        n = self.generator.path.find_resource(Utils.subst_vars(proto_file, self.env))
        if n:
            deps = waf_ddf.do_scan(self, [n], include)[0]
            deps += [n]
            return deps, []
        else:
            return [], []

    task.scan = scan

    @extension(input_ext)
    def xfile(self, node):
        obj_ext = output_ext
        t = self.create_task(name)
        t.set_inputs(node)
        out = node.change_ext(obj_ext)
        if append_to_all:
            self.allnodes.append(out)
        t.set_outputs(out)

def transform_gameobject(msg):
    for c in msg.Components:
        if (c.Resource.endswith('.camera')):
            c.Resource = c.Resource.replace('.camera', '.camerac')
        c.Resource = c.Resource.replace('.model', '.modelc')
        c.Resource = c.Resource.replace('.script', '.scriptc')
    return msg

def transform_model(msg):
    msg.Material = msg.Material.replace('.material', '.materialc')
    msg.Texture0 = msg.Texture0.replace('.png', '.texture')
    msg.Texture0 = msg.Texture0.replace('.tga', '.texture')
    return msg

def transform_emitter(msg):
    msg.Material = msg.Material.replace('.material', '.materialc')
    msg.Texture.Name = msg.Texture.Name.replace('.png', '.texture')
    msg.Texture.Name = msg.Texture.Name.replace('.tga', '.texture')
    return msg

Task.simple_task_type('gameobjectdesc', 'protoc --encode=dmGameObject.PrototypeDesc -I ${DYNAMO_HOME}/share/proto -I ${DYNAMO_HOME}/ext/include ${DYNAMO_HOME}/share/proto/gameobject_ddf.proto < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

waf_content.proto_compile_task('collection', 'gameobject_ddf_pb2', 'CollectionDesc', '.collection', '.collectionc')
waf_content.proto_compile_task('material', 'render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc', '.material', '.materialc')
waf_content.proto_compile_task('emitter', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter', '.emitter', '.emitterc', transform_emitter)
waf_content.proto_compile_task('model', 'render.model_ddf_pb2', 'model_ddf_pb2.ModelDesc', '.model', '.modelc', transform_model)
waf_content.proto_compile_task('gameobject',  'gameobject_ddf_pb2', 'PrototypeDesc', '.go', '.goc', transform_gameobject)
waf_content.proto_compile_task('convexshape',  'physics_ddf_pb2', 'ConvexShape', '.convexshape_pb', '.convexshape')
waf_content.proto_compile_task('collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc', '.collisionobject_pb', '.collisionobject')
waf_content.proto_compile_task('gui',  'gui_ddf_pb2', 'SceneDesc', '.gui', '.guic')

@extension('.go_pb')
def gameobjectdesc_file(self, node):
    obj_ext = '.go'
    task = self.create_task('gameobjectdesc')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)


Task.simple_task_type('luascript', 'cat < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.script')
def testresourcecont_file(self, node):
    obj_ext = '.scriptc'
    task = self.create_task('luascript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)
