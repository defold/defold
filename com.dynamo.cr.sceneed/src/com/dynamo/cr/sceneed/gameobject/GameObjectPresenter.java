package com.dynamo.cr.sceneed.gameobject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeModel;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.cr.sceneed.sprite.SpriteNode;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

public class GameObjectPresenter extends NodePresenter {

    @Inject
    public GameObjectPresenter(NodeModel model, INodeView view) {
        super(model, view);
    }

    public void onAddComponent(String componentType) {
        // Find selected game objects
        // TODO: Support multi selection
        IStructuredSelection structuredSelection = this.model.getSelection();
        Object[] nodes = structuredSelection.toArray();
        GameObjectNode parent = null;
        for (Object node : nodes) {
            if (node instanceof GameObjectNode) {
                parent = (GameObjectNode)node;
                break;
            } else if (node instanceof ComponentNode) {
                parent = (GameObjectNode)((Node)node).getParent();
                break;
            }
        }
        if (parent == null) {
            throw new UnsupportedOperationException("No game object in selection.");
        }
        // TODO: Use node factory
        ComponentNode child = null;
        if (componentType.equals("sprite")) {
            child = new ComponentNode(new SpriteNode());
        }
        if (child != null) {
            this.model.executeOperation(new AddComponentOperation(parent, child));
        } else {
            throw new UnsupportedOperationException("Component type " + componentType + " not registered.");
        }
    }

    public void onRemoveComponent() {
        // Find selected components
        // TODO: Support multi selection
        IStructuredSelection structuredSelection = this.model.getSelection();
        Object[] nodes = structuredSelection.toArray();
        ComponentNode component = null;
        for (Object node : nodes) {
            if (node instanceof ComponentNode) {
                component = (ComponentNode)node;
                break;
            }
        }
        this.model.executeOperation(new RemoveComponentOperation(component));
    }

    @Override
    public void onLoad(InputStream contents) throws IOException {
        // TODO: Move to loader
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        @SuppressWarnings("unused")
        PrototypeDesc desc = builder.build();
        GameObjectNode gameObject = new GameObjectNode();
        // TODO: Fill out game object
        this.model.setRoot(gameObject);
    }
}
