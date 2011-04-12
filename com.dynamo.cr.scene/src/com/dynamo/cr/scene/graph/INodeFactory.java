package com.dynamo.cr.scene.graph;

import java.io.IOException;
import java.util.List;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.Resource;

public interface INodeFactory {

    public boolean canCreate(String name);

    public Node create(String identifier, Resource resource, Node parent, Scene scene) throws IOException, CreateException, CoreException;

    public void reportError(String message);

    public List<String> getErrors();

    public void clearErrors();

}
