package com.dynamo.cr.sceneed.go;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.ISceneView.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GameObjectLoader implements INodeLoader {

    @Override
    public Node load(ILoaderContext context, String type,
            InputStream contents) throws IOException, CoreException {
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
    public Message buildMessage(ILoaderContext context, Node node, IProgressMonitor monitor)
            throws IOException, CoreException {
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
                INodeLoader loader = context.getNodeTypeRegistry().getLoader(componentType.getClass());
                Message message = loader.buildMessage(context, componentType, partProgress.newChild(1));
                SceneUtil.saveMessage(message, byteStream, partProgress.newChild(1));
                componentBuilder.setType(componentType.getTypeId());
                componentBuilder.setData(byteStream.toString());
                builder.addEmbeddedComponents(componentBuilder);
            }
        }
        return builder.build();
    }

    @Override
    public Node createNode(String type) throws IOException, CoreException {
        return new GameObjectNode();
    }

}
