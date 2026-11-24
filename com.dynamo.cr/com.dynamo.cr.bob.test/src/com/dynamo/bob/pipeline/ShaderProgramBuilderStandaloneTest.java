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

import com.dynamo.bob.Platform;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.DefaultFileSystem;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Arrays;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

public class ShaderProgramBuilderStandaloneTest {
    @Rule
    public TemporaryFolder temporary = new TemporaryFolder();

    private String[] createProgram(String name, String color) throws Exception {
        Path root = temporary.newFolder(name).toPath();
        Path vertex = root.resolve("shader.vp");
        Path fragment = root.resolve("shader.fp");
        Files.writeString(vertex, "#version 450\nlayout(location=0) in vec4 position;\n" +
                "void main() { gl_Position = position; }\n");
        Files.writeString(fragment, "#version 450\n#include \"/color.glsl\"\n" +
                "layout(location=0) out vec4 color;\nvoid main() { color = shader_color(); }\n");
        Files.writeString(root.resolve("color.glsl"), "vec4 shader_color() { return vec4(" + color + "); }\n");
        return new String[] {vertex.toString(), fragment.toString(), root.resolve("shader.spc").toString(),
                Platform.getHostPlatform().getPair(), root.toString()};
    }

    private String[] batch(String[]... programs) {
        ArrayList<String> args = new ArrayList<>();
        args.add("--batch");
        for (String[] program : programs) {
            args.addAll(Arrays.asList(program));
        }
        return args.toArray(new String[0]);
    }

    private void compile(String[] args) throws Exception {
        try (Project project = new Project(new DefaultFileSystem(), temporary.getRoot().getPath(), "build")) {
            ShaderProgramBuilder.compileStandalonePrograms(project, args);
        }
    }

    // Shared initialization must preserve standalone output and isolate each program's includes and modules.
    @Test
    public void testBatchMatchesStandaloneWithDifferentContentRoots() throws Exception {
        String[] red = createProgram("red content", "1.0, 0.0, 0.0, 1.0");
        String[] green = createProgram("green content", "0.0, 1.0, 0.0, 1.0");
        compile(red);
        compile(green);
        byte[] expectedRed = Files.readAllBytes(Path.of(red[2]));
        byte[] expectedGreen = Files.readAllBytes(Path.of(green[2]));
        assertFalse(Arrays.equals(expectedRed, expectedGreen));
        Files.delete(Path.of(red[2]));
        Files.delete(Path.of(green[2]));

        compile(batch(red, green));
        assertArrayEquals(expectedRed, Files.readAllBytes(Path.of(red[2])));
        assertArrayEquals(expectedGreen, Files.readAllBytes(Path.of(green[2])));
    }

    // The original four-argument interface must still compile a single shader stage.
    @Test
    public void testSingleStageStandalone() throws Exception {
        String[] program = createProgram("single stage", "1.0, 0.0, 0.0, 1.0");
        compile(new String[] {program[1], program[2], program[3], program[4]});
        ShaderDesc desc = ShaderDesc.parseFrom(Files.readAllBytes(Path.of(program[2])));
        assertTrue(desc.getShadersCount() > 0);
        for (ShaderDesc.Shader shader : desc.getShadersList()) {
            assertEquals(ShaderDesc.ShaderType.SHADER_TYPE_FRAGMENT, shader.getShaderType());
        }
    }

    // A shader compilation error must propagate before any later program is written.
    @Test
    public void testBatchStopsOnCompileFailure() throws Exception {
        String[] first = createProgram("first", "1.0, 0.0, 0.0, 1.0");
        String[] invalid = createProgram("invalid", "1.0, 0.0, 0.0, 1.0");
        String[] later = createProgram("later", "0.0, 1.0, 0.0, 1.0");
        Files.writeString(Path.of(invalid[1]), "#version 450\nthis is not a shader\n");
        try {
            compile(batch(first, invalid, later));
            fail("Expected the invalid shader to fail the batch");
        } catch (com.dynamo.bob.CompileExceptionError expected) {
            assertTrue(Files.exists(Path.of(first[2])));
            assertFalse(Files.exists(Path.of(later[2])));
        }
    }

    // An incomplete batch must fail before compiling even its valid leading program.
    @Test
    public void testRejectsIncompleteBatchBeforeCompiling() throws Exception {
        String[] program = createProgram("incomplete batch", "1.0, 0.0, 0.0, 1.0");
        String[] args = batch(program, new String[] {"missing arguments"});
        try {
            compile(args);
            fail("Expected an incomplete batch to be rejected");
        } catch (IllegalArgumentException expected) {
            assertFalse(Files.exists(Path.of(program[2])));
        }
    }
}
