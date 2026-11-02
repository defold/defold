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
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.attribute.FileTime;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.util.zip.ZipFile;
import java.util.stream.Collectors;
import com.dynamo.bob.archive.liveupdate.PackageBuilder;
import com.dynamo.bob.archive.liveupdate.PackageArtifacts;
import com.dynamo.bob.archive.liveupdate.PackagePreview;
import com.dynamo.bob.archive.liveupdate.PackageReport;
import com.dynamo.bob.archive.liveupdate.PackageResolver;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.archive.liveupdate.PackageWriter;
import com.amazonaws.AmazonClientException;
import com.amazonaws.auth.AnonymousAWSCredentials;
import com.amazonaws.services.s3.AmazonS3Client;
import com.amazonaws.services.s3.model.PutObjectRequest;
import com.amazonaws.services.s3.model.PutObjectResult;
import com.dynamo.liveupdate.proto.LiveUpdateManifest;
import com.google.protobuf.TextFormat;
import com.defold.extension.pipeline.PackageProviderFixture;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import javax.imageio.ImageIO;
import javax.tools.ToolProvider;

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
import com.dynamo.bob.IProgress;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.ArchiveReader;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.AWSPublisher;
import com.dynamo.bob.archive.publisher.FolderPublisher;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import com.dynamo.bob.bundle.HTML5Bundler;
import com.dynamo.bob.bundle.LinuxBundler;
import com.dynamo.bob.bundle.IBundler;
import com.dynamo.bob.bundle.ICanceled;
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
        createFile(contentRoot, "input/valid.gamepads", "driver {\n"
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
        createFile(contentRoot, "game.project", "[display]\n"
                + "width=640\n"
                + "height=480\n"
                + "[input]\n"
                + "gamepads=/input/custom.gamepadsc\n"
                + "gamepad_database=/input/gamecontrollerdb.txt\n"
                + "gamepad_deadzone=0.35\n");
        createFile(contentRoot, "input/custom.gamepads", "driver {\n"
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
        createFile(contentRoot, "input/gamecontrollerdb.txt", "03000000000000000000000000000001,SDL Project Pad,a:b1,platform:Mac OS X,\n"
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
        assertTrue(maps.getMappings(0).hasRawMapping());
        assertFalse(maps.getMappings(1).hasRawMapping());
        assertEquals(0.2f, maps.getMappings(1).getDeadZone(), 0.0f);
    }

    @Test
    public void testDefaultGamepadDatabaseCreatesCombinedGamepadTask() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createFile(contentRoot, "builtins/input/default.gamepads", "driver {\n"
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
        createFile(contentRoot, "builtins/input/gamecontrollerdb.txt", "030000005e0400008e02000014010000,Xbox 360 Controller,a:b1,platform:Mac OS X,\n"
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
        assertTrue(maps.getMappings(0).hasRawMapping());
        assertFalse(maps.getMappings(1).hasRawMapping());
        assertEquals(0.2f, maps.getMappings(1).getDeadZone(), 0.0f);
    }

    @Test
    public void testEmptyGamepadDatabaseDisablesDefaultDatabase() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[display]\n"
                + "width=640\n"
                + "height=480\n"
                + "[input]\n"
                + "gamepad_database=\n");
        createFile(contentRoot, "builtins/input/default.gamepads", "driver {\n"
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
        createFile(contentRoot, "builtins/input/gamecontrollerdb.txt", "030000005e0400008e02000014010000,Xbox 360 Controller,a:b1,platform:Mac OS X,\n"
                + "030000005e0400008e02000014010001,Xbox 360 Controller,a:b1,platform:Linux,\n"
                + "030000005e0400008e02000014010002,Xbox 360 Controller,a:b1,platform:Windows,\n");

        build();

        File output = new File(contentRoot, "build/builtins/input/default.gamepadsc");
        assertTrue(output.exists());
        GamepadMapsRuntime maps = GamepadMapsRuntime.parseFrom(FileUtils.readFileToByteArray(output));
        assertEquals(1, maps.getMappingsCount());
        assertEquals("Default Manual Pad", maps.getMappings(0).getDevice());
        assertFalse(maps.getMappings(0).hasRawMapping());
        assertEquals(0.2f, maps.getMappings(0).getDeadZone(), 0.0f);
    }

    @Test
    public void testGamepadSourceFieldCombinations() throws IOException, ConfigurationException, CompileExceptionError, MultipleCompileException, ParseException {
        createDefaultFiles();
        createFile(contentRoot, "input/custom.gamepads", "driver {\n"
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
        createFile(contentRoot, "input/gamecontrollerdb.txt", "03000000000000000000000000000001,SDL Only Pad,a:b1,platform:Mac OS X,\n"
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
        createFile(contentRoot, "game.project", "[display]\n"
                + "width=640\n"
                + "height=480\n"
                + "[input]\n"
                + "gamepads=" + gamepads + "\n"
                + "gamepad_database=" + gamepadDatabase + "\n");

        build();

        BobProjectProperties outputProps = new BobProjectProperties();
        outputProps.load(new FileInputStream(contentRoot + "/build/game.projectc"));
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
        outputProps.load(new FileInputStream(contentRoot + "/build/game.projectc"));

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
        outputProps.load(new FileInputStream(contentRoot + "/build/game.projectc"));

        checkProjectSetting(outputProps, "project", "title", projectName);

        // Non existent property
        checkProjectSetting(outputProps, "project", "doesn't_exist", null);

        // Default boolean value
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
        outputProps.load(new FileInputStream(contentRoot + "/build/game.projectc"));

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
    public void testFindResourcePathsRespectsDefignoreWildcards() throws IOException {
        createFile(contentRoot, ".defignore", """
                /levels/*/tiled
                /a.b/*
                /dir(1)/*.png
                /**/generated
                /Upper/*
                /lower_lit
                """);
        createFile(contentRoot, "levels/1/tiled/x.txt", "");
        createFile(contentRoot, "levels/1/other.txt", "");
        createFile(contentRoot, "levels/tiled/y.txt", "");
        createFile(contentRoot, "levelsX/1/tiled/z.txt", "");
        createFile(contentRoot, "a.b/m.txt", "");
        createFile(contentRoot, "aXb/k.txt", "");
        createFile(contentRoot, "dir(1)/a.png", "");
        createFile(contentRoot, "dir(1)/b.txt", "");
        createFile(contentRoot, "dir1/c.png", "");
        createFile(contentRoot, "generated/root.txt", "");
        createFile(contentRoot, "deep/nested/generated/n.txt", "");
        createFile(contentRoot, "deep/nested/ungenerated/u.txt", "");
        createFile(contentRoot, "upper/v.txt", "");
        createFile(contentRoot, "Lower_lit/w.txt", "");

        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            List<String> paths = new ArrayList<>();
            project.findResourcePaths("", paths);
            assertEquals(new HashSet<>(Arrays.asList(
                    ".defignore",
                    "game.project",
                    "test.png",
                    "levels/1/other.txt",
                    "levels/tiled/y.txt",
                    "levelsX/1/tiled/z.txt",
                    "aXb/k.txt",
                    "dir(1)/b.txt",
                    "dir1/c.png",
                    "deep/nested/ungenerated/u.txt",
                    "upper/v.txt",
                    "Lower_lit/w.txt")),
                    new HashSet<>(paths));

            List<String> ignoredPaths = new ArrayList<>();
            project.findResourcePaths("levels/1/tiled", ignoredPaths);
            assertTrue(ignoredPaths.isEmpty());

            List<String> dirs = new ArrayList<>();
            project.findResourceDirs("levels/1", dirs);
            assertTrue(dirs.isEmpty());

            List<String> levelDirs = new ArrayList<>();
            project.findResourceDirs("levels", levelDirs);
            assertEquals(new HashSet<>(Arrays.asList("1", "tiled")), new HashSet<>(levelDirs));

            List<String> nestedDirs = new ArrayList<>();
            project.findResourceDirs("deep/nested", nestedDirs);
            assertEquals(new HashSet<>(Arrays.asList("ungenerated")), new HashSet<>(nestedDirs));
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
        outputProps.load(new FileInputStream(contentRoot + "/build/game.projectc"));

        checkProjectSetting(outputProps, "project", "custom_property", null);
    }

    @Test
    public void testArchiveBuildMergesExtensionCustomResources() throws IOException, CompileExceptionError, MultipleCompileException, ParseException {
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ncustom_resources = /project_resource.txt\n");
        createFile(contentRoot, "project_resource.txt", "project");
        createFile(contentRoot, "extension1/ext.manifest", "name: Extension1\n");
        createFile(contentRoot, "extension1/ext.properties", "[project]\ncustom_resources.default = /extension1_resource.txt\n");
        createFile(contentRoot, "extension1_resource.txt", "extension1");
        createFile(contentRoot, "extension2/ext.manifest", "name: Extension2\n");
        createFile(contentRoot, "extension2/ext.properties", "[project]\ncustom_resources.default = /extension2_resource.txt\n");
        createFile(contentRoot, "extension2_resource.txt", "extension2");

        buildArchive(false);

        Manifest.ManifestData bundledManifestData = readManifestData(getBundledManifestFile());
        assertTrue(hasResource(bundledManifestData, "/project_resource.txt"));
        assertTrue(hasResource(bundledManifestData, "/extension1_resource.txt"));
        assertTrue(hasResource(bundledManifestData, "/extension2_resource.txt"));

        BobProjectProperties outputProperties = new BobProjectProperties();
        outputProperties.load(new FileInputStream(new File(contentRoot, "build/game.projectc")));
        String serializedCustomResourcesValue = outputProperties.getStringValue("project", "custom_resources");
        String[] serializedCustomResources = outputProperties.getStringArrayValue("project", "custom_resources", new String[0]);
        assertEquals(3, serializedCustomResourcesValue.split(", ", -1).length);
        assertEquals(new HashSet<>(Arrays.asList("/project_resource.txt", "/extension1_resource.txt", "/extension2_resource.txt")),
                     new HashSet<>(Arrays.asList(serializedCustomResources)));
    }

    @Test
    public void testArchiveBuildPreservesCustomResourcePathSpelling() throws IOException, CompileExceptionError, MultipleCompileException, ParseException {
        createDefaultFiles();
        createFile(contentRoot, "game.project", "[project]\ncustom_resources = foo/../assets/\n");
        createFile(contentRoot, "assets/resource.txt", "resource");

        buildArchive(false);

        assertTrue(new File(contentRoot, "build/assets/resource.txt").isFile());

        BobProjectProperties outputProperties = new BobProjectProperties();
        outputProperties.load(new FileInputStream(new File(contentRoot, "build/game.projectc")));
        assertEquals("foo/../assets/", outputProperties.getStringValue("project", "custom_resources"));
    }

    @Test
    public void testArchiveBuildWithLuaCustomResourceTreatsItAsLeaf() throws IOException, CompileExceptionError, MultipleCompileException {
        createDefaultFiles();
        createFile(contentRoot, "game.project",
                "[project]\n" +
                "custom_resources = /custom\n" +
                "\n" +
                "[display]\n" +
                "width=640\n" +
                "height=480\n");
        createFile(contentRoot, "custom/payload.lua",
                "function init(self)\n" +
                "end\n");

        buildArchive(false);

        assertTrue(new File(contentRoot, "build/custom/payload.lua").isFile());
        assertFalse(new File(contentRoot, "build/custom/payload.luac").exists());
    }

    // Verifies #13324: compiled Ogg stays uncompressed across skipped tasks, and toggling streaming rebuilds only the archive.
    @Test
    public void testIncrementalArchiveKeepsStreamedOggUncompressed() throws Exception {
        createDefaultFiles();
        createFile(contentRoot, "logic/main.collection",
                "name: \"main\"\n" +
                "instances { id: \"sfx\" prototype: \"/logic/sfx.go\" " +
                "position { x: 0 y: 0 z: 0 } rotation { x: 0 y: 0 z: 0 w: 1 } }\n");
        createFile(contentRoot, "logic/sfx.go", "components { id: \"sound\" component: \"/assets/short.sound\" }\n");
        createFile(contentRoot, "assets/short.sound", "sound: \"/assets/short.ogg\"\n");
        byte[] soundData;
        try (InputStream input = getClass().getResourceAsStream("/test/streaming-short.ogg")) {
            assertNotNull(input);
            soundData = input.readAllBytes();
        }
        Files.write(new File(contentRoot, "assets/short.ogg").toPath(), soundData);

        // First rebuild with streaming enabled in game.project; then toggle only the option with the setting disabled.
        boolean[] streamingOptions = {true, true, true, false, true};
        for (int buildIndex = 0; buildIndex < streamingOptions.length; ++buildIndex) {
            boolean streaming = streamingOptions[buildIndex];
            if (buildIndex < 3) {
                createFile(contentRoot, "game.project",
                        "[project]\ntitle = Streaming Archive " + buildIndex + "\n" +
                        "[sound]\nstream_enabled = " + (buildIndex < 2 ? 1 : 0) + "\n");
            }
            try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
                project.setPublisher(new NullPublisher(new PublisherSettings()));
                ClassLoaderScanner scanner = new ClassLoaderScanner();
                project.scan(scanner, "com.dynamo.bob");
                project.scan(scanner, "com.dynamo.bob.pipeline");
                project.setOption("archive", "true");
                project.setOption("sound-stream-enabled", Boolean.toString(streaming));

                TaskResult soundResult = null;
                TaskResult archiveResult = null;
                for (TaskResult result : project.build(Progress.discarding(), "build")) {
                    assertTrue(result.toString(), result.isOk());
                    if (result.getTask().getName().equals("Ogg")) {
                        soundResult = result;
                    }
                    if (result.getTask().getBuilder().isGameProjectBuilder()) {
                        archiveResult = result;
                    }
                }
                assertNotNull("Expected a sound task", soundResult);
                assertEquals(buildIndex == 0 ? TaskResult.Result.SUCCESS : TaskResult.Result.SKIPPED, soundResult.getResult());
                assertNotNull("Expected an archive task", archiveResult);
                assertEquals(TaskResult.Result.SUCCESS, archiveResult.getResult());
            }

            Manifest.ResourceEntry soundResource = readManifestData(getBundledManifestFile()).getResourcesList().stream()
                    .filter(entry -> entry.getUrlHash() == MurmurHash.hash64("/assets/short.oggc"))
                    .findFirst().orElseThrow();
            // Release manifests omit URLs, so match the archive entry by its content hash.
            byte[] soundHash = Arrays.copyOf(soundResource.getHash().getData().toByteArray(), ArchiveReader.HASH_BUFFER_BYTESIZE);
            ArchiveReader reader = new ArchiveReader(
                    new File(contentRoot, "build/game.arci").getAbsolutePath(),
                    new File(contentRoot, "build/game.arcd").getAbsolutePath(),
                    getBundledManifestFile().getAbsolutePath());
            try {
                reader.read();
                ArchiveEntry soundEntry = reader.getEntries().stream()
                        .filter(entry -> Arrays.equals(entry.getHash(), soundHash))
                        .findFirst().orElseThrow();
                assertEquals("Archive compression must match the streaming option",
                        streaming ? 0 : ArchiveEntry.FLAG_COMPRESSED, soundEntry.getFlags() & ArchiveEntry.FLAG_COMPRESSED);
                if (streaming) {
                    assertEquals(ArchiveEntry.FLAG_UNCOMPRESSED, soundEntry.getCompressedSize());
                    byte[] compiledSoundData = Files.readAllBytes(new File(contentRoot, "build/assets/short.oggc").toPath());
                    assertArrayEquals(compiledSoundData, reader.getEntryContent(soundEntry));
                }
            } finally {
                reader.close();
            }
        }
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
        outputProps.load(new FileInputStream(contentRoot + "/build/game.projectc"));

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

    // Archive preparation uses package writers without starting a publisher during compilation.
    @Test
    public void testArchiveBuildDoesNotStartPublisher() throws IOException, CompileExceptionError, MultipleCompileException {
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

        assertEquals(0, publisher.startCount);
        assertEquals(0, publisher.stopCount);
    }

    // An empty legacy package contains no manifest entries for resources already in the base archive.
    @Test
    public void testArchiveBuildWithoutExcludedResourcesKeepsPackageManifestEmpty() throws IOException, CompileExceptionError, MultipleCompileException {
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
        assertEquals(0, publishedManifestData.getResourcesCount());
        assertTrue(bundledManifestData.getResourcesCount() > 0);
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

    private void createLegacyPackageProject() throws IOException {
        createExcludedLiveUpdateProject(null);
        Files.writeString(Path.of(contentRoot, "game.project"), "\n[project]\ncustom_resources = /custom\n", java.nio.file.StandardOpenOption.APPEND);
        createFile(contentRoot, "custom/message.txt", "Custom content stays in the base.\n");
        createFile(contentRoot, "logic/shared.script", "function init(self) self.shared = true end");
        createFile(contentRoot, "logic/shared.go", "components { id: \"shared\" component: \"/logic/shared.script\" }");
        createFile(contentRoot, "logic/included.collection", collection("included", "/logic/shared.go"));
        createFile(contentRoot, "logic/included.collectionproxy", "collection: \"/logic/included.collection\" exclude: false");
        createFile(contentRoot, "logic/mixed.collection", "name: \"mixed\"");
        createFile(contentRoot, "logic/mixed-in.collectionproxy", "collection: \"/logic/mixed.collection\" exclude: false");
        createFile(contentRoot, "logic/mixed-out.collectionproxy", "collection: \"/logic/mixed.collection\" exclude: true");
        createFile(contentRoot, "logic/unused.collectionproxy", "collection: \"/logic/level.collection\" exclude: false");
        createFile(contentRoot, "logic/nested.collection", "name: \"nested\"");
        createFile(contentRoot, "logic/nested.collectionproxy", "collection: \"/logic/nested.collection\" exclude: false");
        Files.writeString(Path.of(contentRoot, "logic/level.collection"), "\ninstances { id: \"shared\" prototype: \"/logic/shared.go\" }", java.nio.file.StandardOpenOption.APPEND);
        Files.writeString(Path.of(contentRoot, "logic/level.go"), "\ncomponents { id: \"nested\" component: \"/logic/nested.collectionproxy\" }", java.nio.file.StandardOpenOption.APPEND);
        Files.writeString(Path.of(contentRoot, "logic/main.go"), """
                components { id: "included" component: "/logic/included.collectionproxy" }
                components { id: "mixed-in" component: "/logic/mixed-in.collectionproxy" }
                components { id: "mixed-out" component: "/logic/mixed-out.collectionproxy" }
                """, java.nio.file.StandardOpenOption.APPEND);
    }

    private static void publishLegacyOutput(Publisher publisher) throws Exception {
        publisher.start();
        try {
            publisher.publish(new ArchiveEntry("previous-release"), "previous release".getBytes(java.nio.charset.StandardCharsets.UTF_8));
        } finally {
            publisher.stop();
        }
    }

    // Adopting a legacy folder preserves old resources and unrelated files across rebuilds and mode changes.
    @Test
    public void testLegacyFolderPreservesExistingFiles() throws Exception {
        createLegacyPackageProject();
        Path folder = Path.of(contentRoot, "liveupdate_output/published");
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            publishLegacyOutput(new FolderPublisher(contentRoot, PublisherSettings.load(project.getResource("/liveupdate.settings"))));
        }
        Path notes = folder.resolve("notes/readme.txt");
        Files.createDirectories(notes.getParent());
        Files.writeString(notes, "User content");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(notes, unchanged);
        var plan = buildPackageProject(true, false);
        assertEquals("previous release", Files.readString(folder.resolve("previous-release")));
        assertEquals("User content", Files.readString(notes));
        assertEquals(unchanged, Files.getLastModifiedTime(notes));
        assertEquals(plan.packages().get("liveupdate").resources(), manifestPaths(readManifestData(getPublishedManifestFile())));
        Set<String> originalFiles;
        try (var files = Files.list(folder)) {
            originalFiles = files.map(path -> path.getFileName().toString()).collect(Collectors.toSet());
        }
        createFile(contentRoot, "logic/level.script", "function init(self) self.changed = true end");
        buildPackageProject(true, false);
        for (String name : originalFiles) assertTrue(name, Files.exists(folder.resolve(name)));
        assertEquals("User content", Files.readString(notes));
        // A later manifest build must not retire a folder containing unowned files.
        createPackageProject();
        savePackages(livePackage("published", "folder", addPath("Data", "/extra/.*")));
        setPackageOutputDirectory("liveupdate_output");
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("A manifest package must not replace an adopted legacy folder");
        } catch (AssertionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("overwrite an existing file or directory"));
        }
        setPackageOutputDirectory("packages");
        buildPackageProject(true, false);
        assertEquals("User content", Files.readString(notes));
        assertEquals("previous release", Files.readString(folder.resolve("previous-release")));
        assertTrue(PackageArtifacts.generatedPaths(Path.of(contentRoot)).contains("/liveupdate_output/published"));
        assertNoPackageStaging();
    }

    // Changing the legacy ZIP filename preserves earlier releases and keeps them out of future source matching.
    @Test
    public void testLegacyZipRetainsEarlierReleases() throws Exception {
        createLegacyPackageProject();
        Map<Path, byte[]> releases = new LinkedHashMap<>();
        for (String name : List.of("first", "second")) {
            createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Zip\nzip-filepath=legacy\nzip-filename=" + name + ".zip\n");
            buildPackageProject(true, false);
            Path output = Path.of(contentRoot, "legacy", name + ".zip");
            releases.put(output, Files.readAllBytes(output));
        }
        for (var release : releases.entrySet()) assertArrayEquals(release.getValue(), Files.readAllBytes(release.getKey()));
        createPackageProject();
        savePackages(livePackage("episode", "zip", addPath("Data and archives", "/.*[.](json|zip)")));
        var plan = buildPackageProject(true, false);
        assertEquals(Set.of("/extra/data.json"), plan.packages().get("episode").resources());
        for (var release : releases.entrySet()) assertArrayEquals(release.getValue(), Files.readAllBytes(release.getKey()));
        assertNoPackageStaging();
    }

    // Existing ZIP/folder bundle outputs created by the old publishers can be replaced without ownership receipts.
    @Test
    public void testLegacyBundleAdoptsExistingOutputs() throws Exception {
        createLegacyPackageProject();
        for (String mode : List.of("Zip", "Folder")) {
            createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=" + mode + "\nzip-filepath=legacy\nzip-filename=episode.zip\n"
                    + "output-directory=legacy\noutput-folder-name=episode\nsave-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n");
            buildPackageProject(true, false);
            Path bundle = Files.createDirectories(Path.of(contentRoot, "bundle-" + mode));
            PublisherSettings settings = new PublisherSettings();
            settings.setZipFilepath(bundle.toString());
            settings.setZipFilename("episode.zip");
            settings.setOutputDirectory(bundle.toString());
            settings.setOutputFolderName("episode");
            Path bundled = bundle.resolve(mode.equals("Zip") ? "episode.zip" : "episode");
            try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
                if (mode.equals("Zip")) {
                    Files.createDirectory(bundled);
                    Files.writeString(bundled.resolve("readme.txt"), "User content");
                    try {
                        BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                        org.junit.Assert.fail("A legacy ZIP must not replace an unrelated directory");
                    } catch (IOException expected) {
                        assertTrue(expected.getMessage(), expected.getMessage().contains("overwrite an existing bundle output"));
                    }
                    assertEquals("User content", Files.readString(bundled.resolve("readme.txt")));
                    FileUtils.deleteDirectory(bundled.toFile());
                }
                publishLegacyOutput(mode.equals("Zip") ? new ZipPublisher(contentRoot, settings) : new FolderPublisher(contentRoot, settings));
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                if (mode.equals("Zip")) {
                    assertArrayEquals(Files.readAllBytes(Path.of(contentRoot, "legacy/episode.zip")), Files.readAllBytes(bundled));
                } else {
                    assertFalse(Files.exists(bundled.resolve("previous-release")));
                    assertArrayEquals(Files.readAllBytes(Path.of(contentRoot, "legacy/episode/liveupdate.game.dmanifest")),
                            Files.readAllBytes(bundled.resolve("liveupdate.game.dmanifest")));
                }
                FileTime unchanged = FileTime.fromMillis(946684800000L);
                Files.setLastModifiedTime(bundled, unchanged);
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                assertEquals(unchanged, Files.getLastModifiedTime(bundled));
            }
            assertNoPackageStaging();
        }
    }

    // Legacy mixed routes, inherited exclusion and custom files keep their exact archive membership.
    // ZIP/folder providers use the same payloads and tight manifests while retaining configured names.
    @Test
    public void testLegacyPackageContentsAndIncrementalOutput() throws Exception {
        createLegacyPackageProject();
        Set<String> excluded = Set.of("/logic/level.collectionc", "/logic/level.goc", "/logic/level.scriptc",
                "/logic/nested.collectionproxyc", "/logic/nested.collectionc");
        Set<String> base = Set.of("/logic/main.collectionc", "/logic/main.goc", "/logic/level.collectionproxyc",
                "/logic/shared.goc", "/logic/shared.scriptc", "/logic/included.collectionproxyc", "/logic/included.collectionc",
                "/logic/mixed-in.collectionproxyc", "/logic/mixed-out.collectionproxyc", "/logic/mixed.collectionc", "/custom/message.txt",
                "/builtins/render/default.renderc", "/builtins/render/default.render_scriptc", "/builtins/render/default.display_profilesc",
                "/builtins/input/default.gamepadsc", "/input/game.input_bindingc");
        Map<String, byte[]> zipPayloads = new LinkedHashMap<>();
        for (String mode : List.of("Zip", "Folder")) {
            createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=" + mode + "\nzip-filepath=legacy\n"
                    + "zip-filename=nested/episode assets.zip\noutput-directory=legacy/nested\noutput-folder-name=episode assets\n"
                    + "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n");
            Path output = Path.of(contentRoot, "legacy/nested/episode assets" + (mode.equals("Zip") ? ".zip" : ""));
            // Existing legacy outputs predate ownership receipts and must remain valid destinations.
            if (mode.equals("Zip")) {
                Files.createDirectories(output.getParent());
                Files.writeString(output, "Previous legacy archive");
            } else {
                Files.createDirectories(output);
                Files.writeString(output.resolve("liveupdate.game.dmanifest"), "Previous legacy manifest");
            }
            PackageResolver.Plan plan = buildPackageProject(true, false);
            assertEquals(base, plan.base());
            assertEquals(excluded, plan.packages().get("liveupdate").resources());
            assertEquals(base, manifestPaths(readManifestData(getBundledManifestFile())));
            ArchiveReader reader = new ArchiveReader(Path.of(contentRoot, "build/game.arci").toString(),
                    Path.of(contentRoot, "build/game.arcd").toString(), getBundledManifestFile().toString());
            try {
                reader.read();
                Set<String> expectedHashes = readManifestData(getBundledManifestFile()).getResourcesList().stream()
                        .map(resource -> com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(resource.getHash().getData().toByteArray())).collect(Collectors.toSet());
                Set<String> storedHashes = reader.getEntries().stream()
                        .map(entry -> com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(Arrays.copyOf(entry.getHash(), 20))).collect(Collectors.toSet());
                assertEquals(expectedHashes, storedHashes);
            } finally {
                reader.close();
            }
            Map<String, byte[]> payloads = new LinkedHashMap<>();
            if (mode.equals("Zip")) {
                try (ZipFile zip = new ZipFile(output.toFile())) {
                    for (var entry : zip.stream().toList()) payloads.put(entry.getName(), zip.getInputStream(entry).readAllBytes());
                }
            } else {
                try (var files = Files.list(output)) {
                    for (Path file : files.toList()) payloads.put(file.getFileName().toString(), Files.readAllBytes(file));
                }
            }
            Manifest.ManifestData manifest = Manifest.ManifestData.parseFrom(Manifest.ManifestFile.parseFrom(payloads.remove("liveupdate.game.dmanifest")).getData());
            assertEquals(excluded, manifestPaths(manifest));
            Set<String> hashes = manifest.getResourcesList().stream()
                    .map(resource -> com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(resource.getHash().getData().toByteArray())).collect(Collectors.toSet());
            assertEquals(hashes, payloads.keySet());
            if (mode.equals("Zip")) zipPayloads.putAll(payloads);
            else for (String hash : hashes) assertArrayEquals(zipPayloads.get(hash), payloads.get(hash));
            FileTime unchanged = FileTime.fromMillis(946684800000L);
            Files.setLastModifiedTime(output, unchanged);
            buildPackageProject(true, false);
            assertEquals(unchanged, Files.getLastModifiedTime(output));
            try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
                Path bundle = Files.createDirectories(Path.of(contentRoot, "bundle-" + mode));
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                assertTrue(Files.exists(bundle.resolve(output.getFileName())));
                assertTrue(Files.exists(output));
            }
            if (mode.equals("Zip")) Files.delete(output);
            else Files.delete(output.resolve("liveupdate.game.dmanifest"));
            buildPackageProject(true, false);
            assertTrue(Files.exists(output));
            assertNoPackageStaging();
        }
        createFile(contentRoot, "logic/included.collectionproxy", "collection: \"/logic/included.collection\" exclude: true");
        Set<String> changedExcluded = new HashSet<>(excluded);
        changedExcluded.addAll(Set.of("/logic/included.collectionc", "/logic/shared.goc", "/logic/shared.scriptc"));
        assertEquals(changedExcluded, buildPackageProject(true, false).packages().get("liveupdate").resources());
        assertEquals(changedExcluded, manifestPaths(readManifestData(Path.of(contentRoot, "legacy/nested/episode assets/liveupdate.game.dmanifest").toFile())));
        Set<String> all = new HashSet<>(base);
        all.addAll(excluded);
        PackageResolver.Plan disabled = buildPackageProject(false, false);
        assertEquals(all, disabled.base());
        assertEquals(all, manifestPaths(readManifestData(getBundledManifestFile())));
        assertTrue(disabled.packages().get("liveupdate").resources().isEmpty());
        assertFalse(Files.exists(Path.of(contentRoot, "content.lumanifest")));
    }

    private void createPackageProject() throws IOException {
        createExcludedLiveUpdateProject("manifest = /content.lumanifest\n");
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Folder\nzip-filepath=packages\noutput-directory=packages\nzip-filename=ignored.zip\noutput-folder-name=ignored\n");
        createFile(contentRoot, "extra/data.json", "{\"ok\":true}");
        savePackages(
                livePackage("episode", "zip", addPath("Level", "/logic/level[.]collection"), addPath("Data", "/extra/.*[.](json|txt)")),
                livePackage("copy", "folder", LiveUpdateManifest.RuleDesc.newBuilder().setName("Copy").setPackage("episode").build()));
    }

    private static LiveUpdateManifest.RuleDesc addPath(String name, String path) {
        return LiveUpdateManifest.RuleDesc.newBuilder().setName(name).setPath(path).build();
    }

    private static LiveUpdateManifest.PackageDesc livePackage(String name, String provider, LiveUpdateManifest.RuleDesc... rules) {
        return LiveUpdateManifest.PackageDesc.newBuilder().setName(name).setProvider(provider).addAllRules(List.of(rules)).build();
    }

    private void savePackages(LiveUpdateManifest.PackageDesc... packages) throws IOException {
        createFile(contentRoot, "content.lumanifest", TextFormat.printer().printToString(
                LiveUpdateManifest.ManifestDesc.newBuilder().setOutputDirectory("packages").addAllPackages(List.of(packages)).build()));
    }

    private void setPackageOutputDirectory(String directory) throws IOException {
        Path path = Path.of(contentRoot, "content.lumanifest");
        var manifest = com.dynamo.bob.archive.liveupdate.PackageBuilder.parse(Files.readAllBytes(path));
        Files.writeString(path, TextFormat.printer().printToString(manifest.toBuilder().setOutputDirectory(directory).build()));
    }

    private void setProviderProperties(String provider, Map<String, String> values) throws IOException {
        Path path = Path.of(contentRoot, "content.lumanifest");
        var manifest = com.dynamo.bob.archive.liveupdate.PackageBuilder.parse(Files.readAllBytes(path));
        var settings = LiveUpdateManifest.ProviderSettings.newBuilder().setProvider(provider);
        values.forEach((name, value) -> settings.addProperties(LiveUpdateManifest.ProviderProperty.newBuilder().setName(name).setValue(value)));
        Files.writeString(path, TextFormat.printer().printToString(manifest.toBuilder().clearProviders()
                .addAllProviders(manifest.getProvidersList().stream().filter(item -> !item.getProvider().equals(provider)).toList())
                .addProviders(settings).build()));
    }

    private PackageResolver.Plan buildPackageProject(boolean export, boolean preview, String... commands) throws Exception {
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");
            project.setOption("archive", "true");
            project.setOption("variant", "debug");
            project.setOption("liveupdate", Boolean.toString(export));
            project.setOption("package-preview", Boolean.toString(preview));
            project.setOption("with-symbols", null); // Match CLI flags that have no argument.
            for (TaskResult result : project.build(Progress.discarding(), commands.length == 0 ? new String[]{"build"} : commands)) assertTrue(result.toString(), result.isOk());
            return project.getLiveUpdatePlan();
        }
    }

    private Manifest.ManifestData zipManifest(String name) throws IOException {
        try (ZipFile zip = new ZipFile(new File(contentRoot, "packages/" + name + ".zip"))) {
            return Manifest.ManifestData.parseFrom(Manifest.ManifestFile.parseFrom(zip.getInputStream(zip.getEntry("liveupdate.game.dmanifest"))).getData());
        }
    }

    private Project packageReportProject(boolean export) {
        return packageReportProject(export, false);
    }

    private Project packageReportProject(boolean export, boolean failBundle) {
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build") {
            @Override
            public IBundler createBundler(Platform platform) {
                return new LinuxBundler() {
                    @Override
                    public void bundleApplication(Project project, Platform platform, File directory, ICanceled canceled) throws IOException {
                        if (failBundle) throw new IOException("Deliberate bundle failure");
                        Files.writeString(directory.toPath().resolve("application"), "complete");
                    }
                };
            }
        };
        project.setOption("archive", "true");
        project.setOption("variant", "release");
        project.setOption("liveupdate", Boolean.toString(export));
        project.setOption("bundle-output", Path.of(contentRoot, "build/bundle").toString());
        project.setOption("bundle-report-html", Path.of(contentRoot, "report.html").toString());
        return project;
    }

    private com.fasterxml.jackson.databind.JsonNode packageReport() throws IOException {
        String html = Files.readString(Path.of(contentRoot, "report.html"));
        String marker = "var report_data = ";
        int start = html.indexOf(marker) + marker.length();
        int end = html.indexOf(";", start);
        return new com.fasterxml.jackson.databind.ObjectMapper().readTree(html.substring(start, end));
    }

    // Reports include exact package membership and actual ZIP/folder/custom output sizes, safely embedded in standalone HTML.
    @Test
    public void testPackageBundleReportContentsAndSizes() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/description.txt", "välkommen!\n");
        Files.writeString(Path.of(contentRoot, "game.project"), "\n[project]\ntitle=</script><script>alert('report')</script>\n",
                java.nio.file.StandardOpenOption.APPEND);
        savePackages(livePackage("episode", "zip", addPath("Startup", "/logic/main[.]collection"), addPath("Data", "/extra/.*")),
                livePackage("copy", "folder", LiveUpdateManifest.RuleDesc.newBuilder().setName("Copy").setPackage("episode").build()),
                livePackage("custom", "test-package", addPath("Data", "/extra/.*")), livePackage("empty", "zip"));
        PackageResolver.Plan plan;
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) assertTrue(result.toString(), result.isOk());
            plan = project.getLiveUpdatePlan();
        }
        var report = packageReport();
        assertEquals(5, report.get("packages").size());
        Map<String, com.fasterxml.jackson.databind.JsonNode> resources = new LinkedHashMap<>();
        for (var row : report.get("package_resources")) {
            String path = row.get("file").asText();
            resources.put(path, row);
            assertEquals(Files.size(Path.of(contentRoot, "build", path.substring(1))), row.get("size").asLong());
        }
        assertEquals(plan.emittedResources(), resources.keySet());
        assertEquals(List.of("episode", "copy", "custom"),
                new com.fasterxml.jackson.databind.ObjectMapper().convertValue(resources.get("/extra/data.json").get("packages"), List.class));
        assertEquals(List.of("base", "episode", "copy"),
                new com.fasterxml.jackson.databind.ObjectMapper().convertValue(resources.get("/logic/main.collectionc").get("packages"), List.class));
        for (var pkg : report.get("packages")) {
            String name = pkg.get("name").asText();
            Set<String> expected = name.equals("base") ? plan.base() : plan.packages().get(name).resources();
            Set<String> actual = new HashSet<>();
            long compiledSize = 0;
            for (var path : pkg.get("resources")) {
                actual.add(path.asText());
                compiledSize += resources.get(path.asText()).get("size").asLong();
            }
            assertEquals(expected, actual);
            assertEquals(compiledSize, pkg.get("size").asLong());
            long outputSize = 0;
            for (var output : pkg.get("outputs")) {
                Path path = Path.of(output.get("path").asText());
                long size = 0;
                try (var files = Files.walk(path)) {
                    for (Path file : files.filter(Files::isRegularFile).toList()) size += Files.size(file);
                }
                assertEquals(size, output.get("size").asLong());
                outputSize += size;
            }
            assertEquals(outputSize, pkg.get("output_size").asLong());
            if (name.equals("custom")) assertEquals(2, pkg.get("outputs").size());
        }
        String html = Files.readString(Path.of(contentRoot, "report.html"));
        assertTrue(html.contains("id=\"package-filter\""));
        assertTrue(html.contains("id=\"resource-search\""));
        assertTrue(html.contains("fuzzyMatch"));
        assertFalse(html.contains("</script><script>alert('report')</script>"));
        assertTrue(html.contains("\\u003c/script\\u003e"));
        assertFalse(java.util.regex.Pattern.compile("(?i)<script[^>]+src\\s*=|<link[^>]+href\\s*=").matcher(html).find());
    }

    // Incremental builds and bundle-only invocations report cached package data without compiling or rewriting packages.
    @Test
    public void testPackageReportsUseCachedBuildData() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) assertTrue(result.toString(), result.isOk());
        }
        var expected = packageReport();
        Path cache = Path.of(contentRoot, "build/game.lureport.json");
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(cache, unchanged);
        Files.setLastModifiedTime(zip, unchanged);
        for (String[] commands : List.of(new String[]{"build", "bundle"}, new String[]{"bundle"})) {
            Files.delete(Path.of(contentRoot, "report.html"));
            try (Project project = packageReportProject(true)) {
                // The bundle command uses existing archives and does not require the build's --archive flag.
                if (commands.length == 1) project.setOption("archive", "false");
                for (TaskResult result : project.build(Progress.discarding(), commands)) assertTrue(result.toString(), result.isOk());
                assertNull(project.getLiveUpdatePlan());
                assertNull(project.getArchiveBuilder());
            }
            assertEquals(expected.get("packages"), packageReport().get("packages"));
            assertEquals(expected.get("package_resources"), packageReport().get("package_resources"));
            assertEquals(unchanged, Files.getLastModifiedTime(cache));
            assertEquals(unchanged, Files.getLastModifiedTime(zip));
            assertTrue(Files.size(Path.of(contentRoot, "report.html")) > 0);
        }
        // Previously generated reports remain excluded when reports are no longer requested.
        savePackages(livePackage("data", "zip", addPath("Files", "/(?:report.*|extra/.*)[.](html|json)")));
        var plan = buildPackageProject(true, false);
        assertEquals(Set.of("/extra/data.json"), plan.packages().get("data").resources());
    }

    // Disabled export reports selected resources in the base and does not count artifacts retained from an earlier export.
    @Test
    public void testPackageReportWithExportDisabled() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        try (Project project = packageReportProject(false)) {
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) assertTrue(result.toString(), result.isOk());
            assertTrue(PackageReport.resource(project).exists());
        }
        var report = packageReport();
        assertEquals(report.get("package_resources").size(), report.get("packages").get(0).get("resources").size());
        for (var pkg : report.get("packages")) {
            if (!pkg.get("name").asText().equals("base")) {
                assertEquals(0, pkg.get("outputs").size());
                assertTrue(pkg.get("output_size").isNull());
            }
        }
        assertTrue(Files.isRegularFile(Path.of(contentRoot, "packages/episode.zip")));
    }

    // Build reports belong to build even if bundling fails; bundle reports are written only after a successful bundle.
    @Test
    public void testBuildAndBundleReportCommands() throws Exception {
        createPackageProject();
        Path buildReport = Path.of(contentRoot, "build-report.html");
        Path bundleReport = Path.of(contentRoot, "report.html");
        try (Project project = packageReportProject(true)) {
            project.setOption("build-report-html", buildReport.toString());
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        assertTrue(Files.size(buildReport) > 0);
        assertFalse(Files.exists(bundleReport));
        Files.delete(buildReport);
        try (Project project = packageReportProject(true, true)) {
            project.setOption("build-report-html", buildReport.toString());
            try {
                project.build(Progress.discarding(), "build", "bundle");
                org.junit.Assert.fail("Expected bundle failure");
            } catch (Exception expected) {
                assertTrue(expected.getMessage().contains("Deliberate bundle failure"));
            }
        }
        assertTrue(Files.size(buildReport) > 0);
        assertFalse(Files.exists(bundleReport));
        byte[] expected = Files.readAllBytes(buildReport);
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(buildReport, unchanged);
        try (Project project = packageReportProject(true)) {
            project.setOption("build-report-html", buildReport.toString());
            project.setOption("archive", "false");
            for (TaskResult result : project.build(Progress.discarding(), "bundle")) assertTrue(result.toString(), result.isOk());
        }
        assertArrayEquals(expected, Files.readAllBytes(buildReport));
        assertEquals(unchanged, Files.getLastModifiedTime(buildReport));
        assertTrue(Files.size(bundleReport) > 0);
        assertFalse(Files.exists(Path.of(contentRoot, "excluded_report.html")));
        assertFalse(Files.exists(Path.of(contentRoot, "report_time.html")));
        assertEquals(3, packageReport().get("packages").size());
    }

    private static class PackagePublication extends AmazonS3Client implements AutoCloseable {
        final List<String> events = new ArrayList<>();
        final Map<String, byte[]> uploaded = new LinkedHashMap<>();
        int bundles;
        int failBundle;
        int uploads;
        int failUpload;
        Runnable duringUpload;
        boolean cancelAfterBundle;
        final AtomicBoolean canceled = new AtomicBoolean();

        PackagePublication() {
            super(new AnonymousAWSCredentials());
        }

        @Override
        public PutObjectResult putObject(PutObjectRequest request) {
            events.add(request.getKey());
            assertEquals("package-test-bucket", request.getBucketName());
            if (++uploads == failUpload) throw new AmazonClientException("Deliberate upload failure");
            try {
                InputStream input = request.getInputStream();
                // Exercise the same repeatable stream supplied to the SDK, including cancellation during a read.
                assertTrue(input.markSupported());
                input.mark(1);
                input.read();
                if (duringUpload != null) duringUpload.run();
                input.reset();
                byte[] bytes = input.readAllBytes();
                assertEquals(bytes.length, request.getMetadata().getContentLength());
                uploaded.put(request.getKey(), bytes);
            } catch (IOException exception) {
                throw new AmazonClientException(exception);
            }
            return new PutObjectResult();
        }

        @Override
        public void shutdown() {
            events.add("stop");
            super.shutdown();
        }

        @Override
        public void close() {
            super.shutdown();
        }
    }

    private Project amazonPackageProject(PackagePublication publication) throws Exception {
        createFile(contentRoot, "liveupdate.settings", """
                [liveupdate]
                mode=Amazon
                zip-filepath=packages
                output-directory=packages
                amazon-credential-profile=package-test-profile
                amazon-bucket=package-test-bucket
                amazon-prefix=releases/v1
                """);
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build") {
            @Override
            public void createPublisher() throws CompileExceptionError {
                super.createPublisher();
                if (getPublisher() instanceof AWSPublisher) {
                    setPublisher(new AWSPublisher(getPublisher().getPublisherSettings()) {
                        @Override
                        public void start() {
                            publication.events.add("start");
                            try {
                                var client = AWSPublisher.class.getDeclaredField("client");
                                client.setAccessible(true);
                                client.set(this, publication);
                            } catch (ReflectiveOperationException exception) {
                                throw new AssertionError(exception);
                            }
                        }
                    });
                }
            }

            @Override
            public IBundler createBundler(Platform platform) {
                return new HTML5Bundler() {
                    @Override
                    public void bundleApplication(Project project, Platform platform, File directory, ICanceled canceled) throws IOException {
                        publication.events.add("bundle" + ++publication.bundles);
                        if (publication.bundles == publication.failBundle) throw new IOException("Deliberate bundle failure");
                        Files.writeString(directory.toPath().resolve("bundle" + publication.bundles), "complete");
                        if (publication.cancelAfterBundle) publication.canceled.set(true);
                    }
                };
            }
        };
        ClassLoaderScanner scanner = new ClassLoaderScanner();
        project.scan(scanner, "com.dynamo.bob");
        project.scan(scanner, "com.dynamo.bob.pipeline");
        project.setOption("archive", "true");
        project.setOption("variant", "debug");
        project.setOption("liveupdate", "true");
        project.setOption("bundle-output", Path.of(contentRoot, "bundle").toString());
        return project;
    }

    private static Progress publicationProgress(PackagePublication publication) {
        return new Progress(new Progress.Reporter() {
            @Override
            public void report(IProgress.Message message, double fraction) {}

            @Override
            public boolean isCanceled() { return publication.canceled.get(); }
        });
    }

    // Amazon receives exact finalized ZIP/folder/custom artifacts once, after every bundle, never during compilation.
    @Test
    public void testAmazonPackagesPublishAfterAllBundles() throws Exception {
        createPackageProject();
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/.*")),
                livePackage("copy", "folder", addPath("Data", "/extra/.*")),
                livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
            assertTrue(publication.events.isEmpty());
            assertTrue(project.getPublisher() instanceof AWSPublisher);
            // Even an explicitly early publish command waits for both bundle commands.
            project.build(Progress.discarding(), "publish", "bundle", "bundle");
            assertEquals(List.of("bundle1", "bundle2", "start"), publication.events.subList(0, 3));
            assertEquals("stop", publication.events.getLast());
            Map<String, byte[]> expected = new LinkedHashMap<>();
            try (var files = Files.walk(Path.of(contentRoot, "packages"))) {
                for (Path file : files.filter(Files::isRegularFile).toList()) {
                    expected.put("releases/v1/" + Path.of(contentRoot, "packages").relativize(file).toString().replace('\\', '/'), Files.readAllBytes(file));
                }
            }
            assertEquals(expected.keySet(), publication.uploaded.keySet());
            assertEquals(expected.size(), publication.uploads);
            for (var entry : expected.entrySet()) assertArrayEquals(entry.getKey(), entry.getValue(), publication.uploaded.get(entry.getKey()));
        }
    }

    // Legacy Amazon publication retains hash-named resources and the manifest key, and waits for all bundles.
    @Test
    public void testLegacyAmazonPackagesPublishAfterAllBundles() throws Exception {
        createLegacyPackageProject();
        for (String mode : List.of("Zip", "Folder")) {
            createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=" + mode + "\nzip-filepath=legacy\nzip-filename=episode.zip\n"
                    + "output-directory=legacy\noutput-folder-name=episode\n");
            if (mode.equals("Folder")) createFile(contentRoot, "legacy/episode/notes.txt", "User content");
            buildPackageProject(true, false);
            try (PackagePublication publication = new PackagePublication();
                 Project project = amazonPackageProject(publication)) {
                try {
                    project.build(Progress.discarding(), "publish");
                    org.junit.Assert.fail("Changing a legacy " + mode + " publisher to Amazon requires prepared resource files");
                } catch (CompileExceptionError expected) {
                    assertTrue(expected.getMessage().contains("run build before publishing"));
                }
                assertTrue(publication.events.isEmpty());
            }
        }
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
            assertTrue(publication.events.isEmpty());
            publication.failBundle = 2;
            try {
                project.build(Progress.discarding(), "bundle", "bundle");
                org.junit.Assert.fail("Expected second bundle failure");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("Deliberate bundle failure"));
            }
            assertEquals(List.of("bundle1", "bundle2"), publication.events);
            publication.failBundle = 0;
            publication.bundles = 0;
            publication.events.clear();
            project.build(Progress.discarding(), "publish", "bundle", "bundle");
            assertEquals(List.of("bundle1", "bundle2", "start"), publication.events.subList(0, 3));
            PackageWriter.Output output = PackageWriter.readOutputs(project).getFirst();
            assertFalse(output.bundle());
            Manifest.ManifestData manifest = readManifestData(output.path().resolve("liveupdate.game.dmanifest").toFile());
            assertEquals(project.getLiveUpdatePlan().packages().get("liveupdate").resources(), manifestPaths(manifest));
            Map<String, byte[]> expected = new LinkedHashMap<>();
            for (Manifest.ResourceEntry resource : manifest.getResourcesList()) {
                String hash = com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(resource.getHash().getData().toByteArray());
                expected.put("releases/v1/" + hash, Files.readAllBytes(output.path().resolve(hash)));
            }
            expected.put("releases/v1/null", Files.readAllBytes(output.path().resolve("liveupdate.game.dmanifest")));
            assertEquals(expected.keySet(), publication.uploaded.keySet());
            for (var file : expected.entrySet()) assertArrayEquals(file.getValue(), publication.uploaded.get(file.getKey()));
            int uploaded = publication.uploads;
            project.build(Progress.discarding(), "publish");
            assertEquals(uploaded * 2, publication.uploads);
            assertEquals("User content", Files.readString(Path.of(contentRoot, "legacy/episode/notes.txt")));
        }
    }

    // Amazon staging must not replace an additive legacy folder at the same path or upload its retained files.
    @Test
    public void testLegacyAmazonPreservesFolderOutput() throws Exception {
        createLegacyPackageProject();
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Folder\noutput-directory=build\noutput-folder-name=liveupdate\n");
        createFile(contentRoot, "build/liveupdate/notes.txt", "User content");
        buildPackageProject(true, false);
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            List<TaskResult> results = project.build(Progress.discarding(), "build", "publish");
            assertTrue(results.stream().anyMatch(result -> !result.isOk() && result.toString().contains("overwrite an existing file or directory")));
            assertTrue(publication.events.isEmpty());
        }
        assertEquals("User content", Files.readString(Path.of(contentRoot, "build/liveupdate/notes.txt")));
        assertNoPackageStaging();
    }

    // A failed second bundle, failed build, disabled export, preview, or cancellation must not start remote publishing.
    @Test
    public void testAmazonPublicationRequiresSuccessfulBundles() throws Exception {
        createPackageProject();
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            publication.failBundle = 2;
            try {
                project.build(Progress.discarding(), "build", "bundle", "bundle");
                org.junit.Assert.fail("Expected second bundle failure");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("Deliberate bundle failure"));
            }
            assertEquals(List.of("bundle1", "bundle2"), publication.events);
            publication.cancelAfterBundle = true;
            try {
                project.build(publicationProgress(publication), "bundle");
                org.junit.Assert.fail("Expected cancellation after bundling");
            } catch (CompileExceptionError expected) {
                assertTrue(publication.canceled.get());
            }
            publication.canceled.set(false);
            publication.cancelAfterBundle = false;
            project.setOption("liveupdate", "false");
            project.build(Progress.discarding(), "build", "bundle", "publish");
            project.setOption("liveupdate", "true");
            project.setOption("package-preview", "true");
            project.build(Progress.discarding(), "build", "publish");
            project.setOption("package-preview", "false");
            savePackages(livePackage("broken", "zip", addPath("Invalid", "[")));
            try {
                List<TaskResult> results = project.build(Progress.discarding(), "build", "bundle");
                assertTrue(results.stream().anyMatch(result -> !result.isOk()));
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("Invalid"));
            }
            assertFalse(publication.events.contains("start"));
            assertEquals(0, publication.uploads);
        }
    }

    // Engine-only bundles need no package receipt and must not upload stale packages; explicit publishing still works.
    @Test
    public void testAmazonPackagesSkipEngineOnlyBundles() throws Exception {
        createPackageProject();
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            project.setOption("exclude-archive", "true");
            project.build(Progress.discarding(), "build", "bundle");
            assertFalse(PackageWriter.receipt(project).exists());
            assertEquals(List.of("bundle1"), publication.events);
            project.setOption("exclude-archive", "false");
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
            project.setOption("exclude-archive", "true");
            project.build(Progress.discarding(), "bundle");
            assertEquals(List.of("bundle1", "bundle2"), publication.events);
            assertEquals(0, publication.uploads);
            project.build(Progress.discarding(), "publish");
            assertTrue(publication.uploaded.containsKey("releases/v1/episode.zip"));
        }
    }

    // Upload failures and cancellation during an active stream close the client and allow retry without rebuilding or rebundling.
    @Test
    public void testAmazonPublicationRetryAndActiveCancellation() throws Exception {
        createPackageProject();
        try (PackagePublication publication = new PackagePublication()) {
            try (Project project = amazonPackageProject(publication)) {
                publication.failUpload = 2;
                try {
                    project.build(Progress.discarding(), "build", "bundle");
                    org.junit.Assert.fail("Expected upload failure");
                } catch (CompileExceptionError expected) {
                    assertTrue(expected.getMessage().contains("retry with the publish command"));
                }
                assertEquals("stop", publication.events.getLast());
                assertEquals(1, publication.uploaded.size());
                Path archive = Path.of(contentRoot, "packages/episode.zip");
                FileTime unchanged = FileTime.fromMillis(946684800000L);
                Files.setLastModifiedTime(archive, unchanged);
                assertTrue(Files.isRegularFile(Path.of(contentRoot, "build/_BobBuildState_")));
                publication.duringUpload = () -> publication.canceled.set(true);
                int uploadsBefore = publication.uploads;
                try {
                    project.build(publicationProgress(publication), "publish");
                    org.junit.Assert.fail("Expected active upload cancellation");
                } catch (CompileExceptionError expected) {
                    assertTrue(expected.getMessage().contains("cancelled"));
                }
                assertEquals(uploadsBefore + 1, publication.uploads);
                assertEquals("stop", publication.events.getLast());
                publication.duringUpload = null;
                publication.canceled.set(false);
                // Retry in a fresh Bob project, using only the saved receipt and current destination settings.
            }
            try (Project retry = amazonPackageProject(publication)) {
                retry.build(Progress.discarding(), "publish");
                assertEquals(1, publication.bundles);
                assertTrue(publication.uploaded.containsKey("releases/v1/copy/liveupdate.game.dmanifest"));
                assertEquals(FileTime.fromMillis(946684800000L), Files.getLastModifiedTime(Path.of(contentRoot, "packages/episode.zip")));
                assertEquals("stop", publication.events.getLast());
            }
        }
    }

    // Validate every artifact and the receipt before connecting to Amazon, including files not copied into application bundles.
    @Test
    public void testAmazonPublicationRejectsIncompleteArtifacts() throws Exception {
        createPackageProject();
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            project.build(Progress.discarding(), "build");
            Path folderManifest = Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest");
            Files.writeString(folderManifest, "modified");
            try {
                project.build(Progress.discarding(), "publish");
                org.junit.Assert.fail("Expected artifact validation failure");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("missing or modified"));
            }
            Files.delete(Path.of(PackageWriter.receipt(project).getAbsPath()));
            try {
                project.build(Progress.discarding(), "publish");
                org.junit.Assert.fail("Expected missing receipt failure");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("receipt is missing"));
            }
            assertTrue(publication.events.isEmpty());
        }
    }

    // Distinct local outputs may share an export name; reject duplicate S3 keys before any artifact is uploaded.
    @Test
    public void testAmazonPublicationRejectsCollidingKeys() throws Exception {
        createPackageProject();
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/.*")),
                livePackage("copy", "zip", addPath("Data", "/extra/.*")));
        try (PackagePublication publication = new PackagePublication();
             Project project = amazonPackageProject(publication)) {
            project.build(Progress.discarding(), "build");
            List<PackageWriter.Output> outputs = PackageWriter.readOutputs(project);
            PackageWriter.Output first = outputs.get(0);
            PackageWriter.Output second = outputs.get(1);
            // Preserve both validated artifacts while giving them the same relative publishing destination.
            PackageWriter.receipt(project).setContent(PackageWriter.serializeOutputs(project, List.of(first,
                    new PackageWriter.Output(second.path(), first.name(), false, false, second.inputSignature(), second.contentHash(), second.legacy()))));
            try {
                project.build(Progress.discarding(), "publish");
                org.junit.Assert.fail("Expected a publication name collision");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage().contains("Colliding Live Update publication outputs"));
            }
            assertTrue(publication.events.isEmpty());
        }
    }

    private static Set<String> manifestPaths(Manifest.ManifestData manifest) {
        return manifest.getResourcesList().stream().map(Manifest.ResourceEntry::getUrl).collect(Collectors.toSet());
    }

    // Missing custom files warn or are ignored, while Error prevents writing until a new matching file appears.
    @Test
    public void testPackageNoMatchPolicies() throws Exception {
        createPackageProject();
        var required = addPath("Required file", "/extra/required[.]txt").toBuilder();
        for (var policy : List.of(LiveUpdateManifest.NoMatchPolicy.WARN, LiveUpdateManifest.NoMatchPolicy.IGNORE)) {
            savePackages(livePackage("episode", "zip", addPath("Data", "/extra/data[.]json"), required.setNoMatch(policy).build()));
            var plan = buildPackageProject(true, false);
            var diagnostics = plan.diagnostics().stream().filter(d -> d.packageName().equals("episode") && d.rule() == 1).toList();
            if (policy == LiveUpdateManifest.NoMatchPolicy.WARN) {
                assertEquals(1, diagnostics.size());
                assertEquals(PackageResolver.Severity.WARNING, diagnostics.get(0).severity());
            } else {
                assertTrue(diagnostics.isEmpty());
            }
            assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest("episode")));
        }
        Path archive = Path.of(contentRoot, "packages/episode.zip");
        byte[] previous = Files.readAllBytes(archive);
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/data[.]json"),
                required.setNoMatch(LiveUpdateManifest.NoMatchPolicy.ERROR).build()));
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Required custom file should fail when absent");
        } catch (CompileExceptionError expected) {
            assertEquals("Rule 'Required file' in package 'episode' matches no source files for Path '/extra/required[.]txt'. "
                    + "Patterns match the entire source path, including its leading '/'.", expected.getMessage());
        }
        assertArrayEquals(previous, Files.readAllBytes(archive));
        createFile(contentRoot, "extra/required.txt", "Required custom content\n");
        var plan = buildPackageProject(true, false);
        assertFalse(plan.hasErrors());
        assertEquals(Set.of("/extra/data.json", "/extra/required.txt"), manifestPaths(zipManifest("episode")));
    }

    // Manifest packages have exact local manifests and identical ZIP/folder payloads, including unreachable custom files.
    @Test
    public void testPackageZipFolderAndBaseContents() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/description.txt", "Episode 00: The forest — välkommen!\n");
        PackageResolver.Plan plan = buildPackageProject(true, false);
        Set<String> packaged = Set.of("/logic/level.collectionc", "/logic/level.goc", "/logic/level.scriptc", "/extra/data.json", "/extra/description.txt");
        Manifest.ManifestData zipManifest = zipManifest("episode");
        Manifest.ManifestData folderManifest = readManifestData(new File(contentRoot, "packages/copy/liveupdate.game.dmanifest"));
        assertEquals(packaged, manifestPaths(zipManifest));
        assertEquals(packaged, manifestPaths(folderManifest));
        assertEquals(packaged, plan.packages().get("episode").resources());
        assertEquals(Set.of("/logic/main.collectionc", "/logic/main.goc", "/logic/level.collectionproxyc",
                "/builtins/render/default.renderc", "/builtins/render/default.render_scriptc",
                "/builtins/render/default.display_profilesc", "/builtins/input/default.gamepadsc", "/input/game.input_bindingc"),
                manifestPaths(readManifestData(getBundledManifestFile())));
        assertEquals("episode", zipManifest.getPackageMetadata().getName());
        assertEquals("copy", folderManifest.getPackageMetadata().getName());
        assertEquals(2, zipManifest.getPackageMetadata().getOutputsCount());
        assertEquals(0, zipManifest.getPackageMetadata().getRequirementsCount());
        Set<String> expectedFiles = zipManifest.getResourcesList().stream()
                .map(resource -> com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(resource.getHash().getData().toByteArray()))
                .collect(Collectors.toSet());
        expectedFiles.add("liveupdate.game.dmanifest");
        Path folder = Path.of(contentRoot, "packages/copy");
        try (var files = Files.walk(folder)) {
            assertEquals(expectedFiles, files.filter(path -> !path.equals(folder))
                    .map(path -> folder.relativize(path).toString().replace('\\', '/')).collect(Collectors.toSet()));
        }
        try (ZipFile zip = new ZipFile(new File(contentRoot, "packages/episode.zip"))) {
            assertEquals(expectedFiles, zip.stream().map(entry -> entry.getName()).collect(Collectors.toSet()));
            assertEquals(expectedFiles.size(), zip.size());
            for (Manifest.ResourceEntry resource : zipManifest.getResourcesList()) {
                String hash = com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(resource.getHash().getData().toByteArray());
                byte[] payload = zip.getInputStream(zip.getEntry(hash)).readAllBytes();
                assertArrayEquals(payload, Files.readAllBytes(new File(contentRoot, "packages/copy/" + hash).toPath()));
                assertEquals(resource.getSize(), java.nio.ByteBuffer.wrap(payload).getInt());
                assertArrayEquals(resource.getHash().getData().toByteArray(), java.security.MessageDigest.getInstance("SHA-1").digest(Arrays.copyOfRange(payload, 16, payload.length)));
                if (resource.getUrl().equals("/extra/description.txt")) {
                    assertArrayEquals(Files.readAllBytes(new File(contentRoot, "extra/description.txt").toPath()),
                            Arrays.copyOfRange(payload, 16, payload.length));
                }
            }
        }
        assertFalse(new File(contentRoot, "packages/ignored.zip").exists());
        assertNoPackageStaging();
    }

    // Bundling copies every declared artifact, including custom sidecars, without deleting outputs already at the destination.
    @Test
    public void testPackageBundlePlacement() throws Exception {
        createPackageProject();
        Files.writeString(new File(contentRoot, "liveupdate.settings").toPath(),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/.*")),
                livePackage("copy", "folder", addPath("Data", "/extra/.*")),
                livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        buildPackageProject(true, false);
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("package-manifest", "/content.lumanifest");
            File bundle = new File(contentRoot, "bundle");
            BundleHelper.moveBundleIfNeed(project, bundle);
            assertTrue(new File(bundle, "episode.zip").isFile());
            assertTrue(new File(bundle, "copy/liveupdate.game.dmanifest").isFile());
            assertArrayEquals(Files.readAllBytes(new File(contentRoot, "packages/custom.custom").toPath()),
                    Files.readAllBytes(new File(bundle, "custom.custom").toPath()));
            assertEquals("{\"package\":\"custom\"}", Files.readString(new File(bundle, "metadata/custom.json").toPath()));
            BundleHelper.moveBundleIfNeed(project, new File(contentRoot, "packages"));
            assertTrue(new File(contentRoot, "packages/copy/liveupdate.game.dmanifest").isFile());
        }
    }

    // Package archives must retain the SSL certificate copy performed by the legacy archive path.
    @Test
    public void testPackageSslCertificates() throws Exception {
        createPackageProject();
        File settings = new File(contentRoot, "game.project");
        Files.writeString(settings.toPath(), Files.readString(settings.toPath()) + "\n[network]\nssl_certificates = certs.pem\n");
        createFile(contentRoot, "certs.pem", "certificate");
        buildPackageProject(true, false);
        assertEquals("certificate", Files.readString(new File(contentRoot, "build/" + BundleHelper.SSL_CERTIFICATES_NAME).toPath()));
    }

    // Disabling export keeps all selected content in the base and preview resolves the same plan without calling writers.
    @Test
    public void testPackageExportDisabledAndPreview() throws Exception {
        createPackageProject();
        PackageResolver.Plan disabled = buildPackageProject(false, false);
        assertEquals(disabled.emittedResources(), manifestPaths(readManifestData(getBundledManifestFile())));
        assertFalse(new File(contentRoot, "packages").exists());
        PackageResolver.Plan preview = buildPackageProject(true, true);
        assertFalse(new File(contentRoot, "packages").exists());
        PackageResolver.Plan exported = buildPackageProject(true, false);
        assertEquals(preview.base(), exported.base());
        assertEquals(preview.packages(), exported.packages());
        assertNoPackageStaging();
    }

    // Preview reports provider-declared export paths using prepared resource metadata without writing any packages.
    @Test
    public void testPackagePreviewExportPaths() throws Exception {
        createPackageProject();
        // Preview snapshots mount Bob's builtins instead of copying them.
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/.*")),
                livePackage("copy", "folder", addPath("Data", "/extra/.*")),
                livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        Path projectRoot = Path.of(contentRoot, "original-project");
        Path outputDirectory = projectRoot.resolve("packages");
        PackageProviderFixture.writes = 0;
        PackagePreview.Result result = PackagePreview.build(Path.of(contentRoot), projectRoot, "/content.lumanifest",
                Map.of("variant", "debug"), getClass().getClassLoader(), Progress.discarding());
        assertEquals(Map.of("episode", List.of(outputDirectory.resolve("episode.zip").toString().replace('\\', '/')),
                "copy", List.of(outputDirectory.resolve("copy").toString().replace('\\', '/')),
                "custom", List.of(outputDirectory.resolve("custom.custom").toString().replace('\\', '/'),
                                  outputDirectory.resolve("metadata/custom.json").toString().replace('\\', '/'))), result.exportPaths());
        var context = PackageProviderFixture.lastDescriptionContext;
        assertEquals("debug", context.variant());
        assertEquals(result.plan(), context.plan());
        assertTrue(context.resources().keySet().containsAll(result.plan().emittedResources()));
        assertEquals(11, context.resources().get("/extra/data.json").size());
        assertFalse(context.resources().get("/extra/data.json").hash().isEmpty());
        assertEquals(0, PackageProviderFixture.writes);
        assertFalse(new File(contentRoot, "packages").exists());
        assertFalse(Files.exists(outputDirectory));
        assertNoPackageStaging();
    }

    // Environment-only changes relocate all providers on an incremental build and retire the previous artifacts.
    @Test
    public void testPackageOutputDirectoryEnvironmentChange() throws Exception {
        createPackageProject();
        setPackageOutputDirectory("${LU_TEST_OUTPUT}/nested");
        Path root = Path.of(contentRoot);
        Path previous = null;
        for (String directory : List.of("first", root.resolve("second").toString())) {
            try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build") {
                @Override
                public String getSystemEnv(String name) {
                    return name.equals("LU_TEST_OUTPUT") ? directory : super.getSystemEnv(name);
                }
            }) {
                ClassLoaderScanner scanner = new ClassLoaderScanner();
                project.scan(scanner, "com.dynamo.bob");
                project.scan(scanner, "com.dynamo.bob.pipeline");
                project.setOption("archive", "true");
                project.setOption("liveupdate", "true");
                for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
                Path output = root.resolve(directory).resolve("nested");
                assertTrue(Files.isRegularFile(output.resolve("episode.zip")));
                assertTrue(Files.isRegularFile(output.resolve("copy/liveupdate.game.dmanifest")));
                assertEquals(Set.of(output.resolve("episode.zip"), output.resolve("copy")),
                        PackageWriter.validatedOutputs(project).stream().map(PackageWriter.Output::path).collect(Collectors.toSet()));
                if (previous != null) {
                    assertFalse(Files.exists(previous.resolve("episode.zip")));
                    assertFalse(Files.exists(previous.resolve("copy")));
                }
                previous = output;
            }
        }
        assertFalse(Files.exists(root.resolve("packages")));
    }

    // Undefined output variables fail export before writing, but do not prevent a build with export disabled.
    @Test
    public void testPackageOutputDirectoryMissingEnvironment() throws Exception {
        createPackageProject();
        setPackageOutputDirectory("${DEFOLD_TEST_UNDEFINED_PACKAGE_OUTPUT}");
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Undefined output variable should fail");
        } catch (CompileExceptionError expected) {
            assertTrue(expected.getMessage().contains("Undefined environment variable 'DEFOLD_TEST_UNDEFINED_PACKAGE_OUTPUT'"));
        }
        var plan = buildPackageProject(false, false);
        assertTrue(plan.base().contains("/extra/data.json"));
        assertFalse(new File(contentRoot, "packages").exists());
        assertNoPackageStaging();
    }

    // Inventory, rule, and dependency edits all invalidate packages without a clean build.
    @Test
    public void testPackageIncrementalRebuild() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        createFile(contentRoot, "extra/new.json", "{\"new\":true}");
        buildPackageProject(true, false);
        assertTrue(hasResource(zipManifest("episode"), "/extra/new.json"));
        createFile(contentRoot, "logic/new.script", "function init(self) self.new = true end");
        createFile(contentRoot, "logic/level.go", "components { id: \"script\" component: \"/logic/new.script\" }\n");
        buildPackageProject(true, false);
        assertTrue(hasResource(zipManifest("episode"), "/logic/new.scriptc"));
        assertFalse(hasResource(zipManifest("episode"), "/logic/level.scriptc"));
        savePackages(livePackage("episode", "zip", addPath("Data only", "/extra/.*[.]json")));
        buildPackageProject(true, false);
        assertEquals(Set.of("/extra/data.json", "/extra/new.json"), manifestPaths(zipManifest("episode")));
        assertTrue(hasResource(readManifestData(getBundledManifestFile()), "/logic/level.collectionc"));
    }

    // Editing one package, its rules, or the base must preserve unrelated package artifacts without relying on sleeps.
    @Test
    public void testPackageIncrementalArtifactReuse() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/second.json", "second");
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/(data|new)[.]json")),
                livePackage("second", "folder", addPath("Second", "/extra/second[.]json")));
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        Path folderManifest = Path.of(contentRoot, "packages/second/liveupdate.game.dmanifest");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(zip, unchanged);
        Files.setLastModifiedTime(folderManifest, unchanged);
        buildPackageProject(true, false);
        assertEquals(unchanged, Files.getLastModifiedTime(zip));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));

        createFile(contentRoot, "extra/data.json", "changed");
        buildPackageProject(true, false);
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));
        Files.setLastModifiedTime(zip, unchanged);

        createFile(contentRoot, "logic/level.script", "function init(self) self.changed = true end");
        buildPackageProject(true, false);
        assertEquals(unchanged, Files.getLastModifiedTime(zip));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));

        createFile(contentRoot, "extra/new.json", "new");
        buildPackageProject(true, false);
        assertEquals(Set.of("/extra/data.json", "/extra/new.json"), manifestPaths(zipManifest("episode")));
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));
        savePackages(livePackage("episode", "zip", addPath("Only new", "/extra/new[.]json")),
                livePackage("second", "folder", addPath("Second", "/extra/second[.]json")));
        buildPackageProject(true, false);
        assertEquals(Set.of("/extra/new.json"), manifestPaths(zipManifest("episode")));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));
        assertNoPackageStaging();
    }

    // Output hashes invalidate Bob's build state when a ZIP or an individual folder payload is damaged or deleted.
    @Test
    public void testPackageIncrementalArtifactRepair() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        Path folder = Path.of(contentRoot, "packages/copy");
        Path payload;
        try (var files = Files.list(folder)) {
            payload = files.filter(path -> !path.getFileName().toString().equals("liveupdate.game.dmanifest")).findFirst().orElseThrow();
        }
        byte[] originalPayload = Files.readAllBytes(payload);
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(zip, unchanged);
        Files.delete(payload);
        buildPackageProject(true, false);
        assertArrayEquals(originalPayload, Files.readAllBytes(payload));
        assertEquals(unchanged, Files.getLastModifiedTime(zip));

        Files.writeString(zip, "corrupt");
        Files.setLastModifiedTime(zip, unchanged);
        buildPackageProject(true, false);
        assertTrue(hasResource(zipManifest("episode"), "/extra/data.json"));
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        Files.setLastModifiedTime(zip, unchanged);
        buildPackageProject(true, false);
        assertEquals(unchanged, Files.getLastModifiedTime(zip));

        Path receipt = Path.of(contentRoot, "build/game.lupackages");
        StringBuilder legacy = new StringBuilder();
        for (String line : Files.readAllLines(receipt)) {
            if (!line.startsWith("project-root ")) legacy.append(String.join(" ", Arrays.copyOf(line.split(" ", -1), 4))).append('\n');
        }
        Files.writeString(receipt, legacy);
        buildPackageProject(true, false);
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            var outputs = PackageWriter.validatedOutputs(project);
            assertEquals(2, outputs.size());
            for (var output : outputs) {
                assertFalse(output.inputSignature().isEmpty());
                assertFalse(output.legacy());
            }
        }
        assertNoPackageStaging();
    }

    // Custom providers opt in to reuse; the full context and the provider's cache key both invalidate it.
    @Test
    public void testCustomPackageIncrementalContext() throws Exception {
        createPackageProject();
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        PackageProviderFixture.writes = 0;
        PackageProviderFixture.cacheKey = "fixture-v1";
        try {
            buildPackageProject(true, false);
            assertEquals(1, PackageProviderFixture.writes);
            Path settings = Path.of(contentRoot, "game.project");
            Files.writeString(settings, Files.readString(settings) + "\n\n");
            buildPackageProject(true, false);
            assertEquals(1, PackageProviderFixture.writes);

            Files.writeString(settings, Files.readString(settings) + "\n[display]\nwidth=641\n");
            buildPackageProject(true, false);
            assertEquals(2, PackageProviderFixture.writes);
            PackageProviderFixture.cacheKey = "fixture-v2";
            buildPackageProject(true, false);
            assertEquals(3, PackageProviderFixture.writes);

            PackageProviderFixture.cacheKey = null;
            buildPackageProject(true, false);
            assertEquals(4, PackageProviderFixture.writes);
            Files.writeString(settings, Files.readString(settings) + "\n\n");
            buildPackageProject(true, false);
            assertEquals(5, PackageProviderFixture.writes);
        } finally {
            PackageProviderFixture.cacheKey = "fixture-v1";
        }
        assertNoPackageStaging();
    }

    // ZIP compression settings invalidate ZIP output while an equivalent folder can be reused.
    @Test
    public void testPackageIncrementalCompressionSettings() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        Path folderManifest = Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(zip, unchanged);
        Files.setLastModifiedTime(folderManifest, unchanged);
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"), "zip-compression-level=0\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));
    }

    // Manifest ZIP settings override legacy defaults and invalidate ZIPs without rewriting unchanged folders.
    @Test
    public void testManifestZipProviderProperties() throws Exception {
        createPackageProject();
        Path legacy = Path.of(contentRoot, "liveupdate.settings");
        Files.writeString(legacy, "zip-compression-level=0\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        Path folderManifest = Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest");
        long uncompressedSize = Files.size(zip);
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(folderManifest, unchanged);
        setProviderProperties("zip", Map.of("compression-level", "9"));
        buildPackageProject(true, false);
        assertTrue(Files.size(zip) < uncompressedSize);
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));
        Files.setLastModifiedTime(zip, unchanged);
        Files.writeString(legacy, Files.readString(legacy).replace("zip-compression-level=0", "zip-compression-level=3"));
        buildPackageProject(true, false);
        assertEquals(unchanged, Files.getLastModifiedTime(zip));
        assertEquals(unchanged, Files.getLastModifiedTime(folderManifest));
    }

    // Custom settings reach discovery, previews and writers as immutable typed values, and invalidate artifact reuse.
    @Test
    public void testCustomProviderProperties() throws Exception {
        createPackageProject();
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        PackageProviderFixture.writes = 0;
        buildPackageProject(true, false);
        assertEquals(Map.of("extension", "custom", "quality", 2L, "scale", 1.0),
                PackageProviderFixture.lastContext.providerSettings().get("test-package"));
        setProviderProperties("test-package", Map.of("extension", "alternate", "quality", "7", "scale", "0.5"));
        Path root = Path.of(contentRoot);
        var plan = buildPackageProject(true, false);
        assertEquals(2, PackageProviderFixture.writes);
        assertFalse(Files.exists(root.resolve("packages/custom.custom")));
        assertTrue(Files.isRegularFile(root.resolve("packages/custom.alternate")));
        var values = PackageProviderFixture.lastContext.providerSettings();
        assertEquals(Map.of("extension", "alternate", "quality", 7L, "scale", 0.5), values.get("test-package"));
        try {
            values.clear();
            org.junit.Assert.fail("Provider settings should be immutable");
        } catch (UnsupportedOperationException expected) { }
        try {
            values.get("test-package").clear();
            org.junit.Assert.fail("Provider values should be immutable");
        } catch (UnsupportedOperationException expected) { }
        buildPackageProject(true, false);
        assertEquals(2, PackageProviderFixture.writes);
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        var preview = PackagePreview.build(root, root, "/content.lumanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        assertEquals(2, PackageProviderFixture.writes);
        assertEquals(List.of(root.resolve("packages/custom.alternate").toString().replace('\\', '/'),
                            root.resolve("packages/metadata/custom.json").toString().replace('\\', '/')), preview.exportPaths().get("custom"));
        assertEquals(plan.packages(), preview.plan().packages());
        assertEquals(values, PackageProviderFixture.lastDescriptionContext.providerSettings());
    }

    // Invalid or unknown provider properties fail before writing any artifacts and retain existing outputs.
    @Test
    public void testInvalidProviderProperties() throws Exception {
        createPackageProject();
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")),
                livePackage("episode", "zip", addPath("Data", "/extra/.*")));
        PackageProviderFixture.writes = 0;
        buildPackageProject(true, false);
        byte[] original = Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip"));
        for (var values : List.of(Map.of("compression-level", "10"), Map.of("compression-level", "abc"), Map.of("unknown", "3"))) {
            setProviderProperties("zip", values);
            try {
                buildPackageProject(true, false);
                org.junit.Assert.fail("Invalid properties should fail");
            } catch (AssertionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("provider 'zip'"));
                assertTrue(expected.getMessage(), expected.getMessage().contains(values.keySet().iterator().next()));
            }
            assertEquals(1, PackageProviderFixture.writes);
            assertArrayEquals(original, Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip")));
            assertNoPackageStaging();
        }
    }

    // Bundle placement preserves unchanged files, repairs damaged copies, and removes obsolete folder entries.
    @Test
    public void testPackageIncrementalBundleCopy() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        Path bundle = Path.of(contentRoot, "bundle");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("package-manifest", "/content.lumanifest");
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            Path zip = bundle.resolve("episode.zip");
            Path manifest = bundle.resolve("copy/liveupdate.game.dmanifest");
            Files.setLastModifiedTime(zip, unchanged);
            Files.setLastModifiedTime(manifest, unchanged);
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            assertEquals(unchanged, Files.getLastModifiedTime(zip));
            assertEquals(unchanged, Files.getLastModifiedTime(manifest));

            Path extra = bundle.resolve("copy/obsolete");
            Files.writeString(extra, "obsolete");
            Files.writeString(manifest, "damaged");
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            assertFalse(Files.exists(extra));
            assertArrayEquals(Files.readAllBytes(Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest")), Files.readAllBytes(manifest));
            assertEquals(unchanged, Files.getLastModifiedTime(zip));

            Files.delete(zip);
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            assertArrayEquals(Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip")), Files.readAllBytes(zip));
            Files.delete(Path.of(contentRoot, "packages/episode.zip"));
            try {
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                org.junit.Assert.fail("Missing source artifacts must not silently produce an incomplete bundle");
            } catch (IOException expected) {
                assertTrue(expected.getMessage().contains("run build before bundling"));
            }
            Files.delete(Path.of(PackageWriter.receipt(project).getAbsPath()));
            byte[] bundledZip = Files.readAllBytes(zip);
            try {
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                org.junit.Assert.fail("Missing receipts must not retire completed package copies");
            } catch (IOException expected) {
                assertTrue(expected.getMessage().contains("receipt is missing"));
            }
            assertArrayEquals(bundledZip, Files.readAllBytes(zip));
        }
    }

    private static String collection(String name, String prototype) {
        return "name: \"" + name + "\" instances { id: \"go\" prototype: \"" + prototype +
                "\" position { x: 0 y: 0 z: 0 } rotation { x: 0 y: 0 z: 0 w: 1 } }";
    }

    // Case-insensitive filesystems must not let a differently cased artifact overwrite authored content.
    @Test
    public void testPackageRejectsSourceAlias() throws Exception {
        createPackageProject();
        Path authored = Path.of(contentRoot, "packages/EPISODE.zip");
        Files.createDirectories(authored.getParent());
        Files.writeString(authored, "authored content");
        Assume.assumeTrue("Requires a case-insensitive filesystem", Files.exists(authored.resolveSibling("episode.zip")));
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("A source alias must reject the entire delivery");
        } catch (AssertionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("overwrite a source resource"));
        }
        assertEquals("authored content", Files.readString(authored));
        assertFalse(Files.exists(Path.of(contentRoot, "packages/copy")));
        assertNoPackageStaging();
    }

    // A copied project rebases internal ownership and cannot retire the original project's outputs.
    @Test
    public void testPackageOwnershipFollowsProjectCopy() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        String original = contentRoot;
        Path copy = Files.createTempDirectory("copied-liveupdate-project");
        try {
            try (Project project = new Project(new DefaultFileSystem(), original, "build")) {
                PackageWriter.copyOutputs(project, Path.of(original, "bundle"));
            }
            FileUtils.copyDirectory(new File(original), copy.toFile());
            contentRoot = copy.toString();
            FileUtils.deleteDirectory(copy.resolve("build").toFile());
            // Retained artifacts remain generated resources, even with a broad rule.
            savePackages(livePackage("renamed", "zip", addPath("Data", "/.*[.](json|zip)")));
            var plan = buildPackageProject(true, false);
            assertEquals(Set.of("/extra/data.json"), plan.packages().get("renamed").resources());
            assertFalse(Files.exists(copy.resolve("packages/episode.zip")));
            assertTrue(Files.exists(Path.of(original, "packages/episode.zip")));
            try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
                PackageWriter.copyOutputs(project, copy.resolve("bundle"));
            }
            assertFalse(Files.exists(copy.resolve("bundle/episode.zip")));
            assertTrue(Files.exists(copy.resolve("bundle/renamed.zip")));
            assertTrue(Files.exists(Path.of(original, "bundle/episode.zip")));
            // Missing copied artifacts must also rebuild without touching the origin.
            FileUtils.deleteDirectory(copy.resolve("packages").toFile());
            buildPackageProject(true, false);
            assertTrue(Files.exists(copy.resolve("packages/renamed.zip")));
            assertTrue(Files.exists(Path.of(original, "packages/episode.zip")));
        } finally {
            contentRoot = original;
            FileUtils.deleteDirectory(copy.toFile());
        }
    }

    // Copying a receipt never transfers ownership of outputs outside the original project.
    @Test
    public void testCopiedProjectDoesNotOwnExternalArtifacts() throws Exception {
        createPackageProject();
        String original = contentRoot;
        Path external = Files.createTempDirectory("external-liveupdate-output");
        Path copy = Files.createTempDirectory("copied-liveupdate-project");
        try {
            setPackageOutputDirectory(external.toString());
            buildPackageProject(true, false);
            byte[] previous = Files.readAllBytes(external.resolve("episode.zip"));
            FileUtils.copyDirectory(new File(original), copy.toFile());
            contentRoot = copy.toString();
            FileUtils.deleteDirectory(copy.resolve("build").toFile());
            savePackages(livePackage("copy", "folder", addPath("Data", "/extra/.*")));
            buildPackageProject(true, false);
            assertArrayEquals(previous, Files.readAllBytes(external.resolve("episode.zip")));
        } finally {
            contentRoot = original;
            FileUtils.deleteDirectory(copy.toFile());
            FileUtils.deleteDirectory(external.toFile());
        }
    }

    // Cancelling during finalization rolls back both new and replaced artifacts, leaving delivery retryable.
    @Test
    public void testPackageBundleCancellationRollsBack() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        Path bundle = Path.of(contentRoot, "bundle");
        Path zip = bundle.resolve("episode.zip");
        for (boolean replacing : List.of(false, true)) {
            if (replacing) {
                Files.delete(zip);
                createFile(contentRoot, "extra/data.json", "{\"changed\":true}");
                buildPackageProject(true, false);
            }
            byte[] previousManifest = replacing ? Files.readAllBytes(bundle.resolve("copy/liveupdate.game.dmanifest")) : null;
            try (Project cancelled = new Project(new DefaultFileSystem(), contentRoot, "build") {
                @Override
                public boolean isBuildCanceled() { return Files.exists(zip); }
            }) {
                try {
                    PackageWriter.copyOutputs(cancelled, bundle);
                    org.junit.Assert.fail("Delivery must be cancelled after installing its first artifact");
                } catch (java.io.InterruptedIOException expected) {
                    assertFalse(Files.exists(zip));
                    if (replacing) assertArrayEquals(previousManifest, Files.readAllBytes(bundle.resolve("copy/liveupdate.game.dmanifest")));
                    else assertFalse(Files.exists(bundle.resolve("copy")));
                }
            }
            try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
                PackageWriter.copyOutputs(project, bundle);
            }
            assertArrayEquals(Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip")), Files.readAllBytes(zip));
            assertNoPackageStaging();
        }
    }

    // Reusing a preview snapshot must still resolve a plan while retaining unchanged compiler outputs.
    @Test
    public void testPackagePreviewReusesCompiledResources() throws Exception {
        createPackageProject();
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        var first = PackagePreview.build(Path.of(contentRoot), Path.of(contentRoot), "/content.lumanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        Path compiled = Path.of(contentRoot, "build/logic/level.collectionc");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(compiled, unchanged);
        var second = PackagePreview.build(Path.of(contentRoot), Path.of(contentRoot), "/content.lumanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        assertEquals(first.plan().packages(), second.plan().packages());
        assertEquals(unchanged, Files.getLastModifiedTime(compiled));
        createFile(contentRoot, "extra/data.json", "{\"changed\":true}");
        var third = PackagePreview.build(Path.of(contentRoot), Path.of(contentRoot), "/content.lumanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        assertEquals(Long.valueOf(16), third.compiledSizes().get("/extra/data.json"));
        assertEquals(unchanged, Files.getLastModifiedTime(compiled));
        assertFalse(Files.exists(Path.of(contentRoot, "packages/episode.zip")));
    }

    // A queued build must not enter compilation while another build owns Bob's shared registries, and can be cancelled.
    @Test
    public void testQueuedBuildCancellation() throws Exception {
        createPackageProject();
        CountDownLatch active = new CountDownLatch(1);
        CountDownLatch release = new CountDownLatch(1);
        CountDownLatch waiting = new CountDownLatch(1);
        AtomicBoolean entered = new AtomicBoolean();
        AtomicBoolean canceled = new AtomicBoolean();
        AtomicInteger checks = new AtomicInteger();
        try (var executor = Executors.newFixedThreadPool(2);
             Project first = new Project(new DefaultFileSystem(), contentRoot, "build") {
                 @Override
                 public void loadProjectFile(boolean scan) throws IOException {
                     active.countDown();
                     try {
                         if (!release.await(30, TimeUnit.SECONDS)) throw new IOException("Build was not released");
                     } catch (InterruptedException e) {
                         Thread.currentThread().interrupt();
                         throw new IOException(e);
                     }
                     super.loadProjectFile(scan);
                 }
             };
             Project second = new Project(new DefaultFileSystem(), contentRoot, "other-build") {
                 @Override
                 public void loadProjectFile(boolean scan) throws IOException {
                     entered.set(true);
                     waiting.countDown();
                     super.loadProjectFile(scan);
                 }
             }) {
            var firstBuild = executor.submit(() -> first.build(Progress.discarding(), "build"));
            try {
                assertTrue(active.await(30, TimeUnit.SECONDS));
                Progress progress = new Progress(new Progress.Reporter() {
                    public void report(com.dynamo.bob.IProgress.Message message, double fraction) {}
                    public boolean isCanceled() {
                        if (checks.incrementAndGet() >= 2) waiting.countDown();
                        return canceled.get();
                    }
                });
                var queuedBuild = executor.submit(() -> second.build(progress, "build"));
                assertTrue(waiting.await(30, TimeUnit.SECONDS));
                assertFalse("Queued build entered shared compiler state", entered.get());
                canceled.set(true);
                try {
                    queuedBuild.get(30, TimeUnit.SECONDS);
                    org.junit.Assert.fail("Queued build must be cancelled before the active build finishes");
                } catch (java.util.concurrent.ExecutionException expected) {
                    assertTrue(expected.getCause().getMessage().toLowerCase().contains("cancel"));
                }
            } finally {
                canceled.set(true);
                release.countDown();
                for (TaskResult result : firstBuild.get(30, TimeUnit.SECONDS)) assertTrue(result.toString(), result.isOk());
            }
        }
    }

    // Concurrent previews for different targets must retain each target's Lua cache signature on subsequent builds.
    @Test
    public void testConcurrentPackagePreviewTargets() throws Exception {
        createPackageProject();
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        Path web = Path.of(contentRoot);
        Path nativeProject = Files.createTempDirectory("liveupdate-native-preview-");
        CountDownLatch ready = new CountDownLatch(2);
        CountDownLatch start = new CountDownLatch(1);
        try (var executor = Executors.newFixedThreadPool(2)) {
            FileUtils.copyDirectory(web.toFile(), nativeProject.toFile());
            var webBuild = executor.submit(() -> {
                ready.countDown();
                assertTrue(start.await(30, TimeUnit.SECONDS));
                return PackagePreview.build(web, web, "/content.lumanifest", Map.of("platform", "wasm-web"), getClass().getClassLoader(), Progress.discarding());
            });
            var nativeBuild = executor.submit(() -> {
                ready.countDown();
                assertTrue(start.await(30, TimeUnit.SECONDS));
                return PackagePreview.build(nativeProject, nativeProject, "/content.lumanifest", Map.of("platform", "arm64-macos"), getClass().getClassLoader(), Progress.discarding());
            });
            assertTrue(ready.await(30, TimeUnit.SECONDS));
            start.countDown();
            webBuild.get(60, TimeUnit.SECONDS);
            nativeBuild.get(60, TimeUnit.SECONDS);
            Path script = Path.of("build/logic/level.scriptc");
            byte[] webScript = Files.readAllBytes(web.resolve(script));
            byte[] nativeScript = Files.readAllBytes(nativeProject.resolve(script));
            assertFalse(Arrays.equals(webScript, nativeScript));
            PackagePreview.build(web, web, "/content.lumanifest", Map.of("platform", "arm64-macos"), getClass().getClassLoader(), Progress.discarding());
            assertArrayEquals(nativeScript, Files.readAllBytes(web.resolve(script)));
            PackagePreview.build(nativeProject, nativeProject, "/content.lumanifest", Map.of("platform", "wasm-web"), getClass().getClassLoader(), Progress.discarding());
            assertArrayEquals(webScript, Files.readAllBytes(nativeProject.resolve(script)));
        } finally {
            start.countDown();
            FileUtils.deleteDirectory(nativeProject.toFile());
        }
    }

    // Adding a pipeline-provider JAR after a first preview refreshes discovery without restarting the JVM.
    @Test
    public void testPackagePreviewDiscoversNewProvider() throws Exception {
        createPackageProject();
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        Path root = Path.of(contentRoot);
        PackagePreview.build(root, root, "/content.lumanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        Path classes = Files.createTempDirectory("liveupdate-provider-classes-");
        try {
            Path source = classes.resolve("AddedPackageProvider.java");
            Files.writeString(source, """
                    package com.defold.extension.pipeline;
                    import com.dynamo.bob.archive.liveupdate.*;
                    import java.util.List;
                    import java.nio.file.*;
                    public final class AddedPackageProvider implements PackageProvider {
                        public String getId() { return "added-provider"; }
                        public List<Artifact> describeOutputs(Context context, PackageResolver.Package pkg) {
                            return List.of(new Artifact(pkg.description().getName() + ".added", false));
                        }
                        public void write(Context context, PackageResolver.Package pkg, List<PreparedResource> resources,
                                          byte[] manifest, Path directory) throws java.io.IOException {
                            Files.write(directory.resolve(pkg.description().getName() + ".added"), manifest);
                        }
                    }
                    """);
            assertEquals(0, ToolProvider.getSystemJavaCompiler().run(null, null, null,
                    "-classpath", System.getProperty("java.class.path"), "-d", classes.toString(), source.toString()));
            Path extension = root.resolve("added-extension");
            Files.createDirectories(extension.resolve("plugins/share"));
            Files.writeString(extension.resolve("ext.manifest"), "name: AddedExtension\n");
            String classFile = "com/defold/extension/pipeline/AddedPackageProvider.class";
            try (var jar = new JarOutputStream(Files.newOutputStream(extension.resolve("plugins/share/provider.jar")))) {
                for (String directory : List.of("com/", "com/defold/", "com/defold/extension/", "com/defold/extension/pipeline/")) {
                    jar.putNextEntry(new JarEntry(directory));
                    jar.closeEntry();
                }
                jar.putNextEntry(new JarEntry(classFile));
                Files.copy(classes.resolve(classFile), jar);
                jar.closeEntry();
            }
            savePackages(livePackage("episode", "added-provider", addPath("Data", "/extra/.*")));
            var result = PackagePreview.build(root, root, "/content.lumanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
            assertEquals(List.of(root.resolve("packages/episode.added").toString().replace('\\', '/')), result.exportPaths().get("episode"));
            assertFalse(Files.exists(root.resolve("packages/episode.added")));
        } finally {
            FileUtils.deleteDirectory(classes.toFile());
        }
    }

    // Rebuilding a web application retains unchanged package copies and removes stale application files transactionally.
    @Test
    public void testHtml5PackageCopiesAreIncremental() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.loadProjectFile();
            var properties = project.getProjectProperties();
            properties.putStringValue("project", "title", "Packages");
            properties.putStringValue("html5", "htmlfile", "/template.html");
            properties.putStringValue("html5", "cssfile", "/style.css");
            createFile(contentRoot, "template.html", "<title>{{project.title}}</title>");
            createFile(contentRoot, "style.css", "");
            project.setOption("platform", Platform.WasmWeb.getPair());
            project.setOption("architectures", "wasm-web");
            Path engines = Path.of(project.getBinaryOutputDirectory(), Platform.WasmWeb.getExtenderPair());
            Files.createDirectories(engines);
            for (String name : Platform.WasmWeb.formatBinaryName("dmengine")) Files.write(engines.resolve(name), new byte[]{1});
            File bundle = new File(contentRoot, "bundle");
            Path app = bundle.toPath().resolve("Packages");
            HTML5Bundler bundler = new HTML5Bundler();
            bundler.bundleApplication(project, Platform.WasmWeb, bundle, () -> false);
            FileTime unchanged = FileTime.fromMillis(946684800000L);
            Path zip = app.resolve("episode.zip");
            Path manifest = app.resolve("copy/liveupdate.game.dmanifest");
            Files.setLastModifiedTime(zip, unchanged);
            Files.setLastModifiedTime(manifest, unchanged);
            Files.writeString(app.resolve("obsolete.txt"), "stale application file");
            bundler.bundleApplication(project, Platform.WasmWeb, bundle, () -> false);
            assertEquals(unchanged, Files.getLastModifiedTime(zip));
            assertEquals(unchanged, Files.getLastModifiedTime(manifest));
            assertFalse(Files.exists(app.resolve("obsolete.txt")));
            byte[] previousHtml = Files.readAllBytes(app.resolve("index.html"));
            byte[] previousZip = Files.readAllBytes(zip);
            savePackages(livePackage("archive", "folder", addPath("Data", "/extra/.*")));
            buildPackageProject(true, false);
            try {
                bundler.bundleApplication(project, Platform.WasmWeb, bundle, () -> false);
                org.junit.Assert.fail("Package must not replace the application's archive directory");
            } catch (IOException expected) {
                assertTrue(expected.getMessage().contains("collides with an application output"));
            }
            assertArrayEquals(previousHtml, Files.readAllBytes(app.resolve("index.html")));
            assertArrayEquals(previousZip, Files.readAllBytes(zip));
            assertNoPackageStaging();
        }
    }

    // Ownership survives both clean commands and disabled export, keeping generated artifacts out of source matching.
    @Test
    public void testPackageOwnershipSurvivesCleanAndExportToggle() throws Exception {
        createPackageProject();
        savePackages(livePackage("episode", "zip", addPath("Data", "/.*[.](json|zip)")));
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        byte[] expected = Files.readAllBytes(zip);
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(zip, unchanged);
        for (String command : List.of("clean", "distclean")) {
            var plan = buildPackageProject(true, false, command, "build");
            assertEquals(Set.of("/extra/data.json"), plan.packages().get("episode").resources());
            assertFalse(plan.sourceOutputs().containsKey("/packages/episode.zip"));
            assertArrayEquals(expected, Files.readAllBytes(zip));
            assertEquals(unchanged, Files.getLastModifiedTime(zip));
        }
        buildPackageProject(false, false);
        assertTrue(PackageArtifacts.generatedPaths(Path.of(contentRoot)).contains("/packages/episode.zip"));
        buildPackageProject(true, false);
        assertArrayEquals(expected, Files.readAllBytes(zip));
        assertEquals(unchanged, Files.getLastModifiedTime(zip));
        assertNoPackageStaging();
    }

    // Successful renames/removals retire owned artifacts; failed providers leave the previous delivery intact.
    @Test
    public void testPackageRetiresObsoleteArtifacts() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        savePackages(livePackage("old", "zip", addPath("Data", "/.*[.](json|zip)")),
                livePackage("copy", "folder", addPath("Data", "/extra/.*")));
        buildPackageProject(true, false);
        Path bundle = Path.of(contentRoot, "bundle");
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("package-manifest", "/content.lumanifest");
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            byte[] previous = Files.readAllBytes(Path.of(contentRoot, "packages/old.zip"));
            savePackages(livePackage("new", "test-package", addPath("Data", "/.*[.](json|zip)")));
            PackageProviderFixture.fail = true;
            try {
                buildPackageProject(true, false);
                org.junit.Assert.fail("Provider should fail");
            } catch (AssertionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Deliberate provider failure"));
            } finally {
                PackageProviderFixture.fail = false;
            }
            assertArrayEquals(previous, Files.readAllBytes(Path.of(contentRoot, "packages/old.zip")));
            assertTrue(Files.exists(Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest")));
            savePackages(livePackage("new", "zip", addPath("Data", "/.*[.](json|zip)")));
            var plan = buildPackageProject(true, false);
            assertEquals(Set.of("/extra/data.json"), plan.packages().get("new").resources());
            assertFalse(Files.exists(Path.of(contentRoot, "packages/old.zip")));
            assertFalse(Files.exists(Path.of(contentRoot, "packages/copy")));
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            assertFalse(Files.exists(bundle.resolve("old.zip")));
            assertFalse(Files.exists(bundle.resolve("copy")));
            assertTrue(Files.exists(bundle.resolve("new.zip")));
            buildPackageProject(true, false, "clean", "build");
            assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest("new")));
        }
        assertNoPackageStaging();
    }

    // Case-only renames replace owned ZIP/folder outputs and their bundle copies on every filesystem.
    @Test
    public void testPackageCaseOnlyRename() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        Path bundle = Path.of(contentRoot, "bundle");
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("package-manifest", "/content.lumanifest");
            for (String name : List.of("episode", "Episode", "episode")) {
                savePackages(livePackage(name, "zip", addPath("Data", "/extra/.*")),
                        livePackage(name + "-folder", "folder", addPath("Data", "/extra/.*")));
                buildPackageProject(true, false);
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest(name)));
                for (Path directory : List.of(Path.of(contentRoot, "packages"), bundle)) {
                    try (var files = Files.list(directory)) {
                        assertEquals(Set.of(name + ".zip", name + "-folder"),
                                files.map(path -> path.getFileName().toString()).collect(Collectors.toSet()));
                    }
                }
                FileTime unchanged = FileTime.fromMillis(946684800000L);
                Path zip = bundle.resolve(name + ".zip");
                Files.setLastModifiedTime(zip, unchanged);
                buildPackageProject(true, false);
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                assertEquals(unchanged, Files.getLastModifiedTime(zip));
            }
        }
        assertNoPackageStaging();
    }

    // A delivery must reject application collisions before copying anything, even if that path used to hold a package.
    @Test
    public void testPackageBundleProtectsApplicationOutputs() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        Path bundle = Path.of(contentRoot, "bundle");
        Path app = bundle.resolve("copy");
        Files.createDirectories(app);
        Files.writeString(app.resolve("game.exe"), "executable");
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("package-manifest", "/content.lumanifest");
            try {
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                org.junit.Assert.fail("Existing application directory must not become a package");
            } catch (IOException expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("overwrite an existing bundle output"));
            }
            assertEquals("executable", Files.readString(app.resolve("game.exe")));
            assertFalse(Files.exists(bundle.resolve("episode.zip")));
            FileUtils.deleteDirectory(app.toFile());
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            // A later application bundle can reuse a name previously owned by a Live Update package.
            Files.writeString(app.resolve("game.exe"), "new executable");
            Files.delete(bundle.resolve("episode.zip"));
            for (Path applicationOutput : List.of(app, app.resolve("game.exe"), bundle)) {
                try {
                    BundleHelper.moveBundleIfNeed(project, bundle.toFile(), applicationOutput.toFile());
                    org.junit.Assert.fail("Application outputs must take precedence over previous package ownership");
                } catch (IOException expected) {
                    assertTrue(expected.getMessage(), expected.getMessage().contains("collides with an application output"));
                }
            }
            assertEquals("new executable", Files.readString(app.resolve("game.exe")));
            assertFalse(Files.exists(bundle.resolve("episode.zip")));
        }
    }

    private Project linuxPackageBundleProject() throws Exception {
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.loadProjectFile();
        project.getProjectProperties().putStringValue("project", "title", "Application");
        project.setOption("platform", Platform.X86_64Linux.getPair());
        project.setOption("architectures", "x86_64-linux");
        project.setOption("variant", "release");
        Path engines = Path.of(project.getBinaryOutputDirectory(), Platform.X86_64Linux.getExtenderPair());
        Files.createDirectories(engines);
        for (String name : Platform.X86_64Linux.formatBinaryName("dmengine")) Files.write(engines.resolve(name), new byte[]{1});
        return project;
    }

    // Native bundling must detect collisions before deleting exported packages or copies from a previous bundle.
    @Test
    public void testPackageNativeBundlePreservesCollidingOutputs() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        Path bundle = Path.of(contentRoot, "bundle");
        try (Project project = linuxPackageBundleProject()) {
            BundleHelper.moveBundleIfNeed(project, bundle.toFile());
            project.getProjectProperties().putStringValue("project", "title", "copy");
            // Disabling export clears the active receipt but retains ownership of existing artifacts.
            for (boolean export : List.of(true, false)) {
                if (!export) buildPackageProject(false, false);
                for (Path destination : List.of(bundle, Path.of(contentRoot, "packages"))) {
                    Path manifest = destination.resolve("copy/liveupdate.game.dmanifest");
                    byte[] previous = Files.readAllBytes(manifest);
                    try {
                        new LinuxBundler().bundleApplication(project, Platform.X86_64Linux, destination.toFile(), () -> false);
                        org.junit.Assert.fail("Application must not replace an existing Live Update package");
                    } catch (IOException expected) {
                        assertTrue(expected.getMessage(), expected.getMessage().contains("collides with an application output"));
                    }
                    assertArrayEquals(previous, Files.readAllBytes(manifest));
                    assertFalse(Files.exists(destination.resolve("copy/copy.x86_64")));
                }
            }
        }
        assertNoPackageStaging();
    }

    // Engine-only native bundles must succeed without a receipt and ignore stale package outputs from a prior build.
    @Test
    public void testPackageNativeBundleWithoutArchive() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        Path bundle = Path.of(contentRoot, "bundle");
        try (Project project = linuxPackageBundleProject()) {
            project.setOption("exclude-archive", "true");
            project.setOption("liveupdate", "true");
            for (boolean previousBuild : List.of(false, true)) {
                if (previousBuild) {
                    buildPackageProject(true, false);
                    Files.delete(Path.of(contentRoot, "packages/episode.zip"));
                } else {
                    assertFalse(PackageWriter.receipt(project).exists());
                }
                new LinuxBundler().bundleApplication(project, Platform.X86_64Linux, bundle.toFile(), () -> false);
                assertArrayEquals(new byte[]{1}, Files.readAllBytes(bundle.resolve("Application/Application.x86_64")));
                assertFalse(Files.exists(bundle.resolve("Application/game.arci")));
                assertFalse(Files.exists(bundle.resolve("episode.zip")));
                assertFalse(Files.exists(bundle.resolve("copy")));
            }
        }
        assertNoPackageStaging();
    }

    // Custom-resource settings seed the base without locking opaque files that a manifest explicitly packages.
    @Test
    public void testPackageCanMoveCustomResourcesOutOfBase() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/kept.txt", "base data");
        Files.writeString(Path.of(contentRoot, "game.project"), "\n[project]\ncustom_resources = /extra\n",
                java.nio.file.StandardOpenOption.APPEND);
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/data[.]json")));
        var plan = buildPackageProject(true, false);
        assertFalse(plan.locks().containsKey("/extra/data.json"));
        assertFalse(plan.base().contains("/extra/data.json"));
        assertTrue(plan.base().contains("/extra/kept.txt"));
        assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest("episode")));
        Set<String> base = manifestPaths(readManifestData(getBundledManifestFile()));
        assertFalse(base.contains("/extra/data.json"));
        assertTrue(base.contains("/extra/kept.txt"));
    }

    // Factory loading flags preserve startup prototypes and collection descriptors while shared eager paths remain locked.
    @Test
    public void testPackageFactoryStartupLocks() throws Exception {
        createPackageProject();
        createFile(contentRoot, "logic/shared.script", "function init(self) self.shared = true end");
        StringBuilder main = new StringBuilder("components { id: \"proxy\" component: \"/logic/level.collectionproxy\" }\n");
        main.append("components { id: \"shared\" component: \"/logic/shared.script\" }\n");
        for (String name : List.of("static-go", "dynamic-go", "static-collection", "dynamic-collection")) {
            boolean isCollection = name.endsWith("collection");
            boolean dynamic = name.startsWith("dynamic");
            createFile(contentRoot, "parts/" + name + ".go", "components { id: \"script\" component: \"/logic/shared.script\" }");
            if (isCollection) createFile(contentRoot, "parts/" + name + ".collection", collection(name, "/parts/" + name + ".go"));
            String extension = isCollection ? ".collectionfactory" : ".factory";
            createFile(contentRoot, "logic/" + name + extension, "prototype: \"/parts/" + name + (isCollection ? ".collection" : ".go") + "\" load_dynamically: " + dynamic);
            main.append("components { id: \"").append(name).append("\" component: \"/logic/").append(name).append(extension).append("\" }\n");
        }
        createFile(contentRoot, "logic/main.go", main.toString());
        // Exclude=false is deliberately retained: every proxy is a deferred boundary in manifest mode.
        createFile(contentRoot, "logic/level.collectionproxy", "collection: \"/logic/level.collection\" exclude: false");
        savePackages(livePackage("episode", "zip", addPath("Level", "/logic/level[.]collection"), addPath("Parts", "/parts/.*[.](go|collection)")));
        PackageResolver.Plan plan = buildPackageProject(true, false);
        assertTrue(plan.base().containsAll(Set.of("/parts/static-go.goc", "/parts/static-collection.collectionc", "/parts/static-collection.goc", "/parts/dynamic-collection.collectionc", "/logic/shared.scriptc")));
        assertFalse(plan.base().contains("/parts/dynamic-go.goc"));
        assertFalse(plan.base().contains("/parts/dynamic-collection.goc"));
        assertFalse(plan.base().contains("/logic/level.collectionc"));
        assertEquals(plan.base(), manifestPaths(readManifestData(getBundledManifestFile())));
        assertTrue(plan.packages().get("episode").resources().contains("/logic/shared.scriptc"));
        // Removing the manifest restores legacy inclusion for every factory prototype and unchecked proxy.
        Path gameProject = Path.of(contentRoot, "game.project");
        Files.writeString(gameProject, Files.readString(gameProject).replace("manifest = /content.lumanifest\n", ""));
        PackageResolver.Plan legacy = buildPackageProject(true, false);
        assertTrue(legacy.packages().get("liveupdate").resources().isEmpty());
        assertEquals(legacy.resources().keySet(), legacy.base());
        assertTrue(legacy.base().containsAll(Set.of("/parts/dynamic-go.goc", "/parts/dynamic-collection.goc", "/logic/level.collectionc")));
        assertEquals(legacy.base(), manifestPaths(readManifestData(getBundledManifestFile())));
    }

    // Add/Remove reference cycles fail during task creation, before any provider can publish output.
    @Test
    public void testPackageReferenceCycleRejectedBeforeWriting() throws Exception {
        createPackageProject();
        savePackages(
                livePackage("episode", "zip", addPath("Data", "/extra/.*"),
                        LiveUpdateManifest.RuleDesc.newBuilder().setName("Include copy").setPackage("copy").build()),
                livePackage("copy", "folder",
                        LiveUpdateManifest.RuleDesc.newBuilder().setName("Subtract episode").setPackage("episode")
                                .setOperation(LiveUpdateManifest.RuleOperation.REMOVE).build()));
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Package reference cycle should fail");
        } catch (CompileExceptionError expected) {
            assertEquals("Package reference cycle in rule 'Include copy' of package 'episode': episode -> copy -> episode",
                    expected.getMessage());
        }
        assertFalse(new File(contentRoot, "packages/episode.zip").exists());
        assertFalse(new File(contentRoot, "packages/copy").exists());
    }

    // A missing provider fails before publishing any package and always removes temporary payloads and staging folders.
    @Test
    public void testPackageFailureCleansStaging() throws Exception {
        createPackageProject();
        savePackages(livePackage("first", "zip", addPath("data", "/extra/.*")), livePackage("broken", "no-such-provider", addPath("data", "/extra/.*")));
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Unknown provider should fail");
        } catch (AssertionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("Unknown Live Update provider"));
        }
        assertFalse(new File(contentRoot, "packages/first.zip").exists());
        assertNoPackageStaging();
    }

    // Custom providers receive the complete immutable plan; collisions precede writes and failures preserve previous outputs.
    @Test
    public void testCustomPackageProviderContextCollisionsAndFailure() throws Exception {
        createPackageProject();
        PackageProviderFixture.writes = 0;
        PackageProviderFixture.fail = false;
        PackageProviderFixture.collide = false;
        try {
            savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
            buildPackageProject(true, false);
            var context = PackageProviderFixture.lastContext;
            assertEquals("debug", context.variant());
            assertTrue(context.options().containsKey("with-symbols"));
            assertNull(context.options().get("with-symbols"));
            assertEquals("true", context.options().get("archive"));
            assertEquals(Set.of("/extra/data.json"), context.plan().packages().get("custom").resources());
            assertTrue(context.resources().keySet().containsAll(context.plan().emittedResources()));
            assertEquals(11, context.resources().get("/extra/data.json").size());
            assertEquals(List.of("/extra/data.json"), context.plan().sourceOutputs().get("/extra/data.json"));
            assertEquals(Set.of("/extra/data.json"), manifestPaths(PackageProviderFixture.lastManifest));
            byte[] previous = Files.readAllBytes(new File(contentRoot, "packages/custom.custom").toPath());
            createFile(contentRoot, "extra/data.json", "changed");
            PackageProviderFixture.fail = true;
            try {
                buildPackageProject(true, false);
                org.junit.Assert.fail("Provider should fail");
            } catch (AssertionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Deliberate provider failure"));
            }
            assertArrayEquals(previous, Files.readAllBytes(new File(contentRoot, "packages/custom.custom").toPath()));
            assertNoPackageStaging();
            PackageProviderFixture.fail = false;
            PackageProviderFixture.collide = true;
            savePackages(livePackage("one", "test-package", addPath("Data", "/extra/.*")),
                    livePackage("two", "test-package", addPath("Data", "/extra/.*")));
            int writes = PackageProviderFixture.writes;
            try {
                buildPackageProject(true, false);
                org.junit.Assert.fail("Outputs should collide");
            } catch (AssertionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Colliding Live Update outputs"));
            }
            assertEquals(writes, PackageProviderFixture.writes);
            assertNoPackageStaging();
        } finally {
            PackageProviderFixture.fail = false;
            PackageProviderFixture.collide = false;
        }
    }

    // Build outputs are reserved before they exist, and collisions must not invoke any package writer.
    @Test
    public void testPackageRejectsBuildOutputCollisions() throws Exception {
        createPackageProject();
        PackageProviderFixture.writes = 0;
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")),
                livePackage("game.lupackages", "folder", addPath("Data", "/extra/.*")));
        setPackageOutputDirectory("build");
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Package output should collide with the build receipt");
        } catch (AssertionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("Colliding Live Update outputs"));
        }
        assertEquals(0, PackageProviderFixture.writes);
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.lupackages")));
        assertFalse(Files.exists(Path.of(contentRoot, "build/custom.custom")));
        assertFalse(Files.exists(Path.of(contentRoot, ".internal/liveupdate")));
        assertNoPackageStaging();
    }

    // Parent/child artifacts collide in either declaration order, including native Windows paths, before providers write.
    @Test
    public void testPackageRejectsNestedProviderOutputs() throws Exception {
        createPackageProject();
        PackageProviderFixture.writes = 0;
        var folder = livePackage("metadata", "folder", addPath("Data", "/extra/.*"));
        var custom = livePackage("custom", "test-package", addPath("Data", "/extra/.*"));
        for (var packages : List.of(List.of(folder, custom), List.of(custom, folder))) {
            savePackages(packages.toArray(LiveUpdateManifest.PackageDesc[]::new));
            try {
                buildPackageProject(true, false);
                org.junit.Assert.fail("The folder and metadata/custom.json must collide");
            } catch (AssertionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Colliding Live Update outputs"));
            }
            assertEquals(0, PackageProviderFixture.writes);
            assertFalse(Files.exists(Path.of(contentRoot, "packages")));
            assertNoPackageStaging();
        }
    }

    // Runtime-derived requirements name alternative packages while the local manifest omits their resources.
    @Test
    public void testPackageDependencyMetadata() throws Exception {
        createPackageProject();
        var removeScript = LiveUpdateManifest.RuleDesc.newBuilder().setName("Separate script")
                .setOperation(LiveUpdateManifest.RuleOperation.REMOVE).setPath("/logic/level[.]script").build();
        savePackages(livePackage("episode", "zip", addPath("Level", "/logic/level[.]collection"), removeScript),
                livePackage("common", "folder", addPath("Script", "/logic/level[.]script")),
                livePackage("alternative", "zip", addPath("Script", "/logic/level[.]script")));
        buildPackageProject(true, false);
        Manifest.ManifestData episode = zipManifest("episode");
        assertEquals(Set.of("/logic/level.collectionc", "/logic/level.goc"), manifestPaths(episode));
        assertEquals(1, episode.getPackageMetadata().getRequirementsCount());
        Manifest.PackageRequirement requirement = episode.getPackageMetadata().getRequirements(0);
        assertEquals(MurmurHash.hash64("/logic/level.scriptc"), requirement.getUrlHash());
        assertEquals(List.of("common", "alternative"), requirement.getPackagesList());
        Set<Long> local = episode.getResourcesList().stream().map(Manifest.ResourceEntry::getUrlHash).collect(Collectors.toSet());
        for (Manifest.ResourceEntry entry : episode.getResourcesList()) assertTrue(local.containsAll(entry.getDependantsList()));
    }

    // Generated secondary runtime outputs are packaged, while component-counter and other build-only outputs are excluded.
    @Test
    public void testPackageGeneratedOutputsAndIgnoredFiles() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/atlas.atlas", "images { image: \"/test.png\" }\n");
        createFile(contentRoot, "extra/ignored.json", "ignored");
        createFile(contentRoot, ".defignore", "/extra/ignored.json\n");
        savePackages(livePackage("episode", "zip", addPath("Atlas", "/extra/.*[.]atlas"), addPath("Data", "/extra/.*[.]json")));
        var plan = buildPackageProject(true, false);
        assertEquals(Set.of("/extra/atlas.a.texturesetc", "/extra/atlas.texturec", "/extra/data.json"), manifestPaths(zipManifest("episode")));
        assertEquals(List.of("/extra/atlas.a.texturesetc", "/extra/atlas.texturec"), plan.sourceOutputs().get("/extra/atlas.atlas"));
        assertEquals("/extra/atlas.atlas", plan.primarySources().get("/extra/atlas.a.texturesetc"));
        assertEquals("/extra/atlas.atlas", plan.primarySources().get("/extra/atlas.texturec"));
        assertEquals("/extra/data.json", plan.primarySources().get("/extra/data.json"));
        assertFalse(plan.sourceOutputs().containsKey("/extra/ignored.json"));
    }

    // A folder rule reuses the atlas task when it also matches a same-stem input image.
    @Test
    public void testPackageFolderRuleWithAtlasInputs() throws Exception {
        createPackageProject();
        Files.copy(Path.of(contentRoot, "test.png"), Path.of(contentRoot, "extra/art.png"));
        createFile(contentRoot, "extra/art.atlas", "images { image: \"/extra/art.png\" }\n");
        savePackages(livePackage("episode", "zip", addPath("Folder", "/extra/.*")));
        var plan = buildPackageProject(true, false);
        assertEquals(Set.of("/extra/art.a.texturesetc", "/extra/art.texturec", "/extra/data.json"), manifestPaths(zipManifest("episode")));
        assertEquals(plan.sourceOutputs().get("/extra/art.atlas"), plan.sourceOutputs().get("/extra/art.png"));
        assertEquals("/extra/art.atlas", plan.primarySources().get("/extra/art.texturec"));
        // A PNG needed independently at runtime still conflicts with the atlas's texture output.
        createFile(contentRoot, "extra/texture.script", "go.property(\"texture\", resource.texture(\"/extra/art.png\"))\n");
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Two runtime textures must not silently share an output");
        } catch (CompileExceptionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("Conflicting output resource"));
        }
    }

    // Broad rules export custom files but omit compiler-only texture profiles and shader includes, also without export.
    @Test
    public void testPackageSkipsBuildOnlyOutputs() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/local.texture_profiles", "");
        createFile(contentRoot, "extra/include.glsl", "#define VALUE 1\n");
        savePackages(livePackage("episode", "zip", addPath("Folder", "/extra/.*")));
        var plan = buildPackageProject(true, false);
        assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest("episode")));
        assertEquals(List.of(), plan.sourceOutputs().get("/extra/local.texture_profiles"));
        assertEquals(List.of(), plan.sourceOutputs().get("/extra/include.glsl"));
        var disabled = buildPackageProject(false, false);
        assertTrue(disabled.base().contains("/extra/data.json"));
        assertFalse(disabled.base().contains("/extra/local.texture_profilesc"));
        assertFalse(disabled.base().contains("/extra/include.glslc"));
    }

    // Cancelling during a provider write prevents finalization and removes staging without timing-dependent sleeps.
    @Test
    public void testPackageCancellationDuringWriting() throws Exception {
        createPackageProject();
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        var canceled = new java.util.concurrent.atomic.AtomicBoolean();
        PackageProviderFixture.beforeWrite = () -> canceled.set(true);
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("archive", "true");
            project.setOption("liveupdate", "true");
            Progress progress = new Progress(new Progress.Reporter() {
                public void report(com.dynamo.bob.IProgress.Message message, double fraction) {}
                public boolean isCanceled() { return canceled.get(); }
            });
            try {
                project.build(progress, "build");
            } catch (Exception expected) {
                assertTrue(canceled.get());
            }
            assertTrue(canceled.get());
            assertFalse(new File(contentRoot, "packages/custom.custom").exists());
            assertNoPackageStaging();
        } finally {
            PackageProviderFixture.beforeWrite = null;
        }
    }

    private void assertNoPackageStaging() throws IOException {
        try (var paths = Files.walk(Path.of(contentRoot))) {
            assertFalse(paths.anyMatch(path -> path.getFileName().toString().startsWith(".liveupdate-")));
        }
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
