package com.dynamo.cr.tileeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc;
import com.dynamo.sprite2.proto.Sprite2.Sprite2Desc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class Sprite2Loader implements INodeLoader<Sprite2Node> {

    @Override
    public Sprite2Node load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = Sprite2Desc.newBuilder();
        TextFormat.merge(reader, builder);
        Sprite2Desc ddf = builder.build();
        Sprite2Node node = new Sprite2Node();
        node.setTileSet(ddf.getTileSet());
        node.setDefaultAnimation(ddf.getDefaultAnimation());
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, Sprite2Node node,
            IProgressMonitor monitor) throws IOException, CoreException {
        Sprite2Desc.Builder builder = Sprite2Desc.newBuilder();
        builder.setTileSet(node.getTileSet()).setDefaultAnimation(node.getDefaultAnimation());
        return builder.build();
    }
}
