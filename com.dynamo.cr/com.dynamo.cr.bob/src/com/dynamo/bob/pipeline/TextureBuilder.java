package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.Project;
import com.dynamo.bob.Task;
import com.dynamo.bob.Task.TaskBuilder;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.util.TextureUtil;
import com.dynamo.graphics.proto.Graphics.TextureImage;
import com.dynamo.graphics.proto.Graphics.TextureProfile;

@BuilderParams(name = "Texture", inExts = {".png", ".jpg"}, outExt = ".texturec")
public class TextureBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException {

        TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        // If there is a texture profiles file, we need to make sure
        // it has been read before building this tile set, add it as an input.
        String textureProfilesPath = this.project.getProjectProperties().getStringValue("graphics", "texture_profiles");
        if (textureProfilesPath != null) {
            taskBuilder.addInput(this.project.getResource(textureProfilesPath));
        }

        return taskBuilder.build();
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        TextureProfile texProfile = TextureUtil.getTextureProfileByPath(this.project.getTextureProfiles(), task.input(0).getPath());

        ByteArrayInputStream is = new ByteArrayInputStream(task.input(0).getContent());
        TextureImage texture;
        try {
            boolean compress = project.option("texture-profiles", "false").equals("true");
            texture = TextureGenerator.generate(is, texProfile, compress);
        } catch (TextureGeneratorException e) {
            throw new CompileExceptionError(task.input(0), -1, e.getMessage(), e);
        }

        for(TextureImage.Image img  : texture.getAlternativesList()) {
            if(img.getCompressionType() != TextureImage.CompressionType.COMPRESSION_TYPE_DEFAULT) {
                this.project.addOutputFlags(task.output(0).getAbsPath(), Project.OutputFlags.UNCOMPRESSED);
            }
        }

        ByteArrayOutputStream out = new ByteArrayOutputStream(1024 * 1024);
        texture.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }

}
