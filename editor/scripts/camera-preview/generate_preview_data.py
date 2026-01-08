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


import sys
import platform
import os
import subprocess

# note! this script must be run from this folder

def get_blender_bin():
    host_platform = platform.system()
    if host_platform == "Darwin":
        return "/Applications/Blender.app/Contents/MacOS/Blender"
    else:
        print("Unable to find blender installation")
        os.exit(-1)

def run_blender_script(path, args):
    blender_bin = get_blender_bin()
    subprocess.run([blender_bin, '-b', '-P', path, "--"] + args)

def main():
    scene_input  = "preview_camera_scene.blend"
    scene_output = "../../resources/meshes/camera-preview-edge-list.json"
    run_blender_script("generate_preview_data_blender.py", [scene_input, scene_output])

if __name__ == '__main__':
    main()
