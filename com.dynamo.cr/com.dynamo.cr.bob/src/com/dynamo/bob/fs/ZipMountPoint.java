// Copyright 2020-2026 The Defold Foundation
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
import java.util.zip.ZipException;

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
    private boolean isProject = true; // is it a Defold project?

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
        public void appendContent(byte[] content) throws IOException {
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

    public ZipMountPoint(IFileSystem fileSystem, String archivePath, boolean projectZip) {
        this.fileSystem = fileSystem;
        this.archivePath = archivePath;
        this.isProject = projectZip;
    }

    @Override
    public IResource get(String path) {
        ZipEntry entry = null;
        if (this.isProject) {
            if (this.file != null && includes(path)) {
                entry = this.file.getEntry(this.includeBaseDir + path);
            }
        } else {
            entry = this.file.getEntry(this.includeBaseDir + path);
        }
        if (entry != null) {
            return new ZipResource(this.fileSystem, path, entry);
        }
        return null;
    }

    @Override
    public void mount() throws IOException {
        try {
            this.file = new ZipFile(this.archivePath);

            if (this.isProject) {
                this.includeBaseDir = LibraryUtil.findIncludeBaseDir(this.file);
                this.includeDirs = LibraryUtil.readIncludeDirsFromArchive(this.includeBaseDir, this.file);
            }
        } catch (ZipException e) {
            throw new IOException(String.format("Failed to mount zip file '%s': %s", this.archivePath, e));
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
                if (this.isProject) {
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
                } else {
                    if (entry.isDirectory()) {
                        walker.handleDirectory(entryPath, results);
                    } else {
                        walker.handleFile(entryPath, results);
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
