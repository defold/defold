package com.dynamo.cr.scene.resource;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.TextFormat;

public class PrototypeLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        SubMonitor progress = SubMonitor.convert(monitor);
        progress.setTaskName("Loading " + name);
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        progress.setWorkRemaining(5);
        SubMonitor componentProgress = progress.newChild(1).setWorkRemaining(desc.getComponentsCount() + desc.getEmbeddedComponentsCount());
        List<Resource> componentResources = new ArrayList<Resource>(desc.getComponentsCount());
        for (int i = 0; i < desc.getComponentsCount(); ++i) {
            String component = desc.getComponents(i).getComponent();
            if (factory.canLoad(component)) {
                componentResources.add((Resource)factory.load(componentProgress.newChild(1), component));
            } else {
                componentResources.add(new Resource(component));
            }
            componentProgress.worked(1);
        }

        for (int i = 0; i < desc.getEmbeddedComponentsCount(); ++i) {
            EmbeddedComponentDesc embeddedDesc = desc.getEmbeddedComponents(i);
            String componentData = embeddedDesc.getData();
            String embeddedPath = name + "_embedded" + i + "." + embeddedDesc.getType();
            if (factory.canLoad(embeddedDesc.getType())) {
                ByteArrayInputStream is = new ByteArrayInputStream(componentData.getBytes());
                Resource resource = (Resource)factory.load(componentProgress.newChild(1), embeddedPath, is);
                resource.setEmbedded(true);
                componentResources.add(resource);
            } else {
                componentResources.add(new Resource(embeddedPath));
            }
            componentProgress.worked(1);
        }

        return new PrototypeResource(name, desc, componentResources);
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        SubMonitor progress = SubMonitor.convert(monitor);
        progress.setTaskName("Reloading " + name);
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        progress.setWorkRemaining(10);
        SubMonitor componentProgress = progress.newChild(1).setWorkRemaining(desc.getComponentsCount());
        PrototypeResource prototypeResource = (PrototypeResource)resource;
        prototypeResource.setDesc(desc);
        List<Resource> componentResources = new ArrayList<Resource>(desc.getComponentsCount());
        for (int i = 0; i < desc.getComponentsCount(); ++i) {
            String component = desc.getComponents(i).getComponent();
            if (factory.canLoad(component)) {
                componentResources.add((Resource)factory.load(componentProgress.newChild(1), desc.getComponents(i).getComponent()));
            } else {
                componentResources.add(new Resource(component));
            }
            componentProgress.setWorkRemaining(desc.getComponentsCount()-i-1);
        }
        prototypeResource.setComponentResources(componentResources);
    }

}
