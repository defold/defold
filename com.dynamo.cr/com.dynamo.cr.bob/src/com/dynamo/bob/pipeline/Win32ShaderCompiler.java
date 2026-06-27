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

import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Platform;
import com.dynamo.bob.bundle.BundlerParams;
import com.dynamo.bob.pipeline.shader.ShaderCompilePipeline;

@BundlerParams(platforms = {"x86_64-win32", "x86-win32"})
public class Win32ShaderCompiler implements IShaderCompiler {

    public int getPriority() {
        return 100;
    }

    private static boolean isWin32Target(Platform platform) {
        return platform == Platform.X86Win32 || platform == Platform.X86_64Win32;
    }

    public boolean configure(Platform platform, ShaderCompilePipeline.Options options) {
        if (!isWin32Target(platform)) {
            return false;
        }
        // Keep Win32 builds on the in-process shader path for now.
        options.externalToolPath = null;
        options.externalToolArgs = null;
        return true;
    }

    private static Platform resolvePlatform(CompileOptions compileOptions) {
        if (compileOptions != null && compileOptions.platform != null) {
            Platform parsed = Platform.get(compileOptions.platform);
            if (isWin32Target(parsed)) {
                return parsed;
            }
        }
        return Platform.X86_64Win32;
    }

    static String ensureInputAssemblerRootFlag(String rootSignatureText) {
        if (rootSignatureText == null || rootSignatureText.isEmpty()) {
            return rootSignatureText;
        }

        final String iaToken = "ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT";
        if (rootSignatureText.contains(iaToken)) {
            return rootSignatureText;
        }

        final String rootFlagsPrefix = "RootFlags(";
        int rootFlagsStart = rootSignatureText.indexOf(rootFlagsPrefix);
        if (rootFlagsStart >= 0) {
            int flagsValueStart = rootFlagsStart + rootFlagsPrefix.length();
            int flagsValueEnd = rootSignatureText.indexOf(')', flagsValueStart);
            if (flagsValueEnd < 0) {
                return rootSignatureText;
            }

            String existingFlags = rootSignatureText.substring(flagsValueStart, flagsValueEnd).trim();
            String mergedFlags = existingFlags.isEmpty() ? iaToken : existingFlags + "|" + iaToken;
            return rootSignatureText.substring(0, flagsValueStart) + mergedFlags + rootSignatureText.substring(flagsValueEnd);
        }

        final String suffix = "\")]";
        if (!rootSignatureText.endsWith(suffix)) {
            return rootSignatureText;
        }

        String body = rootSignatureText.substring(0, rootSignatureText.length() - suffix.length());
        boolean hasParameters = !rootSignatureText.equals("[RootSignature(\"\")]");
        return body + (hasParameters ? "," : "") + "RootFlags(" + iaToken + ")" + suffix;
    }

    @Override
    public ShaderProgramBuilder.ShaderCompileResult compile(ArrayList<ShaderCompilePipeline.ShaderModuleDesc> shaderModules, String resourceOutputPath, CompileOptions compileOptions) throws IOException, CompileExceptionError {
        Platform targetPlatform = resolvePlatform(compileOptions);
        ShaderCompilePipeline.Options pipelineOptions = new ShaderCompilePipeline.Options();
        configure(targetPlatform, pipelineOptions);

        IShaderCompiler commonCompiler = ShaderCompilers.GetCommonShaderCompiler(targetPlatform, pipelineOptions);
        if (commonCompiler == null) {
            throw new CompileExceptionError(String.format("No common shader compiler available for platform '%s'", targetPlatform.getPair()));
        }
        return commonCompiler.compile(shaderModules, resourceOutputPath, compileOptions);
    }
}
