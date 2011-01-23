package com.dynamo.cr.contenteditor.scene.test;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

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

}
