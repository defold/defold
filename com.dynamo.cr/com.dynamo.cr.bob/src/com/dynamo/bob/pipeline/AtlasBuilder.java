package com.dynamo.bob.pipeline;

import java.io.IOException;
import org.apache.commons.io.FilenameUtils;

import com.dynamo.atlas.proto.AtlasProto.Atlas;
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
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;

@BuilderParams(name = "Atlas", inExts = {".atlas"}, outExt = ".texturesetc")
public class AtlasBuilder extends Builder<Void>  {

    static public String generateTextureFilename(String input) {
        return String.format("/%s%s.%s", FilenameUtils.getPath(input), FilenameUtils.getBaseName(input), "texturec");
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
                .addOutput(input.getResource( generateTextureFilename(input.getPath())).output());

        for (String p : AtlasUtil.collectImages(atlas)) {
            taskBuilder.addInput(input.getResource(p));
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
        TextureSetResult result = AtlasUtil.genereateTextureSet(project, task.input(0));

        int buildDirLen = project.getBuildDirectory().length();
        String texturePath = task.output(1).getPath().substring(buildDirLen);
        TextureSet textureSet = result.builder.setTexture(texturePath).build();

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(this.project.getTextureProfiles(), task.input(0).getPath());

        TextureImage texture;
        try {
            boolean compress = project.option("texture-compression", "false").equals("true");
            texture = TextureGenerator.generate(result.image, texProfile, compress);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        for(TextureImage.Image img  : texture.getAlternativesList()) {
            if(img.getCompressionType() != TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT) {
                for(IResource res : task.getOutputs()) {
                    if(res.getPath().endsWith("texturec")) {
                        this.project.addOutputFlags(res.getAbsPath(), Project.OutputFlags.UNCOMPRESSED);
                    }
                }
            }
        }

        task.output(0).setContent(textureSet.toByteArray());
        task.output(1).setContent(texture.toByteArray());
    }

}
