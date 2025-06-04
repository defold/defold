// Copyright 2020-2025 The Defold Foundation
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
import java.util.EnumSet;

import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.Cubemap;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureImage.Image;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.dynamo.graphics.proto.Graphics.TextureProfile;

@ProtoParams(srcClass = Cubemap.class, messageClass = Cubemap.class)
@BuilderParams(name = "Cubemap", inExts = {".cubemap"}, outExt = ".cubemapc", isCacheble = true, paramsForSignature = {"texture-compression"})
public class CubemapBuilder extends ProtoBuilder<Cubemap.Builder> {

    private static Logger logger = Logger.getLogger(CubemapBuilder.class.getName());

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Cubemap.Builder builder = getSrcBuilder(input);

        TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addInput(input.getResource(builder.getRight()))
                .addInput(input.getResource(builder.getLeft()))
                .addInput(input.getResource(builder.getTop()))
                .addInput(input.getResource(builder.getBottom()))
                .addInput(input.getResource(builder.getFront()))
                .addInput(input.getResource(builder.getBack()))
                // outExt should correspond to the protoclass used to parse proto message it produces
                // '.texturec' can't be parsed with Cubemap, we specify this output manually
                .addOutput(input.changeExt(".texturec"));

        TextureUtil.addTextureProfileInput(taskBuilder, project);

        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(task.lastInput(), task.input(0).getPath());
        logger.fine("Compiling %s using profile %s", task.firstInput().getPath(), texProfile!=null?texProfile.getName():"<none>");

        TextureGenerator.GenerateResult[] generateResults = new TextureGenerator.GenerateResult[6];

        try {
            for (int i = 0; i < 6; i++) {
                ByteArrayInputStream is = new ByteArrayInputStream(task.input(i + 1).getContent());
                boolean compress = project.option("texture-compression", "false").equals("true");
                // NOTE: Cubemap sides should not have a flipped Y axis (as opposed to any other texture).
                // I could only find tidbits of information regarding this online, as far as I understand
                // it is not explained in the OGL spec or cubemap extension either.
                // However, most suggest that the origin of cubemap sides are upper left as opposed to lower left
                // like this SO answer suggest;
                //   "Cube Maps have been specified to follow the RenderMan specification (for whatever reason),
                //    and RenderMan assumes the images' origin being in the upper left, contrary to the usual
                //    OpenGL behaviour of having the image origin in the lower left."
                // Source: https://stackoverflow.com/a/11690553/129360
                //
                // So for cube map textures we don't flip on any axis, meaning the texture data begin in the
                // upper left corner of the input image.


                // NOTE: Setting the same input for more than one side will cause a NPE when generating!
                generateResults[i] = TextureGenerator.generate(is, texProfile, compress, EnumSet.noneOf(Texc.FlipAxis.class));
            }

            validate(task, generateResults);

            TextureGenerator.GenerateResult cubeMapResult = TextureUtil.createCombinedTextureImage(generateResults, Type.TYPE_CUBEMAP);
            assert cubeMapResult != null;
            TextureUtil.writeGenerateResultToResource(cubeMapResult, task.output(0));
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }
    }

    private void validate(Task task, TextureGenerator.GenerateResult[] generateResults) throws CompileExceptionError {
        for (int i = 1; i < 6; i++) {
            TextureImage t = generateResults[i].textureImage;
            for (int j = 0; j < t.getAlternativesCount(); j++) {
                Image im = t.getAlternatives(j);

                Image compareTo = generateResults[0].textureImage.getAlternatives(j);
                if (im.getWidth() != compareTo.getWidth() || im.getHeight() != compareTo.getHeight()) {
                    throw new CompileExceptionError(task.input(0), -1, "Cubemap sides must be of identical size", null);
                }
            }
        }
    }
}
