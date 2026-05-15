# Copyright 2020-2025 The Defold Foundation
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

"""Helper routines shared with the legacy waf_gamesys.py script.

This module mirrors the transformation helpers used when converting gamesys
protobuf assets. Unlike waf_gamesys.py it carries no waf dependencies, allowing
it to be imported from standalone tooling (e.g. CMake-based generators).
"""

from __future__ import annotations

import argparse
import importlib
import os
import sys
from dataclasses import dataclass
from typing import Callable, Dict, Iterable, List, Optional

from google.protobuf import text_format


class Context:
    def __init__(self, content_root: str):
        self.content_root = content_root


def _add_pythonpath(entries: Iterable[str]) -> None:
    for entry in entries:
        abs_entry = os.path.abspath(entry)
        if abs_entry not in sys.path:
            sys.path.insert(0, abs_entry)


@dataclass(frozen=True)
class ContentRule:
    module: str
    message: str
    transform: Optional[Callable]
    output_ext: str


def transform_properties(properties, out_properties):
    import dlib
    import gameobject_ddf_pb2
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
            out_properties.float_values.extend(float(v.strip()) for v in property.value.split(','))
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_VECTOR4:
            entry = out_properties.vector4_entries.add()
            entry.index = len(out_properties.float_values)
            out_properties.float_values.extend(float(v.strip()) for v in property.value.split(','))
        elif property.type == gameobject_ddf_pb2.PROPERTY_TYPE_QUAT:
            entry = out_properties.quat_entries.add()
            entry.index = len(out_properties.float_values)
            out_properties.float_values.extend(float(v.strip()) for v in property.value.split(','))
        else:
            raise Exception("Invalid type")
        entry.key = property.id
        entry.id = dlib.dmHashBuffer64(property.id)


def transform_texture_name(_context, name):
    name = name.replace('.png', '.texturec')
    name = name.replace('.jpg', '.texturec')
    return name


def transform_tilesource_name(name):
    name = name.replace('.tileset', '.t.texturesetc')
    name = name.replace('.tilesource', '.t.texturesetc')
    name = name.replace('.atlas', '.t.texturesetc')
    return name


def transform_collection(context, msg):
    for instance in msg.instances:
        instance.prototype = instance.prototype.replace('.go', '.goc')
        instance.id = '/' + instance.id
        for comp_props in instance.component_properties:
            transform_properties(comp_props.properties, comp_props.property_decls)
    for collection_instance in msg.collection_instances:
        collection_instance.collection = collection_instance.collection.replace('.collection', '.collectionc')
    return msg


def transform_collectionproxy(_context, msg):
    msg.collection = msg.collection.replace('.collection', '.collectionc')
    return msg

def transform_collisionobject(context, msg):
    import ddf.ddf_math_pb2
    import dlib
    import google.protobuf.text_format
    import physics_ddf_pb2
    if msg.type != physics_ddf_pb2.COLLISION_OBJECT_TYPE_DYNAMIC:
        msg.mass = 0

    if msg.collision_shape and not (msg.collision_shape.endswith('.tilegrid') or msg.collision_shape.endswith('.tilemap')):
        source_path = os.path.join(context.content_root, msg.collision_shape[1:])

        convex_msg = physics_ddf_pb2.ConvexShape()
        with open(source_path, 'rb') as proto_file:
            google.protobuf.text_format.Merge(proto_file.read(), convex_msg)

        shape = msg.embedded_collision_shape.shapes.add()
        shape.shape_type = convex_msg.shape_type
        shape.position.x = shape.position.y = shape.position.z = 0
        shape.rotation.x = shape.rotation.y = shape.rotation.z = 0
        shape.rotation.w = 1
        shape.index = len(msg.embedded_collision_shape.data)
        shape.count = len(convex_msg.data)

        msg.embedded_collision_shape.data.extend(convex_msg.data)
        msg.collision_shape = ''

    for shape in msg.embedded_collision_shape.shapes:
        shape.id_hash = dlib.dmHashBuffer64(shape.id)

    msg.collision_shape = msg.collision_shape.replace('.convexshape', '.convexshapec')
    msg.collision_shape = msg.collision_shape.replace('.tilemap', '.tilemapc')
    msg.collision_shape = msg.collision_shape.replace('.tilegrid', '.tilemapc')
    return msg


def transform_particlefx(context, msg):
    for emitter in msg.emitters:
        emitter.material = emitter.material.replace('.material', '.materialc')
        emitter.tile_source = transform_tilesource_name(emitter.tile_source)
    return msg


