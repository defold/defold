package com.dynamo.cr.contenteditor.scene;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.List;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

public interface INodeLoaderFactory {

    public boolean canLoad(String name);

    public Node load(IProgressMonitor monitor, Scene scene, String name, InputStream stream, Node parent) throws IOException, LoaderException, CoreException;

    public Node load(IProgressMonitor monitor, Scene scene, String name, Node parent) throws IOException, LoaderException, CoreException;

    public void save(IProgressMonitor monitor, String name, Node node, ByteArrayOutputStream stream) throws IOException, LoaderException;

    public void reportError(String message);

    public List<String> getErrors();

    public void clearErrors();

}
