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

package com.dynamo.bob.test;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import javax.imageio.ImageIO;

import org.apache.commons.configuration2.ex.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.junit.Assume;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.Bob;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.input.proto.Input.GamepadMapsRuntime;
import com.dynamo.liveupdate.proto.Manifest;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;

public class ProjectBuildTest {

    private static final int LARGE_ARCHIVE_CUSTOM_RESOURCE_COUNT = 6;
    private static final long LARGE_ARCHIVE_CUSTOM_RESOURCE_SIZE = 16L;
    private static final int LARGE_ARCHIVE_RESOURCE_PADDING = 512 * 1024 * 1024;
    private static final long TWO_GIB = 2L * 1024L * 1024L * 1024L;
    private static final long MIN_LARGE_ARCHIVE_TEST_FREE_SPACE = 6L * 1024L * 1024L * 1024L;

    private String contentRoot;
    private String projectName = "Unnamed";

    private static class CountingPublisher extends NullPublisher {
        int startCount = 0;
        int stopCount = 0;

        @Override
        public void start() throws CompileExceptionError {
            startCount++;
        }

        @Override
        public void stop() throws CompileExceptionError {
            stopCount++;
        }
    }

    @Before
    public void setUp() throws Exception {
        projectName = "Unnamed";
        contentRoot = Files.createTempDirectory(null).toFile().getAbsolutePath();
        createFile(contentRoot, "game.project", "[display]\nwidth=640\nheight=480\n");

        BufferedImage image = new BufferedImage(16, 16, BufferedImage.TYPE_4BYTE_ABGR);
        ImageIO.write(image, "png", new File(contentRoot, "test.png"));
    }

    @After
    public void tearDown() throws IOException {
        FileUtils.deleteDirectory(new File(contentRoot));
    }

    void build() throws IOException, CompileExceptionError, MultipleCompileException {
        build(false, false);
    }

    void buildArchive(boolean publishLiveupdate) throws IOException, CompileExceptionError, MultipleCompileException {
        build(true, publishLiveupdate);
    }

