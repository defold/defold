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

package com.dynamo.bob.pipeline.shader;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.regex.Pattern;

// Shared by Bob and the offline graphics fixture generator, which runs this
// source directly with Java's source-file launcher without a Bob dependency.
final class WgslShader {
    private static final String WGSL_FLIPPED_VERTEX_ENTRY_POINT_MARKER = "// defold-webgpu-flipped-entry-point: ";
    private static final String WGSL_FLIPPED_VERTEX_ENTRY_POINT_BASE = "_defold_webgpu_main_flipped";

    static String addFlippedVertexEntryPoint(String source) {
        // WebGPU cannot emulate the negative-height viewport used by the engine to
        // preserve its render-target convention. Keep Tint's original entry point
        // for the backbuffer and add a flipped variant that the adapter can select
        // when rendering to an offscreen target.
        int vertexEntryPoint = source.lastIndexOf("@vertex");
        int entryPointFunction = source.indexOf("fn main(", vertexEntryPoint);
        int entryPointBody = source.indexOf('{', entryPointFunction);
        if (vertexEntryPoint == -1 || entryPointFunction == -1 || entryPointBody == -1) {
            throw new IllegalArgumentException("Unable to locate the generated WGSL vertex entry point");
        }

        int braceDepth = 0;
        int entryPointEnd = -1;
        for (int i = entryPointBody; i < source.length(); ++i) {
            char c = source.charAt(i);
            if (c == '{') {
                ++braceDepth;
            } else if (c == '}' && --braceDepth == 0) {
                entryPointEnd = i + 1;
                break;
            }
        }

        if (entryPointEnd == -1) {
            throw new IllegalArgumentException("Unable to locate the end of the generated WGSL vertex entry point");
        }

        String flippedEntryPoint = source.substring(vertexEntryPoint, entryPointEnd);
        int flippedFunction = flippedEntryPoint.indexOf("fn main(");
        int entryPointReturn = flippedEntryPoint.indexOf("  return ");
        if (entryPointReturn == -1 || flippedEntryPoint.indexOf("gl_Position") == -1) {
            throw new IllegalArgumentException("Unable to add the WebGPU vertex Y-flip entry point");
        }

        String flippedEntryPointName = WGSL_FLIPPED_VERTEX_ENTRY_POINT_BASE;
        while (Pattern.compile("\\b" + Pattern.quote(flippedEntryPointName) + "\\b").matcher(source).find()) {
            flippedEntryPointName += "_";
        }

        flippedEntryPoint = flippedEntryPoint.substring(0, flippedFunction) +
                            "fn " + flippedEntryPointName + "(" +
                            flippedEntryPoint.substring(flippedFunction + "fn main(".length());
        entryPointReturn = flippedEntryPoint.indexOf("  return ");
        flippedEntryPoint = flippedEntryPoint.substring(0, entryPointReturn) +
                            "  gl_Position.y = -gl_Position.y;\n" +
                            flippedEntryPoint.substring(entryPointReturn);

        return source.substring(0, entryPointEnd) + "\n\n" +
               WGSL_FLIPPED_VERTEX_ENTRY_POINT_MARKER + flippedEntryPointName + "\n" +
               flippedEntryPoint + source.substring(entryPointEnd);
    }

    public static void main(String[] args) throws IOException {
        if (args.length != 1) {
            System.err.println("Usage: java WgslShader.java <vertex.wgsl>");
            System.exit(1);
        }
        Path path = Path.of(args[0]);
        String source = Files.readString(path, StandardCharsets.UTF_8);
        Files.writeString(path, addFlippedVertexEntryPoint(source), StandardCharsets.UTF_8);
    }
}