def transform_gameobject(context, msg):
    for component in msg.components:
        component.component = component.component.replace('.camera', '.camerac')
        component.component = component.component.replace('.collectionproxy', '.collectionproxyc')
        component.component = component.component.replace('.collisionobject', '.collisionobjectc')
        component.component = component.component.replace('.particlefx', '.particlefxc')
        component.component = component.component.replace('.gui', '.guic')
        component.component = component.component.replace('.model', '.modelc')
        component.component = component.component.replace('.animationset', '.animationsetc')
        component.component = component.component.replace('.script', '.scriptc')
        component.component = component.component.replace('.sound', '.soundc')
        component.component = component.component.replace('.factory', '.factoryc')
        component.component = component.component.replace('.collectionfactory', '.collectionfactoryc')
        component.component = component.component.replace('.label', '.labelc')
        component.component = component.component.replace('.light', '.lightc')
        component.component = component.component.replace('.sprite', '.spritec')
        component.component = component.component.replace('.tileset', '.t.texturesetc')
        component.component = component.component.replace('.tilesource', '.t.texturesetc')
        component.component = component.component.replace('.tilemap', '.tilemapc')
        component.component = component.component.replace('.tilegrid', '.tilemapc')

        transform_properties(component.properties, component.property_decls)
    return msg


def transform_gui(context, msg):
    msg.script = msg.script.replace('.gui_script', '.gui_scriptc')
    font_names = {font.name for font in msg.fonts}
    texture_names = {texture.name for texture in msg.textures}
    material_names = {material.name for material in msg.materials}

    msg.material = msg.material.replace('.material', '.materialc')
    for font in msg.fonts:
        font.font = font.font.replace('.font', '.fontc')
    for material in msg.materials:
        material.material = material.material.replace('.material', '.materialc')
    for texture in msg.textures:
        texture.texture = transform_tilesource_name(transform_texture_name(context, texture.texture))

    for node in msg.nodes:
        if node.texture and node.texture not in texture_names:
            atlas_part = node.texture.split('/', 1)[0]
            if atlas_part not in texture_names:
                raise Exception(f'Texture "{node.texture}" not declared in gui-file')
        if node.material and node.material not in material_names:
            raise Exception(f'Material "{node.material}" not declared in gui-file')
        if node.font and node.font not in font_names:
            raise Exception(f'Font "{node.font}" not declared in gui-file')
    return msg


def transform_factory(context, msg):
    msg.prototype = msg.prototype.replace('.go', '.goc')
    return msg


def transform_collectionfactory(context, msg):
    msg.prototype = msg.prototype.replace('.collection', '.collectionc')
    return msg


def transform_render(context, msg):
    import render.render_ddf_pb2
    msg.script = msg.script.replace('.render_script', '.render_scriptc')

    for material in msg.materials:
        entry = render.render_ddf_pb2.RenderPrototypeDesc.RenderResourceDesc()
        entry.name = material.name
        entry.path = material.material
        msg.render_resources.append(entry)

    for resource in msg.render_resources:
        resource.path = resource.path.replace('.material', '.materialc')
        resource.path = resource.path.replace('.render_target', '.render_targetc')

    msg.materials.clear()
    return msg


def transform_sprite(context, msg):
    import sprite_ddf_pb2
    msg.material = msg.material.replace('.material', '.materialc')
    if msg.tile_set:
        sprite_tex = sprite_ddf_pb2.SpriteTexture()
        sprite_tex.sampler = ""
        sprite_tex.texture = msg.tile_set
        msg.textures.append(sprite_tex)
        msg.tile_set = ""

    for texture in msg.textures:
        texture.texture = transform_tilesource_name(texture.texture)
    return msg


def transform_tilegrid(context, msg):
    msg.tile_set = transform_tilesource_name(msg.tile_set)
    msg.material = msg.material.replace('.material', '.materialc')
    return msg


def transform_sound(context, msg):
    msg.sound = msg.sound.replace('.wav', '.wavc')
    msg.sound = msg.sound.replace('.ogg', '.oggc')
    return msg


def transform_mesh(context, msg):
    msg.vertices = msg.vertices.replace('.buffer', '.bufferc')
    msg.vertices = msg.vertices.replace('.prebuilt_bufferc', '.prebuilt_bufferc')
    msg.material = msg.material.replace('.material', '.materialc')
    return msg


def transform_label(context, msg):
    msg.font = msg.font.replace('.font', '.fontc')
    msg.material = msg.material.replace('.material', '.materialc')
    return msg


