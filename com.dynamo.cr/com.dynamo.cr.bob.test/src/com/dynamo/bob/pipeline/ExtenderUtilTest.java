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
import java.nio.file.Files;
import java.util.Map;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.DefaultFileSystem;

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
}

