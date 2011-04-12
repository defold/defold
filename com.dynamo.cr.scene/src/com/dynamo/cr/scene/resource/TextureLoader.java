package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;

import javax.imageio.ImageIO;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;

public class TextureLoader implements IResourceLoader {

    @Override
    public Resource load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {

        return new TextureResource(name, ImageIO.read(stream));
    }

    @Override
    public void reload(Resource resource, IProgressMonitor monitor, String name,
            InputStream stream, IResourceFactory factory)
            throws IOException, CreateException {
        ((TextureResource)resource).setImage(ImageIO.read(stream));
    }
}
