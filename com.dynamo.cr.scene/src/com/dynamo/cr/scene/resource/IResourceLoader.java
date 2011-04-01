package com.dynamo.cr.scene.resource;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.scene.graph.LoaderException;

public interface IResourceLoader {

    Object load(IProgressMonitor monitor, String name, InputStream stream, IResourceLoaderFactory factory) throws IOException, LoaderException;

}
