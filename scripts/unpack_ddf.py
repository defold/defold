#!/usr/bin/env python3
# Copyright 2020-2023 The Defold Foundation
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



# https://developers.google.com/protocol-buffers/docs/pythontutorial
# https://blog.conan.io/2019/03/06/Serializing-your-data-with-Protobuf.html

# API:
# https://googleapis.dev/python/protobuf/latest/google/protobuf/message.html

import os, sys

from google.protobuf import text_format
import google.protobuf.message

import binascii

import gameobject.gameobject_ddf_pb2
import gameobject.lua_ddf_pb2
import gamesys.model_ddf_pb2
import gamesys.texture_set_ddf_pb2
import graphics.graphics_ddf_pb2
import resource.liveupdate_ddf_pb2
import rig.rig_ddf_pb2
import render.material_ddf_pb2

BUILDERS = {}
BUILDERS['.texturesetc']    = gamesys.texture_set_ddf_pb2.TextureSet
BUILDERS['.modelc']         = gamesys.model_ddf_pb2.Model
BUILDERS['.meshsetc']       = rig.rig_ddf_pb2.MeshSet
BUILDERS['.animationsetc']  = rig.rig_ddf_pb2.AnimationSet
BUILDERS['.rigscenec']      = rig.rig_ddf_pb2.RigScene
BUILDERS['.skeletonc']      = rig.rig_ddf_pb2.Skeleton
BUILDERS['.dmanifest']      = resource.liveupdate_ddf_pb2.ManifestFile
BUILDERS['.vpc']            = graphics.graphics_ddf_pb2.ShaderDesc
BUILDERS['.fpc']            = graphics.graphics_ddf_pb2.ShaderDesc
BUILDERS['.goc']            = gameobject.gameobject_ddf_pb2.PrototypeDesc
BUILDERS['.collectionc']    = gameobject.gameobject_ddf_pb2.CollectionDesc
BUILDERS['.luac']           = gameobject.lua_ddf_pb2.LuaModule
BUILDERS['.materialc']      = render.material_ddf_pb2.MaterialDesc


proto_type_to_string_map = {}
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_BYTES]   = 'TYPE_BYTES'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_DOUBLE]  = 'TYPE_DOUBLE'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_ENUM]    = 'TYPE_ENUM'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_FIXED32] = 'TYPE_FIXED32'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_FIXED64] = 'TYPE_FIXED64'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_FLOAT]   = 'TYPE_FLOAT'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_GROUP]   = 'TYPE_GROUP'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_INT32]   = 'TYPE_INT32'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_INT64]   = 'TYPE_INT64'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_MESSAGE] = 'TYPE_MESSAGE'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_SFIXED32]= 'TYPE_SFIXED32'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_SFIXED64]= 'TYPE_SFIXED64'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_SINT32]  = 'TYPE_SINT32'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_SINT64]  = 'TYPE_SINT64'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_STRING]  = 'TYPE_STRING'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_UINT32]  = 'TYPE_UINT32'
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_UINT64]  = 'TYPE_UINT64'

TYPE_CONVERTERS = {}
TYPE_CONVERTERS["dmLiveUpdateDDF.ManifestFile.data"] = resource.liveupdate_ddf_pb2.ManifestData

def get_field_type(field):
    if field.message_type is not None:
        return field.message_type.name
    return proto_type_to_string_map.get(field.type, str(field.type))

def get_descriptor_type(descriptor):
    if descriptor.message_type is not None:
        return descriptor.message_type.name
    return proto_type_to_string_map.get(descriptor.type, str(descriptor.type))


def print_descriptor_default(descriptor, data):
    if descriptor.type == descriptor.TYPE_ENUM:
        enum_name = descriptor.enum_type.values[data].name
        print(descriptor.name, ":", enum_name, get_descriptor_type(descriptor))
    else:
        print(descriptor.name, ":", data, get_descriptor_type(descriptor))

def print_bytes(descriptor, data):
    print(descriptor.name, ":", str.format('[%s]' % binascii.hexlify(data).decode('utf8')), "len:", len(data) )

def print_string(descriptor, data):
    print(descriptor.name, ":", str.format('"%s"' % data), "len:", len(data))

def print_uint32(descriptor, data):
    print(descriptor.name, ":", str.format('%d\t(0x%08x)' % (data, data)), get_descriptor_type(descriptor))

def print_uint64(descriptor, data):
    print(descriptor.name, ":", str.format('%d\t(0x%016x)' % (data, data)), get_descriptor_type(descriptor))


FIELD_PRINTERS = {}
FIELD_PRINTERS['TYPE_BYTES'] = print_bytes
FIELD_PRINTERS['TYPE_STRING'] = print_string
FIELD_PRINTERS['TYPE_UINT32'] = print_uint32
FIELD_PRINTERS['TYPE_UINT64'] = print_uint64

def print_value(descriptor, value):
    printer = FIELD_PRINTERS.get(get_descriptor_type(descriptor), print_descriptor_default)
    printer(descriptor, value)

# **************************************************************************************************

INDENT = 2

# def Indent(indent):
#     print(' ' * indent, end='')

class Printer(object):
    def __init__(self):
        self.indent = 0
    def Indent(self):
        self.indent += INDENT
    def Unindent(self):
        self.indent -= INDENT
    def PrintIndent(self):
        print(' '*self.indent, end='');
    def Print(self,*args):
        print(' '*self.indent, end='');
        print(*args)

def print_object(printer, msg):
    for descriptor in msg.DESCRIPTOR.fields:
        value = getattr(msg, descriptor.name)

        cls = TYPE_CONVERTERS.get(descriptor.full_name, None)
        if cls is not None:
            newvalue = cls()
            newvalue.MergeFromString(value)
            printer.Print(descriptor.name, ": {")
            printer.Indent()
            print_object(printer, newvalue)
            printer.Unindent()
            printer.Print("}")
            continue

        if descriptor.type == descriptor.TYPE_MESSAGE:
            printer.Print(descriptor.name, ": {")
            printer.Indent()
            if descriptor.label == descriptor.LABEL_REPEATED:
                for v in value:
                    print_object(printer, v)
            else:
                print_object(printer, value)
            printer.Unindent()
            printer.Print("}")
        else:
            if descriptor.label == descriptor.LABEL_REPEATED:
                for v in value:
                    printer.PrintIndent(); print_value(descriptor, v)
            else:
                printer.PrintIndent(); print_value(descriptor, value)

# Should work for most messages
# Also see specific conversions in TYPE_CONVERTERS
def print_message(msg):
    print_object(Printer(), msg)

def print_shader(shader):
    print("{")
    for field, data in shader.ListFields():
        if field.name == 'source':
            lines = str(data.strip(), encoding="ascii").split("\n")
            data = '\n    '.join(['']+lines)
        print(field.name, ":", data)
    print("}")

def print_shader_file(shader_file):
    def print_shader_code(text):
        pass
    print("shaders:")
    for field, data in shader_file.ListFields():
        for shader in data:
            print_shader(shader)


PRINTERS = {}
PRINTERS['.vpc']        = print_shader_file
PRINTERS['.fpc']        = print_shader_file


if __name__ == "__main__":
    path = sys.argv[1]

    _, ext = os.path.splitext(path)
    builder = BUILDERS.get(ext, None)
    if builder is None:
        print("No builder registered for filetype %s" %ext)
        sys.exit(1)

    with open(path, 'rb') as f:
        content = f.read()
        obj = builder()
        obj.ParseFromString(content)

        printer = PRINTERS.get(ext, print_message)
        printer(obj)

