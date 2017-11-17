package com.defold.libs;

import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.io.File;
import java.nio.file.Files;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.Collections;
import java.util.Iterator;
import java.util.stream.Stream;

import com.defold.editor.Editor;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.apache.commons.io.FileUtils;

import com.defold.editor.Platform;

public class ResourceUnpacker {

    public static final String DEFOLD_UNPACK_PATH_KEY = "defold.unpack.path";
    public static final String DEFOLD_UNPACK_PATH_ENV_VAR = "DEFOLD_UNPACK_PATH";
    public static final String DEFOLD_EDITOR_SHA1_KEY = "defold.editor.sha1";

    private static boolean isInitialized = false;
    private static Logger logger = LoggerFactory.getLogger(ResourceUnpacker.class);
    private static Path unpackedLibDir;

    public static void unpackResources() throws IOException, URISyntaxException {
        if (isInitialized) {
            return;
        }
        isInitialized = true;

        Path unpackPath  = getUnpackPath();
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
    }

    public static Path getUnpackedLibraryPath(String libName) {
        String mappedName = System.mapLibraryName(libName);
        return unpackedLibDir.resolve(mappedName);
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
                        logger.warn("unpack '{}' to '{}' failed", source, dest, e);
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
        if (supportPath != null && sha1 != null) {
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
