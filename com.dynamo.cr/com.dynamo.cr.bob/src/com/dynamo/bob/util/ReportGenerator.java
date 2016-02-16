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
import com.dynamo.bob.util.BobProjectProperties;
import com.dynamo.bob.Project;
import com.dynamo.bob.archive.ArchiveReader;
import com.dynamo.bob.archive.ArchiveEntry;

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
     * We need our own structure here since they can be filled by either parsing from
     * DARC or disk.
     */
    private class ResourceEntry {
        String path;
        long size;
        long compressedSize;
        boolean encrypted;
        public ResourceEntry(String path, long size) {
            this.path = path;
            this.size = size;
            this.compressedSize = size;
            this.encrypted = false;
        }
        public ResourceEntry(String path, long size, long compressedSize, boolean encrypted) {
            this.path = path;
            this.size = size;
            this.compressedSize = compressedSize;
            this.encrypted = encrypted;
        }
    }

    private Project project = null;
    private HashMap<String, ResourceEntry> resources;

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

        if (project.option("archive", "false").equals("true")) {
            parseFromDarc();
        } else {
            parseFromDisk();
        }
    }

    /**
     * Gathers file sizes from files on disk
     */
    private void parseFromDisk() throws IOException {

        String rootDir = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());

        for (String output : project.getOutputs()) {

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

    /**
     * Gathers file sizes (both compressed and uncompressed) and encryption flags
     * from file entries stored in a game.darc file.
     */
    private void parseFromDarc() throws IOException {

        String rootDir = FilenameUtils.concat(project.getRootDirectory(), project.getBuildDirectory());
        String darcPath = FilenameUtils.concat(rootDir, "game.darc");

        ArchiveReader ar = new ArchiveReader(darcPath);
        ar.read();

        List<ArchiveEntry> archiveEntries = ar.getEntries();
        for (int i = 0; i < archiveEntries.size(); i++) {
            ArchiveEntry archiveEntry = archiveEntries.get(i);

            boolean encrypted = (archiveEntry.flags & ArchiveEntry.FLAG_ENCRYPTED) == ArchiveEntry.FLAG_ENCRYPTED;

            if (this.resources.containsKey(archiveEntry.fileName)) {
                ResourceEntry resEntry = this.resources.get(archiveEntry.fileName);
                resEntry.compressedSize = archiveEntry.compressedSize;
                resEntry.size = archiveEntry.size;
                resEntry.encrypted = encrypted;
            } else {
                ResourceEntry resEntry = new ResourceEntry(archiveEntry.fileName,
                        archiveEntry.size,
                        archiveEntry.compressedSize,
                        encrypted);

                this.resources.put(archiveEntry.fileName, resEntry);
            }

        }

        ar.close();
    }

    /**
     * Generates and returns JSON, serialised as a string,
     * from previously gathered report information.
     */
    public String generateJSON() throws IOException {

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
            for (Map.Entry<String, String> entry : project.getOptions().entrySet() ) {
                generator.writeFieldName(entry.getKey());
                generator.writeString(entry.getValue());
            }
            generator.writeEndObject();

            // Create dictionary for project properties
            generator.writeFieldName("project_properties");
            generator.writeStartObject();
            for (String categoryName : projectProperties.getCategoryNames()) {
                generator.writeFieldName(categoryName);

                generator.writeStartObject();
                for (String keyName : projectProperties.getKeys(categoryName)) {
                    generator.writeFieldName(keyName);
                    generator.writeString(projectProperties.getStringValue(categoryName, keyName));
                }
                generator.writeEndObject();
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
     * Generates and returns a HTML file, as a string (inlines the supplied
     * JSON report data).
     * @param jsonReportData JSON report data to be inlined
     */
    public String generateHTML(String jsonReportData) throws IOException {

        InputStream templateStream = Bob.class.getResourceAsStream("/lib/report_template.html");

        StringWriter writer = new StringWriter();
        try {
            IOUtils.copy(templateStream, writer);
        } catch (IOException e) {
            throw new IOException("Error while reading report template: " + e.toString());
        }
        String templateString = writer.toString();

        HashMap<String, Object> ctx = new HashMap<String, Object>();
        ctx.put("json-data", jsonReportData);

        Template template = Mustache.compiler().compile(templateString);
        StringWriter sw = new StringWriter();
        template.execute(ctx, sw);
        sw.flush();

        return sw.toString();
    }

}
