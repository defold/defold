package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.google.protobuf.TextFormat;

public class CollectionLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        InputStreamReader reader = new InputStreamReader(stream);
        CollectionDesc.Builder b = CollectionDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionDesc desc = b.build();
        monitor.beginTask("Loading " + name, desc.getCollectionInstancesCount() + desc.getInstancesCount());
        CollectionResource resource = new CollectionResource(name, desc);
        setup(resource, monitor, factory);
        monitor.done();
        return resource;
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        InputStreamReader reader = new InputStreamReader(stream);
        CollectionDesc.Builder b = CollectionDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionDesc desc = b.build();
        CollectionResource collectionResource = (CollectionResource)resource;
        collectionResource.setCollectionDesc(desc);
        monitor.beginTask("Reloading " + name, desc.getCollectionInstancesCount() + desc.getInstancesCount());
        setup(collectionResource, monitor, factory);
        monitor.done();
    }

    private void setup(CollectionResource resource, IProgressMonitor monitor, IResourceFactory factory) throws CreateException {
        CollectionDesc desc = resource.getCollectionDesc();
        List<CollectionResource> collectionInstances = new ArrayList<CollectionResource>(desc.getCollectionInstancesCount());
        for (CollectionInstanceDesc cid : desc.getCollectionInstancesList()) {
            CollectionResource instance;
            try {
                instance = (CollectionResource)factory.load(new SubProgressMonitor(monitor, 1), cid.getCollection());
            } catch (IOException e) {
                instance = new CollectionResource(cid.getCollection(), null);
            } catch (CoreException e) {
                instance = new CollectionResource(cid.getCollection(), null);
            }
            collectionInstances.add(instance);
        }
        resource.setCollectionInstances(collectionInstances);
        List<PrototypeResource> prototypeInstances = new ArrayList<PrototypeResource>(desc.getInstancesCount());
        for (InstanceDesc cid : desc.getInstancesList()) {
            PrototypeResource instance;
            try {
                instance = (PrototypeResource)factory.load(new SubProgressMonitor(monitor, 1), cid.getPrototype());
            } catch (IOException e) {
                instance = new PrototypeResource(cid.getPrototype(), null, new ArrayList<Resource>());
            } catch (CoreException e) {
                instance = new PrototypeResource(cid.getPrototype(), null, new ArrayList<Resource>());
            }
            prototypeInstances.add(instance);
        }
        resource.setPrototypeInstances(prototypeInstances);
    }

}
