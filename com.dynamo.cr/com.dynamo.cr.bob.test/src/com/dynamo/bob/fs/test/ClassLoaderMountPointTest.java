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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.Collection;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.ClassLoaderResourceScanner;
import com.dynamo.bob.fs.ClassLoaderMountPoint;
import com.dynamo.bob.fs.FileSystemWalker;
import com.dynamo.bob.fs.IResource;

public class ClassLoaderMountPointTest {

    ClassLoaderMountPoint mp;

    @Before
    public void setUp() throws Exception {
        this.mp = new ClassLoaderMountPoint(null, "com/dynamo/bob/fs/test/included*", new ClassLoaderResourceScanner());
        this.mp.mount();
    }

    @After
    public void tearDown() throws Exception {
        this.mp.unmount();
    }

    @Test
    public void testResource() throws Exception {
        IResource resource = this.mp.get("com/dynamo/bob/fs/test/included_resource.txt");
        assertTrue(resource != null);
        assertEquals("Unexpected resource contents", "include", new String(resource.getContent()));
    }

    @Test
    public void testExclusion() throws Exception {
        assertFalse(mp.get("com/dynamo/bob/fs/test/excluded_resource.txt") != null);
    }

    @Test
    public void testWalker() throws Exception {
        FileSystemWalker walker = new FileSystemWalker();
        Collection<String> results = new ArrayList<String>();
        this.mp.walk(".", walker, results);
        assertTrue(results.contains("com/dynamo/bob/fs/test/included_resource.txt"));
    }
}
