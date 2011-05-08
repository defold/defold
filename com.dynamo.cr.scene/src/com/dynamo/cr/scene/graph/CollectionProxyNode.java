package com.dynamo.cr.scene.graph;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.CollectionProxyResource;
import com.dynamo.cr.scene.resource.Resource;

public class CollectionProxyNode extends ComponentNode<CollectionProxyResource> {

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource, Node parent, Scene scene, INodeFactory factory)
                    throws IOException, CreateException, CoreException {
                return new CollectionProxyNode(identifier, (CollectionProxyResource)resource, scene, factory);
            }
        };
    }

    public CollectionProxyNode(String identifier, CollectionProxyResource resource, Scene scene, INodeFactory factory) throws CreateException, IOException, CoreException {
        super(identifier, resource, scene);

        Node collection = factory.create(resource.getCollectionProxyDesc().getCollection(), resource.getCollectionResource(), this, scene);
        collection.setParent(this);
        Node.orFlags(collection, Node.FLAG_GHOST);
        Node.andFlags(collection, ~(Node.FLAG_EDITABLE | Node.FLAG_TRANSFORMABLE));
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof CollectionNode;
    }

    @Override
    public void draw(DrawContext context) {
    }
}
