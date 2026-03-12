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
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.Bob;
import com.dynamo.bob.util.Exec;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.map.ObjectMapper;

public class GLTFValidator {
    private static String gltfValidatorExePath;

    public static class ValidateError {
        public String message;
        public String pointer;
        public String code;
    }

    public static class ValidateResult {
        public boolean result;
        public ArrayList<ValidateError> errors = new ArrayList<>();
    }

    // Called from editor
    public static ValidateResult validateGltf(InputStream stream) throws IOException {
        byte[] content = stream.readAllBytes();
        return validateGltf(content);
    }

    public static ValidateResult validateGltf(byte[] content) throws IOException {
        ValidateResult validateResult = new ValidateResult();
        validateResult.result = true;

        // Write the provided content to a temporary file so we can pass a real path
        // to the gltf_validator executable.
        File tmpGltfFile = File.createTempFile("gltf_tmp", ".gltf", Bob.getRootFolder());
        tmpGltfFile.deleteOnExit();

        try (BufferedOutputStream os = new BufferedOutputStream(new FileOutputStream(tmpGltfFile))) {
            os.write(content);
        }

        gltfValidatorExePath = Bob.getHostExeOnce("gltf_validator", gltfValidatorExePath);
        Exec.Result execResult = Exec.execResult(gltfValidatorExePath, "-o", tmpGltfFile.getAbsolutePath());
        String output = new String(execResult.stdOutErr);

        // Parse the summary header line: "Errors: N, Warnings: ..., Infos: ..., Hints: ..."
        Matcher headerMatcher = Pattern.compile("^Errors:\\s*(\\d+)", Pattern.MULTILINE).matcher(output);
        int errorCount = 0;
        if (headerMatcher.find()) {
            errorCount = Integer.parseInt(headerMatcher.group(1));
        } else {
            ValidateError err = new ValidateError();
            err.message = output;
            validateResult.errors.add(err);
            validateResult.result = false;
            return validateResult;
        }

        if (errorCount == 0) {
            // No errors reported; warnings/infos/hints are allowed.
            return validateResult;
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
                ValidateError err = new ValidateError();
                err.message = messageNode.asText();
                JsonNode pointerNode = msgNode.get("pointer");
                JsonNode codeNode = msgNode.get("code");

                if (pointerNode != null && !pointerNode.asText().isEmpty()) {
                    err.pointer = pointerNode.asText();
                }
                if (codeNode != null && !codeNode.asText().isEmpty()) {
                    err.code = codeNode.asText();
                }
                validateResult.errors.add(err);
            }
        }

        validateResult.result = validateResult.errors.isEmpty();
        return validateResult;
    }
}

