package com.dynamo.cr.sceneed.gameobject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

/**
 * Place holder class to support generic components, will be replaced with component-specific presenters in time.
 * @author rasv
 *
 */
public class ComponentPresenter extends NodePresenter {

    @Override
    public Node doLoad(String type, InputStream contents) throws IOException, CoreException {
        if (type.equals("sprite")) {
            InputStreamReader reader = new InputStreamReader(contents);
            SpriteDesc.Builder builder = SpriteDesc.newBuilder();
            TextFormat.merge(reader, builder);
            SpriteDesc desc = builder.build();
            SpriteNode sprite = new SpriteNode(this);
            sprite.setTexture(desc.getTexture());
            sprite.setWidth(desc.getWidth());
            sprite.setHeight(desc.getHeight());
            sprite.setTileWidth(desc.getTileWidth());
            sprite.setTileHeight(desc.getTileHeight());
            sprite.setTilesPerRow(desc.getTilesPerRow());
            sprite.setTileCount(desc.getTileCount());
            return sprite;
        } else if (type.equals("spawnpoint")) {
            InputStreamReader reader = new InputStreamReader(contents);
            SpawnPointDesc.Builder builder = SpawnPointDesc.newBuilder();
            TextFormat.merge(reader, builder);
            SpawnPointDesc desc = builder.build();
            SpawnPointNode spawnPoint = new SpawnPointNode(this);
            spawnPoint.setPrototype(desc.getPrototype());
            return spawnPoint;
        } else if (type.equals("collisionobject")) {
            InputStreamReader reader = new InputStreamReader(contents);
            CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
            TextFormat.merge(reader, builder);
            CollisionObjectDesc desc = builder.build();
            CollisionObjectNode collisionObject = new CollisionObjectNode(this);
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
            CollectionProxyNode collectionProxy = new CollectionProxyNode(this);
            collectionProxy.setCollection(desc.getCollection());
            return collectionProxy;
        }
        return new GenericComponentTypeNode(this, type);
    }

    @Override
    public Node createNode(String type) throws IOException, CoreException {
        if (type.equals("sprite")) {
            return new SpriteNode(this);
        } else if (type.equals("spawnpoint")) {
            return new SpawnPointNode(this);
        } else if (type.equals("collisionobject")) {
            return new CollisionObjectNode(this);
        } else if (type.equals("collectionproxy")) {
            return new CollectionProxyNode(this);
        } else {
            return new GenericComponentTypeNode(this, type);
        }
    }

    @Override
    public Message buildMessage(Node node, IProgressMonitor monitor) throws IOException, CoreException {
        if (node instanceof SpriteNode) {
            SpriteDesc.Builder builder = SpriteDesc.newBuilder();
            SpriteNode sprite = (SpriteNode)node;
            builder.setTexture(sprite.getTexture());
            builder.setWidth(sprite.getWidth());
            builder.setHeight(sprite.getHeight());
            builder.setTileWidth(sprite.getTileWidth());
            builder.setTileHeight(sprite.getTileHeight());
            builder.setTilesPerRow(sprite.getTilesPerRow());
            builder.setTileCount(sprite.getTileCount());
            return builder.build();
        } else if (node instanceof SpawnPointNode) {
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
        if (monitor != null) {
            monitor.done();
        }
        return null;
    }

}
