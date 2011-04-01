package com.dynamo.cr.scene.test.graph;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import com.dynamo.cr.scene.graph.AbstractNodeLoaderFactory;
import com.dynamo.cr.scene.resource.IResourceLoaderFactory;

public class FileNodeLoaderFactory extends AbstractNodeLoaderFactory {

    private String root;

    public FileNodeLoaderFactory(String root, IResourceLoaderFactory resourceFactory) {
        super(resourceFactory);
        this.root = root;
    }

    @Override
    protected InputStream getInputStream(String name) throws IOException {
        return new FileInputStream(root + "/" + name);
    }

    @Override
    public void reportError(String message) {
        // TODO Auto-generated method stub

    }

    @Override
    public List<String> getErrors() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public void clearErrors() {
        // TODO Auto-generated method stub

    }

}
