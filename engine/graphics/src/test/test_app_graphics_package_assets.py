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

import sys, subprocess, os, platform
import test_app_graphics_package_assets_shared

try:
    import test_app_graphics_package_assets_vendor
    sys.modules['test_app_graphics_package_assets_vendor'] = test_app_graphics_package_assets_vendor
    print("Imported %s from %s" % ('test_app_graphics_package_assets_vendor', test_app_graphics_package_assets_vendor.__file__))
except ModuleNotFoundError as e:
    if "No module named 'test_app_graphics_package_assets_vendor'" in str(e):
        print("Couldn't find test_app_graphics_package_assets_vendor.py. Skipping.")
        pass
    else:
        raise e
except Exception as e:
    print("Failed to import test_app_graphics_package_assets_vendor.py:")
    raise e

if 'test_app_graphics_package_assets_vendor' not in sys.modules:
    class test_app_graphics_package_assets_vendor(object):
        @classmethod
        def is_installed(cls):
            return False
        @classmethod
        def to_vendor(cls):
            return ""

def to_spirv(buffer_name, file_path, profile):

    dynamo_home = os.environ['DYNAMO_HOME']

    platform_str = platform.platform().lower()

    if platform_str.startswith("windows"):
        platform_str = "x86_64-win32"
    elif platform_str.startswith("macos"):
        platform_str = "arm64-macos"

    exe = '%s/ext/bin/%s/glslang' % (dynamo_home, platform_str)

    out_path = file_path + '.spv'

    subprocess.call([exe,
        "-w",
        "--entry-point",
        "main",
        "--auto-map-bindings",
        "--auto-map-locations",
        "-Os",
        "--resource-set-binding",
        "frag",
        "1",
        "-S",
        profile,
        "-o",
        out_path,
        "-V",
        file_path
        ])

    buf = test_app_graphics_package_assets_shared.get_file_contents(out_path)
    output = "const unsigned char %s[] = {%s};" % (buffer_name, ",".join(buf))

    return output

def to_plaintext(buffer_name, file_path):
    buf = test_app_graphics_package_assets_shared.get_file_contents(file_path)
    output = "const unsigned char %s[] = {%s};" % (buffer_name, ",".join(buf))
    return output

def write_header(label, header_path, assets):
    asset_lines = "\n".join(assets)
    header_str = test_app_graphics_package_assets_shared.HEADER_TEMPLATE % (label, label, asset_lines)
    f = open(header_path, "w")
    f.write(header_str)
    f.close()

if __name__ == '__main__':
    write_header(
        "GRAPHICS_ASSETS",
        "test_app_graphics_assets.h",
        [to_plaintext("glsl_vertex_program", "test_app_graphics.vs"),
         to_plaintext("glsl_fragment_program", "test_app_graphics.fs"),
         to_plaintext("glsl_compute_program", "test_app_graphics.cp"),
         to_plaintext("glsl_fragment_program_ssbo", "test_app_graphics_ssbo.fs"),
         to_plaintext("glsl_fragment_program_ubo", "test_app_graphics_ubo.fs"),
         to_plaintext("msl_vertex_program", "test_app_graphics.vs.msl"),
         to_plaintext("msl_fragment_program", "test_app_graphics.fs.msl"),
         to_spirv("spirv_vertex_program", "test_app_graphics.vs", "vert"),
         to_spirv("spirv_fragment_program", "test_app_graphics.fs", "frag"),
         to_spirv("spirv_compute_program", "test_app_graphics.cp", "comp"),
         to_spirv("spirv_fragment_program_ssbo", "test_app_graphics_ssbo.fs", "frag"),
         to_spirv("spirv_fragment_program_ubo", "test_app_graphics_ubo.fs", "frag")])

    if test_app_graphics_package_assets_vendor.is_installed():
        write_header(
            "GRAPHICS_ASSETS_VENDOR",
            "test_app_graphics_assets_vendor.h",
            [test_app_graphics_package_assets_vendor.to_vendor("vendor_vertex_program", "test_app_graphics.vs.spv", "vert"),
             test_app_graphics_package_assets_vendor.to_vendor("vendor_fragment_program", "test_app_graphics.fs.spv", "frag")])
