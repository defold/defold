package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

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
        SubMonitor progress = SubMonitor.convert(monitor);
        progress.setTaskName("Loading " + name);
        InputStreamReader reader = new InputStreamReader(stream);
        CollectionDesc.Builder b = CollectionDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionDesc desc = b.build();
        CollectionResource resource = new CollectionResource(name, desc);
        setup(resource, progress, factory);
        return resource;
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        SubMonitor progress = SubMonitor.convert(monitor);
        progress.setTaskName("Reloading " + name);
        InputStreamReader reader = new InputStreamReader(stream);
        CollectionDesc.Builder b = CollectionDesc.newBuilder();
        TextFormat.merge(reader, b);
        CollectionDesc desc = b.build();
        CollectionResource collectionResource = (CollectionResource)resource;
        collectionResource.setCollectionDesc(desc);
        setup(collectionResource, progress, factory);
    }

    private void setup(CollectionResource resource, SubMonitor monitor, IResourceFactory factory) throws CreateException {
        monitor.setWorkRemaining(5);
        CollectionDesc desc = resource.getCollectionDesc();
        SubMonitor collectionInstanceProgress = monitor.newChild(2).setWorkRemaining(desc.getCollectionInstancesCount());
        List<CollectionResource> collectionInstances = new ArrayList<CollectionResource>(desc.getCollectionInstancesCount());
        for (CollectionInstanceDesc cid : desc.getCollectionInstancesList()) {
            CollectionResource instance;
            try {
                instance = (CollectionResource)factory.load(collectionInstanceProgress.newChild(1), cid.getCollection());
            } catch (IOException e) {
                instance = new CollectionResource(cid.getCollection(), null);
            } catch (CoreException e) {
                instance = new CollectionResource(cid.getCollection(), null);
            }
            collectionInstances.add(instance);
        }
        resource.setCollectionInstances(collectionInstances);
        SubMonitor instanceProgress = monitor.newChild(2).setWorkRemaining(desc.getInstancesCount());
        List<PrototypeResource> prototypeInstances = new ArrayList<PrototypeResource>(desc.getInstancesCount());
        for (InstanceDesc cid : desc.getInstancesList()) {
            PrototypeResource instance;
            try {
                instance = (PrototypeResource)factory.load(instanceProgress.newChild(1), cid.getPrototype());
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
