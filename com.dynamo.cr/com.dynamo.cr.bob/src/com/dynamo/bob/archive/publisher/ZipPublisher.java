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

package com.dynamo.bob.archive.publisher;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.logging.Logger;
import org.apache.commons.io.IOUtils;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

public class ZipPublisher extends Publisher {

    private static Logger logger = Logger.getLogger(ZipPublisher.class.getName());

    private File tempZipFile = null;
    private File destZipFile = null;
    private String projectRoot = null;
    private String filename = null;
    private ZipOutputStream zipOutputStream;
    private Set<String> zipEntries = ConcurrentHashMap.newKeySet();

    public ZipPublisher(String projectRoot, PublisherSettings settings) {
        super(settings);
        this.projectRoot = projectRoot;
    }

    public void setFilename(String filename)
    {
        this.filename = filename;
    }

    public File getZipFile() {
        return this.destZipFile;
    }

    @Override
    public void start() throws CompileExceptionError {
        try {
            String tempFilePrefix = "defold.resourcepack_" + this.platform + "_";
            this.tempZipFile = File.createTempFile(tempFilePrefix, ".zip");

            String destZipName = this.tempZipFile.getName();
            if (this.filename != null) {
                destZipName = this.filename;
            }
            PublisherSettings settings = this.getPublisherSettings();
            String filename = settings.getZipFilename();
            if (filename != null && !settings.getZipFilename().isEmpty()) {
                destZipName = settings.getZipFilename();
                if (!destZipName.endsWith(".zip")) {
                    destZipName = destZipName + ".zip";
                }
            }
            this.destZipFile = new File(settings.getZipFilepath(), destZipName);
            if (!destZipFile.isAbsolute())
            {
                File cwd = new File(this.projectRoot);
                this.destZipFile = new File(cwd, this.destZipFile.getPath());
            }

            zipEntries.clear();
            entries.clear();

            BufferedOutputStream resourcePackOutputStream = new BufferedOutputStream(Files.newOutputStream(this.tempZipFile.toPath()));
            zipOutputStream = new ZipOutputStream(resourcePackOutputStream);
            zipOutputStream.setLevel(settings.getCompressionLevel());
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to create zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void stop() throws CompileExceptionError {
        try {
            zipOutputStream.flush();
            IOUtils.closeQuietly(zipOutputStream);

            // make sure parent directories exist
            File destZipDir = this.destZipFile.getParentFile();
            if (!destZipDir.exists()) {
                destZipDir.mkdirs();
            } else if (!destZipDir.isDirectory()) {
                throw new IOException(String.format("'%s' exists, and is not a directory", destZipDir));
            }

            // move resource pack to destination
            Files.move(this.tempZipFile.toPath(), this.destZipFile.toPath(), StandardCopyOption.REPLACE_EXISTING);
            logger.info("Wrote '%s'", this.destZipFile);
        }
        catch (Exception exception) {
            throw new CompileExceptionError("Unable to create zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void publish(ArchiveEntry entry, InputStream data) throws CompileExceptionError {
        final String archiveEntryHexdigest = entry.getHexDigest();
        final String archiveEntryName = entry.getName();
        final String zipEntryName = (archiveEntryHexdigest != null) ? archiveEntryHexdigest : archiveEntryName;
        entries.put(archiveEntryName, entry);
        if (!zipEntries.add(zipEntryName)) {
            return;
        }
        try {
            ZipEntry currentEntry = new ZipEntry(zipEntryName);
            synchronized (zipOutputStream) {
                zipOutputStream.putNextEntry(currentEntry);
                zipOutputStream.write(entry.getHeader());
                data.transferTo(zipOutputStream);
                zipOutputStream.closeEntry();
            }
        } catch (FileNotFoundException exception) {
            throw new CompileExceptionError("Unable to find required file for liveupdate resources: " + exception.getMessage(), exception);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void publish(ArchiveEntry entry, byte[] data) throws CompileExceptionError {
        final String archiveEntryHexdigest = entry.getHexDigest();
        final String archiveEntryName = entry.getName();
        final String zipEntryName = (archiveEntryHexdigest != null) ? archiveEntryHexdigest : archiveEntryName;
        entries.put(archiveEntryName, entry);
        if (!zipEntries.add(zipEntryName)) {
            entry.setDuplicatedDataBlob(true);
            return;
        }
        try {
            ZipEntry currentEntry = new ZipEntry(zipEntryName);
            synchronized (zipOutputStream) {
                zipOutputStream.putNextEntry(currentEntry);
                zipOutputStream.write(entry.getHeader());
                zipOutputStream.write(data);
                zipOutputStream.closeEntry();
            }
        } catch (FileNotFoundException exception) {
            throw new CompileExceptionError("Unable to find required file for liveupdate resources: " + exception.getMessage(), exception);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }
}
