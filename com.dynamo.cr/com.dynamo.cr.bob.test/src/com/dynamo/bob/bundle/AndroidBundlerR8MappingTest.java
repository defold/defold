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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.Platform;

public class AndroidBundlerR8MappingTest {

    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    @Test
    public void testExtenderMappingIsCopiedToApkSymbolsDirectory() throws IOException {
        File extenderOutput = temporaryFolder.newFolder("build");
        File architectureOutput = new File(extenderOutput, Platform.Arm64Android.getExtenderPair());
        assertTrue(architectureOutput.mkdirs());

        String mappingContents = "com.example.Game -> a:\n";
        Files.write(new File(architectureOutput, "mapping.txt").toPath(),
                mappingContents.getBytes(StandardCharsets.UTF_8));

        File symbolsDir = temporaryFolder.newFolder("game.apk.symbols");
        AndroidBundler.copyR8Mapping(extenderOutput, Platform.Arm64Android, symbolsDir);

        File copiedMapping = new File(symbolsDir, "mapping.txt");
        assertTrue(copiedMapping.isFile());
        assertEquals(symbolsDir.getCanonicalFile(), copiedMapping.getParentFile().getCanonicalFile());
        assertEquals(mappingContents,
                new String(Files.readAllBytes(copiedMapping.toPath()), StandardCharsets.UTF_8));
    }
}
