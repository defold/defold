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

package com.dynamo.bob.bundle;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.ZipMountPoint;

public class AndroidBundlerVkQualityTest {
    private final DefaultFileSystem fileSystem = new DefaultFileSystem();

    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private Project createProject(String androidSettings) throws Exception {
        File root = temporaryFolder.newFolder("project");
        Project project = new Project(fileSystem, root.getAbsolutePath(), "build/default");
        project.getGameProjectResource().setContent(("[android]\n" + androidSettings).getBytes(StandardCharsets.UTF_8));
        project.loadProjectFile();
        project.mount(new ClassLoaderResourceScanner());
        return project;
    }

    // Projects without an override must bundle the default database from builtins unchanged.
    @Test
    public void testDefaultDatabase() throws Exception {
        try (Project project = createProject("");
             InputStream database = Project.class.getResourceAsStream("/builtins/manifests/android/vkqualitydata.vkq")) {
            assertNotNull(database);
            byte[] expected = database.readAllBytes();
            assertTrue(expected.length > 0);
            File assets = temporaryFolder.newFolder("assets");

            AndroidBundler.copyVkQualityDataFile(project, assets, () -> false);

            assertArrayEquals(expected, Files.readAllBytes(new File(assets, "vkqualitydata.vkq").toPath()));
        }
    }

    // A custom database must replace the default byte-for-byte under the filename expected by the engine.
    @Test
    public void testCustomDatabase() throws Exception {
        try (Project project = createProject("vkquality_database = /android/custom.vkq\n")) {
            byte[] custom = new byte[] {0, 1, 2, (byte) 0x80, (byte) 0xff};
            project.getResource("/android/custom.vkq").setContent(custom);
            File assets = temporaryFolder.newFolder("assets");
            Files.write(new File(assets, "vkqualitydata.vkq").toPath(), new byte[] {42});

            AndroidBundler.copyVkQualityDataFile(project, assets, () -> false);

            assertArrayEquals(custom, Files.readAllBytes(new File(assets, "vkqualitydata.vkq").toPath()));
            assertFalse(new File(assets, "custom.vkq").exists());
        }
    }

    // Database overrides must also work with mounted library resources, not just files in the project directory.
    @Test
    public void testDatabaseFromLibrary() throws Exception {
        byte[] custom = new byte[] {3, 2, 1, 0};
        File library = temporaryFolder.newFile("library.zip");
        try (ZipOutputStream zip = new ZipOutputStream(Files.newOutputStream(library.toPath()))) {
            zip.putNextEntry(new ZipEntry("library/custom.vkq"));
            zip.write(custom);
            zip.closeEntry();
        }
        try (Project project = createProject("vkquality_database = /library/custom.vkq\n")) {
            fileSystem.addMountPoint(new ZipMountPoint(fileSystem, library.getAbsolutePath()));
            File assets = temporaryFolder.newFolder("assets");

            AndroidBundler.copyVkQualityDataFile(project, assets, () -> false);

            assertArrayEquals(custom, Files.readAllBytes(new File(assets, "vkqualitydata.vkq").toPath()));
        }
    }

    // A missing override must report the setting and path instead of silently bundling the default database.
    @Test
    public void testMissingDatabase() throws Exception {
        try (Project project = createProject("vkquality_database = /android/missing.vkq\n")) {
            File assets = temporaryFolder.newFolder("assets");
            try {
                AndroidBundler.copyVkQualityDataFile(project, assets, () -> false);
                fail("Expected an error for the missing VkQuality database");
            } catch (IOException e) {
                assertTrue(e.getMessage().contains("android.vkquality_database"));
                assertTrue(e.getMessage().contains("android/missing.vkq"));
            }
            assertFalse(new File(assets, "vkqualitydata.vkq").exists());
        }
    }
}
