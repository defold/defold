# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import waflib.Task, waflib.TaskGen, waflib.Utils, re, os, sys
from waflib.TaskGen import extension
from waf_content import proto_compile_task
from threading import Lock
from waf_dynamo import new_copy_task

from google.protobuf import text_format

stderr_lock = Lock()

def configure(conf):
    pass

def transform_properties(properties, out_properties):
    for property in properties:
        entry = None
        if property.type == gameobject_ddf_pb2.PROPERTY_TYPE_NUMBER:
            entry = out_properties.number_entries.add()
            entry.index = len(out_properties.float_values)
            out_properties.float_values.append(float(property.value))
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_HASH:
            entry = out_properties.hash_entries.add()
            entry.index = len(out_properties.hash_values)
            out_properties.hash_values.append(dlib.dmHashBuffer64(property.value))
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_URL:
            entry = out_properties.url_entries.add()
            entry.index = len(out_properties.string_values)
            out_properties.string_values.append(property.value)
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_VECTOR3:
            entry = out_properties.vector3_entries.add()
            entry.index = len(out_properties.float_values)
            out_properties.float_values.extend([float(v.strip()) for v in property.value.split(',')])
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_VECTOR4:
            entry = out_properties.vector4_entries.add()
            entry.index = len(out_properties.float_values)
            out_properties.float_values.extend([float(v.strip()) for v in property.value.split(',')])
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_QUAT:
            entry = out_properties.quat_entries.add()
            entry.index = len(out_properties.float_values)
            out_properties.float_values.extend([float(v.strip()) for v in property.value.split(',')])
        else:
            raise Exception("Invalid type")
        entry.key = property.id
        entry.id = dlib.dmHashBuffer64(property.id)

def transform_texture_name(task, name):
    name = name.replace('.png', '.texturec')
    name = name.replace('.jpg', '.texturec')
    return name

def transform_tilesource_name(name):
    name = name.replace('.tileset', '.t.texturesetc')
    name = name.replace('.tilesource', '.t.texturesetc')
    name = name.replace('.atlas', '.t.texturesetc')
    return name

def transform_collection(task, msg):
    for i in msg.instances:
        i.prototype = i.prototype.replace('.go', '.goc')
        i.id = "/" + i.id
        for comp_props in i.component_properties:
            transform_properties(comp_props.properties, comp_props.property_decls)
    for c in msg.collection_instances:
        c.collection = c.collection.replace('.collection', '.collectionc')
    return msg

def transform_collectionproxy(task, msg):
    msg.collection = msg.collection.replace('.collection', '.collectionc')
    return msg

def transform_collisionobject(task, msg):
    import physics_ddf_pb2
    import google.protobuf.text_format
    import ddf.ddf_math_pb2
    import dlib
    if msg.type != physics_ddf_pb2.COLLISION_OBJECT_TYPE_DYNAMIC:
        msg.mass = 0

    # Merge convex shape resource with collision object
    # NOTE: Special case for tilegrid resources. They are left as is
    if msg.collision_shape and not (msg.collision_shape.endswith('.tilegrid') or msg.collision_shape.endswith('.tilemap')):
        p = os.path.join(task.generator.content_root, msg.collision_shape[1:])
        convex_msg = physics_ddf_pb2.ConvexShape()
        with open(p, 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), convex_msg)
            shape = msg.embedded_collision_shape.shapes.add()
            shape.shape_type = convex_msg.shape_type
            shape.position.x = shape.position.y = shape.position.z = 0
            shape.rotation.x = shape.rotation.y = shape.rotation.z = 0
            shape.rotation.w = 1
            shape.index = len(msg.embedded_collision_shape.data)
            shape.count = len(convex_msg.data)

            for x in convex_msg.data:
                msg.embedded_collision_shape.data.append(x)

        msg.collision_shape = ''

    for x in msg.embedded_collision_shape.shapes:
        x.id_hash = dlib.dmHashBuffer64(x.id)

    msg.collision_shape = msg.collision_shape.replace('.convexshape', '.convexshapec')
    msg.collision_shape = msg.collision_shape.replace('.tilemap', '.tilemapc')
    msg.collision_shape = msg.collision_shape.replace('.tilegrid', '.tilemapc')
    return msg

