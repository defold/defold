package com.dynamo.cr.contenteditor.scene.test;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

import com.dynamo.cr.contenteditor.scene.LoaderFactory;

public class TestLoaderFactory extends LoaderFactory {

    private String root;

    public TestLoaderFactory(String root) {
        this.root = root;
    }

    @Override
    protected InputStream getInputStream(String name) throws IOException {
        return new FileInputStream(root + "/" + name);
    }

}
