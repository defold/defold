// Copyright 2020-2026 The Defold Foundation
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

package com.dynamo.bob.pipeline;

import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Platform;
import com.dynamo.bob.util.Exec;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

public class GLTFValidator {
    private static String gltfValidatorExePath;

    public static record ValidateError(String message, String pointer, String code) {}

    public static record ValidateResult(boolean result, List<ValidateError> errors) {}

    // Called from editor
    public static ValidateResult validateGltf(InputStream stream, String suffix, boolean validateResources) throws IOException {
        byte[] content = stream.readAllBytes();
        return validateGltf(content, suffix, validateResources);
    }

    public static ValidateResult validateGltf(String path, boolean validateResources) throws IOException {
        ArrayList<ValidateError> errors = new ArrayList<>();
        boolean result = true;

        // gltf_validator is not supported on win32 (we can't build the gltf_validator binary for this platform).
        Platform platform = Platform.getHostPlatform();
        if (platform == Platform.X86Win32) {
            return new ValidateResult(result, errors);
        }

        String validateResourcesFlag = validateResources ? "--validate-resources" : "--no-validate-resources";

        gltfValidatorExePath = Bob.getHostExeOnce("gltf_validator", gltfValidatorExePath);
        Exec.Result execResult = Exec.execResult(gltfValidatorExePath, "-o", validateResourcesFlag, path);
        String output = new String(execResult.stdOutErr);

        // Parse the summary header line: "Errors: N, Warnings: ..., Infos: ..., Hints: ..."
        Matcher headerMatcher = Pattern.compile("^Errors:\\s*(\\d+)", Pattern.MULTILINE).matcher(output);
        int errorCount = 0;
        if (headerMatcher.find()) {
            errorCount = Integer.parseInt(headerMatcher.group(1));
        } else {
            ValidateError err = new ValidateError(output, null, null);
            errors.add(err);
            result = false;
            return new ValidateResult(result, errors);
        }

        if (errorCount == 0) {
            // No errors reported; warnings/infos/hints are allowed.
            return new ValidateResult(result, errors);
        }

        int jsonStart = output.indexOf('{');
        assert jsonStart != -1;

        String json = output.substring(jsonStart);
        ObjectMapper mapper = new ObjectMapper();

        InputStreamReader jsonInputStream = new InputStreamReader(new ByteArrayInputStream(json.getBytes(StandardCharsets.UTF_8)), StandardCharsets.UTF_8);
        JsonNode root = mapper.readValue(jsonInputStream, JsonNode.class);

        JsonNode issues = root.get("issues");
        JsonNode messages = issues.get("messages");
        for (JsonNode msgNode : messages) {
            JsonNode severityNode = msgNode.get("severity");
            JsonNode messageNode = msgNode.get("message");

            // 0: error, we only look at errors in validation.
            if (severityNode.asInt() == 0) {
                ValidateError err = new ValidateError(
                    messageNode.asText(),
                    msgNode.get("pointer").asText(),
                    msgNode.get("code").asText()
                );
                errors.add(err);
            }
        }

        result = errors.isEmpty();
        return new ValidateResult(result, errors);
    }

    public static ValidateResult validateGltf(byte[] content, String suffix, boolean validateResources) throws IOException {
        // Write the provided content to a temporary file so we can pass a real path
        // to the gltf_validator executable.
        File tmpGltfFile = File.createTempFile("gltf_tmp", "." + suffix, Bob.getRootFolder());
        try {
            try (BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(tmpGltfFile))) {
                os.write(content);
            }
            return validateGltf(tmpGltfFile.getAbsolutePath(), validateResources);
        } finally {
            tmpGltfFile.delete();
        }
    }
}

