// Copyright 2020-2025 The Defold Foundation
// Licensed under the Defold License version 1.0

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

