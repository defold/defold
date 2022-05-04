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

package com.dynamo.bob.util;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.StringWriter;
import java.io.BufferedWriter;

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.Date;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;

import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerator;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.FilenameUtils;
import org.apache.commons.io.IOUtils;

/**
 * Class helps to profile time of the Bob tool and generate report.
 */
public class TimeProfiler {

    private static final String FILENAME = "time_report";

    /**
     * Helper class that contains profiling data and represents a linked list of scopes hierarchy.
     */
    private static class ProfilingScope {
        public String name;
        public long startTime;
        public long endTime;
        HashMap<String, String> additionalStringData;
        HashMap<String, Float> additionalNumberData;
        HashMap<String, Boolean> additionalBooleanData;

        public ProfilingScope parent;
        public ArrayList<ProfilingScope> children;
    }

    public enum ReportFormat {
        JSON(".html"),
        HTML(".json");

        private String format;

        ReportFormat(String fileFormat) {
            this.format = fileFormat;
        }

        public String getFormat() {
            return format;
        }
    }

    private static ProfilingScope rootScope;
    private static ProfilingScope currentScope;

    private static long time() {
        return System.currentTimeMillis();
    }

    private static void generateJsonRecursively(JsonGenerator generator, ProfilingScope scope) throws IOException {
        generator.writeStartObject();
        generator.writeFieldName("name");
        generator.writeString(scope.name);
        generator.writeFieldName("startTime");
        generator.writeNumber(scope.startTime);
        generator.writeFieldName("endTime");
        generator.writeNumber(scope.endTime);
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

    private static String generateJSON() throws IOException {

        StringWriter strWriter = new StringWriter();
        BufferedWriter writer = null;
        JsonGenerator generator = null;

        try {
            writer = new BufferedWriter(strWriter);
            generator = (new JsonFactory()).createJsonGenerator(writer);
            generator.useDefaultPrettyPrinter();
            generateJsonRecursively(generator, rootScope);
        } finally {
            if (null != generator) {
                generator.close();
            }
            IOUtils.closeQuietly(writer);
        }

        return strWriter.toString();
    }

    public static void init(String reportFolderPath, ReportFormat format) {
        RuntimeMXBean bean = ManagementFactory.getRuntimeMXBean();
        long startTime = bean.getStartTime(); //Returns the start time of the Java virtual machine in milliseconds.
        rootScope = new ProfilingScope();
        rootScope.name = "Total time";
        rootScope.startTime = startTime;
        currentScope = rootScope;

        ProfilingScope initScope = new ProfilingScope();
        initScope.name = "Java VM init";
        initScope.startTime = startTime;
        initScope.endTime = time();
        rootScope.children = new ArrayList<ProfilingScope>();
        rootScope.children.add(initScope);
        initScope.parent = rootScope;

        Runtime.getRuntime().addShutdownHook(new Thread(new Runnable() {
            @Override
            public void run() {
                long reportStartTime = time();

                //Close all unclosed scopes
                while(currentScope != rootScope) {
                    addData("forceFinishedScope", true);
                    stop();
                };
                stop();

                try {
                    String jsonReport = generateJSON();
                    FileWriter fileJSONWriter = null;
                    File reportJSONFile = new File(reportFolderPath, FILENAME + format.getFormat());
                    fileJSONWriter = new FileWriter(reportJSONFile);
                    fileJSONWriter.write(jsonReport);
                    fileJSONWriter.close();
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }

                long reportEndTime = time();
                System.out.printf("\nTime profiler report creation took %.2f seconds", (reportEndTime - reportStartTime)/1000.0f);
            }
        }));
    }

    public static void start(String scopeName) {
        if (rootScope == null) {
            return;
        }
        if (currentScope.children == null) {
            currentScope.children = new ArrayList<ProfilingScope>();
        }
        ProfilingScope scope = new ProfilingScope();
        scope.name = scopeName;
        scope.startTime = time();
        scope.parent = currentScope;
        currentScope.children.add(scope);
        currentScope = scope;
    }

    public static void stop() {
        if (rootScope == null) {
            return;
        }
        currentScope.endTime = time();
        currentScope = currentScope.parent;
    }

    public static void addData(String fieldName, String data) {
        if (rootScope == null) {
            return;
        }
        if (currentScope.additionalStringData == null) {
            currentScope.additionalStringData = new HashMap<String, String>();
        }
        currentScope.additionalStringData.put(fieldName, data);
    }

    public static void addData(String fieldName, Float data) {
        if (rootScope == null) {
            return;
        }
        if (currentScope.additionalNumberData == null) {
            currentScope.additionalNumberData = new HashMap<String, Float>();
        }
        currentScope.additionalNumberData.put(fieldName, data);
    }

    public static void addData(String fieldName, Boolean data) {
        if (rootScope == null) {
            return;
        }
        if (currentScope.additionalBooleanData == null) {
            currentScope.additionalBooleanData = new HashMap<String, Boolean>();
        }
        currentScope.additionalBooleanData.put(fieldName, data);
    }

    public static void addData(String fieldName, Integer data) {
        addData(fieldName, data.floatValue());
    }

}
