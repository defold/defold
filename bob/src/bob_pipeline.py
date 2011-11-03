# This file will be moved to gamesys or similar

from __future__ import with_statement
import os
from os.path import basename, splitext
from bob import add_task, task, change_ext, execute_command, find_cmd, register, make_proto, create_task
from bob import log_error
import shutil

def make_copy_task(name, ext_out):
    def copy_file(prj, tsk):
        shutil.copy(tsk['inputs'][0], tsk['outputs'][0])

    def copy_factory(prj, input):
        return add_task(prj, task(function = copy_file,
                                  name = name,
                                  inputs = [input],
                                  outputs = [change_ext(prj, input, ext_out)]))
    return copy_factory

def texture_file(prj, tsk):
    execute_command(prj, tsk, 'python ${texc} ${inputs[0]} -o ${outputs[0]}')

def texture_factory(prj, input):
    return add_task(prj, task(function = texture_file,
                              name = 'texture',
                              inputs = [input],
                              outputs = [change_ext(prj, input, '.texturec')]))

def go_file(prj, tsk):
    code = 0

    err = ''
    try:
        import google.protobuf.text_format
        import gameobject_ddf_pb2
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(tsk['inputs'][0], 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        for i, c in enumerate(msg.embedded_components):
            with open(tsk['outputs'][i+1], 'wb') as out_f:
                out_f.write(c.data)

            desc = msg.components.add()
            if c.id == '':
                raise Exception('Message is missing required field: id')
            desc.id = c.id

            # NOTE: This is a bit budget. We "calculate" relative
            # path by chopping the length of bld_dir
            # relpath is not available in python 2.5 (jython)
            # As long as we have "sensible" paths this will continue to work
            bld_dir_len = len(prj['bld_dir'])
            desc.component = tsk['outputs'][i+1][bld_dir_len:]

        msg = transform_gameobject(msg)
        while len(msg.embedded_components) > 0:
            del(msg.embedded_components[0])

        with open(tsk['outputs'][0], 'wb') as out_f:
            out_f.write(msg.SerializeToString())

    except Exception, e:
        err = '%s: %s' % (task.inputs[0].srcpath(task.env), str(e))
        code = 5

    info = tsk['info']
    info['code'] = code
    info['stdout'] = ''
    info['stderr'] = err

def go_factory(prj, input):

    try:
        import google.protobuf.text_format
        import gameobject_ddf_pb2
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(input, 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        embed_outputs = []
        embed_tsks = []
        for i, c in enumerate(msg.embedded_components):
            name, ext = splitext(input)
            embed_output = change_ext(prj, '%s_generated_%d' % (name, i), '.' + c.type)
            embed_outputs.append(embed_output)
            sub_tsk = create_task(prj, embed_output)
            embed_tsks.append(sub_tsk)
    except Exception, e:
        log_error('%s: %s' % (input, str(e)))
        return 1

    tsk = task(function = go_file,
               name = 'go',
               inputs = [input],
               outputs = [change_ext(prj, input, '.goc')] + embed_outputs)

    for sb in embed_tsks:
        # Use for to tracking the originating file, i.e. the .go-file
        sb['product_of'] = tsk

    add_task(prj, tsk)
    return tsk

def fontmap_file(prj, tsk):
    execute_command(prj, tsk, 'java -classpath ${classpath} com.dynamo.render.Fontc ${inputs[0]} ${content_root} ${outputs[0]}')

def fontmap_factory(prj, input):
    return add_task(prj, task(function = fontmap_file,
                              name = 'fontmap',
                              inputs = [input],
                              outputs = [change_ext(prj, input, '.fontc')]))

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

def transform_material(msg):
    msg.vertex_program = msg.vertex_program.replace('.vp', '.vpc')
    msg.fragment_program = msg.fragment_program.replace('.fp', '.fpc')
    return msg

def conf(prj):

    # textures
    find_cmd(prj, 'texc.py', var='texc')
    register(prj, '.png .tga', texture_factory)

    # fonts
    register(prj, '.font', fontmap_factory)

    # proto formats
    register(prj, '.collection', make_proto('gameobject_ddf_pb2', 'CollectionDesc', transform_collection))
    register(prj, '.collectionproxy', make_proto('gamesys_ddf_pb2', 'CollectionProxyDesc', transform_collectionproxy))
    register(prj, '.emitter', make_proto('particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter', transform_emitter))
    register(prj, '.model', make_proto('model_ddf_pb2', 'ModelDesc', transform_model))
    register(prj, '.convexshape',  make_proto('physics_ddf_pb2', 'ConvexShape'))
    register(prj, '.collisionobject',  make_proto('physics_ddf_pb2', 'CollisionObjectDesc', transform_collisionobject))
    register(prj, '.gui',  make_proto('gui_ddf_pb2', 'SceneDesc', transform_gui))
    register(prj, '.camera', make_proto('camera_ddf_pb2', 'CameraDesc'))
    register(prj, '.input_binding', make_proto('input_ddf_pb2', 'InputBinding'))
    register(prj, '.gamepads', make_proto('input_ddf_pb2', 'GamepadMaps'))
    register(prj, '.spawnpoint', make_proto('gamesys_ddf_pb2', 'SpawnPointDesc', transform_spawnpoint))
    register(prj, '.light', make_proto('gamesys_ddf_pb2', 'LightDesc'))
    register(prj, '.render', make_proto('render.render_ddf_pb2', 'render_ddf_pb2.RenderPrototypeDesc', transform_render))
    register(prj, '.sprite', make_proto('sprite_ddf_pb2', 'SpriteDesc', transform_sprite))
    register(prj, '.tilegrid', make_proto('tile_ddf_pb2', 'TileGrid', transform_tilegrid))
    register(prj, '.material', make_proto('render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc', transform_material))

    # gameobject
    register(prj, '.go', go_factory)

    register(prj, '.project', make_copy_task('project', '.projectc'))
    register(prj, '.script', make_copy_task('script', '.scriptc'))
    register(prj, '.gui_script', make_copy_task('gui_script', '.gui_scriptc'))
    register(prj, '.wav', make_copy_task('wav', '.wavc'))

    dynamo_home = os.getenv('DYNAMO_HOME')
    if not dynamo_home:
        raise Exception('DYNAMO_HOME not set')

    prj['dynamo_home'] = dynamo_home

    classpath = [ dynamo_home + x for x in ['/ext/share/java/protobuf-java-2.3.0.jar',
                                            '/share/java/ddf.jar',
                                            '/share/java/render.jar',
                                            '/share/java/fontc.jar']]
    prj['classpath'] = os.pathsep.join(classpath)
    prj['content_root'] = '.'
