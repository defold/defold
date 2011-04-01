package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;

import javax.imageio.ImageIO;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.LoaderException;

public class TextureLoader implements IResourceLoader {

    @Override
    public Object load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceLoaderFactory factory)
            throws IOException, LoaderException {

        return new TextureResource(ImageIO.read(stream));
    }

}
