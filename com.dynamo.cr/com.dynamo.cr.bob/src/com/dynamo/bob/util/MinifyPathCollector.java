package com.dynamo.bob.util;

import com.dynamo.bob.fs.ResourceUtil;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

public class MinifyPathCollector {
    private static final String DATA_FILE_NAME = "minified_paths.json";

    public static void saveAsJson(File bundleOutputDirectory) {
        Map<String, String> paths = ResourceUtil.snapshotMinifiedPaths();
        if (paths == null || paths.isEmpty()) {
            return;
        }

        Map<String, Object> data = new HashMap<>();
        String buildDir = ResourceUtil.getBuildDirectory();
        if (buildDir != null) {
            data.put("build_directory", buildDir);
        }
        data.put("paths", paths);

        File outputFile = new File(bundleOutputDirectory, DATA_FILE_NAME);
        try (FileWriter writer = new FileWriter(outputFile)) {
            ObjectMapper mapper = new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);
            mapper.writeValue(writer, data);
        } catch (IOException e) {
            throw new RuntimeException("Failed to write build paths JSON", e);
        }
    }
}

