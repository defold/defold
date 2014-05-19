package com.dynamo.bob.fs;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.URL;
import java.util.Collection;
import java.util.Enumeration;
import java.util.zip.ZipEntry;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.IResourceScanner;
import com.dynamo.bob.fs.IFileSystem.IWalker;
import com.dynamo.bob.util.PathUtil;

public class ClassLoaderMountPoint implements IMountPoint {

    private IFileSystem fileSystem;
    private String filter;
    private IResourceScanner resourceScanner;

    private class Resource extends AbstractResource<IFileSystem> {
        public Resource(IFileSystem fileSystem, String path) {
            super(fileSystem, path);
        }

        @Override
        public byte[] getContent() throws IOException {
            InputStream is = null;
            try {
                is = getClass().getClassLoader().getResourceAsStream(this.path);
                ByteArrayOutputStream os = new ByteArrayOutputStream();
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
    }

    public ClassLoaderMountPoint(IFileSystem fileSystem, String filter, IResourceScanner resourceScanner) {
        this.fileSystem = fileSystem;
        this.filter = filter;
        this.resourceScanner = resourceScanner;
    }

    @Override
    public IResource get(String path) {
        // Ignore paths not matching the filter
        if (this.filter != null && !PathUtil.wildcardMatch(path, this.filter)) {
            return null;
        }
        URL url = getClass().getClassLoader().getResource(path);
        if (url != null) {
            return new Resource(this.fileSystem, path);
        }
        return null;
    }

    @Override
    public void mount() throws IOException {
    }

    @Override
    public void unmount() {
    }

    @Override
    public void walk(String path, IWalker walker, Collection<String> results) {
        path = FilenameUtils.normalizeNoEndSeparator(path, true);
        for (String resourcePath : this.resourceScanner.scan(this.filter)) {
            if (resourcePath.startsWith(path)) {
                walker.handleFile(resourcePath, results);
            }
        }
    }
}
