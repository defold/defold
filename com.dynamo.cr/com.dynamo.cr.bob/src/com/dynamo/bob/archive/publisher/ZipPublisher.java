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
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.ZipEntry;

import org.apache.commons.io.IOUtils;

import org.apache.commons.compress.archivers.zip.ZipArchiveOutputStream;
import org.apache.commons.compress.archivers.zip.ParallelScatterZipCreator;
import org.apache.commons.compress.archivers.zip.ZipArchiveEntry;
import org.apache.commons.compress.parallel.InputStreamSupplier;

import com.dynamo.bob.Project;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.FileUtil;

public class ZipPublisher extends Publisher {

    private static Logger logger = Logger.getLogger(ZipPublisher.class.getName());

    private Project project = null;
    private File resourcePackZip = null;
    private String filename = null;

    public ZipPublisher(PublisherSettings settings, Project project) {
        super(settings);
        this.project = project;
    }

    public void setFilename(String filename)
    {
        this.filename = filename;
    }

    @Override
    public void Publish() throws CompileExceptionError {
        try {
            String tempFilePrefix = "defold.resourcepack_" + this.platform + "_";
            this.resourcePackZip = File.createTempFile(tempFilePrefix, ".zip");

            String outputName = this.resourcePackZip.getName();
            if (this.filename != null) {
                outputName = this.filename;
            }

            int nThreads = project.getMaxCpuThreads();
            logger.info("Writing zip archive with a fixed thread pool executor using %d threads", nThreads);
            ExecutorService executorService = Executors.newFixedThreadPool(nThreads);

            // write files to zip
            final FileOutputStream outputStream = new FileOutputStream(this.resourcePackZip);
            final ZipArchiveOutputStream zipArchiveOutputStream = new ZipArchiveOutputStream(outputStream);
            try {
                final ParallelScatterZipCreator scatterZipCreator = new ParallelScatterZipCreator(executorService);
                for (final File file : this.getEntries().keySet()) {
                    final InputStreamSupplier streamSupplier = new InputStreamSupplier() {
                        @Override
                        public InputStream get() {
                            InputStream is = null;
                            try {
                                is = Files.newInputStream(file.toPath());
                            } catch (IOException e) {
                                e.printStackTrace();
                            }
                            return is;
                        }
                    };
                    final ZipArchiveEntry zipArchiveEntry = new ZipArchiveEntry(file, file.getName());
                    zipArchiveEntry.setMethod(ZipEntry.DEFLATED);
                    scatterZipCreator.addArchiveEntry(zipArchiveEntry, streamSupplier);
                }
                scatterZipCreator.writeTo(zipArchiveOutputStream);
            } catch (Exception exception) {
                throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
            } finally {
                IOUtils.closeQuietly(zipArchiveOutputStream);
            }

            // get handle to output zip file
            File exportFilehandle = new File(this.getPublisherSettings().getZipFilepath(), outputName);
            if (!exportFilehandle.isAbsolute())
            {
                File cwd = new File(project.getRootDirectory());
                exportFilehandle = new File(cwd, exportFilehandle.getPath());
            }

            // create missing folders
            File parentDir = exportFilehandle.getParentFile();
            if (!parentDir.exists()) {
                parentDir.mkdirs();
            } else if (!parentDir.isDirectory()) {
                throw new IOException(String.format("'%s' exists, and is not a directory", parentDir));
            }

            // move temp zip to output zip
            Files.move(this.resourcePackZip.toPath(), exportFilehandle.toPath(), StandardCopyOption.REPLACE_EXISTING);
            System.out.printf("\nZipPublisher: Wrote '%s'\n", exportFilehandle);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to create zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
    }
}