def transform_particlefx(task, msg):
    for emitter in msg.emitters:
        emitter.material = emitter.material.replace('.material', '.materialc')
        emitter.tile_source = transform_tilesource_name(emitter.tile_source)
    return msg

def transform_gameobject(task, msg):
    for c in msg.components:
        c.component = c.component.replace('.camera', '.camerac')
        c.component = c.component.replace('.collectionproxy', '.collectionproxyc')
        c.component = c.component.replace('.collisionobject', '.collisionobjectc')
        c.component = c.component.replace('.particlefx', '.particlefxc')
        c.component = c.component.replace('.gui', '.guic')
        c.component = c.component.replace('.model', '.modelc')
        c.component = c.component.replace('.animationset', '.animationsetc')
        c.component = c.component.replace('.script', '.scriptc')
        c.component = c.component.replace('.sound', '.soundc')
        c.component = c.component.replace('.factory', '.factoryc')
        c.component = c.component.replace('.collectionfactory', '.collectionfactoryc')
        c.component = c.component.replace('.label', '.labelc')
        c.component = c.component.replace('.light', '.lightc')
        c.component = c.component.replace('.sprite', '.spritec')
        c.component = c.component.replace('.tileset', '.t.texturesetc')
        c.component = c.component.replace('.tilesource', '.t.texturesetc')
        c.component = c.component.replace('.tilemap', '.tilemapc')
        c.component = c.component.replace('.tilegrid', '.tilemapc')

        transform_properties(c.properties, c.property_decls)
    return msg

