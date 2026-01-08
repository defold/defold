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

import static org.junit.Assert.assertNotNull;

import java.io.IOException;

import org.junit.Rule;
import org.junit.Test;

import com.dynamo.bob.fs.ZipMountPoint;
import com.dynamo.bob.test.TestLibrariesRule;

public class ZipMountPointMinVersionTest {

    @Rule
    public TestLibrariesRule testLibs = new TestLibrariesRule();

    @Test
    public void testMountWithOkMinVersion() throws Exception {
        ZipMountPoint mp = new ZipMountPoint(null, "server_root/test_min_ok.zip");
        mp.mount();
        try {
            // Should be able to access files under include_dirs
            assertNotNull(mp.get("min_ok/file.in"));
        } finally {
            mp.unmount();
        }
    }

    @Test(expected = IOException.class)
    public void testMountFailsWithTooHighMinVersion() throws Exception {
        ZipMountPoint mp = new ZipMountPoint(null, "server_root/test_min_fail.zip");
        try {
            mp.mount();
        } finally {
            mp.unmount();
        }
    }
}

