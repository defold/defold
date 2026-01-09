#!/usr/bin/env python
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

import bpy
import sys
import os

import json

print("\nDefold: Export all edges from a .blend file")
print("-------------------------------")

def generate(path, outpath):
    bpy.ops.object.delete()
    bpy.ops.wm.open_mainfile(filepath=path)

    out_json = {}

    for x in bpy.context.scene.objects:
        if not x.type == 'MESH':
            continue
        bpy.ops.object.select_all(action='DESELECT')
        x.select_set(True)

        edge_list = []

        for e in x.data.edges:
            vx_0 = e.vertices[0]
            vx_1 = e.vertices[1]

            p_0 = x.data.vertices[vx_0].co
            p_1 = x.data.vertices[vx_1].co

            # Note: The y and z are flipped here
            #       Blender has a right handed coordinate system so we need to account for that
            edge_list.append([p_0.x, p_0.z, p_0.y])
            edge_list.append([p_1.x, p_1.z, p_1.y])

        out_json[x.name] = edge_list

    print("Defold: Writing json to file " + outpath)

    with open(outpath, 'w') as f:
        f.write(json.dumps(out_json))

src_file = sys.argv[-2]
out_file = sys.argv[-1]

generate(src_file, out_file)

print("-------------------------------")
