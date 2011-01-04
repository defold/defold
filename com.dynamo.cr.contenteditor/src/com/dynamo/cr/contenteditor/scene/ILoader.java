package com.dynamo.cr.contenteditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

import org.eclipse.core.runtime.IProgressMonitor;

public interface ILoader {

    Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, LoaderFactory factory) throws IOException, LoaderException;

    void save(IProgressMonitor monitor, String name, Node node, OutputStream stream, LoaderFactory loaderFactory) throws IOException, LoaderException;
}
