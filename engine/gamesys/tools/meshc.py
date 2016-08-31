#! /usr/bin/env python

# NOTE: Optimized version of google.protobuf.internal.output_stream.OutputStream monkey patched here!
# Performance problem seems to be due to slow realloc (on windows) used in array.fromstring.
#import output_stream_fast
#import google.protobuf.internal.output_stream
#google.protobuf.internal.output_stream.OutputStream = output_stream_fast.OutputStream

from optparse import OptionParser

'''

from colladaImEx import collada
from optparse import OptionParser
import os
import mesh_ddf_pb2
import sys
import math

def Translate(t, v):
    return [v[0] + t[0], v[1] + t[1], v[2] + t[2]]

def Rotate(r, v):
    c = math.cos(r[3])
    s = math.sin(r[3])
    t = 1 - c
    x = r[0]
    y = r[1]
    z = r[2]
    rot = [[t*x*x +   c, t*x*y - z*s, t*x*z + y*s],
           [t*x*y + z*s, t*y*y +   c, t*y*z - x*s],
           [t*x*z - y*s, t*y*z + x*s, t*z*z +   c]]
    res = [0, 0, 0]
    for row in range(3):
        res[row] = rot[row][0] * v[0] + rot[row][1] * v[1] + rot[row][2] * v[2]
    return res

def Scale(s, v):
    return [v[0] * s[0], v[1] * s[1], v[2] * s[2]]

def Transform(transforms, v):
    rev_transforms = list(transforms)
    rev_transforms.reverse()
    for transform in rev_transforms:
        if transform[0] == "translate":
            v = Translate(transform[1], v)
        elif transform[0] == "rotate":
            v = Rotate(transform[1], v)
        elif transform[0] == "scale":
            v = Scale(transform[1], v)
    return v

def Compile(file_name, output_file):
    doc = collada.DaeDocument(False)
    doc.LoadDocumentFromFile(file_name)

    mesh = mesh_ddf_pb2.MeshDesc()
    mesh.primitive_type = mesh_ddf_pb2.MeshDesc.TRIANGLES

    if len(doc.visualScenesLibrary.items) != 1:
        print("ERROR: Only one scene is supported.")
        sys.exit(1)

    scene = doc.visualScenesLibrary.items[0]

    for node in scene.nodes:
        for geom in node.iGeometries:
            dae_geom = geom.object

            if not isinstance(dae_geom.data,collada.DaeMesh):
                continue

            comp = mesh.components.add()
            comp.name = dae_geom.name

            dae_mesh = dae_geom.data
            dae_vertices = dae_mesh.vertices

            sources = dict()

            for source in dae_mesh.sources:
                sources[source.id] = source.vectors

            if len(dae_mesh.primitives) > 1:
                print("ERROR: Only one primitive is supported.")
                sys.exit(1)

            primitive = dae_mesh.primitives[0]
            triangles = None
            if isinstance(primitive, collada.DaeTriangles):
                triangles = primitive.triangles
            else:
                if isinstance(primitive, collada.DaePolylist):
                    triangles = primitive.polygons
                    for vc in primitive.vcount:
                        if vc != 3:
                            triangles = None

            if not triangles:
                print("ERROR: Only triangles supported (%s)." % str(primitive))
                sys.exit(1)

            stride = primitive.GetStride()
            vpos_input = dae_vertices.FindInput("POSITION")
            vertex_input = primitive.FindInput("VERTEX")
            normal_input = primitive.FindInput("NORMAL")
            if normal_input == None:
                normal_input = dae_vertices.FindInput("NORMAL")
            texcoord_input = primitive.FindInput("TEXCOORD")

            positions = sources[vpos_input.source]
            normals = sources[normal_input.source]
            if texcoord_input:
                texcoords = sources[texcoord_input.source]

            # TODO: This is not optimal. Vertices are unique and never reused.
            comp.primitive_count = primitive.count
            for i in range(primitive.count):
                idx = i * stride * 3 + vertex_input.offset
                i1 = triangles[idx + stride * 0]
                i2 = triangles[idx + stride * 1]
                i3 = triangles[idx + stride * 2]

                offset = 0
                if normal_input.offset != None:
                    offset = normal_input.offset
                nidx = i * stride * 3 + offset
                ni1 = triangles[nidx + stride * 0]
                ni2 = triangles[nidx + stride * 1]
                ni3 = triangles[nidx + stride * 2]

                if texcoord_input:
                    texcoordidx = i * stride * 3 + texcoord_input.offset
                    texcoordi1 = triangles[texcoordidx + stride * 0]
                    texcoordi2 = triangles[texcoordidx + stride * 1]
                    texcoordi3 = triangles[texcoordidx + stride * 2]

                comp.indices.append(i*3 + 0)
                comp.indices.append(i*3 + 1)
                comp.indices.append(i*3 + 2)

                for j in [i1,i2,i3]:
                    position = [positions[j][0] * doc.asset.unit.meter, positions[j][1] * doc.asset.unit.meter, positions[j][2] * doc.asset.unit.meter]
                    position = Transform(node.transforms, position)
                    comp.positions.append(position[0])
                    comp.positions.append(position[1])
                    comp.positions.append(position[2])

                for j in [ni1,ni2,ni3]:
                    normal = [normals[j][0], normals[j][1], normals[j][2]]
                    transforms = list(node.transforms)
                    transforms.reverse()
                    for transform in transforms:
                        if transform[0] == "rotate":
                            normal = Rotate(transform[1], normal)
                    comp.normals.append(normal[0])
                    comp.normals.append(normal[1])
                    comp.normals.append(normal[2])

                if texcoord_input:
                    for j in [texcoordi1,texcoordi2,texcoordi3]:
                        comp.texcoord0.append(texcoords[j][0])
                        comp.texcoord0.append(texcoords[j][1])

    f = open(output_file, "wb")
    f.write(mesh.SerializeToString())
    f.close()
'''

if __name__ == "__main__":
    usage = "usage: %prog [options] file"
    parser = OptionParser(usage)
    parser.add_option("-o", dest="output_file", help="Output file", metavar="OUTPUT")
    (options, args) = parser.parse_args()
    if not options.output_file:
        parser.error("Output file not specified (-o)")

#    Compile(sys.argv[1], options.output_file)
    f = open(options.output_file, "wb")
    f.write("dummy")
    f.close()


