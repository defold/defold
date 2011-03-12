import Task, TaskGen, Utils, re, os
from TaskGen import extension
from waf_content import proto_compile_task

def configure(conf):
    conf.find_file('meshc.py', var='MESHC', mandatory = True)

def transform_texture_name(name):
    name = name.replace('.png', '.texturec')
    name = name.replace('.jpg', '.texturec')
    return name

def transform_collection(msg):
    for i in msg.Instances:
        i.Prototype = i.Prototype.replace('.go', '.goc')
    for c in msg.CollectionInstances:
        c.Collection = c.Collection.replace('.collection', '.collectionc')
    return msg

def transform_collectionproxy(msg):
    msg.Collection = msg.Collection.replace('.collection', '.collectionc')
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
        c.Resource = c.Resource.replace('.collectionproxy', '.collectionproxyc')
        c.Resource = c.Resource.replace('.collisionobject', '.collisionobjectc')
        c.Resource = c.Resource.replace('.emitter', '.emitterc')
        c.Resource = c.Resource.replace('.gui', '.guic')
        c.Resource = c.Resource.replace('.model', '.modelc')
        c.Resource = c.Resource.replace('.script', '.scriptc')
        c.Resource = c.Resource.replace('.wav', '.wavc')
        c.Resource = c.Resource.replace('.spawnpoint', '.spawnpointc')
        c.Resource = c.Resource.replace('.light', '.lightc')
        c.Resource = c.Resource.replace('.sprite', '.spritec')
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
    for t in msg.Textures:
        t.Texture = transform_texture_name(t.Texture)
    return msg

def transform_spawnpoint(msg):
    msg.Prototype = msg.Prototype.replace('.go', '.goc')
    return msg

def transform_render(msg):
    msg.Script = msg.Script.replace('.render_script', '.render_scriptc')
    for m in msg.Materials:
        m.Material = m.Material.replace('.material', '.materialc')
    return msg

def transform_sprite(msg):
    msg.Texture = transform_texture_name(msg.Texture)
    return msg

def write_embedded(task):
    try:
        import google.protobuf.text_format
        import gameobject_ddf_pb2
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(task.inputs[0].srcpath(task.env), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        msg = transform_gameobject(msg)

        for i, c in enumerate(msg.EmbeddedComponents):
            with open(task.outputs[i].bldpath(task.env), 'wb') as out_f:
                out_f.write(msg.SerializeToString())

        return 0
    except google.protobuf.text_format.ParseError,e:
        print >>sys.stderr, '%s:%s' % (task.inputs[0].srcpath(task.env), str(e))
        return 1

task = Task.task_type_from_func('write_embedded',
                                func    = write_embedded,
                                color   = 'RED',
                                after='proto_gen_py',
                                before='cc cxx')

def compile_go(task):
    try:
        import google.protobuf.text_format
        import gameobject_ddf_pb2
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(task.inputs[0].srcpath(task.env), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        for i, c in enumerate(msg.EmbeddedComponents):
            with open(task.outputs[i+1].bldpath(task.env), 'wb') as out_f:
                out_f.write(c.Data)

            desc = msg.Components.add()
            rel_path_dir = os.path.relpath(task.inputs[0].abspath(), task.generator.content_root)
            rel_path_dir = os.path.dirname(rel_path_dir)
            desc.Resource = rel_path_dir + '/' + task.outputs[i+1].name

        msg = transform_gameobject(msg)
        while len(msg.EmbeddedComponents) > 0:
            del(msg.EmbeddedComponents[0])

        with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
            out_f.write(msg.SerializeToString())

        return 0
    except google.protobuf.text_format.ParseError,e:
        print >>sys.stderr, '%s:%s' % (task.inputs[0].srcpath(task.env), str(e))
        return 1

task = Task.task_type_from_func('gameobject',
                                func    = compile_go,
                                color   = 'RED',
                                after='proto_gen_py',
                                before='cc cxx')

@extension('.go')
def gofile(self, node):
    import google.protobuf.text_format
    import gameobject_ddf_pb2
    msg = gameobject_ddf_pb2.PrototypeDesc()
    with open(node.abspath(self.env), 'rb') as in_f:
        google.protobuf.text_format.Merge(in_f.read(), msg)

    task = self.create_task('gameobject')
    task.set_inputs(node)

    embed_output_nodes = []
    for i, c in enumerate(msg.EmbeddedComponents):
        name = '%s_generated_%d.%s' % (node.name.split('.')[0], i, c.Type)
        embed_node = node.parent.exclusive_build_node(name)
        embed_output_nodes.append(embed_node)

        sub_task = self.create_task(c.Type)
        sub_task.set_inputs(embed_node)
        out = embed_node.change_ext('.' + c.Type + 'c')
        sub_task.set_outputs(out)
        sub_task.set_run_after(task)
    out = node.change_ext('.goc')
    task.set_outputs([out] + embed_output_nodes)

proto_compile_task('collection', 'gameobject_ddf_pb2', 'CollectionDesc', '.collection', '.collectionc', transform_collection)
proto_compile_task('collectionproxy', 'gamesys_ddf_pb2', 'CollectionProxyDesc', '.collectionproxy', '.collectionproxyc', transform_collectionproxy)
proto_compile_task('emitter', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter', '.emitter', '.emitterc', transform_emitter)
proto_compile_task('model', 'model_ddf_pb2', 'ModelDesc', '.model', '.modelc', transform_model)
proto_compile_task('convexshape',  'physics_ddf_pb2', 'ConvexShape', '.convexshape', '.convexshapec')
proto_compile_task('collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc', '.collisionobject', '.collisionobjectc', transform_collisionobject)
proto_compile_task('gui',  'gui_ddf_pb2', 'SceneDesc', '.gui', '.guic', transform_gui)
proto_compile_task('camera', 'camera_ddf_pb2', 'CameraDesc', '.camera', '.camerac')
proto_compile_task('input_binding', 'input_ddf_pb2', 'InputBinding', '.input_binding', '.input_bindingc')
proto_compile_task('gamepads', 'input_ddf_pb2', 'GamepadMaps', '.gamepads', '.gamepadsc')
proto_compile_task('spawnpoint', 'gamesys_ddf_pb2', 'SpawnPointDesc', '.spawnpoint', '.spawnpointc', transform_spawnpoint)
proto_compile_task('light', 'gamesys_ddf_pb2', 'LightDesc', '.light', '.lightc')
proto_compile_task('render', 'render.render_ddf_pb2', 'render_ddf_pb2.RenderPrototypeDesc', '.render', '.renderc', transform_render)
proto_compile_task('sprite', 'sprite_ddf_pb2', 'SpriteDesc', '.sprite', '.spritec', transform_sprite)

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
