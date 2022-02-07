#!/usr/bin/env python

# https://developers.google.com/protocol-buffers/docs/pythontutorial
# https://blog.conan.io/2019/03/06/Serializing-your-data-with-Protobuf.html

import os, sys

from google.protobuf import text_format
import google.protobuf.message

import gamesys.texture_set_ddf_pb2
import rig.rig_ddf_pb2

BUILDERS = {}
BUILDERS['.texturesetc']    = gamesys.texture_set_ddf_pb2.TextureSet
BUILDERS['.meshsetc']       = rig.rig_ddf_pb2.MeshSet
BUILDERS['.animationsetc']  = rig.rig_ddf_pb2.AnimationSet
BUILDERS['.rigscenec']      = rig.rig_ddf_pb2.RigScene
BUILDERS['.skeletonc']      = rig.rig_ddf_pb2.Skeleton

if __name__ == "__main__":
    path = sys.argv[1]

    _, ext = os.path.splitext(path)
    builder = BUILDERS.get(ext, None)
    if builder is None:
        print "No builder registered for filetype %s" %ext
        sys.exit(1)

    with open(path, 'rb') as f:
        content = f.read()
        obj = builder()
        obj.ParseFromString(content)

        out = text_format.MessageToString(obj)
        print out

