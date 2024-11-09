// Copyright 2020-2024 The Defold Foundation
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

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.zip.ZipFile;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.archive.ArchiveEntry;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.util.FileUtil;

public class ZipPublisher extends Publisher {

    private static Logger logger = Logger.getLogger(ZipPublisher.class.getName());

    private File tempZipFile = null;
    private File destZipFile = null;
    private String projectRoot = null;
    private String filename = null;
    private ZipOutputStream zipOutputStream;

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
            this.destZipFile = new File(this.getPublisherSettings().getZipFilepath(), destZipName);
            if (!destZipFile.isAbsolute())
            {
                File cwd = new File(this.projectRoot);
                this.destZipFile = new File(cwd, this.destZipFile.getPath());
            }

            FileOutputStream resourcePackOutputStream = new FileOutputStream(this.tempZipFile);
            zipOutputStream = new ZipOutputStream(resourcePackOutputStream);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to create zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void stop() throws CompileExceptionError {
        try {
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
        finally {
            IOUtils.closeQuietly(zipOutputStream);
        }
    }

    @Override
    public void publish(ArchiveEntry entry, InputStream data) throws CompileExceptionError {
        try {
            final String archiveEntryHexdigest = entry.getHexDigest();
            final String archiveEntryName = entry.getName();
            final String zipEntryName = (archiveEntryHexdigest != null) ? archiveEntryHexdigest : archiveEntryName;
            ZipEntry currentEntry = new ZipEntry(zipEntryName);
            zipOutputStream.putNextEntry(currentEntry);
            zipOutputStream.write(entry.getHeader());
            data.transferTo(zipOutputStream);
            zipOutputStream.flush();
            zipOutputStream.closeEntry();
        } catch (FileNotFoundException exception) {
            throw new CompileExceptionError("Unable to find required file for liveupdate resources: " + exception.getMessage(), exception);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }
}
