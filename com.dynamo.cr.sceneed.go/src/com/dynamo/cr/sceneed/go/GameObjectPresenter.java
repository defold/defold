package com.dynamo.cr.sceneed.go;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.ISceneView.Context;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.go.operations.AddComponentOperation;
import com.dynamo.cr.sceneed.go.operations.RemoveComponentOperation;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GameObjectPresenter implements ISceneView.NodePresenter {

    private GameObjectNode findGameObjectFromSelection(IStructuredSelection selection) {
        Object[] nodes = selection.toArray();
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
        return parent;
    }

    public void onAddComponent(Context context) {
        // Find selected game objects
        // TODO: Support multi selection
        GameObjectNode parent = findGameObjectFromSelection(context.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No game object in selection.");
        }
        String componentType = context.getView().selectComponentType();
        if (componentType != null) {
            ComponentTypeNode child = null;
            try {
                child = (ComponentTypeNode)context.getPresenter(componentType).onCreateNode(context, componentType);
            } catch (Exception e) {
                context.logException(e);
            }
            if (child != null) {
                context.executeOperation(new AddComponentOperation(parent, new ComponentNode(child)));
            } else {
                throw new UnsupportedOperationException("Component type " + componentType + " not registered.");
            }
        }
    }

    public void onAddComponentFromFile(Context context) {
        // Find selected game objects
        // TODO: Support multi selection
        GameObjectNode parent = findGameObjectFromSelection(context.getSelection());
        if (parent == null) {
            throw new UnsupportedOperationException("No game object in selection.");
        }
        String path = context.getView().selectComponentFromFile();
        if (path != null) {
            ComponentTypeNode child = null;
            try {
                child = (ComponentTypeNode)context.loadNode(path);
            } catch (Exception e) {
                context.logException(e);
            }
            if (child != null) {
                RefComponentNode component = new RefComponentNode(child);
                component.setComponent(path);
                context.executeOperation(new AddComponentOperation(parent, component));
            } else {
                throw new UnsupportedOperationException("Component " + path + " has unknown type.");
            }
        }
    }

    public void onRemoveComponent(Context context) {
        // Find selected components
        // TODO: Support multi selection
        IStructuredSelection structuredSelection = context.getSelection();
        Object[] nodes = structuredSelection.toArray();
        ComponentNode component = null;
        for (Object node : nodes) {
            if (node instanceof ComponentNode) {
                component = (ComponentNode)node;
                break;
            }
        }
        context.executeOperation(new RemoveComponentOperation(component));
    }

    @Override
    public Node onLoad(Context context, String type, InputStream contents) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = PrototypeDesc.newBuilder();
        TextFormat.merge(reader, builder);
        PrototypeDesc desc = builder.build();
        GameObjectNode gameObject = new GameObjectNode();
        int n = desc.getComponentsCount();
        for (int i = 0; i < n; ++i) {
            ComponentDesc componentDesc = desc.getComponents(i);
            String path = componentDesc.getComponent();
            ComponentTypeNode componentType = (ComponentTypeNode)context.loadNode(path);
            RefComponentNode componentNode = new RefComponentNode(componentType);
            componentNode.setId(componentDesc.getId());
            componentNode.setComponent(path);
            gameObject.addComponent(componentNode);
        }
        n = desc.getEmbeddedComponentsCount();
        for (int i = 0; i < n; ++i) {
            EmbeddedComponentDesc componentDesc = desc.getEmbeddedComponents(i);
            ComponentTypeNode componentType = (ComponentTypeNode)context.loadNode(componentDesc.getType(), new ByteArrayInputStream(componentDesc.getData().getBytes()));
            ComponentNode component = new ComponentNode(componentType);
            component.setId(componentDesc.getId());
            gameObject.addComponent(component);
        }
        return gameObject;
    }

    @Override
    public Message onBuildMessage(Context context, Node node, IProgressMonitor monitor) throws IOException, CoreException {
        GameObjectNode gameObject = (GameObjectNode)node;
        Builder builder = PrototypeDesc.newBuilder();
        SubMonitor progress = SubMonitor.convert(monitor, gameObject.getChildren().size());
        for (Node child : gameObject.getChildren()) {
            if (child instanceof RefComponentNode) {
                RefComponentNode component = (RefComponentNode)child;
                ComponentDesc.Builder componentBuilder = ComponentDesc.newBuilder();
                componentBuilder.setId(component.getId());
                componentBuilder.setComponent(component.getComponent());
                builder.addComponents(componentBuilder);
                progress.worked(1);
            } else if (child instanceof ComponentNode) {
                ComponentNode component = (ComponentNode)child;
                EmbeddedComponentDesc.Builder componentBuilder = EmbeddedComponentDesc.newBuilder();
                componentBuilder.setId(component.getId());
                ComponentTypeNode componentType = (ComponentTypeNode)component.getChildren().get(0);
                ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
                SubMonitor partProgress = progress.newChild(1).setWorkRemaining(2);
                Message message = context.buildMessage(componentType, partProgress.newChild(1));
                SceneUtil.saveMessage(message, byteStream, partProgress.newChild(1));
                componentBuilder.setType(componentType.getTypeId());
                componentBuilder.setData(byteStream.toString());
                builder.addEmbeddedComponents(componentBuilder);
            }
        }
        return builder.build();
    }

    @Override
    public Node onCreateNode(Context context, String type) throws IOException, CoreException {
        return new GameObjectNode();
    }
}
