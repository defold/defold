#!/usr/bin/python
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

import google.protobuf.text_format
import sys
import os

# Script to add '/' in front of all resource. A blue-print script on how to automate content changes

def is_resource(field_desc):
    for options_field_desc, value in field_desc.GetOptions().ListFields():
        if options_field_desc.name == 'resource' and value:
            return True
    return False

def fix_resource_files(msg):
    import google.protobuf
    from google.protobuf.descriptor import FieldDescriptor

    descriptor = getattr(msg, 'DESCRIPTOR')
    for field in descriptor.fields:
        value = getattr(msg, field.name)
        if field.type == FieldDescriptor.TYPE_MESSAGE:
            if field.label == FieldDescriptor.LABEL_REPEATED:
                for x in value:
                    fix_resource_files(x)
            else:
                fix_resource_files(value)
        elif is_resource(field):
            if field.label == FieldDescriptor.LABEL_REPEATED:
                for i, x in enumerate(value):
                    if not x.startswith('/'):
                        value[i] = '/' + x
            else:
                if not value.startswith('/'):
                    setattr(msg, field.name, '/' + value)

class ProtofileType(object):
    ext_to_protofile_type = {}
    def __init__(self, ext, module, msg_type):
        self.module = module
        self.py_module = __import__(module)
        self.msg_type = msg_type
        self.ext = ext
        ProtofileType.ext_to_protofile_type[ext] = self

def process_file(file_name):
    _, ext = os.path.splitext(file_name)
    if ProtofileType.ext_to_protofile_type.has_key(ext):
        type = ProtofileType.ext_to_protofile_type[ext]

        msg = eval('type.py_module.' + type.msg_type)() # Call constructor on message type
        with open(file_name, 'rb') as in_f:
            google.protobuf.text_format.Merge(in_f.read(), msg)
        msg_str = str(msg)
        fix_resource_files(msg)
        if ext == '.go':
            for embedded in msg.embedded_components:
                embedded_type = ProtofileType.ext_to_protofile_type['.' + embedded.type]
                embedded_msg = eval('embedded_type.py_module.' + embedded_type.msg_type)() # Call constructor on message type
                google.protobuf.text_format.Merge(embedded.data, embedded_msg)
                fix_resource_files(embedded_msg)
                embedded.data = str(embedded_msg)
        msg_str_prim = str(msg)
        if msg_str != msg_str_prim:
            with open(file_name, 'wb') as out_f:
                print('Updating %s' % file_name)
                out_f.write(msg_str_prim)
#        print msg

    else:
        print('Unsupported extension %s' % ext)

ProtofileType('.collection', 'gameobject_ddf_pb2', 'CollectionDesc')
ProtofileType('.go', 'gameobject_ddf_pb2', 'PrototypeDesc')
ProtofileType('.collectionproxy', 'gamesys_ddf_pb2', 'CollectionProxyDesc')
ProtofileType('.emitter', 'particle.particle_ddf_pb2', 'particle_ddf_pb2.Emitter')
ProtofileType('.model', 'model_ddf_pb2', 'ModelDesc')
ProtofileType('.convexshape',  'physics_ddf_pb2', 'ConvexShape')
ProtofileType('.collisionobject',  'physics_ddf_pb2', 'CollisionObjectDesc')
ProtofileType('.gui',  'gui_ddf_pb2', 'SceneDesc')
ProtofileType('.camera', 'camera_ddf_pb2', 'CameraDesc')
ProtofileType('.input_binding', 'input_ddf_pb2', 'InputBinding')
ProtofileType('.gamepads', 'input_ddf_pb2', 'GamepadMaps')
ProtofileType('.factory', 'gamesys_ddf_pb2', 'FactoryDesc')
ProtofileType('.light', 'gamesys_ddf_pb2', 'LightDesc')
ProtofileType('.render', 'render.render_ddf_pb2', 'render_ddf_pb2.RenderPrototypeDesc')
ProtofileType('.sprite', 'sprite_ddf_pb2', 'SpriteDesc')
ProtofileType('.material', 'render.material_ddf_pb2', 'material_ddf_pb2.MaterialDesc')
ProtofileType('.font', 'render.font_ddf_pb2', 'font_ddf_pb2.FontDesc')


for root, dirs, files in os.walk(sys.argv[1]):
    for f in files:
        _, ext = os.path.splitext(f)
        if ProtofileType.ext_to_protofile_type.has_key(ext):
            full_path = os.path.join(root, f)
            process_file(full_path)
