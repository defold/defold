package com.dynamo.cr.contenteditor.resource;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.contenteditor.scene.LoaderException;

public class ResourceLoaderFactory implements IResourceLoaderFactory {

    private IContainer contentRoot;
    private Map<Path, Object> resources = new HashMap<Path, Object>();

    Map<String, IResourceLoader> loaders = new HashMap<String, IResourceLoader>();

    public void addLoader(IResourceLoader loader, String extension) {
        loaders.put(extension, loader);
    }

    public ResourceLoaderFactory(IContainer contentRoot) {
        this.contentRoot = contentRoot;
    }

    IResourceLoader getLoader(String name) {
        int i = name.lastIndexOf(".");

        if (i != -1) {
            IResourceLoader loader = loaders.get(name.substring(i+1));
            return loader;
        }
        return null;
    }

    protected InputStream getInputStream(String name) throws IOException, CoreException {
        IFile f;
        if (name.startsWith("/")) {
            f = contentRoot.getWorkspace().getRoot().getFile(new Path(name));
        }
        else {
            f = contentRoot.getFile(new Path(name));
        }
        return f.getContents();
    }

    @Override
    public Object load(IProgressMonitor monitor, String name)
            throws IOException, LoaderException, CoreException {

        // Normalize path by using the path-class
        Path path = new Path(name);
        if (!resources.containsKey(path)) {
            IResourceLoader loader = getLoader(name);
            if (loader == null)
                throw new LoaderException("No support for loading " + name);

            InputStream stream = getInputStream(name);
            Object resource = loader.load(monitor, name, stream, this);
            resources.put(path, resource);
        }

        return resources.get(path);
    }

}
