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

    protected class ShaderModuleLegacy extends ShaderCompilePipeline.ShaderModule {
        public ShaderCompilerHelpers.SPIRVCompileResult spirvResult;

        public ShaderModuleLegacy(String source, ShaderDesc.ShaderType type) {
            super(source, type);
        }
    }

	public ShaderCompilePipelineLegacy(String pipelineName) {
        super(pipelineName);
    }

    private ShaderModuleLegacy getShaderModule(ShaderDesc.ShaderType shaderType) {
        for (ShaderModule m : this.shaderModules) {
            if (m.type == shaderType && m instanceof ShaderModuleLegacy) {
                return (ShaderModuleLegacy) m;
            }
        }
        return null;
    }

    @Override
    protected void addShaderModule(String source, ShaderDesc.ShaderType type) {
        shaderModules.add(new ShaderModuleLegacy(source, type));
    }

    @Override
    protected void prepare() throws IOException, CompileExceptionError {
        // Generate spirv for each shader module so we can utilize the reflection from it
        for (ShaderModule module : this.shaderModules) {
            ShaderCompilerHelpers.SPIRVCompileResult result = ShaderCompilerHelpers.compileGLSLToSPIRV(module.source, module.type, this.pipelineName, "", false, false);

            ShaderModuleLegacy moduleLegacy = (ShaderModuleLegacy) module;
            moduleLegacy.spirvResult = result;
        }
    }

    @Override
    public byte[] crossCompile(ShaderDesc.ShaderType shaderType, ShaderDesc.Language shaderLanguage) throws IOException, CompileExceptionError {
        ShaderModuleLegacy module = getShaderModule(shaderType);
        if (module == null) {
            throw new CompileExceptionError("No module found for " + shaderType);
        }

        // Todo: this function should return a reflector and the byte source
        this.spirvReflector = module.spirvResult.reflector;

        if (shaderLanguage == ShaderDesc.Language.LANGUAGE_SPIRV) {
            return module.spirvResult.source;
        } else if (canBeCrossCompiled(shaderLanguage)) {
            String result = ShaderCompilerHelpers.compileGLSL(module.source, shaderType, shaderLanguage, false);
            return result.getBytes();
        }

        throw new CompileExceptionError("Unsupported shader language: " + shaderLanguage);
    }
}
