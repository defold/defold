package com.dynamo.cr.scene.graph;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.Resource;

public class NodeFactory implements INodeFactory {

    Map<String, INodeCreator> creators;

    public void addCreator(String extension, INodeCreator creator) {
        this.creators.put(extension, creator);
    }

    INodeCreator getCreator(String name) {
        int i = name.lastIndexOf(".");

        if (i != -1) {
            INodeCreator loader = this.creators.get(name.substring(i+1));
            return loader;
        }
        return null;
    }

    public NodeFactory() {
        this.creators = new HashMap<String, INodeCreator>();
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.contenteditor.scene.INodeLoaderFactory#canLoad(java.lang.String)
     */
    @Override
    public boolean canCreate(String name) {
        return getCreator(name) != null;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.contenteditor.scene.INodeLoaderFactory#load(org.eclipse.core.runtime.IProgressMonitor, com.dynamo.cr.contenteditor.scene.Scene, java.lang.String, java.io.InputStream)
     */
    @Override
    public Node create(String identifier, Resource resource, Node parent, Scene scene) throws IOException, CreateException, CoreException {

        INodeCreator creator = getCreator(resource.getPath());
        if (creator != null) {
            return creator.create(identifier, resource, parent, scene, this);
        } else {
            throw new CreateException(String.format("No support for creating %s (%s).", identifier, resource.getPath()));
        }
    }

    private IContainer contentRoot;

    public IContainer getContentRoot() {
        return contentRoot;
    }

    public void setContentRoot(IContainer contentRoot) {
        this.contentRoot = contentRoot;
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
