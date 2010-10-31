#! /usr/bin/env python

# NOTE: Optimized version of google.protobuf.internal.output_stream.OutputStream monkey patched here!
# Performance problem seems to be due to slow realloc (on windows) used in array.fromstring.
import output_stream_fast
import google.protobuf.internal.output_stream
google.protobuf.internal.output_stream.OutputStream = output_stream_fast.OutputStream

import sys, os
from optparse import OptionParser
import mesh_ddf_pb2
import gamemap_ddf_pb2

def Compile(file_name, output_file):
    
    gamemap = gamemap_ddf_pb2.GameMapDesc()
    
    f = open(sys.argv[1], "rb")
    gamemap.ParseFromString(f.read())
    f.close()

    mesh = mesh_ddf_pb2.Mesh()
#    mesh.Name = dae_geom.name
    mesh.PrimitiveType = mesh_ddf_pb2.Mesh.TRIANGLES 
    mesh.Name = gamemap.Name
    mesh.PrimitiveCount = gamemap.Height*gamemap.Width*2


    size = gamemap.TileSize
    y = -0.5
    z = 0.1
    o = 0
    
    v = 0
    us = 0.5
    vs = 1 
    for j in range(gamemap.Height):
        x = -0.5
        for i in range(gamemap.Width):
            u = 0.5 * gamemap.Map[o]

            mesh.Indices.append(o*4 + 0)
            mesh.Indices.append(o*4 + 1)
            mesh.Indices.append(o*4 + 2)
    
            mesh.Indices.append(o*4 + 2)
            mesh.Indices.append(o*4 + 3)
            mesh.Indices.append(o*4 + 0)
    
            mesh.Positions.append(0+x)
            mesh.Positions.append(0+y)
            mesh.Positions.append(z)
            mesh.Texcoord0.append(u)
            mesh.Texcoord0.append(v)
        
            mesh.Positions.append(size+x)
            mesh.Positions.append(0+y)
            mesh.Positions.append(z)
            mesh.Texcoord0.append(u+us)
            mesh.Texcoord0.append(v)
        
            mesh.Positions.append(size+x)
            mesh.Positions.append(size+y)
            mesh.Positions.append(z)
            mesh.Texcoord0.append(u+us)
            mesh.Texcoord0.append(v+vs)
    
            mesh.Positions.append(0+x)
            mesh.Positions.append(size+y)
            mesh.Positions.append(z)
            mesh.Texcoord0.append(u)
            mesh.Texcoord0.append(v+vs)

            x = x + size
            o = o + 1
        y = y + size


#    print gamemap

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
