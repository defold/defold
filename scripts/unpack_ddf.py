#!/usr/bin/env python
# Copyright 2020-2022 The Defold Foundation
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
        print("No builder registered for filetype %s" %ext)
        sys.exit(1)

    with open(path, 'rb') as f:
        content = f.read()
        obj = builder()
        obj.ParseFromString(content)

        out = text_format.MessageToString(obj)
        print(out)

