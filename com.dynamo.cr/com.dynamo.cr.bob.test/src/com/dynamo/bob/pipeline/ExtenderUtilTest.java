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
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.yaml.snakeyaml.Yaml;

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

    private ExtenderResource findResource(List<ExtenderResource> resources, String path) {
        for (ExtenderResource resource : resources) {
            if (path.equals(resource.getPath())) {
                return resource;
            }
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    @Test
    public void testLegacyWindowsAppManifestLibrariesAreMigratedBeforeUpload() throws Exception {
        String libraries = "[libphysics, libphysics_3d.lib, record_null.lib, "
                + "librender_font_default, librender_font_default.lib, render_font_default.lib, render_font_default, "
                + "libmbedtls, libmbedtls.lib, mbedtls.lib, mbedtls, "
                + "libmbedtls_noasan, libmbedtls_noasan.lib, mbedtls_noasan.lib, mbedtls_noasan, "
                + "libdmbedtls.lib, libdmbedtls_noasan, libfont_render, libgameobject.lib, "
                + "physics, libbox2d_defold, libopus.lib, vpx, vulkan-1, libcustom.lib, null, 42]";
        List<Object> expectedLibraries = Arrays.asList(
                "physics", "physics_3d", "record_null",
                "font_render", "font_render", "font_render", "font_render",
                "dmbedtls", "dmbedtls", "dmbedtls", "dmbedtls",
                "dmbedtls_noasan", "dmbedtls_noasan", "dmbedtls_noasan", "dmbedtls_noasan",
                "dmbedtls", "dmbedtls_noasan", "font_render", "gameobject",
                "physics", "libbox2d_defold", "libopus.lib", "vpx", "vulkan-1", "libcustom.lib", null, 42);
        String manifestYaml = "context:\n    libs: " + libraries + "\nplatforms:\n";
        for (String platform : List.of("win32", "x86-win32", "x86_64-win32", "common", "x86_64-linux")) {
            manifestYaml += "    " + platform + ":\n        context:\n";
            for (String key : List.of("excludeLibs", "libs", "engineLibs", "symbols")) {
                manifestYaml += "            " + key + ": " + libraries + "\n";
            }
        }
        byte[] originalContent = manifestYaml.getBytes(StandardCharsets.UTF_8);
        createFile(fileSystem, "legacy-windows.appmanifest", originalContent);
        project.getProjectProperties().putStringValue("native_extension", "app_manifest", "legacy-windows.appmanifest");

        ExtenderResource uploadedResource = findResource(
                ExtenderUtil.getExtensionSources(project, Platform.X86_64Win32, null), ExtenderUtil.appManifestPath);
        byte[] migratedContent = uploadedResource.getContent();
        Map<String, Object> manifest = new Yaml().load(new String(migratedContent, StandardCharsets.UTF_8));
        Map<String, Object> original = new Yaml().load(manifestYaml);
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

    @Test
    public void testUnchangedAppManifestsPreserveTheirContent() throws Exception {
        for (String manifestYaml : List.of(
                "# Preserve comments and formatting\nplatforms: {win32: {context: {libs: [font_render, dmbedtls, libcustom.lib]}}}\n",
                "platforms: [",
                "platforms: {win32: {context: {libs: libmbedtls.lib}}, x86-win32: null, x86_64-win32: {context: {libs: [null, 42, libcustom.lib]}}}",
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
