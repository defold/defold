package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class CollectionProxyLoader implements INodeLoader<CollectionProxyNode> {

    @Override
    public CollectionProxyNode load(ILoaderContext context, InputStream contents)
            throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        CollectionProxyDesc.Builder builder = CollectionProxyDesc.newBuilder();
        TextFormat.merge(reader, builder);
        CollectionProxyDesc desc = builder.build();
        CollectionProxyNode collectionProxy = new CollectionProxyNode();
        collectionProxy.setCollection(desc.getCollection());
        collectionProxy.setCollectionNode((CollectionNode)context.loadNode(desc.getCollection()));
        return collectionProxy;
    }

    @Override
    public Message buildMessage(ILoaderContext context, CollectionProxyNode node,
            IProgressMonitor monitor) throws IOException, CoreException {
        CollectionProxyDesc.Builder builder = CollectionProxyDesc.newBuilder();
        CollectionProxyNode collectionProxy = node;
        builder.setCollection(collectionProxy.getCollection());
        return builder.build();
    }

}
