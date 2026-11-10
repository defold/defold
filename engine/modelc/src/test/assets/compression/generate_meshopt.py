#!/usr/bin/env python3
"""Regenerate committed fixtures with a shared build of Meshoptimizer v1.2.

Usage: python3 generate_meshopt.py /path/to/libmeshoptimizer.dylib
No encoder is required when running modelc tests.
"""
import base64
import copy
import ctypes as c
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parent
lib = c.CDLL(sys.argv[1])
for name in ('meshopt_encodeVertexBufferBound', 'meshopt_encodeIndexBufferBound', 'meshopt_encodeIndexSequenceBound'):
    getattr(lib, name).argtypes = [c.c_size_t, c.c_size_t]
    getattr(lib, name).restype = c.c_size_t
lib.meshopt_encodeVertexBuffer.argtypes = [c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t, c.c_size_t]
lib.meshopt_encodeVertexBuffer.restype = c.c_size_t
for name in ('meshopt_encodeIndexBuffer', 'meshopt_encodeIndexSequence'):
    getattr(lib, name).argtypes = [c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t]
    getattr(lib, name).restype = c.c_size_t
for name in ('Oct', 'Quat', 'Color'):
    getattr(lib, 'meshopt_encodeFilter' + name).argtypes = [c.c_void_p, c.c_size_t, c.c_size_t, c.c_int, c.c_void_p]
lib.meshopt_encodeFilterExp.argtypes = [c.c_void_p, c.c_size_t, c.c_size_t, c.c_int, c.c_void_p, c.c_int]
for name in ('Oct', 'Quat', 'Exp', 'Color'):
    getattr(lib, 'meshopt_decodeFilter' + name).argtypes = [c.c_void_p, c.c_size_t, c.c_size_t]


def encode(raw, stride, mode='ATTRIBUTES', version=1):
    count = len(raw) // stride
    if mode == 'ATTRIBUTES':
        lib.meshopt_encodeVertexVersion(version)
        bound = lib.meshopt_encodeVertexBufferBound(count, stride)
        output = c.create_string_buffer(bound)
        size = lib.meshopt_encodeVertexBuffer(output, bound, c.create_string_buffer(raw), count, stride)
    else:
        values = struct.unpack('<' + ('H' if stride == 2 else 'I') * count, raw)
        indices = (c.c_uint * count)(*values)
        stem = 'IndexBuffer' if mode == 'TRIANGLES' else 'IndexSequence'
        bound = getattr(lib, 'meshopt_encode' + stem + 'Bound')(count, max(values) + 1)
        output = c.create_string_buffer(bound)
        size = getattr(lib, 'meshopt_encode' + stem)(output, bound, indices, count)
    assert size > 0
    return output.raw[:size]


def save_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + '\n')


def embedded(raw):
    return 'data:application/octet-stream;base64,' + base64.b64encode(raw).decode()


