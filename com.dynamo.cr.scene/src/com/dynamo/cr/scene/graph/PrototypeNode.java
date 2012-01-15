package com.dynamo.cr.scene.graph;

import java.io.IOException;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.scene.resource.DummyResource;
import com.dynamo.cr.scene.resource.IResourceListener;
import com.dynamo.cr.scene.resource.PrototypeResource;
import com.dynamo.cr.scene.resource.Resource;


public class PrototypeNode extends Node implements IResourceListener {

    public static INodeCreator getCreator() {
        return new INodeCreator() {

            @Override
            public Node create(String identifier, Resource resource, Node parent,
                    Scene scene, INodeFactory factory) throws IOException,
                    CreateException, CoreException {
                PrototypeResource prototypeResource = (PrototypeResource)resource;
                return new PrototypeNode(identifier, prototypeResource, scene, factory);
            }
        };
    }

    private PrototypeResource resource;
    private INodeFactory factory;

    public PrototypeNode(String identifier, PrototypeResource resource, Scene scene, INodeFactory factory) {
        super(identifier, scene, FLAG_CAN_HAVE_CHILDREN);
        this.resource = resource;
        this.resource.addListener(this);
        this.factory = factory;
        setup();
    }

    @Override
    protected void finalize() throws Throwable {
        this.resource.removeListener(this);
        super.finalize();
    }

    public PrototypeResource getResource() {
        return this.resource;
    }

    @Override
    public void nodeAdded(Node node) {
        assert (node instanceof ComponentNode);
    }

    @Override
    public void draw(DrawContext context) {
    }

    @Override
    protected boolean verifyChild(Node child) {
        return child instanceof ComponentNode;
    }

    private void setup() {
        boolean canHaveChildren = (getFlags() & FLAG_CAN_HAVE_CHILDREN) == FLAG_CAN_HAVE_CHILDREN;
        setFlags(getFlags() | FLAG_CAN_HAVE_CHILDREN);
        clearNodes();
        int i = 0;

        for (Resource componentResource : resource.getComponentResources()) {
            Node comp;
            if (factory.canCreate(componentResource.getPath())) {
                try {
                    comp = factory.create(componentResource.getPath(), componentResource, this, getScene());
                } catch (Exception e) {
                    comp = new ErrorComponentNode(componentResource.getPath(), new DummyResource(), getScene());
                    comp.setError(ERROR_FLAG_RESOURCE_ERROR, e.getMessage());
                }
            } else {
                comp = new ErrorComponentNode(componentResource.getPath(), new DummyResource(), getScene());
            }
            comp.setLocalTranslation(resource.getComponentTranslations().get(i));
            comp.setLocalRotation(resource.getComponentRotations().get(i));
            comp.setParent(this);
            ++i;
        }
        if (!canHaveChildren) {
            setFlags(getFlags() & ~FLAG_CAN_HAVE_CHILDREN);
        }
    }

    @Override
    public void resourceChanged(Resource resource) {
        setup();
        getScene().fireSceneEvent(new SceneEvent(SceneEvent.NODE_CHANGED, this));
    }
}
