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
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimDesc;
import com.dynamo.bob.textureset.TextureSetGenerator.AnimIterator;
import com.dynamo.bob.textureset.TextureSetGenerator.TextureSetResult;
import com.dynamo.tile.proto.Tile.Playback;

public class AtlasUtil {
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

    public static List<String> collectImages(Atlas atlas) {
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

    private static List<IResource> toResources(IResource baseResource, List<String> paths) {
        List<IResource> resources = new ArrayList<IResource>(paths.size());
        for (String path : paths) {
            resources.add(baseResource.getResource(path));
        }
        return resources;
    }

    public static List<BufferedImage> loadImages(List<IResource> resources) throws IOException, CompileExceptionError {
        List<BufferedImage> images = new ArrayList<BufferedImage>(resources.size());

        for (IResource resource : resources) {
            BufferedImage image = ImageIO.read(new ByteArrayInputStream(resource.getContent()));
            if (image == null) {
                throw new CompileExceptionError(resource, -1, "Unable to load image " + resource.getPath());
            }
            images.add(image);
        }
        return images;
    }

    private interface PathTransformer {
        String transform(String path);
    }

    private static List<MappedAnimDesc> createAnimDescs(Atlas atlas, PathTransformer transformer) {
        List<MappedAnimDesc> animDescs = new ArrayList<MappedAnimDesc>(atlas.getAnimationsCount()
                + atlas.getImagesCount());
        for (AtlasAnimation anim : atlas.getAnimationsList()) {
            List<String> frameIds = new ArrayList<String>();
            for (AtlasImage image : anim.getImagesList()) {
                frameIds.add(transformer.transform(image.getImage()));
            }
            animDescs.add(new MappedAnimDesc(anim.getId(), frameIds, anim.getPlayback(), anim.getFps(), anim
                    .getFlipHorizontal() != 0, anim.getFlipVertical() != 0));
        }
        for (AtlasImage image : atlas.getImagesList()) {
            MappedAnimDesc animDesc = new MappedAnimDesc(pathToId(image.getImage()), Collections.singletonList(transformer.transform(image.getImage())));
            animDescs.add(animDesc);
        }

        return animDescs;
    }

    public static TextureSetResult genereateTextureSet(final Project project, IResource atlasResource) throws IOException, CompileExceptionError {
        Atlas.Builder builder = Atlas.newBuilder();
        ProtoUtil.merge(atlasResource, builder);
        Atlas atlas = builder.build();

        List<String> imagePaths = collectImages(atlas);
        List<IResource> imageResources = toResources(atlasResource, imagePaths);
        List<BufferedImage> images = AtlasUtil.loadImages(imageResources);
        PathTransformer transformer = new PathTransformer() {
            @Override
            public String transform(String path) {
                return project.getResource(path).getPath();
            }
        };
        List<MappedAnimDesc> animDescs = createAnimDescs(atlas, transformer);
        int imagePathCount = imagePaths.size();
        for (int i = 0; i < imagePathCount; ++i) {
            imagePaths.set(i, transformer.transform(imagePaths.get(i)));
        }
        MappedAnimIterator iterator = new MappedAnimIterator(animDescs, imagePaths);
        return TextureSetGenerator.generate(images, iterator,
                Math.max(0, atlas.getMargin()),
                Math.max(0,  atlas.getInnerPadding()),
                Math.max(0, atlas.getExtrudeBorders()), false, false, true, false, null);
    }
}
