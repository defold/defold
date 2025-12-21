// Copyright 2020-2025 The Defold Foundation
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

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.FileUtils;
import org.junit.rules.ExternalResource;

public class TestLibrariesRule extends ExternalResource {

    private File serverLocation;

    private void createEntry(ZipOutputStream out, String name, byte[] data) throws IOException {
        ZipEntry ze = new ZipEntry(name);
        out.putNextEntry(ze);
        if (data != null) {
            out.write(data);
        }
        out.closeEntry();
    }

    void createLib(String root, String subdir, String name, String sha1) throws IOException {
        File file = new File(String.format("%s/test_lib%s.zip", root, name));
        ZipOutputStream out = new ZipOutputStream(new FileOutputStream(file));
        out.setComment(sha1);
        ZipEntry ze;

        if (!subdir.isEmpty()) {
            String accumulate = "";
            String[] parts = subdir.split("/");
            for (String part : parts) {
                accumulate += part + "/";
                createEntry(out, accumulate, null);
            }
        }

        createEntry(out, String.format("%sgame.project", subdir), String.format("[library]\ninclude_dirs=test_lib%s", name).getBytes());

        createEntry(out, String.format("%stest_lib%s/", subdir, name), null);
        createEntry(out, String.format("%stest_lib%s/testdir1/", subdir, name), null);
        createEntry(out, String.format("%stest_lib%s/testdir2/", subdir, name), null);
        createEntry(out, String.format("%stest_lib%s/file%s.in", subdir, name, name), String.format("file%s", name).getBytes());

        out.close();
    }

    void createTestLib(String root, String sha1) throws IOException {
        File file = new File(String.format("%s/test.zip", root));
        ZipOutputStream out = new ZipOutputStream(new FileOutputStream(file));
        out.setComment(sha1);
        ZipEntry ze;

        createEntry(out, "game.project", "[library]\ninclude_dirs=test".getBytes());
        createEntry(out, "test/", null);
        createEntry(out, "test/testdir1/", null);
        createEntry(out, "test/testdir2/", null);
        createEntry(out, "test/file.in", "testfile".getBytes());

        out.close();
    }

    void createMinVersionLib(String root, String zipNameBase, String dirName, String minVersion) throws IOException {
        File file = new File(String.format("%s/%s.zip", root, zipNameBase));
        ZipOutputStream out = new ZipOutputStream(new FileOutputStream(file));
        // Construct a simple project with include_dirs and defold_min_version
        createEntry(out, "game.project", String.format("[library]\ninclude_dirs=%s\ndefold_min_version=%s", dirName, minVersion).getBytes());
        createEntry(out, String.format("%s/", dirName), null);
        createEntry(out, String.format("%s/testdir1/", dirName), null);
        createEntry(out, String.format("%s/testdir2/", dirName), null);
        createEntry(out, String.format("%s/file.in", dirName), "testfile".getBytes());
        out.close();
    }

    @Override
    protected void before() throws Throwable {
        serverLocation = new File("server_root");
        serverLocation.mkdirs();
        createLib(serverLocation.getAbsolutePath(), "", "1", "111");
        createLib(serverLocation.getAbsolutePath(), "", "2", "222");
        createLib(serverLocation.getAbsolutePath(), "subdir/", "3", "333");
        createLib(serverLocation.getAbsolutePath(), "subdir/second/", "4", "444");
        createLib(serverLocation.getAbsolutePath(), "", "5", "555");
        createLib(serverLocation.getAbsolutePath(), "", "6", "666");
        createTestLib(serverLocation.getAbsolutePath(), "test123");
        // Libraries for defold_min_version tests
        createMinVersionLib(serverLocation.getAbsolutePath(), "test_min_ok", "min_ok", "1.0.0");
        createMinVersionLib(serverLocation.getAbsolutePath(), "test_min_fail", "min_fail", "999.0.0");
    }

    @Override
    protected void after() {
        try {
            FileUtils.deleteDirectory(serverLocation);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public String getServerLocation() {
        return serverLocation.getAbsolutePath();
    }


}
