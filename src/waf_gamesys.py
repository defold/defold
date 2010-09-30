import Task, TaskGen, Utils, re, os
from TaskGen import extension
from waf_content import proto_compile_task

def transform_gameobject(msg):
    for c in msg.Components:
        if (c.Resource.endswith('.camera')):
            c.Resource = c.Resource.replace('.camera', '.camerac')
        c.Resource = c.Resource.replace('.model', '.modelc')
        c.Resource = c.Resource.replace('.script', '.scriptc')
        c.Resource = c.Resource.replace('.emitter', '.emitterc')
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

proto_compile_task('collection', 'gameobject_ddf_pb2', 'CollectionDesc', '.collection', '.collectionc')
proto_compile_task('material', 'render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc', '.material', '.materialc')
proto_compile_task('emitter', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter', '.emitter', '.emitterc', transform_emitter)
proto_compile_task('model', 'render.model_ddf_pb2', 'model_ddf_pb2.ModelDesc', '.model', '.modelc', transform_model)
proto_compile_task('gameobject',  'gameobject_ddf_pb2', 'PrototypeDesc', '.go', '.goc', transform_gameobject)
proto_compile_task('convexshape',  'physics_ddf_pb2', 'ConvexShape', '.convexshape_pb', '.convexshape')
proto_compile_task('collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc', '.collisionobject_pb', '.collisionobject')
proto_compile_task('gui',  'gui_ddf_pb2', 'SceneDesc', '.gui', '.guic')
proto_compile_task('camera', 'camera_ddf_pb2', 'CameraDesc', '.camera', '.camerac')
proto_compile_task('input_binding', 'input_ddf_pb2', 'InputBinding', '.input_binding', '.input_bindingc')
proto_compile_task('gamepads', 'input_ddf_pb2', 'GamepadMaps', '.gamepads', '.gamepadsc')

TaskGen.declare_chain('project', 'cat < ${SRC} > ${TGT}', ext_in='.project', ext_out='.projectc', reentrant = False)

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

Task.simple_task_type('wav', 'cat < ${SRC} > ${TGT}',
                      color='PINK',
                      shell=True)

@extension('.wav')
def testresourcecont_file(self, node):
    obj_ext = '.wavc'
    task = self.create_task('wav')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

Task.simple_task_type('mesh', 'python ../tools/modelc.py ${SRC} -o ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.dae')
def dae_file(self, node):
    obj_ext = '.mesh'
    mesh = self.create_task('mesh')
    mesh.set_inputs(node)
    out = node.change_ext(obj_ext)
    mesh.set_outputs(out)


Task.simple_task_type('gui_script', 'cat < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.gui_script')
def testresourcecont_file(self, node):
    obj_ext = '.gui_scriptc'
    task = self.create_task('gui_script')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)
