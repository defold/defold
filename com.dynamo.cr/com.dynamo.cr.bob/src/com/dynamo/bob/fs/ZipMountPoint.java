package com.dynamo.bob.fs;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.apache.commons.io.IOUtils;

public class ZipMountPoint implements IMountPoint {

    IFileSystem fileSystem;
    String archivePath;
    ZipFile file;

    private class ZipResource extends AbstractResource<IFileSystem> {
        ZipEntry entry;

        public ZipResource(IFileSystem fileSystem, String path, ZipEntry entry) {
            super(fileSystem, path);
            this.entry = entry;
        }

        @Override
        public byte[] getContent() throws IOException {
            byte[] buffer = new byte[(int)this.entry.getSize()];
            InputStream is = null;
            try {
                is = file.getInputStream(this.entry);
                is.read(buffer);
                return buffer;
            } finally {
                IOUtils.closeQuietly(is);
            }
        }

        @Override
        public void setContent(byte[] content) throws IOException {
            throw new IOException("Zip resources can't be written to.");
        }

        @Override
        public boolean exists() {
            return true;
        }

        @Override
        public void remove() {
            throw new RuntimeException("Zip resources can't be removed.");
        }

        @Override
        public void setContent(InputStream stream) throws IOException {
            throw new IOException("Zip resources can't be removed.");
        }
    }

    public ZipMountPoint(IFileSystem fileSystem, String archivePath) {
        this.fileSystem = fileSystem;
        this.archivePath = archivePath;
    }

    @Override
    public IResource get(String path) {
        if (this.file != null) {
            ZipEntry entry = this.file.getEntry(path);
            if (entry != null) {
                return new ZipResource(this.fileSystem, path, entry);
            }
        }
        return null;
    }

    @Override
    public void mount() throws IOException {
        this.file = new ZipFile(this.archivePath);
    }

    @Override
    public void unmount() {
        // Zip files are not quietly closeable on their own
        IOUtils.closeQuietly(new Closeable() {
            @Override
            public void close() throws IOException {
                if (file != null) {
                    file.close();
                }
            }
        });
        this.file = null;
    }
}
