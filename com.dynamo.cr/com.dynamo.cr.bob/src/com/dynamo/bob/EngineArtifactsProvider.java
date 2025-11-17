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

package com.dynamo.bob;

import com.dynamo.bob.archive.EngineVersion;
import com.dynamo.bob.bundle.BundleHelper;
import com.dynamo.bob.logging.Logger;
import org.apache.commons.io.FileUtils;
import com.dynamo.bob.util.PackedResources;
import com.dynamo.bob.util.TimeProfiler;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;
import com.dynamo.graphics.proto.Graphics.PlatformProfile.OS;

/**
 * Provides access to engine artifacts (binaries and symbols), fetching them locally
 * or downloading from the artifact store when not available.
 */
public final class EngineArtifactsProvider {
    private static final Logger logger = Logger.getLogger(EngineArtifactsProvider.class.getName());
    private static File sCacheBase;

    private EngineArtifactsProvider() {}

    public static synchronized void setCacheBase(File cacheBase) {
        sCacheBase = cacheBase;
    }

    private static synchronized File getCacheBase() {
        if (sCacheBase != null) return sCacheBase;
        return Bob.getRootFolder();
    }

    public static List<File> getDefaultDmengineFiles(Platform platform, String variant) throws IOException {
        try {
            List<String> binaryPaths = getDefaultDmenginePaths(platform, variant);
            List<File> files = new ArrayList<File>();
            for (String path : binaryPaths) {
                files.add(new File(path));
            }
            return files;
        } catch (RuntimeException e) {
            return downloadExes(platform, variant, Bob.ARTIFACTS_URL);
        }
    }

    public static void downloadSymbols(Project project, IProgress progress) throws IOException, CompileExceptionError {
        String archs = project.option("architectures", null);
        String[] platforms;
        if (archs != null) {
            platforms = archs.split(",");
        } else {
            platforms = project.getPlatformStrings();
        }

        progress.beginTask(IProgress.Task.DOWNLOADING_SYMBOLS, platforms.length);

        final String variant = project.option("variant", Bob.VARIANT_RELEASE);
        String variantSuffix = getVariantSuffix(variant);

        for (String platformKey : platforms) {
            Platform p = Platform.get(platformKey);
            String symbolsFilename = getSymbolsFilenameForPlatform(p, variantSuffix);

            if (symbolsFilename != null) {
                try {
                    String fallbackKey = (p != null) ? p.getOs() : null;
                    File cached = getOrDownloadArtifact(platformKey, symbolsFilename, fallbackKey, false);
                    if (cached == null || !cached.exists() || cached.length() == 0) {
                        logger.warning("Symbols not available: %s/%s", platformKey, symbolsFilename);
                        continue;
                    }
                    File targetFolder = new File(project.getBinaryOutputDirectory(), p.getExtenderPair());
                    if (symbolsFilename.endsWith(".zip")) {
                        File expectedDir = new File(targetFolder, symbolsFilename.substring(0, symbolsFilename.length() - 4));
                        if (!expectedDir.exists()) {
                            unzipIfZip(cached, targetFolder);
                        }
                    } else {
                        File target = new File(targetFolder, symbolsFilename);
                        if (!target.exists()) {
                            target.getParentFile().mkdirs();
                            FileUtils.copyFile(cached, target);
                        }
                    }
                } catch (Exception e) {
                    logger.warning("Failed to fetch symbols '%s' for platform '%s': %s", symbolsFilename, platformKey, e.getMessage());
                    // Skip; do not fail the build
                }
            }
            progress.worked(1);
        }
    }

    private static List<File> downloadExes(Platform platform, String variant, String artifactsURL) throws IOException {
        Bob.init();
        TimeProfiler.start("DownloadExes %s for %s", platform, variant);
        List<File> binaryFiles = new ArrayList<File>();
        String[] exeSuffixes = platform.getExeSuffixes();
        String defaultDmengineExeName = getDefaultDmengineExeName(variant);
        for (String exeSuffix : exeSuffixes) {
            String exeName = platform.getExePrefix() + defaultDmengineExeName + exeSuffix;
            File file = getOrDownloadArtifact(platform.getPair(), exeName, platform.getOs(), true);
            if (file == null || !file.exists() || file.length() == 0) {
                throw new IOException(String.format("%s could not be found locally or downloaded, create an application manifest to build the engine remotely.", exeName));
            }
            binaryFiles.add(file);
        }
        TimeProfiler.stop();
        return binaryFiles;
    }

