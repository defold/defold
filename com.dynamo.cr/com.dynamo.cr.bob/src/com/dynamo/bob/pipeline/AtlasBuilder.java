// Copyright 2020-2022 The Defold Foundation
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
import java.awt.image.BufferedImage;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;

@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".a.texturesetc")
public class AtlasBuilder extends Builder<TextureSetResult>  {

    @Override
    public Task<TextureSetResult> create(IResource input) throws IOException, CompileExceptionError {
        Atlas.Builder builder = Atlas.newBuilder();
        ProtoUtil.merge(input, builder);
        Atlas atlas = builder.build();

        TaskBuilder<TextureSetResult> taskBuilder = Task.<TextureSetResult>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        for (AtlasImage image : AtlasUtil.collectImages(atlas)) {
            taskBuilder.addInput(input.getResource(image.getImage()));
        }

        // If there is a texture profiles file, we need to make sure
        // it has been read before building this tile set, add it as an input.
        String textureProfilesPath = this.project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {
            taskBuilder.addInput(this.project.getResource(textureProfilesPath));
        }

        // JG: This is probably a bad idea, but I think we need to know how many outputs we need to add right?
        TextureSetResult result = AtlasUtil.generateTextureSet(this.project, input);

        for (int i=0; i < result.images.size(); i++) {
            taskBuilder.addOutput(input.changeExt("_page" + i + ".texturec"));
        }

        taskBuilder.setData(result);
        return taskBuilder.build();
    }

    @Override
    public void build(Task<TextureSetResult> task) throws CompileExceptionError, IOException {
        TextureSetResult result = task.data;
        TextureSet.Builder textureSetBuilder = result.builder;
        int buildDirLen = project.getBuildDirectory().length();

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(this.project.getTextureProfiles(), task.input(0).getPath());
        Bob.verbose("Compiling %s using profile %s", task.input(0).getPath(), texProfile!=null?texProfile.getName():"<none>");

        int image_i = 1;
        for (BufferedImage image : result.images)
        {
            TextureImage texture;
            try {
                boolean compress = project.option("texture-compression", "false").equals("true");
                texture = TextureGenerator.generate(image, texProfile, compress);
            } catch (TextureGeneratorException e) {
                throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
            }

            String texturePath = task.output(image_i).getPath().substring(buildDirLen);
            textureSetBuilder.addTextures(texturePath);

            task.output(image_i).setContent(texture.toByteArray());
            image_i++;
        }

        TextureSet textureSet = textureSetBuilder.build();
        task.output(0).setContent(textureSet.toByteArray());
    }

}
