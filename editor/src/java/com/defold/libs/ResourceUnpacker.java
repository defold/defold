// Copyright 2020-2022 The Defold Foundation
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

package com.defold.libs;

import java.io.*;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.nio.file.*;
import java.util.Date;
import java.util.Collections;
import java.util.Iterator;
import java.util.stream.Stream;

import com.defold.editor.Editor;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.apache.commons.io.FileUtils;

import com.dynamo.bob.Platform;

public class ResourceUnpacker {

    public static final String DEFOLD_UNPACK_PATH_KEY = "defold.unpack.path";
    public static final String DEFOLD_UNPACK_PATH_ENV_VAR = "DEFOLD_UNPACK_PATH";
    public static final String DEFOLD_EDITOR_SHA1_KEY = "defold.editor.sha1";

    private static volatile boolean isInitialized = false;
    private static Object lock = new Object();
    private static Logger logger = LoggerFactory.getLogger(ResourceUnpacker.class);
    private static Path unpackedLibDir;

    // unpack dirs should be automatically deleted using a shutdown hook but
    // the shutdown hook will not run if the editor doesn't shut down gracefully
    // in which case we may collect old unpack dirs
    // this function will delete unpack dirs older than a week if we are running
    // against the user home unpack dir
    private static void deleteOldUnpackDirs(Path unpackPath) throws IOException {
        final Path unpackRoot = unpackPath.getParent();
        final Path supportPath = Editor.getSupportPath();
        if (!unpackRoot.startsWith(supportPath)) {
            logger.info("not deleting old unpack dirs from {} as it is not in system support path {}", unpackRoot, supportPath);
            return;
        }

        logger.info("deleting old unpack dirs from {}", unpackRoot);
        final long now = new Date().getTime();
        final long oneWeekAgo = now - (7 * 24 * 60 * 60 * 1000);
        for (File unpackDir : unpackRoot.toFile().listFiles(File::isDirectory)) {
            long creationTime = Files.getLastModifiedTime(unpackDir.toPath()).toMillis();
            if (creationTime < oneWeekAgo) {
                logger.info("deleting unpack dir {}", unpackDir);
                FileUtils.deleteQuietly(unpackDir);
            }
        }
    }

    public static void unpackResources() throws IOException, URISyntaxException {
        // Once the resources have been unpacked, we can avoid acquiring the
        // lock by checking this early.
        if (isInitialized) {
            return;
        }

        synchronized (lock) {
            // Another thread might have already unpacked the resources while we
            // were waiting for the lock to be released.
            if (isInitialized) {
                return;
            }

            try {
                Path unpackPath  = getUnpackPath();
                deleteOldUnpackDirs(unpackPath);
                unpackResourceFile("builtins.zip", unpackPath.resolve("builtins"));
                unpackResourceDir("/_unpack", unpackPath);

                Path binDir = unpackPath.resolve(Platform.getJavaPlatform().getPair() + "/bin").toAbsolutePath();
                if (binDir.toFile().exists()) {
                    Files.walk(binDir).forEach(path -> path.toFile().setExecutable(true));
                }

                unpackedLibDir = unpackPath.resolve(Platform.getJavaPlatform().getPair() + "/lib").toAbsolutePath();
                System.setProperty("java.library.path", unpackedLibDir.toString());
                System.setProperty("jna.library.path", unpackedLibDir.toString());

                // Disable JOGL automatic library loading as it caused JVM
                // crashes on Linux. See DEFEDIT-494 for more details.
                System.setProperty("jogamp.gluegen.UseTempJarCache", "false");

                System.setProperty(DEFOLD_UNPACK_PATH_KEY, unpackPath.toAbsolutePath().toString());
                logger.info("defold.unpack.path={}", System.getProperty(DEFOLD_UNPACK_PATH_KEY));
            } finally {
                isInitialized = true;
            }
        }
    }

    public static Path getUnpackedLibraryPath(String libName) {
        String mappedName = System.mapLibraryName(libName);
        return unpackedLibDir.resolve(mappedName);
    }

