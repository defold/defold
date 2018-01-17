package com.dynamo.cr.go.core;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.Vector;

import javax.vecmath.Vector3d;

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
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.ComponentPropertyDesc;
import com.dynamo.gameobject.proto.GameObject.EmbeddedInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstancePropertyDesc;
import com.dynamo.gameobject.proto.GameObject.PropertyDesc;
import com.dynamo.proto.DdfMath;
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
            RefGameObjectInstanceNode instanceNode = new RefGameObjectInstanceNode(gameObjectNode);
            instanceNode.setTranslation(LoaderUtil.toPoint3d(instanceDesc.getPosition()));
            instanceNode.setRotation(LoaderUtil.toQuat4(instanceDesc.getRotation()));
            if (instanceDesc.hasScale3()) {
                instanceNode.setScale(LoaderUtil.toVector3(instanceDesc.getScale3()));
            } else {
                double s = instanceDesc.getScale();
                instanceNode.setScale(new Vector3d(s, s, s));
            }

            instanceNode.setId(instanceDesc.getId());
            instanceNode.setGameObject(path);
            idToInstance.put(instanceDesc.getId(), instanceNode);
            remainingInstances.add(instanceNode);
            int compPropCount = instanceDesc.getComponentPropertiesCount();
            for (int j = 0; j < compPropCount; ++j) {
                ComponentPropertyDesc compPropDesc = instanceDesc.getComponentProperties(j);
                Map<String, Object> componentProperties = new HashMap<String, Object>();
                int propCount = compPropDesc.getPropertiesCount();
                for (int k = 0; k < propCount; ++k) {
                    PropertyDesc propDesc = compPropDesc.getProperties(k);
                    componentProperties.put(propDesc.getId(), GoPropertyUtil.stringToProperty(propDesc.getType(), propDesc.getValue()));
                }
                instanceNode.setComponentProperties(compPropDesc.getId(), componentProperties);
            }
        }
        n = desc.getEmbeddedInstancesCount();
        for (int i = 0; i < n; ++i) {
            EmbeddedInstanceDesc instanceDesc = desc.getEmbeddedInstances(i);
            GameObjectNode instanceNode = (GameObjectNode)context.loadNode("go", new ByteArrayInputStream(instanceDesc.getData().getBytes()));
            instanceNode.setTranslation(LoaderUtil.toPoint3d(instanceDesc.getPosition()));
            instanceNode.setRotation(LoaderUtil.toQuat4(instanceDesc.getRotation()));
            if (instanceDesc.hasScale3()) {
                instanceNode.setScale(LoaderUtil.toVector3(instanceDesc.getScale3()));
            } else {
                double s = instanceDesc.getScale();
                instanceNode.setScale(new Vector3d(s, s, s));
            }
            instanceNode.setId(instanceDesc.getId());
            idToInstance.put(instanceDesc.getId(), instanceNode);
            remainingInstances.add(instanceNode);
        }
        n = desc.getInstancesCount();
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
        n = desc.getEmbeddedInstancesCount();
        for (int i = 0; i < n; ++i) {
            EmbeddedInstanceDesc instanceDesc = desc.getEmbeddedInstances(i);
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
            if (instanceDesc.hasScale3()) {
                instanceNode.setScale(LoaderUtil.toVector3(instanceDesc.getScale3()));
            } else {
                double s = instanceDesc.getScale();
                instanceNode.setScale(new Vector3d(s, s, s));
            }
            instanceNode.setId(instanceDesc.getId());
            instanceNode.setCollection(path);
            for (InstancePropertyDesc instancePropDesc : instanceDesc.getInstancePropertiesList()) {
                for (ComponentPropertyDesc compPropDesc : instancePropDesc.getPropertiesList()) {
                    Map<String, Object> componentProperties = new HashMap<String, Object>();
                    int propCount = compPropDesc.getPropertiesCount();
                    for (int k = 0; k < propCount; ++k) {
                        PropertyDesc propDesc = compPropDesc.getProperties(k);
                        componentProperties.put(propDesc.getId(), GoPropertyUtil.stringToProperty(propDesc.getType(), propDesc.getValue()));
                    }
                    instanceNode.setInstanceProperties(instancePropDesc.getId(), compPropDesc.getId(), componentProperties);
                }
            }
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
        buildInstances(context, collection, builder, monitor);
        return builder.build();
    }

    private void buildInstances(ILoaderContext context, Node node, CollectionDesc.Builder builder, IProgressMonitor monitor) throws IOException, CoreException {
        SubMonitor progress = SubMonitor.convert(monitor, node.getChildren().size());
        for (Node child : node.getChildren()) {
            if (child instanceof RefGameObjectInstanceNode) {
                RefGameObjectInstanceNode instance = (RefGameObjectInstanceNode)child;
                InstanceDesc.Builder instanceBuilder = InstanceDesc.newBuilder();
                instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                instanceBuilder.setScale3(LoaderUtil.toVector3(instance.getScale()));
                instanceBuilder.setId(instance.getId());
                instanceBuilder.setPrototype(instance.getGameObject());
                for (Node grandChild : child.getChildren()) {
                    if (grandChild instanceof GameObjectInstanceNode) {
                        GameObjectInstanceNode goi = (GameObjectInstanceNode)grandChild;
                        // The internal GameObjectNode in a ref instance does not have an id
                        if (goi.getId() != null) {
                            instanceBuilder.addChildren(goi.getId());
                        }
                    }
                }
                instanceBuilder.addAllComponentProperties(buildComponentProperties(instance));
                builder.addInstances(instanceBuilder);
                buildInstances(context, child, builder, monitor);
            } else if (child instanceof GameObjectNode) {
                GameObjectNode instance = (GameObjectNode)child;
                if (instance.getId() != null) {
                    EmbeddedInstanceDesc.Builder instanceBuilder = EmbeddedInstanceDesc.newBuilder();
                    instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                    instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                    instanceBuilder.setScale3(LoaderUtil.toVector3(instance.getScale()));
                    instanceBuilder.setId(instance.getId());
                    ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
                    SubMonitor partProgress = progress.newChild(1).setWorkRemaining(2);
                    INodeTypeRegistry registry = context.getNodeTypeRegistry();
                    INodeType nodeType = registry.getNodeTypeClass(GameObjectNode.class);
                    INodeLoader<Node> loader = nodeType.getLoader();
                    Message message = loader.buildMessage(context, instance, partProgress.newChild(1));
                    SceneUtil.saveMessage(message, byteStream, partProgress.newChild(1), false);
                    instanceBuilder.setData(byteStream.toString());
                    for (Node grandChild : child.getChildren()) {
                        if (grandChild instanceof GameObjectInstanceNode) {
                            GameObjectInstanceNode goi = (GameObjectInstanceNode)grandChild;
                            instanceBuilder.addChildren(goi.getId());
                        }
                    }
                    builder.addEmbeddedInstances(instanceBuilder);
                    buildInstances(context, child, builder, monitor);
                }
            } else if (child instanceof CollectionInstanceNode) {
                CollectionInstanceNode instance = (CollectionInstanceNode)child;
                CollectionInstanceDesc.Builder instanceBuilder = CollectionInstanceDesc.newBuilder();
                instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                instanceBuilder.setScale3(LoaderUtil.toVector3(instance.getScale()));
                instanceBuilder.setId(instance.getId());
                instanceBuilder.setCollection(instance.getCollection());
                instanceBuilder.addAllInstanceProperties(buildInstanceProperties(instance));
                builder.addCollectionInstances(instanceBuilder);
            }
            progress.worked(1);
        }
    }

    private Iterable<ComponentPropertyDesc> buildComponentProperties(RefGameObjectInstanceNode instance) {
        Vector<ComponentPropertyDesc> componentProperties = new Vector<ComponentPropertyDesc>();
        for (Node instanceChild : instance.getChildren()) {
            if (instanceChild instanceof ComponentPropertyNode) {
                ComponentPropertyNode compNode = (ComponentPropertyNode)instanceChild;
                List<LuaScanner.Property> defaults = compNode.getPropertyDefaults();
                Map<String, Object> properties = compNode.getComponentProperties();
                ComponentPropertyDesc.Builder compPropBuilder = ComponentPropertyDesc.newBuilder();
                compPropBuilder.setId(compNode.getId());
                for (LuaScanner.Property property : defaults) {
                    if (property.status == LuaScanner.Property.Status.OK) {
                        Object value = properties.get(property.name);
                        if (value != null) {
                            PropertyDesc.Builder propBuilder = PropertyDesc.newBuilder();
                            propBuilder.setId(property.name);
                            propBuilder.setType(property.type);
                            propBuilder.setSubType(property.subType);
                            propBuilder.setValue(GoPropertyUtil.propertyToString(value));
                            compPropBuilder.addProperties(propBuilder);
                        }
                    }
                }
                if (compPropBuilder.getPropertiesCount() > 0) {
                    componentProperties.add(compPropBuilder.build());
                }
            }
        }
        return componentProperties;
    }

    private Iterable<InstancePropertyDesc> buildInstanceProperties(CollectionInstanceNode instance) {
        Vector<InstancePropertyDesc> instanceProperties = new Vector<InstancePropertyDesc>();
        for (Node instanceChild : instance.getChildren()) {
            if (instanceChild instanceof PropertyNode<?>) {
                buildInstanceProperties("", (PropertyNode<?>)instanceChild, instanceProperties);
            }
        }
        return instanceProperties;
    }

    private void buildInstanceProperties(String path, PropertyNode<?> node, Vector<InstancePropertyDesc> outInstanceProperties) {
        if (node instanceof CollectionPropertyNode) {
            CollectionPropertyNode collectionProp = (CollectionPropertyNode)node;
            String currentPath = path + collectionProp.getId() + "/";
            for (Node child : node.getChildren()) {
                if (child instanceof PropertyNode<?>) {
                    buildInstanceProperties(currentPath, (PropertyNode<?>)child, outInstanceProperties);
                }
            }
        } else if (node instanceof GameObjectPropertyNode) {
            GameObjectPropertyNode goProp = (GameObjectPropertyNode)node;
            String currentPath = path + goProp.getId();
            InstancePropertyDesc.Builder instanceBuilder = InstancePropertyDesc.newBuilder();
            instanceBuilder.setId(currentPath);
            for (Node child : node.getChildren()) {
                if (child instanceof ComponentPropertyNode) {
                    ComponentPropertyNode compNode = (ComponentPropertyNode)child;
                    List<LuaScanner.Property> defaults = compNode.getPropertyDefaults();
                    Map<String, Object> properties = compNode.getComponentProperties();
                    ComponentPropertyDesc.Builder compPropBuilder = ComponentPropertyDesc.newBuilder();
                    compPropBuilder.setId(compNode.getId());
                    for (LuaScanner.Property property : defaults) {
                        if (property.status == LuaScanner.Property.Status.OK) {
                            Object value = properties.get(property.name);
                            if (value != null) {
                                PropertyDesc.Builder propBuilder = PropertyDesc.newBuilder();
                                propBuilder.setId(property.name);
                                propBuilder.setType(property.type);
                                propBuilder.setSubType(property.subType);
                                propBuilder.setValue(GoPropertyUtil.propertyToString(value));
                                compPropBuilder.addProperties(propBuilder);
                            }
                        }
                    }
                    if (compPropBuilder.getPropertiesCount() > 0) {
                        instanceBuilder.addProperties(compPropBuilder);
                    }
                } else if (child instanceof PropertyNode<?>) {
                    buildInstanceProperties(path, (PropertyNode<?>)child, outInstanceProperties);
                }
            }
            if (instanceBuilder.getPropertiesCount() > 0) {
                outInstanceProperties.add(instanceBuilder.build());
            }
        }
    }
}
