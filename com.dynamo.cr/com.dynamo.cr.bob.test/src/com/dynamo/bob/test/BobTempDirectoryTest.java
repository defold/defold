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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.util.BobTempDirectory;

public class BobTempDirectoryTest {
    private static final String KEEP_TEMP_PROPERTY = "defold.bob.keepTemp";

    private String previousKeepTempProperty;

    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Before
    public void setUp() {
        previousKeepTempProperty = System.getProperty(KEEP_TEMP_PROPERTY);
        System.setProperty(KEEP_TEMP_PROPERTY, "false");
    }

    @After
    public void tearDown() {
        if (previousKeepTempProperty == null) {
            System.clearProperty(KEEP_TEMP_PROPERTY);
        } else {
            System.setProperty(KEEP_TEMP_PROPERTY, previousKeepTempProperty);
        }
    }

    public static class UnclosedDirectoryProcess {
        public static void main(String[] args) throws Exception {
            System.setProperty(KEEP_TEMP_PROPERTY, "false");
            BobTempDirectory directory = new BobTempDirectory();
            File tempFile = directory.createTempFile("bob-temp-directory-shutdown-test", ".tmp");

            Files.write(tempFile.toPath(), "temp".getBytes(StandardCharsets.UTF_8));
            Files.write(new File(args[0]).toPath(), directory.getRootDirectory().getAbsolutePath().getBytes(StandardCharsets.UTF_8));
        }
    }

    @Test
    public void testCloseDeletesTempDirectory() throws Exception {
        // Creates a temp file, directory, and nested file, then verifies close() removes the entire temp tree.
        File rootDirectory;
        File tempFile;
        File tempDirectory;

        try (BobTempDirectory directory = new BobTempDirectory()) {
            rootDirectory = directory.getRootDirectory();
            tempFile = directory.createTempFile("bob-temp-directory-test", ".tmp");
            tempDirectory = directory.createTempDirectory("bob-temp-directory-test-dir");
            File nestedFile = new File(tempDirectory, "nested.tmp");

            Files.write(tempFile.toPath(), "temp".getBytes(StandardCharsets.UTF_8));
            Files.write(nestedFile.toPath(), "nested".getBytes(StandardCharsets.UTF_8));

            assertTrue(rootDirectory.isDirectory());
            assertTrue(tempFile.isFile());
            assertTrue(tempDirectory.isDirectory());
            assertTrue(nestedFile.isFile());
        }

        assertFalse(rootDirectory.exists());
        assertFalse(tempFile.exists());
        assertFalse(tempDirectory.exists());
    }

    @Test
    public void testUnclosedTempDirectoryIsCleanedWhenJvmExits() throws Exception {
        // Starts a child JVM that leaves a temp directory open, then verifies process shutdown removes the root.
        File rootPathFile = temporaryFolder.newFile("unclosed-temp-directory-root.txt");
        assertTrue(rootPathFile.delete());

        String javaExecutable = new File(System.getProperty("java.home"), "bin/java").getAbsolutePath();
        Process process = new ProcessBuilder(
                javaExecutable,
                "-cp",
                System.getProperty("java.class.path"),
                UnclosedDirectoryProcess.class.getName(),
                rootPathFile.getAbsolutePath())
                .redirectErrorStream(true)
                .start();

        String processOutput = new String(process.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
        int exitCode = process.waitFor();

        assertEquals(processOutput, 0, exitCode);
        File rootDirectory = new File(new String(Files.readAllBytes(rootPathFile.toPath()), StandardCharsets.UTF_8).trim());
        assertFalse(rootDirectory.exists());
    }

    @Test
    public void testClosePreventsFurtherTempCreation() throws Exception {
        // Verifies a closed temp directory deletes its root and rejects any later temp-file creation with IOException.
        BobTempDirectory directory = new BobTempDirectory();
        directory.close();

        try {
            directory.createTempFile("bob-temp-directory-test", ".tmp");
            fail("Expected temp file creation after close to fail");
        } catch (IOException expected) {
        }

        assertFalse(directory.getRootDirectory().exists());
    }

    @Test
    public void testProjectDisposeClosesTempDirectory() throws Exception {
        // Verifies Project.dispose() closes the attached temp directory and removes temps created through the Project API.
        BobTempDirectory directory = new BobTempDirectory();
        File rootDirectory = directory.getRootDirectory();
        File projectRoot = temporaryFolder.newFolder("project");
        Project project = new Project(getClass().getClassLoader(), new DefaultFileSystem(), projectRoot.getAbsolutePath(), "build/default", directory);

        File tempFile = project.createTempFile("project-temp-directory-test", ".tmp");
        File tempDirectory = project.createTempDirectory("project-temp-directory-test-dir");

        assertTrue(rootDirectory.isDirectory());
        assertTrue(tempFile.isFile());
        assertTrue(tempDirectory.isDirectory());

        project.dispose();

        assertFalse(rootDirectory.exists());
        assertFalse(tempFile.exists());
        assertFalse(tempDirectory.exists());
    }
}