source = json.loads((ROOT / 'box/glTF/Box.gltf').read_text())
source_bytes = (ROOT / 'box/glTF/Box0.bin').read_bytes()
for variant, extension, version in [('EXT', 'EXT_meshopt_compression', 0), ('KHR', 'KHR_meshopt_compression', 1)]:
    model = copy.deepcopy(source)
    compressed = bytearray()
    for view in model['bufferViews']:
        stride = view.get('byteStride', 2)
        mode = 'TRIANGLES' if view.get('target') == 34963 else 'ATTRIBUTES'
        raw = source_bytes[view.get('byteOffset', 0):view.get('byteOffset', 0) + view['byteLength']]
        encoded = encode(raw, stride, mode, version)
        view['buffer'] = 1
        view['extensions'] = {extension: dict(buffer=0, byteOffset=len(compressed), byteLength=len(encoded), byteStride=stride, count=len(raw)//stride, mode=mode)}
        compressed.extend(encoded)
    model['buffers'] = [dict(uri='Box.bin', byteLength=len(compressed)), dict(byteLength=len(source_bytes))]
    model['extensionsUsed'] = [extension]
    model['extensionsRequired'] = [extension]
    directory = ROOT / 'box/glTF-Meshopt'
    if variant == 'EXT':
        save_json(directory / 'Box.gltf', model)
        (directory / 'Box.bin').write_bytes(compressed)
        model['buffers'][0]['uri'] = embedded(compressed)
        save_json(directory / 'Box-embedded.gltf', model)
    else:
        del model['buffers'][0]['uri']
        json_bytes = json.dumps(model, separators=(',', ':')).encode()
        json_bytes += b' ' * (-len(json_bytes) % 4)
        binary = bytes(compressed) + b'\0' * (-len(compressed) % 4)
        glb = struct.pack('<III', 0x46546c67, 2, 12+8+len(json_bytes)+8+len(binary))
        glb += struct.pack('<II', len(json_bytes), 0x4e4f534a) + json_bytes
        glb += struct.pack('<II', len(binary), 0x004e4942) + binary
        (directory / 'Box.glb').write_bytes(glb)

# Small independent golden streams exercise codec modes and filters directly.
case_dir = ROOT / 'meshopt'
case_dir.mkdir(exist_ok=True)
cases = []
for name, mode, stride, raw in [
    ('attributes', 'ATTRIBUTES', 12, struct.pack('<9f', 0,0,0, 1,0,0, 0,1,0)),
    ('triangles16', 'TRIANGLES', 2, struct.pack('<6H', 0,1,2, 2,1,3)),
    ('triangles32', 'TRIANGLES', 4, struct.pack('<6I', 0,1,2, 2,1,3)),
    ('indices16', 'INDICES', 2, struct.pack('<4H', 0,2,1,3)),
    ('indices32', 'INDICES', 4, struct.pack('<4I', 0,70000,1,80000)),
]:
    cases.append((name, mode, stride, raw, raw, 'NONE'))
for name, suffix, stride, values in [
    ('oct8', 'Oct', 4, [0,0,1,1, 0,1,0,-1]),
    ('oct16', 'Oct', 8, [0,0,1,1, 1,0,0,-1]),
    ('quaternion', 'Quat', 8, [0,0,0,1, 0,0,1,0]),
    ('color8', 'Color', 4, [1,0,0,1, 0,1,0,1]),
    ('color16', 'Color', 8, [1,0,0,1, 0,0,1,1]),
    ('exponential', 'Exp', 12, [0.25,0.5,1, 2,4,8]),
]:
    count = len(values) // (3 if suffix == 'Exp' else 4)
    buffer = c.create_string_buffer(count * stride)
    floats = (c.c_float * len(values))(*values)
    args = [buffer, count, stride, 16 if stride == 8 else 8, floats]
    if suffix == 'Exp': args = [buffer, count, stride, 24, floats, 0]
    getattr(lib, 'meshopt_encodeFilter' + suffix)(*args)
    encoded_input = buffer.raw
    getattr(lib, 'meshopt_decodeFilter' + suffix)(buffer, count, stride)
    cases.append((name, 'ATTRIBUTES', stride, encoded_input, buffer.raw, {'Oct':'OCTAHEDRAL','Quat':'QUATERNION','Color':'COLOR','Exp':'EXPONENTIAL'}[suffix]))
# The small codec vectors total less than 1 KiB; keep them in one embedded table.
def literal(data):
    return '"' + ''.join('\\x%02x' % byte for byte in data) + '"'

header = [
    '// Copyright 2026 The Defold Foundation',
    '// Licensed under the Defold License version 1.0. See https://www.defold.com/license',
    '// Generated by generate_meshopt.py with Meshoptimizer v1.2. Do not edit.',
    'struct MeshoptTestCase',
    '{',
    '    Codec::MeshoptBuffer m_Buffer;',
    '    uint32_t m_EncodedSize;',
    '    uint32_t m_DecodedSize;',
    '    const char* m_Encoded;',
    '    const char* m_Decoded;',
    '};',
    '',
    'static const MeshoptTestCase MESHOPT_CASES[] =',
    '{',
]
for name, mode, stride, raw, decoded, filter_name in cases:
    encoded = encode(raw, stride, mode)
    header += [
        '    // ' + name,
        '    { { Codec::MODE_%s, Codec::FILTER_%s, Codec::KHR_MESHOPT, %d, %d }, %d, %d,' %
            (mode, filter_name, len(raw)//stride, stride, len(encoded), len(decoded)),
        '      ' + literal(encoded) + ',',
        '      ' + literal(decoded) + ' },',
    ]
header += ['};', '']
(case_dir / 'cases.h').write_text('\n'.join(header))

# A skinned, animated triangle with interleaved positions/UVs, filtered colors
# and rotations, and a sparse morph target. Tests assert the small known arrays.
model = dict(asset=dict(version='2.0'), bufferViews=[], accessors=[])
compressed, decoded = bytearray(), bytearray()
def view(raw, stride, mode='ATTRIBUTES', filter_name='NONE', decoded_raw=None):
    index = len(model['bufferViews'])
    encoded = encode(raw, stride, mode)
    model['bufferViews'].append(dict(buffer=1, byteOffset=len(decoded), byteLength=len(raw),
        extensions={'KHR_meshopt_compression': dict(buffer=0, byteOffset=len(compressed), byteLength=len(encoded), byteStride=stride, count=len(raw)//stride, mode=mode, filter=filter_name)}))
    compressed.extend(encoded)
    decoded.extend(raw if decoded_raw is None else decoded_raw)
    return index

def accessor(view_index, component_type, count, kind, **kwargs):
    index = len(model['accessors'])
    model['accessors'].append(dict(bufferView=view_index, componentType=component_type, count=count, type=kind, **kwargs))
    return index
v = view(struct.pack('<15f', 0,0,0,0,0, 1,0,0,1,0, 0,1,0,0,1), 20)
model['bufferViews'][v]['byteStride'] = 20
position = accessor(v,5126,3,'VEC3',min=[0,0,0],max=[1,1,0])
uv = accessor(v,5126,3,'VEC2',byteOffset=12)
indices = accessor(view(struct.pack('<3H',0,1,2),2,'TRIANGLES'),5123,3,'SCALAR')
joints = accessor(view(bytes(12),4),5121,3,'VEC4')
weights = accessor(view(bytes([255,0,0,0])*3,4),5121,3,'VEC4',normalized=True)
color_case = next(item for item in cases if item[0]=='color8')
color_raw, color_decoded = color_case[3] + color_case[3][:4], color_case[4] + color_case[4][:4]
colors = accessor(view(color_raw,4,filter_name='COLOR',decoded_raw=color_decoded),5121,3,'VEC4',normalized=True)
sparse_indices = view(struct.pack('<H',1),2,'INDICES')
sparse_values = view(struct.pack('<3f',0,0,2),12)
morph = len(model['accessors'])
model['accessors'].append(dict(componentType=5126,count=3,type='VEC3', sparse=dict(count=1,indices=dict(bufferView=sparse_indices,componentType=5123),values=dict(bufferView=sparse_values))))
times = accessor(view(struct.pack('<2f',0,1),4),5126,2,'SCALAR')
quat_case = next(item for item in cases if item[0]=='quaternion')
rotation = accessor(view(quat_case[3],8,filter_name='QUATERNION',decoded_raw=quat_case[4]),5122,2,'VEC4',normalized=True)
exp_case = next(item for item in cases if item[0]=='exponential')
translation = accessor(view(exp_case[3],12,filter_name='EXPONENTIAL',decoded_raw=exp_case[4]),5126,2,'VEC3')
inverse_bind = accessor(view(struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1),64),5126,1,'MAT4')
model.update(nodes=[dict(name='joint'),dict(name='mesh',mesh=0,skin=0)],scenes=[dict(nodes=[0,1])],scene=0,
    skins=[dict(joints=[0],inverseBindMatrices=inverse_bind)],
    meshes=[dict(weights=[0],primitives=[dict(attributes=dict(POSITION=position,TEXCOORD_0=uv,JOINTS_0=joints,WEIGHTS_0=weights,COLOR_0=colors),indices=indices,targets=[dict(POSITION=morph)])])],
    animations=[dict(samplers=[dict(input=times,output=rotation),dict(input=times,output=translation)],channels=[dict(sampler=0,target=dict(node=0,path='rotation')),dict(sampler=1,target=dict(node=0,path='translation'))])],
    buffers=[dict(byteLength=len(compressed),uri=embedded(compressed)),dict(byteLength=len(decoded))],
    extensionsUsed=['KHR_meshopt_compression'],extensionsRequired=['KHR_meshopt_compression'])
# Align all fallback views; each view starts a new allocation when decoded.
# Keep accessor alignment valid even though each decoded view is allocated separately.
aligned = bytearray()
for v in model['bufferViews']:
    raw = decoded[v['byteOffset']:v['byteOffset']+v['byteLength']]
    aligned.extend(b'\0' * (-len(aligned)%4))
    v['byteOffset'] = len(aligned)
    aligned.extend(raw)
model['buffers'][1]['byteLength'] = len(aligned)
save_json(ROOT/'meshopt/streams.gltf',model)
# Malformed decoded indices exercise validation through view overrides.
def replace_payload(model, view_index, payload):
    model = copy.deepcopy(model)
    data = base64.b64decode(model['buffers'][0]['uri'].split(',')[1])
    extension = model['bufferViews'][view_index]['extensions']['KHR_meshopt_compression']
    offset, size = extension['byteOffset'], extension['byteLength']
    data = data[:offset]+payload+data[offset+size:]
    for v in model['bufferViews']:
        e = v['extensions']['KHR_meshopt_compression']
        if e['byteOffset'] > offset: e['byteOffset'] += len(payload)-size
    extension['byteLength'] = len(payload)
    model['buffers'][0] = dict(byteLength=len(data),uri=embedded(data))
    return model
save_json(ROOT/'meshopt/invalid-indices.gltf',replace_payload(model,1,encode(struct.pack('<3H',0,1,3),2,'TRIANGLES')))
save_json(ROOT/'meshopt/invalid-sparse.gltf',replace_payload(model,sparse_indices,encode(struct.pack('<H',3),2,'INDICES')))