    private static void unpackResourceFile(String resourceFileName, Path target) throws URISyntaxException, IOException {
        ClassLoader classLoader = ResourceUnpacker.class.getClassLoader();

        try (InputStream inputStream = classLoader.getResourceAsStream(resourceFileName)) {
            if (inputStream == null) {
                logger.warn("attempted to unpack non-existent resource file: {}", resourceFileName);
                return;
            }

            Path outputPath = target.resolve(resourceFileName);
            File outputFile = outputPath.toFile();

            try {
                if (outputFile.exists() && outputFile.isDirectory()) {
                    FileUtils.deleteQuietly(outputFile);
                }

                logger.debug("unpacking file '{}' to '{}'", resourceFileName, outputPath);
                File outputDirectory = outputFile.getParentFile();

                if (outputDirectory != null) {
                    outputDirectory.mkdirs();
                }

                Files.copy(inputStream, outputPath, StandardCopyOption.REPLACE_EXISTING);
            }
            catch (IOException exception) {
                logger.warn("unpacking file '{}' to '{}' failed", resourceFileName, outputPath, exception);
            }
        }
    }

    private static void unpackResourceDir(String resourceDir, Path target) throws IOException, URISyntaxException {
        URL url = ResourceUnpacker.class.getResource(resourceDir);
        if (url == null) {
            logger.warn("attempted to unpack non-existent resource directory: {}", resourceDir);
            return;
        }

        URI uri = url.toURI();
        try (FileSystem fs = getResourceFileSystem(uri)) {
            Path path;
            if (fs != null) {
                path = fs.getPath(resourceDir);
            } else {
                path = Paths.get(uri);
            }

            try (Stream<Path> walk = Files.walk(path)) {
                for (Iterator<Path> it = walk.iterator(); it.hasNext();) {
                    Path source = it.next();
                    Path dest = target.resolve(Paths.get(path.relativize(source).toString()));
                    if (dest.equals(target)) {
                        continue;
                    }
                    File destFile = dest.toFile();
                    if (destFile.exists() && destFile.isDirectory()) {
                        FileUtils.deleteQuietly(destFile);
                    }
                    logger.debug("unpacking '{}' to '{}'", source, dest);
                    try {
                        Files.copy(source, dest, StandardCopyOption.REPLACE_EXISTING);
                    }
                    catch (IOException e) {
                        logger.warn("unpacking '{}' to '{}' failed", source, dest, e);
                    }
                }
            }
        }
    }

    private static FileSystem getResourceFileSystem(URI uri) throws IOException, URISyntaxException {
        if (uri.getScheme().equals("jar")) {
            return FileSystems.newFileSystem(uri, Collections.<String, Object>emptyMap());
        } else {
            return null;
        }
    }

    private static Path getUnpackPath() throws IOException {
        String unpackPath = System.getProperty(DEFOLD_UNPACK_PATH_KEY);
        if (unpackPath == null) {
            unpackPath = System.getenv(DEFOLD_UNPACK_PATH_ENV_VAR);
        }

        if (unpackPath != null) {
            return ensureDirectory(Paths.get(unpackPath), true);
        }

        Path supportPath = Editor.getSupportPath();
        String sha1 = System.getProperty(DEFOLD_EDITOR_SHA1_KEY);
        if (sha1 != null) {
            return ensureDirectory(supportPath.resolve(Paths.get("unpack", sha1)), false);
        }

        Path tmpDir = Files.createTempDirectory("defold-unpack");
        deleteOnExit(tmpDir);
        return tmpDir;
    }

    private static Path ensureDirectory(Path path, boolean userDir) throws IOException {
        File f = path.toFile();
        if (!f.exists()) {
            f.mkdirs();
            if (!userDir) {
                deleteOnExit(path);
            }
        }
        return path;
    }

    // Note! There is a method FileUtils#forceDeleteOnExit which does not seem to work,
    // it does not delete the directory although the Javadoc says it should.
    private static void deleteOnExit(Path path) {
        File f = path.toFile();
        if (f.isDirectory()) {
            Runtime.getRuntime().addShutdownHook(new Thread(() -> FileUtils.deleteQuietly(f)));
        } else {
            f.deleteOnExit();
        }
    }
}
