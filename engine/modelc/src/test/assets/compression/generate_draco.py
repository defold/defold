#!/usr/bin/env python3
"""Generate Draco fixtures using external/draco/generate_test_mesh.cpp.
Pass the compiled generator as the only argument. See README.md.
"""
import base64
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent / 'draco'
root.mkdir(exist_ok=True)
# Raw encoder output is temporary; only the self-contained glTF is retained.
meshes = []
with tempfile.TemporaryDirectory() as directory:
    for offset in (0, 10):
        path = Path(directory) / 'quad.drc'
        subprocess.run([sys.argv[1], str(path), str(offset)], check=True)
        meshes.append(path.read_bytes())
quad, offset_quad = meshes
uv = struct.pack('<8f',0,0,1,0,0,1,1,1)
normals = struct.pack('<12f', *([0,0,1]*4))
morph = struct.pack('<12f', *([0,0,0.25]*4))
raw = quad + offset_quad
raw += bytes(-len(raw)%4)
uv_offset = len(raw)
raw += uv + normals + morph
model = dict(asset=dict(version='2.0'), scene=0, scenes=[dict(nodes=[0])], nodes=[dict(mesh=0)],
    buffers=[dict(byteLength=len(raw),uri='data:application/octet-stream;base64,'+base64.b64encode(raw).decode())],
    bufferViews=[dict(buffer=0,byteLength=len(quad)),dict(buffer=0,byteOffset=len(quad),byteLength=len(offset_quad)),
        dict(buffer=0,byteOffset=uv_offset,byteLength=len(uv)),
        dict(buffer=0,byteOffset=uv_offset+len(uv),byteLength=len(normals)),
        dict(buffer=0,byteOffset=uv_offset+len(uv)+len(normals),byteLength=len(morph))],
    accessors=[dict(componentType=5123,count=6,type='SCALAR'),dict(componentType=5126,count=4,type='VEC3'),
        dict(componentType=5121,count=4,type='VEC4',normalized=True),dict(componentType=5121,count=4,type='VEC4'),
        dict(componentType=5121,count=4,type='VEC4',normalized=True),
        dict(bufferView=2,componentType=5126,count=4,type='VEC2'),
        dict(bufferView=3,componentType=5126,count=4,type='VEC3'),dict(bufferView=4,componentType=5126,count=4,type='VEC3')],
    extensionsUsed=['KHR_draco_mesh_compression'],extensionsRequired=['KHR_draco_mesh_compression'])
primitive = dict(attributes=dict(POSITION=1,COLOR_0=2,JOINTS_0=3,WEIGHTS_0=4,TEXCOORD_0=5,NORMAL=6),indices=0,mode=4,
    targets=[dict(POSITION=7)], extensions={'KHR_draco_mesh_compression':dict(bufferView=0,attributes=dict(POSITION=42,COLOR_0=77,JOINTS_0=101,WEIGHTS_0=205))})
strip = copy.deepcopy(primitive)
strip['mode'] = 5
strip['indices'] = len(model['accessors'])
model['accessors'].append(dict(componentType=5123, count=4, type='SCALAR'))
strip['extensions']['KHR_draco_mesh_compression']['bufferView'] = 1
model['meshes'] = [dict(weights=[0], primitives=[primitive, strip])]
(root/'shared.gltf').write_text(json.dumps(model, indent=2)+'\n')
