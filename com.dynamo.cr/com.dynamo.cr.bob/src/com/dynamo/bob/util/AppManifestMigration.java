// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.util;

import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public final class AppManifestMigration {

    // Shared by Bob and the editor. Only known engine libraries lost their lib
    // prefix; external and custom libraries must retain their original names.
    public static final Map<String, String> WINDOWS_LIBRARY_NAMES;

    static {
        Map<String, String> names = new HashMap<>();
        for (String library : List.of(
                "basis_encoder",
                "basis_encoder_noasan",
                "basis_transcoder",
                "crashext",
                "crashext_null",
                "ddf",
                "ddf_noasan",
                "decoder_ogg",
                "decoder_opus",
                "decoder_wav",
                "dlib",
                "dlib_noasan",
                "dmbedtls",
                "dmbedtls_noasan",
                "engine",
                "engine_release",
                "engine_service",
                "engine_service_null",
                "extension",
                "font",
                "font_skribidi",
                "gameobject",
                "gamesys",
                "gamesys_model",
                "gamesys_model_null",
                "gamesys_rig",
                "gamesys_rig_null",
                "graphics",
                "graphics_dx12",
                "graphics_null",
                "graphics_null_noasan",
                "graphics_opengles",
                "graphics_proto",
                "graphics_proto_noasan",
                "graphics_transcoder_basisu",
                "graphics_transcoder_null",
                "graphics_vulkan",
                "graphics_webgpu",
                "graphics_webgpu_wagyu",
                "gui",
                "hid",
                "hid_null",
                "image",
                "image_noasan",
                "image_null",
                "image_null_noasan",
                "input",
                "launcherutil",
                "liveupdate",
                "liveupdate_null",
                "lua",
                "model",
                "particle",
                "physics",
                "physics_2d",
                "physics_2d_defold",
                "physics_3d",
                "physics_null",
                "platform",
                "platform_null",
                "platform_vulkan",
                "profile",
                "profile_noasan",
                "profile_null",
                "profile_null_noasan",
                "profiler_js",
                "profiler_remotery",
                "profilerext",
                "profilerext_null",
                "record",
                "record_null",
                "render",
                "render_font_default",
                "resource",
                "rig",
                "rig_null",
                "script",
                "script_box2d",
                "script_box2d_defold",
                "sound",
                "sound_nosimd",
                "sound_null",
                "sound_openal",
                "zip",
                "zip_noasan")) {
            addWindowsLibraryNames(names, library, library);
        }
        addWindowsLibraryNames(names, "mbedtls", "dmbedtls");
        addWindowsLibraryNames(names, "mbedtls_noasan", "dmbedtls_noasan");
        WINDOWS_LIBRARY_NAMES = Collections.unmodifiableMap(names);
    }

    private AppManifestMigration() {
    }

    private static void addWindowsLibraryNames(Map<String, String> names, String previousName, String currentName) {
        names.put(previousName, currentName);
        names.put(previousName + ".lib", currentName);
        names.put("lib" + previousName, currentName);
        names.put("lib" + previousName + ".lib", currentName);
    }
}
