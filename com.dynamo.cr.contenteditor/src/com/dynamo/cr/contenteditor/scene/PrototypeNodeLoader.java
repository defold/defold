package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.google.protobuf.TextFormat;

public class PrototypeNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory, Node parent) throws IOException, LoaderException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);

        PrototypeNode node = new PrototypeNode(name, scene);

        PrototypeDesc.Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        for (ComponentDesc comp_desc : desc.getComponentsList()) {
            Node comp;
            if (factory.canLoad(comp_desc.getResource())) {
                comp = factory.load(monitor, scene, comp_desc.getResource(), node);
            } else {
                comp = new ComponentNode(comp_desc.getResource(), scene);
            }
            comp.setParent(node);
            // Do not add node here. setParent take care of tht
        }

        return node;
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node, OutputStream stream,
            INodeLoaderFactory loaderFactory) throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }

}
