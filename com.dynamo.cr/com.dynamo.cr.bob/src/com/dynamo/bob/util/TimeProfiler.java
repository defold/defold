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

import com.dynamo.bob.Bob;

import java.io.File;
import java.io.InputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.io.BufferedWriter;
import java.lang.Thread;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.concurrent.ConcurrentHashMap;

import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

/**
 * Class helps to profile time of the Bob tool and generate report.
 */
public class TimeProfiler {

    private static final String FILENAME_POSTFIX = "_time";

    /**
     * Helper class that contains profiling data and represents a linked list of scopes hierarchy.
     */
    public static class ProfilingScope {
        public long startTime;
        public long endTime;
        HashMap<String, Object> data;

        public ProfilingScope parent;
        public ArrayList<ProfilingScope> children;

        public ProfilingScope() {}

        public void addData(String fieldName, Object value) {
            if (data == null) {
                data = new HashMap<>();
            }
            data.put(fieldName, value);
        }

        public void stop() {
            endTime = System.currentTimeMillis();
            TimeProfiler.setCurrentScope(this.parent);
        }
    }

    /**
     * Helper class that contains marks data.
     */
    private static class ProfilingMark {
        public String shortName;
        public String fullName;
        public String color;
        public long timestamp;
    }

    private static final ArrayList<ProfilingMark> marks = new ArrayList<>();
    private static long buildTime;
    private static final Map<Long, ProfilingScope> rootScopes = new ConcurrentHashMap<>();
    private static long mainThreadId;
    private static final Map<Long, ProfilingScope> currentScopes = new ConcurrentHashMap<>();
    private static List<File> reportFiles;

    private static long time() {
        return System.currentTimeMillis();
    }

    private static void generateJsonRecursively(JsonGenerator generator, ProfilingScope scope) throws IOException {
        generator.writeStartObject();
        generator.writeFieldName("start");
        generator.writeNumber(scope.startTime - buildTime);
        generator.writeFieldName("duration");
        generator.writeNumber(scope.endTime - scope.startTime);
        if (scope.data != null) {
            for (Map.Entry<String, Object> entry : scope.data.entrySet()) {
                generator.writeFieldName(entry.getKey());
                generator.writeObject(entry.getValue());
            }
        }
        if (scope.children != null) {
            generator.writeFieldName("children");
            generator.writeStartArray();
            for(ProfilingScope childScope : scope.children) {
                generateJsonRecursively(generator, childScope);
            }
            generator.writeEndArray();
        }
        generator.writeEndObject();
    }

    private static void generateJSON(BufferedWriter writer) throws IOException {
        JsonGenerator generator = null;
        try {
            generator = (new JsonFactory()).createJsonGenerator(writer);
            generator.useDefaultPrettyPrinter();
            generator.writeStartObject();
            generator.writeFieldName("data");
            generator.writeStartArray();

            // Ensure the main thread root comes first
            if (rootScopes.containsKey(mainThreadId)) {
                generateJsonRecursively(generator, rootScopes.get(mainThreadId));
                rootScopes.remove(mainThreadId);
            }

            List<ProfilingScope> sortedScopes = new ArrayList<>(rootScopes.values());
            Collections.sort(sortedScopes, Comparator.comparing((ProfilingScope scope) -> (String)scope.data.get("name")));
            for (ProfilingScope scope : sortedScopes) {
                generateJsonRecursively(generator, scope);
            }

            generator.writeEndArray();
            generator.writeFieldName("marks");
            generator.writeStartArray();
            for(ProfilingMark mark : marks) {
                generator.writeStartObject();
                generator.writeFieldName("shortName");
                generator.writeString(mark.shortName);
                generator.writeFieldName("fullName");
                generator.writeString(mark.fullName);
                generator.writeFieldName("color");
                generator.writeString(mark.color);
                generator.writeFieldName("timestamp");
                generator.writeNumber(mark.timestamp - buildTime);
                generator.writeEndObject();
            }
            generator.writeEndArray();
            generator.writeEndObject();
        } finally {
            if (generator != null) {
                generator.flush();
            }
        }
    }

    public static String[] getHtmlContent() throws IOException {
        InputStream templateStream = Bob.class.getResourceAsStream("/lib/time_report_template.html");
        try {
            StringWriter writer = new StringWriter();
            IOUtils.copy(templateStream, writer);
            String templateString = writer.toString();
            String token = "{{json-data}}";
            int tokenIndex = templateString.indexOf(token);
            String beforeToken = templateString.substring(0, tokenIndex - 1);
            String afterToken = templateString.substring(tokenIndex + token.length()+1);
            return new String[]{beforeToken, afterToken};
        } catch (IOException e) {
            throw new IOException("Error while reading time report template: " + e.toString());
        }
        finally {
            IOUtils.closeQuietly(templateStream);
        }
    }

