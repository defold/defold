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

package com.dynamo.bob.util;

import java.util.Map;
import java.util.HashMap;
import java.util.List;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.StringWriter;
import java.io.BufferedWriter;

import com.dynamo.bob.Bob;
import com.dynamo.bob.archive.ArchiveBuilder;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ArchiveEntry;

import com.dynamo.bob.archive.publisher.Publisher;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;
import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;
import com.samskivert.mustache.Mustache;
import com.samskivert.mustache.Template;

/**
 * Class to generate build reports in JSON and HTML format from a Project instance.
 * Report data can be gathered either from disk or a DARC file (both analyzing the
 * Projects output paths).
 */
public class ReportGenerator {

    public static final String REPORT_VERSION_NUMBER = "1.0.0";

    /**
     * Helper class to keep track resources sizes/flags used by a project.
     */
    private class ResourceEntry {
        String path;
        long size;
        long compressedSize;
        boolean encrypted;
        boolean excluded;

        public ResourceEntry(String path, long size, long compressedSize, boolean encrypted, boolean excluded) {
            this.path = path;
            this.size = size;
            this.compressedSize = compressedSize;
            this.encrypted = encrypted;
            this.excluded = excluded;
        }
        public ResourceEntry(String path, long size, long compressedSize, boolean encrypted) {
            this(path, size, compressedSize, encrypted, false);
        }
        public ResourceEntry(String path, long size) {
            this(path, size, size, false, false);
        }
    }

    private Project project = null;
    private HashMap<String, ResourceEntry> resources;
    private HashMap<String, ResourceEntry> excludedResources;

    /**
     * Creates a ReportGenerator that looks into the supplied project instance
     * for build options, projects properties and resource ouputs.
     * The project is parsed on creation so the Project must have finished
     * building before creating a report generator.
     * @param project A Project instance
     */
    public ReportGenerator(Project project) throws IOException {
        this.project = project;
        this.resources = new HashMap<String, ResourceEntry>();
        this.excludedResources = new HashMap<String, ResourceEntry>();

        if (project.option("archive", "false").equals("true")) {
            ArchiveBuilder archiveBuilder = project.getArchiveBuilder();
            if (archiveBuilder != null) {
                parseArchiveBuilder(archiveBuilder);
            }
        } else {
            parseFromDisk();
        }

        parseFromPublisher(project.getPublisher());
    }

    /**
     * Gathers file sizes from files on disk
     */
    private void parseFromDisk() throws IOException {

        String rootDir = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());

