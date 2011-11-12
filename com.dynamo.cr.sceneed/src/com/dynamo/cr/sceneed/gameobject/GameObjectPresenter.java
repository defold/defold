package com.dynamo.cr.sceneed.gameobject;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeManager;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

public class GameObjectPresenter extends NodePresenter {

    @Inject private NodeManager manager;

    public void onAddComponent() {
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
        ComponentTypeNode child = null;
        String componentType = this.view.selectComponentType();
        if (componentType != null) {
            try {
                child = (ComponentTypeNode)this.manager.getPresenter(componentType).create(componentType);
            } catch (Exception e) {
                logException(e);
            }
        }
        if (child != null) {
            this.model.executeOperation(new AddComponentOperation(parent, new ComponentNode(child)));
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
    public Node load(String type, InputStream contents) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        GameObjectNode gameObject = new GameObjectNode();
        int n = desc.getComponentsCount();
        for (int i = 0; i < n; ++i) {
            ComponentDesc componentDesc = desc.getComponents(i);
            String path = componentDesc.getComponent();
            ComponentTypeNode componentType = (ComponentTypeNode)this.loader.load(path);
            RefComponentNode componentNode = new RefComponentNode(componentType);
            componentNode.setId(componentDesc.getId());
            componentNode.setReference(path);
            gameObject.addComponent(componentNode);
        }
        n = desc.getEmbeddedComponentsCount();
        for (int i = 0; i < n; ++i) {
            EmbeddedComponentDesc componentDesc = desc.getEmbeddedComponents(i);
            ComponentTypeNode componentType = (ComponentTypeNode)this.loader.load(componentDesc.getType(), new ByteArrayInputStream(componentDesc.getData().getBytes()));
            ComponentNode component = new ComponentNode(componentType);
            component.setId(componentDesc.getId());
            gameObject.addComponent(component);
        }
        return gameObject;
    }

    @Override
    public Node create(String type) throws IOException, CoreException {
        return new GameObjectNode();
    }
}
