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



# https://developers.google.com/protocol-buffers/docs/pythontutorial
# https://blog.conan.io/2019/03/06/Serializing-your-data-with-Protobuf.html

# API:
# https://googleapis.dev/python/protobuf/latest/google/protobuf/message.html

import glob
import os, sys
import importlib
import subprocess
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.dirname(SCRIPT_DIR)

GENERATED_PROTO_MODULES = {
    'input.input_ddf_pb2': (os.path.join(REPO_ROOT, "engine", "input", "proto"), "input/input_ddf.proto"),
}

def prepend_python_paths(paths):
    for path in reversed(paths):
        if path is not None and os.path.isdir(path) and path not in sys.path:
            sys.path.insert(0, path)

dynamo_home = os.environ.get('DYNAMO_HOME')
if dynamo_home is None:
    default_dynamo_home = os.path.join(REPO_ROOT, "tmp", "dynamo_home")
    if os.path.isdir(default_dynamo_home):
        dynamo_home = default_dynamo_home

python_paths = []
if dynamo_home is not None:
    python_paths.extend([
        os.path.join(dynamo_home, "lib", "python"),
        os.path.join(dynamo_home, "ext", "lib", "python"),
    ])

python_paths.extend([
    os.path.join(REPO_ROOT, "engine", "ddf", "build", "src"),
    os.path.join(REPO_ROOT, "engine", "input", "build", "python"),
    os.path.join(REPO_ROOT, "engine", "gamesys", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "render", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "rig", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "resource", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "graphics", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "gameobject", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "engine", "build", "proto"),
    os.path.join(REPO_ROOT, "engine", "particle", "build", "proto"),
])
prepend_python_paths(python_paths)

from google.protobuf import text_format
import google.protobuf.message

import binascii

def find_protoc():
    if dynamo_home is None:
        return None

    patterns = [
        os.path.join(dynamo_home, "ext", "bin", "*", "protoc"),
        os.path.join(dynamo_home, "ext", "bin", "*", "protoc.exe"),
    ]
    for pattern in patterns:
        for path in glob.glob(pattern):
            if os.path.isfile(path) and os.access(path, os.X_OK):
                return path
    return None

def ensure_generated_module(module_name):
    proto_info = GENERATED_PROTO_MODULES.get(module_name)
    if proto_info is None:
        return

    proto_dir, proto_file = proto_info
    proto_path = os.path.join(proto_dir, proto_file)
    if not os.path.isfile(proto_path):
        return

    out_dir = os.path.join(tempfile.gettempdir(), "defold_unpack_ddf_proto")
    module_path = os.path.join(out_dir, *module_name.split('.')) + ".py"
    if os.path.isfile(module_path) and os.path.getmtime(module_path) >= os.path.getmtime(proto_path):
        prepend_python_paths([out_dir])
        return

    protoc = find_protoc()
    if protoc is None:
        return

    os.makedirs(out_dir, exist_ok=True)
    try:
        subprocess.check_call([protoc, "--python_out=%s" % out_dir, "-I%s" % proto_dir, proto_path], stdout=subprocess.DEVNULL)
    except subprocess.CalledProcessError:
        return

    package_dir = os.path.dirname(module_path)
    os.makedirs(package_dir, exist_ok=True)
    with open(os.path.join(package_dir, "__init__.py"), "a"):
        pass
    prepend_python_paths([out_dir])

def load_type(module_name, type_name):
    ensure_generated_module(module_name)
    module = importlib.import_module(module_name)
    return getattr(module, type_name)

BUILDERS = {}
BUILDERS['.animationsetc']    = ('rig.rig_ddf_pb2', 'AnimationSet')
BUILDERS['.collectionc']      = ('gameobject.gameobject_ddf_pb2', 'CollectionDesc')
BUILDERS['.collisionobjectc'] = ('gamesys.physics_ddf_pb2', 'CollisionObjectDesc')
BUILDERS['.computec']         = ('render.compute_ddf_pb2', 'ComputeDesc')
BUILDERS['.convexshapec']     = ('gamesys.physics_ddf_pb2', 'ConvexShape')
BUILDERS['.dmanifest']        = ('resource.liveupdate_ddf_pb2', 'ManifestFile')
BUILDERS['.fontc']            = ('render.font_ddf_pb2', 'FontMap')
BUILDERS['.gamepadsc']        = ('input.input_ddf_pb2', 'GamepadMapsRuntime')
BUILDERS['.glyph_bankc']      = ('render.font_ddf_pb2', 'GlyphBank')
BUILDERS['.goc']              = ('gameobject.gameobject_ddf_pb2', 'PrototypeDesc')
BUILDERS['.guic']             = ('gamesys.gui_ddf_pb2', 'SceneDesc')
BUILDERS['.input_bindingc']   = ('input.input_ddf_pb2', 'InputBinding')
BUILDERS['.luac']             = ('gameobject.lua_ddf_pb2', 'LuaModule')
BUILDERS['.labelc']           = ('gamesys.label_ddf_pb2', 'LabelDesc')
BUILDERS['.materialc']        = ('render.material_ddf_pb2', 'MaterialDesc')
BUILDERS['.meshsetc']         = ('rig.rig_ddf_pb2', 'MeshSet')
BUILDERS['.modelc']           = ('gamesys.model_ddf_pb2', 'Model')
BUILDERS['.particlefxc']      = ('particle.particle_ddf_pb2', 'ParticleFX')
BUILDERS['.renderc']          = ('render.render_ddf_pb2', 'RenderPrototypeDesc')
BUILDERS['.rigscenec']        = ('rig.rig_ddf_pb2', 'RigScene')
BUILDERS['.skeletonc']        = ('rig.rig_ddf_pb2', 'Skeleton')
BUILDERS['.spc']              = ('graphics.graphics_ddf_pb2', 'ShaderDesc')
BUILDERS['.spritec']          = ('gamesys.sprite_ddf_pb2', 'SpriteDesc')
BUILDERS['.texturec']         = ('graphics.graphics_ddf_pb2', 'TextureImage')
BUILDERS['.texturesetc']      = ('gamesys.texture_set_ddf_pb2', 'TextureSet')
BUILDERS['.camerac']          = ('gamesys.camera_ddf_pb2', 'CameraDesc')

