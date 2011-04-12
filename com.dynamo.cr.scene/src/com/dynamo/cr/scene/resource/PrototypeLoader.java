package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.TextFormat;

public class PrototypeLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        List<Resource> componentResources = new ArrayList<Resource>(desc.getComponentsCount());
        for (int i = 0; i < desc.getComponentsCount(); ++i) {
            String component = desc.getComponents(i).getComponent();
            if (factory.canLoad(component)) {
                componentResources.add((Resource)factory.load(monitor, component));
            } else {
                componentResources.add(new Resource(component));
            }
        }
        return new PrototypeResource(name, desc, componentResources);
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory) throws IOException,
            CreateException, CoreException {
        InputStreamReader reader = new InputStreamReader(stream);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        PrototypeResource prototypeResource = (PrototypeResource)resource;
        prototypeResource.setDesc(desc);
        List<Resource> componentResources = new ArrayList<Resource>(desc.getComponentsCount());
        for (int i = 0; i < desc.getComponentsCount(); ++i) {
            String component = desc.getComponents(i).getComponent();
            if (factory.canLoad(component)) {
                componentResources.add((Resource)factory.load(monitor, desc.getComponents(i).getComponent()));
            } else {
                componentResources.add(new Resource(component));
            }
        }
        prototypeResource.setComponentResources(componentResources);
    }

}
