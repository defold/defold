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
import java.awt.image.BufferedImage;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.util.List;

import java.nio.file.Path;
import java.nio.file.Paths;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.pipeline.TextureGeneratorException;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;

import com.google.protobuf.TextFormat;

@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".a.texturesetc")
public class AtlasBuilder extends Builder<TextureImage.Type>  {

    private static Logger logger = Logger.getLogger(AtlasBuilder.class.getName());

    private static TextureImage.Type getTexureType(Atlas atlas) {
        // We can't just look at result of texture generation to decide the image type,
        // a texture specified with max page size can still generate one page but used with a material that has array samplers
        // so we need to know this beforehand for both validation and runtime
        return atlas.getMaxPageWidth() > 0 && atlas.getMaxPageHeight() > 0 ? TextureImage.Type.TYPE_2D_ARRAY : TextureImage.Type.TYPE_2D;
    }

    private static int getPageCount(List<BufferedImage> images, TextureImage.Type textureType) {
        int numPages = images.size();
        // Even though technically a 2D texture image has one page,
        // it differs conceptually how we treat the image in the engine
        if (textureType == TextureImage.Type.TYPE_2D) {
            numPages = 0;
        }
        return numPages;
    }

    @Override
    public Task<TextureImage.Type> create(IResource input) throws IOException, CompileExceptionError {
        Atlas.Builder builder = Atlas.newBuilder();
        ProtoUtil.merge(input, builder);
        Atlas atlas = builder.build();

        TextureImage.Type textureImageType = getTexureType(atlas);

        TaskBuilder<TextureImage.Type> taskBuilder = Task.<TextureImage.Type>newBuilder(this)
                .setName(params.name())
                .setData(textureImageType)
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .addOutput(input.changeExt(".texturec"));

        AtlasUtil.PathTransformer transformer = AtlasUtil.createPathTransformer(project, atlas.getRenamePatterns());
        for (AtlasImage image : AtlasUtil.collectImages(input, atlas, transformer)) {
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
    public void build(Task<TextureImage.Type> task) throws CompileExceptionError, IOException {
        TextureSetResult result            = AtlasUtil.generateTextureSet(this.project, task.input(0));
        TextureImage.Type textureImageType = task.getData();

        int buildDirLen         = project.getBuildDirectory().length();
        String texturePath      = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet   = result.builder.setPageCount(getPageCount(result.images, textureImageType))
                                                .setTexture(texturePath)
                                                .build();

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(this.project.getTextureProfiles(), task.input(0).getPath());
        logger.info("Compiling %s using profile %s", task.input(0).getPath(), texProfile!=null?texProfile.getName():"<none>");

        boolean compress = project.option("texture-compression", "false").equals("true");
        TextureImage texture = null;
        try {
            texture = TextureUtil.createMultiPageTexture(result.images, textureImageType, texProfile, compress);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        task.output(0).setContent(textureSet.toByteArray());
        task.output(1).setContent(texture.toByteArray());
    }

    public static void main(String[] args) throws IOException, CompileExceptionError, TextureGeneratorException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 4) {
            System.err.println("Usage: AtlasBuilder atlas-path texture-set-out-path texture-out-path project-path");
            System.exit(1);
        }

        String atlasInPath       = args[0];
        String textureSetOutPath = args[1];
        String textureOutPath    = args[2];
        String baseDir           = args[3];

        final File inFile           = new File(atlasInPath);
        FileInputStream inputStream = new FileInputStream(inFile);
        InputStreamReader reader    = new InputStreamReader(inputStream);

        Atlas.Builder builder = Atlas.newBuilder();
        TextFormat.merge(reader, builder);
        reader.close();

        Atlas atlas = builder.build();
        TextureImage.Type textureImageType = getTexureType(atlas);

        TextureSetResult result = AtlasUtil.generateTextureSet(atlas, new AtlasUtil.PathTransformer() {
            @Override
            public String transform(String path) {
                String outPath = baseDir + path;
                return outPath;
            }
        });

        Path basedirAbsolutePath = Paths.get(baseDir).toAbsolutePath();
        Path textureProjectPath  = Paths.get(inFile.getAbsolutePath().replace(".atlas", ".texturec"));
        Path textureRelativePath = basedirAbsolutePath.relativize(textureProjectPath);
        String textureProjectStr = "/" + textureRelativePath.toString().replace("\\","/");

        TextureSet textureSet = result.builder.setPageCount(getPageCount(result.images, textureImageType))
                                        .setTexture(textureProjectStr)
                                        .build();
        TextureImage texture = TextureUtil.createMultiPageTexture(result.images, textureImageType, null, false);

        FileOutputStream textureSetOutStream = new FileOutputStream(textureSetOutPath);
        textureSet.writeTo(textureSetOutStream);
        textureSetOutStream.close();

        FileOutputStream textureOutStream = new FileOutputStream(textureOutPath);
        texture.writeTo(textureOutStream);
        textureOutStream.close();
    }
}
