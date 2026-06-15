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
import com.dynamo.bob.util.BobTempScope;

public class BobTempScopeTest {
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

    @Test
    public void testCloseDeletesScopeDirectory() throws Exception {
        // Creates a scoped temp file, directory, and nested file, then verifies close() removes the entire scope tree.
        File scopeRoot;
        File tempFile;
        File tempDirectory;

        try (BobTempScope scope = new BobTempScope()) {
            scopeRoot = scope.getRootDirectory();
            tempFile = scope.createTempFile("bob-temp-scope-test", ".tmp");
            tempDirectory = scope.createTempDirectory("bob-temp-scope-test-dir");
            File nestedFile = new File(tempDirectory, "nested.tmp");

            Files.write(tempFile.toPath(), "temp".getBytes(StandardCharsets.UTF_8));
            Files.write(nestedFile.toPath(), "nested".getBytes(StandardCharsets.UTF_8));

            assertTrue(scopeRoot.isDirectory());
            assertTrue(tempFile.isFile());
            assertTrue(tempDirectory.isDirectory());
            assertTrue(nestedFile.isFile());
        }

        assertFalse(scopeRoot.exists());
        assertFalse(tempFile.exists());
        assertFalse(tempDirectory.exists());
    }

    @Test
    public void testClosePreventsFurtherTempCreation() throws Exception {
        // Verifies a closed scope deletes its root and rejects any later temp-file creation with IOException.
        BobTempScope scope = new BobTempScope();
        scope.close();

        try {
            scope.createTempFile("bob-temp-scope-test", ".tmp");
            fail("Expected temp file creation after close to fail");
        } catch (IOException expected) {
        }

        assertFalse(scope.getRootDirectory().exists());
    }

    @Test
    public void testProjectDisposeClosesTempScope() throws Exception {
        // Verifies Project.dispose() closes the attached scope and removes temps created through the Project API.
        BobTempScope scope = new BobTempScope();
        File scopeRoot = scope.getRootDirectory();
        File projectRoot = temporaryFolder.newFolder("project");
        Project project = new Project(getClass().getClassLoader(), new DefaultFileSystem(), projectRoot.getAbsolutePath(), "build/default", scope);

        File tempFile = project.createTempFile("project-temp-scope-test", ".tmp");
        File tempDirectory = project.createTempDirectory("project-temp-scope-test-dir");

        assertTrue(scopeRoot.isDirectory());
        assertTrue(tempFile.isFile());
        assertTrue(tempDirectory.isDirectory());

        project.dispose();

        assertFalse(scopeRoot.exists());
        assertFalse(tempFile.exists());
        assertFalse(tempDirectory.exists());
    }
}
