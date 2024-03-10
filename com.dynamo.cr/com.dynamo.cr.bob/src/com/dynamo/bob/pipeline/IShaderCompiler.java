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
import java.util.ArrayList;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.ShaderProgramBuilder;
import com.dynamo.bob.pipeline.ShaderUtil.ES2ToES3Converter;

public interface IShaderCompiler {
    public ArrayList<ShaderProgramBuilder.ShaderBuildResult> compile(String shaderSource, ES2ToES3Converter.ShaderType shaderType, String resourceOutputPath, String resourceOutput, boolean isDebug, boolean outputSpirv, boolean soft_fail) throws IOException, CompileExceptionError;
}