proto_type_to_string_map = {}
proto_type_to_string_map[google.protobuf.descriptor.FieldDescriptor.TYPE_BOOL]    = 'TYPE_BOOL'
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
TYPE_CONVERTERS["dmLiveUpdateDDF.ManifestFile.data"] = ('resource.liveupdate_ddf_pb2', 'ManifestData')

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
        e = descriptor.enum_type.values_by_number.get(data)
        print(descriptor.name, ":", e.name, get_descriptor_type(descriptor))
    else:
        print(descriptor.name, ":", data, get_descriptor_type(descriptor))

def print_bytes(descriptor, data):
    print(descriptor.name, ":", str.format('[%s]' % binascii.hexlify(data).decode('utf8')), "len:", len(data) )

def print_string(descriptor, data):
    print(descriptor.name, ":", '"' + data.replace('"', '\\"') + '"', "len:", len(data))

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

class hexdump:
    def __init__(self, buf, off=0):
        self.buf = buf
        self.off = off

    def __iter__(self):
        last_bs, last_line = None, None
        for i in range(0, len(self.buf), 16):
            bs = bytearray(self.buf[i : i + 16])
            line = "{:08x}  {:23}  {:23}  |{:16}|".format(
                self.off + i,
                " ".join(("{:02x}".format(x) for x in bs[:8])),
                " ".join(("{:02x}".format(x) for x in bs[8:])),
                "".join((chr(x) if 32 <= x < 127 else "." for x in bs)),
            )
            if bs == last_bs:
                line = "*"
            if bs != last_bs or line != last_line:
                yield line
            last_bs, last_line = bs, line
        yield "{:08x}".format(self.off + len(self.buf))

    def __str__(self):
        return "\n".join(self)

    def __repr__(self):
        return "\n".join(self)

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

        if not descriptor.label == descriptor.LABEL_REPEATED:
            if not msg.HasField(descriptor.name):
                continue

        cls_info = TYPE_CONVERTERS.get(descriptor.full_name, None)
        if cls_info is not None:
            cls = load_type(*cls_info)
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

def print_lua_file(script):
    print_message(script)
    source = getattr(getattr(script, "source"), "script")
    if source:
        lines = source.decode("utf-8", errors="replace").strip().split("\n")
        source = '\n    '.join(['']+lines)
        print("Source: ", source)

def print_shader(shader):
    print("{")
    for field, data in shader.ListFields():
        if field.name == 'source':
            lines = data.decode("utf-8", errors="replace").strip().split("\n")
            data = '\n    '.join(['']+lines)
        print(field.name, ":", data)
    print("}")

def print_shader_file(shader_file):
    def print_shader_code(text):
        pass
    print("shaders:")
    for field, data in shader_file.ListFields():
        try:
            iter(data)
        except TypeError:
            print("Unknown:", data)
        else:
            for shader in data:
                print_shader(shader)


PRINTERS = {}
PRINTERS['.luac'] = print_lua_file
PRINTERS['.spc']  = print_shader_file

if __name__ == "__main__":
    path = sys.argv[1]

    with open(path, 'rb') as f:
        content = f.read()
        base, ext = os.path.splitext(path)
        if ext == ".lz4":
            import lz4.block

            base, ext = os.path.splitext(base)
            decompressed_size = len(content) * 2
            while True:
                try:
                    content = lz4.block.decompress(content, uncompressed_size=decompressed_size, return_bytearray=True)
                    break
                except lz4.block.LZ4BlockError:
                    decompressed_size *= 2
        builder_info = BUILDERS.get(ext, None)
        if builder_info is None:
            print("No builder registered for filetype %s" %ext)
            try:
                utf8 = content.decode("utf-8")
                print("%s" % utf8)
            except:
                print(hexdump(content))
        else:
            builder = load_type(*builder_info)
            obj = builder()
            obj.ParseFromString(content)

            printer = PRINTERS.get(ext, print_message)
            printer(obj)