        for (String output : project.getOutputs().keySet()) {

            String filePath = output.substring(rootDir.length());
            File file = new File(output);
            long size = file.length();

            if (this.resources.containsKey(filePath)) {
                ResourceEntry resEntry = this.resources.get(filePath);
                resEntry.size = size;
                resEntry.compressedSize = size;
                resEntry.encrypted = false;
            } else {
                ResourceEntry resEntry = new ResourceEntry(filePath, size);
                this.resources.put(filePath, resEntry);
            }

        }
    }

    private void parseFromPublisher(Publisher p) {
        Map<String, ArchiveEntry> entries = p.getEntries();
        for (String fileName : entries.keySet()) {
            ArchiveEntry archiveEntry = entries.get(fileName);
            long compressedSize = archiveEntry.isCompressed() ? archiveEntry.getCompressedSize() : archiveEntry.getSize();
            boolean encrypted = archiveEntry.isEncrypted();
            boolean excluded = true;

            ResourceEntry resEntry = new ResourceEntry(archiveEntry.getRelativeFilename(),
                        archiveEntry.getSize(),
                        compressedSize,
                        encrypted,
                        excluded);

            this.excludedResources.put(archiveEntry.getRelativeFilename(), resEntry);
        }
    }

    private void parseArchiveBuilder(ArchiveBuilder archiveBuilder) {
        List<ArchiveEntry> includedEntries = archiveBuilder.getIncludedEntries();
        List<ArchiveEntry> excludedEntries = archiveBuilder.getExcludedEntries();
        int padding = archiveBuilder.getResourcePadding();

        for (ArchiveEntry archiveEntry : includedEntries) {
            long compressedSize = 0;
            long size = 0;
            if (!archiveEntry.isDuplicatedDataBlob()) {
                compressedSize = archiveEntry.isCompressed() ? archiveEntry.getCompressedSize() : archiveEntry.getSize();
                compressedSize = ((compressedSize + padding - 1) / padding) * padding;
                size= archiveEntry.getSize();
            }
            boolean encrypted = archiveEntry.isEncrypted();
            boolean excluded = false;

            ResourceEntry resEntry = new ResourceEntry(archiveEntry.getRelativeFilename(),
                    size,
                    compressedSize,
                    encrypted,
                    excluded);

            this.resources.put(archiveEntry.getRelativeFilename(), resEntry);
        }

        for (ArchiveEntry archiveEntry : excludedEntries) {
            long compressedSize = 0;
            long size = 0;
            if (!archiveEntry.isDuplicatedDataBlob()) {
                compressedSize = archiveEntry.isCompressed() ? archiveEntry.getCompressedSize() : archiveEntry.getSize();
                size = archiveEntry.getSize();
            }
            boolean encrypted = archiveEntry.isEncrypted();
            boolean excluded = true;

            ResourceEntry resEntry = new ResourceEntry(archiveEntry.getRelativeFilename(),
                    size,
                    compressedSize,
                    encrypted,
                    excluded);

            this.excludedResources.put(archiveEntry.getRelativeFilename(), resEntry);
        }
    }

    private String generateJSON(HashMap<String, ResourceEntry> resources) throws IOException {
        StringWriter strWriter = new StringWriter();
        BufferedWriter writer = null;
        JsonGenerator generator = null;

        try {
            writer = new BufferedWriter(strWriter);
            generator = (new JsonFactory()).createJsonGenerator(writer);
            generator.useDefaultPrettyPrinter();

            // Get game.project properties
            BobProjectProperties projectProperties = project.getProjectProperties();

            // General/project info
            generator.writeStartObject();
            generator.writeFieldName("report_version");
            generator.writeString(REPORT_VERSION_NUMBER);
            generator.writeFieldName("project_name");
            generator.writeString(projectProperties.getStringValue("project", "title"));
            generator.writeFieldName("build_timestamp");
            generator.writeNumber(System.currentTimeMillis() / 1000L);

            // Create dictionary for build options
            generator.writeFieldName("build_options");
            generator.writeStartObject();
            String key;
            for (Map.Entry<String, String> entry : project.getOptions().entrySet() ) {
                key = entry.getKey();
                if (key.equals("auth") || key.equals("email"))
                {
                    continue;
                }
                generator.writeFieldName(key);
                generator.writeString(entry.getValue());
            }
            generator.writeEndObject();

            // Resources
            generator.writeFieldName("resources");
            generator.writeStartArray();
            for (Map.Entry<String, ResourceEntry> entry : resources.entrySet()) {
                ResourceEntry resource = entry.getValue();
                generator.writeStartObject();
                generator.writeFieldName("file");
                generator.writeString(resource.path);
                generator.writeFieldName("size");
                generator.writeNumber(resource.size);
                generator.writeFieldName("compressed_size");
                generator.writeNumber(resource.compressedSize);
                generator.writeFieldName("encrypted");
                generator.writeBoolean(resource.encrypted);
                generator.writeEndObject();
            }
            generator.writeEndArray();

            generator.writeEndObject();
        }
        finally {
            if (null != generator) {
                generator.close();
            }
            IOUtils.closeQuietly(writer);
        }
        return strWriter.toString();
    }


    /**
     * Generates and returns JSON, serialised as a string,
     * from previously gathered resource information.
     */
    public String generateResourceReportJSON() throws IOException {
        return generateJSON(resources);
    }

    /**
     * Generates and returns JSON, serialised as a string,
     * from previously gathered excluded resource information.
     */
    public String generateExcludedResourceReportJSON() throws IOException {
        return generateJSON(excludedResources);
    }

    /**
     * Generates and returns a HTML file, as a string (inlines the supplied
     * JSON report data).
     * @param jsonReportData JSON report data to be inlined
     */
    public String generateHTML(String jsonReportData) throws IOException {
        InputStream templateStream = Bob.class.getResourceAsStream("/lib/report_template.html");
        try {
            StringWriter writer = new StringWriter();
            IOUtils.copy(templateStream, writer);
            String templateString = writer.toString();

            HashMap<String, Object> ctx = new HashMap<String, Object>();
            ctx.put("json-data", jsonReportData);

            Template template = Mustache.compiler().compile(templateString);
            StringWriter sw = new StringWriter();
            template.execute(ctx, sw);
            sw.flush();

            return sw.toString();
        } catch (IOException e) {
            throw new IOException("Error while reading report template: " + e.toString());
        }
        finally {
            IOUtils.closeQuietly(templateStream);
        }
    }

}
