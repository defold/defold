package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.CameraResource;
import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;

public class CameraNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name,
            InputStream stream, INodeLoaderFactory factory,
            IResourceLoaderFactory resourceFactory, Node parent) throws IOException,
            LoaderException, CoreException {

        CameraResource cameraResource = (CameraResource) resourceFactory.load(monitor, name);
        return new CameraNode(scene, name, cameraResource);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node,
            OutputStream stream, INodeLoaderFactory loaderFactory)
            throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }

}
