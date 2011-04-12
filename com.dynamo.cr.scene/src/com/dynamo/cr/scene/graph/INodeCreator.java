package com.dynamo.cr.scene.graph;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.Resource;

public interface INodeCreator {

    Node create(String identifier, Resource resource, Node parent, Scene scene, INodeFactory factory) throws IOException, CreateException, CoreException;

}
