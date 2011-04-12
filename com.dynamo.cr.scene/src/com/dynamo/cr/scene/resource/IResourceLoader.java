package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.CreateException;

public interface IResourceLoader {

    Resource load(IProgressMonitor monitor, String name, InputStream stream, IResourceFactory factory) throws IOException, CreateException, CoreException;
    void reload(Resource resource, IProgressMonitor monitor, String name, InputStream stream, IResourceFactory factory) throws IOException, CreateException, CoreException;
}
