package com.defold.libs;

import java.io.Closeable;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.security.CodeSource;
import java.security.ProtectionDomain;
import java.util.UUID;
import java.util.zip.ZipEntry;
import java.util.zip.ZipException;
import java.util.zip.ZipFile;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;

public class Libs {
    private static String texcLibDir = null;

    public static String getTexcLibDir() {
        if (texcLibDir == null) {
            try {
                setTexcLibDir();
            } catch (ZipException e) {
                throw new RuntimeException(e);
            } catch (URISyntaxException e) {
                throw new RuntimeException(e);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }
        return texcLibDir;
    }

    private enum PlatformType {
        Windows,
        Darwin,
        Linux
    }

    private static PlatformType getPlatform() {
        String os_name = System.getProperty("os.name").toLowerCase();

        if (os_name.indexOf("win") != -1) {
            return PlatformType.Windows;
        } else if (os_name.indexOf("mac") != -1) {
            return PlatformType.Darwin;
        } else if (os_name.indexOf("linux") != -1) {
            return PlatformType.Linux;
        } else {
            throw new RuntimeException(String.format("Could not identify OS: '%s'", os_name));
        }
    }

    private static void setTexcLibDir() throws URISyntaxException, ZipException, IOException {
        URI uri = getJarURI();
        String libSubPath = null;
        PlatformType platform = getPlatform();
        switch (platform) {
        case Windows:
            libSubPath = "lib/win32/texc_shared.dll";
            break;
        case Darwin:
            libSubPath = "lib/x86_64-darwin/libtexc_shared.dylib";
            break;
        case Linux:
            libSubPath = "lib/linux/libtexc_shared.so";
            break;
        }

        URL url = getFile(uri, libSubPath).toURL();
        File file = FileUtils.toFile(url);
        if (file.exists()) {
            texcLibDir = file.getParentFile().getAbsolutePath();
        } else {
            throw new IOException(String.format("Could not locate '%s' (%s)", libSubPath, url));
        }
    }

    private static URI getJarURI() throws URISyntaxException {
        final ProtectionDomain domain = Libs.class.getProtectionDomain();
        final CodeSource source = domain.getCodeSource();
        final URL url = source.getLocation();
        final URI uri;
        try {
            uri = url.toURI();
        } catch (URISyntaxException e) {
            e.printStackTrace();
            throw e;
        }
        return uri;
    }

    private static URI getFile(final URI where, final String filePath) throws ZipException, IOException {
        final File location;
        final URI fileURI;

        location = new File(where);

        // not in a JAR, just return the path on disk
        if(location.isDirectory()) {
            fileURI = URI.create(where.toString() + filePath);
        } else {
            final ZipFile zipFile = new ZipFile(location);
            try {
                fileURI = extract(zipFile, filePath);
            } finally {
                zipFile.close();
            }
        }

        return (fileURI);
    }

    private static String uniqueTmpDir() {
        final int length = 8;
        String tmp = FileUtils.getTempDirectory().getAbsolutePath();
        String uniquePath = FilenameUtils.concat(tmp, UUID.randomUUID().toString().substring(0, length));
        File f = new File(uniquePath);
        while (f.exists()) {
            uniquePath = FilenameUtils.concat(tmp, UUID.randomUUID().toString().substring(length));
            f = new File(uniquePath);
        }
        f.mkdir();
        f.deleteOnExit();
        return uniquePath;
    }

    private static URI extract(final ZipFile zipFile, final String filePath) throws IOException {
        String fileName = FilenameUtils.getName(filePath);
        String dstPath = FilenameUtils.concat(uniqueTmpDir(), fileName);
        final ZipEntry entry = zipFile.getEntry(filePath);

        if(entry == null) {
            throw new FileNotFoundException("cannot find file: " + filePath + " in archive: " + zipFile.getName());
        }

        final InputStream zipStream  = zipFile.getInputStream(entry);
        OutputStream fileStream = null;

        try {
            final byte[] buf;
            int i;

            fileStream = new FileOutputStream(dstPath);
            buf = new byte[1024];
            i = 0;

            while((i = zipStream.read(buf)) != -1) {
                fileStream.write(buf, 0, i);
            }
        } finally {
            close(zipStream);
            close(fileStream);
        }

        return (new File(dstPath).toURI());
    }

    private static void close(final Closeable stream) {
        if(stream != null) {
            try {
                stream.close();
            } catch(final IOException ex) {
                ex.printStackTrace();
            }
        }
    }
}