def compile_model(task):

    def _replace_model_ext(orig, newext):
        if 'dae' in orig:
            return orig.replace(".dae", newext)
        else:
            return orig.replace(".gltf", newext)

    try:
        import google.protobuf.text_format
        import model_ddf_pb2
        import rig.rig_ddf_pb2

        msg = model_ddf_pb2.ModelDesc()
        with open(task.inputs[0].srcpath(), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        msg_out = rig.rig_ddf_pb2.RigScene()
        msg_out.mesh_set = "/" + _replace_model_ext(msg.mesh, ".meshsetc")
        msg_out.skeleton = "/" + _replace_model_ext(msg.mesh, ".skeletonc")
        msg_out.animation_set = "/" + _replace_model_ext(msg.animations, ".animationsetc")
        with open(task.outputs[1].abspath(), 'wb') as out_f:
            out_f.write(msg_out.SerializeToString())

        msg_out = model_ddf_pb2.Model()
        msg_out.rig_scene = "/" + os.path.relpath(task.outputs[1].bldpath(), task.generator.content_root)

        if msg.material or msg.textures:
            material = model_ddf_pb2.Material()
            material.name = "unknown"
            material.material = msg.material.replace(".material", ".materialc")

            for i,n in enumerate(msg.textures):
                if not n:
                    continue
                texture = model_ddf_pb2.Texture()
                texture.sampler = ""
                texture.texture = transform_texture_name(task, msg.textures[i])
                material.textures.append(texture)

            msg_out.materials.append(material)

        for i,n in enumerate(msg.materials):
            material = msg.materials[i]
            material.material = material.material.replace(".material", ".materialc")
            for i,texture in enumerate(material.textures):
                texture.texture = transform_texture_name(task, texture.texture)
            msg_out.materials.append(material)

        with open(task.outputs[0].abspath(), 'wb') as out_f:
            out_f.write(msg_out.SerializeToString())

        return 0
    except (google.protobuf.text_format.ParseError,) as e:
        print ('%s:%s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
        return 1

waflib.Task.task_factory('model',
                         func    = compile_model,
                         color   = 'PINK')

@extension('.model')
def model_file(self, node):
    obj_ext = '.modelc'
    rig_ext = '.rigscenec'
    task = self.create_task('model')
    task.set_inputs(node)
    out_model    = node.change_ext(obj_ext)
    out_rigscene = node.change_ext(rig_ext)
    task.set_outputs([out_model, out_rigscene])

waflib.Task.task_factory('shaderbuilder', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.pipeline.ShaderProgramBuilder ${SRC} ${TGT} ${PLATFORM} ${CONTENT_ROOT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=False)

@extension('.vp', '.fp', '.cp')
def vertexprogram_file(self, node):
    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar'] + self.env['PLATFORM_SHADER_COMPILER_PLUGIN_JAR']
    shader = self.create_task('shaderbuilder')
    shader.env['CLASSPATH'] = os.pathsep.join(classpath)
    shader.env['CONTENT_ROOT'] = "."
    shader.set_inputs(node)
    _, ext = os.path.splitext(node.abspath())
    obj_ext = ext + ".spc"
    out = node.change_ext(obj_ext)
    shader.set_outputs(out)

def compile_animationset(task):
    try:
        import google.protobuf.text_format
        import rig.rig_ddf_pb2
        import dlib
        msg = rig.rig_ddf_pb2.AnimationSetDesc()
        with open(task.inputs[0].srcpath(), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)
        msg_animset = rig.rig_ddf_pb2.AnimationSet()
        msg_riganim = msg_animset.animations.add()
        msg_riganim.id = dlib.dmHashBuffer64(os.path.splitext(os.path.basename(msg.animations[0].animation))[0])
        msg_riganim.duration = 0.0
        msg_riganim.sample_rate = 30.0
        with open(task.outputs[0].abspath(), 'wb') as out_f:
            out_f.write(msg_animset.SerializeToString())
        return 0
    except (google.protobuf.text_format.ParseError,) as e:
        print ('%s:%s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
        return 1

waflib.Task.task_factory('animationset',
                         func    = compile_animationset,
                         color   = 'PINK')

@extension('.animationset')
def animationset_file(self, node):
    out_ext = '.animationsetc'
    task = self.create_task('animationset')
    task.set_inputs(node)
    out_animationset  = node.change_ext(out_ext)
    task.set_outputs([out_animationset])

def transform_gui(task, msg):
    msg.script = msg.script.replace('.gui_script', '.gui_scriptc')
    font_names = set()
    texture_names = set()
    material_names = set()

    msg.material = msg.material.replace(".material", ".materialc")
    for f in msg.fonts:
        font_names.add(f.name)
        f.font = f.font.replace('.font', '.fontc')
    for f in msg.materials:
        material_names.add(f.name)
        f.material = f.material.replace('.material', '.materialc')
    for t in msg.textures:
        texture_names.add(t.name)
        t.texture = transform_tilesource_name(transform_texture_name(task, t.texture))
    for n in msg.nodes:
        if n.texture:
            if not n.texture in texture_names:
                atlas_part = n.texture[:n.texture.index("/")]
                if not atlas_part in texture_names:
                    raise Exception('Texture "%s" not declared in gui-file' % (n.texture))
        if n.material:
            if not n.material in material_names:
                raise Exception('Material "%s" not declared in gui-file' % (n.material))
        if n.font:
            if not n.font in font_names:
                raise Exception('Font "%s" not declared in gui-file' % (n.font))
    return msg

def transform_factory(task, msg):
    msg.prototype = msg.prototype.replace('.go', '.goc')
    return msg

def transform_collectionfactory(task, msg):
    msg.prototype = msg.prototype.replace('.collection', '.collectionc')
    return msg

def transform_render(task, msg):
    import render.render_ddf_pb2
    msg.script = msg.script.replace('.render_script', '.render_scriptc')

    # Migrate from the old format to the new format for render prototypes
    for m in msg.materials:
        entry = render.render_ddf_pb2.RenderPrototypeDesc.RenderResourceDesc()
        entry.name = m.name
        entry.path = m.material
        msg.render_resources.append(entry)

    for r in msg.render_resources:
        r.path = r.path.replace('.material', '.materialc')
        r.path = r.path.replace('.render_target', '.render_targetc')

    msg.materials.clear()
    return msg

def transform_render_target(task, msg):
    msg.prototype = msg.prototype.replace('.render_target', '.render_targetc')
    return msg

def transform_sprite(task, msg):
    import sprite_ddf_pb2

    msg.material = msg.material.replace('.material', '.materialc')
    if msg.tile_set:
        st = sprite_ddf_pb2.SpriteTexture()
        st.sampler = ""
        st.texture = msg.tile_set
        msg.textures.append(st)
        msg.tile_set = ""

    for st in msg.textures:
        st.texture = transform_tilesource_name(st.texture)
    return msg

def transform_tilegrid(task, msg):
    msg.tile_set = transform_tilesource_name(msg.tile_set)
    msg.material = msg.material.replace('.material', '.materialc')
    return msg

def transform_sound(task, msg):
    msg.sound = msg.sound.replace('.wav', '.wavc')
    msg.sound = msg.sound.replace('.ogg', '.oggc')
    return msg

def transform_rig_scene(task, msg):
    msg.skeleton = msg.skeleton.replace('.skeleton', '.skeletonc')
    msg.animation_set = msg.animation_set.replace('.animationset', '.animationsetc')
    msg.mesh_set = msg.mesh_set.replace('.meshset', '.meshsetc')
    msg.texture_set = msg.texture_set.replace('.atlas', '.a.texturesetc')
    return msg

def transform_label(task, msg):
    msg.font = msg.font.replace('.font', '.fontc')
    msg.material = msg.material.replace('.material', '.materialc')
    return msg

def transform_mesh(task, msg):
    msg.vertices = msg.vertices.replace('.buffer', '.bufferc')
    msg.vertices = msg.vertices.replace('.prebuilt_bufferc', '.prebuilt_bufferc')
    msg.material = msg.material.replace('.material', '.materialc')
    return msg

def write_embedded(task):
    try:
        import gameobject_ddf_pb2
        import google.protobuf.text_format
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(task.inputs[0].srcpath(), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        msg = transform_gameobject(task, msg)

        for i, c in enumerate(msg.embedded_components):
            with open(task.outputs[i].abspath(), 'wb') as out_f:
                out_f.write(msg.SerializeToString())

        return 0
    except (google.protobuf.text_format.ParseError, google.protobuf.message.EncodeError) as e:
        stderr_lock.acquire()
        try:
            print ('%s: %s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
        finally:
            stderr_lock.release()
        return 1

task = waflib.Task.task_factory('write_embedded',
                                func    = write_embedded,
                                color   = 'RED',
                                after='proto_gen_py',
                                before='c cxx')

def compile_go(task):
    try:
        import gameobject_ddf_pb2
        import google.protobuf.text_format
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(task.inputs[0].srcpath(), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        for i, c in enumerate(msg.embedded_components):
            with open(task.outputs[i+1].abspath(), 'wb') as out_f:
                out_f.write(c.data.encode('ascii'))
            desc = msg.components.add()
            rel_path_dir = os.path.relpath(task.inputs[0].abspath(), task.generator.content_root)
            rel_path_dir = os.path.dirname(rel_path_dir)
            if c.id == '':
                raise Exception('Message is missing required field: id')
            desc.id = c.id
            desc.position.x = c.position.x
            desc.position.y = c.position.y
            desc.position.z = c.position.z
            desc.rotation.x = c.rotation.x
            desc.rotation.y = c.rotation.y
            desc.rotation.z = c.rotation.z
            desc.rotation.w = c.rotation.w

            desc.component = '/' + rel_path_dir + '/' + task.outputs[i+1].name

        msg = transform_gameobject(task, msg)

        while len(msg.embedded_components) > 0:
            del(msg.embedded_components[0])

        with open(task.outputs[0].abspath(), 'wb') as out_f:
            out_f.write(msg.SerializeToString())

        return 0
    except (google.protobuf.text_format.ParseError, google.protobuf.message.EncodeError, Exception) as e:
        stderr_lock.acquire()
        try:
            print ('%s: %s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
        finally:
            stderr_lock.release()
        return 1

task = waflib.Task.task_factory('gameobject',
                                func    = compile_go,
                                color   = 'RED',
                                after='proto_gen_py',
                                before='c cxx')

GENERATOR_ID = 0

@extension('.go')
def gofile(self, node):
    global GENERATOR_ID
    try:
        import gameobject_ddf_pb2
        import google.protobuf.text_format
        msg = gameobject_ddf_pb2.PrototypeDesc()
        with open(node.abspath(), 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)

        task = self.create_task('gameobject')
        task.set_inputs(node)

        embed_output_nodes = []
        for i, c in enumerate(msg.embedded_components):
            GENERATOR_ID = GENERATOR_ID + 1

            name = '%s_generated_%d.%s' % (node.name.split('.')[0], GENERATOR_ID, c.type)

            embed_node = node.parent.get_bld().make_node(name)
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
            print ('%s: %s' % (node.srcpath(), str(e)), file=sys.stderr)
        finally:
            stderr_lock.release()
        return 1

proto_compile_task('collection', 'gameobject_ddf_pb2', 'CollectionDesc', '.collection', '.collectionc', transform_collection)
proto_compile_task('collectionproxy', 'gamesys_ddf_pb2', 'CollectionProxyDesc', '.collectionproxy', '.collectionproxyc', transform_collectionproxy)
proto_compile_task('particlefx', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.ParticleFX', '.particlefx', '.particlefxc', transform_particlefx)
proto_compile_task('convexshape',  'physics_ddf_pb2', 'ConvexShape', '.convexshape', '.convexshapec')
proto_compile_task('collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc', '.collisionobject', '.collisionobjectc', transform_collisionobject)
proto_compile_task('gui',  'gui_ddf_pb2', 'SceneDesc', '.gui', '.guic', transform_gui)
proto_compile_task('camera', 'camera_ddf_pb2', 'CameraDesc', '.camera', '.camerac')
proto_compile_task('input_binding', 'input_ddf_pb2', 'InputBinding', '.input_binding', '.input_bindingc')
proto_compile_task('gamepads', 'input_ddf_pb2', 'GamepadMaps', '.gamepads', '.gamepadsc')
proto_compile_task('script', 'gamesys_ddf_pb2', 'ScriptDesc', '.script', '.scriptc', transform_factory)
proto_compile_task('factory', 'gamesys_ddf_pb2', 'FactoryDesc', '.factory', '.factoryc', transform_factory)
proto_compile_task('collectionfactory', 'gamesys_ddf_pb2', 'CollectionFactoryDesc', '.collectionfactory', '.collectionfactoryc', transform_collectionfactory)
proto_compile_task('light', 'gamesys_ddf_pb2', 'LightDesc', '.light', '.lightc')
proto_compile_task('label', 'label_ddf_pb2', 'LabelDesc', '.label', '.labelc', transform_label)
proto_compile_task('render', 'render.render_ddf_pb2', 'render_ddf_pb2.RenderPrototypeDesc', '.render', '.renderc', transform_render)
proto_compile_task('render_target', 'render.render_target_ddf_pb2', 'render_target_ddf_pb2.RenderTargetDesc', '.render_target', '.render_targetc')
proto_compile_task('sprite', 'sprite_ddf_pb2', 'SpriteDesc', '.sprite', '.spritec', transform_sprite)
proto_compile_task('tilegrid', 'tile_ddf_pb2', 'TileGrid', '.tilegrid', '.tilemapc', transform_tilegrid)
proto_compile_task('tilemap', 'tile_ddf_pb2', 'TileGrid', '.tilemap', '.tilemapc', transform_tilegrid)
proto_compile_task('sound', 'sound_ddf_pb2', 'SoundDesc', '.sound', '.soundc', transform_sound)
proto_compile_task('mesh', 'mesh_ddf_pb2', 'MeshDesc', '.mesh', '.meshc', transform_mesh)
proto_compile_task('display_profiles', 'render.render_ddf_pb2', 'render_ddf_pb2.DisplayProfiles', '.display_profiles', '.display_profilesc')

new_copy_task('project', '.project', '.projectc')
new_copy_task('glsl', '.glsl', '.glslc')

# Copy prebuilt spine scenes
new_copy_task('copy prebuilt animationsetc', '.prebuilt_animationsetc', '.animationsetc')
new_copy_task('copy prebuilt meshsetc', '.prebuilt_meshsetc', '.meshsetc')
new_copy_task('copy prebuilt rigscenec', '.prebuilt_rigscenec', '.rigscenec')
new_copy_task('copy prebuilt skeletonc', '.prebuilt_skeletonc', '.skeletonc')
new_copy_task('copy prebuilt texturec', '.prebuilt_texturec', '.texturec')
new_copy_task('copy prebuilt texturesetc', '.prebuilt_texturesetc', '.texturesetc')
new_copy_task('copy prebuilt modelc', '.prebuilt_modelc', '.modelc')

# Copy prebuilt mesh and buffer resources
new_copy_task('copy prebuilt meshc', '.prebuilt_meshc', '.meshc')
new_copy_task('copy prebuilt bufferc', '.prebuilt_bufferc', '.bufferc')

# Copy raw data
new_copy_task('copy raw data', '.raw', '.rawc')

from io import StringIO
def strip_single_lua_comments(str):
    str = str.decode('utf-8').replace("\r", "");
    sb = StringIO()
    for line in str.split('\n'):
        lineTrimmed = line.strip()
        # Strip single line comments but preserve "pure" multi-line comments
        # Note that ---[[ is a single line comment
        # You can enable a block in Lua by adding a hyphen, e.g.
        #
        # ---[[
        # The block is enabled
        # --]]
        #

        if not lineTrimmed.startswith("--") or lineTrimmed.startswith("--[[") or lineTrimmed.startswith("--]]"):
            sb.write(line)
        sb.write("\n")
    return sb.getvalue()

def scan_lua(str):
    str = strip_single_lua_comments(str)
    ptr = re.compile('--\\[\\[.*?--\\]\\]', re.MULTILINE | re.DOTALL)
    # NOTE: We don't preserve line-numbers
    # '' could be replaced with a function
    str = ptr.sub('', str)

    modules = []
    rp1 = re.compile("require\\s*?\"(.*?)\"$")
    rp2 = re.compile("require\\s*?\\(\\s*?\"(.*?)\"\\s*?\\)$")
    for line in str.split('\n'):
        line = line.strip()
        m1 = rp1.match(line)
        m2 = rp2.match(line)
        if m1:
            modules.append(m1.group(1))
        elif m2:
            modules.append(m2.group(1))
    return modules

def compile_lua(task):
    import lua_ddf_pb2
    with open(task.inputs[0].srcpath(), 'rb') as in_f:
        script = in_f.read()
        modules = scan_lua(script)
        lua_module = lua_ddf_pb2.LuaModule()
        lua_module.source.script = script

        # Making the file name path relative to the source folder
        if hasattr(task.generator, "source_root"):
            base = os.path.abspath(task.generator.source_root)
            p = os.path.abspath(task.outputs[0].abspath())
            p = os.path.relpath(p, base)
            # Keep the suffix
            lua_module.source.filename = os.path.splitext(p)[0] + os.path.splitext(task.inputs[0].srcpath())[1]
        else:
            lua_module.source.filename = task.inputs[0].srcpath()

        for m in modules:
            module_file = "/%s.lua" % m.replace(".", "/")
            lua_module.modules.append(m)
            lua_module.resources.append(module_file + 'c')

        with open(task.outputs[0].abspath(), 'wb') as out_f:
            out_f.write(lua_module.SerializeToString())

    return 0

task = waflib.Task.task_factory('luascript',
                                func    = compile_lua,
                                color   = 'PINK')

@extension('.script')
def script_file(self, node):
    obj_ext = '.scriptc'
    task = self.create_task('luascript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

@extension('.lua')
def script_file(self, node):
    obj_ext = '.luac'
    task = self.create_task('luascript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

new_copy_task('wav', '.wav', '.wavc')
new_copy_task('ogg', '.ogg', '.oggc')

def compile_mesh(task):
    try:
        import google.protobuf.text_format
        import rig.rig_ddf_pb2

        # write skeleton, mesh set and animation set files
        with open(task.outputs[0].abspath(), 'wb') as out_f:
            out_f.write(rig.rig_ddf_pb2.Skeleton().SerializeToString())
        with open(task.outputs[1].abspath(), 'wb') as out_f:
            out_f.write(rig.rig_ddf_pb2.MeshSet().SerializeToString())
        with open(task.outputs[2].abspath(), 'wb') as out_f:
            out_f.write(rig.rig_ddf_pb2.AnimationSet().SerializeToString())

        return 0
    except (google.protobuf.text_format.ParseError,) as e:
        print ('%s:%s' % (task.inputs[0].srcpath(), str(e)), file=sys.stderr)
        return 1

waflib.Task.task_factory('model_file',
                         func    = compile_mesh,
                         color   = 'PINK')

@extension('.dae')
def dae_file(self, node):
    mesh = self.create_task('model_file')
    mesh.set_inputs(node)
    ext_skeleton      = '.skeletonc'
    ext_mesh_set      = '.meshsetc'
    ext_animation_set = '.animationsetc'
    out_skeleton      = node.change_ext(ext_skeleton)
    out_mesh_set      = node.change_ext(ext_mesh_set)
    out_animation_set = node.change_ext(ext_animation_set)
    mesh.set_outputs([out_skeleton, out_mesh_set, out_animation_set])

@extension('.gltf')
def gltf_file(self, node):
    mesh = self.create_task('model_file')
    mesh.set_inputs(node)
    ext_skeleton      = '.skeletonc'
    ext_mesh_set      = '.meshsetc'
    ext_animation_set = '.animationsetc'
    out_skeleton      = node.change_ext(ext_skeleton)
    out_mesh_set      = node.change_ext(ext_mesh_set)
    out_animation_set = node.change_ext(ext_animation_set)
    mesh.set_outputs([out_skeleton, out_mesh_set, out_animation_set])

@extension('.gui_script')
def script_file(self, node):
    obj_ext = '.gui_scriptc'
    task = self.create_task('luascript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

@extension('.render_script')
def render_script_file(self, node):
    obj_ext = '.render_scriptc'
    task = self.create_task('luascript')
    task.set_inputs(node)
    out = node.change_ext(obj_ext)
    task.set_outputs(out)

waflib.Task.task_factory('atlas', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.pipeline.AtlasBuilder ${SRC} ${TGT} ${CONTENT_ROOT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=False)

@extension('.atlas')
def tileset_file(self, node):
    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar']
    atlas = self.create_task('atlas')
    atlas.env['CLASSPATH'] = os.pathsep.join(classpath)
    atlas.env['CONTENT_ROOT'] = atlas.generator.content_root

    atlas.set_inputs(node)

    texture_set = node.change_ext('.t.texturesetc')
    texture = node.change_ext('.texturec')

    atlas.set_outputs([texture_set, texture])

waflib.Task.task_factory('tileset', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.tile.TileSetc ${SRC} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=False)

@extension('.tileset', '.tilesource')
def tileset_file(self, node):
    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar']
    tileset = self.create_task('tileset')
    tileset.env['CLASSPATH'] = os.pathsep.join(classpath)
    tileset.set_inputs(node)
    obj_ext = '.t.texturesetc'
    out = node.change_ext(obj_ext)
    tileset.set_outputs(out)


waflib.Task.task_factory('compute', '${JAVA} -classpath ${CLASSPATH} com.dynamo.bob.pipeline.ComputeBuilder ${SRC} ${SPC} ${TGT}',
                      color='PINK',
                      after='proto_gen_py',
                      before='c cxx',
                      shell=False)

@extension('.compute')
def compute_file(self, node):
    import google.protobuf.text_format
    import render.compute_ddf_pb2
    import dlib

    msg = render.compute_ddf_pb2.ComputeDesc()
    with open(node.srcpath(), 'rb') as in_f:
        google.protobuf.text_format.Merge(in_f.read(), msg)

    shader_name = msg.compute_program + ".spc"
    classpath = [self.env['DYNAMO_HOME'] + '/share/java/bob-light.jar']
    compute = self.create_task('compute')
    compute.env['CLASSPATH'] = os.pathsep.join(classpath)
    compute.env['SPC'] = shader_name

    compute.set_inputs(node)
    obj_ext = '.computec'
    out = node.change_ext(obj_ext)
    compute.set_outputs(out)
