package com.dynamo.cr.contenteditor.resource;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.contenteditor.scene.LoaderException;

public interface IResourceLoaderFactory {

    public Object load(IProgressMonitor monitor, String name) throws IOException, LoaderException, CoreException;

}
