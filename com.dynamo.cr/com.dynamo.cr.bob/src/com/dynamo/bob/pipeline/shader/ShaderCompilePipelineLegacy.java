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

    @Override
    protected void prepare() throws IOException, CompileExceptionError {
        for (ShaderModule module : this.shaderModules) {
            module.source = ShaderCompilerHelpers.compileGLSL(module.source, module.type, ShaderDesc.Language.LANGUAGE_GLSL_SM140, false);
        }
        super.prepare();
	}
}
