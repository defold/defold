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

import com.defold.extension.pipeline.PackageProviderFixture;
import com.dynamo.bob.Bob;
import com.dynamo.bob.ClassLoaderScanner;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IProgress;
import com.dynamo.bob.MultipleCompileException;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Project;
import com.dynamo.bob.TaskResult;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.archive.ArchiveReader;
import com.dynamo.bob.archive.ManifestBuilder;
import com.dynamo.bob.archive.liveupdate.PackageArtifacts;
import com.dynamo.bob.archive.liveupdate.PackageBuilder;
import com.dynamo.bob.archive.liveupdate.PackageReport;
import com.dynamo.bob.archive.liveupdate.PackageResolver;
import com.dynamo.bob.archive.liveupdate.PackageWriter;
import com.dynamo.bob.archive.publisher.FolderPublisher;
import com.dynamo.bob.archive.publisher.NullPublisher;
import com.dynamo.bob.archive.publisher.Publisher;
import com.dynamo.bob.archive.publisher.PublisherSettings;
import com.dynamo.bob.archive.publisher.ZipPublisher;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.bundle.HTML5Bundler;
import com.dynamo.bob.bundle.IBundler;
import com.dynamo.bob.bundle.ICanceled;
import com.dynamo.bob.bundle.LinuxBundler;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.bob.pipeline.graph.ResourceGraph;
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.input.proto.Input.GamepadMapsRuntime;
import com.dynamo.liveupdate.proto.LiveUpdateManifest;
import com.dynamo.liveupdate.proto.Manifest;
import com.dynamo.liveupdate.proto.Manifest.ResourceEntryFlag;
import com.google.protobuf.TextFormat;
import org.apache.commons.configuration2.ex.ConfigurationException;
import org.apache.commons.io.FileUtils;
import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Test;

import javax.imageio.ImageIO;
import javax.tools.ToolProvider;
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
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;
import java.util.stream.Collectors;
import java.util.zip.ZipFile;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

public class ProjectBuildTest {

    private static final int LARGE_ARCHIVE_CUSTOM_RESOURCE_COUNT = 6;
    private static final long LARGE_ARCHIVE_CUSTOM_RESOURCE_SIZE = 16L;
    private static final int LARGE_ARCHIVE_RESOURCE_PADDING = 512 * 1024 * 1024;
    private static final long TWO_GIB = 2L * 1024L * 1024L * 1024L;
    private static final long MIN_LARGE_ARCHIVE_TEST_FREE_SPACE = 6L * 1024L * 1024L * 1024L;

    private String contentRoot;
    private String projectName = "Unnamed";

    private static class ArchiveProject extends Project {
        boolean failBundle;

