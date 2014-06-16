package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
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
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.tile.proto.Tile.Playback;

@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".texturesetc")
public class AtlasBuilder extends Builder<Void>  {

    public static class MappedAnimDesc extends AnimDesc {
        List<String> ids;

        public MappedAnimDesc(String id, List<String> ids, Playback playback, int fps, boolean flipHorizontal,
                boolean flipVertical) {
            super(id, playback, fps, flipHorizontal, flipVertical);
            this.ids = ids;
        }

        public MappedAnimDesc(String id, List<String> ids) {
            super(id, Playback.PLAYBACK_NONE, 0, false, false);
            this.ids = ids;
        }

        public List<String> getIds() {
            return this.ids;
        }
    }
    
    public static class MappedAnimIterator implements AnimIterator {
        final List<MappedAnimDesc> anims;
        final List<String> imageIds;
        int nextAnimIndex;
        int nextFrameIndex;

        public MappedAnimIterator(List<MappedAnimDesc> anims, List<String> imageIds) {
            this.anims = anims;
            this.imageIds = imageIds;
        }

        @Override
        public AnimDesc nextAnim() {
            if (nextAnimIndex < anims.size()) {
                nextFrameIndex = 0;
                return anims.get(nextAnimIndex++);
            }
            return null;
        }

        @Override
        public Integer nextFrameIndex() {
            MappedAnimDesc anim = anims.get(nextAnimIndex - 1);
            if (nextFrameIndex < anim.getIds().size()) {
                return imageIds.indexOf(anim.getIds().get(nextFrameIndex++));
            }
            return null;
        }

        @Override
        public void rewind() {
            nextAnimIndex = 0;
            nextFrameIndex = 0;
        }
    }

    private static List<String> collectImages(Atlas atlas) {
        List<String> images = new ArrayList<String>();
        for (AtlasImage image : atlas.getImagesList()) {
            String p = image.getImage();
            if (!images.contains(p)) {
                images.add(p);
            }
        }

        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            for (AtlasImage image : anim.getImagesList() ) {
                String p = image.getImage();
                if (!images.contains(p)) {
                    images.add(p);
                }
            }
        }
        return images;
    }

    private static String pathToId(String path) {
        return FilenameUtils.removeExtension(FilenameUtils.getName(path));
    }

    private static List<String> getImageInputs(Task<Void> task) {
        int inputCount = task.getInputs().size();
        List<String> images = new ArrayList<String>(inputCount);
        for (int i = 1; i < inputCount; ++i) {
            images.add(task.input(i).getPath());
        }
        return images;
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

    private List<MappedAnimDesc> createAnimDescs(List<String> images, Atlas atlas) {
        List<MappedAnimDesc> animDescs = new ArrayList<MappedAnimDesc>(atlas.getAnimationsCount()
                + atlas.getImagesCount());
        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            List<String> frameIds = new ArrayList<String>();
            for (AtlasImage image : anim.getImagesList()) {
                frameIds.add(project.getResource(image.getImage()).getPath());
            }
            animDescs.add(new MappedAnimDesc(anim.getId(), frameIds, anim.getPlayback(), anim.getFps(), anim
                    .getFlipHorizontal() != 0, anim.getFlipVertical() != 0));
        }
        for (AtlasImage image : atlas.getImagesList()) {
            MappedAnimDesc animDesc = new MappedAnimDesc(pathToId(image.getImage()), Collections.singletonList(project
                    .getResource(image.getImage()).getPath()));
            animDescs.add(animDesc);
        }

        return animDescs;
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

        List<String> imagePaths = getImageInputs(task);
        List<BufferedImage> images = loadImages(task);
        List<MappedAnimDesc> animDescs = createAnimDescs(imagePaths, atlas);

        MappedAnimIterator iterator = new MappedAnimIterator(animDescs, imagePaths);

        TextureSetResult result = TextureSetGenerator.generate(images, iterator,
                Math.max(0, atlas.getMargin()), Math.max(0, atlas.getExtrudeBorders()), false);

        int buildDirLen = project.getBuildDirectory().length();
        String texturePath = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet = result.builder.setTexture(texturePath).build();

        TextureImage texture;
        try {
            texture = TextureGenerator.generate(result.image);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(),
                    e);
        }

        task.output(0).setContent(textureSet.toByteArray());
        task.output(1).setContent(texture.toByteArray());
    }

}
