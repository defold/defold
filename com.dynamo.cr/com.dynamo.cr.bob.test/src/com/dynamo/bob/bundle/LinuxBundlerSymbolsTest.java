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
import static org.junit.Assert.assertFalse;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;

import org.apache.commons.io.FileUtils;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;

public class LinuxBundlerSymbolsTest {

    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private File bundleSymbols(Platform platform, boolean withSymbols) throws IOException {
        File binaryDir = temporaryFolder.newFolder("build", platform.getExtenderPair());
        FileUtils.writeStringToFile(new File(binaryDir, "dmengine_unstripped"), "unstripped engine", StandardCharsets.UTF_8);
        File bundleDir = temporaryFolder.newFolder("bundle");
        FileUtils.writeStringToFile(new File(bundleDir, LinuxBundler.getExeFilename(platform, "game") + ".debug"), "stale symbols", StandardCharsets.UTF_8);
        try (Project project = new Project(new DefaultFileSystem(), temporaryFolder.newFolder("project").getAbsolutePath(), "build")) {
            if (withSymbols) {
                project.setOption("with-symbols", "");
            }
            LinuxBundler.copySymbols(project, platform, binaryDir, bundleDir, "game");
        }
        return bundleDir;
    }

    // Verifies --with-symbols puts the unstripped engine next to the app folder (#11999: Linux bundles had no symbols).
    @Test
    public void testWithSymbolsCopiesUnstrippedEngine() throws IOException {
        File bundleDir = bundleSymbols(Platform.X86_64Linux, true);
        assertEquals("unstripped engine", Files.readString(new File(bundleDir, "game.x86_64.debug").toPath()));
    }

    // Verifies the arm64 symbol file is named after the arm64 executable.
    @Test
    public void testWithSymbolsUsesArm64ExeName() throws IOException {
        File bundleDir = bundleSymbols(Platform.Arm64Linux, true);
        assertEquals("unstripped engine", Files.readString(new File(bundleDir, "game.arm64.debug").toPath()));
    }

    // Verifies a symbol file left by an earlier --with-symbols bundle is removed when bundling without the flag.
    @Test
    public void testWithoutSymbolsRemovesStaleSymbols() throws IOException {
        File bundleDir = bundleSymbols(Platform.X86_64Linux, false);
        assertFalse(new File(bundleDir, "game.x86_64.debug").exists());
    }
}
