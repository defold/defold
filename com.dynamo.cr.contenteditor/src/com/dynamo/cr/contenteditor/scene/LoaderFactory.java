package com.dynamo.cr.contenteditor.scene;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IProgressMonitor;

public abstract class LoaderFactory {

    Map<String, ILoader> loaders = new HashMap<String, ILoader>();

    public void addLoader(ILoader loader, String extension) {
        loaders.put(extension, loader);
    }

    ILoader getLoader(String name) {
        int i = name.lastIndexOf(".");

        if (i != -1) {
            ILoader loader = loaders.get(name.substring(i+1));
            return loader;
        }
        return null;
    }

    public boolean canLoad(String name) {
        return getLoader(name) != null;
    }

    protected abstract InputStream getInputStream(String name) throws IOException;

    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream) throws IOException, LoaderException {

        ILoader loader = getLoader(name);
        if (loader != null) {
            try {
                Node node = loader.load(monitor, scene, name, stream, this);
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



    public Node load(IProgressMonitor monitor, Scene scene, String name) throws IOException, LoaderException {

        ILoader loader = getLoader(name);
        if (loader != null) {

            if (!cache.containsKey(name)) {
                InputStream stream = getInputStream(name);
                ByteArrayOutputStream out = new ByteArrayOutputStream(128 * 1024);
                copy(stream, out);
                cache.put(name, out.toByteArray());
            }

            ByteArrayInputStream stream = new ByteArrayInputStream(cache.get(name));

            try {
                Node node = loader.load(monitor, scene, name, stream, this);
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

    public void save(IProgressMonitor monitor, String name, Node node, ByteArrayOutputStream stream) throws IOException, LoaderException {
        ILoader loader = getLoader(name);
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
