package com.dynamo.bob.pipeline;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.ImageIO;

import org.apache.commons.io.FilenameUtils;
import org.apache.commons.lang3.Pair;

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
import com.dynamo.bob.atlas.AtlasGenerator.AnimDesc;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;

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

        Pair<com.dynamo.textureset.proto.TextureSetProto.TextureSet.Builder, BufferedImage> pair
            = AtlasGenerator.generate(images, ids, animDescs, Math.max(0, atlas.getMargin()));

        int buildDirLen = project.getBuildDirectory().length();
        String texturePath = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet = pair.left.setTexture(texturePath).build();

        TextureImage texture;
        try {
            texture = TextureGenerator.generate(pair.right);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, "Unable to create atlas texture", e);
        }

        task.output(0).setContent(textureSet.toByteArray());
        task.output(1).setContent(texture.toByteArray());
    }

}
