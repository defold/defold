import Task, TaskGen, Utils, re, os, sys
from TaskGen import extension
from waf_content import proto_compile_task
from threading import Lock

stderr_lock = Lock()

def configure(conf):
    conf.find_file('meshc.py', var='MESHC', mandatory = True)

def transform_texture_name(name):
    name = name.replace('.png', '.texturec')
    name = name.replace('.jpg', '.texturec')
    return name

def transform_collection(msg):
    for i in msg.instances:
        i.prototype = i.prototype.replace('.go', '.goc')
    for c in msg.collection_instances:
        c.collection = c.collection.replace('.collection', '.collectionc')
    return msg

def transform_collectionproxy(msg):
    msg.collection = msg.collection.replace('.collection', '.collectionc')
    return msg

def transform_collisionobject(msg):
    msg.collision_shape = msg.collision_shape.replace('.convexshape', '.convexshapec')
    msg.collision_shape = msg.collision_shape.replace('.tilegrid', '.tilegridc')
    return msg

def transform_emitter(msg):
    msg.material = msg.material.replace('.material', '.materialc')
    msg.texture.name = transform_texture_name(msg.texture.name)
    return msg

def transform_gameobject(msg):
    for c in msg.components:
        c.component = c.component.replace('.camera', '.camerac')
        c.component = c.component.replace('.collectionproxy', '.collectionproxyc')
        c.component = c.component.replace('.collisionobject', '.collisionobjectc')
        c.component = c.component.replace('.emitter', '.emitterc')
        c.component = c.component.replace('.gui', '.guic')
        c.component = c.component.replace('.model', '.modelc')
        c.component = c.component.replace('.script', '.scriptc')
        c.component = c.component.replace('.wav', '.wavc')
        c.component = c.component.replace('.spawnpoint', '.spawnpointc')
        c.component = c.component.replace('.light', '.lightc')
        c.component = c.component.replace('.sprite', '.spritec')
        c.component = c.component.replace('.tileset', '.tilesetc')
        c.component = c.component.replace('.tilegrid', '.tilegridc')
    return msg

def transform_model(msg):
    msg.mesh = msg.mesh.replace('.dae', '.meshc')
    msg.material = msg.material.replace('.material', '.materialc')
    for i,n in enumerate(msg.textures):
        msg.textures[i] = transform_texture_name(msg.textures[i])
    return msg

def transform_gui(msg):
    msg.script = msg.script.replace('.gui_script', '.gui_scriptc')
    font_names = set()
    texture_names = set()
    for f in msg.fonts:
        font_names.add(f.name)
        f.font = f.font.replace('.font', '.fontc')
    for t in msg.textures:
        texture_names.add(t.name)
        t.texture = transform_texture_name(t.texture)
    for n in msg.nodes:
        if n.texture:
            if not n.texture in texture_names:
                raise Exception('Texture "%s" not declared in gui-file' % (n.texture))
        if n.font:
            if not n.font in font_names:
                raise Exception('Font "%s" not declared in gui-file' % (n.font))
    return msg

def transform_spawnpoint(msg):
    msg.prototype = msg.prototype.replace('.go', '.goc')
    return msg

def transform_render(msg):
    msg.script = msg.script.replace('.render_script', '.render_scriptc')
    for m in msg.materials:
        m.material = m.material.replace('.material', '.materialc')
    return msg

def transform_sprite(msg):
    msg.texture = transform_texture_name(msg.texture)
    return msg

def transform_tilegrid(msg):
    msg.tile_set = msg.tile_set + 'c'
    return msg

