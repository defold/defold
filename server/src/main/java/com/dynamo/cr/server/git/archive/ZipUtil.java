package com.dynamo.cr.server.git.archive;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;
import org.apache.commons.io.filefilter.AbstractFileFilter;
import org.apache.commons.io.filefilter.IOFileFilter;
import org.apache.commons.io.filefilter.TrueFileFilter;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Collection;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public class ZipUtil {

    public static void zipFilesAtomic(File sourceDirectory, File targetZipFile, String comment) throws IOException {
        File temporaryZipFile = null;
        try {
            temporaryZipFile = File.createTempFile("tmp", ".zip", targetZipFile.getParentFile());
            ZipUtil.zipFiles(sourceDirectory, temporaryZipFile, comment);
            Files.move(temporaryZipFile.toPath(), targetZipFile.toPath(), StandardCopyOption.ATOMIC_MOVE);
        } catch (IOException e) {
            if (temporaryZipFile != null) {
                temporaryZipFile.delete();
            }
            throw e;
        }
    }

    private static void zipFiles(File directory, File zipFile, String comment) throws IOException {
        ZipOutputStream zipOut = null;

        try {
            zipOut = new ZipOutputStream(new FileOutputStream(zipFile));
            zipOut.setComment(comment);
            IOFileFilter dirFilter = new AbstractFileFilter() {
                @Override
                public boolean accept(File file) {
                    return !file.getName().equals(".git");
                }
            };

            Collection<File> files = FileUtils.listFiles(directory, TrueFileFilter.INSTANCE, dirFilter);
            int prefixLength = directory.getAbsolutePath().length();
            for (File file : files) {
                String p = file.getAbsolutePath().substring(prefixLength);
                if (p.startsWith("/")) {
                    p = p.substring(1);
                }
                ZipEntry ze = new ZipEntry(p);
                zipOut.putNextEntry(ze);
                FileUtils.copyFile(file, zipOut);
            }
        } finally {
            IOUtils.closeQuietly(zipOut);
        }
    }
}
