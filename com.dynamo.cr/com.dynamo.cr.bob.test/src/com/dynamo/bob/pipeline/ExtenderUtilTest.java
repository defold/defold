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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.yaml.snakeyaml.Yaml;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Platform;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.defold.extender.client.ExtenderResource;

public class ExtenderUtilTest {

    private DefaultFileSystem fileSystem;
    private Project project;
    private File tmpDir;

    private void createDirs(DefaultFileSystem fileSystem, String path) {
        File dir = new File(tmpDir, path);
        dir.mkdirs();
    }

    private void createFile(DefaultFileSystem fileSystem, String path, byte[] data) throws IOException {
        File f = new File(tmpDir, path);
        f.getParentFile().mkdirs();
        Files.write(f.toPath(), data);
    }


    @Before
    public void setUp() throws Exception {

        tmpDir = Files.createTempDirectory("defold_").toFile();

        fileSystem = new DefaultFileSystem();
        createFile(fileSystem, "extension1/ext.manifest", "name: Extension1\n".getBytes());
        createFile(fileSystem, "extension1/src/ext1.cpp", "// ext1.cpp".getBytes());
        createFile(fileSystem, "extension1/res/android/res/values/values.xml", "<xml>/<xml>".getBytes());

        createFile(fileSystem, "extension2/ext.manifest", "name: Extension2\n".getBytes());
        createFile(fileSystem, "extension2/src/ext1.cpp", "// ext2.cpp".getBytes());
        createFile(fileSystem, "extension2/res/android/res/com.foo.org/values/values.xml", "<xml>/<xml>".getBytes());

        createFile(fileSystem, "extension3/ext.manifest", "name: Extension3\n".getBytes());
        createFile(fileSystem, "extension3/src/ext1.cpp", "// ext3.cpp".getBytes());
        createFile(fileSystem, "extension3/res/android/res/com.foo.org/values/values.xml", "<xml>/<xml>".getBytes());
        createFile(fileSystem, "extension3/res/android/res/com.bar.org/values/values.xml", "<xml>/<xml>".getBytes());

        createDirs(fileSystem, "notextension/res/android/res/bla");

        createFile(fileSystem, "bundle1/armv7-android/res/values/strings.xml", "<xml>/<xml>".getBytes());
        createFile(fileSystem, "bundle2/arm64-android/res/values/strings.xml", "<xml>/<xml>".getBytes());

        createFile(fileSystem, "game.project", "[project]\nbundle_resources = /bundle1,/bundle2".getBytes());

        project = new Project(fileSystem, tmpDir.getAbsolutePath(), "build/default");

        project.loadProjectFile(true);
    }

    @After
    public void tearDown() throws Exception {
        project.dispose();
    }
    @Test
    public void testIsAndroidAssetDirectory() throws Exception {
        assertTrue(ExtenderUtil.isAndroidAssetDirectory(project, "extension1/res/android/res/"));
        assertFalse(ExtenderUtil.isAndroidAssetDirectory(project, "extension2/res/android/res/"));
    }

    @Test
    public void testGetAndroidResources() throws Exception {
        Map<String, IResource> resources = ExtenderUtil.getAndroidResources(project);
        for (String key : resources.keySet()) {
            IResource r = resources.get(key);
            System.out.printf("key: %s -> %s\n", key, r.getAbsPath());
        }
        assertEquals(6, resources.size());
        assertTrue(resources.containsKey("extension1/values/values.xml"));
        assertTrue(resources.containsKey("extension2/com.foo.org/values/values.xml"));
        assertTrue(resources.containsKey("extension3/com.foo.org/values/values.xml"));
        assertTrue(resources.containsKey("extension3/com.bar.org/values/values.xml"));
        // Check bundle resources
        assertTrue(resources.containsKey("bundle1/values/strings.xml"));
        assertTrue(resources.containsKey("bundle2/values/strings.xml"));
    }

    // Verifies a project containing native extensions reports them globally and
    // for the tested Android and Linux platforms.
    @Test
    public void testNativeExtensionsAreDetectedForEveryPlatform() {
        assertTrue(ExtenderUtil.hasNativeExtensions(project));
        assertTrue(ExtenderUtil.hasNativeExtensions(project, Platform.Arm64Android));
        assertTrue(ExtenderUtil.hasNativeExtensions(project, Platform.X86_64Linux));
    }

