package com.dynamo.cr.spine.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.spine.proto.Spine.SpineModelDesc;
import com.dynamo.spine.proto.Spine.SpineModelDesc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class SpineModelLoader implements INodeLoader<SpineModelNode> {

    @Override
    public SpineModelNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = SpineModelDesc.newBuilder();
        TextFormat.merge(reader, builder);
        SpineModelDesc ddf = builder.build();
        SpineModelNode node = new SpineModelNode();
        node.setSpineScene(ddf.getSpineScene());
        node.setDefaultAnimation(ddf.getDefaultAnimation());
        node.setSkin(ddf.getSkin());
        node.setMaterial(ddf.getMaterial());
        node.setBlendMode(ddf.getBlendMode());
        int tc = ddf.getTexturesCount();
        node.setTexture0("");
        node.setTexture1(tc > 1 ? ddf.getTextures(1) : "");
        node.setTexture2(tc > 2 ? ddf.getTextures(2) : "");
        node.setTexture3(tc > 3 ? ddf.getTextures(3) : "");
        node.setTexture4(tc > 4 ? ddf.getTextures(4) : "");
        node.setTexture5(tc > 5 ? ddf.getTextures(5) : "");
        node.setTexture6(tc > 6 ? ddf.getTextures(6) : "");
        node.setTexture7(tc > 7 ? ddf.getTextures(7) : "");
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, SpineModelNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        SpineModelDesc.Builder builder = SpineModelDesc.newBuilder();
        builder.setSpineScene(node.getSpineScene()).setDefaultAnimation(node.getDefaultAnimation())
                .setSkin(node.getSkin()).setMaterial(node.getMaterial()).setBlendMode(node.getBlendMode());
        builder.addTextures("");
        builder.addTextures(node.getTexture1());
        builder.addTextures(node.getTexture2());
        builder.addTextures(node.getTexture3());
        builder.addTextures(node.getTexture4());
        builder.addTextures(node.getTexture5());
        builder.addTextures(node.getTexture6());
        builder.addTextures(node.getTexture7());
        return builder.build();
    }
}
