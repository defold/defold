#!/usr/bin/env python
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
