package com.dynamo.cr.ddfeditor.scene;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.go.core.ComponentLoader;
import com.dynamo.cr.sceneed.core.ISceneView.ILoaderContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class DdfComponentLoader extends ComponentLoader {
    @Override
    public Node load(ILoaderContext context, String type, InputStream contents)
            throws IOException, CoreException {
        if (type.equals("spawnpoint")) {
            InputStreamReader reader = new InputStreamReader(contents);
            SpawnPointDesc.Builder builder = SpawnPointDesc.newBuilder();
            TextFormat.merge(reader, builder);
            SpawnPointDesc desc = builder.build();
            SpawnPointNode spawnPoint = new SpawnPointNode();
            spawnPoint.setPrototype(desc.getPrototype());
            return spawnPoint;
        } else if (type.equals("collisionobject")) {
            InputStreamReader reader = new InputStreamReader(contents);
            CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
            TextFormat.merge(reader, builder);
            CollisionObjectDesc desc = builder.build();
            CollisionObjectNode collisionObject = new CollisionObjectNode();
            collisionObject.setCollisionShape(desc.getCollisionShape());
            collisionObject.setType(desc.getType());
            collisionObject.setMass(desc.getMass());
            collisionObject.setFriction(desc.getFriction());
            collisionObject.setRestitution(desc.getRestitution());
            collisionObject.setGroup(desc.getGroup());
            StringBuffer mask = new StringBuffer();
            int n = desc.getMaskCount();
            for (int i = 0; i < n; ++i) {
                mask.append(desc.getMask(i));
                if (i < n - 1) {
                    mask.append(", ");
                }
            }
            collisionObject.setMask(mask.toString());
            return collisionObject;
        } else if (type.equals("collectionproxy")) {
            InputStreamReader reader = new InputStreamReader(contents);
            CollectionProxyDesc.Builder builder = CollectionProxyDesc.newBuilder();
            TextFormat.merge(reader, builder);
            CollectionProxyDesc desc = builder.build();
            CollectionProxyNode collectionProxy = new CollectionProxyNode();
            collectionProxy.setCollection(desc.getCollection());
            return collectionProxy;
        }
        return super.load(context, type, contents);
    }
    @Override
    public Node createNode(String type) throws IOException,
    CoreException {
        if (type.equals("spawnpoint")) {
            return new SpawnPointNode();
        } else if (type.equals("collisionobject")) {
            return new CollisionObjectNode();
        } else if (type.equals("collectionproxy")) {
            return new CollectionProxyNode();
        } else if (type.equals("camera")) {
            return new CameraNode(type);
        } else if (type.equals("emitter")) {
            return new EmitterNode(type);
        } else if (type.equals("gui")) {
            return new GuiNode(type);
        } else if (type.equals("light")) {
            return new LightNode(type);
        } else if (type.equals("model")) {
            return new ModelNode(type);
        }
        return super.createNode(type);
    }

    @Override
    public Message buildMessage(ILoaderContext context, Node node,
            IProgressMonitor monitor) throws IOException, CoreException {
        if (node instanceof SpawnPointNode) {
            SpawnPointDesc.Builder builder = SpawnPointDesc.newBuilder();
            SpawnPointNode spawnPoint = (SpawnPointNode)node;
            builder.setPrototype(spawnPoint.getPrototype());
            return builder.build();
        } else if (node instanceof CollisionObjectNode) {
            CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
            CollisionObjectNode collisionObject = (CollisionObjectNode)node;
            builder.setCollisionShape(collisionObject.getCollisionShape());
            builder.setType(collisionObject.getType());
            builder.setMass(collisionObject.getMass());
            builder.setFriction(collisionObject.getFriction());
            builder.setRestitution(collisionObject.getRestitution());
            builder.setGroup(collisionObject.getGroup());
            String[] masks = collisionObject.getMask().split("[ ,]+");
            for (String mask : masks) {
                builder.addMask(mask);
            }
            return builder.build();
        } else if (node instanceof CollectionProxyNode) {
            CollectionProxyDesc.Builder builder = CollectionProxyDesc.newBuilder();
            CollectionProxyNode collectionProxy = (CollectionProxyNode)node;
            builder.setCollection(collectionProxy.getCollection());
            return builder.build();
        }
        return super.buildMessage(context, node, monitor);
    }
}
