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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.IOException;

import org.junit.Test;

import com.dynamo.bob.fs.FileSystemMountPoint;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.test.util.MockFileSystem;

public class FileSystemMountPointTest {

    @Test
    public void testDisableMinifyPathPreservesMountedWrapperForChangeExt() throws IOException {
        MockFileSystem rootFileSystem = new MockFileSystem();
        rootFileSystem.setBuildDirectory("build/default");

        MockFileSystem mountedFileSystem = new MockFileSystem();
        mountedFileSystem.addFile("/main/test.lua", "".getBytes());

        rootFileSystem.addMountPoint(new FileSystemMountPoint(rootFileSystem, mountedFileSystem));

        IResource input = rootFileSystem.get("/main/test.lua");
        IResource output = input.disableMinifyPath().changeExt(".luac");

        assertEquals("build/default/main/test.luac", output.getPath());
        assertTrue(output.isOutput());
    }
}