_CONTENT_RULES: Dict[str, ContentRule] = {
    '.collection': ContentRule('gameobject_ddf_pb2', 'CollectionDesc', transform_collection, '.collectionc'),
    '.collectionproxy': ContentRule('gamesys_ddf_pb2', 'CollectionProxyDesc', transform_collectionproxy, '.collectionproxyc'),
    '.particlefx': ContentRule('particle_ddf_pb2', 'ParticleFX', transform_particlefx, '.particlefxc'),
    '.convexshape': ContentRule('physics_ddf_pb2', 'ConvexShape', None, '.convexshapec'),
    '.collisionobject': ContentRule('physics_ddf_pb2', 'CollisionObjectDesc', transform_collisionobject, '.collisionobjectc'),
    '.gui': ContentRule('gui_ddf_pb2', 'SceneDesc', transform_gui, '.guic'),
    '.camera': ContentRule('camera_ddf_pb2', 'CameraDesc', None, '.camerac'),
    '.input_binding': ContentRule('input_ddf_pb2', 'InputBinding', None, '.input_bindingc'),
    '.gamepads': ContentRule('input_ddf_pb2', 'GamepadMaps', None, '.gamepadsc'),
    '.factory': ContentRule('gamesys_ddf_pb2', 'FactoryDesc', transform_factory, '.factoryc'),
    '.collectionfactory': ContentRule('gamesys_ddf_pb2', 'CollectionFactoryDesc', transform_collectionfactory, '.collectionfactoryc'),
    '.light': ContentRule('data_ddf_pb2', 'Data', None, '.lightc'),
    '.label': ContentRule('label_ddf_pb2', 'LabelDesc', transform_label, '.labelc'),
    '.render': ContentRule('render_ddf_pb2', 'RenderPrototypeDesc', transform_render, '.renderc'),
    '.render_target': ContentRule('render_target_ddf_pb2', 'RenderTargetDesc', None, '.render_targetc'),
    '.sprite': ContentRule('sprite_ddf_pb2', 'SpriteDesc', transform_sprite, '.spritec'),
    '.tilegrid': ContentRule('tile_ddf_pb2', 'TileGrid', transform_tilegrid, '.tilemapc'),
    '.tilemap': ContentRule('tile_ddf_pb2', 'TileGrid', transform_tilegrid, '.tilemapc'),
    '.sound': ContentRule('sound_ddf_pb2', 'SoundDesc', transform_sound, '.soundc'),
    '.mesh': ContentRule('mesh_ddf_pb2', 'MeshDesc', transform_mesh, '.meshc'),
    '.display_profiles': ContentRule('render_ddf_pb2', 'DisplayProfiles', None, '.display_profilesc'),
}


def _rule_for_proto(proto_path: str) -> ContentRule:
    _, ext = os.path.splitext(proto_path)
    ext = ext.lower()
    try:
        return _CONTENT_RULES[ext]
    except KeyError as exc:
        raise ValueError(f"No gamesys content rule for extension '{ext}' (file: {proto_path})") from exc


def _load_message_for_rule(proto_path: str, rule: ContentRule):
    module = importlib.import_module(rule.module)
    message_cls = module
    for token in rule.message.split('.'):
        message_cls = getattr(message_cls, token)
    message = message_cls()
    with open(proto_path, 'rb') as proto_file:
        text_format.Merge(proto_file.read(), message)
    return message


def _collect_resources_from_message(message, resources):
    descriptor = message.DESCRIPTOR
    for field in descriptor.fields:
        value = getattr(message, field.name)
        if field.type == field.TYPE_MESSAGE:
            if field.label == field.LABEL_REPEATED:
                for sub in value:
                    _collect_resources_from_message(sub, resources)
            else:
                if hasattr(value, 'ByteSize') and value.ByteSize() == 0:
                    continue
                _collect_resources_from_message(value, resources)
        else:
            options = dict((opt_desc.name, opt_val) for opt_desc, opt_val in field.GetOptions().ListFields())
            if options.get('resource'):
                if field.label == field.LABEL_REPEATED:
                    for item in value:
                        if item:
                            resources.add(item)
                else:
                    if value:
                        resources.add(value)


def get_dependencies(proto_path: str, rule: ContentRule) -> List[str]:
    """Return a sorted list of resource paths referenced by the given proto.

    Example (list dependencies)::

        python engine/gamesys/src/proto_gamesys.py \
            --proto engine/gamesys/src/gamesys/test/gui/valid.gui \
            --output /tmp/valid.deps

    Example (compile)::

        python engine/gamesys/src/proto_gamesys.py \
            --proto engine/gamesys/src/gamesys/test/gui/valid.gui \
            --output /tmp/valid.guic --compile \
            --content-root engine/gamesys/src/gamesys/test
    """

    message = _load_message_for_rule(proto_path, rule)
    resources = set()
    _collect_resources_from_message(message, resources)
    return sorted(resources)


def _parse_args():
    parser = argparse.ArgumentParser(description="List resource dependencies from text-format DDF files or compile transformed binaries.")
    parser.add_argument("--proto", required=True, help="Path to the text-format protobuf file")
    parser.add_argument("--output", help="Optional file to write dependencies or binary output")
    parser.add_argument("--compile", action="store_true", help="Compile the proto to binary output")
    parser.add_argument("--content-root", default=".", help="Content root used when resolving relative resource paths")
    parser.add_argument("--pythonpath", action="append", default=[], help="Additional entries to prepend to sys.path")
    return parser.parse_args()


def main():
    args = _parse_args()
    _add_pythonpath(args.pythonpath)

    try:
        rule = _rule_for_proto(args.proto)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc

    if args.compile:
        if not args.output:
            raise SystemExit("--output is required when using --compile")

        message = _load_message_for_rule(args.proto, rule)

        if rule.transform is not None:
            context = Context(args.content_root)
            result = rule.transform(context, message)
            if result is not None:
                message = result

        with open(args.output, 'wb') as out_file:
            out_file.write(message.SerializeToString())
    else:
        deps = get_dependencies(args.proto, rule)
        if args.output:
            with open(args.output, 'w', encoding='utf-8') as out_file:
                out_file.write("\n".join(deps))
                out_file.write("\n")
        else:
            for dependency in deps:
                print(dependency)

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
