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

import java.io.IOException;
import java.awt.image.BufferedImage;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.util.List;

import java.nio.file.Path;
import java.nio.file.Paths;

import com.defold.extension.pipeline.texture.TextureCompression;
import com.defold.extension.pipeline.texture.TextureCompressorASTC;
import com.defold.extension.pipeline.texture.TextureCompressorBasisU;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.Task;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.logging.Logger;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.AtlasProto.Atlas;
import com.dynamo.gamesys.proto.AtlasProto.AtlasImage;

import com.google.protobuf.TextFormat;

@ProtoParams(srcClass = Atlas.class, messageClass = TextureSet.class)
@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".a.texturesetc", isCacheble = true, paramsForSignature = {"texture-compression"})
public class AtlasBuilder extends ProtoBuilder<Atlas.Builder> {

    private static final Logger logger = Logger.getLogger(AtlasBuilder.class.getName());

    private static TextureImage.Type getTexureType(Atlas.Builder builder) {
        // We can't just look at result of texture generation to decide the image type,
        // a texture specified with max page size can still generate one page but used with a material that has array samplers
        // so we need to know this beforehand for both validation and runtime
        return builder.getMaxPageWidth() > 0 && builder.getMaxPageHeight() > 0 ? TextureImage.Type.TYPE_2D_ARRAY : TextureImage.Type.TYPE_2D;
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
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Atlas.Builder builder = getSrcBuilder(input);
        Atlas atlas = builder.build();

        TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .addOutput(input.changeExt(".texturec"));

        AtlasUtil.PathTransformer transformer = AtlasUtil.createPathTransformer(project, atlas.getRenamePatterns());
        for (AtlasImage image : AtlasUtil.collectImages(input, atlas, transformer)) {
            taskBuilder.addInput(input.getResource(image.getImage()));
        }

        TextureUtil.addTextureProfileInput(taskBuilder, project);

        return taskBuilder.build();
    }

    @Override
    public void build(Task task) throws CompileExceptionError, IOException {
        Atlas.Builder builder = getSrcBuilder(task.firstInput());
        TextureSetResult result            = AtlasUtil.generateTextureSet(this.project, task.firstInput(), builder);
        TextureImage.Type textureImageType = getTexureType(builder);

        int buildDirLen         = project.getBuildDirectory().length();
        String texturePath      = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet   = result.builder.setPageCount(getPageCount(result.images, textureImageType))
                                                .setTexture(texturePath)
                                                .build();

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(task.lastInput(), task.input(0).getPath());
        logger.fine("Compiling %s using profile %s", task.input(0).getPath(), texProfile!=null?texProfile.getName():"<none>");

        boolean compress = project.option("texture-compression", "false").equals("true");
        TextureGenerator.GenerateResult generateResult = null;
        try {
            generateResult = TextureUtil.createMultiPageTexture(result.images, textureImageType, texProfile, compress);

        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        task.output(0).setContent(textureSet.toByteArray());
        TextureUtil.writeGenerateResultToResource(generateResult, task.output(1));
    }

    public static void main(String[] args) throws IOException, CompileExceptionError, TextureGeneratorException {
        System.setProperty("java.awt.headless", "true");

        if (args.length < 4) {
            System.err.println("Usage: AtlasBuilder atlas-path texture-set-out-path texture-out-path project-path");
            System.exit(1);
        }

        TextureCompression.registerCompressor(new TextureCompressorBasisU());
        TextureCompression.registerCompressor(new TextureCompressorASTC());

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

        TextureImage.Type textureImageType = getTexureType(builder);

        Atlas atlas = builder.build();
        TextureSetResult result = AtlasUtil.generateTextureSet(atlas, path -> baseDir + path);

        Path basedirAbsolutePath = Paths.get(baseDir).toAbsolutePath();
        Path textureProjectPath  = Paths.get(inFile.getAbsolutePath().replace(".atlas", ".texturec"));
        Path textureRelativePath = basedirAbsolutePath.relativize(textureProjectPath);
        String textureProjectStr = "/" + textureRelativePath.toString().replace("\\","/");

        TextureSet textureSet = result.builder.setPageCount(getPageCount(result.images, textureImageType))
                                        .setTexture(textureProjectStr)
                                        .build();
        TextureGenerator.GenerateResult generateResult = TextureUtil.createMultiPageTexture(result.images, textureImageType, null, false);

        FileOutputStream textureSetOutStream = new FileOutputStream(textureSetOutPath);
        textureSet.writeTo(textureSetOutStream);
        textureSetOutStream.close();

        FileOutputStream textureOutStream = new FileOutputStream(textureOutPath);
        TextureUtil.writeGenerateResultToOutputStream(generateResult, textureOutStream);
        textureOutStream.close();
    }
}
