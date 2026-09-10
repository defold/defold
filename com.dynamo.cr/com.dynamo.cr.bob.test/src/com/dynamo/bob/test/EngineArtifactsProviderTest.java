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

package com.dynamo.bob.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import com.dynamo.bob.Bob;
import com.dynamo.bob.EngineArtifactsProvider;
import com.dynamo.bob.Platform;
import com.dynamo.bob.Progress;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.fs.DefaultFileSystem;

public class EngineArtifactsProviderTest {
    @Rule
    public TemporaryFolder temporaryFolder = new TemporaryFolder();

    private File archive;
    private File cache;

    @Before
    public void setUp() throws IOException {
        archive = temporaryFolder.newFolder("archive");
        cache = temporaryFolder.newFolder("cache");
        EngineArtifactsProvider.setCacheBase(cache);
    }

    @After
    public void tearDown() {
        EngineArtifactsProvider.setCacheBase(null);
    }

    // Use a file-backed archive to exercise downloads and cache reuse without a server.
    @SuppressWarnings("unchecked")
    private List<File> downloadExes(Platform platform, String variant) throws Exception {
        Method method = EngineArtifactsProvider.class.getDeclaredMethod("downloadExes", Platform.class, String.class, String.class);
        method.setAccessible(true);
        try {
            return (List<File>) method.invoke(null, platform, variant, archive.toURI().toString());
        } catch (InvocationTargetException e) {
            if (e.getCause() instanceof Exception) {
                throw (Exception) e.getCause();
            }
            throw e;
        }
    }

    private File writeArtifact(String platformKey, String path, String content) throws IOException {
        File file = new File(archive, EngineVersion.sha1 + "/engine/" + platformKey + "/" + path);
        FileUtils.writeStringToFile(file, content, StandardCharsets.UTF_8);
        return file;
    }

    private void checkEngineDownloads(Platform platform, String artifactDirectory) throws Exception {
        String[] variants = {Bob.VARIANT_DEBUG, Bob.VARIANT_RELEASE, Bob.VARIANT_HEADLESS};
        String[] names = {"dmengine", "dmengine_release", "dmengine_headless"};
        for (int i = 0; i < variants.length; ++i) {
            List<String> filenames = platform.formatBinaryName(names[i]);
            for (String filename : filenames) {
                if (!artifactDirectory.isEmpty()) {
                    writeArtifact(platform.getPair(), filename, "unstripped symbols");
                }
                writeArtifact(platform.getPair(), artifactDirectory + filename, "engine " + filename);
            }

            List<File> downloaded = downloadExes(platform, variants[i]);
            assertEquals(filenames.size(), downloaded.size());
            for (int j = 0; j < filenames.size(); ++j) {
                assertEquals(filenames.get(j), downloaded.get(j).getName());
                assertEquals("engine " + filenames.get(j), Files.readString(downloaded.get(j).toPath()));
                Files.delete(new File(archive, EngineVersion.sha1 + "/engine/" + platform.getPair() + "/" + artifactDirectory + filenames.get(j)).toPath());
            }
            assertEquals(downloaded, downloadExes(platform, variants[i]));
        }
    }

    @Test
    public void testNativeEnginesUseStrippedArtifactsAndReuseCache() throws Exception {
        Platform[] platforms = {Platform.Arm64Linux, Platform.Arm64IosSim, Platform.X86_64Android,
                Platform.Armv7Android, Platform.Arm64Android, Platform.Arm64Ios,
                Platform.X86_64Linux, Platform.X86_64MacOS, Platform.Arm64MacOS};
        for (Platform platform : platforms) {
            checkEngineDownloads(platform, "stripped/");
        }
    }

    @Test
    public void testWebAndWindowsEnginesUseOriginalArtifactsAndReuseCache() throws Exception {
        Platform[] platforms = {Platform.WasmWeb, Platform.WasmPthreadWeb, Platform.X86Win32, Platform.X86_64Win32};
        for (Platform platform : platforms) {
            checkEngineDownloads(platform, "");
        }
    }

    @Test
    public void testAndroidSymbolsAndStrippedEngineHaveSeparateCaches() throws Exception {
        Platform platform = Platform.X86_64Android;
        String filename = "libdmengine_release.so";
        File cachedSymbols = new File(cache, platform.getPair() + "/" + EngineVersion.sha1 + "/" + filename);
        FileUtils.writeStringToFile(cachedSymbols, "unstripped symbols", StandardCharsets.UTF_8);
        writeArtifact(platform.getPair(), "stripped/" + filename, "stripped engine");

        File engine = downloadExes(platform, Bob.VARIANT_RELEASE).get(0);
        assertEquals("stripped engine", Files.readString(engine.toPath()));
        assertEquals("unstripped symbols", Files.readString(cachedSymbols.toPath()));

        try (Project project = new Project(new DefaultFileSystem(), temporaryFolder.newFolder("project").getAbsolutePath(), "build")) {
            project.setOption("architectures", platform.getPair());
            project.setOption("variant", Bob.VARIANT_RELEASE);
            EngineArtifactsProvider.downloadSymbols(project, Progress.discarding());
            File symbols = new File(project.getBinaryOutputDirectory(), platform.getExtenderPair() + "/" + filename);
            assertEquals("unstripped symbols", Files.readString(symbols.toPath()));
            assertEquals("stripped engine", Files.readString(engine.toPath()));
        }
    }

    @Test
    public void testLegacyPlatformFallbackUsesStrippedArtifacts() throws Exception {
        writeArtifact("android", "stripped/libdmengine.so", "legacy stripped engine");
        File engine = downloadExes(Platform.Armv7Android, Bob.VARIANT_DEBUG).get(0);
        assertEquals("legacy stripped engine", Files.readString(engine.toPath()));
    }

    @Test
    public void testMissingPthreadWasmReportsDownloadFailure() throws Exception {
        writeArtifact("wasm_pthread-web", "dmengine_release.js", "engine loader");
        try {
            downloadExes(Platform.WasmPthreadWeb, Bob.VARIANT_RELEASE);
            fail("Expected the missing WASM artifact to fail the download");
        } catch (IOException e) {
            assertTrue(e.getMessage().contains("release engine for wasm_pthread-web (dmengine_release.wasm)"));
            assertTrue(e.getMessage().contains("Check your internet connection and try again."));
        }
    }
}
