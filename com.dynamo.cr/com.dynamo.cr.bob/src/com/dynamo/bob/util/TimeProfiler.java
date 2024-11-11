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

package com.dynamo.bob.util;

import com.dynamo.bob.Bob;

import java.io.File;
import java.io.InputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.io.BufferedWriter;

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.lang.Thread;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;

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
        HashMap<String, String> additionalStringData;
        HashMap<String, Float> additionalNumberData;
        HashMap<String, Boolean> additionalBooleanData;

        public ProfilingScope parent;
        public ArrayList<ProfilingScope> children;

        public ProfilingScope() {}

        public void addData(String fieldName, String data) {
            if (additionalStringData == null) {
                additionalStringData = new HashMap<>();
            }
            additionalStringData.put(fieldName, data);
        }

        public void addData(String fieldName, Float data) {
            if (additionalNumberData == null) {
                additionalNumberData = new HashMap<>();
            }
            additionalNumberData.put(fieldName, data);
        }

        public void addData(String fieldName, Boolean data) {
            if (additionalBooleanData == null) {
                additionalBooleanData = new HashMap<>();
            }
            additionalBooleanData.put(fieldName, data);
        }

        public void addData(String fieldName, int data) {
            if (additionalNumberData == null) {
                additionalNumberData = new HashMap<>();
            }
            additionalNumberData.put(fieldName, (float)data);
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

    private static ArrayList<ProfilingMark> marks = new ArrayList();
    private static long buildTime;

    private static ProfilingScope rootScope;
    private static Map<Long, ProfilingScope> currentScopes = new HashMap<>();
    private static List<File> reportFiles;
    private static Boolean fromEditor;

    private static final long MAIN_THREAD_ID = -1;

    private static long time() {
        return System.currentTimeMillis();
    }

    private static void generateJsonRecursively(JsonGenerator generator, ProfilingScope scope) throws IOException {
        generator.writeStartObject();
        generator.writeFieldName("start");
        generator.writeNumber(scope.startTime - buildTime);
        generator.writeFieldName("duration");
        generator.writeNumber(scope.endTime - scope.startTime);
        if (scope.additionalStringData != null) {
            for (Map.Entry<String, String> entry : scope.additionalStringData.entrySet())  {
                generator.writeFieldName(entry.getKey());
                generator.writeString(entry.getValue());
            }
        }
        if (scope.additionalNumberData != null) {
            for (Map.Entry<String, Float> entry : scope.additionalNumberData.entrySet())  {
                generator.writeFieldName(entry.getKey());
                generator.writeNumber(entry.getValue());
            }
        }
        if (scope.additionalBooleanData != null) {
            for (Map.Entry<String, Boolean> entry : scope.additionalBooleanData.entrySet())  {
                generator.writeFieldName(entry.getKey());
                generator.writeBoolean(entry.getValue());
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

    private static void generateJSON(ProfilingScope scope, BufferedWriter writer) throws IOException {

        JsonGenerator generator = null;
        try {
            generator = (new JsonFactory()).createJsonGenerator(writer);
            generator.useDefaultPrettyPrinter();
            generator.writeStartObject();
            generator.writeFieldName("data");
            generator.writeStartArray();
            generateJsonRecursively(generator, scope);
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
            if (null != generator) {
                generator.flush();
            }
        }
    }

    public static String[] getHtmlContent() throws IOException {
        InputStream templateStream = Bob.class.getResourceAsStream("/lib/time_report_template.html");

        StringWriter writer = new StringWriter();
        try {
            IOUtils.copy(templateStream, writer);
        } catch (IOException e) {
            throw new IOException("Error while reading time report template: " + e.toString());
        }
        String templateString = writer.toString();
        String token = "{{json-data}}";
        int tokenIndex = templateString.indexOf(token);
        String beforeToken = templateString.substring(0, tokenIndex - 1);
        String afterToken = templateString.substring(tokenIndex + token.length()+1);
        return new String[]{beforeToken, afterToken};
    }

    public static void createReport(Boolean fromEditor) {
        // avoid double creation of the report by checking `fromEditor` flag
        if (rootScope == null || TimeProfiler.fromEditor != fromEditor) {
            return;
        }
        ProfilingScope _rootScope = rootScope;
        // Make sure that using of TimeProfiler is impossible from now on
        rootScope = null;
        long reportStartTime = time();

        // Close all unclosed scopes
        for (ProfilingScope scope : currentScopes.values()) {
            while (scope != null) {
                scope.addData("forceFinishedScope", true);
                scope.addData("color", "#FF0000");
                scope.endTime = time();
                scope = scope.parent;
            }
        }
        _rootScope.stop();

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
                        generateJSON(_rootScope, writer);
                    } else if (extension.equals(".html")) {
                        String[] htmlContent = getHtmlContent();
                        writer.write(htmlContent[0]);
                        writer.flush();
                        generateJSON(_rootScope, writer);
                        writer.flush();
                        writer.write(htmlContent[1]);
                    } else {
                        System.err.println("Report file " + reportFileName + "has unsupported extension");
                    }
                    writer.flush();
                    IOUtils.closeQuietly(writer);
                }
                catch (IOException e) {
                    throw e;
                }
            }
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        long reportEndTime = time();
        System.out.printf("\nTime profiler report creation took %.2f seconds", (reportEndTime - reportStartTime)/1000.0f);
    }

    private static long getCurrentThreadId() {
        Thread t = Thread.currentThread();
        return (t != null) ? t.getId() : MAIN_THREAD_ID;
    }

    private static void setCurrentScope(ProfilingScope scope) {
        currentScopes.put(getCurrentThreadId(), scope);
    }

    public static ProfilingScope getCurrentScope() {
        return currentScopes.get(getCurrentThreadId());
    }

    public static void init(List<File> reportFiles, Boolean fromEditor) throws IOException {
        if (rootScope != null) {
            throw new RuntimeException("TimeProfiler.init() called more than once");
        }
        if (!currentScopes.isEmpty()) {
            throw new RuntimeException("TimeProfiler.init() called after scopes were added");
        }
        TimeProfiler.reportFiles = reportFiles;
        TimeProfiler.fromEditor = fromEditor;
        currentScopes.clear();
        marks.clear();
        long startTime = time();
        if (!fromEditor) {
            RuntimeMXBean bean = ManagementFactory.getRuntimeMXBean();
            startTime = bean.getStartTime(); //Returns the start time of the Java virtual machine in milliseconds.
        }
        buildTime = startTime;
        rootScope = new ProfilingScope();
        rootScope.startTime = startTime;
        rootScope.addData("name", "Total time");
        setCurrentScope(rootScope);

        if (!fromEditor) {
            ProfilingScope initScope = new ProfilingScope();
            initScope.additionalStringData = new HashMap<String, String>();
            initScope.additionalStringData.put("name", "Java VM init");
            initScope.startTime = startTime;
            initScope.endTime = time();
            rootScope.children = new ArrayList<ProfilingScope>();
            rootScope.children.add(initScope);
            initScope.parent = rootScope;
        }

        Runtime.getRuntime().addShutdownHook(new Thread(new Runnable() {
            @Override
            public void run() {
                createReport(fromEditor);
            }
        }));
    }

    public static void addScopeToCurrentThread(ProfilingScope scope) {
        ProfilingScope currentScope = getCurrentScope();
        if (currentScope != null) {
            if (currentScope.children == null) {
                currentScope.children = new ArrayList<ProfilingScope>();
            }
            currentScope.children.add(scope);
        }
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

    public static void addData(String fieldName, String data) {
        final ProfilingScope scope = getCurrentScope();
        if (scope != null) scope.addData(fieldName, data);
    }

    public static void addData(String fieldName, Float data) {
        final ProfilingScope scope = getCurrentScope();
        if (scope != null) scope.addData(fieldName, data);
    }

    public static void addData(String fieldName, Boolean data) {
        final ProfilingScope scope = getCurrentScope();
        if (scope != null) scope.addData(fieldName, data);
    }

    public static void addData(String fieldName, int data) {
        final ProfilingScope scope = getCurrentScope();
        if (scope != null) scope.addData(fieldName, (float)data);
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
