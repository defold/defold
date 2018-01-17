package com.dynamo.cr.go.core;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

import com.dynamo.bob.pipeline.LuaScanner;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.gameobject.proto.GameObject.ComponentDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedComponentDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc;
import com.dynamo.gameobject.proto.GameObject.PrototypeDesc.Builder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class GameObjectLoader implements INodeLoader<GameObjectNode> {

    @Override
    public GameObjectNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
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
            componentNode.setTranslation(LoaderUtil.toPoint3d(componentDesc.getPosition()));
            componentNode.setRotation(LoaderUtil.toQuat4(componentDesc.getRotation()));
            componentNode.setId(componentDesc.getId());
            componentNode.setComponent(path);
            Map<String, Object> properties = new HashMap<String, Object>();
            int propertyCount = componentDesc.getPropertiesCount();
            for (int j = 0; j < propertyCount; ++j) {
                PropertyDesc propDesc = componentDesc.getProperties(j);
                properties.put(propDesc.getId(), GoPropertyUtil.stringToProperty(propDesc.getType(), propDesc.getValue()));
            }
            componentNode.setPrototypeProperties(properties);
            gameObject.addChild(componentNode);
        }
        n = desc.getEmbeddedComponentsCount();
        for (int i = 0; i < n; ++i) {
            EmbeddedComponentDesc componentDesc = desc.getEmbeddedComponents(i);
            // TODO temp hack to "convert" from sprite2 to sprite
            String type = componentDesc.getType();
            if (type.equals("sprite2")) {
                type = "sprite";
            }
            // TODO temp hack to "convert" from spawnpoint to factory
            if (type.equals("spawnpoint")) {
                type = "factory";
            }
            ComponentTypeNode componentType = (ComponentTypeNode)context.loadNode(type, new ByteArrayInputStream(componentDesc.getData().getBytes()));
            componentType.setTranslation(LoaderUtil.toPoint3d(componentDesc.getPosition()));
            componentType.setRotation(LoaderUtil.toQuat4(componentDesc.getRotation()));
            componentType.setId(componentDesc.getId());
            gameObject.addChild(componentType);
        }
        return gameObject;
    }

    @Override
    public Message buildMessage(ILoaderContext context, GameObjectNode gameObject, IProgressMonitor monitor)
            throws IOException, CoreException {
        Builder builder = PrototypeDesc.newBuilder();
        SubMonitor progress = SubMonitor.convert(monitor, gameObject.getChildren().size());
        for (Node child : gameObject.getChildren()) {
            if (child instanceof RefComponentNode) {
                RefComponentNode component = (RefComponentNode)child;
                ComponentDesc.Builder componentBuilder = ComponentDesc.newBuilder();
                componentBuilder.setPosition(LoaderUtil.toPoint3(component.getTranslation()));
                componentBuilder.setRotation(LoaderUtil.toQuat(component.getRotation()));
                componentBuilder.setId(component.getId());
                componentBuilder.setComponent(component.getComponent());
                // Store properties
                List<LuaScanner.Property> propertyDefaults = component.getPropertyDefaults();
                Map<String, Object> properties = component.getPrototypeProperties();
                for (LuaScanner.Property property : propertyDefaults) {
                    if (property.status == LuaScanner.Property.Status.OK) {
                        Object value = properties.get(property.name);
                        if (value != null) {
                            PropertyDesc.Builder propertyBuilder = PropertyDesc.newBuilder();
                            propertyBuilder.setId(property.name);
                            propertyBuilder.setType(property.type);
                            propertyBuilder.setSubType(property.subType);
                            propertyBuilder.setValue(GoPropertyUtil.propertyToString(value));
                            componentBuilder.addProperties(propertyBuilder);
                        }
                    }
                }
                builder.addComponents(componentBuilder);
                progress.worked(1);
            } else if (child instanceof ComponentTypeNode) {
                ComponentTypeNode componentType = (ComponentTypeNode)child;
                EmbeddedComponentDesc.Builder componentBuilder = EmbeddedComponentDesc.newBuilder();
                componentBuilder.setPosition(LoaderUtil.toPoint3(componentType.getTranslation()));
                componentBuilder.setRotation(LoaderUtil.toQuat(componentType.getRotation()));
                componentBuilder.setId(componentType.getId());
                ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
                SubMonitor partProgress = progress.newChild(1).setWorkRemaining(2);
                INodeTypeRegistry registry = context.getNodeTypeRegistry();
                INodeType nodeType = registry.getNodeTypeClass(componentType.getClass());
                INodeLoader<Node> loader = nodeType.getLoader();
                Message message = loader.buildMessage(context, componentType, partProgress.newChild(1));
                SceneUtil.saveMessage(message, byteStream, partProgress.newChild(1), false);
                componentBuilder.setType(nodeType.getExtension());
                componentBuilder.setData(byteStream.toString());
                builder.addEmbeddedComponents(componentBuilder);
            }
        }
        return builder.build();
    }

}
