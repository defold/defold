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

import java.awt.Graphics2D;
import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;

import javax.imageio.ImageIO;

import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.CompileExceptionError;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.bob.tile.TileSetGenerator;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.bob.logging.Logger;
import com.dynamo.graphics.proto.Graphics.TextureProfile;
import com.dynamo.gamesys.proto.TextureSetProto.TextureSet;
import com.dynamo.gamesys.proto.Tile.TileSet;

@ProtoParams(srcClass = TileSet.class, messageClass = TileSet.class)
@BuilderParams(name = "TileSet", inExts = {".tileset", ".tilesource"}, outExt = ".t.texturesetc", isCacheble = true, paramsForSignature = {"texture-compression"})
public class TileSetBuilder extends ProtoBuilder<TileSet.Builder> {

    private static Logger logger = Logger.getLogger(TileSetBuilder.class.getName());

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        TileSet.Builder builder = getSrcBuilder(input);
        String imgPath = builder.getImage();
        String collisionPath = builder.getCollision();
        IResource image = this.project.getResource(imgPath);
        IResource collision = this.project.getResource(collisionPath);
        if (image.exists() || collision.exists()) {
            TaskBuilder taskBuilder = Task.newBuilder(this)
                    .setName(params.name())
                    .addInput(input)
                    .addOutput(input.changeExt(params.outExt()));
            String texturePath = String.format("%s__%s_tilesource.%s", FilenameUtils.getPath(input.getPath()),
                    FilenameUtils.getBaseName(input.getPath()), "texturec");
            taskBuilder.addOutput(this.project.getResource(texturePath).output());
            if (image.exists()) {
                taskBuilder.addInput(image);
            }
            if (collision.exists()) {
                taskBuilder.addInput(collision);
            }

            TextureUtil.addTextureProfileInput(taskBuilder, project);

            return taskBuilder.build();
        } else {
            if (!imgPath.isEmpty()) {
                BuilderUtil.checkResource(this.project, input, "image", imgPath);
            } else if (!collisionPath.isEmpty()) {
                BuilderUtil.checkResource(this.project, input, "collision", collisionPath);
            } else {
                throw new CompileExceptionError(input, 0, Messages.TileSetBuilder_MISSING_IMAGE_AND_COLLISION);
            }
            // will not be reached
            return null;
        }
    }

    @Override
    public void build(Task task) throws CompileExceptionError,
            IOException {

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(task.lastInput(), task.firstInput().getPath());
        logger.fine("Compiling %s using profile %s", task.firstInput().getPath(), texProfile!=null?texProfile.getName():"<none>");

        TileSet.Builder builder = getSrcBuilder(task.firstInput());
        TileSet tileSet = builder.build();

        String imgPath = tileSet.getImage();

        String collisionPath = tileSet.getCollision();
        IResource imageRes = this.project.getResource(imgPath);
        IResource collisionRes = this.project.getResource(collisionPath);

        BufferedImage image = null;
        if (imageRes.exists()) {
            image = ImageIO.read(new ByteArrayInputStream(imageRes.getContent()));
        }
        if (image != null && (image.getWidth() < tileSet.getTileWidth() || image.getHeight() < tileSet.getTileHeight())) {
            throw new CompileExceptionError(task.firstInput(), -1, String.format(
                    "the image dimensions (%dx%d) are smaller than the tile dimensions (%dx%d)", image.getWidth(),
                    image.getHeight(), tileSet.getTileWidth(), tileSet.getTileHeight()));
        }

        BufferedImage collisionImage = null;
        if (collisionRes.exists()) {
            BufferedImage origImage = ImageIO.read(new ByteArrayInputStream(collisionRes.getContent()));
            collisionImage = new BufferedImage(origImage.getWidth(), origImage.getHeight(),
                    BufferedImage.TYPE_4BYTE_ABGR);
            Graphics2D g2d = collisionImage.createGraphics();
            g2d.drawImage(origImage, 0, 0, null);
            g2d.dispose();
        }

        if (image != null && collisionImage != null
                && (image.getWidth() != collisionImage.getWidth() || image.getHeight() != collisionImage.getHeight())) {
            throw new CompileExceptionError(task.input(0), -1, String.format(
                    "the image and collision image has different dimensions: (%dx%d) vs (%dx%d)", image.getWidth(),
                    image.getHeight(), collisionImage.getWidth(), collisionImage.getHeight()));
        }
        if (collisionImage != null && !collisionImage.getColorModel().hasAlpha()) {
            throw new CompileExceptionError(task.input(0), -1, "the collision image does not have an alpha channel");
        }
        TextureSetResult result = TileSetGenerator.generate(tileSet, image, collisionImage);
        TextureSet.Builder textureSetBuilder = result.builder;

        int buildDirLen = project.getBuildDirectory().length();
        String texturePath = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet = textureSetBuilder.setTexture(texturePath).build();

        TextureGenerator.GenerateResult generateResult;
        try {
            boolean compress = project.option("texture-compression", "false").equals("true");
            generateResult = TextureGenerator.generate(result.images.get(0), texProfile, compress);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        task.output(0).setContent(textureSet.toByteArray());
        TextureUtil.writeGenerateResultToResource(generateResult, task.output(1));
    }
}