    private static URL buildArtifactURL(String artifactsURL, String platformKey, String filename) throws IOException {
        try {
            return new URL(String.format(artifactsURL + "%s/engine/%s/%s", EngineVersion.sha1, platformKey, filename));
        } catch (Exception e) {
            throw new IOException(e);
        }
    }

    private static void downloadWithFallback(URL primary, URL fallback, File target, boolean executable) throws IOException {
        try {
            target.getParentFile().mkdirs();
            Bob.atomicCopy(primary, target, executable);
        } catch (IOException e) {
            if (fallback == null) throw e;
            logger.info("First download attempt failed, trying fallback URL: %s", fallback);
            Bob.atomicCopy(fallback, target, executable);
        }
    }

    private static void unzipIfZip(File file, File targetFolder) throws IOException {
        if (file.getName().endsWith(".zip")) {
            try (FileInputStream fis = new FileInputStream(file)) {
                BundleHelper.unzip(fis, targetFolder.toPath());
            }
        }
    }

    private static String getVariantSuffix(String variant) {
        switch (variant) {
            case Bob.VARIANT_RELEASE:
                return "_release";
            case Bob.VARIANT_HEADLESS:
                return "_headless";
            default:
                return ""; // debug
        }
    }

    private static String getDefaultDmengineExeName(String variant) {
        return "dmengine" + getVariantSuffix(variant);
    }

    private static String getSymbolsFilenameForPlatform(Platform platform, String variantSuffix) {
        if (platform == null) return null;
        OS os = platform.getOsID();
        if (os == OS.OS_ID_WINDOWS) {
            return String.format("dmengine%s.pdb", variantSuffix);
        }
        if (os == OS.OS_ID_ANDROID) {
            return String.format("libdmengine%s.so", variantSuffix);
        }
        if (os == OS.OS_ID_OSX || os == OS.OS_ID_IOS) {
            return String.format("dmengine%s.dSYM.zip", variantSuffix);
        }
        if (os == OS.OS_ID_WEB) {
            return String.format("dmengine%s.js.symbols", variantSuffix);
        }
        return null;
    }

    private static File getCacheFile(String platformKey, String filename) {
        File base = getCacheBase();
        File dir = new File(new File(base, platformKey), EngineVersion.sha1);
        dir.mkdirs();
        return new File(dir, filename);
    }

    private static File getOrDownloadArtifact(String platformKey, String filename, String fallbackPlatformKey, boolean executable) {
        File cached = getCacheFile(platformKey, filename);
        try {
            if (cached.exists() && cached.length() > 0) {
                return cached;
            }
            URL primary = buildArtifactURL(Bob.ARTIFACTS_URL, platformKey, filename);
            URL fallback = fallbackPlatformKey != null ? buildArtifactURL(Bob.ARTIFACTS_URL, fallbackPlatformKey, filename) : null;
            downloadWithFallback(primary, fallback, cached, executable);
            if (executable) {
                cached.setExecutable(true);
            }
            return cached;
        } catch (IOException e) {
            logger.warning("Artifact download failed: %s/%s (%s)", platformKey, filename, e.getMessage());
            return null;
        }
    }

    private static List<String> getDefaultDmenginePaths(Platform platform, String variant) throws IOException {
        return getExes(platform, getDefaultDmengineExeName(variant));
    }

    private static List<String> getExes(Platform platform, String name) throws IOException {
        String[] exeSuffixes = platform.getExeSuffixes();
        List<String> exes = new ArrayList<String>();
        for (String exeSuffix : exeSuffixes) {
            exes.add(getExeWithExtension(platform, name, exeSuffix));
        }
        return exes;
    }

    private static String getExeWithExtension(Platform platform, String name, String extension) throws IOException {
        Bob.init();
        PackedResources.waitForuUpackAllLibsAsync();
        TimeProfiler.start("getExeWithExtension %s.%s", name, extension);
        String exeName = platform.getPair() + "/" + platform.getExePrefix() + name + extension;
        File f = new File(Bob.getRootFolder(), exeName);
        if (!f.exists()) {
            URL url = Bob.class.getResource("/libexec/" + exeName);
            if (url == null) {
                throw new RuntimeException(String.format("/libexec/%s could not be found locally, create an application manifest to build the engine remotely.", exeName));
            }

            Bob.atomicCopy(url, f, true);
        }
        TimeProfiler.addData("path", f.getAbsolutePath());
        TimeProfiler.stop();
        return f.getAbsolutePath();
    }
}
