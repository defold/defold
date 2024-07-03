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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import java.util.ArrayList;
import java.util.List;

import org.junit.Test;

import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.pipeline.ShaderCompilePipeline;
import com.dynamo.bob.pipeline.ShaderUtil.SPIRVReflector;
import com.dynamo.graphics.proto.Graphics.ShaderDesc;

public class ShaderCompilePipelineTest {
	@Test
    public void testSimple() throws Exception {

    	String vsShader =
    		"#version 140\n" +
    		"in vec4 position;\n" +
    		"uniform uniforms\n" +
    		"{\n" +
    		"	mat4 view;\n" +
    		"};\n" +
    		"void main() {\n" +
    		"	gl_Position = view * position;\n" +
    		"}\n";

		String fsShader =
				"#version 140\n" +
				"out vec4 color;\n" +
				"void main() {\n" +
				"	color = vec4(1.0);\n" +
				"}\n";

		ShaderCompilePipeline pipeline = ShaderCompilePipeline.createShaderPipelineGraphics("testSimple", vsShader, fsShader);
		byte[] source = pipeline.crossCompile(ShaderDesc.ShaderType.SHADER_TYPE_VERTEX, ShaderDesc.Language.LANGUAGE_GLES_SM100);

    	SPIRVReflector reflector 	     			 = pipeline.getReflectionData();
    	ArrayList<SPIRVReflector.Resource> inputs    = reflector.getInputs();
		ArrayList<SPIRVReflector.Resource> outputs   = reflector.getOutputs();
		ArrayList<SPIRVReflector.Resource> ubos      = reflector.getUBOs();
		ArrayList<SPIRVReflector.ResourceType> types = reflector.getTypes();

		for (SPIRVReflector.Resource res : inputs) {
			System.out.println("Input: " + res.name);
		}
		for (SPIRVReflector.Resource res : outputs) {
			System.out.println("Output: " + res.name);
		}
		for (SPIRVReflector.Resource res : ubos) {
			System.out.println("UBO: " + res.name);
		}
		for (SPIRVReflector.ResourceType res : types) {
			System.out.println("Type: " + res.name);

			for (SPIRVReflector.ResourceMember member : res.members) {
				System.out.println("Member: " + member.name + " " + member.type);
			}
		}
    }
}
