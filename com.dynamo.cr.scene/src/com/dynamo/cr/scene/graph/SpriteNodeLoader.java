package com.dynamo.cr.scene.graph;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.resource.IResourceLoaderFactory;
import com.dynamo.cr.scene.resource.SpriteResource;
import com.dynamo.cr.scene.resource.TextureResource;

public class SpriteNodeLoader implements INodeLoader {

    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name,
            InputStream stream, INodeLoaderFactory factory,
            IResourceLoaderFactory resourceFactory, Node parent) throws IOException,
            LoaderException, CoreException {

        SpriteResource spriteResource = (SpriteResource) resourceFactory.load(monitor, name);
        TextureResource textureResource = (TextureResource) resourceFactory.load(monitor, spriteResource.getSpriteDesc().getTexture());
        return new SpriteNode(name, scene, spriteResource, textureResource);
    }

    @Override
    public void save(IProgressMonitor monitor, String name, Node node,
            OutputStream stream, INodeLoaderFactory loaderFactory)
            throws IOException, LoaderException {
        throw new UnsupportedOperationException();
    }

}
