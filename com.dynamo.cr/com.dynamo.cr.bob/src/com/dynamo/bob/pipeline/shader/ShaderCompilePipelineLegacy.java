// Copyright 2020-2024 The Defold Foundation
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

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.ShaderCompilePipeline;
import com.dynamo.bob.pipeline.ShaderCompilerHelpers;

import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilePipelineLegacy extends ShaderCompilePipeline {

	public ShaderCompilePipelineLegacy(String pipelineName) {
        super(pipelineName);
    }

    private String getShaderSource(ShaderDesc.ShaderType shaderType) {
        for (ShaderModule m : this.shaderModules) {
            if (m.type == shaderType) {
                return m.source;
            }
        }
        return null;
    }

    private boolean isLanguageClassGLSL(ShaderDesc.Language shaderLanguage) {
        return shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM100 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM120 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM140 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLES_SM300 ||
               shaderLanguage == ShaderDesc.Language.LANGUAGE_GLSL_SM430;
    }

    @Override
    protected void prepare() throws IOException, CompileExceptionError {
        // We can't run the prepare step on the legacy pipeline
    }

    @Override
    public byte[] crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws IOException, CompileExceptionError {
        String source = getShaderSource(shaderType);
        if (source == null) {
            throw new CompileExceptionError("No source for " + shaderType);
        }

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            ShaderCompilerHelpers.SPIRVCompileResult result = ShaderCompilerHelpers.compileGLSLToSPIRV(source, shaderType, this.pipelineName, "", false, false);
            return result.source;
        } else if (isLanguageClassGLSL(shaderLanguage)) {
            String result = ShaderCompilerHelpers.compileGLSL(source, shaderType, shaderLanguage, false);
            return result.getBytes();
        }

        throw new CompileExceptionError("Unsupported shader language: " + shaderLanguage);
    }
}
