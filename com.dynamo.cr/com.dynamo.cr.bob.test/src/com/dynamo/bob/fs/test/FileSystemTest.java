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

package com.dynamo.bob.fs.test;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import org.apache.commons.io.FilenameUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IFileSystem.IWalker;
import com.dynamo.bob.test.TestLibrariesRule;

public class FileSystemTest {

    DefaultFileSystem fileSystem;

    @Rule
    public TestLibrariesRule testLibs = new TestLibrariesRule();

    @Before
    public void setUp() throws Exception {
        this.fileSystem = new DefaultFileSystem();
        this.fileSystem.setRootDirectory(testLibs.getServerLocation());
    }

    @After
    public void tearDown() throws Exception {
        this.fileSystem.close();
    }

    @Test
    public void testWalker() throws Exception {
        IWalker walker = new ZipWalker();
        List<String> results = new ArrayList<String>();
        this.fileSystem.walk(".", walker, results);
        assertFalse(results.isEmpty());
        assertTrue(results.contains("test_lib1.zip"));
        assertTrue(results.contains("test_lib2.zip"));
        assertTrue(results.contains("test_lib3.zip"));
        assertTrue(results.contains("test_lib4.zip"));
    }

    private static class ZipWalker extends FileSystemWalker {
        @Override
        public void handleFile(String path, Collection<String> results) {
            if (FilenameUtils.getExtension(path).equals("zip")) {
                results.add(path);
            }
        }
    }
}
