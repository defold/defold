package com.dynamo.bob.util;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;
import org.apache.commons.io.IOUtils;

public final class ZipUtils {

    public static void zipFolder(final File folder, final File zipFile) throws IOException {
        zipFolder(folder, new FileOutputStream(zipFile));
    }

    public static void zipFolder(final File folder, final OutputStream outputStream) throws IOException {
        try (ZipOutputStream zipOutputStream = new ZipOutputStream(outputStream)) {
            processFolder(folder, zipOutputStream, folder.getPath().length() + 1);
        }
    }

    private static void processFolder(final File folder, final ZipOutputStream zipOutputStream, final int prefixLength) throws IOException {
        for (final File file : folder.listFiles()) {
            if (file.isFile()) {
                final ZipEntry zipEntry = new ZipEntry(file.getPath().substring(prefixLength));
                zipOutputStream.putNextEntry(zipEntry);
                try (FileInputStream inputStream = new FileInputStream(file)) {
                    IOUtils.copy(inputStream, zipOutputStream);
                }
                zipOutputStream.closeEntry();
            }
			else if (file.isDirectory()) {
                processFolder(file, zipOutputStream, prefixLength);
            }
        }
    }
}
