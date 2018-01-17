package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.util.ArrayList;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.dynamo.spine.proto.Spine.SpineModelDesc;

@ProtoParams(messageClass = SpineModelDesc.class)
@BuilderParams(name="SpineModel", inExts=".spinemodel", outExt=".spinemodelc")
public class SpineModelBuilder extends ProtoBuilder<SpineModelDesc.Builder> {

    @Override
    protected SpineModelDesc.Builder transform(Task<Void> task, IResource resource, SpineModelDesc.Builder messageBuilder) throws IOException, CompileExceptionError {
        BuilderUtil.checkResource(this.project, resource, "spineScene", messageBuilder.getSpineScene());
        messageBuilder.setSpineScene(BuilderUtil.replaceExt(messageBuilder.getSpineScene(), ".spinescene", ".rigscenec"));
        BuilderUtil.checkResource(this.project, resource, "material", messageBuilder.getMaterial());
        messageBuilder.setMaterial(BuilderUtil.replaceExt(messageBuilder.getMaterial(), ".material", ".materialc"));

        // Texture0 is taken from the spine scene
        ArrayList<String> textures = new ArrayList<String>(messageBuilder.getTexturesCount());
        SpineModelDesc.Builder modelDescBuilder = SpineModelDesc.newBuilder();
        ProtoUtil.merge(resource, modelDescBuilder);
        IResource spineScene = BuilderUtil.checkResource(this.project, resource, "spineScene", modelDescBuilder.getSpineScene());
        SpineSceneDesc.Builder spineSceneDescBuilder = SpineSceneDesc.newBuilder();
        ProtoUtil.merge(spineScene, spineSceneDescBuilder);
        textures.add(ProtoBuilders.resolveTextureFilename(spineSceneDescBuilder.getAtlas()));
        // Texture1-x is taken from the spine model
        for (int i = 1; i < messageBuilder.getTexturesCount(); ++i) {
            String texture = messageBuilder.getTextures(i);
            if(!texture.isEmpty()) {
                BuilderUtil.checkResource(this.project, resource, "texture", texture);
            }
            textures.add(ProtoBuilders.resolveTextureFilename(texture));
        }
        messageBuilder.clearTextures();
        messageBuilder.addAllTextures(textures);

        return messageBuilder;
    }
}
