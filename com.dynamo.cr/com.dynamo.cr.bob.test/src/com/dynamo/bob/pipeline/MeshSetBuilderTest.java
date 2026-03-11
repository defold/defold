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

package com.dynamo.bob.pipeline;

import static org.junit.Assert.assertTrue;
import static org.junit.Assert.assertFalse;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;

public class MeshSetBuilderTest extends AbstractProtoBuilderTest {
    @Before
    public void setup() {
        addTestFiles();
    }

    private GLTFValidator.ValidateResult parseValidatorResult(String errorText) {
        GLTFValidator.ValidateResult result = new GLTFValidator.ValidateResult();
        result.result = true;

        if (errorText == null) {
            return result;
        }

        String[] lines = errorText.split("\\r?\\n");
        for (String line : lines) {
            line = line.trim();
            if (!line.startsWith("- ")) {
                continue;
            }

            // Strip leading "- "
            String body = line.substring(2).trim();

            String message = body;
            String pointer = null;
            String code = null;

            int paren = body.indexOf(" (");
            if (paren != -1 && body.endsWith(")")) {
                message = body.substring(0, paren).trim();
                String inside = body.substring(paren + 2, body.length() - 1);
                String[] parts = inside.split(",\\s*");
                for (String part : parts) {
                    if (part.startsWith("pointer: ")) {
                        pointer = part.substring("pointer: ".length());
                    } else if (part.startsWith("code: ")) {
                        code = part.substring("code: ".length());
                    }
                }
            }

            GLTFValidator.ValidateError err = new GLTFValidator.ValidateError();
            err.message = message;
            err.pointer = pointer;
            err.code = code;
            result.errors.add(err);
        }

        if (!result.errors.isEmpty()) {
            result.result = false;
        }
        return result;
    }

    private GLTFValidator.ValidateResult doTest(String glTF) {
        String template =
                "name: \"invalid_gltf_model\"\n" +
                "mesh: \"%s\"\n" +
                "animations: \"%s\"\n" +
                "default_animation: \"invalid\"\n";
        try {
            build("/test_model.model", String.format(template, glTF, glTF));
            GLTFValidator.ValidateResult ok = new GLTFValidator.ValidateResult();
            ok.result = true;
            return ok;
        } catch (CompileExceptionError e) {
            return parseValidatorResult(e.getMessage());
        } catch (Exception e) {
            GLTFValidator.ValidateResult res = new GLTFValidator.ValidateResult();
            res.result = false;
            GLTFValidator.ValidateError err = new GLTFValidator.ValidateError();
            err.message = e.getMessage();
            res.errors.add(err);
            return res;
        }
    }

    @Test
    public void testValidGLTF() {
        GLTFValidator.ValidateResult res = doTest("/gltf/valid.gltf");
        assertTrue(res.result);
        assertTrue(res.errors.isEmpty());
    }

    @Test
    public void testInvalidGLTF() {
        GLTFValidator.ValidateResult res = doTest("/gltf/accessor_element_out_of_max_bound.gltf");
        assertFalse(res.result);
        assertFalse(res.errors.isEmpty());
    }
}
