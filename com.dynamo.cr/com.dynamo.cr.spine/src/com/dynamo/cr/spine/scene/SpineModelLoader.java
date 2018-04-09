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
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, SpineModelNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        SpineModelDesc.Builder builder = SpineModelDesc.newBuilder();
        builder.setSpineScene(node.getSpineScene()).setDefaultAnimation(node.getDefaultAnimation())
                .setSkin(node.getSkin()).setMaterial(node.getMaterial()).setBlendMode(node.getBlendMode());
        return builder.build();
    }
}
