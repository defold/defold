// Copyright 2020-2022 The Defold Foundation
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
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.Bob;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;

public class ZipPublisher extends Publisher {

    private String projectRoot = null;
    private String archivePrefix = "";

    public ZipPublisher(String projectRoot, String archivePrefix, PublisherSettings settings) {
        super(settings);
        this.projectRoot = projectRoot;
        this.archivePrefix = archivePrefix;
    }

    private File createArchive(String archiveName) throws CompileExceptionError {
        try {
            String namePrefix = String.format("%s_%s_%s_", this.archivePrefix, this.platform, archiveName);
            File zipFile = File.createTempFile(namePrefix, ".zip");

            Bob.verbose("Compressing %s\n", zipFile.getName());

            FileOutputStream resourcePackOutputStream = new FileOutputStream(zipFile);
            ZipOutputStream zipOutputStream = new ZipOutputStream(resourcePackOutputStream);
            try {
                Map<String, File> entries = this.getEntries(archiveName);

                for (String hexDigest : entries.keySet()) {
                    File fhandle = entries.get(hexDigest);

                    ZipEntry currentEntry = new ZipEntry(fhandle.getName());
                    zipOutputStream.putNextEntry(currentEntry);

                    FileInputStream currentInputStream = new FileInputStream(fhandle);
                    int currentLength = 0;
                    byte[] currentBuffer = new byte[1024];
                    while ((currentLength = currentInputStream.read(currentBuffer)) > 0) {
                        zipOutputStream.write(currentBuffer, 0, currentLength);
                    }

                    zipOutputStream.closeEntry();
                    IOUtils.closeQuietly(currentInputStream);
                }
            } catch (FileNotFoundException exception) {
                throw new CompileExceptionError("Unable to find required file for liveupdate resources: " + exception.getMessage(), exception);
            } catch (IOException exception) {
                throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
            } finally {
                IOUtils.closeQuietly(zipOutputStream);
            }

            return zipFile;
        } catch (IOException exception) {
            throw new CompileExceptionError(String.format("Unable to create zip archive '%s' for liveupdate resources: %s", exception.getMessage()), exception);
        }
    }

    private void moveFiles(File outputDir, List<File> files) throws CompileExceptionError {
        try {
            for (File file : files) {
                File target = new File(outputDir, file.getName());
                if (!target.isAbsolute())
                {
                    File cwd = new File(this.projectRoot);
                    target = new File(cwd, target.getPath());
                }

                File parentDir = target.getParentFile();
                if (!parentDir.exists()) {
                    parentDir.mkdirs();
                } else if (!parentDir.isDirectory()) {
                    throw new IOException(String.format("'%s' exists, and is not a directory", parentDir));
                }

                Files.move(file.toPath(), target.toPath(), StandardCopyOption.REPLACE_EXISTING);
                System.out.printf("\nZipPublisher: Wrote '%s'\n", target);
            }
        } catch (IOException exception) {
            throw new CompileExceptionError(String.format("Unable to create zip archive '%s' for liveupdate resources: %s",  exception.getMessage()), exception);
        }
    }

    @Override
    public void Publish() throws CompileExceptionError {
        List<File> outputArchives = new ArrayList<>();

        for (String archiveName : this.getArchiveNames()) {
            File archive = createArchive(archiveName);
            outputArchives.add(archive);
        }

        File outputDir = new File(this.getPublisherSettings().getZipFilepath());
        if (!outputDir.isAbsolute()) {
            File cwd = new File(this.projectRoot);
            outputDir = new File(cwd, outputDir.getPath());
        }
        moveFiles(outputDir, outputArchives);
    }

    public List<IResource> getOutputs(IResource input) {
        List<IResource> outputs = new ArrayList<IResource>();
        return outputs;
    }

    public List<InputStream> getOutputResults() {
        List<InputStream> outputs = new ArrayList<InputStream>();
        return outputs;
    }

}
