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

import os, sys

from google.protobuf import text_format
import google.protobuf.message

import gameobject.gameobject_ddf_pb2
import gameobject.lua_ddf_pb2
import gamesys.model_ddf_pb2
import gamesys.texture_set_ddf_pb2
import graphics.graphics_ddf_pb2
import resource.liveupdate_ddf_pb2
import rig.rig_ddf_pb2

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


def print_dmanifest(manifest_file):
    for field, data in manifest_file.ListFields():
        if field.name == 'data':
            t = resource.liveupdate_ddf_pb2.ManifestData()
            t.MergeFromString(data)
            print(field.name, ":")
            print(t)

        else:
            print(field.name, ":", data)

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

def text_printer(obj):
    out = text_format.MessageToString(obj)
    print(out)

PRINTERS = {}
PRINTERS['.dmanifest']  = print_dmanifest
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

        printer = PRINTERS.get(ext, text_printer)
        printer(obj)

