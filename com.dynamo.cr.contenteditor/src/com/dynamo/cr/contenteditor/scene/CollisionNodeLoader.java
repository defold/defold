package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.CollisionResource;
import com.dynamo.cr.contenteditor.resource.ConvexShapeResource;
import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;

public class CollisionNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name,
            InputStream stream, INodeLoaderFactory factory,
            IResourceLoaderFactory resourceFactory, Node parent) throws IOException,
            LoaderException, CoreException {

        CollisionResource collisionResource = (CollisionResource) resourceFactory.load(monitor, name);
        ConvexShapeResource convexShapeResource = (ConvexShapeResource) resourceFactory.load(monitor, collisionResource.getCollisionDesc().getCollisionShape());
        return new CollisionNode(name, scene, collisionResource, convexShapeResource);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node,
            OutputStream stream, INodeLoaderFactory loaderFactory)
            throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }

}