    private void build(boolean archive, boolean publishLiveupdate) throws IOException, CompileExceptionError, MultipleCompileException {
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setPublisher(new NullPublisher(new PublisherSettings()));

            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");

            if (archive) {
                project.setOption("archive", "true");
            }
            if (publishLiveupdate) {
                project.setOption("liveupdate", "true");
            }

            // project.setOption("platform", Platform.X86Win32.getPair());
            List<TaskResult> result = project.build(Progress.discarding(), "clean", "build");
            for (TaskResult taskResult : result) {
                assertTrue(taskResult.toString(), taskResult.isOk());
            }
        }
    }

    // Returns the number of files that will be put into the DARC file
    // Note that the game.project isn't put in the archive either
    protected int createDefaultFiles() throws IOException {
        int count = 0;
        createFile(contentRoot, "logic/main.collection", "name: \"default\"\nscale_along_z: 0\n");
        count++;
        createFile(contentRoot, "builtins/render/default.render", "script: \"/builtins/render/default.render_script\"\n");
        count++;
        createFile(contentRoot, "builtins/render/default.render_script", "");
        count++;
        createFile(contentRoot, "builtins/render/default.display_profiles", "");
        count++;
        createFile(contentRoot, "builtins/graphics/default.texture_profiles", "");
        count++;
        createFile(contentRoot, "builtins/input/default.gamepads", "");
        count++;
        createFile(contentRoot, "builtins/input/gamecontrollerdb.txt", "");
        createFile(contentRoot, "input/game.input_binding", "");
        count++;

        return count;
    }

    @Test
    public void testBuildInputFileWithoutGameProject() throws Exception {
        Files.delete(new File(contentRoot, "game.project").toPath());
        createFile(contentRoot, "data/valid.data", "tags: \"tag-one\"\ndata { string: \"hello\" }\n");
        createFile(contentRoot, "build.inputs", "# direct test data roots\n/data/valid.data\n\n");

        Bob.InvocationResult result = Bob.invoke(null, Progress.discarding(), null, new String[]{
                "--root", contentRoot,
                "--build-input-file", "build.inputs",
                "build"
        });

        assertTrue(result.success);
        assertTrue(new File(contentRoot, "build/default/data/valid.datac").exists());
    }

    @Test
    public void testBuildInputFileGamepadsWithoutGameProject() throws Exception {
        Files.delete(new File(contentRoot, "game.project").toPath());
        createFile(contentRoot, "input/valid.gamepads", ""
                + "driver {\n"
                + "  device: \"Direct Pad\"\n"
                + "  platform: \"macos\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n");
        createFile(contentRoot, "build.inputs", "# direct test data roots\n/input/valid.gamepads\n\n");

        Bob.InvocationResult result = Bob.invoke(null, Progress.discarding(), null, new String[]{
                "--root", contentRoot,
                "--build-input-file", "build.inputs",
                "--platform", "x86_64-macos",
                "build"
        });

        assertTrue(result.success);
        File output = new File(contentRoot, "build/default/input/valid.gamepadsc");
        assertTrue(output.exists());
        GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(FileUtils.readFileToByteArray(output));
        assertEquals(1, maps.getMappingsCount());
        assertEquals("Direct Pad", maps.getMappings(0).getDevice());
        assertEquals(0.2f, maps.getMappings(0).getDeadZone(), 0.0f);
    }

    @Test
    public void testBuild() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        build();
    }

    @Test
    public void testGamepadProjectPropertiesCreateCombinedGamepadTask() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createFile(contentRoot, "game.project", ""
                + "[display]\n"
                + "width=640\n"
                + "height=480\n"
                + "[input]\n"
                + "gamepads=/input/custom.gamepadsc\n"
                + "gamepad_database=/input/gamecontrollerdb.txt\n"
                + "gamepad_deadzone=0.35\n");
        createFile(contentRoot, "input/custom.gamepads", ""
                + "driver {\n"
                + "  device: \"Manual Project Pad\"\n"
                + "  platform: \"macos\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Manual Project Pad\"\n"
                + "  platform: \"linux\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Manual Project Pad\"\n"
                + "  platform: \"windows\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n");
        createFile(contentRoot, "input/gamecontrollerdb.txt", ""
                + "03000000000000000000000000000001,SDL Project Pad,a:b1,platform:Mac OS X,\n"
                + "03000000000000000000000000000002,SDL Project Pad,a:b1,platform:Linux,\n"
                + "03000000000000000000000000000003,SDL Project Pad,a:b1,platform:Windows,\n"
                + "03000000000000000000000000000004,SDL Project Pad,a:b1,platform:iOS,\n"
                + "03000000000000000000000000000005,SDL Project Pad,a:b1,platform:Android,\n"
                + "03000000000000000000000000000006,SDL Project Pad,a:b1,platform:Web,\n");

        build();

        File output = new File(contentRoot, "build/input/custom.gamepadsc");
        assertTrue(output.exists());
        GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(FileUtils.readFileToByteArray(output));
        assertEquals(2, maps.getMappingsCount());
        assertEquals("SDL Project Pad", maps.getMappings(0).getDevice());
        assertEquals("Manual Project Pad", maps.getMappings(1).getDevice());
        assertFalse(maps.getMappings(0).hasDeadZone());
        assertTrue(maps.getMappings(0).hasGuid());
        assertFalse(maps.getMappings(1).hasGuid());
        assertEquals(0.2f, maps.getMappings(1).getDeadZone(), 0.0f);
    }

    @Test
    public void testDefaultGamepadDatabaseCreatesCombinedGamepadTask() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createFile(contentRoot, "builtins/input/default.gamepads", ""
                + "driver {\n"
                + "  device: \"Default Manual Pad\"\n"
                + "  platform: \"macos\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Default Manual Pad\"\n"
                + "  platform: \"linux\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Default Manual Pad\"\n"
                + "  platform: \"windows\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n");
        createFile(contentRoot, "builtins/input/gamecontrollerdb.txt", ""
                + "030000005e0400008e02000014010000,Xbox 360 Controller,a:b1,platform:Mac OS X,\n"
                + "030000005e0400008e02000014010001,Xbox 360 Controller,a:b1,platform:Linux,\n"
                + "030000005e0400008e02000014010002,Xbox 360 Controller,a:b1,platform:Windows,\n");

        build();

        File output = new File(contentRoot, "build/builtins/input/default.gamepadsc");
        assertTrue(output.exists());
        GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(FileUtils.readFileToByteArray(output));
        assertEquals(2, maps.getMappingsCount());
        assertEquals("Xbox 360 Controller", maps.getMappings(0).getDevice());
        assertEquals("Default Manual Pad", maps.getMappings(1).getDevice());
        assertFalse(maps.getMappings(0).hasDeadZone());
        assertTrue(maps.getMappings(0).hasGuid());
        assertFalse(maps.getMappings(1).hasGuid());
        assertEquals(0.2f, maps.getMappings(1).getDeadZone(), 0.0f);
    }

    @Test
    public void testEmptyGamepadDatabaseDisablesDefaultDatabase() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createFile(contentRoot, "game.project", ""
                + "[display]\n"
                + "width=640\n"
                + "height=480\n"
                + "[input]\n"
                + "gamepad_database=\n");
        createFile(contentRoot, "builtins/input/default.gamepads", ""
                + "driver {\n"
                + "  device: \"Default Manual Pad\"\n"
                + "  platform: \"macos\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Default Manual Pad\"\n"
                + "  platform: \"linux\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Default Manual Pad\"\n"
                + "  platform: \"windows\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n");
        createFile(contentRoot, "builtins/input/gamecontrollerdb.txt", ""
                + "030000005e0400008e02000014010000,Xbox 360 Controller,a:b1,platform:Mac OS X,\n"
                + "030000005e0400008e02000014010001,Xbox 360 Controller,a:b1,platform:Linux,\n"
                + "030000005e0400008e02000014010002,Xbox 360 Controller,a:b1,platform:Windows,\n");

        build();

        File output = new File(contentRoot, "build/builtins/input/default.gamepadsc");
        assertTrue(output.exists());
        GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(FileUtils.readFileToByteArray(output));
        assertEquals(1, maps.getMappingsCount());
        assertEquals("Default Manual Pad", maps.getMappings(0).getDevice());
        assertFalse(maps.getMappings(0).hasGuid());
        assertEquals(0.2f, maps.getMappings(0).getDeadZone(), 0.0f);
    }

    @Test
    public void testGamepadSourceFieldCombinations() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createDefaultFiles();
        createFile(contentRoot, "input/custom.gamepads", ""
                + "driver {\n"
                + "  device: \"Manual Project Pad\"\n"
                + "  platform: \"macos\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Manual Project Pad\"\n"
                + "  platform: \"linux\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n"
                + "driver {\n"
                + "  device: \"Manual Project Pad\"\n"
                + "  platform: \"windows\"\n"
                + "  dead_zone: 0.2\n"
                + "  map { input: GAMEPAD_RPAD_DOWN type: GAMEPAD_TYPE_BUTTON index: 0 }\n"
                + "}\n");
        createFile(contentRoot, "input/gamecontrollerdb.txt", ""
                + "03000000000000000000000000000001,SDL Only Pad,a:b1,platform:Mac OS X,\n"
                + "03000000000000000000000000000002,SDL Only Pad,a:b1,platform:Linux,\n"
                + "03000000000000000000000000000003,SDL Only Pad,a:b1,platform:Windows,\n");

        checkGamepadSourceFieldCombination(
                "/input/custom.gamepadsc", "/input/gamecontrollerdb.txt",
                "/input/custom.gamepadsc", "input/custom.gamepadsc",
                new String[]{"SDL Only Pad", "Manual Project Pad"});
        checkGamepadSourceFieldCombination(
                "/input/custom.gamepadsc", "",
                "/input/custom.gamepadsc", "input/custom.gamepadsc",
                new String[]{"Manual Project Pad"});
        checkGamepadSourceFieldCombination(
                "", "/input/gamecontrollerdb.txt",
                "/input/gamecontrollerdb.gamepadsc", "input/gamecontrollerdb.gamepadsc",
                new String[]{"SDL Only Pad"});
        checkGamepadSourceFieldCombination("", "", "", null, new String[0]);
    }

    private void checkGamepadSourceFieldCombination(String gamepads,
                                                    String gamepadDatabase,
                                                    String expectedProjectGamepads,
                                                    String expectedBuildPath,
                                                    String[] expectedDevices) throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createFile(contentRoot, "game.project", ""
                + "[display]\n"
                + "width=640\n"
                + "height=480\n"
                + "[input]\n"
                + "gamepads=" + gamepads + "\n"
                + "gamepad_database=" + gamepadDatabase + "\n");

        build();

        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));
        checkProjectSetting(outputProps, "input", "gamepads", expectedProjectGamepads);
        checkProjectSetting(outputProps, "input", "gamepad_database", null);

        if (expectedBuildPath == null) {
            assertFalse(new File(contentRoot, "build/input/custom.gamepadsc").exists());
            assertFalse(new File(contentRoot, "build/input/gamecontrollerdb.gamepadsc").exists());
            assertFalse(new File(contentRoot, "build/builtins/input/default.gamepadsc").exists());
            assertFalse(new File(contentRoot, "build/builtins/input/gamecontrollerdb.gamepadsc").exists());
        } else {
            File output = new File(contentRoot, "build/" + expectedBuildPath);
            assertTrue(output.exists());
            GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(FileUtils.readFileToByteArray(output));
            assertEquals(expectedDevices.length, maps.getMappingsCount());
            for (int i = 0; i < expectedDevices.length; i++) {
                assertEquals(expectedDevices[i], maps.getMappings(i).getDevice());
            }
        }
    }

    static private void checkProjectSetting(BobProjectProperties properties, String category, String key, String expectedValue)
    {
        assertEquals(expectedValue, properties.getStringValue(category, key));
    }

    static private void checkProjectSettingArray(BobProjectProperties properties, String category, String key, String[] expectedValues)
    {
        String[] values = properties.getStringArrayValue(category, key, new String[0]);

        assertEquals(expectedValues.length, values.length);

        for (int i = 0; i < expectedValues.length; i++) {
            String actual = values[i];
            String expected = expectedValues[i];
            assertEquals(expected, actual);
        }
    }

    @Test
    public void testGamePropertiesBuildtimeTransform() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        projectName = "Game Project Properties Transform";
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ntitle = " + projectName +"\ndependencies = http://test.com/test.zip\n\n[display]" + "\nvariable_dt = 1\n" + "vsync = 1\n" + "update_frequency = 30\n");
        build();
        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));

        checkProjectSetting(outputProps, "display", "vsync", "0");
        checkProjectSetting(outputProps, "display", "update_frequency", "0");
    }

    @Test
    public void testGameProperties() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        projectName = "Game Project Properties";
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ntitle = " + projectName +"\ndependencies = http://test.com/test.zip\n\n[custom]\nlove = defold\nshould_be_empty =\nempty_list =\nempty_list2 =,,,\nlist1 = a\nlist2 = a,b,c\n");
        build();
        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));

        checkProjectSetting(outputProps, "project", "title", projectName);

        // Non existent property
        checkProjectSetting(outputProps, "project", "doesn't_exist", null);

        // Default boolean value
        checkProjectSetting(outputProps, "script", "shared_state", "0");
        checkProjectSetting(outputProps, "display", "vsync", "1");
        checkProjectSetting(outputProps, "display", "update_frequency", "0");

        // Default number value
        checkProjectSetting(outputProps, "display", "width", "960");

        // Custom property
        checkProjectSetting(outputProps, "custom", "love", "defold");

        // project.dependencies entry should be removed
        checkProjectSetting(outputProps, "project", "dependencies", null);

        // Compiled resource
        checkProjectSetting(outputProps, "display", "display_profiles", "/builtins/render/default.display_profilesc");

        // Copy-only resource
        checkProjectSetting(outputProps, "osx", "infoplist", "/builtins/manifests/osx/Info.plist");

        // Check so that empty defaults are not included
        checkProjectSetting(outputProps, "resource", "uri", null);

        // Check so empty custom properties are included as empty strings
        checkProjectSetting(outputProps, "custom", "should_be_empty", "");

        // Check different types of string array values
        checkProjectSettingArray(outputProps, "custom", "empty_list", new String[0]);
        checkProjectSettingArray(outputProps, "custom", "empty_list2", new String[0]);
        checkProjectSettingArray(outputProps, "custom", "list1", new String[]{"a"});
        checkProjectSettingArray(outputProps, "custom", "list2", new String[]{"a", "b", "c"});
    }

    @Test
    public void testStringArray() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
       /* This test helps to make sure that any array of strings that uses # for indices may be parsed,
        * build into a comma separated string and then be parsed again in the same order.
        */
        projectName = "String Array";
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ntitle = " + projectName +
            "\ncustom_string_list#0 = http://test.com/test.zip\ncustom_string_list#2 = http://test.com/test2.zip\ncustom_string_list#1 = http://test.com/test1.zip\n");
        build();
        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));

        checkProjectSettingArray(outputProps, "project", "custom_string_list", new String[]{"http://test.com/test.zip", "http://test.com/test1.zip", "http://test.com/test2.zip"});
    }

    @Test
    public void testFindResourcePathsRespectsDefignore() throws IOException {
        createFile(contentRoot, ".defignore", "ignored-without-leading-slash.txt\n/ignored-dir/\n/ignored-file.txt\n");
        createFile(contentRoot, "visible/keep.txt", "");
        createFile(contentRoot, "ignored-dir/skip.txt", "");
        createFile(contentRoot, "ignored-dir-sibling.txt", "");
        createFile(contentRoot, "sub/ignored-dir/keep.txt", "");
        createFile(contentRoot, "ignored-file.txt", "");
        createFile(contentRoot, "ignored-without-leading-slash.txt", "");

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            List<String> paths = new ArrayList<>();
            project.findResourcePaths("", paths);
            assertEquals(7, paths.size());
            assertEquals(new HashSet<>(Arrays.asList(
                    ".defignore",
                    "game.project",
                    "ignored-dir-sibling.txt",
                    "ignored-without-leading-slash.txt",
                    "sub/ignored-dir/keep.txt",
                    "test.png",
                    "visible/keep.txt")),
                    new HashSet<>(paths));

            List<String> ignoredPaths = new ArrayList<>();
            project.findResourcePaths("ignored-dir", ignoredPaths);
            assertTrue(ignoredPaths.isEmpty());

            List<String> dirs = new ArrayList<>();
            project.findResourceDirs("", dirs);
            assertEquals(2, dirs.size());
            assertEquals(new HashSet<>(Arrays.asList("sub", "visible")), new HashSet<>(dirs));
        }
    }

    @Test
    public void testGameProjectMetaProperties() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        projectName = "String Array";
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ntitle = " + projectName +
            "\ncustom_property = just content");
        createFile(contentRoot, BobProjectProperties.PROPERTIES_PROJECT_FILE, "[project]\ncustom_property.private = 1\n");
        build();
        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));

        checkProjectSetting(outputProps, "project", "custom_property", null);
    }

    @Test
    public void testArchiveBuildStripsExcludedEntriesFromBundledManifestByDefault() throws IOException, CompileExceptionError, MultipleCompileException {
        createExcludedLiveUpdateProject(null);

        buildArchive(true);

        Manifest.ManifestData bundledManifestData = readManifestData(getBundledManifestFile());
        Manifest.ManifestData publishedManifestData = readManifestData(getPublishedManifestFile());

        assertEquals(0, countExcludedEntries(bundledManifestData));
        assertTrue(countExcludedEntries(publishedManifestData) > 0);
        assertTrue(bundledManifestData.getHasExcludedResources());
        assertTrue(publishedManifestData.getHasExcludedResources());

        assertFalse(hasResource(bundledManifestData, "/logic/level.collectionc"));
        assertFalse(hasResource(bundledManifestData, "/logic/level.goc"));
        assertFalse(hasResource(bundledManifestData, "/logic/level.scriptc"));

        assertTrue(hasResource(publishedManifestData, "/logic/level.collectionc"));
        assertTrue(hasResource(publishedManifestData, "/logic/level.goc"));
        assertTrue(hasResource(publishedManifestData, "/logic/level.scriptc"));
    }

    @Test
    public void testArchiveBuildIgnoresDeprecatedManifestEntrySetting() throws IOException, CompileExceptionError, MultipleCompileException, ParseException {
        createExcludedLiveUpdateProject("exclude_entries_from_main_manifest = 0\n");

        buildArchive(true);

        Manifest.ManifestData bundledManifestData = readManifestData(getBundledManifestFile());
        Manifest.ManifestData publishedManifestData = readManifestData(getPublishedManifestFile());
        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(new File(contentRoot + "/build/game.projectc")));

        assertEquals(0, countExcludedEntries(bundledManifestData));
        assertTrue(bundledManifestData.getHasExcludedResources());
        assertTrue(publishedManifestData.getHasExcludedResources());
        assertFalse(hasResource(bundledManifestData, "/logic/level.collectionc"));
        assertTrue(hasResource(publishedManifestData, "/logic/level.collectionc"));
        checkProjectSetting(outputProps, "liveupdate", "exclude_entries_from_main_manifest", null);
    }

    @Test
    public void testArchiveBuildWithoutLiveUpdatePublishingKeepsExcludedEntriesBundled() throws IOException, CompileExceptionError, MultipleCompileException {
        createExcludedLiveUpdateProject(null);

        buildArchive(false);

        Manifest.ManifestData bundledManifestData = readManifestData(getBundledManifestFile());

        assertEquals(0, countExcludedEntries(bundledManifestData));
        assertFalse(bundledManifestData.getHasExcludedResources());
        assertTrue(hasResource(bundledManifestData, "/logic/level.collectionc"));
        assertTrue(hasResource(bundledManifestData, "/logic/level.goc"));
        assertTrue(hasResource(bundledManifestData, "/logic/level.scriptc"));
    }

    @Test
    // Verifies archive builds start the publisher once, so stateful publishers do not leak or reset temp outputs.
    public void testArchiveBuildStartsPublisherOnce() throws IOException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        CountingPublisher publisher = new CountingPublisher();

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build") {
            @Override
            public void createPublisher() throws CompileExceptionError {
                setPublisher(publisher);
            }
        }) {
            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");
            project.setOption("archive", "true");

            List<TaskResult> result = project.build(Progress.discarding(), "clean", "build");
            for (TaskResult taskResult : result) {
                assertTrue(taskResult.toString(), taskResult.isOk());
            }
        }

        assertEquals(1, publisher.startCount);
        assertEquals(1, publisher.stopCount);
    }

    @Test
    public void testArchiveBuildWithoutExcludedResourcesKeepsManifestUnchanged() throws IOException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createLiveUpdatePublisherSettings();
        createFile(contentRoot, "game.project", "[display]\nwidth=640\nheight=480\n[liveupdate]\nsettings = /liveupdate.settings\n");

        buildArchive(true);

        Manifest.ManifestData bundledManifestData = readManifestData(getBundledManifestFile());
        Manifest.ManifestData publishedManifestData = readManifestData(getPublishedManifestFile());

        assertEquals(0, countExcludedEntries(bundledManifestData));
        assertEquals(0, countExcludedEntries(publishedManifestData));
        assertFalse(bundledManifestData.getHasExcludedResources());
        assertFalse(publishedManifestData.getHasExcludedResources());
        assertEquals(publishedManifestData, bundledManifestData);
    }

    @Test
    // Tests that Bob can build archives with resource offsets above 2 GiB.
    // It generates small custom resources, uses large archive-resource-padding to
    // create sparse gaps, then verifies game.arcd crosses 2 GiB and the index
    // contains at least one unsigned resource offset above that boundary.
    public void testArchiveBuildGeneratedLargeCustomResources() throws Exception {
        Assume.assumeTrue(
                "Generated large archive test needs at least 6 GiB of free temp disk space.",
                new File(contentRoot).getUsableSpace() >= MIN_LARGE_ARCHIVE_TEST_FREE_SPACE);

        try {
            createGeneratedLargeCustomResourceArchiveProject();
            buildGeneratedLargeCustomResourceArchiveProject();

            File archiveData = new File(contentRoot, "build/game.arcd");
            File archiveIndex = new File(contentRoot, "build/game.arci");
            File buildReportJson = new File(contentRoot, "large_archive_report.json");
            File buildReportHtml = new File(contentRoot, "large_archive_report.html");
            assertTrue("Expected game.arcd to exceed 2 GiB, was " + archiveData.length(), archiveData.length() > TWO_GIB);
            assertTrue("Expected at least one archive entry to start beyond 2 GiB.", hasArchiveEntryOffsetAbove2GiB(archiveIndex));
            assertTrue("Expected JSON build report to be written.", buildReportJson.isFile());
            assertTrue("Expected HTML build report to be written.", buildReportHtml.isFile());
        } finally {
            FileUtils.deleteDirectory(new File(contentRoot));
        }
    }

    private void createGeneratedLargeCustomResourceArchiveProject() throws IOException {
        createDefaultFiles();
        Files.deleteIfExists(new File(contentRoot, "test.png").toPath());
        createFile(contentRoot, "game.project",
                "[project]\n" +
                "title = Large Archive Test\n" +
                "compress_archive = 0\n" +
                "custom_resources = /large\n" +
                "\n" +
                "[bootstrap]\n" +
                "main_collection = /logic/main.collectionc\n" +
                "\n" +
                "[display]\n" +
                "width=640\n" +
                "height=480\n");

        for (int i = 0; i < LARGE_ARCHIVE_CUSTOM_RESOURCE_COUNT; ++i) {
            createLargeCustomResource(new File(contentRoot, String.format("large/blob_%02d.raw", i)), LARGE_ARCHIVE_CUSTOM_RESOURCE_SIZE, i);
        }
    }

    private void buildGeneratedLargeCustomResourceArchiveProject() throws IOException, CompileExceptionError, MultipleCompileException {
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setPublisher(new NullPublisher(new PublisherSettings()));

            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");

            project.setOption("archive", "true");
            project.setOption("archive-resource-padding", Integer.toString(LARGE_ARCHIVE_RESOURCE_PADDING));
            project.setOption("build-report-json", new File(contentRoot, "large_archive_report.json").getAbsolutePath());
            project.setOption("build-report-html", new File(contentRoot, "large_archive_report.html").getAbsolutePath());
            project.setOption("max-cpu-threads", "1");

            List<TaskResult> result = project.build(Progress.discarding(), "clean", "build");
            for (TaskResult taskResult : result) {
                assertTrue(taskResult.toString(), taskResult.isOk());
            }
        }
    }

    private static void createLargeCustomResource(File file, long size, int index) throws IOException {
        File parent = file.getParentFile();
        if (parent != null) {
            parent.mkdirs();
        }

        try (RandomAccessFile output = new RandomAccessFile(file, "rw")) {
            output.setLength(size);
            output.seek(0);
            output.writeLong(0xdef01d0000000000L + index);
            output.seek(size - Long.BYTES);
            output.writeLong(0x5eed000000000000L + index);
        }
    }

    private static boolean hasArchiveEntryOffsetAbove2GiB(File archiveIndex) throws IOException {
        try (RandomAccessFile input = new RandomAccessFile(archiveIndex, "r")) {
            int version = input.readInt();
            input.readInt();  // Padding
            input.readLong(); // UserData
            int entryCount = input.readInt();
            long entryOffset = Integer.toUnsignedLong(input.readInt());
            input.readInt(); // HashOffset
            input.readInt(); // HashLength
            input.seek(entryOffset);
            if (version != ArchiveBuilder.VERSION_6) {
                throw new IOException("Unsupported archive index version: " + version);
            }
            for (int i = 0; i < entryCount; ++i) {
                long resourceOffset = input.readLong() & ArchiveBuilder.V6_OFFSET_MASK;
                input.readInt(); // ResourceSize
                input.readInt(); // ResourceCompressedSize
                if (resourceOffset > TWO_GIB) {
                    return true;
                }
            }
        }
        return false;
    }

    private void createExcludedLiveUpdateProject(String extraLiveUpdateSettings) throws IOException {
        createDefaultFiles();
        createLiveUpdatePublisherSettings();

        StringBuilder gameProject = new StringBuilder();
        gameProject.append("[display]\nwidth=640\nheight=480\n");
        gameProject.append("[liveupdate]\nsettings = /liveupdate.settings\n");
        if (extraLiveUpdateSettings != null) {
            gameProject.append(extraLiveUpdateSettings);
        }
        createFile(contentRoot, "game.project", gameProject.toString());

        createFile(contentRoot, "logic/main.collection",
                "name: \"main\"\n" +
                "instances {\n" +
                "  id: \"main\"\n" +
                "  prototype: \"/logic/main.go\"\n" +
                "  position {\n" +
                "    x: 0.0\n" +
                "    y: 0.0\n" +
                "    z: 0.0\n" +
                "  }\n" +
                "  rotation {\n" +
                "    x: 0.0\n" +
                "    y: 0.0\n" +
                "    z: 0.0\n" +
                "    w: 1.0\n" +
                "  }\n" +
                "}\n");
        createFile(contentRoot, "logic/main.go",
                "components {\n" +
                "  id: \"level_proxy\"\n" +
                "  component: \"/logic/level.collectionproxy\"\n" +
                "}\n");
        createFile(contentRoot, "logic/level.collectionproxy",
                "collection: \"/logic/level.collection\"\n" +
                "exclude: true\n");
        createFile(contentRoot, "logic/level.collection",
                "name: \"level\"\n" +
                "instances {\n" +
                "  id: \"level\"\n" +
                "  prototype: \"/logic/level.go\"\n" +
                "  position {\n" +
                "    x: 0.0\n" +
                "    y: 0.0\n" +
                "    z: 0.0\n" +
                "  }\n" +
                "  rotation {\n" +
                "    x: 0.0\n" +
                "    y: 0.0\n" +
                "    z: 0.0\n" +
                "    w: 1.0\n" +
                "  }\n" +
                "}\n");
        createFile(contentRoot, "logic/level.go",
                "components {\n" +
                "  id: \"script\"\n" +
                "  component: \"/logic/level.script\"\n" +
                "}\n");
        createFile(contentRoot, "logic/level.script", "function init(self)\nend\n");
    }

    private void createLiveUpdatePublisherSettings() throws IOException {
        createFile(contentRoot, "liveupdate.settings",
                "[liveupdate]\n" +
                "mode = Folder\n" +
                "output-directory = liveupdate_output\n" +
                "output-folder-name = published\n");
    }

    private File getBundledManifestFile() {
        return new File(contentRoot, "build/game.dmanifest");
    }

    private File getPublishedManifestFile() {
        return new File(contentRoot, "liveupdate_output/published/liveupdate.game.dmanifest");
    }

    private Manifest.ManifestData readManifestData(File manifestFile) throws IOException {
        try (FileInputStream inputStream = new FileInputStream(manifestFile)) {
            Manifest.ManifestFile manifest = Manifest.ManifestFile.parseFrom(inputStream);
            return Manifest.ManifestData.parseFrom(manifest.getData());
        }
    }

    private int countExcludedEntries(Manifest.ManifestData manifestData) {
        int excludedEntries = 0;
        for (Manifest.ResourceEntry entry : manifestData.getResourcesList()) {
            if ((entry.getFlags() & ResourceEntryFlag.EXCLUDED.getNumber()) != 0) {
                excludedEntries++;
            }
        }
        return excludedEntries;
    }

    private boolean hasResource(Manifest.ManifestData manifestData, String url) {
        long urlHash = MurmurHash.hash64(url);
        for (Manifest.ResourceEntry entry : manifestData.getResourcesList()) {
            if (url.equals(entry.getUrl()) || entry.getUrlHash() == urlHash) {
                return true;
            }
        }
        return false;
    }

    private String createFile(String root, String name, String content) throws IOException {
        File file = new File(root, name);
        FileUtils.copyInputStreamToFile(new ByteArrayInputStream(content.getBytes()), file);
        return file.getAbsolutePath();
    }

}
