#! /usr/bin/env python

# NOTE: Optimized version of google.protobuf.internal.output_stream.OutputStream monkey patched here!
# Performance problem seems to be due to slow realloc (on windows) used in array.fromstring.
#import output_stream_fast
#import google.protobuf.internal.output_stream
#google.protobuf.internal.output_stream.OutputStream = output_stream_fast.OutputStream

from colladaImEx import collada
from optparse import OptionParser
import os
import mesh_ddf_pb2
import sys

def Compile(file_name, output_file):
    doc = collada.DaeDocument(False)
    doc.LoadDocumentFromFile(file_name)

    mesh = mesh_ddf_pb2.MeshDesc()
    mesh.PrimitiveType = mesh_ddf_pb2.MeshDesc.TRIANGLES

    for dae_geom in doc.geometriesLibrary.items:
        if not isinstance(dae_geom.data,collada.DaeMesh):
            continue

        comp = mesh.Components.add()
        comp.Name = dae_geom.name

        dae_mesh = dae_geom.data
        dae_vertices = dae_mesh.vertices

        sources = dict()

        for source in dae_mesh.sources:
            sources[source.id] = source.vectors

        if len(dae_mesh.primitives) > 1:
            print("ERROR: Only one primitive is supported. ")
            sys.exit(1)

        primitive = dae_mesh.primitives[0]
        if not isinstance(primitive, collada.DaeTriangles):
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
        comp.PrimitiveCount = primitive.count
        for i in range(primitive.count):
            idx = i * stride * 3 + vertex_input.offset
            i1 = primitive.triangles[idx + stride * 0]
            i2 = primitive.triangles[idx + stride * 1]
            i3 = primitive.triangles[idx + stride * 2]

            offset = 0
            if normal_input.offset != None:
                offset = normal_input.offset
            nidx = i * stride * 3 + offset
            ni1 = primitive.triangles[nidx + stride * 0]
            ni2 = primitive.triangles[nidx + stride * 1]
            ni3 = primitive.triangles[nidx + stride * 2]

            if texcoord_input:
                texcoordidx = i * stride * 3 + texcoord_input.offset
                texcoordi1 = primitive.triangles[texcoordidx + stride * 0]
                texcoordi2 = primitive.triangles[texcoordidx + stride * 1]
                texcoordi3 = primitive.triangles[texcoordidx + stride * 2]

            comp.Indices.append(i*3 + 0)
            comp.Indices.append(i*3 + 1)
            comp.Indices.append(i*3 + 2)

            for j in [i1,i2,i3]:
                comp.Positions.append(positions[j][0] * doc.asset.unit.meter)
                comp.Positions.append(positions[j][1] * doc.asset.unit.meter)
                comp.Positions.append(positions[j][2] * doc.asset.unit.meter)

            for j in [ni1,ni2,ni3]:
                comp.Normals.append(normals[j][0])
                comp.Normals.append(normals[j][1])
                comp.Normals.append(normals[j][2])

            if texcoord_input:
                for j in [texcoordi1,texcoordi2,texcoordi3]:
                    comp.Texcoord0.append(texcoords[j][0])
                    comp.Texcoord0.append(texcoords[j][1])

    f = open(output_file, "wb")
    f.write(mesh.SerializeToString())
    f.close()

if __name__ == "__main__":
    usage = "usage: %prog [options] file"
    parser = OptionParser(usage)
    parser.add_option("-o", dest="output_file", help="Output file", metavar="OUTPUT")
    (options, args) = parser.parse_args()
    if not options.output_file:
        parser.error("Output file not specified (-o)")

    Compile(sys.argv[1], options.output_file)
