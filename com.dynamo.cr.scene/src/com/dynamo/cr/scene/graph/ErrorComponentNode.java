package com.dynamo.cr.scene.graph;

import com.dynamo.cr.scene.resource.DummyResource;

public class ErrorComponentNode extends ComponentNode<DummyResource> {

    public ErrorComponentNode(String resourceName, DummyResource resource,
            Scene scene) {
        super(resourceName, resource, scene);
    }

}
