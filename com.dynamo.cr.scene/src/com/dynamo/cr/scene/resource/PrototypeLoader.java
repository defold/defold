package com.dynamo.cr.scene.resource;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

import com.dynamo.cr.scene.graph.CreateException;
import com.dynamo.cr.scene.math.MathUtil;
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
        List<Vector4d> componentTranslations = new ArrayList<Vector4d>();
        List<Quat4d> componentRotations = new ArrayList<Quat4d>();
        for (int i = 0; i < desc.getComponentsCount(); ++i) {
            String component = desc.getComponents(i).getComponent();
            if (factory.canLoad(component)) {
                componentResources.add((Resource)factory.load(componentProgress.newChild(1), component));
            } else {
                componentResources.add(new Resource(component));
            }
            componentTranslations.add(MathUtil.toVector4(desc.getComponents(i).getPosition()));
            componentRotations.add(MathUtil.toQuat4(desc.getComponents(i).getRotation()));
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
            componentTranslations.add(MathUtil.toVector4(desc.getEmbeddedComponents(i).getPosition()));
            componentRotations.add(MathUtil.toQuat4(desc.getEmbeddedComponents(i).getRotation()));
            componentProgress.worked(1);
        }

        return new PrototypeResource(name, desc, componentResources, componentTranslations, componentRotations);
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

        for (int i = 0; i < desc.getEmbeddedComponentsCount(); ++i) {
            EmbeddedComponentDesc embeddedDesc = desc.getEmbeddedComponents(i);
            String componentData = embeddedDesc.getData();
            String embeddedPath = name + "_embedded" + i + "." + embeddedDesc.getType();
            if (factory.canLoad(embeddedDesc.getType())) {
                ByteArrayInputStream is = new ByteArrayInputStream(componentData.getBytes());
                Resource embeddedResource = (Resource)factory.load(componentProgress.newChild(1), embeddedPath, is);
                embeddedResource.setEmbedded(true);
                componentResources.add(embeddedResource);
            } else {
                componentResources.add(new Resource(embeddedPath));
            }
            componentProgress.worked(1);
        }

        prototypeResource.setComponentResources(componentResources);
    }

}
