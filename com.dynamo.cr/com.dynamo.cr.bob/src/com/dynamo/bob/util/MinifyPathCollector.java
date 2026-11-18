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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.fs.DefaultResource;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.fasterxml.jackson.databind.SerializationFeature;
import org.apache.commons.io.FilenameUtils;

import java.io.File;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.zip.CRC32;

public class MinifyPathCollector {
    public static final String DATA_FILE_NAME = "minified_paths.json";

    public static void saveAsJson(IResource outputResource, IResource... buildArtifacts) throws IOException {
        Map<String, String> paths = ResourceUtil.snapshotMinifiedPaths();
        if (paths.isEmpty()) {
            outputResource.remove();
            return;
        }

        Map<String, Object> data = new HashMap<>();
        String buildDir = ResourceUtil.getBuildDirectory();
        if (buildDir != null) {
            data.put("build_directory", buildDir);
        }
        data.put("paths", paths);

        Map<String, String> artifactChecksums = new HashMap<>();
        for (IResource artifact : buildArtifacts) {
            artifactChecksums.put(FilenameUtils.getName(artifact.getPath()), checksumArtifact(artifact));
        }
        data.put("build_artifacts", artifactChecksums);

        ObjectMapper mapper = new ObjectMapper().enable(SerializationFeature.INDENT_OUTPUT);
        outputResource.setContent(mapper.writeValueAsBytes(data));
    }

    public static byte[] readAndValidate(IResource mappingResource, IResource... buildArtifacts) throws IOException, CompileExceptionError {
        byte[] content = mappingResource.getContent();
        if (content == null) {
            return null;
        }

        JsonNode data;
        try {
            data = new ObjectMapper().readTree(content);
        } catch (JsonProcessingException e) {
            throw new CompileExceptionError(mappingResource, 0, "Invalid minified paths mapping. Run 'build' with '--archive' before bundling.", e);
        }
        JsonNode artifactChecksums = data == null ? null : data.get("build_artifacts");
        for (IResource artifact : buildArtifacts) {
            String name = FilenameUtils.getName(artifact.getPath());
            String checksum = checksumArtifact(artifact);
            if (checksum == null || artifactChecksums == null || !checksum.equals(artifactChecksums.path(name).asText())) {
                throw new CompileExceptionError(mappingResource, 0, String.format(
                        "Minified paths mapping is stale or missing a build fingerprint for '%s'. Run 'build' with '--archive' before bundling.", name));
            }
        }
        return content;
    }

    private static String checksumArtifact(IResource artifact) throws IOException {
        if (artifact instanceof DefaultResource) {
            File file = new File(artifact.getAbsPath());
            return file.isFile() ? Long.toHexString(FileUtil.calculateCrc32(file).getValue()) : null;
        }

        byte[] content = artifact.getContent();
        if (content == null) {
            return null;
        }
        CRC32 checksum = new CRC32();
        checksum.update(content);
        return Long.toHexString(checksum.getValue());
    }
}

