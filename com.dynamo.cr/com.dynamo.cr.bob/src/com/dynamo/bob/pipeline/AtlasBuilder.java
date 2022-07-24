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
import java.io.ByteArrayOutputStream;
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
import com.dynamo.graphics.proto.Graphics.TextureImage.Image;
import com.dynamo.graphics.proto.Graphics.TextureImage.Type;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;
import com.google.protobuf.ByteString;

@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".a.texturesetc")
public class AtlasBuilder extends Builder<Void>  {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Atlas.Builder builder = Atlas.newBuilder();
        ProtoUtil.merge(input, builder);
        Atlas atlas = builder.build();

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .addOutput(input.changeExt(".texturec"));

        for (AtlasImage image : AtlasUtil.collectImages(atlas)) {
            taskBuilder.addInput(input.getResource(image.getImage()));
        }

        // If there is a texture profiles file, we need to make sure
        // it has been read before building this tile set, add it as an input.
        String textureProfilesPath = this.project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {
            taskBuilder.addInput(this.project.getResource(textureProfilesPath));
        }

        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        TextureSetResult result = AtlasUtil.generateTextureSet(this.project, task.input(0));
        int buildDirLen         = project.getBuildDirectory().length();
        String texturePath      = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet   = result.builder.setTexture(texturePath).build();

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(this.project.getTextureProfiles(), task.input(0).getPath());
        Bob.verbose("Compiling %s using profile %s", task.input(0).getPath(), texProfile!=null?texProfile.getName():"<none>");

        int numTextures              = result.images.size();
        TextureImage textureImages[] = new TextureImage[numTextures];

        for (int i = 0; i < numTextures; i++)
        {
            TextureImage texture;
            try {
                boolean compress = project.option("texture-compression", "false").equals("true");
                texture = TextureGenerator.generate(result.images.get(i), texProfile, compress);
            } catch (TextureGeneratorException e) {
                throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
            }
            textureImages[i] = texture;
        }

        TextureImage.Builder textureImageBuilder = TextureImage.newBuilder(textureImages[0]);

        // Merge this with cubemapbuilder?
        for (int i = 0; i < textureImageBuilder.getAlternativesCount(); i++) {
            Image.Builder imageBuilder = TextureImage.Image.newBuilder(textureImages[0].getAlternatives(i));

            ByteArrayOutputStream os = new ByteArrayOutputStream(1024 * 4);
            for (int j = 0; j < imageBuilder.getMipMapSizeCount(); j++) {
                int mipSize = imageBuilder.getMipMapSize(j);
                byte[] buf = new byte[mipSize];
                for (int k = 0; k < numTextures; k++) {
                    ByteString data = textureImages[k].getAlternatives(i).getData();
                    int mipOffset = imageBuilder.getMipMapOffset(j);

                    System.out.println("mipOffset " + mipOffset + ", mipSize " + mipSize);

                    data.copyTo(buf, mipOffset, 0, mipSize);
                    os.write(buf);
                }
            }
            os.flush();
            imageBuilder.setData(ByteString.copyFrom(os.toByteArray()));
            for (int j = 0; j < imageBuilder.getMipMapSizeCount(); j++) {
                imageBuilder.setMipMapOffset(j, imageBuilder.getMipMapOffset(j) * numTextures);
            }
            textureImageBuilder.setAlternatives(i, imageBuilder);
        }

        textureImageBuilder.setCount(numTextures);
        textureImageBuilder.setType(Type.TYPE_2D_ARRAY);
        TextureImage texture = textureImageBuilder.build();

        task.output(0).setContent(textureSet.toByteArray());
        task.output(1).setContent(texture.toByteArray());
    }

}