def write_embedded(task):
    try:
        import google.protobuf.text_format
        import gameobject_ddf_pb2
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(task.inputs[0].srcpath(task.env), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        msg = transform_gameobject(msg)

        for i, c in enumerate(msg.embedded_components):
            with open(task.outputs[i].bldpath(task.env), 'wb') as out_f:
                out_f.write(msg.SerializeToString())

        return 0
    except (google.protobuf.text_format.ParseError, google.protobuf.message.EncodeError) as e:
        stderr_lock.acquire()
        try:
            print >>sys.stderr, '%s: %s' % (task.inputs[0].srcpath(task.env), str(e))
        finally:
            stderr_lock.release()
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

        for i, c in enumerate(msg.embedded_components):
            with open(task.outputs[i+1].bldpath(task.env), 'wb') as out_f:
                out_f.write(c.data)

            desc = msg.components.add()
            rel_path_dir = os.path.relpath(task.inputs[0].abspath(), task.generator.content_root)
            rel_path_dir = os.path.dirname(rel_path_dir)
            if c.id == '':
                raise Exception('Message is missing required field: id')
            desc.id = c.id
            desc.component = '/' + rel_path_dir + '/' + task.outputs[i+1].name

        msg = transform_gameobject(msg)
        while len(msg.embedded_components) > 0:
            del(msg.embedded_components[0])

        with open(task.outputs[0].bldpath(task.env), 'wb') as out_f:
            out_f.write(msg.SerializeToString())

        return 0
    except (google.protobuf.text_format.ParseError, google.protobuf.message.EncodeError, Exception) as e:
        stderr_lock.acquire()
        try:
            print >>sys.stderr, '%s: %s' % (task.inputs[0].srcpath(task.env), str(e))
        finally:
            stderr_lock.release()
        return 1

task = Task.task_type_from_func('gameobject',
                                func    = compile_go,
                                color   = 'RED',
                                after='proto_gen_py',
                                before='cc cxx')

@extension('.go')
def gofile(self, node):
    try:
        import google.protobuf.text_format
        import gameobject_ddf_pb2
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(node.abspath(self.env), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        task = self.create_task('gameobject')
        task.set_inputs(node)

        embed_output_nodes = []
        for i, c in enumerate(msg.embedded_components):
            name = '%s_generated_%d.%s' % (node.name.split('.')[0], i, c.type)
            embed_node = node.parent.exclusive_build_node(name)
            embed_output_nodes.append(embed_node)

            sub_task = self.create_task(c.type)
            sub_task.set_inputs(embed_node)
            out = embed_node.change_ext('.' + c.type + 'c')
            sub_task.set_outputs(out)
            sub_task.set_run_after(task)
        out = node.change_ext('.goc')
        task.set_outputs([out] + embed_output_nodes)
    except (google.protobuf.text_format.ParseError, google.protobuf.message.EncodeError, Exception) as e:
        stderr_lock.acquire()
        try:
            print >>sys.stderr, '%s: %s' % (node.srcpath(self.env), str(e))
        finally:
            stderr_lock.release()
        return 1

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
proto_compile_task('tilegrid', 'tile_ddf_pb2', 'TileGrid', '.tilegrid', '.tilegridc', transform_tilegrid)

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

Task.simple_task_type('tileset', '${JAVA} -classpath ${CLASSPATH} com.dynamo.tile.TileSetc ${SRC} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='cc cxx',
                      shell=False)

@extension('.tileset')
def tileset_file(self, node):
    classpath = [self.env['DYNAMO_HOME'] + '/ext/share/java/protobuf-java-2.3.0.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/ddf.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/gamesys.jar',
                 self.env['DYNAMO_HOME'] + '/share/java/tile.jar',
                 self.env['DYNAMO_HOME'] + '/ext/share/java/vecmath.jar',
                 # NOTE: Only needed when running within gamesys-project.
                 # Should be fixed somehow... in configure perhaps?
                 'default/src/java',
                 'default/src/gamesys/gamesys.jar']
    tileset = self.create_task('tileset')
    tileset.env['CLASSPATH'] = os.pathsep.join(classpath)
    tileset.set_inputs(node)
    obj_ext = '.tilesetc'
    out = node.change_ext(obj_ext)
    tileset.set_outputs(out)
