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

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import org.apache.commons.io.FileUtils;

import com.defold.editor.Platform;

public class ResourceUnpacker {

    public static final String DEFOLD_UNPACK_PATH_KEY = "defold.unpack.path";

    private static boolean isInitialized = false;
    private static Logger logger = LoggerFactory.getLogger(ResourceUnpacker.class);

    public static void unpackResources() throws IOException, URISyntaxException {
        if (isInitialized) {
            return;
        }

        isInitialized = true;

        Path unpackPath  = getUnpackPath();
        unpackResourceDir("/_unpack/" + Platform.getJavaPlatform().getPair(), unpackPath);
        unpackResourceDir("/_unpack/shared", unpackPath);

        String[] platforms = new String[] { "armv7-ios", "arm64-ios" };
        for (String p : platforms) {
            Path dstPath = unpackPath.resolve(p);
            dstPath.toFile().mkdirs();
            unpackResourceDir("/_unpack/" + p, dstPath);
        }

        Path binDir = unpackPath.resolve("bin").toAbsolutePath();
        if (binDir.toFile().exists()) {
            Files.walk(binDir).forEach(path -> path.toFile().setExecutable(true));
        }

        String libDir = unpackPath.resolve("lib").toAbsolutePath().toString();
        System.setProperty("java.library.path", libDir);
        System.setProperty("jna.library.path", libDir);

        // Disable JOGL automatic library loading as it caused JVM
        // crashes on Linux. See DEFEDIT-494 for more details.
        System.setProperty("jogamp.gluegen.UseTempJarCache", "false");

        System.setProperty(DEFOLD_UNPACK_PATH_KEY, unpackPath.toAbsolutePath().toString());
        logger.info("defold.unpack.path={}", System.getProperty(DEFOLD_UNPACK_PATH_KEY));
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
                    logger.debug("unpacking '{}' to '{}'", source, dest);
                    Files.copy(source, dest, StandardCopyOption.REPLACE_EXISTING);
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
        if (unpackPath != null) {
            Path p = Paths.get(unpackPath);
            File f = p.toFile();
            if (f.exists()) {
                FileUtils.cleanDirectory(f);
            } else {
                f.mkdirs();
            }
            return p;
        } else {
            return Files.createTempDirectory("defold-unpack");
        }
    }
}
