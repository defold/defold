package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc.Builder;
import com.google.protobuf.TextFormat;

public class CollectionProxyLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);
        Builder b = CollectionProxyDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionProxyDesc collectionProxy = b.build();

        Resource collectionResource = factory.load(monitor, collectionProxy.getCollection());
        CollectionProxyResource resource = new CollectionProxyResource(name, collectionProxy, collectionResource);
        return resource;
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor,
            String name, InputStream stream, IResourceFactory factory)
            throws IOException, CreateException, CoreException {

        CollectionProxyResource collectionProxyResource = (CollectionProxyResource) resource;

        InputStreamReader reader = new InputStreamReader(stream);
        Builder b = CollectionProxyDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionProxyDesc collectionProxy = b.build();

        Resource collectionResource = factory.load(monitor, collectionProxy.getCollection());

        collectionProxyResource.setCollectionProxyDesc(collectionProxy);
        collectionProxyResource.setCollectionResource(collectionResource);
    }

}
