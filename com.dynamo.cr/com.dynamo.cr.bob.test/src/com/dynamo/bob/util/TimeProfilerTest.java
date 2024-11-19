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
        var htmlReport = Files.createTempFile("time-profiler-test", "report.html");
        var result = Bob.invoke(null, new ConsoleProgress(), null, new String[]{
                "--root", cwd + "/test/time_profiler_project",
                "--build-report-json", jsonReport.toString(),
                "--build-report-html", htmlReport.toString(),
                "--texture-compression", "true",
                "--strip-executable",
                "--verbose",
                "--variant", "release",
                "--archive",
                "distclean", "resolve", "build", "bundle"
        });
        assertTrue(result.success);
        var jsonReportData = new ObjectMapper().readValue(jsonReport.toFile(), Map.class);
        assertNotNull(jsonReportData.get("build_options"));
        assertFalse(((List<?>) jsonReportData.get("resources")).isEmpty());

        var jsonTimeReport = jsonReport.getParent().resolve(jsonReport.getFileName().toString().replace("report.json", "report_time.json"));
        var jsonTimeReportData = new ObjectMapper().readValue(jsonTimeReport.toFile(), Map.class);
        assertFalse(((List<?>) jsonTimeReportData.get("data")).isEmpty());

        var htmlTimeReport = htmlReport.getParent().resolve(htmlReport.getFileName().toString().replace("report.html", "report_time.html"));
        assertTrue(Files.exists(htmlTimeReport));
    }

    @Test
    public void testMultipleBuildReportsInTheSameJVM() throws Exception {
        var cwd = System.getProperty("user.dir");
        var jsonReport1 = Files.createTempFile("time-profiler-test", "report.json");
        var jsonReport2 = Files.createTempFile("time-profiler-test", "report.json");
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
        assertTrue(Files.exists(jsonReport1));
        assertTrue(Files.exists(jsonReport2));
    }
}
