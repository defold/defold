package com.dynamo.cr.scene.resource;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.LoaderException;

public interface IResourceLoaderFactory {

    public Object load(IProgressMonitor monitor, String name) throws IOException, LoaderException, CoreException;

}
