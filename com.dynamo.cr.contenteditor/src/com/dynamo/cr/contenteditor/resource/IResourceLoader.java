package com.dynamo.cr.contenteditor.resource;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.scene.LoaderException;

public interface IResourceLoader {

    Object load(IProgressMonitor monitor, String name, InputStream stream, IResourceLoaderFactory factory) throws IOException, LoaderException;

}
