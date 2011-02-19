package com.dynamo.cr.contenteditor.scene.test;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import com.dynamo.cr.contenteditor.scene.AbstractNodeLoaderFactory;

public class TestNodeLoaderFactory extends AbstractNodeLoaderFactory {

    private String root;

    public TestNodeLoaderFactory(String root) {
        super(null);
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