    private Project createR8Project(String gameProject, boolean customRules) throws Exception {
        File projectDir = Files.createTempDirectory("defold_r8_").toFile();
        Files.write(new File(projectDir, "game.project").toPath(), gameProject.getBytes(StandardCharsets.UTF_8));

        File builtInRules = new File(projectDir, "builtins/manifests/android/dmengine.keep");
        builtInRules.getParentFile().mkdirs();
        Files.write(builtInRules.toPath(), "-keep class com.dynamo.android.DefoldActivity { *; }\n".getBytes(StandardCharsets.UTF_8));

        if (customRules) {
            Files.write(new File(projectDir, "custom.keep").toPath(), "-keep class example.Custom\n".getBytes(StandardCharsets.UTF_8));
            Files.write(new File(projectDir, "custom.pro").toPath(), "-keep class example.Legacy\n".getBytes(StandardCharsets.UTF_8));
        }

        DefaultFileSystem r8FileSystem = new DefaultFileSystem();
        Project r8Project = new Project(r8FileSystem, projectDir.getAbsolutePath(), "build/default");
        r8Project.loadProjectFile(true);
        return r8Project;
    }

    private ExtenderResource findResource(List<ExtenderResource> resources, String path) {
        for (ExtenderResource resource : resources) {
            if (path.equals(resource.getPath())) {
                return resource;
            }
        }
        return null;
    }

