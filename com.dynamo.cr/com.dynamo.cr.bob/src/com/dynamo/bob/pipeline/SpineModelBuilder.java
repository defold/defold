package com.dynamo.bob.pipeline;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.spine.proto.Spine.SpineModelDesc;

@ProtoParams(messageClass = SpineModelDesc.class)
@BuilderParams(name="SpineModel", inExts=".spinemodel", outExt=".spinemodelc")
public class SpineModelBuilder extends ProtoBuilder<SpineModelDesc.Builder> {

    @Override
    protected SpineModelDesc.Builder transform(Task<Void> task, IResource resource, SpineModelDesc.Builder messageBuilder) throws CompileExceptionError {
        BuilderUtil.checkResource(this.project, resource, "spineScene", messageBuilder.getSpineScene());
        messageBuilder.setSpineScene(BuilderUtil.replaceExt(messageBuilder.getSpineScene(), ".spinescene", ".rigscenec"));
        BuilderUtil.checkResource(this.project, resource, "material", messageBuilder.getMaterial());
        messageBuilder.setMaterial(BuilderUtil.replaceExt(messageBuilder.getMaterial(), ".material", ".materialc"));

        return messageBuilder;
    }
}
