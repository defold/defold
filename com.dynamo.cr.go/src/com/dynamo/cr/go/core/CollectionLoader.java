package com.dynamo.cr.go.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.SubMonitor;

import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.LoaderUtil;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc.Builder;
import com.dynamo.gameobject.proto.GameObject.CollectionInstanceDesc;
import com.dynamo.gameobject.proto.GameObject.InstanceDesc;
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
        int n = desc.getInstancesCount();
        for (int i = 0; i < n; ++i) {
            InstanceDesc instanceDesc = desc.getInstances(i);
            String path = instanceDesc.getPrototype();
            GameObjectNode gameObjectNode = (GameObjectNode)context.loadNode(path);
            GameObjectInstanceNode instanceNode = new GameObjectInstanceNode(gameObjectNode);
            instanceNode.setTranslation(LoaderUtil.toPoint3d(instanceDesc.getPosition()));
            instanceNode.setRotation(LoaderUtil.toQuat4(instanceDesc.getRotation()));
            instanceNode.setId(instanceDesc.getId());
            instanceNode.setGameObject(path);
            node.addChild(instanceNode);
        }
        n = desc.getCollectionInstancesCount();
        for (int i = 0; i < n; ++i) {
            CollectionInstanceDesc instanceDesc = desc.getCollectionInstances(i);
            String path = instanceDesc.getCollection();
            CollectionNode collectionNode = (CollectionNode)context.loadNode(path);
            CollectionInstanceNode instanceNode = new CollectionInstanceNode(collectionNode);
            instanceNode.setTranslation(LoaderUtil.toPoint3d(instanceDesc.getPosition()));
            instanceNode.setRotation(LoaderUtil.toQuat4(instanceDesc.getRotation()));
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
        SubMonitor progress = SubMonitor.convert(monitor, collection.getChildren().size());
        builder.setName(collection.getName());
        for (Node child : collection.getChildren()) {
            if (child instanceof GameObjectInstanceNode) {
                GameObjectInstanceNode instance = (GameObjectInstanceNode)child;
                InstanceDesc.Builder instanceBuilder = InstanceDesc.newBuilder();
                instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                instanceBuilder.setId(instance.getId());
                instanceBuilder.setPrototype(instance.getGameObject());
                builder.addInstances(instanceBuilder);
            } else if (child instanceof CollectionInstanceNode) {
                CollectionInstanceNode instance = (CollectionInstanceNode)child;
                CollectionInstanceDesc.Builder instanceBuilder = CollectionInstanceDesc.newBuilder();
                instanceBuilder.setPosition(LoaderUtil.toPoint3(instance.getTranslation()));
                instanceBuilder.setRotation(LoaderUtil.toQuat(instance.getRotation()));
                instanceBuilder.setId(instance.getId());
                instanceBuilder.setCollection(instance.getCollection());
                builder.addCollectionInstances(instanceBuilder);
            }
            progress.worked(1);
        }
        return builder.build();
    }

}
