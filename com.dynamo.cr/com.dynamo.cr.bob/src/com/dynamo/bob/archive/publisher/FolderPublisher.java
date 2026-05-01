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

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

public class FolderPublisher extends Publisher {

    private static Logger logger = Logger.getLogger(FolderPublisher.class.getName());

    private File outputDirectory = null;
    private String projectRoot = null;
    private String directoryName = null;
    private Set<String> writtenFiles = ConcurrentHashMap.newKeySet();

    public FolderPublisher(String projectRoot, PublisherSettings settings) {
        super(settings);
        this.projectRoot = projectRoot;
    }

    public void setDirectoryName(String directoryName) {
        this.directoryName = directoryName;
    }

    public File getOutputDirectory() {
        return this.outputDirectory;
    }

    @Override
    public void start() throws CompileExceptionError {
        try {
            PublisherSettings settings = this.getPublisherSettings();
            
            // Determine output directory name
            String destDirName = "defold_resources_" + this.platform + String.valueOf(System.currentTimeMillis() / 1000);
            if (this.directoryName != null) {
                destDirName = this.directoryName;
            }
            String folderName = settings.getOutputFolderName();
            if (folderName != null && !folderName.isEmpty()) {
                destDirName = folderName;
            }
            
            // Use output directory setting as base directory
            String outputDirectoryPath = settings.getOutputDirectory();
            if (outputDirectoryPath != null && !outputDirectoryPath.isEmpty()) {
                this.outputDirectory = new File(outputDirectoryPath, destDirName);
            } else {
                this.outputDirectory = new File(destDirName);
            }
            
            // Make path absolute if it's relative
            if (!this.outputDirectory.isAbsolute()) {
                File cwd = new File(this.projectRoot);
                this.outputDirectory = new File(cwd, this.outputDirectory.getPath());
            }

            // Create output directory if it doesn't exist
            if (!this.outputDirectory.exists()) {
                if (!this.outputDirectory.mkdirs()) {
                    throw new IOException("Failed to create output directory: " + this.outputDirectory.getAbsolutePath());
                }
            } else if (!this.outputDirectory.isDirectory()) {
                throw new IOException(String.format("'%s' exists, and is not a directory", this.outputDirectory));
            }

            writtenFiles.clear();
            entries.clear();

            logger.info("FolderPublisher initialized with output directory: %s", this.outputDirectory.getAbsolutePath());
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to create output directory for liveupdate resources: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void stop() throws CompileExceptionError {
        try {
            logger.info("FolderPublisher finished. Wrote %d files to '%s'", writtenFiles.size(), outputDirectory.getAbsolutePath());
        } catch (Exception exception) {
            throw new CompileExceptionError("Unable to finalize folder publisher: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void publish(ArchiveEntry entry, InputStream data) throws CompileExceptionError {
        final String archiveEntryHexdigest = entry.getHexDigest();
        final String archiveEntryName = entry.getName();
        final String fileName = (archiveEntryHexdigest != null) ? archiveEntryHexdigest : archiveEntryName;
        
        entries.put(archiveEntryName, entry);
        
        // Skip if file already written (duplicate entry)
        if (!writtenFiles.add(fileName)) {
            return;
        }

        try {
            File outputFile = new File(outputDirectory, fileName);

            // Write file with header + data
            try (BufferedOutputStream outputStream = new BufferedOutputStream(new FileOutputStream(outputFile))) {
                outputStream.write(entry.getHeader());
                data.transferTo(outputStream);
                outputStream.flush();
            }
            
            logger.info("Wrote file: %s", outputFile.getAbsolutePath());
        } catch (FileNotFoundException exception) {
            throw new CompileExceptionError("Unable to create file for liveupdate resource: " + exception.getMessage(), exception);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to write file for liveupdate resource: " + exception.getMessage(), exception);
        }
    }

    @Override
    public void publish(ArchiveEntry entry, byte[] data) throws CompileExceptionError {
        final String archiveEntryHexdigest = entry.getHexDigest();
        final String archiveEntryName = entry.getName();
        final String fileName = (archiveEntryHexdigest != null) ? archiveEntryHexdigest : archiveEntryName;
        
        entries.put(archiveEntryName, entry);
        
        // Skip if file already written (duplicate entry)
        if (!writtenFiles.add(fileName)) {
            entry.setDuplicatedDataBlob(true);
            return;
        }

        try {
            File outputFile = new File(outputDirectory, fileName);

            // Write file with header + data
            try (BufferedOutputStream outputStream = new BufferedOutputStream(new FileOutputStream(outputFile))) {
                outputStream.write(entry.getHeader());
                outputStream.write(data);
                outputStream.flush();
            }
            
            logger.fine("Wrote file: %s", outputFile.getAbsolutePath());
        } catch (FileNotFoundException exception) {
            throw new CompileExceptionError("Unable to create file for liveupdate resource: " + exception.getMessage(), exception);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to write file for liveupdate resource: " + exception.getMessage(), exception);
        }
    }
} 