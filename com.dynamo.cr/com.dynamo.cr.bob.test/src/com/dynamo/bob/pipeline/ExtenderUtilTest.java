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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
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
            assertTrue(appRules != null);
            assertEquals("-keep class com.dynamo.android.DefoldActivity { *; }\n", new String(appRules.getContent(), StandardCharsets.UTF_8));
            assertTrue(findResource(androidResources, "_app/dmengine.keep") == null);
            assertTrue(findResource(linuxResources, ExtenderUtil.r8KeepRulesPath) == null);
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
            assertTrue(appRules != null);
            assertEquals("-keep class example.Custom\n", new String(appRules.getContent(), StandardCharsets.UTF_8));
            assertTrue(findResource(resources, "_app/dmengine.keep") == null);
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
            assertTrue(findResource(resources, ExtenderUtil.r8KeepRulesPath) == null);
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
            assertTrue(findResource(
                    ExtenderUtil.getExtensionSources(r8Project, Platform.X86_64Linux, null),
                    ExtenderUtil.r8KeepRulesPath) == null);
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
                "            excludeLibs: [libLinearMath, libBulletDynamics, libBulletCollision]\n" +
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
        assertTrue(((List<String>) windowsContext.get("excludeSymbols")).contains("ScriptBullet3DExt"));

        Map<String, Object> currentContext = (Map<String, Object>) ((Map<String, Object>) platforms.get("x86_64-linux")).get("context");
        assertEquals(1, Collections.frequency((List<String>) currentContext.get("excludeLibs"), "script_bullet3d"));
        assertEquals(1, Collections.frequency((List<String>) currentContext.get("excludeSymbols"), "ScriptBullet3DExt"));

        Map<String, Object> partialContext = (Map<String, Object>) ((Map<String, Object>) platforms.get("arm64-linux")).get("context");
        assertFalse(((List<String>) partialContext.get("excludeLibs")).contains("script_bullet3d"));
        assertFalse(((List<String>) partialContext.get("excludeSymbols")).contains("ScriptBullet3DExt"));
    }
}
