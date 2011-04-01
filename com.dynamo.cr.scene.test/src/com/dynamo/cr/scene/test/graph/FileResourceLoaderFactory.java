package com.dynamo.cr.scene.test.graph;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.LoaderException;
import com.dynamo.cr.scene.resource.IResourceLoader;
import com.dynamo.cr.scene.resource.IResourceLoaderFactory;

public class FileResourceLoaderFactory implements IResourceLoaderFactory {

    private String root;
    Map<String, IResourceLoader> loaders = new HashMap<String, IResourceLoader>();

    public FileResourceLoaderFactory(String root) {
        this.root = root;
    }

    public void addLoader(IResourceLoader loader, String extension) {
        loaders.put(extension, loader);
    }

    private IResourceLoader getLoader(String name) {
        int i = name.lastIndexOf(".");

        if (i != -1) {
            IResourceLoader loader = loaders.get(name.substring(i+1));
            return loader;
        }
        return null;
    }

    protected InputStream getInputStream(String name) throws IOException, CoreException {
        return new FileInputStream(name);
    }

    @Override
    public Object load(IProgressMonitor monitor, String name)
            throws IOException, LoaderException, CoreException {
        IResourceLoader loader = getLoader(name);
        if (loader == null)
            throw new LoaderException("No support for loading " + name);
        return loader.load(monitor, name, new FileInputStream(root + "/" + name), this);
    }
}
