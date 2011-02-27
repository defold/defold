package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.ddf.DDF;
import com.dynamo.gameobject.ddf.GameObject;
import com.dynamo.gameobject.ddf.GameObject.ComponentDesc;

public class PrototypeNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, INodeLoaderFactory factory, IResourceLoaderFactory resourceFactory, Node parent) throws IOException, LoaderException, CoreException {

        InputStreamReader reader = new InputStreamReader(stream);

        PrototypeNode node = new PrototypeNode(scene, name);

        GameObject.PrototypeDesc desc = DDF.loadTextFormat(reader, GameObject.PrototypeDesc.class);
        for (ComponentDesc comp_desc : desc.m_Components) {

            Node comp;
            if (factory.canLoad(comp_desc.m_Resource)) {
                comp = factory.load(monitor, scene, comp_desc.m_Resource, node);
            } else {
                comp = new ComponentNode(scene, comp_desc.m_Resource);
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