    public static void createReport() {
        if (reportFiles == null) {
            return;
        }
        long reportStartTime = time();

        for (Map.Entry<Long, ProfilingScope> entry : rootScopes.entrySet()) {
            ProfilingScope rootScope = entry.getValue();
            rootScope.endTime = reportStartTime;
        }

        // Close all unclosed scopes
        for (ProfilingScope scope : currentScopes.values()) {
            while (scope != null && scope.endTime == 0) {
                scope.addData("forceFinishedScope", true);
                scope.addData("color", "#FF0000");
                scope.endTime = time();
                scope = scope.parent;
            }
        }

        try {
            // save report files, add '_time' to the given filenames
            // foo.json -> foo_time.json
            for (File reportFile : reportFiles) {
                String reportFileName = reportFile.getName();
                String extension = "." + FilenameUtils.getExtension(reportFileName);
                String finalReportFileName = reportFileName.replace(extension, FILENAME_POSTFIX + extension);
                File finalReportFile = new File(reportFile.getParent(), finalReportFileName);
                File parentDir = finalReportFile.getParentFile();
                if (parentDir != null && !parentDir.exists()) {
                    if (!parentDir.mkdirs()) {
                        System.out.println("Failed to create directories: " + parentDir);
                    }
                }
                int bufferSize = 8 * 1024 * 1024; // 8 MB in bytes
                try (BufferedWriter writer = new BufferedWriter(new FileWriter(finalReportFile), bufferSize)) {
                    if (extension.equals(".json")) {
                        generateJSON(writer);
                    } else if (extension.equals(".html")) {
                        String[] htmlContent = getHtmlContent();
                        writer.write(htmlContent[0]);
                        writer.flush();
                        generateJSON(writer);
                        writer.flush();
                        writer.write(htmlContent[1]);
                    } else {
                        System.err.println("Report file " + reportFileName + "has unsupported extension");
                    }
                    writer.flush();
                    IOUtils.closeQuietly(writer);
                }
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            marks.clear();
            currentScopes.clear();
            rootScopes.clear();
            reportFiles = null;
        }

        long reportEndTime = time();
        System.out.printf("Time profiler report creation took %.2f seconds\n", (reportEndTime - reportStartTime)/1000.0f);
    }

    private static long getCurrentThreadId() {
        return Thread.currentThread().getId();
    }

    private static void setCurrentScope(ProfilingScope scope) {
        currentScopes.put(getCurrentThreadId(), scope);
    }

    public static ProfilingScope getCurrentScope() {
        long threadId = getCurrentThreadId();
        ProfilingScope profilingScope = currentScopes.get(getCurrentThreadId());
        if (profilingScope == null) {
            ProfilingScope newRoot = new ProfilingScope();
            newRoot.startTime = time(); // Initialize properly
            newRoot.addData("name", "Thread " + threadId);
            newRoot.children = new ArrayList<>();
            rootScopes.put(threadId, newRoot);
            setCurrentScope(newRoot);
            profilingScope = newRoot;
        }
        return profilingScope;
    }

    public static void init(List<File> reportFiles) throws IOException {
        if (TimeProfiler.reportFiles != null) {
            throw new RuntimeException("TimeProfiler.init() called while profiling was already in progress");
        }
        TimeProfiler.reportFiles = reportFiles;

        long startTime = time();
        buildTime = startTime;
        mainThreadId = getCurrentThreadId();

        ProfilingScope mainRoot = new ProfilingScope();
        mainRoot.startTime = startTime;
        mainRoot.addData("name", "Main thread");
        rootScopes.put(mainThreadId, mainRoot);
        setCurrentScope(mainRoot);

        ProfilingScope initScope = new ProfilingScope();
        initScope.addData("name", "Time Profiler init");
        initScope.startTime = startTime;
        initScope.endTime = time();
        mainRoot.children = new ArrayList<>();
        mainRoot.children.add(initScope);
        initScope.parent = mainRoot;
    }

    public static void  addScopeToCurrentThread(ProfilingScope scope) {
        if (scope == null) {
            return;
        }
        ProfilingScope currentScope = getCurrentScope();
        if (currentScope.children == null) {
            currentScope.children = new ArrayList<>();
        }
        currentScope.children.add(scope);
        scope.parent = currentScope;
    }

    public static void start() {
        start(null);
    }

    public static void start(String scopeName) {
        ProfilingScope scope = new ProfilingScope();
        scope.startTime = time();
        if (scopeName != null) {
            scope.addData("name", scopeName);
        }
        addScopeToCurrentThread(scope);
        setCurrentScope(scope);
    }

    public static void start(String fmt, Object... args) {
        start(String.format(fmt, args));
    }

    public static void stop() {
        final ProfilingScope scope = getCurrentScope();
        if (scope != null) scope.stop();
    }

    public static void addData(String fieldName, Object data) {
        final ProfilingScope scope = getCurrentScope();
        if (scope != null) scope.addData(fieldName, data);
    }

    public static void addMark(String shortName, String fullName, String color) {
        ProfilingMark mark = new ProfilingMark();
        mark.timestamp = time();
        mark.shortName = shortName;
        mark.fullName = fullName;
        mark.color = color;
        marks.add(mark);
    }

    public static void addMark(String shortName) {
        addMark(shortName, shortName, "#EADDCA");
    }

    public static void addMark(String shortName, String fullName) {
        addMark(shortName, shortName, "#EADDCA");
    }

}
