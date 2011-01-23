package com.dynamo.cr.contenteditor.resource;

import java.io.IOException;
import java.io.InputStream;

import javax.imageio.ImageIO;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.editors.TextureResource;
import com.dynamo.cr.contenteditor.scene.LoaderException;

public class TextureLoader implements IResourceLoader {

    @Override
    public Object load(IProgressMonitor monitor, String name,
            InputStream stream, IResourceLoaderFactory factory)
            throws IOException, LoaderException {

        return new TextureResource(ImageIO.read(stream));
    }

}
