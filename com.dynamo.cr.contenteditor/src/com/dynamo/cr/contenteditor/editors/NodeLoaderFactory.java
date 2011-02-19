package com.dynamo.cr.contenteditor.editors;

import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;

import com.dynamo.cr.contenteditor.resource.IResourceLoaderFactory;
import com.dynamo.cr.contenteditor.scene.AbstractNodeLoaderFactory;
import com.dynamo.cr.editor.core.EditorUtil;

public class NodeLoaderFactory extends AbstractNodeLoaderFactory {

    public NodeLoaderFactory(IResourceLoaderFactory resourceFactory) {
        super(resourceFactory);
    }

    private IContainer contentRoot;

    @Override
    protected InputStream getInputStream(String name) throws IOException {

        IFile f;
        if (name.startsWith("/")) {
            f = contentRoot.getWorkspace().getRoot().getFile(new Path(name));
            //System.out.println(f.getFullPath().toPortableString());
        }
        else {
            f = contentRoot.getFile(new Path(name));
        }

        try {
            // TODO: EFS REALLY NECCESSARY
            URI uri = f.getLocationURI();
            IFileStore store = EFS.getStore(uri);
            if (!store.fetchInfo().exists()) {
                throw new IOException(String.format("File %s doesn't exists", name));
            }

            return store.openInputStream(EFS.NONE, new NullProgressMonitor());

            //return f.getContents();
        } catch (CoreException e) {
            throw new IOException(e);
        }
    }

    public IContainer getContentRoot() {
        return contentRoot;
    }

    public boolean findContentRoot(IFile file) {
        this.contentRoot = EditorUtil.findContentRoot(file);
        return this.contentRoot != null;
    }

    ArrayList<String> errors = new ArrayList<String>();

    @Override
    public void reportError(String message) {
        this.errors.add(message);
    }

    @Override
    public void clearErrors() {
        this.errors.clear();
    }

    @Override
    public List<String> getErrors() {
        return Collections.unmodifiableList(errors);
    }

}
