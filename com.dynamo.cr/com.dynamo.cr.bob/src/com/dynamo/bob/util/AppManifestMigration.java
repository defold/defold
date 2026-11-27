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

import com.dynamo.bob.Platform;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public final class AppManifestMigration {

    // Shared by Bob and the editor. Only known engine libraries lost their lib
    // prefix; external and custom libraries must retain their original names.
    public static final Map<String, String> WINDOWS_LIBRARY_NAMES;

    // GLFW 2 backends are now compiled into the engine platform libraries.
    public static final Map<String, String> NATIVE_PLATFORM_LIBRARY_NAMES = Collections.unmodifiableMap(
            new HashMap<>(Map.of("dmglfw", "platform", "dmglfw_vulkan", "platform_vulkan")));

    private static final Set<String> TREMOLO_LIBRARIES = Set.of("tremolo", "libtremolo", "libtremolo.a", "tremolo.lib", "libtremolo.lib");
    private static final List<String> STB_ONLY_FAMILIES = List.of("web", "win32", "linux");
    private static final String STB = "AudioDecoderStbVorbis";
    private static final String TREMOLO = "AudioDecoderTremolo";
    private static final String VORBIS = "AudioDecoderVorbis";

    static {
        Map<String, String> names = new HashMap<>();
        for (String library : List.of(
                "basis_encoder",
                "basis_encoder_noasan",
                "basis_transcoder",
                "crashext",
                "crashext_null",
                "datastore",
                "datastore_null",
                "ddf",
                "ddf_noasan",
                "decoder_ogg",
                "decoder_ogg_nosimd",
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
                "font_render",
                "font_richtext",
                "font_richtext_null",
                "font_skribidi",
                "gameobject",
                "gamesys",
                "gamesys_gui",
                "gamesys_model",
                "gamesys_model_null",
                "gamesys_particle",
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
                "gui_null",
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
                "particle_null",
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
                // Retained to migrate manifests that selected the removed null backend.
                "record_null",
                "record_script",
                "record_service",
                "render",
                "resource",
                "rig",
                "rig_null",
                "script",
                "script_box2d",
                "script_box2d_defold",
                "script_bullet3d",
                "sound",
                "sound_nosimd",
                "sound_null",
                "sound_openal",
                "zip",
                "zip_noasan")) {
            addWindowsLibraryNames(names, library, library);
        }
        addWindowsLibraryNames(names, "render_font_default", "font_render");
        addWindowsLibraryNames(names, "mbedtls", "dmbedtls");
        addWindowsLibraryNames(names, "mbedtls_noasan", "dmbedtls_noasan");
        WINDOWS_LIBRARY_NAMES = Collections.unmodifiableMap(names);
    }

    // Shared by Bob and the editor: preserve an explicit pre-DX12 Vulkan-only choice.
    public static boolean isLegacyWindowsVulkanSelection(Collection<?> enabled, Collection<?> excluded) {
        Set<String> additions = normalizeGraphicsItems(enabled);
        Set<String> removals = normalizeGraphicsItems(excluded);
        return (additions.contains("graphics_vulkan") || additions.contains("GraphicsAdapterVulkan"))
                && (removals.contains("graphics") || removals.contains("GraphicsAdapterOpenGL"))
                && !additions.contains("graphics_dx12") && !additions.contains("GraphicsAdapterDX12")
                && !removals.contains("graphics_dx12") && !removals.contains("GraphicsAdapterDX12")
                && !removals.contains("graphics_vulkan") && !removals.contains("GraphicsAdapterVulkan");
    }

    private static List<?> replaceVorbisSymbols(List<?> items, boolean addVorbis) {
        addVorbis &= !items.contains(VORBIS);
        if (!items.contains(STB) && !items.contains(TREMOLO) && !addVorbis) {
            return items;
        }
        List<Object> result = new ArrayList<>(items.size());
        for (Object item : items) {
            if (STB.equals(item) || TREMOLO.equals(item)) {
                if (addVorbis) {
                    result.add(VORBIS);
                    addVorbis = false;
                }
            } else {
                result.add(item);
            }
        }
        if (addVorbis) {
            result.add(VORBIS);
        }
        return result;
    }

    // Extender merges platforms.common, the OS family, and the architecture in that order.
    // Apple and Android registered both decoders. Web, Windows, and Linux registered only stb by default.
    public static Map<String, List<?>> migrateVorbisExclusions(Map<String, List<?>> exclusions, Map<String, List<?>> selections) {
        List<?> common = exclusions.getOrDefault("common", List.of());
        Set<String> platforms = new LinkedHashSet<>(exclusions.keySet());
        if (common.contains(STB) && !common.contains(TREMOLO) && !common.contains(VORBIS)) {
            platforms.addAll(STB_ONLY_FAMILIES);
        }
        // An architecture can explicitly link Tremolo on an otherwise stb-only family.
        // In that case only its siblings may inherit the new exclusion.
        Set<String> splitFamilies = new HashSet<>();
        for (var entry : selections.entrySet()) {
            String target = entry.getKey();
            String family = vorbisFamily(target);
            if (!target.equals(family) && STB_ONLY_FAMILIES.contains(family) && entry.getValue().contains(TREMOLO)
                    && vorbisDisabled(family, exclusions, selections) && !vorbisDisabled(target, exclusions, selections)) {
                splitFamilies.add(family);
            }
        }
        if (!splitFamilies.isEmpty()) {
            for (Platform platform : Platform.values()) {
                String target = platform.getExtenderPair();
                if (splitFamilies.contains(vorbisFamily(target))) {
                    platforms.add(target);
                }
            }
        }
        Map<String, List<?>> changes = new LinkedHashMap<>();
        for (String platform : platforms) {
            String family = vorbisFamily(platform);
            String parent = platform.equals(family) ? "common" : family;
            List<?> local = exclusions.getOrDefault(platform, List.of());
            boolean exclude = !splitFamilies.contains(platform) && vorbisDisabled(platform, exclusions, selections)
                    && (platform.equals("common") || splitFamilies.contains(parent) || !vorbisDisabled(parent, exclusions, selections)
                        || excludesVorbis(local, STB_ONLY_FAMILIES.contains(family)));
            List<?> migrated = replaceVorbisSymbols(local, exclude);
            if (!local.equals(migrated)) {
                changes.put(platform, migrated);
            }
        }
        return changes;
    }

    private static String vorbisFamily(String platform) {
        return platform.substring(platform.indexOf('-') + 1);
    }

    private static boolean vorbisDisabled(String platform, Map<String, List<?>> exclusions, Map<String, List<?>> selections) {
        String family = vorbisFamily(platform);
        boolean excludeStb = false;
        boolean excludeTremolo = false;
        boolean hasTremolo = !STB_ONLY_FAMILIES.contains(family);
        for (String scope : List.of("common", family, platform)) {
            List<?> excluded = exclusions.getOrDefault(scope, List.of());
            excludeStb |= excluded.contains(STB);
            excludeTremolo |= excluded.contains(TREMOLO);
            hasTremolo |= selections.getOrDefault(scope, List.of()).contains(TREMOLO);
        }
        return excludeStb && (!hasTremolo || excludeTremolo);
    }

    private static boolean excludesVorbis(Collection<?> symbols, boolean stbOnly) {
        return symbols.contains(VORBIS) || (symbols.contains(STB) && (stbOnly || symbols.contains(TREMOLO)));
    }

    public static boolean selectsScalarVorbis(Object libraries) {
        if (libraries instanceof List<?> items) {
            for (Object item : items) {
                if ("decoder_ogg_nosimd".equals(WINDOWS_LIBRARY_NAMES.get(item)) || "libdecoder_ogg_nosimd.a".equals(item)) {
                    return true;
                }
            }
        }
        return false;
    }

    public static List<?> migrateVorbisItems(String key, List<?> items, boolean expandArchiveExclusions) {
        if ("symbols".equals(key) || "excludeSymbols".equals(key)) {
            boolean select = "symbols".equals(key) && (items.contains(STB) || items.contains(TREMOLO));
            return replaceVorbisSymbols(items, select || excludesVorbis(items, false));
        }
        if ("libs".equals(key) || "engineLibs".equals(key) || "excludeLibs".equals(key)) {
            List<Object> result = new ArrayList<>(items);
            result.removeIf(item -> item instanceof String && TREMOLO_LIBRARIES.contains(item));
            if (expandArchiveExclusions && "excludeLibs".equals(key) && !selectsScalarVorbis(items)) {
                for (Object item : items) {
                    if ("decoder_ogg".equals(WINDOWS_LIBRARY_NAMES.get(item)) || "libdecoder_ogg.a".equals(item)) {
                        result.add(((String) item).replace("decoder_ogg", "decoder_ogg_nosimd"));
                        break;
                    }
                }
            }
            return result;
        }
        return items;
    }

    private static Set<String> normalizeGraphicsItems(Collection<?> items) {
        Set<String> result = new HashSet<>();
        for (Object item : items) {
            if (item instanceof String) {
                String name = (String) item;
                result.add(WINDOWS_LIBRARY_NAMES.getOrDefault(name, name));
            }
        }
        return result;
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
