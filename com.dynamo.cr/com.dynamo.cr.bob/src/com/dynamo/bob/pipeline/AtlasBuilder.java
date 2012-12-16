package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
import com.dynamo.atlas.proto.AtlasProto.AtlasAnimation;
import com.dynamo.atlas.proto.AtlasProto.AtlasImage;
import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.IResource;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.atlas.AtlasGenerator;
import com.dynamo.bob.atlas.AtlasMap;
import com.dynamo.bob.atlas.AtlasGenerator.AnimDesc;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.ByteString;

@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".texturesetc")
public class AtlasBuilder extends Builder<Void>  {

    private static List<String> collectImages(Atlas atlas) {
        List<String> images = new ArrayList<String>();
        for (AtlasImage image : atlas.getImagesList()) {
            String p = image.getImage();
            images.add(p);
        }

        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            for (AtlasImage image : anim.getImagesList() ) {
                String p = image.getImage();
                images.add(p);
            }
        }
        return images;
    }

    private static String pathToId(String path) {
        return FilenameUtils.removeExtension(FilenameUtils.getName(path));
    }

    private static List<BufferedImage> loadImages(Task<Void> task) throws IOException {
        List<IResource> inputs = task.getInputs();
        List<BufferedImage> images = new ArrayList<BufferedImage>(inputs.size() - 1);

        // NOTE: By convention images are in inputs [1, ]
        for (int i = 1; i < inputs.size(); ++i) {
            BufferedImage image = ImageIO.read(new ByteArrayInputStream(inputs.get(i).getContent()));
            images.add(image);

        }
        return images;
    }

    private static List<String> createImageIds(Task<Void> task) throws IOException {
        List<IResource> inputs = task.getInputs();
        List<String> ids = new ArrayList<String>(inputs.size() - 1);

        // NOTE: By convention images are in inputs [1, ]
        for (int i = 1; i < inputs.size(); ++i) {
            ids.add(pathToId(inputs.get(i).getAbsPath()));

        }
        return ids;
    }

    private static List<AnimDesc> createAnimDescs(Atlas atlas) {
        List<AtlasGenerator.AnimDesc> animDescs = new ArrayList<AtlasGenerator.AnimDesc>(atlas.getAnimationsCount());
        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            List<String> frameIds = new ArrayList<String>();
            for (AtlasImage image : anim.getImagesList()) {
                frameIds.add(pathToId(image.getImage()));
            }
            AnimDesc animDesc = new AtlasGenerator.AnimDesc(anim.getId(), frameIds, anim.getPlayback(), anim.getFps(), anim.getFlipHorizontal() != 0, anim.getFlipVertical() != 0);
            animDescs.add(animDesc);
        }
        return animDescs;
    }

    private static short toShortUV(float fuv) {
        int uv = (int) (fuv * 65535.0f);
        return (short) (uv & 0xffff);
    }

    private static void createVertices(AtlasMap atlasMap, TextureSet.Builder b) {
        int vertexCount = 0;
        for (TextureSetAnimation anim : atlasMap.getAnimations()) {
            vertexCount += atlasMap.getVertexCount(anim);
        }

        FloatBuffer inVb = atlasMap.getVertexBuffer();
        ByteBuffer outVb = ByteBuffer.allocate(vertexCount * (3 * 4 + 4)).order(ByteOrder.LITTLE_ENDIAN);
        for (TextureSetAnimation anim : atlasMap.getAnimations()) {
            int start = atlasMap.getVertexStart(anim);
            int count = atlasMap.getVertexCount(anim);

            for (int i = 0; i < count; ++i) {
                outVb.putFloat(inVb.get((start + i) * 5 + 0));
                outVb.putFloat(inVb.get((start + i) * 5 + 1));
                outVb.putFloat(inVb.get((start + i) * 5 + 2));
                outVb.putShort(toShortUV((start + i) * 5 + 3));
                outVb.putShort(toShortUV((start + i) * 5 + 4));
            }
            b.addVertexCount(count);
        }

        outVb.rewind();
        b.setVertices(ByteString.copyFrom(outVb));
    }

    public static TextureSet.Builder createTextureSet(AtlasMap atlasMap) {
        TextureSet.Builder b = TextureSet.newBuilder();

        for (TextureSetAnimation anim : atlasMap.getAnimations()) {
            b.addAnimations(anim);
        }
        createVertices(atlasMap, b);

        return b;
    }

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

        for (String p : collectImages(atlas)) {
            taskBuilder.addInput(input.getResource(p));
        }
        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError, IOException {
        Atlas.Builder builder = Atlas.newBuilder();
        ProtoUtil.merge(task.input(0), builder);
        Atlas atlas = builder.build();

        List<BufferedImage> images = loadImages(task);
        List<String> ids = createImageIds(task);
        List<AnimDesc> animDescs = createAnimDescs(atlas);

        AtlasMap atlasMap = AtlasGenerator.generate(images, ids, animDescs, Math.max(0, atlas.getMargin()));

        TextureSet.Builder textureSetBuilder = createTextureSet(atlasMap);
        int buildDirLen = project.getBuildDirectory().length();
        String texturePath = task.output(1).getPath().substring(buildDirLen);
        textureSetBuilder.setTexture(texturePath);
        TextureSet textureSet = textureSetBuilder.build();

        TextureImage texture;
        try {
            texture = TextureGenerator.generate(atlasMap.getImage());
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, "Unable to create atlas texture", e);
        }

        task.output(0).setContent(textureSet.toByteArray());
        task.output(1).setContent(texture.toByteArray());
    }

}
