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
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

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
    private static final Pattern EXTENDER_VARIABLE = Pattern.compile("\\{\\{[\\w.]+}}");

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

    private enum VorbisChange { KEEP, EXCLUDE, INCLUDE }

    private static List<?> items(Map<?, ?> context, String key) {
        return context.get(key) instanceof List<?> list ? list : List.of();
    }

    private static Set<String> matchVorbisSymbols(String expression) {
        try {
            if (expression.contains("{{")) {
                // Extender treats template variables as literals when it matches symbols.
                expression = EXTENDER_VARIABLE.matcher(expression).replaceAll(m -> Matcher.quoteReplacement(Pattern.quote(m.group())));
            }
            Pattern pattern = Pattern.compile(expression);
            Set<String> matches = new HashSet<>();
            for (String symbol : List.of(STB, TREMOLO, VORBIS)) {
                if (pattern.matcher(symbol).matches()) {
                    matches.add(symbol);
                }
            }
            return matches;
        } catch (PatternSyntaxException ignored) {
            // Leave incomplete or invalid expressions for manifest validation.
            return Set.of();
        }
    }

    private static String exceptVorbis(String expression) {
        String start = "(?!" + VORBIS + "\\z)(?:" + expression;
        // A valid Java regex may end in a line comment or an unterminated quoted literal.
        for (String end : List.of(")", "\n)", "\\E)")) {
            String narrowed = start + end;
            if (matchVorbisSymbols(narrowed).contains(TREMOLO)) {
                return narrowed;
            }
        }
        return expression;
    }

    private record VorbisFilter(Set<String> matches, Set<String> patternMatches, boolean targetsNewName) {
        static VorbisFilter from(List<?> items, Map<String, Set<String>> patterns) {
            Set<String> matches = new HashSet<>();
            Set<String> patternMatches = new HashSet<>();
            boolean targetsNewName = false;
            for (Object item : items) {
                if (item instanceof String expression) {
                    Set<String> symbols = patterns.computeIfAbsent(expression, AppManifestMigration::matchVorbisSymbols);
                    matches.addAll(symbols);
                    if (!STB.equals(item) && !TREMOLO.equals(item)) {
                        patternMatches.addAll(symbols);
                    }
                    targetsNewName |= symbols.size() == 1 && symbols.contains(VORBIS);
                }
            }
            return new VorbisFilter(matches, patternMatches, targetsNewName);
        }
    }

    private static boolean hasLegacyVorbis(Set<String> excluded, Set<String> included, boolean hasTremolo) {
        return !excluded.contains(STB) || included.contains(STB)
                || (hasTremolo && (!excluded.contains(TREMOLO) || included.contains(TREMOLO)));
    }

    private static String vorbisFamily(String platform) {
        return platform.substring(platform.indexOf('-') + 1);
    }

    private static List<String> vorbisScopes(String platform) {
        String family = vorbisFamily(platform);
        if ("common".equals(platform)) {
            return List.of("common");
        }
        return platform.equals(family) ? List.of("common", family) : List.of("common", family, platform);
    }

    private record VorbisSelection(boolean enabled, boolean enabledAfterRename, boolean explicitNewFilter, boolean excludesNew, boolean includesNew) {
        static VorbisSelection forPlatform(String platform, Map<String, Map<?, ?>> contexts, Map<String, VorbisFilter> exclusions,
                Map<String, VorbisFilter> inclusions) {
            Set<String> excluded = new HashSet<>();
            Set<String> patternExclusions = new HashSet<>();
            Set<String> included = new HashSet<>();
            boolean explicitNewFilter = false;
            boolean defaultTremolo = !STB_ONLY_FAMILIES.contains(vorbisFamily(platform));
            boolean hasTremolo = defaultTremolo;
            for (String scope : vorbisScopes(platform)) {
                if (contexts.containsKey(scope)) {
                    VorbisFilter exclude = exclusions.get(scope);
                    VorbisFilter include = inclusions.get(scope);
                    excluded.addAll(exclude.matches);
                    patternExclusions.addAll(exclude.patternMatches);
                    included.addAll(include.matches);
                    explicitNewFilter |= exclude.targetsNewName || include.targetsNewName;
                    hasTremolo |= items(contexts.get(scope), "symbols").contains(TREMOLO);
                }
            }
            // The next migration will still see the regexes, but the legacy registration names will be gone.
            return new VorbisSelection(hasLegacyVorbis(excluded, included, hasTremolo),
                    hasLegacyVorbis(patternExclusions, included, defaultTremolo), explicitNewFilter,
                    excluded.contains(VORBIS), included.contains(VORBIS));
        }

        VorbisChange change(boolean includedAfterRename) {
            boolean enabledNow = !excludesNew || includedAfterRename;
            if (explicitNewFilter || (enabledNow == enabled && enabledAfterRename == enabled)) {
                return VorbisChange.KEEP;
            }
            return enabled ? VorbisChange.INCLUDE : VorbisChange.EXCLUDE;
        }
    }

    // Extender merges common, family and architecture lists, then applies includeSymbols over excludeSymbols.
    public static Map<String, Map<String, List<?>>> migrateVorbisFilters(Map<String, Map<?, ?>> contexts) {
        Map<String, Set<String>> patterns = new HashMap<>();
        Map<String, VorbisFilter> exclusions = new HashMap<>();
        Map<String, VorbisFilter> inclusions = new HashMap<>();
        for (var entry : contexts.entrySet()) {
            exclusions.put(entry.getKey(), VorbisFilter.from(items(entry.getValue(), "excludeSymbols"), patterns));
            inclusions.put(entry.getKey(), VorbisFilter.from(items(entry.getValue(), "includeSymbols"), patterns));
        }
        if (exclusions.values().stream().noneMatch(filter -> filter.matches.contains(STB) || filter.matches.contains(TREMOLO))) {
            return Map.of();
        }

        Set<String> platforms = new LinkedHashSet<>(contexts.keySet());
        platforms.add("common");
        for (Platform platform : Platform.values()) {
            platforms.add(platform.getExtenderPair());
        }
        for (String platform : List.copyOf(platforms)) {
            platforms.add(vorbisFamily(platform));
        }

        Map<String, VorbisSelection> selections = new LinkedHashMap<>();
        Set<String> narrowInclusions = new HashSet<>();
        for (String platform : platforms) {
            VorbisSelection selection = VorbisSelection.forPlatform(platform, contexts, exclusions, inclusions);
            selections.put(platform, selection);
            if (!selection.explicitNewFilter && (!selection.enabled || !selection.enabledAfterRename) && selection.includesNew) {
                narrowInclusions.addAll(vorbisScopes(platform));
            }
        }

        // An include regex can match Tremolo and the new name on a target which never registered Tremolo.
        // Includes win over exclusions, so exempt the new name while keeping its matches for all other symbols.
        Map<String, List<?>> updatedInclusions = new LinkedHashMap<>();
        for (String scope : narrowInclusions) {
            Map<?, ?> context = contexts.getOrDefault(scope, Map.of());
            List<?> original = items(context, "includeSymbols");
            List<Object> migrated = new ArrayList<>(original);
            for (int i = 0; i < original.size(); ++i) {
                Object item = original.get(i);
                if (item instanceof String expr && patterns.get(expr).equals(Set.of(TREMOLO, VORBIS))) {
                    migrated.set(i, exceptVorbis(expr));
                }
            }
            if (!migrated.equals(original)) {
                updatedInclusions.put(scope, migrated);
                inclusions.put(scope, VorbisFilter.from(migrated, patterns));
            }
        }

        Map<String, VorbisChange> changes = new LinkedHashMap<>();
        for (var entry : selections.entrySet()) {
            String platform = entry.getKey();
            boolean included = vorbisScopes(platform).stream().anyMatch(scope ->
                    inclusions.containsKey(scope) && inclusions.get(scope).matches.contains(VORBIS));
            changes.put(platform, entry.getValue().change(included));
        }

        // Only put a rule on a shared scope if every descendant needs the same change.
        Set<String> split = new HashSet<>();
        for (var entry : changes.entrySet()) {
            String family = vorbisFamily(entry.getKey());
            if (entry.getValue() != changes.get(family)) {
                split.add(family);
            }
            if (entry.getValue() != changes.get("common")) {
                split.add("common");
            }
        }
        Map<String, Map<String, List<?>>> result = new LinkedHashMap<>();
        for (var entry : changes.entrySet()) {
            String platform = entry.getKey();
            String family = vorbisFamily(platform);
            String parent = platform.equals(family) ? "common" : family;
            Map<?, ?> context = contexts.getOrDefault(platform, Map.of());
            List<?> original = items(context, "excludeSymbols");
            boolean apply = !split.contains(platform) && (platform.equals("common") || split.contains(parent)
                    || entry.getValue() != changes.get(parent) || excludesVorbis(original, STB_ONLY_FAMILIES.contains(family)));
            Map<String, List<?>> filters = new LinkedHashMap<>();
            List<?> migrated = replaceVorbisSymbols(original, apply && entry.getValue() == VorbisChange.EXCLUDE);
            if (!migrated.equals(original) && (!context.containsKey("excludeSymbols") || context.get("excludeSymbols") instanceof List<?>)) {
                filters.put("excludeSymbols", migrated);
            }
            List<?> originalIncludes = items(context, "includeSymbols");
            List<?> include = updatedInclusions.getOrDefault(platform, originalIncludes);
            if (apply && entry.getValue() == VorbisChange.INCLUDE
                    && (!context.containsKey("includeSymbols") || context.get("includeSymbols") instanceof List<?>)) {
                List<Object> withVorbis = new ArrayList<>(include);
                withVorbis.add(VORBIS);
                include = withVorbis;
            }
            if (!include.equals(originalIncludes)) {
                filters.put("includeSymbols", include);
            }
            if (!filters.isEmpty()) {
                result.put(platform, filters);
            }
        }
        return result;
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
