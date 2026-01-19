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

import java.io.ByteArrayInputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureProfile;

@BuilderParams(name = "Texture", inExts = {".png", ".jpg", ".jpeg"}, outExt = ".texturec", isCacheble = true, paramsForSignature = {"texture-compression"})
public class TextureBuilder extends Builder {

    private static Logger logger = Logger.getLogger(TextureBuilder.class.getName());

    @Override
    public Task create(IResource input) throws IOException {

        TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        TextureUtil.addTextureProfileInput(taskBuilder, project);

        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError,
            IOException {

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(task.lastInput(), task.firstInput().getPath());
        logger.fine("Compiling %s using profile %s", task.firstInput().getPath(), texProfile!=null?texProfile.getName():"<none>");

        ByteArrayInputStream is = new ByteArrayInputStream(task.firstInput().getContent());

        TextureGenerator.GenerateResult generateResult;
        try {
            boolean compress = project.option("texture-compression", "false").equals("true");
            generateResult = TextureGenerator.generate(is, texProfile, compress);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        TextureUtil.writeGenerateResultToResource(generateResult, task.output(0));
    }

}
