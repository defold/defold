package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.gamesystem.proto.GameSystem.CollectionFactoryDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class CollectionFactoryLoader implements INodeLoader<CollectionFactoryNode> {

    @Override
    public CollectionFactoryNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        CollectionFactoryDesc.Builder builder = CollectionFactoryDesc.newBuilder();
        TextFormat.merge(reader, builder);
        CollectionFactoryDesc desc = builder.build();
        CollectionFactoryNode factory = new CollectionFactoryNode();
        factory.setPrototype(desc.getPrototype());
        factory.setLoadDynamically(desc.getLoadDynamically());
        return factory;
    }

    @Override
    public Message buildMessage(ILoaderContext context, CollectionFactoryNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        CollectionFactoryDesc.Builder builder = CollectionFactoryDesc.newBuilder();
        builder.setPrototype(node.getPrototype());
        builder.setLoadDynamically(node.getLoadDynamically());
        return builder.build();
    }

}