        ArchiveProject(String root, String buildDirectory) {
            super(new DefaultFileSystem(), root, buildDirectory);
        }

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
    }

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
        try (Project project = new ArchiveProject(contentRoot, "build")) {
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

            List<TaskResult> result = project.build(Progress.discarding(), archive
                    ? new String[]{"clean", "build", "bundle"} : new String[]{"clean", "build"});
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

        try (Project project = new ArchiveProject(contentRoot, "build")) {
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

        try (Project project = new ArchiveProject(contentRoot, "build")) {
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
            try (Project project = new ArchiveProject(contentRoot, "build")) {
                project.setPublisher(new NullPublisher(new PublisherSettings()));
                ClassLoaderScanner scanner = new ClassLoaderScanner();
                project.scan(scanner, "com.dynamo.bob");
                project.scan(scanner, "com.dynamo.bob.pipeline");
                project.setOption("archive", "true");
                project.setOption("sound-stream-enabled", Boolean.toString(streaming));

                TaskResult soundResult = null;
                TaskResult archiveResult = null;
                for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
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

        try (Project project = new ArchiveProject(contentRoot, "build") {
            @Override
            public void createPublisher() throws CompileExceptionError {
                setPublisher(publisher);
            }
        }) {
            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");
            project.setOption("archive", "true");

            List<TaskResult> result = project.build(Progress.discarding(), "clean", "build", "bundle");
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
        try (Project project = new ArchiveProject(contentRoot, "build")) {
            project.setPublisher(new NullPublisher(new PublisherSettings()));

            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");

            project.setOption("archive", "true");
            project.setOption("archive-resource-padding", Integer.toString(LARGE_ARCHIVE_RESOURCE_PADDING));
            project.setOption("build-report-json", new File(contentRoot, "large_archive_report.json").getAbsolutePath());
            project.setOption("build-report-html", new File(contentRoot, "large_archive_report.html").getAbsolutePath());
            project.setOption("max-cpu-threads", "1");

            List<TaskResult> result = project.build(Progress.discarding(), "clean", "build", "bundle");
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
        try (Project project = new ArchiveProject(contentRoot, "build")) {
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
        } catch (Exception | AssertionError expected) {
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

    // Existing ZIP/folder bundle outputs created by the old publishers can be replaced without ownership indexes.
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
            try (Project project = new ArchiveProject(contentRoot, "build")) {
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
                assertFalse(unchanged.equals(Files.getLastModifiedTime(bundled)));
            }
            assertNoPackageStaging();
        }
    }

    // Legacy mixed routes, inherited exclusion and custom files keep their exact archive membership.
    // ZIP/folder providers use the same payloads and tight manifests while retaining configured names.
    @Test
    public void testLegacyPackageContentsAndRebuild() throws Exception {
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
            // Existing legacy outputs predate ownership indexes and must remain valid destinations.
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
            assertFalse(unchanged.equals(Files.getLastModifiedTime(output)));
            try (Project project = new ArchiveProject(contentRoot, "build")) {
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
        // Removed publisher modes must not prevent ordinary builds with Live Update disabled.
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Amazon\n");
        PackageResolver.Plan disabled = buildPackageProject(false, false);
        assertEquals(all, disabled.base());
        assertEquals(all, manifestPaths(readManifestData(getBundledManifestFile())));
        assertTrue(disabled.packages().get("liveupdate").resources().isEmpty());
        assertFalse(Files.exists(Path.of(contentRoot, "content.archivemanifest")));
    }

    // Conversion preserves mixed proxy routes, inherited exclusion, shared content and custom files in the emitted archives.
    @Test
    public void testLegacyManifestConversion() throws Exception {
        createLegacyPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"), "save-folder-in-bundle-folder=1\n",
                java.nio.file.StandardOpenOption.APPEND);
        PackageResolver.Plan legacy = buildPackageProject(true, false);
        File bundle = new File(contentRoot, "bundle");
        try (Project project = new ArchiveProject(contentRoot, "build")) {
            BundleHelper.moveBundleIfNeed(project, bundle);
        }
        assertTrue(new File(bundle, "published/liveupdate.game.dmanifest").isFile());
        var manifest = LiveUpdateManifest.ManifestDesc.newBuilder().setOutputDirectory("packages")
                .addPackages(LiveUpdateManifest.PackageDesc.newBuilder().setName("liveupdate")
                        .addAllRules(PackageResolver.migrateLegacy(legacy))).build();
        createFile(contentRoot, "converted.archivemanifest", TextFormat.printer().printToString(manifest));
        Files.writeString(Path.of(contentRoot, "game.project"), "\n[liveupdate]\nsettings =\n\n[archive]\nmanifest = /converted.archivemanifest\n",
                java.nio.file.StandardOpenOption.APPEND);
        PackageResolver.Plan converted = buildPackageProject(true, false);
        try (Project project = new ArchiveProject(contentRoot, "build")) {
            BundleHelper.moveBundleIfNeed(project, bundle);
        }
        assertFalse(new File(bundle, "published").exists());
        assertEquals(legacy.base(), converted.base());
        assertEquals(legacy.packages().get("liveupdate").resources(), converted.packages().get("liveupdate").resources());
        assertEquals(legacy.base(), manifestPaths(readManifestData(getBundledManifestFile())));
        assertEquals(legacy.packages().get("liveupdate").resources(), manifestPaths(zipManifest("liveupdate")));
        assertEquals("collection: \"/logic/level.collection\"\nexclude: true\n",
                Files.readString(Path.of(contentRoot, "logic/level.collectionproxy")));
    }

    private void createPackageProject() throws IOException {
        createExcludedLiveUpdateProject("settings =\n\n[archive]\nmanifest = /content.archivemanifest\n");
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
        return LiveUpdateManifest.PackageDesc.newBuilder().setName(name).setContainerPreset(provider.equals("zip") ? "" : provider).addAllRules(List.of(rules)).build();
    }

    private void savePackages(LiveUpdateManifest.PackageDesc... packages) throws IOException {
        var manifest = LiveUpdateManifest.ManifestDesc.newBuilder().setOutputDirectory("packages").addAllPackages(List.of(packages));
        for (String provider : Arrays.stream(packages).map(LiveUpdateManifest.PackageDesc::getContainerPreset).distinct().toList()) {
            if (!provider.isEmpty()) {
                manifest.addContainerPresets(LiveUpdateManifest.ContainerPreset.newBuilder().setName(provider).setProvider(provider));
            }
        }
        createFile(contentRoot, "content.archivemanifest", TextFormat.printer().printToString(manifest.build()));
    }

    private void setPackageOutputDirectory(String directory) throws IOException {
        Path path = Path.of(contentRoot, "content.archivemanifest");
        var manifest = com.dynamo.bob.archive.liveupdate.PackageBuilder.parse(Files.readAllBytes(path));
        Files.writeString(path, TextFormat.printer().printToString(manifest.toBuilder().setOutputDirectory(directory).build()));
    }

    private void setFileCompressionPreset(String packageName, LiveUpdateManifest.FileCompressionPreset preset) throws IOException {
        Path path = Path.of(contentRoot, "content.archivemanifest");
        var manifest = PackageBuilder.parse(Files.readAllBytes(path)).toBuilder();
        manifest.clearFileCompressionPresets().addFileCompressionPresets(preset);
        for (var pkg : manifest.getPackagesBuilderList()) {
            if (pkg.getName().equals(packageName)) {
                pkg.setFileCompressionPreset(preset.getName());
            }
        }
        Files.writeString(path, TextFormat.printer().printToString(manifest.build()));
    }

    private void setContainerPreset(String packageName, LiveUpdateManifest.ContainerPreset preset) throws IOException {
        Path path = Path.of(contentRoot, "content.archivemanifest");
        var manifest = PackageBuilder.parse(Files.readAllBytes(path)).toBuilder();
        var presets = manifest.getContainerPresetsList().stream().filter(item -> !item.getName().equals(preset.getName())).toList();
        manifest.clearContainerPresets().addAllContainerPresets(presets).addContainerPresets(preset);
        for (var pkg : manifest.getPackagesBuilderList()) {
            if (pkg.getName().equals(packageName)) {
                pkg.setContainerPreset(preset.getName());
            }
        }
        Files.writeString(path, TextFormat.printer().printToString(manifest.build()));
    }

    private void setProviderProperties(String provider, Map<String, String> values) throws IOException {
        Path path = Path.of(contentRoot, "content.archivemanifest");
        var manifest = com.dynamo.bob.archive.liveupdate.PackageBuilder.parse(Files.readAllBytes(path));
        var settings = LiveUpdateManifest.ProviderSettings.newBuilder().setProvider(provider);
        values.forEach((name, value) -> settings.addProperties(LiveUpdateManifest.ProviderProperty.newBuilder().setName(name).setValue(value)));
        Files.writeString(path, TextFormat.printer().printToString(manifest.toBuilder().clearProviders()
                .addAllProviders(manifest.getProvidersList().stream().filter(item -> !item.getProvider().equals(provider)).toList())
                .addProviders(settings).build()));
    }

    private PackageResolver.Plan buildPackageProject(boolean export, boolean preview, String... commands) throws Exception {
        try (Project project = new ArchiveProject(contentRoot, "build")) {
            ClassLoaderScanner scanner = new ClassLoaderScanner();
            project.scan(scanner, "com.dynamo.bob");
            project.scan(scanner, "com.dynamo.bob.pipeline");
            project.setOption("archive", "true");
            project.setOption("variant", "debug");
            project.setOption("liveupdate", Boolean.toString(export));
            project.setOption("package-preview", Boolean.toString(preview));
            project.setOption("with-symbols", null); // Match CLI flags that have no argument.
            List<String> requested = new ArrayList<>(commands.length == 0 ? List.of("build") : List.of(commands));
            if (!preview && !requested.contains("bundle")) {
                requested.add("bundle");
            }
            for (TaskResult result : project.build(Progress.discarding(), requested.toArray(String[]::new))) assertTrue(result.toString(), result.isOk());
            return project.getLiveUpdatePlan();
        }
    }

    // Build saves the graph without archives; bundle uses prebuilt bytes even when source contents have changed.
    @Test
    public void testBuildAndBundleAreSeparate() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(true)) {
            project.setOption("variant", "debug");
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        Path root = Path.of(contentRoot);
        assertTrue(Files.isRegularFile(root.resolve("build/game.graph.json")));
        assertFalse(Files.exists(root.resolve("build/game.arcd")));
        assertFalse(Files.exists(root.resolve("build/game.dmanifest")));
        assertFalse(Files.exists(root.resolve("build/game.lupackages")));
        assertFalse(Files.exists(root.resolve("packages")));
        byte[] compiled = Files.readAllBytes(root.resolve("build/extra/data.json"));
        Files.writeString(root.resolve("extra/data.json"), "changed after build");
        Files.writeString(root.resolve("logic/level.collection"), "not valid collection syntax");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            assertTrue(project.build(Progress.discarding(), "bundle").isEmpty());
            assertEquals(Set.of("episode"), PackageArtifacts.readOutputs(project).stream()
                    .map(PackageArtifacts.Output::packageName).collect(Collectors.toSet()));
        }
        assertArrayEquals(compiled, Files.readAllBytes(root.resolve("build/extra/data.json")));
        assertFalse(Files.exists(root.resolve("packages/copy")));
        assertFalse(Files.exists(root.resolve("build/game.arcd")));
        assertFalse(Files.exists(root.resolve("build/bundle/application")));
        assertEquals(1, packageReport().get("packages").size());
        assertEquals("episode", packageReport().get("packages").get(0).get("name").asText());
        assertTrue(manifestPaths(zipManifest("episode")).contains("/extra/data.json"));
    }

    // The initial selective build establishes the graph; later selections retain other compiled data and artifacts.
    @Test
    public void testSelectiveBuildsAccumulate() throws Exception {
        createPackageProject();
        createFile(contentRoot, "other/notes.txt", "first notes");
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/.*")),
                livePackage("notes", "zip", addPath("Notes", "/other/.*")));
        Path root = Path.of(contentRoot);
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        assertEquals("first notes", Files.readString(root.resolve("build/other/notes.txt")));
        Files.writeString(root.resolve("extra/data.json"), "second data");
        Files.writeString(root.resolve("other/notes.txt"), "second notes");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
                assertFalse(result.getTask().firstInput().getPath().endsWith("other/notes.txt"));
            }
        }
        assertEquals("second data", Files.readString(root.resolve("build/extra/data.json")));
        assertEquals("first notes", Files.readString(root.resolve("build/other/notes.txt")));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "notes");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
                assertFalse(result.getTask().firstInput().getPath().endsWith("extra/data.json"));
            }
        }
        assertEquals("second notes", Files.readString(root.resolve("build/other/notes.txt")));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode,notes");
            project.build(Progress.discarding(), "bundle");
        }
        assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest("episode")));
        assertEquals(Set.of("/other/notes.txt"), manifestPaths(zipManifest("notes")));
        byte[] previous = Files.readAllBytes(root.resolve("packages/notes.zip"));
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(root.resolve("packages/notes.zip"), unchanged);
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            project.build(Progress.discarding(), "bundle");
        }
        assertArrayEquals(previous, Files.readAllBytes(root.resolve("packages/notes.zip")));
        assertEquals(unchanged, Files.getLastModifiedTime(root.resolve("packages/notes.zip")));
        createFile(contentRoot, "other/new.txt", "newly matching");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        assertEquals("newly matching", Files.readString(root.resolve("build/other/new.txt")));
        Files.delete(root.resolve("other/new.txt"));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
            assertEquals(Set.of("/other/notes.txt"), project.getLiveUpdatePlan().packages().get("notes").resources());
        }
    }

    // A saved graph lets selective builds skip invalid unselected sources, including those behind bootstrap proxies.
    @Test
    public void testSelectiveBuildSkipsUnselectedSources() throws Exception {
        createPackageProject();
        createFile(contentRoot, "other/isolated.collection", "name: \"isolated\"\n");
        savePackages(livePackage("common", "zip", addPath("Data", "/extra/data[.]json")),
                livePackage("episode", "zip", addPath("Level", "/logic/level[.]collection")),
                livePackage("isolated", "zip", addPath("Isolated", "/other/isolated[.]collection")));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "common");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        Path root = Path.of(contentRoot);
        byte[] level = Files.readAllBytes(root.resolve("build/logic/level.collectionc"));
        byte[] isolated = Files.readAllBytes(root.resolve("build/other/isolated.collectionc"));
        createFile(contentRoot, "logic/level.collection", "invalid collection syntax");
        createFile(contentRoot, "other/isolated.collection", "invalid collection syntax");
        createFile(contentRoot, "extra/data.json", "updated data");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "common");
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                assertTrue(result.toString(), result.isOk());
                assertFalse(result.getTask().firstInput().getPath().startsWith("logic/"));
                assertFalse(result.getTask().firstInput().getPath().startsWith("other/"));
            }
        }
        assertEquals("updated data", Files.readString(root.resolve("build/extra/data.json")));
        assertArrayEquals(level, Files.readAllBytes(root.resolve("build/logic/level.collectionc")));
        assertArrayEquals(isolated, Files.readAllBytes(root.resolve("build/other/isolated.collectionc")));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode,isolated");
            project.build(Progress.discarding(), "bundle");
        }
        assertEquals(Set.of("/logic/level.collectionc", "/logic/level.goc", "/logic/level.scriptc"),
                manifestPaths(zipManifest("episode")));
        assertEquals(Set.of("/other/isolated.collectionc"), manifestPaths(zipManifest("isolated")));
        Manifest.ResourceEntry script = zipManifest("episode").getResourcesList().stream()
                .filter(resource -> resource.getUrl().equals("/logic/level.scriptc")).findFirst().orElseThrow();
        assertTrue((script.getFlags() & ResourceEntryFlag.ENCRYPTED_VALUE) != 0);
    }

    // Legacy selective builds compile excluded roots without parsing Base and retain the mixed-proxy classification.
    @Test
    public void testLegacySelectiveBuildSkipsBaseSources() throws Exception {
        createLegacyPackageProject();
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Zip\nzip-filepath=packages\nzip-filename=episode.zip\n");
        PackageResolver.Plan previous = buildPackageProject(true, false);
        createFile(contentRoot, "logic/main.collection", "invalid collection syntax");
        createFile(contentRoot, "logic/level.script", "function init(self) print('changed') end\n");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "liveupdate");
            boolean rebuiltScript = false;
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                assertTrue(result.toString(), result.isOk());
                if (result.getTask().firstInput().getPath().equals("logic/level.script")) {
                    rebuiltScript = result.getResult() == TaskResult.Result.SUCCESS;
                }
            }
            assertTrue(rebuiltScript);
            assertEquals(previous.base(), project.getLiveUpdatePlan().base());
        }
        assertEquals(previous.packages().get("liveupdate").resources(), manifestPaths(zipManifest("episode")));
    }

    // Selective builds retain legacy engine preparation: rebuild native engines and remove them when extensions disappear.
    @Test
    public void testSelectiveBuildKeepsEnginePreparation() throws Exception {
        Platform platform = Platform.getHostPlatform();
        Path engine = Path.of(contentRoot, "build", platform.getExtenderPair(), platform.formatBinaryName("dmengine").getFirst());
        for (boolean legacy : List.of(false, true)) {
            if (legacy) {
                createLegacyPackageProject();
            } else {
                createPackageProject();
            }
            String selection = legacy ? "liveupdate" : "episode";
            createFile(contentRoot, "extension/ext.manifest", "name: TestExtension\n");
            createFile(contentRoot, "extension/src/test.cpp", "// Native extension source\n");
            Files.createDirectories(engine.getParent());
            Files.writeString(engine, "debug");
            try (Project project = new ArchiveProject(contentRoot, "build") {
                @Override
                public void buildEnginePlatform(File buildDir, File cacheDir, Map<String, String> options,
                        Platform target, com.dynamo.bob.IProgress progress) throws IOException {
                    try (progress) {
                        Files.writeString(buildDir.toPath().resolve(target.formatBinaryName("dmengine").getFirst()),
                                options.get("baseVariant"));
                    }
                }
            }) {
                project.setOption("platform", platform.getPair());
                project.setOption("architectures", platform.getPair());
                project.setOption("variant", "release");
                project.setOption("package", selection);
                for (TaskResult result : project.build(Progress.discarding(), "build")) {
                    assertTrue(result.toString(), result.isOk());
                }
            }
            assertEquals("release", Files.readString(engine));
            Files.delete(Path.of(contentRoot, "extension/ext.manifest"));
            try (Project project = packageReportProject(false)) {
                project.setOption("platform", platform.getPair());
                project.setOption("architectures", platform.getPair());
                project.setOption("package", selection);
                for (TaskResult result : project.build(Progress.discarding(), "build")) {
                    assertTrue(result.toString(), result.isOk());
                }
            }
            assertFalse(Files.exists(engine));
        }
    }

    // A changed provider destination cannot replace an unselected package's previously generated artifact.
    @Test
    public void testSelectiveBundlePreservesArtifactOwnership() throws Exception {
        createPackageProject();
        PackageProviderFixture.collide = true;
        try {
            savePackages(livePackage("one", "test-package", addPath("Data", "/extra/.*")));
            buildPackageProject(true, false);
            Path artifact = Path.of(contentRoot, "packages/collision.custom");
            byte[] previous = Files.readAllBytes(artifact);
            savePackages(livePackage("two", "test-package", addPath("Data", "/extra/.*")));
            int writes = PackageProviderFixture.writes;
            try (Project project = packageReportProject(false)) {
                project.setOption("variant", "debug");
                project.setOption("package", "two");
                try {
                    project.build(Progress.discarding(), "bundle");
                    org.junit.Assert.fail("Selective bundle must preserve the unselected package");
                } catch (CompileExceptionError expected) {
                    assertTrue(expected.getMessage(), expected.getMessage().contains("unselected package 'one'"));
                }
            }
            assertEquals(writes, PackageProviderFixture.writes);
            assertArrayEquals(previous, Files.readAllBytes(artifact));
            assertNoPackageStaging();
        } finally {
            PackageProviderFixture.collide = false;
        }
    }

    // Current rules and compression apply without compilation; newly matched unbuilt sources require a build.
    @Test
    public void testBundleUsesCurrentManifestWithPrebuiltResources() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/notes.txt", "compressible data ".repeat(200));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        Path root = Path.of(contentRoot);
        byte[] graph = Files.readAllBytes(root.resolve("build/game.graph.json"));
        savePackages(livePackage("notes", "zip", addPath("Notes", "/extra/notes[.]txt")));
        setFileCompressionPreset("notes", LiveUpdateManifest.FileCompressionPreset.newBuilder()
                .setName("plain").setFileCompression(LiveUpdateManifest.FileCompression.NONE).build());
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "notes");
            project.build(Progress.discarding(), "bundle");
        }
        Manifest.ManifestData manifest = zipManifest("notes");
        assertEquals(Set.of("/extra/notes.txt"), manifestPaths(manifest));
        assertEquals(0, manifest.getResources(0).getFlags() & Manifest.ResourceEntryFlag.COMPRESSED_VALUE);
        assertArrayEquals(graph, Files.readAllBytes(root.resolve("build/game.graph.json")));
        createFile(contentRoot, "extra/unbuilt.txt", "not compiled");
        savePackages(livePackage("notes", "zip", addPath("Notes", "/extra/.*[.]txt")));
        byte[] previous = Files.readAllBytes(root.resolve("packages/notes.zip"));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "notes");
            try {
                project.build(Progress.discarding(), "bundle");
                org.junit.Assert.fail("Bundling an unbuilt resource must fail");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Resource has not been built: /extra/unbuilt.txt"));
            }
        }
        assertArrayEquals(previous, Files.readAllBytes(root.resolve("packages/notes.zip")));
    }

    // Explicit Base uses manifest membership; the default folds all selected content into a self-contained Base.
    @Test
    public void testBasePackageSelection() throws Exception {
        createPackageProject();
        PackageResolver.Plan expected = buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        byte[] external = Files.readAllBytes(zip);
        for (boolean explicitBase : List.of(true, false)) {
            try (Project project = packageReportProject(false)) {
                project.setOption("variant", "debug");
                if (explicitBase) project.setOption("package", "base");
                project.build(Progress.discarding(), "bundle");
            }
            assertEquals(explicitBase ? expected.base() : expected.emittedResources(),
                    manifestPaths(readManifestData(getBundledManifestFile())));
            assertArrayEquals(external, Files.readAllBytes(zip));
            assertEquals(1, packageReport().get("packages").size());
        }
    }

    // A failed compilation cannot publish a graph, and bundle detects changes to recorded compiled bytes.
    @Test
    public void testFailedBuildDoesNotPublishGraph() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        Path root = Path.of(contentRoot);
        byte[] graph = Files.readAllBytes(root.resolve("build/game.graph.json"));
        Files.writeString(root.resolve("logic/level.script"), "function broken(\n");
        try (Project project = packageReportProject(true)) {
            try {
                project.build(Progress.discarding(), "build");
                org.junit.Assert.fail("Invalid source must fail the build");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("mismatched input"));
            }
        }
        assertArrayEquals(graph, Files.readAllBytes(root.resolve("build/game.graph.json")));
        Files.writeString(root.resolve("build/extra/data.json"), "changed compiled bytes");
        try (Project project = packageReportProject(true)) {
            try {
                project.build(Progress.discarding(), "bundle");
                org.junit.Assert.fail("Changed compiled data must fail validation");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Compiled resource is missing or changed: /extra/data.json"));
            }
        }
        assertFalse(Files.exists(root.resolve("packages")));
    }

    // Cancellation after task completion must retain the last successful graph for subsequent validation.
    @Test
    public void testCanceledBuildDoesNotPublishGraph() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        Path root = Path.of(contentRoot);
        Path compiled = root.resolve("build/extra/data.json");
        byte[] graph = Files.readAllBytes(root.resolve("build/game.graph.json"));
        byte[] previous = Files.readAllBytes(compiled);
        Files.writeString(root.resolve("extra/data.json"), "updated data");
        AtomicBoolean canceled = new AtomicBoolean();
        Progress progress = new Progress(new Progress.Reporter() {
            public void report(com.dynamo.bob.IProgress.Message message, double fraction) {
                try {
                    if (!Arrays.equals(previous, Files.readAllBytes(compiled))) {
                        canceled.set(true);
                    }
                } catch (IOException e) {
                    throw new java.io.UncheckedIOException(e);
                }
            }
            public boolean isCanceled() {
                return canceled.get();
            }
        });
        try (Project project = packageReportProject(true)) {
            try {
                project.build(progress, "build");
                org.junit.Assert.fail("Cancellation must fail the build");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().toLowerCase().contains("cancel"));
            }
        }
        assertTrue(canceled.get());
        assertArrayEquals(graph, Files.readAllBytes(root.resolve("build/game.graph.json")));
        assertFalse(Files.exists(root.resolve("packages")));
    }

    // Contentless applications need no saved graph, and external-only selections must not invoke the application bundler.
    @Test
    public void testContentlessBundleSelection() throws Exception {
        createPackageProject();
        Path application = Path.of(contentRoot, "build/bundle/application");
        try (Project project = packageReportProject(false)) {
            project.setOption("exclude-archive", "true");
            project.setOption("package", "episode");
            assertTrue(project.build(Progress.discarding(), "bundle").isEmpty());
        }
        assertFalse(Files.exists(application));
        try (Project project = packageReportProject(false)) {
            project.setOption("exclude-archive", "true");
            project.setOption("package", "base");
            assertTrue(project.build(Progress.discarding(), "bundle").isEmpty());
        }
        assertTrue(Files.exists(application));
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.graph.json")));
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.arcd")));
        assertFalse(Files.exists(Path.of(contentRoot, "packages")));
    }

    // Contentless bundles must not require legacy publisher settings when package export is enabled.
    @Test
    public void testContentlessBundleWithoutPublisherSettings() throws Exception {
        for (String option : List.of("liveupdate", "all-packages")) {
            Path bundleDirectory = Path.of(contentRoot, "build/bundle", option);
            try (Project project = packageReportProject(false)) {
                project.setOption("exclude-archive", "true");
                project.setOption(option, "true");
                project.setOption("bundle-output", bundleDirectory.toString());
                assertTrue(project.build(Progress.discarding(), "bundle").isEmpty());
            }
            assertEquals("complete", Files.readString(bundleDirectory.resolve("application")));
        }
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.graph.json")));
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.arcd")));
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.lupackages")));
    }

    // CLI aliases warn once, and repeated package options override the deprecated Live Update switch.
    @Test
    public void testDeprecatedArchiveAndLiveUpdateOptions() throws Exception {
        createPackageProject();
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        var output = new java.io.ByteArrayOutputStream();
        java.io.PrintStream original = System.out;
        try (var capture = new java.io.PrintStream(output, true, java.nio.charset.StandardCharsets.UTF_8)) {
            System.setOut(capture);
            var result = Bob.invoke(null, Progress.discarding(), null, new String[]{
                    "--root", contentRoot, "--output", "build", "--variant", "debug",
                    "-a", "--archive", "--liveupdate", "yes", "--all-packages", "build"});
            assertTrue(result.success);
        } finally {
            System.setOut(original);
        }
        String warnings = output.toString(java.nio.charset.StandardCharsets.UTF_8);
        assertEquals(1, warnings.split("--archive is deprecated", -1).length - 1);
        assertEquals(1, warnings.split("--liveupdate is deprecated", -1).length - 1);
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.arcd")));
        assertFalse(Files.exists(Path.of(contentRoot, "packages")));
        var result = Bob.invoke(null, Progress.discarding(), null, new String[]{
                "--root", contentRoot, "--output", "build", "--variant", "debug",
                "--liveupdate", "no", "--package", "episode", "--package", "copy", "bundle"});
        assertTrue(result.success);
        assertTrue(Files.exists(Path.of(contentRoot, "packages/episode.zip")));
        assertTrue(Files.exists(Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest")));
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.arcd")));
    }

    // Package references select their roots for compilation, including changed runtime dependencies, without emitting them.
    @Test
    public void testSelectivePackageReferencesBuildDependencies() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "copy");
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        Path compiled = Path.of(contentRoot, "build/logic/level.scriptc");
        byte[] previous = Files.readAllBytes(compiled);
        createFile(contentRoot, "logic/new_dependency.lua", "return {value = 'updated dependency'}\n");
        createFile(contentRoot, "logic/level.script", "local dependency = require 'logic.new_dependency'\n"
                + "function init(self) print(dependency.value) end\n");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "copy");
            boolean rebuiltScript = false;
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                assertTrue(result.toString(), result.isOk());
                if (result.getTask().firstInput().getPath().endsWith("logic/level.script")) {
                    assertEquals(TaskResult.Result.SUCCESS, result.getResult());
                    rebuiltScript = true;
                }
            }
            assertTrue(rebuiltScript);
        }
        assertFalse(Arrays.equals(previous, Files.readAllBytes(compiled)));
        assertFalse(Files.exists(Path.of(contentRoot, "packages/episode.zip")));
        Manifest.ManifestData manifest = readManifestData(new File(contentRoot, "packages/copy/liveupdate.game.dmanifest"));
        assertTrue(manifestPaths(manifest).contains("/logic/level.scriptc"));
        assertTrue(manifestPaths(manifest).contains("/logic/new_dependency.luac"));
    }

    // Bundle requires a compatible successful build and never repairs missing inputs by compiling them.
    @Test
    public void testBundleRequiresCompatibleBuildGraph() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(true)) {
            try {
                project.build(Progress.discarding(), "bundle");
                org.junit.Assert.fail("Bundle without a build graph must fail");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Build graph is missing"));
            }
        }
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        for (var option : Map.of("variant", "debug", "experimental-path-minification", "true").entrySet()) {
            try (Project project = packageReportProject(true)) {
                project.setOption(option.getKey(), option.getValue());
                try {
                    project.build(Progress.discarding(), "bundle");
                    org.junit.Assert.fail("Different compilation settings must fail");
                } catch (CompileExceptionError expected) {
                    assertTrue(expected.getMessage(), expected.getMessage().contains("Build configuration differs"));
                }
            }
        }
        Files.delete(Path.of(contentRoot, "build/extra/data.json"));
        try (Project project = packageReportProject(true)) {
            try {
                project.build(Progress.discarding(), "bundle");
                org.junit.Assert.fail("Missing compiled data must fail");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Compiled resource is missing or changed"));
            }
        }
        assertFalse(Files.exists(Path.of(contentRoot, "packages")));
    }

    // Invalid selections fail before compilation or packaging, including conflicting CLI root selectors.
    @Test
    public void testPackageSelectionValidation() throws Exception {
        createPackageProject();
        try (Project project = packageReportProject(false)) {
            project.setOption("package", "missing");
            try {
                project.build(Progress.discarding(), "build");
                org.junit.Assert.fail("Unknown packages must fail");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Unknown package 'missing'"));
            }
        }
        for (List<String> options : List.of(List.of("--all-packages"), List.of("--build-input", "/logic/main.collection"))) {
            List<String> arguments = new ArrayList<>(List.of("--root", contentRoot, "--package", "episode"));
            arguments.addAll(options);
            arguments.add("build");
            try {
                Bob.invoke(null, Progress.discarding(), null, arguments.toArray(String[]::new));
                org.junit.Assert.fail("Conflicting package selection must fail");
            } catch (Bob.OptionValidationException expected) {
                // The CLI rejected the combination before invoking any build command.
            }
        }
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.graph.json")));
        assertFalse(Files.exists(Path.of(contentRoot, "packages")));
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
        ArchiveProject project = new ArchiveProject(contentRoot, "build");
        project.failBundle = failBundle;
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

    // External-only reports must not count Base storage for resources shared with startup content.
    @Test
    public void testSelectedPackageReportExcludesBaseSizes() throws Exception {
        createPackageProject();
        savePackages(livePackage("startup", "zip", addPath("Startup", "/logic/main[.]collection")));
        try (Project project = packageReportProject(false)) {
            project.setOption("package", "startup");
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        var report = packageReport();
        assertEquals(1, report.get("packages").size());
        assertEquals("startup", report.get("packages").get(0).get("name").asText());
        assertTrue(report.get("package_resources").size() > 0);
        for (var resource : report.get("package_resources")) {
            assertEquals(1, resource.get("compressed_sizes").size());
            assertTrue(resource.get("compressed_sizes").has("startup"));
            assertFalse(resource.get("compressed_sizes").has("base"));
        }
    }

    // Bundles regenerate packages and reports from the saved graph without compiling resources.
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
        for (String[] commands : List.of(new String[]{"build", "bundle"}, new String[]{"bundle"})) {
            Files.setLastModifiedTime(cache, unchanged);
            Files.setLastModifiedTime(zip, unchanged);
            Files.delete(Path.of(contentRoot, "report.html"));
            try (Project project = packageReportProject(true)) {
                // The deprecated archive flag has no effect on bundling.
                if (commands.length == 1) project.setOption("archive", "false");
                for (TaskResult result : project.build(Progress.discarding(), commands)) assertTrue(result.toString(), result.isOk());
                assertNotNull(project.getLiveUpdatePlan());
                assertNotNull(project.getArchiveBuilder());
            }
            assertEquals(expected.get("packages"), packageReport().get("packages"));
            assertEquals(expected.get("package_resources"), packageReport().get("package_resources"));
            assertNotEquals(unchanged, Files.getLastModifiedTime(cache));
            assertNotEquals(unchanged, Files.getLastModifiedTime(zip));
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

    // Build reports count compiled resources independently of archive deduplication and padding.
    @Test
    public void testBuildReportUsesCompiledSizes() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/first.json", "12345");
        createFile(contentRoot, "extra/second.json", "12345");
        Path reportPath = Path.of(contentRoot, "build-report.json");
        try (Project project = packageReportProject(false)) {
            project.setOption("archive-resource-padding", "64");
            project.setOption("build-report-json", reportPath.toString());
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        var report = new com.fasterxml.jackson.databind.ObjectMapper().readTree(Files.readAllBytes(reportPath));
        int count = 0;
        long size = 0;
        long compressedSize = 0;
        for (var resource : report.get("resources")) {
            String path = resource.get("file").asText();
            if (path.equals("/extra/first.json") || path.equals("/extra/second.json")) {
                ++count;
                size += resource.get("size").asLong();
                compressedSize += resource.get("compressed_size").asLong();
            }
        }
        assertEquals(2, count);
        assertEquals(10, size);
        assertEquals(10, compressedSize);
        assertFalse(report.has("packages"));
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

    // Base and overlapping ZIP/folder packages choose compression independently; manifests and payloads agree.
    @Test
    public void testPackageFileCompression() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/data.txt", "Package compression example.\n".repeat(1000));
        createFile(contentRoot, "logic/startup.script", "function init(self) print(\"" + "Startup content. ".repeat(1000) + "\") end");
        Files.writeString(Path.of(contentRoot, "logic/main.go"),
                "components { id: \"startup\" component: \"/logic/startup.script\" }\n", java.nio.file.StandardOpenOption.APPEND);
        var manifest = LiveUpdateManifest.ManifestDesc.newBuilder().setOutputDirectory("packages");
        manifest.addContainerPresets(LiveUpdateManifest.ContainerPreset.newBuilder().setName("folder").setProvider("folder"));
        manifest.addContainerPresets(LiveUpdateManifest.ContainerPreset.newBuilder().setName("test-package").setProvider("test-package"));
        for (var compression : List.of(LiveUpdateManifest.FileCompression.LZ4, LiveUpdateManifest.FileCompression.NONE)) {
            manifest.addFileCompressionPresets(LiveUpdateManifest.FileCompressionPreset.newBuilder().setName(compression.name())
                    .setFileCompression(compression).setLz4Level(12));
            for (String provider : List.of("zip", "folder")) {
                manifest.addPackages(livePackage(provider + compression, provider,
                        addPath("Shared resources", "/extra/data[.]txt|/logic/startup[.]script"))
                        .toBuilder().setFileCompressionPreset(compression.name()));
            }
        }
        manifest.addPackages(livePackage("custom", "test-package", addPath("Data", "/extra/data[.]txt"))
                .toBuilder().setFileCompressionPreset("NONE"));
        String gameProject = Files.readString(Path.of(contentRoot, "game.project"));
        for (var compression : List.of(LiveUpdateManifest.FileCompression.NONE, LiveUpdateManifest.FileCompression.LZ4)) {
            boolean compressed = compression == LiveUpdateManifest.FileCompression.LZ4;
            Files.writeString(Path.of(contentRoot, "game.project"), gameProject + "\n[project]\ncompress_archive=" + (compressed ? "0" : "1") + "\n");
            Files.writeString(Path.of(contentRoot, "content.archivemanifest"),
                    TextFormat.printer().printToString(manifest.setBaseFileCompressionPreset(compression.name()).build()));
            buildPackageProject(true, false);
            Manifest.ResourceEntry base = readManifestData(getBundledManifestFile()).getResourcesList().stream()
                    .filter(entry -> entry.getUrl().equals("/logic/startup.scriptc")).findFirst().orElseThrow();
            assertEquals(compressed, (base.getFlags() & Manifest.ResourceEntryFlag.COMPRESSED_VALUE) != 0);
            var report = new com.fasterxml.jackson.databind.ObjectMapper().readTree(
                    Files.readAllBytes(Path.of(contentRoot, "build/game.lureport.json")));
            Map<String, com.fasterxml.jackson.databind.JsonNode> sizes = new LinkedHashMap<>();
            for (var resource : report.get("package_resources")) {
                sizes.put(resource.get("file").asText(), resource.get("compressed_sizes"));
            }
            for (var pkg : manifest.getPackagesList()) {
                String provider = PackageResolver.containerPreset(manifest.build(), pkg.getContainerPreset()).getProvider();
                if (provider.equals("test-package")) {
                    var info = PackageProviderFixture.lastContext.resources().get("/extra/data.txt");
                    assertEquals(compressed, (info.flags() & ArchiveEntry.FLAG_COMPRESSED) != 0);
                    assertEquals(Set.of("/extra/data.txt"), manifestPaths(PackageProviderFixture.lastManifest));
                    assertEquals(ArchiveEntry.FLAG_UNCOMPRESSED, PackageProviderFixture.lastManifest.getResources(0).getCompressedSize());
                    continue;
                }
                Manifest.ManifestData local = provider.equals("zip") ? zipManifest(pkg.getName())
                        : readManifestData(Path.of(contentRoot, "packages", pkg.getName(), "liveupdate.game.dmanifest").toFile());
                assertEquals(Set.of("/extra/data.txt", "/logic/startup.scriptc"), manifestPaths(local));
                for (Manifest.ResourceEntry entry : local.getResourcesList()) {
                    boolean fileCompressed = PackageResolver.fileCompressionPreset(manifest.build(), pkg.getFileCompressionPreset()).getFileCompression() == LiveUpdateManifest.FileCompression.LZ4;
                    assertEquals(fileCompressed, (entry.getFlags() & Manifest.ResourceEntryFlag.COMPRESSED_VALUE) != 0);
                    String hash = com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
                    byte[] payload;
                    if (provider.equals("zip")) {
                        try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages", pkg.getName() + ".zip").toFile())) {
                            payload = zip.getInputStream(zip.getEntry(hash)).readAllBytes();
                        }
                    } else {
                        payload = Files.readAllBytes(Path.of(contentRoot, "packages", pkg.getName(), hash));
                    }
                    assertPackagePayload(entry, payload);
                    assertEquals(payload.length - 16, sizes.get(entry.getUrl()).get(pkg.getName()).asLong());
                    if (entry.getUrl().equals(base.getUrl()) && compression == PackageResolver.fileCompressionPreset(manifest.build(), pkg.getFileCompressionPreset()).getFileCompression()) {
                        assertEquals(base.getHash(), entry.getHash());
                    }
                }
            }
        }
        // Turning export off puts all selected files in Base using Base's compression.
        buildPackageProject(false, false);
        Manifest.ResourceEntry data = readManifestData(getBundledManifestFile()).getResourcesList().stream()
                .filter(entry -> entry.getUrl().equals("/extra/data.txt")).findFirst().orElseThrow();
        assertTrue((data.getFlags() & Manifest.ResourceEntryFlag.COMPRESSED_VALUE) != 0);
    }

    // Output declarations retain all packages without reading inputs for unselected compression variants.
    @Test
    public void testSelectivePackageCompression() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/data.txt", "Selected content.\n".repeat(1000));
        createFile(contentRoot, "extra/unselected.txt", "Unselected content.\n".repeat(1000));
        var manifest = LiveUpdateManifest.ManifestDesc.newBuilder().setOutputDirectory("packages")
                .addFileCompressionPresets(LiveUpdateManifest.FileCompressionPreset.newBuilder().setName("none")
                        .setFileCompression(LiveUpdateManifest.FileCompression.NONE))
                .addFileCompressionPresets(LiveUpdateManifest.FileCompressionPreset.newBuilder().setName("hc12").setLz4Level(12))
                .setBaseFileCompressionPreset("none")
                .addPackages(livePackage("selected", "zip", addPath("Data", "/extra/data[.]txt"))
                        .toBuilder().setFileCompressionPreset("hc12"));
        for (String provider : List.of("zip", "folder", "test-package")) {
            manifest.addContainerPresets(LiveUpdateManifest.ContainerPreset.newBuilder().setName(provider).setProvider(provider));
            manifest.addPackages(livePackage("unselected-" + provider, provider, addPath("Data", "/extra/unselected[.]txt"))
                    .toBuilder().setFileCompressionPreset("hc12"));
        }
        Files.writeString(Path.of(contentRoot, "content.archivemanifest"), TextFormat.printer().printToString(manifest.build()));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "selected");
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                assertTrue(result.toString(), result.isOk());
            }
            ManifestBuilder manifests = new ManifestBuilder();
            manifests.setProjectIdentifier("Selective compression");
            manifests.setResourceHashAlgorithm(Manifest.HashAlgorithm.HASH_SHA1);
            manifests.setResourceGraph(new ResourceGraph(project));
            try (PackageWriter writer = new PackageWriter(project, project.getLiveUpdatePlan(), manifest.getFileCompressionPresets(0))) {
                for (ArchiveEntry entry : project.getArchiveBuilder().getExcludedEntries()) {
                    byte[] payload = project.getArchiveBuilder().prepareResource(entry);
                    writer.accept(entry, payload);
                    manifests.addResourceEntry(entry.getRelativeFilename(), payload, entry.getSize(), entry.getCompressedSize(),
                            Manifest.ResourceEntryFlag.EXCLUDED_VALUE);
                }
                // Base payloads are already retained. Only a package-specific preparation would reread this file.
                Files.delete(Path.of(contentRoot, "build/extra/unselected.txt"));
                var written = writer.write(manifests, List.of(), Set.of("selected"));
                assertEquals(Set.of("selected"), written.packages().keySet());
                assertEquals(Set.of("selected"), written.storedSizes().keySet());
                assertEquals(Set.of("/extra/data.txt"), written.storedSizes().get("selected").keySet());
            }
        }
        Manifest.ManifestData selected = zipManifest("selected");
        assertEquals(Set.of("/extra/data.txt"), manifestPaths(selected));
        assertEquals(Set.of("selected", "unselected-zip", "unselected-folder", "unselected-test-package"),
                selected.getPackageMetadata().getOutputsList().stream().map(Manifest.PackageOutput::getPackage).collect(Collectors.toSet()));
        Manifest.ResourceEntry entry = selected.getResources(0);
        assertTrue((entry.getFlags() & Manifest.ResourceEntryFlag.COMPRESSED_VALUE) != 0);
        try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/selected.zip").toFile())) {
            String hash = ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
            assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
        }
        try (var files = Files.list(Path.of(contentRoot, "packages"))) {
            assertEquals(List.of("selected.zip"), files.map(path -> path.getFileName().toString()).toList());
        }
        assertNoPackageStaging();
    }

    // Low and high LZ4 levels produce distinct decodable payloads for overlapping packages.
    @Test
    public void testLz4FileCompressionPresetLevels() throws Exception {
        createPackageProject();
        java.util.Random random = new java.util.Random(42);
        StringBuilder data = new StringBuilder();
        for (int i = 0; i < 20000; ++i) {
            if (i > 100 && random.nextBoolean()) {
                int offset = random.nextInt(data.length() - 50);
                data.append(data.substring(offset, offset + 50));
            } else {
                data.append((char)('a' + random.nextInt(26)));
            }
        }
        createFile(contentRoot, "extra/data.txt", data.toString());
        var manifest = LiveUpdateManifest.ManifestDesc.newBuilder().setOutputDirectory("packages");
        for (int level : List.of(1, 12)) {
            String name = "level" + level;
            manifest.addFileCompressionPresets(LiveUpdateManifest.FileCompressionPreset.newBuilder().setName(name).setLz4Level(level));
            manifest.addPackages(livePackage(name, "zip", addPath("Data", "/extra/data[.]txt"))
                    .toBuilder().setFileCompressionPreset(name));
        }
        Files.writeString(Path.of(contentRoot, "content.archivemanifest"), TextFormat.printer().printToString(manifest.build()));
        buildPackageProject(true, false);
        List<Integer> sizes = new ArrayList<>();
        for (int level : List.of(1, 12)) {
            Manifest.ResourceEntry entry = zipManifest("level" + level).getResources(0);
            sizes.add(entry.getCompressedSize());
            String hash = com.dynamo.bob.archive.ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
            try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/level" + level + ".zip").toFile())) {
                assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
            }
        }
        assertTrue("Higher LZ4 level must affect the emitted payload", sizes.get(1) < sizes.get(0));
        for (int level : List.of(1, 12)) {
            Files.writeString(Path.of(contentRoot, "content.archivemanifest"),
                    TextFormat.printer().printToString(manifest.setBaseFileCompressionPreset("level" + level).build()));
            buildPackageProject(false, false, "bundle");
            Manifest.ResourceEntry base = readManifestData(getBundledManifestFile()).getResourcesList().stream()
                    .filter(entry -> entry.getUrl().equals("/extra/data.txt")).findFirst().orElseThrow();
            assertEquals(zipManifest("level" + level).getResources(0).getHash(), base.getHash());
        }
    }

    private void assertPackagePayload(Manifest.ResourceEntry entry, byte[] payload) throws Exception {
        assertEquals(entry.getSize(), java.nio.ByteBuffer.wrap(payload).getInt());
        boolean compressed = (entry.getFlags() & Manifest.ResourceEntryFlag.COMPRESSED_VALUE) != 0;
        boolean encrypted = (entry.getFlags() & Manifest.ResourceEntryFlag.ENCRYPTED_VALUE) != 0;
        assertEquals(ArchiveEntry.FLAG_LIVEUPDATE | (compressed ? ArchiveEntry.FLAG_COMPRESSED : 0)
                | (encrypted ? ArchiveEntry.FLAG_ENCRYPTED : 0), payload[4]);
        byte[] bytes = Arrays.copyOfRange(payload, 16, payload.length);
        assertArrayEquals(entry.getHash().getData().toByteArray(), java.security.MessageDigest.getInstance("SHA-1").digest(bytes));
        assertEquals(compressed ? entry.getCompressedSize() : entry.getSize(), bytes.length);
        if (encrypted) {
            bytes = com.dynamo.crypt.Crypt.decryptCTR(bytes, "aQj8CScgNP4VsfXK".getBytes(java.nio.charset.StandardCharsets.UTF_8));
        }
        if (compressed) {
            var decoder = com.sun.jna.Native.load(Bob.getSharedLib("dlib_shared").getAbsolutePath(),
                    com.dynamo.bob.archive.test.ArchiveTest.LZ4Decoder.class);
            byte[] decoded = new byte[entry.getSize()];
            int[] size = new int[1];
            assertEquals(0, decoder.LZ4DecompressBuffer(bytes, bytes.length, decoded, decoded.length, size));
            assertEquals(decoded.length, size[0]);
            bytes = decoded;
        }
        assertArrayEquals(Files.readAllBytes(Path.of(contentRoot, "build", entry.getUrl().substring(1))), bytes);
    }

    // An unselected legacy settings file does not affect manifest providers or their output directory.
    @Test
    public void testManifestSettingsDoNotUseLegacyPublisher() throws Exception {
        createPackageProject();
        Files.writeString(Path.of(contentRoot, "liveupdate.settings"),
                "save-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n", java.nio.file.StandardOpenOption.APPEND);
        savePackages(livePackage("episode", "zip", addPath("Data", "/extra/.*")),
                livePackage("copy", "folder", addPath("Data", "/extra/.*")),
                livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        buildPackageProject(true, false);
        try (Project project = new ArchiveProject(contentRoot, "build")) {
            project.loadProjectFile();
            File bundle = new File(contentRoot, "bundle");
            BundleHelper.moveBundleIfNeed(project, bundle);
            assertFalse(new File(bundle, "episode.zip").exists());
            assertFalse(new File(bundle, "copy").exists());
            assertFalse(new File(bundle, "custom.custom").exists());
            assertTrue(new File(contentRoot, "packages/episode.zip").isFile());
            assertTrue(new File(contentRoot, "packages/copy/liveupdate.game.dmanifest").isFile());
            assertTrue(new File(contentRoot, "packages/custom.custom").isFile());
            assertEquals("{\"package\":\"custom\"}", Files.readString(Path.of(contentRoot, "packages/metadata/custom.json")));
            try (var records = Files.list(Path.of(contentRoot, ".internal/liveupdate"))) {
                assertFalse(records.anyMatch(path -> path.getFileName().toString().startsWith("bundle-")));
            }
        }
        createFile(contentRoot, "liveupdate.settings", "Not an INI file");
        buildPackageProject(true, false);
    }

    // Both explicit settings fail before build or bundle, even with export disabled; the inherited legacy default is not a conflict.
    @Test
    public void testArchiveAndLegacySettingsConflict() throws Exception {
        createPackageProject();
        Path gameProject = Path.of(contentRoot, "game.project");
        String manifestSettings = Files.readString(gameProject);
        Files.writeString(gameProject, manifestSettings + "\n[liveupdate]\nsettings = /liveupdate.settings\n");
        for (String command : List.of("build", "bundle")) {
            for (boolean export : List.of(false, true)) {
                try (Project project = packageReportProject(export)) {
                    try {
                        project.build(Progress.discarding(), command);
                        org.junit.Assert.fail("Both settings must be rejected by " + command);
                    } catch (CompileExceptionError expected) {
                        assertEquals(project.getGameProjectResource(), expected.getResource());
                        assertTrue(expected.getMessage(), expected.getMessage().contains("archive.manifest and liveupdate.settings cannot both be set"));
                    }
                }
            }
        }
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.graph.json")));
        assertFalse(Files.exists(Path.of(contentRoot, "packages")));

        Files.writeString(gameProject, manifestSettings.replace("settings = /liveupdate.settings\n", "").replace("settings =\n", ""));
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        assertTrue(Files.exists(Path.of(contentRoot, "build/game.graph.json")));
    }

    // A missing archive.manifest fails instead of silently falling back to an existing legacy settings file.
    @Test
    public void testMissingArchiveManifestDoesNotUseLegacySettings() throws Exception {
        createExcludedLiveUpdateProject("settings =\n\n[archive]\nmanifest = /missing.archivemanifest\n");
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("A configured archive manifest must exist");
        } catch (CompileExceptionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("manifest does not exist"));
        }
        assertFalse(Files.exists(Path.of(contentRoot, "build/game.graph.json")));
    }

    // Package archives must retain the SSL certificate copy performed by the legacy archive path.
    @Test
    public void testPackageSslCertificates() throws Exception {
        createPackageProject();
        File settings = new File(contentRoot, "game.project");
        Files.writeString(settings.toPath(), Files.readString(settings.toPath()) + "\n[network]\nssl_certificates = certs.pem\n");
        createFile(contentRoot, "certs.pem", "certificate");
        try (Project project = packageReportProject(true)) {
            for (TaskResult result : project.build(Progress.discarding(), "build")) assertTrue(result.toString(), result.isOk());
        }
        Files.delete(Path.of(contentRoot, "certs.pem"));
        try (Project project = packageReportProject(true)) {
            project.build(Progress.discarding(), "bundle");
        }
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

    // Environment-only changes relocate all providers on the next build and retire the previous artifacts.
    @Test
    public void testPackageOutputDirectoryEnvironmentChange() throws Exception {
        createPackageProject();
        setPackageOutputDirectory("${LU_TEST_OUTPUT}/nested");
        Path root = Path.of(contentRoot);
        Path previous = null;
        for (String directory : List.of("first", root.resolve("second").toString())) {
            try (Project project = new ArchiveProject(contentRoot, "build") {
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
                for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) assertTrue(result.toString(), result.isOk());
                Path output = root.resolve(directory).resolve("nested");
                assertTrue(Files.isRegularFile(output.resolve("episode.zip")));
                assertTrue(Files.isRegularFile(output.resolve("copy/liveupdate.game.dmanifest")));
                assertEquals(Set.of(output.resolve("episode.zip"), output.resolve("copy")),
                        PackageArtifacts.readOutputs(project).stream().map(PackageArtifacts.Output::path).collect(Collectors.toSet()));
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

    // Package rebuilds include inventory, rule, and dependency edits without a clean build.
    @Test
    public void testPackageRebuildIncludesEdits() throws Exception {
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

    // Every export rewrites ZIP/folder packages from scratch while leaving unchanged compiler outputs cached.
    @Test
    public void testPackageAlwaysRebuildsArtifacts() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        Path folder = Path.of(contentRoot, "packages/copy");
        Path folderManifest = folder.resolve("liveupdate.game.dmanifest");
        Path compiled = Path.of(contentRoot, "build/logic/level.scriptc");
        byte[] expectedZip = Files.readAllBytes(zip);
        byte[] expectedManifest = Files.readAllBytes(folderManifest);
        FileTime previous = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(zip, previous);
        Files.setLastModifiedTime(folderManifest, previous);
        Files.setLastModifiedTime(compiled, previous);
        Files.writeString(folder.resolve("obsolete"), "old package entry");
        buildPackageProject(true, false);
        assertFalse(previous.equals(Files.getLastModifiedTime(zip)));
        assertFalse(previous.equals(Files.getLastModifiedTime(folderManifest)));
        assertEquals(previous, Files.getLastModifiedTime(compiled));
        assertArrayEquals(expectedZip, Files.readAllBytes(zip));
        assertArrayEquals(expectedManifest, Files.readAllBytes(folderManifest));
        assertFalse(Files.exists(folder.resolve("obsolete")));
        assertNoPackageStaging();
    }

    // Rebuilding restores damaged ZIPs and missing folder payloads without relying on cached artifact state.
    @Test
    public void testPackageRebuildRepairsArtifacts() throws Exception {
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
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));

        Files.writeString(zip, "corrupt");
        Files.setLastModifiedTime(zip, unchanged);
        buildPackageProject(true, false);
        assertTrue(hasResource(zipManifest("episode"), "/extra/data.json"));
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        Files.setLastModifiedTime(zip, unchanged);
        buildPackageProject(true, false);
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));

        try (Project project = new ArchiveProject(contentRoot, "build")) {
            var outputs = PackageArtifacts.readOutputs(project);
            assertEquals(2, outputs.size());
            for (var output : outputs) {
                assertFalse(output.legacy());
            }
        }
        assertNoPackageStaging();
    }

    // Custom providers run on every export, even when the manifest and resource contents have not changed.
    @Test
    public void testCustomPackageAlwaysRebuilds() throws Exception {
        createPackageProject();
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        PackageProviderFixture.writes = 0;
        for (int build = 1; build <= 3; ++build) {
            buildPackageProject(true, false);
            assertEquals(build, PackageProviderFixture.writes);
        }
        assertNoPackageStaging();
    }

    // ZIP presets apply at bundle time, independently of legacy settings and without recompiling resources.
    @Test
    public void testManifestZipCompressionPresets() throws Exception {
        createPackageProject();
        Path legacy = Path.of(contentRoot, "liveupdate.settings");
        Files.writeString(legacy, "zip-compression-level=0\n", java.nio.file.StandardOpenOption.APPEND);
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "packages/episode.zip");
        Path folderManifest = Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest");
        long defaultSize = Files.size(zip);
        byte[] defaultZip = Files.readAllBytes(zip);
        Manifest.ManifestData defaultManifest = zipManifest("episode");
        byte[] defaultFolderManifest = Files.readAllBytes(folderManifest);
        Files.writeString(legacy, Files.readString(legacy).replace("zip-compression-level=0", "zip-compression-level=3"));
        buildPackageProject(true, false);
        assertArrayEquals(defaultZip, Files.readAllBytes(zip));
        assertArrayEquals(defaultFolderManifest, Files.readAllBytes(folderManifest));

        byte[] graph = Files.readAllBytes(Path.of(contentRoot, "build/game.graph.json"));
        setContainerPreset("episode", LiveUpdateManifest.ContainerPreset.newBuilder().setName("zip").setZipLevel(0).build());
        buildPackageProject(true, false, "bundle");
        long uncompressedSize = Files.size(zip);
        assertTrue(defaultSize < uncompressedSize);
        assertEquals(defaultManifest, zipManifest("episode"));
        setContainerPreset("episode", LiveUpdateManifest.ContainerPreset.newBuilder().setName("zip").setZipLevel(9).build());
        buildPackageProject(true, false, "bundle");
        assertArrayEquals(graph, Files.readAllBytes(Path.of(contentRoot, "build/game.graph.json")));
        assertTrue(Files.size(zip) < uncompressedSize);
        assertEquals(defaultManifest, zipManifest("episode"));
        assertArrayEquals(defaultFolderManifest, Files.readAllBytes(folderManifest));
    }

    // The legacy adapter copies compression settings explicitly before using the package writer.
    @Test
    public void testLegacyZipCompressionSettings() throws Exception {
        createLegacyPackageProject();
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Zip\nzip-filepath=legacy\n"
                + "zip-filename=episode.zip\nzip-compression-level=0\n");
        buildPackageProject(true, false);
        Path zip = Path.of(contentRoot, "legacy/episode.zip");
        long uncompressedSize = Files.size(zip);
        Path settings = Path.of(contentRoot, "liveupdate.settings");
        Files.writeString(settings, Files.readString(settings).replace("zip-compression-level=0", "zip-compression-level=9"));
        buildPackageProject(true, false);
        assertTrue(Files.size(zip) < uncompressedSize);
        Files.writeString(settings, "[liveupdate]\nmode=Folder\noutput-directory=legacy\noutput-folder-name=folder\nzip-compression-level=unused\n");
        buildPackageProject(true, false);
        assertTrue(Files.isRegularFile(Path.of(contentRoot, "legacy/folder/liveupdate.game.dmanifest")));
    }

    // Custom settings reach discovery, previews and writers as immutable typed values on each build.
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
        assertEquals(3, PackageProviderFixture.writes);
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        var preview = PackageBuilder.buildPlan(root, "/content.archivemanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        assertEquals(3, PackageProviderFixture.writes);
        assertEquals(plan.packages(), preview.packages());
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
        for (var values : List.of(Map.of("quality", "abc"), Map.of("unknown", "3"))) {
            setProviderProperties("test-package", values);
            try {
                buildPackageProject(true, false);
                org.junit.Assert.fail("Invalid properties should fail");
            } catch (Exception | AssertionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("provider 'test-package'"));
                assertTrue(expected.getMessage(), expected.getMessage().contains(values.keySet().iterator().next()));
            }
            assertEquals(1, PackageProviderFixture.writes);
            assertArrayEquals(original, Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip")));
            assertNoPackageStaging();
        }
    }

    // Legacy bundle copies are rebuilt each time; missing or wrong-type sources fail before replacing the previous copy.
    @Test
    public void testLegacyAlwaysCopiesBundleOutputs() throws Exception {
        createLegacyPackageProject();
        for (String mode : List.of("Zip", "Folder")) {
            createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=" + mode + "\nzip-filepath=legacy\nzip-filename=episode.zip\n"
                    + "output-directory=legacy\noutput-folder-name=episode\nsave-zip-in-bundle-folder=1\nsave-folder-in-bundle-folder=1\n");
            buildPackageProject(true, false);
            String name = mode.equals("Zip") ? "episode.zip" : "episode";
            Path source = Path.of(contentRoot, "legacy", name);
            Path bundle = Path.of(contentRoot, "bundle-" + mode);
            Path destination = bundle.resolve(name);
            Path payload = mode.equals("Zip") ? destination : destination.resolve("liveupdate.game.dmanifest");
            try (Project project = new ArchiveProject(contentRoot, "build")) {
                project.setOption("liveupdate", "true");
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                byte[] expected = Files.readAllBytes(payload);
                FileTime old = FileTime.fromMillis(946684800000L);
                Files.setLastModifiedTime(payload, old);
                BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                assertFalse(old.equals(Files.getLastModifiedTime(payload)));
                assertArrayEquals(expected, Files.readAllBytes(payload));
                if (mode.equals("Zip")) {
                    Files.delete(source);
                } else {
                    FileUtils.deleteDirectory(source.toFile());
                }
                try {
                    BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                    org.junit.Assert.fail("Missing artifacts must fail bundling");
                } catch (IOException e) {
                    assertTrue(e.getMessage().contains("run bundle again"));
                }
                if (mode.equals("Zip")) {
                    Files.createDirectory(source);
                } else {
                    Files.writeString(source, "Wrong file type");
                }
                try {
                    BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                    org.junit.Assert.fail("Artifact types must match their declarations");
                } catch (IOException e) {
                    assertTrue(e.getMessage().contains("invalid file type"));
                }
                Files.delete(source);
                Files.delete(Path.of(PackageArtifacts.resource(project).getAbsPath()));
                try {
                    BundleHelper.moveBundleIfNeed(project, bundle.toFile());
                    org.junit.Assert.fail("Missing indexes must not retire completed copies");
                } catch (IOException e) {
                    assertTrue(e.getMessage().contains("index is missing"));
                }
                assertArrayEquals(expected, Files.readAllBytes(payload));
            }
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
        } catch (Exception | AssertionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("overwrite a source resource"));
        }
        assertEquals("authored content", Files.readString(authored));
        assertFalse(Files.exists(Path.of(contentRoot, "packages/copy")));
        assertNoPackageStaging();
    }

    // Editor and CLI build directories can replace the same packages without retiring each other's separate outputs.
    @Test
    public void testPackageOutputsAcrossBuildDirectories() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        byte[] zip = Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip"));
        byte[] manifest = Files.readAllBytes(Path.of(contentRoot, "packages/copy/liveupdate.game.dmanifest"));
        for (String outputDirectory : List.of("other-packages", "packages")) {
            setPackageOutputDirectory(outputDirectory);
            try (Project project = new ArchiveProject(contentRoot, "build/default_bundle")) {
                ClassLoaderScanner scanner = new ClassLoaderScanner();
                project.scan(scanner, "com.dynamo.bob");
                project.scan(scanner, "com.dynamo.bob.pipeline");
                project.setOption("archive", "true");
                project.setOption("variant", "debug");
                project.setOption("liveupdate", "true");
                for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                    assertTrue(result.toString(), result.isOk());
                }
            }
            assertArrayEquals(zip, Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip")));
            assertArrayEquals(zip, Files.readAllBytes(Path.of(contentRoot, outputDirectory, "episode.zip")));
            assertArrayEquals(manifest, Files.readAllBytes(Path.of(contentRoot, outputDirectory, "copy/liveupdate.game.dmanifest")));
        }
        assertFalse(Files.exists(Path.of(contentRoot, "other-packages/episode.zip")));
        assertFalse(Files.exists(Path.of(contentRoot, "other-packages/copy")));
        buildPackageProject(true, false);
        assertArrayEquals(zip, Files.readAllBytes(Path.of(contentRoot, "packages/episode.zip")));
        assertNoPackageStaging();
    }

    // A copied project rebases internal ownership and cannot retire the original project's outputs.
    @Test
    public void testPackageOwnershipFollowsProjectCopy() throws Exception {
        createPackageProject();
        buildPackageProject(true, false);
        String original = contentRoot;
        Path copy = Files.createTempDirectory("copied-liveupdate-project");
        try {
            FileUtils.copyDirectory(new File(original), copy.toFile());
            contentRoot = copy.toString();
            FileUtils.deleteDirectory(copy.resolve("build").toFile());
            // Retained artifacts remain generated resources, even with a broad rule.
            savePackages(livePackage("renamed", "zip", addPath("Data", "/.*[.](json|zip)")));
            var plan = buildPackageProject(true, false);
            assertEquals(Set.of("/extra/data.json"), plan.packages().get("renamed").resources());
            assertFalse(Files.exists(copy.resolve("packages/episode.zip")));
            assertTrue(Files.exists(Path.of(original, "packages/episode.zip")));
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

    // Copying an index never transfers ownership of outputs outside the original project.
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

    // Cancelling after installing a legacy ZIP restores the previous copy and leaves delivery retryable.
    @Test
    public void testLegacyBundleCancellationRollsBack() throws Exception {
        createLegacyPackageProject();
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Zip\nzip-filepath=legacy\nzip-filename=episode.zip\nsave-zip-in-bundle-folder=1\n");
        buildPackageProject(true, false);
        Path bundle = Path.of(contentRoot, "bundle");
        Path zip = bundle.resolve("episode.zip");
        for (boolean replacing : List.of(false, true)) {
            byte[] previous = replacing ? Files.readAllBytes(zip) : null;
            if (replacing) {
                createFile(contentRoot, "logic/level.script", "function init(self) self.changed = true end");
                buildPackageProject(true, false);
            }
            byte[] expected = Files.readAllBytes(Path.of(contentRoot, "legacy/episode.zip"));
            try (Project cancelled = new ArchiveProject(contentRoot, "build") {
                @Override
                public boolean isBuildCanceled() {
                    try {
                        return Files.exists(zip) && Arrays.equals(expected, Files.readAllBytes(zip));
                    } catch (IOException e) {
                        throw new java.io.UncheckedIOException(e);
                    }
                }
            }) {
                try {
                    PackageWriter.copyOutputs(cancelled, bundle);
                    org.junit.Assert.fail("Delivery must be cancelled after installing the ZIP");
                } catch (java.io.InterruptedIOException e) {
                    if (replacing) {
                        assertArrayEquals(previous, Files.readAllBytes(zip));
                    } else {
                        assertFalse(Files.exists(zip));
                    }
                }
            }
            try (Project project = new ArchiveProject(contentRoot, "build")) {
                PackageWriter.copyOutputs(project, bundle);
            }
            assertArrayEquals(expected, Files.readAllBytes(zip));
            assertNoPackageStaging();
        }
    }

    // Reusing a preview snapshot must still resolve a plan while retaining unchanged compiler outputs.
    @Test
    public void testPackagePreviewReusesCompiledResources() throws Exception {
        createPackageProject();
        FileUtils.deleteDirectory(new File(contentRoot, "builtins"));
        var first = PackageBuilder.buildPlan(Path.of(contentRoot), "/content.archivemanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        Path compiled = Path.of(contentRoot, "build/logic/level.collectionc");
        FileTime unchanged = FileTime.fromMillis(946684800000L);
        Files.setLastModifiedTime(compiled, unchanged);
        var second = PackageBuilder.buildPlan(Path.of(contentRoot), "/content.archivemanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        assertEquals(first.packages(), second.packages());
        assertEquals(unchanged, Files.getLastModifiedTime(compiled));
        createFile(contentRoot, "extra/data.json", "{\"changed\":true}");
        PackageBuilder.buildPlan(Path.of(contentRoot), "/content.archivemanifest", Map.of(), getClass().getClassLoader(), Progress.discarding());
        assertEquals("{\"changed\":true}", Files.readString(Path.of(contentRoot, "build/extra/data.json")));
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
             Project first = new ArchiveProject(contentRoot, "build") {
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
                return PackageBuilder.buildPlan(web, "/content.archivemanifest", Map.of("platform", "wasm-web"), getClass().getClassLoader(), Progress.discarding());
            });
            var nativeBuild = executor.submit(() -> {
                ready.countDown();
                assertTrue(start.await(30, TimeUnit.SECONDS));
                return PackageBuilder.buildPlan(nativeProject, "/content.archivemanifest", Map.of("platform", "arm64-macos"), getClass().getClassLoader(), Progress.discarding());
            });
            assertTrue(ready.await(30, TimeUnit.SECONDS));
            start.countDown();
            webBuild.get(60, TimeUnit.SECONDS);
            nativeBuild.get(60, TimeUnit.SECONDS);
            Path script = Path.of("build/logic/level.scriptc");
            byte[] webScript = Files.readAllBytes(web.resolve(script));
            byte[] nativeScript = Files.readAllBytes(nativeProject.resolve(script));
            assertFalse(Arrays.equals(webScript, nativeScript));
            PackageBuilder.buildPlan(web, "/content.archivemanifest", Map.of("platform", "arm64-macos"), getClass().getClassLoader(), Progress.discarding());
            assertArrayEquals(nativeScript, Files.readAllBytes(web.resolve(script)));
            PackageBuilder.buildPlan(nativeProject, "/content.archivemanifest", Map.of("platform", "wasm-web"), getClass().getClassLoader(), Progress.discarding());
            assertArrayEquals(webScript, Files.readAllBytes(nativeProject.resolve(script)));
        } finally {
            start.countDown();
            FileUtils.deleteDirectory(nativeProject.toFile());
        }
    }

    // Adding a pipeline-provider JAR after a first build refreshes discovery without restarting the JVM.
    @Test
    public void testPackageBuildDiscoversNewProvider() throws Exception {
        createPackageProject();
        Path root = Path.of(contentRoot);
        buildPackageProject(true, false);
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
            buildPackageProject(true, false);
            assertTrue(Files.isRegularFile(root.resolve("packages/episode.added")));
        } finally {
            FileUtils.deleteDirectory(classes.toFile());
        }
    }

    // Web bundles restore legacy package copies after recreating the application, without rebuilding the source packages.
    @Test
    public void testHtml5PackagePlacement() throws Exception {
        createLegacyPackageProject();
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Zip\nzip-filepath=packages\nzip-filename=episode.zip\nsave-zip-in-bundle-folder=1\n");
        buildPackageProject(true, false);
        try (Project project = new ArchiveProject(contentRoot, "build")) {
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
            Path sourceZip = Path.of(contentRoot, "packages/episode.zip");
            Files.setLastModifiedTime(sourceZip, unchanged);
            Files.writeString(app.resolve("obsolete.txt"), "stale application file");
            bundler.bundleApplication(project, Platform.WasmWeb, bundle, () -> false);
            assertEquals(unchanged, Files.getLastModifiedTime(sourceZip));
            assertArrayEquals(Files.readAllBytes(sourceZip), Files.readAllBytes(zip));
            assertFalse(Files.exists(app.resolve("obsolete.txt")));
            assertNoPackageStaging();
        }
    }

    // Base-only HTML5 bundling must restore an unselected legacy ZIP after the application directory is recreated.
    @Test
    public void testSelectiveHtml5BundlePreservesPackageCopy() throws Exception {
        createLegacyPackageProject();
        createFile(contentRoot, "liveupdate.settings", "[liveupdate]\nmode=Zip\nzip-filepath=packages\nzip-filename=episode.zip\nsave-zip-in-bundle-folder=1\n");
        Files.writeString(Path.of(contentRoot, "game.project"), "\n[project]\ntitle=Packages\n"
                + "[html5]\nhtmlfile=/template.html\ncssfile=/style.css\n", java.nio.file.StandardOpenOption.APPEND);
        createFile(contentRoot, "template.html", "<title>{{project.title}}</title>");
        createFile(contentRoot, "style.css", "");
        Path root = Path.of(contentRoot);
        try (Project project = new Project(new DefaultFileSystem(), contentRoot, "build")) {
            project.setOption("platform", Platform.WasmWeb.getPair());
            project.setOption("architectures", "wasm-web");
            project.setOption("variant", "debug");
            project.setOption("bundle-output", root.resolve("bundle").toString());
            project.setOption("all-packages", "true");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
            Path engines = Path.of(project.getBinaryOutputDirectory(), Platform.WasmWeb.getExtenderPair());
            Files.createDirectories(engines);
            for (String name : Platform.WasmWeb.formatBinaryName("dmengine")) {
                Files.write(engines.resolve(name), new byte[]{1});
            }
            project.build(Progress.discarding(), "bundle");
            Path source = root.resolve("packages/episode.zip");
            Path copy = root.resolve("bundle/Packages/episode.zip");
            byte[] expected = Files.readAllBytes(source);
            assertArrayEquals(expected, Files.readAllBytes(copy));
            FileTime unchanged = FileTime.fromMillis(946684800000L);
            Files.setLastModifiedTime(source, unchanged);
            project.setOption("all-packages", "false");
            project.setOption("package", "base");
            project.build(Progress.discarding(), "bundle");
            assertArrayEquals(expected, Files.readAllBytes(copy));
            assertArrayEquals(expected, Files.readAllBytes(source));
            assertEquals(unchanged, Files.getLastModifiedTime(source));
            assertTrue(PackageArtifacts.readOutputs(project).isEmpty());
        }
        assertNoPackageStaging();
    }

    private Project html5PackageProject() throws IOException {
        createPackageProject();
        setPackageOutputDirectory("bundle/Packages/packages");
        Files.writeString(Path.of(contentRoot, "game.project"), "\n[project]\ntitle=Packages\n"
                + "[html5]\nhtmlfile=/template.html\ncssfile=/style.css\n", java.nio.file.StandardOpenOption.APPEND);
        createFile(contentRoot, "template.html", "<title>{{project.title}}</title>");
        createFile(contentRoot, "style.css", "");
        Project project = new Project(new DefaultFileSystem(), contentRoot, "build");
        project.setOption("platform", Platform.WasmWeb.getPair());
        project.setOption("architectures", "wasm-web");
        project.setOption("variant", "debug");
        project.setOption("bundle-output", Path.of(contentRoot, "bundle").toString());
        return project;
    }

    // Reject package destinations inside the application before writing artifacts or replacing the old application.
    @Test
    public void testHtml5BundleRejectsNestedPackageOutputs() throws Exception {
        Path root = Path.of(contentRoot);
        try (Project project = html5PackageProject()) {
            project.setOption("all-packages", "true");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
            Path engines = Path.of(project.getBinaryOutputDirectory(), Platform.WasmWeb.getExtenderPair());
            Files.createDirectories(engines);
            for (String name : Platform.WasmWeb.formatBinaryName("dmengine")) {
                Files.write(engines.resolve(name), new byte[]{1});
            }
            Path previous = root.resolve("bundle/Packages/previous.txt");
            Files.createDirectories(previous.getParent());
            Files.writeString(previous, "previous application");
            try {
                project.build(Progress.discarding(), "bundle");
                org.junit.Assert.fail("Packages inside the application directory must be rejected");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("overlaps the application bundle directory"));
            }
            assertEquals("previous application", Files.readString(previous));
            assertFalse(Files.exists(root.resolve("bundle/Packages/packages")));
            assertFalse(Files.exists(root.resolve("build/game.lupackages")));
            assertNoPackageStaging();

            // A sibling directory under the same bundle root is safe.
            setPackageOutputDirectory("bundle/liveupdate");
            project.build(Progress.discarding(), "bundle");
            assertFalse(Files.exists(previous));
            try (ZipFile zip = new ZipFile(root.resolve("bundle/liveupdate/episode.zip").toFile())) {
                Manifest.ManifestData manifest = Manifest.ManifestData.parseFrom(Manifest.ManifestFile.parseFrom(
                        zip.getInputStream(zip.getEntry("liveupdate.game.dmanifest"))).getData());
                assertEquals(project.getLiveUpdatePlan().packages().get("episode").resources(), manifestPaths(manifest));
            }
            assertEquals(project.getLiveUpdatePlan().packages().get("copy").resources(),
                    manifestPaths(readManifestData(root.resolve("bundle/liveupdate/copy/liveupdate.game.dmanifest").toFile())));
        }
        assertNoPackageStaging();
    }

    // Base-only and contentless bundles must not delete unselected packages, even after changing their output setting.
    @Test
    public void testHtml5BaseBundleRejectsExistingNestedPackages() throws Exception {
        Path root = Path.of(contentRoot);
        try (Project project = html5PackageProject()) {
            project.setOption("package", "episode,copy");
            for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                assertTrue(result.toString(), result.isOk());
            }
            Path engines = Path.of(project.getBinaryOutputDirectory(), Platform.WasmWeb.getExtenderPair());
            Files.createDirectories(engines);
            for (String name : Platform.WasmWeb.formatBinaryName("dmengine")) {
                Files.write(engines.resolve(name), new byte[]{1});
            }
            Path previous = root.resolve("bundle/Packages/previous.txt");
            Files.writeString(previous, "previous application");
            Map<Path, byte[]> expectedFiles = new LinkedHashMap<>();
            try (var files = Files.walk(previous.getParent())) {
                for (Path file : files.filter(Files::isRegularFile).toList()) {
                    expectedFiles.put(file, Files.readAllBytes(file));
                }
            }
            Path zip = root.resolve("bundle/Packages/packages/episode.zip");
            FileTime unchanged = FileTime.fromMillis(946684800000L);
            Files.setLastModifiedTime(zip, unchanged);
            setPackageOutputDirectory("bundle/liveupdate");
            project.setOption("package", "base");
            for (boolean contentless : List.of(false, true)) {
                project.setOption("exclude-archive", Boolean.toString(contentless));
                try {
                    project.build(Progress.discarding(), "bundle");
                    org.junit.Assert.fail("Existing packages inside the application directory must be protected");
                } catch (CompileExceptionError expected) {
                    assertTrue(expected.getMessage(), expected.getMessage().contains("overlaps the application bundle directory"));
                }
                for (Map.Entry<Path, byte[]> entry : expectedFiles.entrySet()) {
                    assertArrayEquals(entry.getKey().toString(), entry.getValue(), Files.readAllBytes(entry.getKey()));
                }
                assertEquals(unchanged, Files.getLastModifiedTime(zip));
                assertNoPackageStaging();
            }
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
            assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        }
        buildPackageProject(false, false);
        assertTrue(PackageArtifacts.generatedPaths(Path.of(contentRoot)).contains("/packages/episode.zip"));
        buildPackageProject(true, false);
        assertArrayEquals(expected, Files.readAllBytes(zip));
        assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        assertNoPackageStaging();
    }

    // Successful renames/removals retire owned artifacts; failed providers leave the previous delivery intact.
    @Test
    public void testPackageRetiresObsoleteArtifacts() throws Exception {
        createPackageProject();
        savePackages(livePackage("old", "zip", addPath("Data", "/.*[.](json|zip)")),
                livePackage("copy", "folder", addPath("Data", "/extra/.*")));
        buildPackageProject(true, false);
        byte[] previous = Files.readAllBytes(Path.of(contentRoot, "packages/old.zip"));
        savePackages(livePackage("new", "test-package", addPath("Data", "/.*[.](json|zip)")));
        PackageProviderFixture.fail = true;
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Provider should fail");
        } catch (Exception | AssertionError expected) {
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
        buildPackageProject(true, false, "clean", "build");
        assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest("new")));
        assertNoPackageStaging();
    }

    // Case-only renames replace owned ZIP/folder outputs on every filesystem.
    @Test
    public void testPackageCaseOnlyRename() throws Exception {
        createPackageProject();
        for (String name : List.of("episode", "Episode", "episode")) {
            savePackages(livePackage(name, "zip", addPath("Data", "/extra/.*")),
                    livePackage(name + "-folder", "folder", addPath("Data", "/extra/.*")));
            buildPackageProject(true, false);
            assertEquals(Set.of("/extra/data.json"), manifestPaths(zipManifest(name)));
            try (var files = Files.list(Path.of(contentRoot, "packages"))) {
                assertEquals(Set.of(name + ".zip", name + "-folder"),
                        files.map(path -> path.getFileName().toString()).collect(Collectors.toSet()));
            }
            FileTime unchanged = FileTime.fromMillis(946684800000L);
            Path zip = Path.of(contentRoot, "packages", name + ".zip");
            Files.setLastModifiedTime(zip, unchanged);
            buildPackageProject(true, false);
            assertFalse(unchanged.equals(Files.getLastModifiedTime(zip)));
        }
        assertNoPackageStaging();
    }

    private Project linuxPackageBundleProject() throws Exception {
        Project project = new ArchiveProject(contentRoot, "build");
        project.loadProjectFile();
        project.getProjectProperties().putStringValue("project", "title", "Application");
        project.setOption("platform", Platform.X86_64Linux.getPair());
        project.setOption("architectures", "x86_64-linux");
        project.setOption("variant", "release");
        return project;
    }

    // Engine-only native bundles must succeed without an index and ignore stale package outputs from a prior build.
    @Test
    public void testPackageNativeBundleWithoutArchive() throws Exception {
        createPackageProject();
        Path bundle = Path.of(contentRoot, "bundle");
        try (Project project = linuxPackageBundleProject()) {
            project.setOption("exclude-archive", "true");
            project.setOption("liveupdate", "true");
            for (boolean previousBuild : List.of(false, true)) {
                if (previousBuild) {
                    buildPackageProject(true, false);
                    Files.delete(Path.of(contentRoot, "packages/episode.zip"));
                } else {
                    assertFalse(PackageArtifacts.resource(project).exists());
                }
                // Content builds clear host-platform engines, so create the dummy after building.
                Path engines = Path.of(project.getBinaryOutputDirectory(), Platform.X86_64Linux.getExtenderPair());
                Files.createDirectories(engines);
                for (String name : Platform.X86_64Linux.formatBinaryName("dmengine")) Files.write(engines.resolve(name), new byte[]{1});
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
        // Switching to legacy settings restores inclusion for every factory prototype and unchecked proxy.
        Path gameProject = Path.of(contentRoot, "game.project");
        Files.writeString(gameProject, Files.readString(gameProject).replace("manifest = /content.archivemanifest\n", "manifest =\n\n[liveupdate]\nsettings = /liveupdate.settings\n"));
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
        } catch (Exception | AssertionError expected) {
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
            } catch (Exception | AssertionError expected) {
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
            } catch (Exception | AssertionError expected) {
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
            org.junit.Assert.fail("Package output should collide with the build index");
        } catch (Exception | AssertionError expected) {
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
            } catch (Exception | AssertionError expected) {
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

    // Independent model outputs survive a selective build that only uses the mesh, including later source edits.
    @Test
    public void testPackageIndependentRuntimeOutputs() throws Exception {
        createPackageProject();
        try (var input = getClass().getResourceAsStream("/com/dynamo/bob/pipeline/bend2bones.gltf")) {
            Files.copy(input, Path.of(contentRoot, "extra/model.gltf"));
        }
        createFile(contentRoot, "extra/display.model", "mesh: \"/extra/model.gltf\"\n");
        savePackages(livePackage("episode", "zip", addPath("Model", "/extra/model[.]gltf")),
                livePackage("display", "zip", addPath("Display", "/extra/display[.]model")));
        buildPackageProject(true, true, "build");
        var plan = buildPackageProject(true, false, "bundle");
        Set<String> outputs = Set.of("/extra/model.meshsetc", "/extra/model.skeletonc", "/extra/model_generated_0.animationsetc");
        assertEquals(outputs, manifestPaths(zipManifest("episode")));
        assertEquals(outputs, Set.copyOf(plan.sourceOutputs().get("/extra/model.gltf")));
        for (String output : outputs) {
            assertEquals("/extra/model.gltf", plan.primarySources().get(output));
        }
        for (boolean editSource : List.of(false, true)) {
            if (editSource) {
                Path source = Path.of(contentRoot, "extra/model.gltf");
                var json = new com.fasterxml.jackson.databind.ObjectMapper();
                var model = (com.fasterxml.jackson.databind.node.ObjectNode) json.readTree(Files.readString(source));
                model.remove("animations");
                Files.writeString(source, json.writeValueAsString(model));
            }
            try (Project project = packageReportProject(false)) {
                project.setOption("variant", "debug");
                project.setOption("package", "display");
                for (TaskResult result : project.build(Progress.discarding(), "build")) {
                    assertTrue(result.toString(), result.isOk());
                }
            }
            var updated = buildPackageProject(true, false, "bundle");
            assertEquals(outputs, Set.copyOf(updated.sourceOutputs().get("/extra/model.gltf")));
            assertEquals(outputs, manifestPaths(zipManifest("episode")));
            try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/episode.zip").toFile())) {
                assertEquals(outputs.size() + 1, zip.size());
                for (Manifest.ResourceEntry entry : zipManifest("episode").getResourcesList()) {
                    String hash = ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
                    assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
                }
            }
        }
    }

    // Newly selecting a dependency requires complete root outputs, even when the next build selects an unrelated package.
    @Test
    public void testPackageNewRootRequiresCompleteOutputs() throws Exception {
        createPackageProject();
        try (var input = getClass().getResourceAsStream("/com/dynamo/bob/pipeline/bend2bones.gltf")) {
            Files.copy(input, Path.of(contentRoot, "extra/model.gltf"));
        }
        createFile(contentRoot, "extra/display.model", "mesh: \"/extra/model.gltf\"\nskeleton: \"/extra/model.gltf\"\n");
        var display = livePackage("display", "zip", addPath("Display", "/extra/display[.]model"));
        var data = livePackage("data", "zip", addPath("Data", "/extra/data[.]json"));
        savePackages(display, data);
        var initial = buildPackageProject(true, true, "build");
        assertEquals(List.of("/extra/model.skeletonc"), initial.sourceOutputs().get("/extra/model.gltf"));

        savePackages(display, data, livePackage("model", "zip", addPath("Model", "/extra/model[.]gltf")));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "model");
            try {
                project.build(Progress.discarding(), "bundle");
                org.junit.Assert.fail("Dependency mappings must not silently omit a new package root's outputs");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("/extra/model.gltf"));
                assertTrue(expected.getMessage(), expected.getMessage().contains("run build before bundle"));
            }
        }
        assertFalse(Files.exists(Path.of(contentRoot, "packages/model.zip")));

        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "data");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "model");
            project.build(Progress.discarding(), "bundle");
        }
        Set<String> outputs = Set.of("/extra/model.meshsetc", "/extra/model.skeletonc", "/extra/model_generated_0.animationsetc");
        assertEquals(outputs, manifestPaths(zipManifest("model")));
        try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/model.zip").toFile())) {
            assertEquals(outputs.size() + 1, zip.size());
            for (Manifest.ResourceEntry entry : zipManifest("model").getResourcesList()) {
                String hash = ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
                assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
            }
        }
    }

    // Embedded outputs retain their authored source across bundling and replace obsolete generated mappings after edits.
    @Test
    public void testPackageEmbeddedOutputSources() throws Exception {
        createPackageProject();
        String collection = "name: \"embedded\"\nembedded_instances { id: \"object\" data: \"components { id: \\\"script\\\" component: \\\"/logic/level.script\\\" }\" }\n";
        createFile(contentRoot, "extra/embedded.collection", collection);
        savePackages(livePackage("episode", "zip", addPath("Collection", "/extra/embedded[.]collection")));
        buildPackageProject(true, true, "build");
        PackageResolver.Plan plan;
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            project.build(Progress.discarding(), "bundle");
            plan = project.getLiveUpdatePlan();
        }
        String generated = plan.packages().get("episode").resources().stream()
                .filter(path -> path.startsWith("/_generated") && path.endsWith(".goc")).findFirst().orElseThrow();
        assertEquals("/extra/embedded.collection", plan.primarySources().get(generated));
        assertEquals(Set.of("/extra/embedded.collectionc", generated), Set.copyOf(plan.sourceOutputs().get("/extra/embedded.collection")));
        Map<String, String> reportedSources = new LinkedHashMap<>();
        for (var row : packageReport().get("package_resources")) {
            reportedSources.put(row.get("file").asText(), row.get("source").asText());
        }
        assertEquals("/extra/embedded.collection", reportedSources.get(generated));

        createFile(contentRoot, "extra/embedded.collection", collection.replace("id: \\\"script\\\"", "id: \\\"changed\\\""));
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "episode");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        var updated = buildPackageProject(true, false, "bundle");
        assertFalse(updated.resources().containsKey(generated));
        assertFalse(updated.primarySources().containsKey(generated));
        List<String> outputs = updated.sourceOutputs().get("/extra/embedded.collection");
        assertEquals(2, outputs.size());
        assertFalse(outputs.contains(generated));
        for (String output : outputs) {
            assertEquals("/extra/embedded.collection", updated.primarySources().get(output));
        }
    }

    // Shared embedded outputs retain their payload and source attribution when either producer is rebuilt or edited.
    @Test
    public void testSelectiveBuildSharesGeneratedOutputs() throws Exception {
        assertSelectiveBuildSharesGeneratedOutputs(false);
    }

    // Minification must not make shared generated outputs look like conflicting outputs from another producer.
    @Test
    public void testSelectiveBuildSharesMinifiedGeneratedOutputs() throws Exception {
        boolean oldMinification = ResourceUtil.isMinificationEnabled();
        try {
            assertSelectiveBuildSharesGeneratedOutputs(true);
        } finally {
            ResourceUtil.enableMinification(oldMinification);
        }
    }

    private void assertSelectiveBuildSharesGeneratedOutputs(boolean minify) throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/shared.script", "function init(self) end\n");
        for (String name : List.of("first", "second")) {
            createFile(contentRoot, "extra/" + name + ".collection", "name: \"" + name + "\"\n"
                    + "embedded_instances { id: \"object\" data: \"components { id: \\\"script\\\" component: \\\"/extra/shared.script\\\" }\" }\n");
        }
        savePackages(livePackage("first", "zip", addPath("First", "/extra/first[.]collection")),
                livePackage("second", "zip", addPath("Second", "/extra/second[.]collection")));
        Map<String, String> options = minify
                ? Map.of("variant", "debug", "experimental-path-minification", "true")
                : Map.of("variant", "debug");
        PackageResolver.Plan initial;
        try (Project project = packageReportProject(true)) {
            options.forEach(project::setOption);
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
            initial = project.getLiveUpdatePlan();
        }
        String generated = initial.packages().get("first").resources().stream()
                .filter(path -> path.endsWith(".goc")).findFirst().orElseThrow();
        assertEquals(!minify, generated.contains("_generated_"));
        assertTrue(initial.packages().get("second").resources().contains(generated));
        Map<String, String> collections = new LinkedHashMap<>();
        for (String name : List.of("first", "second")) {
            collections.put(name, initial.packages().get(name).resources().stream()
                    .filter(path -> path.endsWith(".collectionc")).findFirst().orElseThrow());
        }
        Path compiled = Path.of(contentRoot, "build", generated.substring(1));
        byte[] original = Files.readAllBytes(compiled);
        PackageResolver.Plan plan = initial;
        for (String name : List.of("first", "second", "first")) {
            try (Project project = packageReportProject(false)) {
                options.forEach(project::setOption);
                project.setOption("package", name);
                for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                    assertTrue(result.toString(), result.isOk());
                }
                plan = project.getLiveUpdatePlan();
            }
            assertArrayEquals(original, Files.readAllBytes(compiled));
            assertEquals(Set.of(collections.get(name), generated, "/extra/shared.scriptc"),
                    manifestPaths(zipManifest(name)));
            try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/" + name + ".zip").toFile())) {
                assertEquals(4, zip.size());
                for (Manifest.ResourceEntry entry : zipManifest(name).getResourcesList()) {
                    String hash = ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
                    assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
                }
            }
        }

        String source = plan.primarySources().get(generated);
        String editedPackage = source.equals("/extra/first.collection") ? "first" : "second";
        String remainingPackage = editedPackage.equals("first") ? "second" : "first";
        String remainingSource = "/extra/" + remainingPackage + ".collection";
        Path collection = Path.of(contentRoot, source.substring(1));
        Files.writeString(collection, Files.readString(collection).replace("id: \\\"script\\\"", "id: \\\"changed\\\""));
        try (Project project = packageReportProject(false)) {
            options.forEach(project::setOption);
            project.setOption("package", editedPackage);
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
        }
        try (Project project = packageReportProject(false)) {
            options.forEach(project::setOption);
            project.setOption("package", remainingPackage);
            project.build(Progress.discarding(), "bundle");
            plan = project.getLiveUpdatePlan();
        }
        assertEquals(remainingSource, plan.primarySources().get(generated));
        assertEquals(Set.of(collections.get(remainingPackage), generated), Set.copyOf(plan.sourceOutputs().get(remainingSource)));
        assertFalse(plan.sourceOutputs().get(source).contains(generated));
        assertArrayEquals(original, Files.readAllBytes(compiled));
        assertEquals(Set.of(collections.get(remainingPackage), generated, "/extra/shared.scriptc"),
                manifestPaths(zipManifest(remainingPackage)));
        Map<String, String> reportedSources = new LinkedHashMap<>();
        for (var row : packageReport().get("package_resources")) {
            reportedSources.put(row.get("file").asText(), row.get("source").asText());
        }
        assertEquals(remainingSource, reportedSources.get(generated));
    }

    // Folder and selective image builds retain the atlas producer for shared outputs, including after atlas edits.
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

        savePackages(livePackage("episode", "zip", addPath("Folder", "/extra/.*")),
                livePackage("image", "zip", addPath("Image", "/extra/art[.]png")));
        Path texture = Path.of(contentRoot, "build/extra/art.texturec");
        Path textureSet = Path.of(contentRoot, "build/extra/art.a.texturesetc");
        byte[] originalTexture = Files.readAllBytes(texture);
        byte[] originalTextureSet = Files.readAllBytes(textureSet);
        for (boolean editAtlas : List.of(false, true)) {
            if (editAtlas) {
                createFile(contentRoot, "extra/art.atlas", "images { image: \"/extra/art.png\" }\nextrude_borders: 4\n");
            }
            try (Project project = packageReportProject(false)) {
                project.setOption("variant", "debug");
                project.setOption("package", "image");
                for (TaskResult result : project.build(Progress.discarding(), "build", "bundle")) {
                    assertTrue(result.toString(), result.isOk());
                }
                var updated = project.getLiveUpdatePlan();
                assertEquals(plan.sourceOutputs().get("/extra/art.atlas"), updated.sourceOutputs().get("/extra/art.atlas"));
                assertEquals(updated.sourceOutputs().get("/extra/art.atlas"), updated.sourceOutputs().get("/extra/art.png"));
                assertEquals("/extra/art.atlas", updated.primarySources().get("/extra/art.texturec"));
            }
            byte[] selectedTexture = Files.readAllBytes(texture);
            byte[] selectedTextureSet = Files.readAllBytes(textureSet);
            if (!editAtlas) {
                assertArrayEquals(originalTexture, selectedTexture);
                assertArrayEquals(originalTextureSet, selectedTextureSet);
            }
            assertEquals(Set.of("/extra/art.a.texturesetc", "/extra/art.texturec"), manifestPaths(zipManifest("image")));
            try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/image.zip").toFile())) {
                assertEquals(3, zip.size());
                for (Manifest.ResourceEntry entry : zipManifest("image").getResourcesList()) {
                    String hash = ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
                    assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
                }
            }
            buildPackageProject(true, true, "build");
            assertArrayEquals(Files.readAllBytes(texture), selectedTexture);
            assertArrayEquals(Files.readAllBytes(textureSet), selectedTextureSet);
        }
        // A PNG needed independently at runtime still conflicts with the atlas's texture output.
        createFile(contentRoot, "extra/texture.script", "go.property(\"texture\", resource.texture(\"/extra/art.png\"))\n");
        try {
            buildPackageProject(true, false);
            org.junit.Assert.fail("Two runtime textures must not silently share an output");
        } catch (CompileExceptionError expected) {
            assertTrue(expected.getMessage(), expected.getMessage().contains("Conflicting output resource"));
        }
    }

    // A new dependency must not overwrite an unselected producer's output or invalidate the last successful build.
    @Test
    public void testSelectiveBuildRejectsNewOutputConflict() throws Exception {
        createPackageProject();
        Files.copy(Path.of(contentRoot, "test.png"), Path.of(contentRoot, "extra/art.png"));
        createFile(contentRoot, "extra/art.atlas", "images { image: \"/extra/art.png\" }\nextrude_borders: 4\n");
        createFile(contentRoot, "extra/texture.script", "function init(self) end\n");
        savePackages(livePackage("atlas", "zip", addPath("Atlas", "/extra/art[.]atlas")),
                livePackage("logic", "zip", addPath("Script", "/extra/texture[.]script")));
        buildPackageProject(true, true, "build");
        Map<Path, byte[]> previous = new LinkedHashMap<>();
        for (String path : List.of("extra/art.texturec", "extra/art.a.texturesetc", "extra/texture.scriptc", "game.graph.json")) {
            Path output = Path.of(contentRoot, "build", path);
            previous.put(output, Files.readAllBytes(output));
        }
        createFile(contentRoot, "extra/texture.script", "go.property(\"texture\", resource.texture(\"/extra/art.png\"))\n");
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "logic");
            try {
                project.build(Progress.discarding(), "build");
                org.junit.Assert.fail("A new dependency must not overwrite the unselected atlas texture");
            } catch (CompileExceptionError expected) {
                assertTrue(expected.getMessage(), expected.getMessage().contains("Conflicting output resource"));
                assertTrue(expected.getMessage(), expected.getMessage().contains("extra/art.texturec"));
                assertTrue(expected.getMessage(), expected.getMessage().contains("extra/art.atlas"));
                assertTrue(expected.getMessage(), expected.getMessage().contains("extra/art.png"));
            }
        }
        for (var entry : previous.entrySet()) {
            assertArrayEquals(entry.getKey().toString(), entry.getValue(), Files.readAllBytes(entry.getKey()));
        }
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "atlas");
            project.build(Progress.discarding(), "bundle");
        }
        assertEquals(Set.of("/extra/art.a.texturesetc", "/extra/art.texturec"), manifestPaths(zipManifest("atlas")));
    }

    // Removing an old producer establishes a new graph instead of trying to rebuild the deleted source.
    @Test
    public void testSelectiveBuildReplacesRemovedOutputProducer() throws Exception {
        createPackageProject();
        Files.copy(Path.of(contentRoot, "test.png"), Path.of(contentRoot, "extra/art.png"));
        createFile(contentRoot, "extra/art.atlas", "images { image: \"/extra/art.png\" }\nextrude_borders: 4\n");
        var image = livePackage("image", "zip", addPath("Image", "/extra/art[.]png"));
        savePackages(livePackage("atlas", "zip", addPath("Atlas", "/extra/art[.]atlas")), image);
        buildPackageProject(true, true, "build");
        Files.delete(Path.of(contentRoot, "extra/art.atlas"));
        savePackages(image);
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "image");
            for (TaskResult result : project.build(Progress.discarding(), "build")) {
                assertTrue(result.toString(), result.isOk());
            }
            var plan = project.getLiveUpdatePlan();
            assertEquals(List.of("/extra/art.texturec"), plan.sourceOutputs().get("/extra/art.png"));
            assertEquals("/extra/art.png", plan.primarySources().get("/extra/art.texturec"));
            assertFalse(plan.sourceOutputs().containsKey("/extra/art.atlas"));
            assertFalse(plan.resources().containsKey("/extra/art.a.texturesetc"));
        }
        try (Project project = packageReportProject(false)) {
            project.setOption("variant", "debug");
            project.setOption("package", "image");
            project.build(Progress.discarding(), "bundle");
        }
        assertEquals(Set.of("/extra/art.texturec"), manifestPaths(zipManifest("image")));
        try (ZipFile zip = new ZipFile(Path.of(contentRoot, "packages/image.zip").toFile())) {
            assertEquals(2, zip.size());
            Manifest.ResourceEntry entry = zipManifest("image").getResources(0);
            String hash = ManifestBuilder.CryptographicOperations.hexdigest(entry.getHash().getData().toByteArray());
            assertPackagePayload(entry, zip.getInputStream(zip.getEntry(hash)).readAllBytes());
        }
    }

    // Broad rules export custom files but omit compiler-only texture profiles and shader includes, also without export.
    @Test
    public void testPackageSkipsBuildOnlyOutputs() throws Exception {
        createPackageProject();
        createFile(contentRoot, "extra/local.texture_profiles", "");
        createFile(contentRoot, "extra/include.glsl", "#define VALUE 1\n");
        createFile(contentRoot, "extra/object.go", "");
        createFile(contentRoot, "extra/scene.collection", "name: \"scene\"\n");
        savePackages(livePackage("episode", "zip", addPath("Folder", "/extra/.*")));
        var plan = buildPackageProject(true, false);
        assertEquals(Set.of("/extra/data.json", "/extra/object.goc", "/extra/scene.collectionc"), manifestPaths(zipManifest("episode")));
        assertEquals(List.of(), plan.sourceOutputs().get("/extra/local.texture_profiles"));
        assertEquals(List.of(), plan.sourceOutputs().get("/extra/include.glsl"));
        var disabled = buildPackageProject(false, false);
        assertTrue(disabled.base().contains("/extra/data.json"));
        assertFalse(disabled.base().contains("/extra/local.texture_profilesc"));
        assertFalse(disabled.base().contains("/extra/include.glslc"));
        assertFalse(disabled.base().contains("/extra/object.compcount_go"));
        assertFalse(disabled.base().contains("/extra/scene.compcount_col"));
    }

    // Cancelling during a provider write prevents finalization and removes staging without timing-dependent sleeps.
    @Test
    public void testPackageCancellationDuringWriting() throws Exception {
        createPackageProject();
        savePackages(livePackage("custom", "test-package", addPath("Data", "/extra/.*")));
        var canceled = new java.util.concurrent.atomic.AtomicBoolean();
        PackageProviderFixture.beforeWrite = () -> canceled.set(true);
        try (Project project = new ArchiveProject(contentRoot, "build")) {
            project.setOption("archive", "true");
            project.setOption("liveupdate", "true");
            Progress progress = new Progress(new Progress.Reporter() {
                public void report(com.dynamo.bob.IProgress.Message message, double fraction) {}
                public boolean isCanceled() { return canceled.get(); }
            });
            try {
                project.build(progress, "build", "bundle");
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
