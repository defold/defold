package com.dynamo.cr.contenteditor.scene;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;

public abstract class AbstractNodeLoaderFactory implements INodeLoaderFactory {

    Map<String, INodeLoader> loaders = new HashMap<String, INodeLoader>();
    private IResourceLoaderFactory resourceFactory;

    public void addLoader(INodeLoader loader, String extension) {
        loaders.put(extension, loader);
    }

    INodeLoader getLoader(String name) {
        int i = name.lastIndexOf(".");

        if (i != -1) {
            INodeLoader loader = loaders.get(name.substring(i+1));
            return loader;
        }
        return null;
    }

    public AbstractNodeLoaderFactory(IResourceLoaderFactory resourceFactory) {
        this.resourceFactory = resourceFactory;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.contenteditor.scene.INodeLoaderFactory#canLoad(java.lang.String)
     */
    @Override
    public boolean canLoad(String name) {
        return getLoader(name) != null;
    }

    protected abstract InputStream getInputStream(String name) throws IOException;

    /* (non-Javadoc)
     * @see com.dynamo.cr.contenteditor.scene.INodeLoaderFactory#load(org.eclipse.core.runtime.IProgressMonitor, com.dynamo.cr.contenteditor.scene.Scene, java.lang.String, java.io.InputStream)
     */
    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, Node parent) throws IOException, LoaderException, CoreException {

        INodeLoader loader = getLoader(name);
        if (loader != null) {
            try {
                Node node = loader.load(monitor, scene, name, stream, this, resourceFactory, parent);
                return node;
            }
            finally {
                stream.close();
            }
        }
        else {
            throw new LoaderException("No support for loading " + name);
        }
    }

    Map<String, byte[]> cache = new HashMap<String, byte[]>();

    static void copy(InputStream in, OutputStream out)
            throws IOException {
        byte[] b = new byte[10 * 1024];
        int read;
        while ((read = in.read(b)) != -1) {
            out.write(b, 0, read);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.contenteditor.scene.INodeLoaderFactory#load(org.eclipse.core.runtime.IProgressMonitor, com.dynamo.cr.contenteditor.scene.Scene, java.lang.String)
     */
    @Override
    public Node load(IProgressMonitor monitor, Scene scene, String name, Node parent) throws IOException, LoaderException, CoreException {

        INodeLoader loader = getLoader(name);
        if (loader != null) {

            if (!cache.containsKey(name)) {
                InputStream stream = getInputStream(name);
                ByteArrayOutputStream out = new ByteArrayOutputStream(128 * 1024);
                copy(stream, out);
                cache.put(name, out.toByteArray());
            }

            ByteArrayInputStream stream = new ByteArrayInputStream(cache.get(name));

            try {
                Node node = loader.load(monitor, scene, name, stream, this, resourceFactory, parent);
                return node;
            }
            finally {
                stream.close();
            }
        }
        else {
            throw new LoaderException("No support for loading " + name);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.contenteditor.scene.INodeLoaderFactory#save(org.eclipse.core.runtime.IProgressMonitor, java.lang.String, com.dynamo.cr.contenteditor.scene.Node, java.io.ByteArrayOutputStream)
     */
    @Override
    public void save(IProgressMonitor monitor, String name, Node node, ByteArrayOutputStream stream) throws IOException, LoaderException {
        INodeLoader loader = getLoader(name);
        if (loader != null) {
            try {
                loader.save(monitor, name, node, stream, this);
            }
            finally {
                stream.close();
            }
        }
        else {
            throw new LoaderException("No support for saving " + name);
        }
    }

}
