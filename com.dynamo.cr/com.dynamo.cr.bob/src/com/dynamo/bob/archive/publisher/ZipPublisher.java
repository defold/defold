// Copyright 2020-2023 The Defold Foundation
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
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import org.apache.commons.io.IOUtils;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.FileUtil;

public class ZipPublisher extends Publisher {

    private File resourcePackZip = null;
    private String projectRoot = null;
    private String filename = null;

    public ZipPublisher(String projectRoot, PublisherSettings settings) {
        super(settings);
        this.projectRoot = projectRoot;
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

            FileOutputStream resourcePackOutputStream = new FileOutputStream(this.resourcePackZip);
            ZipOutputStream zipOutputStream = new ZipOutputStream(resourcePackOutputStream);
            try {
                for (File fhandle : this.getEntries().keySet()) {
                    ZipEntry currentEntry = new ZipEntry(fhandle.getName());
                    zipOutputStream.putNextEntry(currentEntry);
                    FileUtil.writeToStream(fhandle, zipOutputStream);
                    zipOutputStream.closeEntry();
                }
            } catch (FileNotFoundException exception) {
                throw new CompileExceptionError("Unable to find required file for liveupdate resources: " + exception.getMessage(), exception);
            } catch (IOException exception) {
                throw new CompileExceptionError("Unable to write to zip archive for liveupdate resources: " + exception.getMessage(), exception);
            } finally {
                IOUtils.closeQuietly(zipOutputStream);
            }

            File exportFilehandle = new File(this.getPublisherSettings().getZipFilepath(), outputName);
            if (!exportFilehandle.isAbsolute())
            {
                File cwd = new File(this.projectRoot);
                exportFilehandle = new File(cwd, exportFilehandle.getPath());
            }

            File parentDir = exportFilehandle.getParentFile();
            if (!parentDir.exists()) {
                parentDir.mkdirs();
            } else if (!parentDir.isDirectory()) {
                throw new IOException(String.format("'%s' exists, and is not a directory", parentDir));
            }

            Files.move(this.resourcePackZip.toPath(), exportFilehandle.toPath(), StandardCopyOption.REPLACE_EXISTING);
            System.out.printf("\nZipPublisher: Wrote '%s'\n", exportFilehandle);
        } catch (IOException exception) {
            throw new CompileExceptionError("Unable to create zip archive for liveupdate resources: " + exception.getMessage(), exception);
        }
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
