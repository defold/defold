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

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.dynamo.bob.Bob;
import com.dynamo.bob.util.Exec;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonParseException;
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
        public ArrayList<ValidateError> errors = new ArrayList<ValidateError>();
    }

    public static ValidateResult validateGltf(String path) throws IOException {
        ValidateResult validateResult = new ValidateResult();
        validateResult.result = true;

        gltfValidatorExePath = Bob.getHostExeOnce("gltf_validator", gltfValidatorExePath);
        Exec.Result execResult = Exec.execResult(gltfValidatorExePath, "-o", path);

        String output = new String(execResult.stdOutErr);

        int errorCount = 0;
        Matcher headerMatcher = Pattern.compile("Errors:\\s*(\\d+)").matcher(output);
        if (headerMatcher.find()) {
            errorCount = Integer.parseInt(headerMatcher.group(1));
        } else if (execResult.ret != 0) {
            validateResult.result = false;
            ValidateError err = new ValidateError();
            err.message = output;
            validateResult.errors.add(err);
            return validateResult;
        }

        if (errorCount == 0) {
            return validateResult;
        }

        int jsonStart = output.indexOf('{');
        if (jsonStart == -1) {
            validateResult.result = false;
            ValidateError err = new ValidateError();
            err.message = output;
            validateResult.errors.add(err);
            return validateResult;
        }

        String json = output.substring(jsonStart);

        try {
            ObjectMapper mapper = new ObjectMapper();
            JsonNode root = mapper.readValue(
                    new InputStreamReader(new ByteArrayInputStream(json.getBytes("UTF-8")), "UTF-8"),
                    JsonNode.class);

            JsonNode issues = root.get("issues");
            if (issues == null || !issues.has("messages")) {
                validateResult.result = false;
                ValidateError err = new ValidateError();
                err.message = output;
                validateResult.errors.add(err);
                return validateResult;
            }

            JsonNode messages = issues.get("messages");
            if (messages != null && messages.isArray()) {
                for (JsonNode msgNode : messages) {
                    JsonNode severityNode = msgNode.get("severity");
                    JsonNode messageNode = msgNode.get("message");
                    if (severityNode == null || messageNode == null) {
                        continue;
                    }

                    int severity = severityNode.asInt();
                    if (severity == 0) {
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
            }

            validateResult.result = validateResult.errors.isEmpty();
            return validateResult;
        } catch (JsonParseException e) {
            validateResult.result = false;
            ValidateError err = new ValidateError();
            err.message = output;
            validateResult.errors.add(err);
            return validateResult;
        }
    }
}

