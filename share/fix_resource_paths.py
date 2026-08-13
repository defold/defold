#!/usr/bin/env python3
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



# NOTE: This must be included explicitly prior to any modules with fields that set [(resource) = true].
# Otherwise field_desc.GetOptions().ListFields() will return [] for the first loaded module
# Strange error and probably a bug in google protocol buffers
import ddf.ddf_extensions_pb2

import importlib
import os
import sys

from google.protobuf import text_format
from protobuf_text import merge_gameobject_source_text

# Script to add '/' in front of all resource. A blue-print script on how to automate content changes

def is_resource(field_desc):
    for options_field_desc, value in field_desc.GetOptions().ListFields():
        if options_field_desc.name == 'resource' and value:
            return True
    return False

def fix_resource_files(msg):
    from google.protobuf.descriptor import FieldDescriptor

    descriptor = getattr(msg, 'DESCRIPTOR')
    for field in descriptor.fields:
        value = getattr(msg, field.name)
        if field.type == FieldDescriptor.TYPE_MESSAGE:
            if field.label == FieldDescriptor.LABEL_REPEATED:
                for x in value:
                    fix_resource_files(x)
            elif msg.HasField(field.name):
                fix_resource_files(value)
        elif is_resource(field):
            if field.label == FieldDescriptor.LABEL_REPEATED:
                for i, x in enumerate(value):
                    if x and not x.startswith('/'):
                        value[i] = '/' + x
            else:
                if value and not value.startswith('/'):
                    setattr(msg, field.name, '/' + value)

class ProtofileType(object):
    ext_to_protofile_type = {}
    def __init__(self, ext, module, msg_type):
        self.modules = (module, module.rsplit('.', 1)[-1])
        self.msg_type = msg_type
        self.ext = ext
        ProtofileType.ext_to_protofile_type[ext] = self

    def new_message(self):
        py_module = None
        for module in self.modules:
            try:
                py_module = importlib.import_module(module)
                break
            except ModuleNotFoundError as error:
                if error.name != module and not module.startswith(error.name + '.'):
                    raise
        if py_module is None:
            raise ModuleNotFoundError(self.modules[0])

        message_type = py_module
        for name in self.msg_type.split('.'):
            message_type = getattr(message_type, name)
        return message_type()


def message_to_text(message):
    return text_format.MessageToString(message, as_utf8=True)


def fix_legacy_embedded_component(embedded):
    embedded_type = ProtofileType.ext_to_protofile_type.get('.' + embedded.type)
    if embedded_type is None:
        return

    embedded_message = embedded_type.new_message()
    merge_gameobject_source_text(embedded.data, embedded_message)
    fix_resource_files(embedded_message)
    embedded.data = message_to_text(embedded_message)


def fix_source_prototype(prototype):
    for embedded in prototype.embedded_components:
        if embedded.WhichOneof('payload') == 'data':
            fix_legacy_embedded_component(embedded)


def fix_source_collection(collection):
    prototype_type = ProtofileType.ext_to_protofile_type['.go']
    for embedded in collection.embedded_instances:
        payload = embedded.WhichOneof('payload')
        if payload == 'prototype':
            fix_source_prototype(embedded.prototype)
        elif payload == 'data':
            prototype = prototype_type.new_message()
            merge_gameobject_source_text(embedded.data, prototype)
            fix_resource_files(prototype)
            fix_source_prototype(prototype)
            embedded.data = message_to_text(prototype)

def process_file(file_name):
    _, ext = os.path.splitext(file_name)
    if ext in ProtofileType.ext_to_protofile_type:
        protofile_type = ProtofileType.ext_to_protofile_type[ext]

        msg = protofile_type.new_message()
        with open(file_name, 'r', encoding='utf-8') as in_f:
            merge_gameobject_source_text(in_f.read(), msg)
        msg_str = message_to_text(msg)
        fix_resource_files(msg)
        if ext == '.go':
            fix_source_prototype(msg)
        elif ext == '.collection':
            fix_source_collection(msg)
        msg_str_prim = message_to_text(msg)
        if msg_str != msg_str_prim:
            with open(file_name, 'w', encoding='utf-8') as out_f:
                print('Updating %s' % file_name)
                out_f.write(msg_str_prim)

    else:
        print('Unsupported extension %s' % ext)


ProtofileType('.collection', 'gameobject_source.gameobject_source_ddf_pb2', 'CollectionDesc')
ProtofileType('.go', 'gameobject_source.gameobject_source_ddf_pb2', 'PrototypeDesc')
ProtofileType('.collectionproxy', 'gamesys.collectionproxy_ddf_pb2', 'CollectionProxyDesc')
ProtofileType('.collectionfactory', 'gamesys.gamesys_ddf_pb2', 'CollectionFactoryDesc')
ProtofileType('.emitter', 'particle.particle_ddf_pb2', 'Emitter')
ProtofileType('.model', 'gamesys.model_ddf_pb2', 'ModelDesc')
ProtofileType('.convexshape',  'gamesys.physics_ddf_pb2', 'ConvexShape')
ProtofileType('.collisionobject',  'gamesys.physics_ddf_pb2', 'CollisionObjectDesc')
ProtofileType('.gui',  'gamesys.gui_ddf_pb2', 'SceneDesc')
ProtofileType('.camera', 'gamesys.camera_ddf_pb2', 'CameraDesc')
ProtofileType('.input_binding', 'input.input_ddf_pb2', 'InputBinding')
ProtofileType('.gamepads', 'input.input_ddf_pb2', 'GamepadMaps')
ProtofileType('.factory', 'gamesys.gamesys_ddf_pb2', 'FactoryDesc')
ProtofileType('.label', 'gamesys.label_ddf_pb2', 'LabelDesc')
ProtofileType('.mesh', 'gamesys.mesh_ddf_pb2', 'MeshDesc')
ProtofileType('.particlefx', 'particle.particle_ddf_pb2', 'ParticleFX')
ProtofileType('.render', 'render.render_ddf_pb2', 'RenderPrototypeDesc')
ProtofileType('.sound', 'gamesys.sound_ddf_pb2', 'SoundDesc')
ProtofileType('.sprite', 'gamesys.sprite_ddf_pb2', 'SpriteDesc')
ProtofileType('.tilemap', 'gamesys.tile_ddf_pb2', 'TileGrid')
ProtofileType('.tilegrid', 'gamesys.tile_ddf_pb2', 'TileGrid')
ProtofileType('.directional_light', 'gamesys.data_ddf_pb2', 'Data')
ProtofileType('.point_light', 'gamesys.data_ddf_pb2', 'Data')
ProtofileType('.spot_light', 'gamesys.data_ddf_pb2', 'Data')
ProtofileType('.ambient_light', 'gamesys.data_ddf_pb2', 'Data')
ProtofileType('.material', 'render.material_ddf_pb2', 'MaterialDesc')
ProtofileType('.font', 'render.font_ddf_pb2', 'FontDesc')


def main(project_root):
    for root, _, files in os.walk(project_root):
        for file_name in files:
            _, ext = os.path.splitext(file_name)
            if ext in ProtofileType.ext_to_protofile_type:
                process_file(os.path.join(root, file_name))


if __name__ == '__main__':
    main(sys.argv[1])