    // Verifies built-in R8 keep rules select Extender only for Android and are
    // uploaded solely at the canonical R8 rules path.
    @Test
    public void testR8KeepRulesSelectExtenderAndUploadBuiltinRules() throws Exception {
        Project r8Project = createR8Project(
                "[android]\nr8_keep_rules = /builtins/manifests/android/dmengine.keep\n",
                false);
        try {
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project));
            assertTrue(ExtenderUtil.hasNativeExtensions(r8Project, Platform.Arm64Android));
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project, Platform.X86_64Linux));
            List<ExtenderResource> androidResources = ExtenderUtil.getExtensionSources(r8Project, Platform.Arm64Android, null);
            List<ExtenderResource> linuxResources = ExtenderUtil.getExtensionSources(r8Project, Platform.X86_64Linux, null);
            ExtenderResource appRules = findResource(androidResources, ExtenderUtil.r8KeepRulesPath);
            assertNotNull(appRules);
            assertEquals("-keep class com.dynamo.android.DefoldActivity { *; }\n", new String(appRules.getContent(), StandardCharsets.UTF_8));
            assertNull(findResource(androidResources, "_app/dmengine.keep"));
            assertNull(findResource(linuxResources, ExtenderUtil.r8KeepRulesPath));
        } finally {
            r8Project.dispose();
        }
    }

    // Verifies custom R8 keep rules replace the built-in contents while retaining the canonical upload path.
    @Test
    public void testCustomR8KeepRulesReplaceBuiltinRules() throws Exception {
        Project r8Project = createR8Project("[android]\nr8_keep_rules = /custom.keep\n", true);
        try {
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project));
            assertTrue(ExtenderUtil.hasNativeExtensions(r8Project, Platform.Arm64Android));
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project, Platform.X86_64Linux));
            List<ExtenderResource> resources = ExtenderUtil.getExtensionSources(r8Project, Platform.Arm64Android, null);
            ExtenderResource appRules = findResource(resources, ExtenderUtil.r8KeepRulesPath);
            assertNotNull(appRules);
            assertEquals("-keep class example.Custom\n", new String(appRules.getContent(), StandardCharsets.UTF_8));
            assertNull(findResource(resources, "_app/dmengine.keep"));
        } finally {
            r8Project.dispose();
        }
    }

    // Verifies the removed android.proguard property neither selects Extender nor uploads R8 keep rules.
    @Test
    public void testRemovedProGuardPropertyDoesNotForceNativeBuild() throws Exception {
        Project r8Project = createR8Project("[android]\nproguard = /custom.pro\n", true);
        try {
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project));
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project, Platform.Arm64Android));
            List<ExtenderResource> resources = ExtenderUtil.getExtensionSources(r8Project, Platform.Arm64Android, null);
            assertNull(findResource(resources, ExtenderUtil.r8KeepRulesPath));
        } finally {
            r8Project.dispose();
        }
    }

    // Verifies an empty android.r8_keep_rules value is treated as absent and does not select Extender.
    @Test
    public void testEmptyR8KeepRulesDoNotForceNativeBuild() throws Exception {
        Project r8Project = createR8Project("[android]\nr8_keep_rules =\n", false);
        try {
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project));
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project, Platform.Arm64Android));
        } finally {
            r8Project.dispose();
        }
    }

    // Verifies a missing R8 rules resource selects Extender only for Android and
    // reports the missing resource during source collection.
    @Test
    public void testMissingR8KeepRulesStillSelectExtenderAndReportResourceError() throws Exception {
        Project r8Project = createR8Project("[android]\nr8_keep_rules = /missing.keep\n", false);
        try {
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project));
            assertTrue(ExtenderUtil.hasNativeExtensions(r8Project, Platform.Arm64Android));
            assertFalse(ExtenderUtil.hasNativeExtensions(r8Project, Platform.X86_64Linux));
            assertNull(findResource(
                    ExtenderUtil.getExtensionSources(r8Project, Platform.X86_64Linux, null),
                    ExtenderUtil.r8KeepRulesPath));
            try {
                ExtenderUtil.getExtensionSources(r8Project, Platform.Arm64Android, null);
                throw new AssertionError("Expected missing R8 rules to fail");
            } catch (CompileExceptionError e) {
                assertTrue(e.getMessage().contains("No such resource: android.r8_keep_rules: /missing.keep"));
            }
        } finally {
            r8Project.dispose();
        }
    }

    @Test
    public void testSimulatorGraphicsMigratesBeforeUpload() throws Exception {
        project.getProjectProperties().putStringValue("ios", "privacymanifest", "");
        String yaml = "platforms:\n  arm64_sim-ios:\n    context:\n"
                + "      libs: [graphics_metal, custom]\n"
                + "      excludeLibs: [graphics_metal]\n"
                + "      excludeSymbols: [GraphicsAdapterMetal]\n"
                + "  arm64-ios:\n    context:\n      excludeLibs: [graphics_metal]\n";
        byte[] original = yaml.getBytes(StandardCharsets.UTF_8);
        createFile(fileSystem, "simulator.appmanifest", original);
        project.getProjectProperties().putStringValue("native_extension", "app_manifest", "simulator.appmanifest");
        byte[] migrated = findResource(ExtenderUtil.getExtensionSources(project, Platform.Arm64IosSim, null),
                ExtenderUtil.appManifestPath).getContent();
        String expected = "platforms:\n  arm64_sim-ios:\n    context:\n"
                + "      libs: [custom]\n      excludeLibs: []\n      excludeSymbols: []\n"
                + "  arm64-ios:\n    context:\n      excludeLibs: [graphics_metal]\n";
        assertEquals((Object) new Yaml().load(expected), (Object) new Yaml().load(new String(migrated, StandardCharsets.UTF_8)));
        assertArrayEquals(original, project.getResource("simulator.appmanifest").getContent());
        createFile(fileSystem, "simulator.appmanifest", migrated);
        assertArrayEquals(migrated, findResource(ExtenderUtil.getExtensionSources(project, Platform.Arm64IosSim, null),
                ExtenderUtil.appManifestPath).getContent());
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testLegacyMacOSVulkanPlatformMigratesBeforeUpload() throws Exception {
        project.getProjectProperties().putStringValue("osx", "privacymanifest", "");
        for (String platform : List.of("osx", "arm64-osx", "x86_64-osx")) {
            for (String key : List.of("libs", "engineLibs")) {
                String yaml = "platforms:\n  " + platform + ":\n    context:\n"
                        + "      " + key + ": [graphics_vulkan, platform_vulkan, MoltenVK, custom]\n"
                        + "      excludeLibs: [graphics_metal]\n";
                byte[] original = yaml.getBytes(StandardCharsets.UTF_8);
                createFile(fileSystem, "legacy-macos.appmanifest", original);
                project.getProjectProperties().putStringValue("native_extension", "app_manifest", "legacy-macos.appmanifest");
                byte[] migrated = findResource(ExtenderUtil.getExtensionSources(project, Platform.Arm64MacOS, null),
                        ExtenderUtil.appManifestPath).getContent();
                Map<String, Object> expected = new Yaml().load(yaml);
                Map<String, Object> platforms = (Map<String, Object>) expected.get("platforms");
                Map<String, Object> context = (Map<String, Object>) ((Map<String, Object>) platforms.get(platform)).get("context");
                context.put("excludeLibs", List.of("graphics_metal", "platform"));
                assertEquals(expected, new Yaml().load(new String(migrated, StandardCharsets.UTF_8)));
                assertArrayEquals(original, project.getResource("legacy-macos.appmanifest").getContent());
                createFile(fileSystem, "legacy-macos.appmanifest", migrated);
                assertArrayEquals(migrated, findResource(ExtenderUtil.getExtensionSources(project, Platform.Arm64MacOS, null),
                        ExtenderUtil.appManifestPath).getContent());
            }
        }
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testLegacyWindowsAppManifestLibrariesAreMigratedBeforeUpload() throws Exception {
        String libraries = "[libphysics, libphysics_3d.lib, profile_null.lib, "
                + "record_script, record_script.lib, librecord_script, librecord_script.lib, "
                + "librender_font_default, librender_font_default.lib, render_font_default.lib, render_font_default, "
                + "libmbedtls, libmbedtls.lib, mbedtls.lib, mbedtls, "
                + "libmbedtls_noasan, libmbedtls_noasan.lib, mbedtls_noasan.lib, mbedtls_noasan, "
                + "libdmbedtls.lib, libdmbedtls_noasan, libfont_render, libgameobject.lib, "
                + "physics, libbox2d_defold, libopus.lib, vpx, vulkan-1, libcustom.lib, null, 42]";
        List<Object> expectedLibraries = Arrays.asList(
                "physics", "physics_3d", "profile_null",
                "record_script", "record_script", "record_script", "record_script",
                "font_render", "font_render", "font_render", "font_render",
                "dmbedtls", "dmbedtls", "dmbedtls", "dmbedtls",
                "dmbedtls_noasan", "dmbedtls_noasan", "dmbedtls_noasan", "dmbedtls_noasan",
                "dmbedtls", "dmbedtls_noasan", "font_render", "gameobject",
                "physics", "libbox2d_defold", "libopus.lib", "vpx", "vulkan-1", "libcustom.lib", null, 42);
        StringBuilder manifestYaml = new StringBuilder("context:\n    libs: " + libraries + "\nplatforms:\n");
        for (String platform : List.of("win32", "x86-win32", "x86_64-win32", "common", "x86_64-linux")) {
            manifestYaml.append("    ").append(platform).append(":\n        context:\n");
            for (String key : List.of("excludeLibs", "libs", "engineLibs", "symbols")) {
                manifestYaml.append("            ").append(key).append(": ").append(libraries).append("\n");
            }
        }
        byte[] originalContent = manifestYaml.toString().getBytes(StandardCharsets.UTF_8);
        createFile(fileSystem, "legacy-windows.appmanifest", originalContent);
        project.getProjectProperties().putStringValue("native_extension", "app_manifest", "legacy-windows.appmanifest");

        ExtenderResource uploadedResource = findResource(
                ExtenderUtil.getExtensionSources(project, Platform.X86_64Win32, null), ExtenderUtil.appManifestPath);
        byte[] migratedContent = uploadedResource.getContent();
        Map<String, Object> manifest = new Yaml().load(new String(migratedContent, StandardCharsets.UTF_8));
        Map<String, Object> original = new Yaml().load(manifestYaml.toString());
        Map<String, Object> platforms = (Map<String, Object>) manifest.get("platforms");
        Map<String, Object> originalPlatforms = (Map<String, Object>) original.get("platforms");
        for (String platform : List.of("win32", "x86-win32", "x86_64-win32")) {
            Map<String, Object> context = (Map<String, Object>) ((Map<String, Object>) platforms.get(platform)).get("context");
            for (String key : List.of("excludeLibs", "libs", "engineLibs")) {
                assertEquals(platform + "/" + key, expectedLibraries, context.get(key));
            }
            assertEquals(new Yaml().load(libraries), context.get("symbols"));
        }
        assertEquals(original.get("context"), manifest.get("context"));
        assertEquals(originalPlatforms.get("common"), platforms.get("common"));
        assertEquals(originalPlatforms.get("x86_64-linux"), platforms.get("x86_64-linux"));
        assertArrayEquals(originalContent, project.getResource("legacy-windows.appmanifest").getContent());

        createFile(fileSystem, "current-windows.appmanifest", migratedContent);
        project.getProjectProperties().putStringValue("native_extension", "app_manifest", "current-windows.appmanifest");
        ExtenderResource currentResource = findResource(
                ExtenderUtil.getExtensionSources(project, Platform.X86_64Win32, null), ExtenderUtil.appManifestPath);
        assertArrayEquals(migratedContent, currentResource.getContent());
    }

    // Verifies legacy mobile manifests upload native platform libraries while preserving custom and desktop inputs.
    @SuppressWarnings("unchecked")
    @Test
    public void testLegacyMobilePlatformLibrariesMigrateBeforeUpload() throws Exception {
        for (String platform : List.of("android", "armv7-android", "arm64-android", "x86_64-android",
                "ios", "arm64-ios", "arm64_sim-ios", "web", "wasm-web", "wasm_pthread-web")) {
            boolean simulator = platform.equals("arm64_sim-ios");
            String yaml = "platforms:\n  " + platform + ":\n    context:\n"
                    + "      libs: [dmglfw, custom]\n"
                    + "      excludeLibs: [dmglfw_vulkan]\n"
                    + "      engineLibs: " + (simulator ? "[dmglfw, custom]\n" : "[dmglfw, 42, null]\n")
                    + "  x86_64-linux: {context: {libs: [dmglfw]}}\n";
            byte[] original = yaml.getBytes(StandardCharsets.UTF_8);
            createFile(fileSystem, "legacy-mobile.appmanifest", original);
            ExtenderUtil.FSAppManifestResource resource = new ExtenderUtil.FSAppManifestResource(
                    project.getResource("legacy-mobile.appmanifest"), project.getRootDirectory(), ExtenderUtil.appManifestPath, null);
            byte[] migrated = resource.getContent();
            Map<String, Object> manifest = new Yaml().load(new String(migrated, StandardCharsets.UTF_8));
            Map<String, Object> platforms = (Map<String, Object>) manifest.get("platforms");
            Map<String, Object> context = (Map<String, Object>) ((Map<String, Object>) platforms.get(platform)).get("context");
            assertEquals(List.of("platform", "custom"), context.get("libs"));
            assertEquals(simulator ? List.of() : List.of("platform_vulkan"), context.get("excludeLibs"));
            assertEquals(simulator ? List.of("platform", "custom") : Arrays.asList("platform", 42, null), context.get("engineLibs"));
            assertEquals(Map.of("context", Map.of("libs", List.of("dmglfw"))), platforms.get("x86_64-linux"));
            assertArrayEquals(original, project.getResource("legacy-mobile.appmanifest").getContent());
            createFile(fileSystem, "current-mobile.appmanifest", migrated);
            assertArrayEquals(migrated, new ExtenderUtil.FSAppManifestResource(
                    project.getResource("current-mobile.appmanifest"), project.getRootDirectory(), ExtenderUtil.appManifestPath, null).getContent());
        }
    }

    // Verifies legacy recording exclusions strip the Lua API, backends, service
    // and Linux encoder before upload, without changing source files or migrating twice.
    @SuppressWarnings("unchecked")
    @Test
    public void testLegacyRecordingExclusionsMigrateBeforeUpload() throws Exception {
        for (String platform : List.of("root", "common", "osx", "arm64-osx", "x86_64-osx",
                "linux", "x86_64-linux", "arm64-linux", "win32", "x86-win32", "x86_64-win32",
                "ios", "arm64-ios", "arm64_sim-ios", "android", "armv7-android", "arm64-android",
                "x86_64-android", "web", "wasm-web", "wasm_pthread-web")) {
            for (String vpx : List.of("vpx", "vpx.lib", "libvpx", "libvpx.lib")) {
                boolean windows = platform.endsWith("win32");
                boolean linux = List.of("root", "common", "linux", "x86_64-linux", "arm64-linux").contains(platform);
                boolean desktop = linux || windows || platform.endsWith("osx");
                String contextYaml = "{excludeLibs: [custom, " + (windows ? "librecord" : "record") + ", " + vpx
                        + "], excludeSymbols: [CustomSymbol], libs: [" + (windows ? "librecord_null.lib" : "record_null")
                        + ", custom], defines: [CUSTOM]}";
                String yaml = platform.equals("root")
                        ? "context: " + contextYaml + "\n"
                        : "platforms: {" + platform + ": {context: " + contextYaml + "}}\n";
                byte[] original = yaml.getBytes(StandardCharsets.UTF_8);
                createFile(fileSystem, "legacy-record.appmanifest", original);
                project.getProjectProperties().putStringValue("native_extension", "app_manifest", "legacy-record.appmanifest");
                byte[] migrated = findResource(ExtenderUtil.getExtensionSources(project, Platform.X86_64Linux, null),
                        ExtenderUtil.appManifestPath).getContent();
                Map<String, Object> manifest = new Yaml().load(new String(migrated, StandardCharsets.UTF_8));
                Map<String, Object> platformValue = platform.equals("root")
                        ? manifest
                        : (Map<String, Object>) ((Map<String, Object>) manifest.get("platforms")).get(platform);
                Map<String, Object> context = (Map<String, Object>) platformValue.get("context");
                assertEquals(platform, linux ? List.of("custom", "record", "record_script", "record_service", "openh264")
                        : desktop ? List.of("custom", "record", "record_script", "record_service")
                        : List.of("custom", "record", "record_script"), context.get("excludeLibs"));
                assertEquals(platform, desktop ? List.of("CustomSymbol", "ScriptRecord", "EngineRecord")
                        : List.of("CustomSymbol", "ScriptRecord"), context.get("excludeSymbols"));
                assertEquals(List.of("custom"), context.get("libs"));
                assertEquals(List.of("CUSTOM"), context.get("defines"));
                assertArrayEquals(original, project.getResource("legacy-record.appmanifest").getContent());
                createFile(fileSystem, "legacy-record.appmanifest", migrated);
                assertArrayEquals(migrated, findResource(ExtenderUtil.getExtensionSources(project, Platform.X86_64Linux, null),
                        ExtenderUtil.appManifestPath).getContent());
            }
        }
    }

    // Verifies exclusions already migrated away from VPX also strip ScriptRecord
    // and explicit recording libraries, preserving custom settings and idempotence.
    @SuppressWarnings("unchecked")
    @Test
    public void testNativeRecordingExclusionsMigrateBeforeUpload() throws Exception {
        for (String platform : List.of("root", "common", "osx", "arm64-osx", "x86_64-osx",
                "linux", "x86_64-linux", "arm64-linux", "win32", "x86-win32", "x86_64-win32",
                "ios", "arm64-ios", "arm64_sim-ios", "android", "armv7-android", "arm64-android",
                "x86_64-android", "web", "wasm-web", "wasm_pthread-web")) {
            for (String key : List.of("libs", "engineLibs")) {
                for (String nullLibrary : List.of("record_null", "record_null.lib", "librecord_null", "librecord_null.lib")) {
                    boolean linux = List.of("root", "common", "linux", "x86_64-linux", "arm64-linux").contains(platform);
                    boolean desktop = linux || platform.endsWith("win32") || platform.endsWith("osx");
                    List<String> excludedLibraries = linux ? List.of("custom", "record", "record_service", "openh264")
                            : desktop ? List.of("custom", "record", "record_service") : List.of("custom", "record");
                    List<String> excludedSymbols = desktop ? List.of("CustomSymbol", "EngineRecord") : List.of("CustomSymbol");
                    Map<String, Object> context = Map.of("excludeLibs", excludedLibraries,
                            "excludeSymbols", excludedSymbols, key, List.of(nullLibrary, "custom"), "defines", List.of("CUSTOM"));
                    Map<String, Object> manifest = platform.equals("root") ? Map.of("context", context)
                            : Map.of("platforms", Map.of(platform, Map.of("context", context)));
                    byte[] original = new Yaml().dump(manifest).getBytes(StandardCharsets.UTF_8);
                    createFile(fileSystem, "native-record.appmanifest", original);
                    byte[] migrated = new ExtenderUtil.FSAppManifestResource(
                            project.getResource("native-record.appmanifest"), project.getRootDirectory(),
                            ExtenderUtil.appManifestPath, null).getContent();
                    Map<String, Object> result = new Yaml().load(new String(migrated, StandardCharsets.UTF_8));
                    Map<String, Object> platformValue = platform.equals("root") ? result
                            : (Map<String, Object>) ((Map<String, Object>) result.get("platforms")).get(platform);
                    Map<String, Object> migratedContext = (Map<String, Object>) platformValue.get("context");
                    List<String> expectedLibraries = new ArrayList<>(excludedLibraries);
                    expectedLibraries.add("record_script");
                    List<String> expectedSymbols = new ArrayList<>(excludedSymbols);
                    expectedSymbols.add("ScriptRecord");
                    assertEquals(platform, Map.of("excludeLibs", expectedLibraries, "excludeSymbols", expectedSymbols,
                            key, List.of("custom"), "defines", List.of("CUSTOM")), migratedContext);
                    assertArrayEquals(original, project.getResource("native-record.appmanifest").getContent());
                    createFile(fileSystem, "native-record.appmanifest", migrated);
                    assertArrayEquals(migrated, new ExtenderUtil.FSAppManifestResource(
                            project.getResource("native-record.appmanifest"), project.getRootDirectory(),
                            ExtenderUtil.appManifestPath, null).getContent());
                }
            }
        }
    }

    // Verifies explicit null backends disable all recording even without an
    // exclusion list, preserving custom libraries and the source manifest.
    @SuppressWarnings("unchecked")
    @Test
    public void testNullRecordingBackendSelectionsMigrateBeforeUpload() throws Exception {
        for (String platform : List.of("root", "common", "osx", "linux", "win32", "ios", "android", "web")) {
            for (String key : List.of("libs", "engineLibs")) {
                for (String nullLibrary : List.of("record_null", "record_null.lib", "librecord_null", "librecord_null.lib")) {
                    Map<String, Object> context = Map.of(key, List.of(nullLibrary, "custom"), "defines", List.of("CUSTOM"));
                    Map<String, Object> manifest = platform.equals("root") ? Map.of("context", context)
                            : Map.of("platforms", Map.of(platform, Map.of("context", context)));
                    byte[] original = new Yaml().dump(manifest).getBytes(StandardCharsets.UTF_8);
                    createFile(fileSystem, "null-record.appmanifest", original);
                    byte[] migrated = new ExtenderUtil.FSAppManifestResource(
                            project.getResource("null-record.appmanifest"), project.getRootDirectory(),
                            ExtenderUtil.appManifestPath, null).getContent();
                    Map<String, Object> result = new Yaml().load(new String(migrated, StandardCharsets.UTF_8));
                    Map<String, Object> platformValue = platform.equals("root") ? result
                            : (Map<String, Object>) ((Map<String, Object>) result.get("platforms")).get(platform);
                    boolean linux = List.of("root", "common", "linux").contains(platform);
                    boolean desktop = linux || List.of("osx", "win32").contains(platform);
                    List<String> libraries = linux ? List.of("record", "record_script", "record_service", "openh264")
                            : desktop ? List.of("record", "record_script", "record_service") : List.of("record", "record_script");
                    assertEquals(platform, Map.of(key, List.of("custom"), "defines", List.of("CUSTOM"),
                            "excludeLibs", libraries, "excludeSymbols", desktop ? List.of("ScriptRecord", "EngineRecord")
                                    : List.of("ScriptRecord")), platformValue.get("context"));
                    assertArrayEquals(original, project.getResource("null-record.appmanifest").getContent());
                    createFile(fileSystem, "null-record.appmanifest", migrated);
                    assertArrayEquals(migrated, new ExtenderUtil.FSAppManifestResource(
                            project.getResource("null-record.appmanifest"), project.getRootDirectory(),
                            ExtenderUtil.appManifestPath, null).getContent());
                }
            }
        }
    }

    // Verifies recording migrations add missing library and symbol lists, avoid duplicate
    // exclusions, and preserve partial, current, and malformed manifests.
    @Test
    public void testRecordingExclusionMigrationEdgeCases() throws Exception {
        for (String[] contexts : List.of(
                new String[]{"{excludeLibs: [record, vpx]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord]}"},
                new String[]{"{excludeLibs: [record, vpx, record_service, openh264], excludeSymbols: [EngineRecord]}",
                        "{excludeLibs: [record, record_service, openh264, record_script], excludeSymbols: [EngineRecord, ScriptRecord]}"},
                new String[]{"{excludeLibs: [record, record_script], libs: [librecord.lib, record_null, record_script.lib, librecord_service, custom]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord], libs: [custom]}"},
                new String[]{"{excludeLibs: [record], libs: [record_null]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord], libs: []}"},
                new String[]{"{excludeLibs: [record], excludeSymbols: [ScriptRecord], libs: [record_null]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord], libs: []}"},
                new String[]{"{excludeLibs: null, excludeSymbols: null, libs: [record_null]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord], libs: []}"},
                new String[]{"{excludeLibs: [record, record_null]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord]}"},
                new String[]{"{excludeLibs: [record, record_null, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord]}",
                        "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord]}"},
                new String[]{"{excludeLibs: [custom, record_null, record_null.lib, librecord_null, librecord_null.lib], defines: [CUSTOM]}",
                        "{excludeLibs: [custom], defines: [CUSTOM]}"})) {
            createFile(fileSystem, "record.appmanifest", ("platforms: {linux: {context: " + contexts[0] + "}}").getBytes(StandardCharsets.UTF_8));
            byte[] migrated = new ExtenderUtil.FSAppManifestResource(
                    project.getResource("record.appmanifest"), project.getRootDirectory(), ExtenderUtil.appManifestPath, null).getContent();
            assertEquals((Object) new Yaml().load("platforms: {linux: {context: " + contexts[1] + "}}"),
                    (Object) new Yaml().load(new String(migrated, StandardCharsets.UTF_8)));
        }
        for (String context : List.of("null", "42", "[]", "{excludeLibs: [record]}",
                "{excludeLibs: [vpx]}", "{excludeLibs: record}", "{excludeLibs: [null, 42, custom]}",
                "{excludeLibs: [record, vpx], excludeSymbols: invalid}",
                "{excludeLibs: [record, vpx], libs: invalid}", "{excludeLibs: [record, vpx], engineLibs: invalid}",
                "{libs: [record_null], excludeLibs: invalid}",
                "{libs: [record_null], excludeSymbols: invalid}",
                "{excludeLibs: [record_script]}", "{libs: [record]}",
                "{excludeLibs: [record, record_script, record_service, openh264], excludeSymbols: [ScriptRecord, EngineRecord], libs: []}")) {
            byte[] original = ("# Preserve formatting\nplatforms: {linux: {context: " + context + "}}").getBytes(StandardCharsets.UTF_8);
            createFile(fileSystem, "record.appmanifest", original);
            assertArrayEquals(original, new ExtenderUtil.FSAppManifestResource(
                    project.getResource("record.appmanifest"), project.getRootDirectory(), ExtenderUtil.appManifestPath, null).getContent());
        }
    }

    @Test
    public void testUnchangedAppManifestsPreserveTheirContent() throws Exception {
        for (String manifestYaml : List.of(
                "# Preserve comments and formatting\nplatforms: {win32: {context: {libs: [font_render, dmbedtls, libcustom.lib]}}}\n",
                "platforms: [",
                "platforms: {win32: {context: {libs: libmbedtls.lib}}, x86-win32: null, x86_64-win32: {context: {libs: [null, 42, libcustom.lib]}}}",
                "platforms: {osx: {context: {libs: [platform_vulkan], excludeLibs: [platform_vulkan]}}}",
                "platforms: {osx: {context: {libs: [graphics_metal]}}}",
                "platforms: {osx: {context: {libs: [platform_vulkan], excludeLibs: invalid}}}",
                "platforms: {arm64-ios: {context: {libs: [platform_vulkan]}}}",
                "", "null", "[]", "not a map", "platforms: null")) {
            byte[] originalContent = manifestYaml.getBytes(StandardCharsets.UTF_8);
            createFile(fileSystem, "unchanged.appmanifest", originalContent);
            project.getProjectProperties().putStringValue("native_extension", "app_manifest", "unchanged.appmanifest");
            ExtenderResource uploadedResource = findResource(
                    ExtenderUtil.getExtensionSources(project, Platform.X86_64Win32, null), ExtenderUtil.appManifestPath);
            assertArrayEquals(originalContent, uploadedResource.getContent());
        }
    }

    // Verifies that legacy manifests gain only the missing Bullet3D script
    // exclusions, without duplicates or changes to partial matches. This keeps
    // projects saved by older editors linkable after the script-library split.
    @SuppressWarnings("unchecked")
    @Test
    public void testLegacyBullet3DAppManifestCompatibility() throws Exception {
        String manifestYaml =
                "context:\n" +
                "    excludeLibs: [LinearMath, BulletDynamics, BulletCollision]\n" +
                "platforms:\n" +
                "    x86_64-win32:\n" +
                "        context:\n" +
                "            excludeLibs: [libLinearMath, libBulletDynamics, libBulletCollision, libphysics_3d]\n" +
                "            excludeSymbols: []\n" +
                "    x86_64-linux:\n" +
                "        context:\n" +
                "            excludeLibs: [LinearMath, BulletDynamics, BulletCollision, script_bullet3d]\n" +
                "            excludeSymbols: [ScriptBullet3DExt]\n" +
                "    arm64-linux:\n" +
                "        context:\n" +
                "            excludeLibs: [LinearMath, BulletDynamics]\n" +
                "            excludeSymbols: []\n";
        createFile(fileSystem, "legacy.appmanifest", manifestYaml.getBytes(StandardCharsets.UTF_8));

        IResource resource = project.getResource("legacy.appmanifest");
        ExtenderUtil.FSAppManifestResource appManifest = new ExtenderUtil.FSAppManifestResource(
                resource, tmpDir.getAbsolutePath(), "_app/app.manifest", null);
        Map<String, Object> manifest = new Yaml().load(new String(appManifest.getContent(), StandardCharsets.UTF_8));

        Map<String, Object> rootContext = (Map<String, Object>) manifest.get("context");
        assertTrue(((List<String>) rootContext.get("excludeLibs")).contains("script_bullet3d"));
        assertTrue(((List<String>) rootContext.get("excludeSymbols")).contains("ScriptBullet3DExt"));

        Map<String, Object> platforms = (Map<String, Object>) manifest.get("platforms");
        Map<String, Object> windowsContext = (Map<String, Object>) ((Map<String, Object>) platforms.get("x86_64-win32")).get("context");
        assertTrue(((List<String>) windowsContext.get("excludeLibs")).contains("script_bullet3d"));
        assertTrue(((List<String>) windowsContext.get("excludeLibs")).contains("physics_3d"));
        assertFalse(((List<String>) windowsContext.get("excludeLibs")).contains("libphysics_3d"));
        assertTrue(((List<String>) windowsContext.get("excludeSymbols")).contains("ScriptBullet3DExt"));

        Map<String, Object> currentContext = (Map<String, Object>) ((Map<String, Object>) platforms.get("x86_64-linux")).get("context");
        assertEquals(1, Collections.frequency((List<String>) currentContext.get("excludeLibs"), "script_bullet3d"));
        assertEquals(1, Collections.frequency((List<String>) currentContext.get("excludeSymbols"), "ScriptBullet3DExt"));

        Map<String, Object> partialContext = (Map<String, Object>) ((Map<String, Object>) platforms.get("arm64-linux")).get("context");
        assertFalse(((List<String>) partialContext.get("excludeLibs")).contains("script_bullet3d"));
        assertFalse(((List<String>) partialContext.get("excludeSymbols")).contains("ScriptBullet3DExt"));
    }
}
