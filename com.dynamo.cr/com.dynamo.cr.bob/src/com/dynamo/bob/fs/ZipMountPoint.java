package com.dynamo.bob.fs;

import java.io.ByteArrayOutputStream;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;
import java.util.Collection;
import java.util.Enumeration;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.fs.IFileSystem.IWalker;
import com.dynamo.bob.util.LibraryUtil;

public class ZipMountPoint implements IMountPoint {

    IFileSystem fileSystem;
    String archivePath;
    ZipFile file;
    Set<String> includeDirs = null;
    String includeBaseDir = "";

    private class ZipResource extends AbstractResource<IFileSystem> {
        ZipEntry entry;

        public ZipResource(IFileSystem fileSystem, String path, ZipEntry entry) {
            super(fileSystem, path);
            this.entry = entry;
        }

        @Override
        public byte[] getContent() throws IOException {
            InputStream is = null;
            try {
                ByteArrayOutputStream os = new ByteArrayOutputStream((int)this.entry.getSize());
                is = file.getInputStream(this.entry);
                IOUtils.copy(is, os);
                return os.toByteArray();
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

        @Override
        public long getLastModified() {
            return entry.getLastModifiedTime().toMillis();
        }

        @Override
        public boolean isFile() {
            boolean isDir = entry.isDirectory();
            return !isDir;
        }
    }

    public ZipMountPoint(IFileSystem fileSystem, String archivePath) {
        this.fileSystem = fileSystem;
        this.archivePath = archivePath;
    }

    @Override
    public IResource get(String path) {
        if (this.file != null && includes(path)) {
            ZipEntry entry = this.file.getEntry(this.includeBaseDir + path);
            if (entry != null) {
                return new ZipResource(this.fileSystem, path, entry);
            }
        }
        return null;
    }

    @Override
    public void mount() throws IOException {
        this.file = new ZipFile(this.archivePath);
        try {
            this.includeBaseDir = LibraryUtil.findIncludeBaseDir(this.file);
            this.includeDirs = LibraryUtil.readIncludeDirsFromArchive(this.includeBaseDir, this.file);
        } catch (ParseException e) {
            throw new IOException(e);
        }
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

    @Override
    public void walk(String path, IWalker walker, Collection<String> results) {
        path = FilenameUtils.normalizeNoEndSeparator(path, true);
        if (this.file != null) {
            Enumeration<? extends ZipEntry> entries = this.file.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String entryPath = entry.getName();
                if (entryPath.startsWith(this.includeBaseDir)) {
                    entryPath = entryPath.substring(this.includeBaseDir.length());
                    if (includes(entryPath) && entryPath.startsWith(path)) {
                        if (entry.isDirectory()) {
                            walker.handleDirectory(entryPath, results);
                        } else {
                            walker.handleFile(entryPath, results);
                        }
                    }
                }
            }
        }
    }

    private boolean includes(String path) {
        int sep = path.indexOf('/');
        if (sep != -1) {
            String dir = path.substring(0, sep);
            if (this.includeDirs.contains(dir)) {
                return true;
            }
        }
        return false;
    }
}
