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
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

public class AndroidBundlerResourcesTest {
    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private File createCompiledResources() throws IOException {
        File source = temporaryFolder.newFolder("compiled-resources");
        Files.write(new File(source, "AndroidManifest.xml").toPath(), new byte[] {1, 2, 3});
        Files.write(new File(source, "resources.pb").toPath(), new byte[] {4, 5, 6});
        return source;
    }

    private void assertManifestAndTableCopied(File source, File base) throws IOException {
        assertArrayEquals(Files.readAllBytes(new File(source, "AndroidManifest.xml").toPath()),
                Files.readAllBytes(new File(base, "manifest/AndroidManifest.xml").toPath()));
        assertArrayEquals(Files.readAllBytes(new File(source, "resources.pb").toPath()),
                Files.readAllBytes(new File(base, "resources.pb").toPath()));
    }

    // Verifies Bob accepts an R8 archive with only a manifest and resource table after every file resource is removed.
    @Test
    public void testResourcesWithoutResDirectory() throws IOException {
        File source = createCompiledResources();
        File base = temporaryFolder.newFolder("base");

        AndroidBundler.copyCompiledResources(source, base);

        assertManifestAndTableCopied(source, base);
        assertFalse(new File(base, "res").exists());
    }

    // Verifies Bob copies the surviving resource payloads together with their matching manifest and resource table.
    @Test
    public void testResourcesWithSurvivingFiles() throws IOException {
        File source = createCompiledResources();
        File raw = new File(source, "res/raw");
        assertTrue(raw.mkdirs());
        byte[] payload = "retained dynamic resource".getBytes(StandardCharsets.UTF_8);
        Files.write(new File(raw, "dynamic.txt").toPath(), payload);
        File base = temporaryFolder.newFolder("base");

        AndroidBundler.copyCompiledResources(source, base);

        assertManifestAndTableCopied(source, base);
        assertArrayEquals(payload, Files.readAllBytes(new File(base, "res/raw/dynamic.txt").toPath()));
    }
}
