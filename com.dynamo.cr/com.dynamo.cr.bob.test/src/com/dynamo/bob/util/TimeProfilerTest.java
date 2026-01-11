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
import com.dynamo.bob.ConsoleProgress;
import org.codehaus.jackson.map.ObjectMapper;
import org.junit.Test;

import java.nio.file.Files;
import java.util.List;
import java.util.Map;

import static org.junit.Assert.*;

public class TimeProfilerTest {
    @Test
    public void testBuildReportGeneration() throws Exception {
        var cwd = System.getProperty("user.dir");
        var jsonReport = Files.createTempFile("time-profiler-test", "report.json");
        var jsonTimeReport = jsonReport.getParent().resolve(jsonReport.getFileName().toString().replace("report.json", "report_time.json"));
        var htmlReport = Files.createTempFile("time-profiler-test", "report.html");
        var htmlTimeReport = htmlReport.getParent().resolve(htmlReport.getFileName().toString().replace("report.html", "report_time.html"));

        assertFalse(Files.exists(htmlTimeReport));
        assertFalse(Files.exists(jsonTimeReport));
        assertEquals(0, Files.size(jsonReport));
        assertEquals(0, Files.size(htmlReport));

        var result = Bob.invoke(null, new ConsoleProgress(), null, new String[]{
                "--root", cwd + "/test/time_profiler_project",
                "--build-report-json", jsonReport.toString(),
                "--build-report-html", htmlReport.toString(),
                "--texture-compression",
                "--strip-executable",
                "--verbose",
                "--variant", "release",
                "--archive",
                "distclean", "resolve", "build", "bundle"
        });

        assertTrue(result.success);
        assertTrue(Files.exists(htmlTimeReport));
        assertTrue(Files.exists(jsonTimeReport));
        assertTrue(Files.size(jsonReport) > 0);
        assertTrue(Files.size(htmlReport) > 0);

        var jsonReportData = new ObjectMapper().readValue(jsonReport.toFile(), Map.class);
        assertNotNull(jsonReportData.get("build_options"));
        assertFalse(((List<?>) jsonReportData.get("resources")).isEmpty());

        var jsonTimeReportData = new ObjectMapper().readValue(jsonTimeReport.toFile(), Map.class);
        assertFalse(((List<?>) jsonTimeReportData.get("data")).isEmpty());
    }

    @Test
    public void testMultipleBuildReportsInTheSameJVM() throws Exception {
        var cwd = System.getProperty("user.dir");
        var jsonReport1 = Files.createTempFile("time-profiler-test", "report.json");
        var jsonReport2 = Files.createTempFile("time-profiler-test", "report.json");
        assertEquals(0, Files.size(jsonReport1));
        assertEquals(0, Files.size(jsonReport2));
        var result1 = Bob.invoke(null, new ConsoleProgress(), null, new String[]{
                "--root", cwd + "/test/time_profiler_project",
                "--build-report-json", jsonReport1.toString(),
                "distclean", "resolve", "build"
        });
        assertTrue(result1.success);
        var result2 = Bob.invoke(null, new ConsoleProgress(), null, new String[]{
                "--root", cwd + "/test/time_profiler_project",
                "--build-report-json", jsonReport2.toString(),
                "distclean", "resolve", "build"
        });
        assertTrue(result2.success);
        assertTrue(Files.size(jsonReport1) > 0);
        assertTrue(Files.size(jsonReport2) > 0);
    }
}
