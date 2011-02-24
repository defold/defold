package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.cr.contenteditor.resource.LightResource;

public class LightNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name,
            InputStream stream, INodeLoaderFactory factory,
            IResourceLoaderFactory resourceFactory, Node parent) throws IOException,
            LoaderException, CoreException {

        LightResource lightResource = (LightResource) resourceFactory.load(monitor, name);
        return new LightNode(scene, name, lightResource);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node,
            OutputStream stream, INodeLoaderFactory loaderFactory)
            throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }

}
