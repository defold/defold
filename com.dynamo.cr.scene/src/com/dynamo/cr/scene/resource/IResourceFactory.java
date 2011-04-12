package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;

public interface IResourceFactory {

    public boolean canLoad(String path);
    public Resource load(IProgressMonitor monitor, String path) throws IOException, CreateException, CoreException;
    public Resource load(IProgressMonitor monitor, String path, InputStream in) throws IOException, CreateException, CoreException;

}
