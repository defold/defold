package com.dynamo.cr.sceneed.core;

import java.io.IOException;
import java.io.InputStream;

import javax.inject.Inject;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.Path;

public class NodeLoader {

    @Inject private NodeManager manager;
    @Inject private IContainer contentRoot;

    public Node load(String path) throws IOException, CoreException {
        return load(this.contentRoot.getFile(new Path(path)));
    }

    public Node load(IFile f) throws IOException, CoreException {
        return load(f.getFileExtension(), f.getContents());
    }

    public Node load(String type, InputStream is) throws IOException, CoreException {
        return this.manager.getPresenter(type).load(type, is);
    }
}
