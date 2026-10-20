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

