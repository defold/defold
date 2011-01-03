import Task, TaskGen, Utils, re, os
from TaskGen import extension
from waf_content import proto_compile_task

def configure(conf):
    conf.find_file('meshc.py', var='MESHC', mandatory = True)

def transform_texture_name(name):
    name = name.replace('.png', '.texturec')
    name = name.replace('.tga', '.texturec')
    return name

def transform_collection(msg):
    for i in msg.Instances:
        i.Prototype = i.Prototype.replace('.go', '.goc')
    for c in msg.CollectionInstances:
        c.Collection = c.Collection.replace('.collection', '.collectionc')
    return msg

def transform_collisionobject(msg):
    msg.CollisionShape = msg.CollisionShape.replace('.convexshape', '.convexshapec')
    return msg

def transform_emitter(msg):
    msg.Material = msg.Material.replace('.material', '.materialc')
    msg.Texture.Name = transform_texture_name(msg.Texture.Name)
    return msg

def transform_gameobject(msg):
    for c in msg.Components:
        c.Resource = c.Resource.replace('.camera', '.camerac')
        c.Resource = c.Resource.replace('.collisionobject', '.collisionobjectc')
        c.Resource = c.Resource.replace('.emitter', '.emitterc')
        c.Resource = c.Resource.replace('.gui', '.guic')
        c.Resource = c.Resource.replace('.model', '.modelc')
        c.Resource = c.Resource.replace('.script', '.scriptc')
        c.Resource = c.Resource.replace('.wav', '.wavc')
        c.Resource = c.Resource.replace('.spawnpoint', '.spawnpointc')
        c.Resource = c.Resource.replace('.light', '.lightc')
    return msg

def transform_model(msg):
    msg.Mesh = msg.Mesh.replace('.dae', '.meshc')
    msg.Material = msg.Material.replace('.material', '.materialc')
    for i,n in enumerate(msg.Textures):
        msg.Textures[i] = transform_texture_name(msg.Textures[i])
    return msg

def transform_gui(msg):
    msg.Script = msg.Script.replace('.gui_script', '.gui_scriptc')
    for f in msg.Fonts:
        f.Font = f.Font.replace('.font', '.fontc')
    for i,n in enumerate(msg.Textures):
        msg.Textures[i] = transform_texture_name(n)
    return msg

def transform_spawnpoint(msg):
    msg.Prototype = msg.Prototype.replace('.go', '.goc')
    return msg

proto_compile_task('collection', 'gameobject_ddf_pb2', 'CollectionDesc', '.collection', '.collectionc', transform_collection)
proto_compile_task('emitter', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter', '.emitter', '.emitterc', transform_emitter)
proto_compile_task('model', 'model_ddf_pb2', 'ModelDesc', '.model', '.modelc', transform_model)
proto_compile_task('gameobject',  'gameobject_ddf_pb2', 'PrototypeDesc', '.go', '.goc', transform_gameobject)
proto_compile_task('convexshape',  'physics_ddf_pb2', 'ConvexShape', '.convexshape', '.convexshapec')
proto_compile_task('collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc', '.collisionobject', '.collisionobjectc', transform_collisionobject)
proto_compile_task('gui',  'gui_ddf_pb2', 'SceneDesc', '.gui', '.guic', transform_gui)
proto_compile_task('camera', 'camera_ddf_pb2', 'CameraDesc', '.camera', '.camerac')
proto_compile_task('input_binding', 'input_ddf_pb2', 'InputBinding', '.input_binding', '.input_bindingc')
proto_compile_task('gamepads', 'input_ddf_pb2', 'GamepadMaps', '.gamepads', '.gamepadsc')
proto_compile_task('spawnpoint', 'gamesys_ddf_pb2', 'SpawnPointDesc', '.spawnpoint', '.spawnpointc', transform_spawnpoint)
proto_compile_task('light', 'gamesys_ddf_pb2', 'LightDesc', '.light', '.lightc')

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

Task.simple_task_type('render_script', 'cat < ${SRC} > ${TGT}',
                      color='PINK',
                      before='cc cxx',
                      shell=True)

@extension('.render_script')
def testresourcecont_file(self, node):
    obj_ext = '.render_scriptc'
    task = self.create_task('render_script')
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

Task.simple_task_type('mesh', 'python ${MESHC} ${SRC} -o ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=True)

@extension('.dae')
def dae_file(self, node):
    obj_ext = '.meshc'
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
