package com.dynamo.cr.go.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

import com.dynamo.cr.go.core.script.LuaPropertyParser;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyType;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class CollectionLoader implements INodeLoader<CollectionNode> {

    @Override
    public CollectionNode load(ILoaderContext context, InputStream contents) throws IOException, CoreException {
        InputStreamReader reader = new InputStreamReader(contents);
        Builder builder = CollectionDesc.newBuilder();
        TextFormat.merge(reader, builder);
        CollectionDesc desc = builder.build();
        CollectionNode node = new CollectionNode();
        node.setName(desc.getName());
        node.setScaleAlongZ(desc.getScaleAlongZ() != 0);
        Map<String, GameObjectInstanceNode> idToInstance = new HashMap<String, GameObjectInstanceNode>();
        Set<GameObjectInstanceNode> remainingInstances = new HashSet<GameObjectInstanceNode>();
        int n = desc.getInstancesCount();
        for (int i = 0; i < n; ++i) {
            InstanceDesc instanceDesc = desc.getInstances(i);
            String path = instanceDesc.getPrototype();
            GameObjectNode gameObjectNode = (GameObjectNode)context.loadNode(path);
            GameObjectInstanceNode instanceNode = new GameObjectInstanceNode(gameObjectNode);
            instanceNode.setTranslation(LoaderUtil.toPoint3d(instanceDesc.getPosition()));
            instanceNode.setRotation(LoaderUtil.toQuat4(instanceDesc.getRotation()));
            instanceNode.setScale(instanceDesc.getScale());
            instanceNode.setId(instanceDesc.getId());
            instanceNode.setGameObject(path);
            idToInstance.put(instanceDesc.getId(), instanceNode);
            remainingInstances.add(instanceNode);
            int compPropCount = instanceDesc.getComponentPropertiesCount();
            for (int j = 0; j < compPropCount; ++j) {
                ComponentPropertyDesc compPropDesc = instanceDesc.getComponentProperties(j);
                Map<String, String> componentProperties = new HashMap<String, String>();
                int propCount = compPropDesc.getPropertiesCount();
                for (int k = 0; k < propCount; ++k) {
                    PropertyDesc propDesc = compPropDesc.getProperties(k);
                    componentProperties.put(propDesc.getId(), propDesc.getValue());
                }
                instanceNode.setComponentProperties(compPropDesc.getId(), componentProperties);
            }
        }
        for (int i = 0; i < n; ++i) {
            InstanceDesc instanceDesc = desc.getInstances(i);
            Node parent = idToInstance.get(instanceDesc.getId());
            List<String> children = instanceDesc.getChildrenList();
            for (String childId : children) {
                Node child = idToInstance.get(childId);
                parent.addChild(child);
                remainingInstances.remove(child);
            }
        }
        for (GameObjectInstanceNode instance : remainingInstances) {
            node.addChild(instance);
        }
        n = desc.getCollectionInstancesCount();
        for (int i = 0; i < n; ++i) {
            CollectionInstanceDesc instanceDesc = desc.getCollectionInstances(i);
            String path = instanceDesc.getCollection();
            CollectionNode collectionNode = (CollectionNode)context.loadNode(path);
            CollectionInstanceNode instanceNode = new CollectionInstanceNode(collectionNode);
            instanceNode.setTranslation(LoaderUtil.toPoint3d(instanceDesc.getPosition()));
            instanceNode.setRotation(LoaderUtil.toQuat4(instanceDesc.getRotation()));
            instanceNode.setScale(instanceDesc.getScale());
            instanceNode.setId(instanceDesc.getId());
            instanceNode.setCollection(path);
            node.addChild(instanceNode);
        }
        return node;
    }

    @Override
    public Message buildMessage(ILoaderContext context, CollectionNode collection, IProgressMonitor monitor)
            throws IOException, CoreException {
        Builder builder = CollectionDesc.newBuilder();
        builder.setName(collection.getName());
        builder.setScaleAlongZ(collection.isScaleAlongZ() ? 1 : 0);
        buildInstances(collection, builder, monitor);
        return builder.build();
    }

    private void buildInstances(Node node, CollectionDesc.Builder builder, IProgressMonitor monitor) {
        SubMonitor progress = SubMonitor.convert(monitor, node.getChildren().size());
        for (Node child : node.getChildren()) {
            if (child instanceof GameObjectInstanceNode) {
                GameObjectInstanceNode instance = (GameObjectInstanceNode)child;
                InstanceDesc.Builder instanceBuilder = InstanceDesc.newBuilder();
                instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                instanceBuilder.setScale((float)instance.getScale());
                instanceBuilder.setId(instance.getId());
                instanceBuilder.setPrototype(instance.getGameObject());
                for (Node grandChild : child.getChildren()) {
                    if (grandChild instanceof GameObjectInstanceNode) {
                        instanceBuilder.addChildren(((GameObjectInstanceNode)grandChild).getId());
                    }
                }
                buildProperties(instance, instanceBuilder);
                builder.addInstances(instanceBuilder);
                buildInstances(child, builder, monitor);
            } else if (child instanceof CollectionInstanceNode) {
                CollectionInstanceNode instance = (CollectionInstanceNode)child;
                CollectionInstanceDesc.Builder instanceBuilder = CollectionInstanceDesc.newBuilder();
                instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                instanceBuilder.setScale((float)instance.getScale());
                instanceBuilder.setId(instance.getId());
                instanceBuilder.setCollection(instance.getCollection());
                builder.addCollectionInstances(instanceBuilder);
            }
            progress.worked(1);
        }
    }

    private void buildProperties(GameObjectInstanceNode instance, InstanceDesc.Builder instanceBuilder) {
        for (Node instanceChild : instance.getChildren()) {
            if (instanceChild instanceof ComponentPropertyNode) {
                ComponentPropertyNode compNode = (ComponentPropertyNode)instanceChild;
                List<LuaPropertyParser.Property> defaults = compNode.getPropertyDefaults();
                Map<String, String> properties = compNode.getComponentProperties();
                ComponentPropertyDesc.Builder compPropBuilder = ComponentPropertyDesc.newBuilder();
                compPropBuilder.setId(compNode.getId());
                for (LuaPropertyParser.Property property : defaults) {
                    if (property.getStatus() == LuaPropertyParser.Property.Status.OK) {
                        String value = properties.get(property.getName());
                        if (value != null) {
                            PropertyDesc.Builder propBuilder = PropertyDesc.newBuilder();
                            propBuilder.setId(property.getName());
                            switch (property.getType()) {
                            case NUMBER:
                                propBuilder.setType(PropertyType.PROPERTY_TYPE_NUMBER);
                                break;
                            case HASH:
                                propBuilder.setType(PropertyType.PROPERTY_TYPE_HASH);
                                break;
                            case URL:
                                propBuilder.setType(PropertyType.PROPERTY_TYPE_URL);
                                break;
                            }
                            propBuilder.setValue(value);
                            compPropBuilder.addProperties(propBuilder);
                        }
                    }
                }
                if (compPropBuilder.getPropertiesCount() > 0) {
                    instanceBuilder.addComponentProperties(compPropBuilder);
                }
            }
        }
    }
}
